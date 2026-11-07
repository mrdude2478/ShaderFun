#include "library.h"

#include "player.h"
#include "tags.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace library {

namespace {

const size_t MAX_FILES = 30000;
const int MAX_DEPTH = 16;
const char* CACHE_MAGIC = "SFLIB1";

SDL_mutex* g_mu = nullptr;
std::shared_ptr<const Snapshot> g_snap = std::make_shared<Snapshot>();
SDL_atomic_t g_version, g_scanning, g_done, g_total, g_cancel;
SDL_Thread* g_thread = nullptr;
std::string g_cachePath;
std::string g_jobRoot;

// ------------------------------------------------------------------ small helpers
std::string lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
	return s;
}

std::string stripExt(const std::string& name) {
	size_t dot = name.find_last_of('.');
	return (dot == std::string::npos || dot == 0) ? name : name.substr(0, dot);
}

std::string yearOnly(const std::string& y) {
	return y.size() >= 4 ? y.substr(0, 4) : y;
}

bool pathIsDir(const std::string& path, const struct dirent* ent) {
	if (ent && ent->d_type == DT_DIR) return true;
	if (ent && ent->d_type != DT_UNKNOWN) return false;
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string fileNameNoExt(const std::string& path) { return stripExt(player::baseName(path)); }

// ------------------------------------------------------------------ building a snapshot
struct SortKey {
	std::string artist, album, path;
	int disc = 0, track = 0;
};

SortKey keyOf(const Track& t) {
	SortKey k;
	const std::string a = t.albumArtist.empty() ? t.artist : t.albumArtist;
	k.artist = a.empty() ? std::string("\x7f") : lower(a); // unknown artists last
	k.album = lower(albumDisplayName(t)) + "\x1f" + (t.albumArtist.empty() ? lower(player::parentPath(t.path)) : lower(t.albumArtist));
	k.path = lower(t.path);
	k.disc = t.discNo;
	k.track = t.trackNo;
	return k;
}

bool trackLess(const SortKey& a, const SortKey& b) {
	if (a.artist != b.artist) return a.artist < b.artist;
	if (a.album != b.album) return a.album < b.album;
	if (a.disc != b.disc) return a.disc < b.disc;
	if (a.track != b.track) return a.track < b.track;
	return a.path < b.path;
}

// within one album: disc, track number, then file name
bool inAlbumLess(const SortKey& a, const SortKey& b) {
	if (a.disc != b.disc) return a.disc < b.disc;
	if (a.track != b.track) return a.track < b.track;
	return a.path < b.path;
}

// within an artist or genre: album first, then disc, track
bool inCollectionLess(const SortKey& a, const SortKey& b) {
	if (a.artist != b.artist) return a.artist < b.artist;
	if (a.album != b.album) return a.album < b.album;
	return inAlbumLess(a, b);
}

std::string albumArtistOf(const Snapshot& s, const std::vector<int>& idx) {
	for (int i : idx) if (!s.tracks[i].albumArtist.empty()) return s.tracks[i].albumArtist;
	std::string first;
	for (int i : idx) {
		const std::string& a = s.tracks[i].artist;
		if (a.empty()) continue;
		if (first.empty()) first = a;
		else if (lower(a) != lower(first)) return "Various artists";
	}
	return first;
}

std::vector<Group> buildAlbums(const Snapshot& s, const std::vector<int>& subset) {
	std::vector<SortKey> keys(s.tracks.size());
	std::unordered_map<std::string, std::vector<int>> byKey;
	std::vector<std::string> order;
	for (int i : subset) {
		keys[i] = keyOf(s.tracks[i]);
		auto it = byKey.find(keys[i].album);
		if (it == byKey.end()) { order.push_back(keys[i].album); it = byKey.emplace(keys[i].album, std::vector<int>()).first; }
		it->second.push_back(i);
	}
	std::vector<Group> out;
	for (const std::string& k : order) {
		std::vector<int>& idx = byKey[k];
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return inAlbumLess(keys[a], keys[b]); });
		Group g;
		g.name = albumDisplayName(s.tracks[idx[0]]);
		g.sub = albumArtistOf(s, idx);
		g.tracks = idx;
		out.push_back(std::move(g));
	}
	std::sort(out.begin(), out.end(), [](const Group& a, const Group& b) {
		const std::string la = lower(a.name), lb = lower(b.name);
		return la != lb ? la < lb : lower(a.sub) < lower(b.sub);
	});
	return out;
}

std::shared_ptr<const Snapshot> buildSnapshot(std::vector<Track> tracks) {
	auto snap = std::make_shared<Snapshot>();

	// sort the tracks, remembering their keys
	std::vector<SortKey> keys(tracks.size());
	for (size_t i = 0; i < tracks.size(); i++) keys[i] = keyOf(tracks[i]);
	std::vector<int> order(tracks.size());
	for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
	std::sort(order.begin(), order.end(), [&](int a, int b) { return trackLess(keys[a], keys[b]); });
	snap->tracks.reserve(tracks.size());
	for (int i : order) snap->tracks.push_back(std::move(tracks[i]));

	std::vector<int> all(snap->tracks.size());
	for (size_t i = 0; i < all.size(); i++) all[i] = (int)i;
	snap->albums = buildAlbums(*snap, all);

	// artists and genres
	std::vector<SortKey> k2(snap->tracks.size());
	for (size_t i = 0; i < k2.size(); i++) k2[i] = keyOf(snap->tracks[i]);
	auto collect = [&](bool artists) {
		std::unordered_map<std::string, Group> map;
		for (size_t i = 0; i < snap->tracks.size(); i++) {
			const Track& t = snap->tracks[i];
			std::string name = artists ? (t.artist.empty() ? t.albumArtist : t.artist) : t.genre;
			std::string key = lower(name);
			Group& g = map[key];
			if (g.name.empty()) g.name = name.empty() ? (artists ? "Unknown artist" : "Unknown genre") : name;
			g.tracks.push_back((int)i);
		}
		std::vector<Group> v;
		for (auto& kv : map) {
			Group g = std::move(kv.second);
			std::sort(g.tracks.begin(), g.tracks.end(), [&](int a, int b) { return inCollectionLess(k2[a], k2[b]); });
			v.push_back(std::move(g));
		}
		std::sort(v.begin(), v.end(), [](const Group& a, const Group& b) {
			const bool ua = a.name.compare(0, 7, "Unknown") == 0, ub = b.name.compare(0, 7, "Unknown") == 0;
			if (ua != ub) return ub; // "Unknown ..." last
			return lower(a.name) < lower(b.name);
		});
		return v;
	};
	snap->artists = collect(true);
	snap->genres = collect(false);
	return snap;
}

void publish(std::vector<Track> tracks) {
	std::shared_ptr<const Snapshot> snap = buildSnapshot(std::move(tracks));
	SDL_LockMutex(g_mu);
	g_snap = snap;
	SDL_UnlockMutex(g_mu);
	SDL_AtomicAdd(&g_version, 1);
}

// ------------------------------------------------------------------ cache file (tab separated)
std::string clean(const std::string& s) {
	std::string o = s;
	for (char& c : o) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	return o;
}

void saveCache(const std::string& root, const std::vector<Track>& tracks) {
	if (g_cachePath.empty()) return;
	std::ofstream f(g_cachePath);
	if (!f.is_open()) return;
	f << CACHE_MAGIC << "\n" << clean(root) << "\n";
	for (const Track& t : tracks) {
		if (t.path.find_first_of("\t\n\r") != std::string::npos) continue; // can't be stored; it is simply re-read next time
		f << t.path << '\t' << t.size << '\t' << t.mtime << '\t' << clean(t.title) << '\t' << clean(t.artist) << '\t'
			<< clean(t.album) << '\t' << clean(t.albumArtist) << '\t' << clean(t.genre) << '\t' << clean(t.year) << '\t'
			<< t.trackNo << '\t' << t.discNo << '\n';
	}
}

bool loadCache(const std::string& root, std::unordered_map<std::string, Track>& out) {
	std::ifstream f(g_cachePath);
	if (!f.is_open()) return false;
	std::string line;
	if (!std::getline(f, line) || line != CACHE_MAGIC) return false;
	if (!std::getline(f, line) || line != clean(root)) return false; // another music folder
	while (std::getline(f, line)) {
		std::vector<std::string> p;
		size_t start = 0;
		while (true) {
			size_t tab = line.find('\t', start);
			if (tab == std::string::npos) { p.push_back(line.substr(start)); break; }
			p.push_back(line.substr(start, tab - start));
			start = tab + 1;
		}
		if (p.size() != 11) continue; // truncated / damaged row
		Track t;
		t.path = p[0];
		t.size = strtoull(p[1].c_str(), nullptr, 10);
		t.mtime = strtoll(p[2].c_str(), nullptr, 10);
		t.title = p[3]; t.artist = p[4]; t.album = p[5]; t.albumArtist = p[6]; t.genre = p[7]; t.year = p[8];
		t.trackNo = atoi(p[9].c_str());
		t.discNo = atoi(p[10].c_str());
		out[t.path] = std::move(t);
	}
	return true;
}

// ------------------------------------------------------------------ scanning
struct FileInfo {
	std::string path;
	uint64_t size;
	int64_t mtime;
};

void walk(const std::string& dir, std::vector<FileInfo>& out, int depth) {
	if (depth > MAX_DEPTH || out.size() >= MAX_FILES || SDL_AtomicGet(&g_cancel)) return;
	DIR* d = opendir(dir.c_str());
	if (!d) return;
	std::vector<std::string> subdirs;
	struct dirent* ent;
	while ((ent = readdir(d)) != NULL) {
		std::string name = ent->d_name;
		if (name.empty() || name[0] == '.') continue;
		std::string full = player::joinPath(dir, name);
		if (pathIsDir(full, ent)) { subdirs.push_back(full); continue; }
		if (!player::isMusicFile(name)) continue;
		struct stat st;
		FileInfo fi;
		fi.path = full;
		fi.size = 0;
		fi.mtime = 0;
		if (stat(full.c_str(), &st) == 0) { fi.size = (uint64_t)st.st_size; fi.mtime = (int64_t)st.st_mtime; }
		out.push_back(fi);
		if (out.size() >= MAX_FILES) break;
	}
	closedir(d);
	for (const std::string& s : subdirs) walk(s, out, depth + 1);
}

Track trackFromTags(const FileInfo& fi) {
	Track t;
	t.path = fi.path;
	t.size = fi.size;
	t.mtime = fi.mtime;
	tags::TrackTags tg;
	if (tags::read(fi.path, tg, 0.0, true)) {
		t.title = tg.title;
		t.artist = tg.artist;
		t.album = tg.album;
		t.albumArtist = tg.albumArtist;
		t.genre = tg.genre;
		t.year = yearOnly(tg.year);
		t.trackNo = atoi(tg.track.c_str());
		t.discNo = atoi(tg.disc.c_str());
	}
	return t;
}

int scanThread(void*) {
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW); // this affects the calling thread, so it has to be done in here
	const std::string root = g_jobRoot;
	SDL_AtomicSet(&g_done, 0);
	SDL_AtomicSet(&g_total, 0);

	std::unordered_map<std::string, Track> cache;
	const bool haveCache = loadCache(root, cache) && !cache.empty();
	if (haveCache) { // show the cached library straight away
		std::vector<Track> early;
		early.reserve(cache.size());
		for (auto& kv : cache) early.push_back(kv.second);
		publish(std::move(early));
	}

	std::vector<FileInfo> files;
	walk(root, files, 0);
	SDL_AtomicSet(&g_total, (int)files.size());

	std::vector<Track> out;
	out.reserve(files.size());
	size_t changed = 0;
	Uint32 lastPublish = SDL_GetTicks();
	for (size_t i = 0; i < files.size(); i++) {
		if (SDL_AtomicGet(&g_cancel)) { SDL_AtomicSet(&g_scanning, 0); return 0; }
		const FileInfo& fi = files[i];
		auto it = cache.find(fi.path);
		if (it != cache.end() && it->second.size == fi.size && it->second.mtime == fi.mtime) {
			out.push_back(it->second);
		}
		else {
			out.push_back(trackFromTags(fi));
			changed++;
			SDL_Delay(1); // give the SD card to the audio decoder now and then
		}
		SDL_AtomicSet(&g_done, (int)i + 1);
		// first scan (nothing cached): grow the library on screen while we go
		if (!haveCache && SDL_GetTicks() - lastPublish > 1500) {
			publish(out);
			lastPublish = SDL_GetTicks();
		}
	}
	publish(out);
	if (changed > 0 || cache.size() != out.size() || !haveCache) saveCache(root, out);
	printf("Library: %zu tracks (%zu read, %zu from cache)\n", out.size(), changed, out.size() - changed);
	SDL_AtomicSet(&g_scanning, 0);
	return 0;
}

void stopScan() {
	if (g_thread) {
		SDL_AtomicSet(&g_cancel, 1);
		SDL_WaitThread(g_thread, nullptr);
		g_thread = nullptr;
		SDL_AtomicSet(&g_cancel, 0);
		SDL_AtomicSet(&g_scanning, 0);
	}
}

} // namespace

// ================================================================== public
std::string albumDisplayName(const Track& t) {
	if (!t.album.empty()) return t.album;
	const std::string folder = player::baseName(player::parentPath(t.path));
	return folder.empty() ? std::string("Unknown album") : folder;
}

std::string displayTitle(const Track& t) {
	return t.title.empty() ? fileNameNoExt(t.path) : t.title;
}

std::vector<Group> groupAlbums(const Snapshot& s, const std::vector<int>& trackIdx) {
	return buildAlbums(s, trackIdx);
}

void init(const char* cachePath) {
	g_cachePath = cachePath ? cachePath : "";
	if (!g_mu) g_mu = SDL_CreateMutex();
	SDL_AtomicSet(&g_version, 0);
	SDL_AtomicSet(&g_scanning, 0);
	SDL_AtomicSet(&g_done, 0);
	SDL_AtomicSet(&g_total, 0);
	SDL_AtomicSet(&g_cancel, 0);
}

void shutdown() {
	stopScan();
	if (g_mu) { SDL_DestroyMutex(g_mu); g_mu = nullptr; }
}

void startScan(const std::string& musicRoot) {
	if (!g_mu) return;
	stopScan();
	g_jobRoot = musicRoot;
	SDL_AtomicSet(&g_scanning, 1);
	g_thread = SDL_CreateThread(scanThread, "library-scan", nullptr);
	if (!g_thread) {
		printf("Library: could not start the scan thread: %s\n", SDL_GetError());
		SDL_AtomicSet(&g_scanning, 0);
		return;
	}
}

bool scanning() { return SDL_AtomicGet(&g_scanning) != 0; }
int scanDone() { return SDL_AtomicGet(&g_done); }
int scanTotal() { return SDL_AtomicGet(&g_total); }
uint32_t version() { return (uint32_t)SDL_AtomicGet(&g_version); }

std::shared_ptr<const Snapshot> snapshot() {
	if (!g_mu) return std::make_shared<Snapshot>();
	SDL_LockMutex(g_mu);
	std::shared_ptr<const Snapshot> s = g_snap;
	SDL_UnlockMutex(g_mu);
	return s;
}

// ================================================================== playlists
std::string playlistsDir() { return "sdmc:/switch/shaderfun/playlists"; }

namespace {

bool hasPlaylistExt(const std::string& name) {
	const std::string l = lower(name);
	auto ends = [&](const char* e) { size_t n = strlen(e); return l.size() > n && l.compare(l.size() - n, n, e) == 0; };
	return ends(".m3u") || ends(".m3u8");
}

// resolves "./" and "../" in a path, keeping a leading "sdmc:" style prefix
std::string normalizePath(const std::string& p) {
	std::vector<std::string> parts;
	size_t start = 0;
	while (start <= p.size()) {
		size_t slash = p.find('/', start);
		std::string comp = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
		if (comp == "..") { if (parts.size() > 1) parts.pop_back(); }
		else if (!comp.empty() && comp != ".") parts.push_back(comp);
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	std::string out;
	for (size_t i = 0; i < parts.size(); i++) {
		if (i == 0) out = parts[0];
		else out += "/" + parts[i];
		if (i == 0 && parts[0].back() == ':') out += "/";
	}
	if (!p.empty() && p[0] == '/' ) out = "/" + out;
	return out;
}

std::string resolveEntry(const std::string& playlistDir, std::string e) {
	for (char& c : e) if (c == '\\') c = '/';
	if (e.compare(0, 4, "http") == 0 || e.compare(0, 7, "file://") == 0) return "";
	const size_t colon = e.find(':');
	const size_t slash = e.find('/');
	std::string full;
	if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
		if (e.compare(0, 5, "sdmc:") == 0 || e.compare(0, 6, "romfs:") == 0) full = e;
		else return "";                               // a Windows drive path (C:\...) can't be mapped
	}
	else if (!e.empty() && e[0] == '/') {
		struct stat st;
		full = (stat(e.c_str(), &st) == 0) ? e : "sdmc:" + e; // a POSIX path as is, else a PC-made "/Music/x.mp3" on the SD card
	}
	else full = player::joinPath(playlistDir, e);
	return normalizePath(full);
}

void listIn(const std::string& dir, std::vector<Playlist>& out) {
	DIR* d = opendir(dir.c_str());
	if (!d) return;
	struct dirent* ent;
	while ((ent = readdir(d)) != NULL) {
		std::string name = ent->d_name;
		if (name.empty() || name[0] == '.' || !hasPlaylistExt(name)) continue;
		const std::string full = player::joinPath(dir, name);
		if (pathIsDir(full, ent)) continue;
		bool dup = false;
		for (const Playlist& p : out) if (p.path == full) dup = true;
		if (!dup) out.push_back({ stripExt(name), full });
	}
	closedir(d);
}

} // namespace

std::vector<Playlist> listPlaylists(const std::string& musicRoot) {
	std::vector<Playlist> out;
	listIn(playlistsDir(), out);
	if (!musicRoot.empty()) listIn(musicRoot, out);
	std::sort(out.begin(), out.end(), [](const Playlist& a, const Playlist& b) { return lower(a.name) < lower(b.name); });
	return out;
}

std::vector<std::string> loadPlaylist(const std::string& path) {
	std::vector<std::string> out;
	std::ifstream f(path);
	if (!f.is_open()) return out;
	const std::string dir = player::parentPath(path);
	std::string line;
	bool first = true;
	while (std::getline(f, line)) {
		if (first && line.size() >= 3 && (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF) line = line.substr(3); // BOM
		first = false;
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
		size_t a = 0;
		while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) a++;
		line = line.substr(a);
		if (line.empty() || line[0] == '#') continue;
		const std::string full = resolveEntry(dir, line);
		if (full.empty() || !player::isMusicFile(player::baseName(full))) continue;
		struct stat st;
		if (stat(full.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) continue; // missing files are skipped
		out.push_back(full);
	}
	return out;
}

namespace {
// creates every missing folder of a path ("sdmc:/switch/shaderfun/playlists")
void ensureDir(const std::string& dir) {
	for (size_t i = 0; i <= dir.size(); i++) {
		if (i == dir.size() || dir[i] == '/') {
			if (i == 0) continue;
			mkdir(dir.substr(0, i).c_str(), 0777); // fails harmlessly if it exists
		}
	}
}
} // namespace

bool savePlaylist(const std::string& path, const std::vector<std::string>& tracks) {
	ensureDir(player::parentPath(path));
	std::ofstream f(path);
	if (!f.is_open()) return false;
	f << "#EXTM3U\n";
	for (const std::string& t : tracks) f << t << "\n";
	return f.good();
}

} // namespace library
