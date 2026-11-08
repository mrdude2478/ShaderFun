#include "player.h"
#include "smbnet.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <random>
#include <fstream>
#include <map>
#include <sstream>

#ifndef SDL_MIXER_VERSION_ATLEAST
#define SDL_MIXER_VERSION_ATLEAST(X, Y, Z) 0
#endif

namespace player {

namespace {

const double SEEK_STEP_RESTART_THRESHOLD = 3.0; // prev() restarts the track after this many seconds
const size_t MAX_QUEUE = 30000;
const int MAX_DEPTH = 16;

enum State { STOPPED, PLAYING, PAUSED };

Mix_Music* g_music = nullptr;
State g_state = STOPPED;

std::vector<std::string> g_queue;  // tracks in "natural" order
std::vector<int> g_order;          // play order: indices into g_queue
bool g_keepQueue = false;          // remember this queue for the next launch (it was made by the user, not the automatic start-up queue)
bool g_loading = false;            // waiting for a network song to finish downloading
bool g_loadStream = false;         // ...it is being streamed (playing starts after the first part arrives)
bool g_streaming = false;          // the music object reads its data from a stream
std::string g_streamPath;          // smb:// path of the stream
bool g_pauseAfterLoad = false;     // ...and it should stay paused when it is ready (sleep timer)
std::string g_loadingPath;         // the smb:// path being downloaded
std::string g_curLocal;            // cached copy of the current network song ("" for local songs)
bool g_queueDirty = false;         // the queue changed since last_queue.txt was written
std::vector<char> g_chosen;        // parallel to g_queue: 1 = the user picked it, 0 = loaded automatically (start-up / idle folder queue)
int g_pos = -1;                    // index into g_order

std::string g_title, g_artist, g_album; // tags of the loaded track
double g_duration = 0.0;
double g_basePos = 0.0;            // fallback position clock
Uint32 g_baseTick = 0;
uint32_t g_changeCounter = 0;

int g_volume = MIX_MAX_VOLUME / 2;
PlayMode g_mode = MODE_REPEAT_ALL;
std::string g_musicFolder = "sdmc:/switch/shaderfun/music";
std::string g_configPath;
std::string g_emptyString;
std::map<std::string, std::string> g_extra; // other settings in config.ini (kept when the file is rewritten)

std::mt19937 g_rng;

// ------------------------------------------------------------------ strings / paths
std::string lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
	return s;
}

// Case-insensitive "natural" comparison: "Track 2" < "Track 10"
bool naturalLess(const std::string& a, const std::string& b) {
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[j];
		if (isdigit(ca) && isdigit(cb)) {
			size_t si = i, sj = j;
			while (si < a.size() && a[si] == '0') si++;
			while (sj < b.size() && b[sj] == '0') sj++;
			size_t ei = si, ej = sj;
			while (ei < a.size() && isdigit((unsigned char)a[ei])) ei++;
			while (ej < b.size() && isdigit((unsigned char)b[ej])) ej++;
			if (ei - si != ej - sj) return (ei - si) < (ej - sj);
			int c = a.compare(si, ei - si, b, sj, ej - sj);
			if (c != 0) return c < 0;
			i = ei;
			j = ej;
		}
		else {
			int la = tolower(ca), lb = tolower(cb);
			if (la != lb) return la < lb;
			i++;
			j++;
		}
	}
	return (a.size() - i) < (b.size() - j);
}

std::string stripExtension(const std::string& name) {
	size_t dot = name.find_last_of('.');
	if (dot == std::string::npos || dot == 0) return name;
	return name.substr(0, dot);
}

bool isTrackerFormat(const std::string& path) {
	static const char* exts[] = { ".mod", ".xm", ".s3m", ".it", ".mid", ".midi" };
	std::string l = lower(path);
	for (const char* e : exts) {
		size_t n = strlen(e);
		if (l.size() > n && l.compare(l.size() - n, n, e) == 0) return true;
	}
	return false;
}

bool pathIsDir(const std::string& path, const struct dirent* ent) {
	if (ent && ent->d_type == DT_DIR) return true;
	if (ent && ent->d_type != DT_UNKNOWN) return false;
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void scanRecursive(const std::string& dir, std::vector<std::string>& out, int depth) {
	if (depth > MAX_DEPTH || out.size() >= MAX_QUEUE) return;
	DIR* d = opendir(dir.c_str());
	if (!d) return;
	std::vector<std::string> dirs, files;
	struct dirent* ent;
	while ((ent = readdir(d)) != NULL) {
		std::string name = ent->d_name;
		if (name.empty() || name[0] == '.') continue;
		std::string full = joinPath(dir, name);
		if (pathIsDir(full, ent)) dirs.push_back(full);
		else if (isMusicFile(name)) files.push_back(full);
	}
	closedir(d);
	std::sort(files.begin(), files.end(), naturalLess);
	std::sort(dirs.begin(), dirs.end(), naturalLess);
	for (auto& f : files) out.push_back(f);
	for (auto& sub : dirs) scanRecursive(sub, out, depth + 1);
}

// ------------------------------------------------------------------ config
void saveConfig() {
	if (g_configPath.empty()) return;
	std::ofstream f(g_configPath);
	if (!f.is_open()) return;
	f << "music_folder=" << g_musicFolder << "\n";
	f << "volume=" << g_volume << "\n";
	f << "play_mode=" << (int)g_mode << "\n";
	for (const auto& kv : g_extra) f << kv.first << "=" << kv.second << "\n";
}

void loadConfig() {
	std::ifstream f(g_configPath);
	if (!f.is_open()) return;
	std::string line;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq), val = line.substr(eq + 1);
		if (key == "music_folder" && !val.empty()) g_musicFolder = val;
		else if (key == "volume") g_volume = std::max(0, std::min(MIX_MAX_VOLUME, atoi(val.c_str())));
		else if (key == "play_mode") {
			int m = atoi(val.c_str());
			if (m >= 0 && m < MODE_COUNT) g_mode = (PlayMode)m;
		}
		else if (!key.empty()) g_extra[key] = val;
	}
}

// ------------------------------------------------------------------ queue / order
void rebuildOrder(int keepQueueIndex) {
	const int n = (int)g_queue.size();
	g_order.resize(n);
	for (int i = 0; i < n; i++) g_order[i] = i;
	if (g_mode == MODE_SHUFFLE && n > 1) {
		std::shuffle(g_order.begin(), g_order.end(), g_rng);
		if (keepQueueIndex >= 0) { // current track goes first
			for (int i = 0; i < n; i++) {
				if (g_order[i] == keepQueueIndex) { std::swap(g_order[0], g_order[i]); break; }
			}
		}
		g_pos = (keepQueueIndex >= 0) ? 0 : (n ? 0 : -1);
	}
	else {
		g_pos = (keepQueueIndex >= 0 && keepQueueIndex < n) ? keepQueueIndex : (n ? 0 : -1);
	}
}

int currentQueueIndex() {
	return (g_pos >= 0 && g_pos < (int)g_order.size()) ? g_order[g_pos] : -1;
}

void setQueue(const std::vector<std::string>& tracks, int startIndex, bool userChosen = true) {
	g_queue = tracks;
	g_chosen.assign(tracks.size(), userChosen ? 1 : 0);
	g_keepQueue = userChosen;
	g_queueDirty = true;
	rebuildOrder(startIndex);
}

// ---- sleep timer state
const Uint32 SLEEP_FADE_MS = 15000;
float g_fade = 1.0f;             // 1 = normal volume, fades to 0 as the sleep timer runs out
bool g_sleepOn = false;
bool g_sleepAtTrackEnd = false;
bool g_sleepFired = false;
Uint32 g_sleepEnd = 0;           // SDL tick when the timer runs out

int effVol() { return (int)(g_volume * g_fade + 0.5f); } // what is actually sent to the mixer

std::string queueFilePath() {
	return g_configPath.empty() ? std::string() : parentPath(g_configPath) + "/last_queue.txt";
}

// Only a queue that contains tracks YOU chose is kept. An untouched start-up queue is simply rebuilt from the music folder.
void writeQueueFile() {
	const std::string path = queueFilePath();
	if (path.empty()) return;
	bool any = false;
	for (char c : g_chosen) if (c) { any = true; break; }
	if (!any && !g_keepQueue) { remove(path.c_str()); return; } // an untouched automatic queue is just rebuilt next time
	std::ofstream f(path);
	if (!f.is_open()) return;
	f << "#SHADERFUN-QUEUE 1\n";
	for (size_t i = 0; i < g_queue.size(); i++) f << ((i < g_chosen.size() && g_chosen[i]) ? '1' : '0') << '|' << g_queue[i] << '\n';
}

bool readQueueFile(std::vector<std::string>& tracks, std::vector<char>& chosen) {
	const std::string path = queueFilePath();
	if (path.empty()) return false;
	std::ifstream f(path);
	if (!f.is_open()) return false;
	std::string line;
	if (!std::getline(f, line) || line.compare(0, 18, "#SHADERFUN-QUEUE 1") != 0) return false;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.size() < 3 || line[1] != '|') continue;
		chosen.push_back(line[0] == '1' ? 1 : 0);
		tracks.push_back(line.substr(2));
	}
	return !tracks.empty();
}

void freeMusic() {
	if (g_streaming) smbnet::abortStreams(); // do not wait for data that is not wanted any more
	if (g_music) {
		Mix_HaltMusic();
		Mix_FreeMusic(g_music);
		g_music = nullptr;
	}
	g_streaming = false;
}

void fillMetadata(bool skipDuration = false) {
	g_title.clear();
	g_artist.clear();
	g_album.clear();
	g_duration = 0.0;
	if (!g_music) return;
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
	const char* t = Mix_GetMusicTitleTag(g_music);
	const char* a = Mix_GetMusicArtistTag(g_music);
	const char* al = Mix_GetMusicAlbumTag(g_music);
	if (t) g_title = t;
	if (a) g_artist = a;
	if (al) g_album = al;
	if (!skipDuration) {
		const double d = Mix_MusicDuration(g_music);
		if (d > 0.0) g_duration = d;
	}
#endif
}

bool isStreamableName(const std::string& path) {
	static const char* exts[] = { ".mp3", ".flac", ".ogg", ".oga", ".opus", ".wav" };
	const std::string l = lower(path);
	for (const char* e : exts) {
		const size_t n = strlen(e);
		if (l.size() > n && l.compare(l.size() - n, n, e) == 0) return true;
	}
	return false;
}

bool isMp3Name(const std::string& path) {
	const std::string l = lower(path);
	return l.size() > 4 && l.compare(l.size() - 4, 4, ".mp3") == 0;
}

// Length of an MP3 without reading all of it (SDL_mixer's own measurement would read the whole file):
// from the Xing / VBRI header if there is one, otherwise from the bitrate of the first frame.
double estimateMp3Duration(const std::vector<uint8_t>& b, uint64_t fileSize) {
	size_t i = 0;
	if (b.size() >= 10 && memcmp(b.data(), "ID3", 3) == 0)
		i = 10 + (((size_t)(b[6] & 0x7f) << 21) | ((size_t)(b[7] & 0x7f) << 14) | ((size_t)(b[8] & 0x7f) << 7) | (size_t)(b[9] & 0x7f));
	const size_t audioStart = i;
	for (; i + 4 <= b.size() && i < audioStart + 8192; i++) {
		if (b[i] != 0xFF || (b[i + 1] & 0xE0) != 0xE0) continue;
		const int ver = (b[i + 1] >> 3) & 3;     // 3 = MPEG1, 2 = MPEG2, 0 = MPEG2.5
		const int layer = (b[i + 1] >> 1) & 3;   // 1 = layer III, 2 = II, 3 = I
		const int bi = (b[i + 2] >> 4) & 15, si = (b[i + 2] >> 2) & 3;
		if (ver == 1 || layer == 0 || bi == 0 || bi == 15 || si == 3) continue;
		static const int br1[3][15] = { { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
			{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
			{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } };
		static const int br2[3][15] = { { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },
			{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
			{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 } };
		const bool mpeg1 = (ver == 3);
		const int bitrate = (mpeg1 ? br1 : br2)[layer == 3 ? 0 : (layer == 2 ? 1 : 2)][bi];
		static const int sr[3][3] = { { 11025, 12000, 8000 }, { 0, 0, 0 }, { 22050, 24000, 16000 } };
		const int rate = mpeg1 ? (si == 0 ? 44100 : (si == 1 ? 48000 : 32000)) : sr[ver == 2 ? 2 : 0][si];
		if (bitrate <= 0 || rate <= 0) continue;
		const int spf = (layer == 3) ? 384 : ((layer == 2 || mpeg1) ? 1152 : 576);
		const bool mono = ((b[i + 3] >> 6) & 3) == 3;
		const size_t side = mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
		const size_t x = i + 4 + ((b[i + 1] & 1) ? 0 : 2) + side;   // where a Xing / Info header would start
		if (x + 12 <= b.size() && (memcmp(&b[x], "Xing", 4) == 0 || memcmp(&b[x], "Info", 4) == 0) && (b[x + 7] & 1)) {
			const uint32_t frames = ((uint32_t)b[x + 8] << 24) | ((uint32_t)b[x + 9] << 16) | ((uint32_t)b[x + 10] << 8) | b[x + 11];
			if (frames > 0) return (double)frames * spf / rate;
		}
		const size_t v = i + 4 + 32;                                // VBRI header
		if (v + 18 <= b.size() && memcmp(&b[v], "VBRI", 4) == 0) {
			const uint32_t frames = ((uint32_t)b[v + 14] << 24) | ((uint32_t)b[v + 15] << 16) | ((uint32_t)b[v + 16] << 8) | b[v + 17];
			if (frames > 0) return (double)frames * spf / rate;
		}
		if (fileSize > i) return (double)(fileSize - i) * 8.0 / ((double)bitrate * 1000.0); // constant bitrate
		return 0.0;
	}
	return 0.0;
}

// Starts playing a file that is on the SD card. 'shown' is the name used in the log (the smb:// path for network songs).
// g_music is loaded: start it and fill in the song information. streaming = it reads from a stream that is still arriving.
bool beginMusic(const std::string& shown, bool streaming) {
	if (Mix_PlayMusic(g_music, 0) == -1) {
		printf("Failed to play %s: %s\n", shown.c_str(), Mix_GetError());
		Mix_FreeMusic(g_music);
		g_music = nullptr;
		return false;
	}
	Mix_VolumeMusic(effVol());
	g_streaming = streaming;
	g_streamPath = streaming ? shown : std::string();
	if (streaming && isMp3Name(shown)) {
		// the exact length would mean reading the whole MP3, which has not arrived yet: use the header
		fillMetadata(true);
		g_duration = estimateMp3Duration(smbnet::streamHead(shown, 1024 * 1024), smbnet::streamTotal(shown));
	}
	else {
		fillMetadata();
	}
	g_state = PLAYING;
	g_basePos = 0.0;
	g_baseTick = SDL_GetTicks();
	g_changeCounter++;
	printf("Now playing: %s%s\n", shown.c_str(), streaming ? " (streaming)" : "");

	// network songs: the cache only holds the song playing and the next one (which starts downloading now,
	// so the change is instant). Everything else is deleted.
	std::string nextRemote;
	const int n = (int)g_order.size();
	int nextPos = -1;
	if (n > 1) {
		if (g_pos + 1 < n) nextPos = g_pos + 1;
		else if (g_mode == MODE_REPEAT_ALL || g_mode == MODE_SHUFFLE) nextPos = 0; // wraps round
	}
	if (nextPos >= 0) {
		const std::string& next = g_queue[g_order[nextPos]];
		if (smbnet::isRemote(next) && next != shown) nextRemote = next;
	}
	smbnet::trace("playing %s (position %d of %d, mode %d); next to fetch: %s", shown.c_str(), g_pos + 1, n, (int)g_mode,
		nextRemote.empty() ? "(nothing)" : nextRemote.c_str());
	std::vector<std::string> keep;
	if (smbnet::isRemote(shown)) keep.push_back(shown);
	if (!nextRemote.empty()) keep.push_back(nextRemote);
	smbnet::keepOnly(keep);
	if (!nextRemote.empty()) smbnet::prefetch(nextRemote);
	return true;
}

bool startFile(const std::string& path, const std::string& shown) {
	g_music = Mix_LoadMUS(path.c_str());
	if (!g_music) {
		printf("Failed to load %s: %s\n", shown.c_str(), Mix_GetError());
		return false;
	}
	return beginMusic(shown, false);
}

// Loads the current queue entry. autoplay=false is not needed: selecting never loads.
// A network song that is not on the SD card yet is requested; update() starts it when it has arrived.
bool loadCurrent() {
	freeMusic();
	g_loading = false;
	g_loadStream = false;
	g_pauseAfterLoad = false;
	g_curLocal.clear();
	const int qi = currentQueueIndex();
	if (qi < 0) return false;
	const std::string path = g_queue[qi];

	if (smbnet::isRemote(path)) {
		std::string local;
		if (smbnet::cachedFile(path, local)) {
			g_curLocal = local;
			return startFile(local, path);
		}
		g_loading = true;
		g_loadingPath = path;
		// start playing after the first part arrives when the format allows it (and it is not already being downloaded)
		if (isStreamableName(path) && !smbnet::isFetching(path)) {
			smbnet::requestStream(path);
			g_loadStream = true;
		}
		else {
			smbnet::requestFetch(path);
		}
		fillMetadata(); // empty: the title falls back to the file name
		g_state = PAUSED;
		g_basePos = 0.0;
		g_changeCounter++;
		return true;
	}
	return startFile(path, path);
}

// called every frame while a network song is downloading
void pollRemoteLoad() {
	std::string local, err;
	if (g_loadStream) {
		const smbnet::StreamState ss = smbnet::streamState(g_loadingPath, local, err);
		if (ss == smbnet::STREAM_PREPARING) return;
		if (ss == smbnet::STREAM_UNSUPPORTED) { // too big to stream: download it completely
			g_loadStream = false;
			smbnet::requestFetch(g_loadingPath);
			return;
		}
		g_loading = false;
		g_loadStream = false;
		if (ss == smbnet::STREAM_READY) {
			SDL_RWops* rw = smbnet::openStreamRW(g_loadingPath);
			if (rw) {
				g_music = Mix_LoadMUS_RW(rw, 1);
				if (g_music) {
					g_curLocal = local; // the first part is on the SD card already: tags, cover art and lyrics read it
					if (beginMusic(g_loadingPath, true)) {
						if (g_pauseAfterLoad) { g_pauseAfterLoad = false; pause(); }
						return;
					}
				}
				else printf("Failed to stream %s: %s\n", g_loadingPath.c_str(), Mix_GetError());
			}
		}
		g_pauseAfterLoad = false;
		g_curLocal.clear();
		g_state = STOPPED; // could not start it (the reason is shown as a message)
		return;
	}
	const smbnet::FetchState st = smbnet::fetchState(g_loadingPath, local, err);
	if (st == smbnet::FETCH_BUSY) return;
	g_loading = false;
	if (st == smbnet::FETCH_DONE) {
		g_curLocal = local;
		if (startFile(local, g_loadingPath)) {
			if (g_pauseAfterLoad) { g_pauseAfterLoad = false; pause(); }
			return;
		}
	}
	g_pauseAfterLoad = false;
	g_state = STOPPED; // could not get the song: stop (the reason is shown as a message)
}

// Try the current entry; on failure skip forward (bounded) so one bad file doesn't stop playback.
bool loadCurrentOrSkip() {
	const int n = (int)g_order.size();
	for (int tries = 0; tries < n; tries++) {
		if (loadCurrent()) return true;
		if (g_pos + 1 >= n) break;
		g_pos++;
	}
	g_state = STOPPED;
	return false;
}

// Move g_pos by delta in play order. Returns false if it ran off the end and wrap was false.
bool step(int delta, bool wrap) {
	const int n = (int)g_order.size();
	if (n == 0) return false;
	int p = g_pos + delta;
	if (p >= n || p < 0) {
		if (!wrap) return false;
		if (p >= n && g_mode == MODE_SHUFFLE && n > 1) {
			rebuildOrder(-1); // fresh shuffle for the next loop
			return true;      // g_pos is 0 after rebuild
		}
		p = (p % n + n) % n;
	}
	g_pos = p;
	return true;
}

void onTrackEnded() {
	if (g_sleepOn && g_sleepAtTrackEnd) { // sleep timer "after this track": line up the next track, but keep it paused
		g_sleepOn = false;
		g_sleepAtTrackEnd = false;
		g_sleepFired = true;
		const bool moved = (g_mode == MODE_REPEAT_ONE) || step(+1, g_mode != MODE_ONCE);
		if (moved) {
			const float f = g_fade;
			g_fade = 0.0f; // silent, so no blip of the next track
			if (loadCurrentOrSkip()) { if (g_loading) g_pauseAfterLoad = true; else pause(); }
			g_fade = f;
			Mix_VolumeMusic(effVol());
		}
		else {
			freeMusic();
			g_state = STOPPED;
			g_pos = g_order.empty() ? -1 : 0;
			fillMetadata();
		}
		return;
	}
	if (g_mode == MODE_REPEAT_ONE) {
		loadCurrentOrSkip();
		return;
	}
	const bool wrap = (g_mode != MODE_ONCE);
	if (!step(+1, wrap)) {
		// end of queue in "play once" mode
		freeMusic();
		g_state = STOPPED;
		g_pos = g_order.empty() ? -1 : 0;
		fillMetadata();
		return;
	}
	loadCurrentOrSkip();
}

} // namespace

// ================================================================== public helpers
bool isMusicFile(const std::string& name) {
	static const char* exts[] = { ".mp3", ".wav", ".ogg", ".mod", ".xm", ".s3m", ".it",
		".flac", ".aif", ".aiff", ".mid", ".midi", ".opus" };
	std::string l = lower(name);
	for (const char* e : exts) {
		size_t n = strlen(e);
		if (l.size() > n && l.compare(l.size() - n, n, e) == 0) return true;
	}
	return false;
}

std::string joinPath(const std::string& dir, const std::string& name) {
	if (dir.empty()) return name;
	if (dir.back() == '/') return dir + name;
	return dir + "/" + name;
}

std::string parentPath(const std::string& path) {
	std::string p = path;
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	size_t s = p.find_last_of('/');
	if (s == std::string::npos) return "";
	if (s == 0) return p.size() > 1 ? "/" : "";
	std::string par = p.substr(0, s);
	if (par.back() == ':') par += '/'; // "sdmc:" -> "sdmc:/"
	return par;
}

std::string baseName(const std::string& path) {
	std::string p = path;
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	size_t s = p.find_last_of('/');
	std::string b = (s == std::string::npos) ? p : p.substr(s + 1);
	return b.empty() ? p : b;
}

std::vector<DirEntry> listDirectory(const std::string& dir) {
	std::vector<DirEntry> dirs, files;
	DIR* d = opendir(dir.c_str());
	if (!d) return dirs;
	struct dirent* ent;
	while ((ent = readdir(d)) != NULL) {
		std::string name = ent->d_name;
		if (name.empty() || name[0] == '.') continue;
		DirEntry e;
		e.name = name;
		e.path = joinPath(dir, name);
		e.isDir = pathIsDir(e.path, ent);
		if (e.isDir) dirs.push_back(e);
		else if (isMusicFile(name)) files.push_back(e);
	}
	closedir(d);
	auto cmp = [](const DirEntry& a, const DirEntry& b) { return naturalLess(a.name, b.name); };
	std::sort(dirs.begin(), dirs.end(), cmp);
	std::sort(files.begin(), files.end(), cmp);
	dirs.insert(dirs.end(), files.begin(), files.end());
	return dirs;
}

// ================================================================== lifecycle
void init(const char* configPath) {
	g_rng.seed((unsigned)SDL_GetTicks() ^ 0x5eed1234u);
	g_configPath = configPath ? configPath : "";
	loadConfig();
	// songs downloaded in a previous run: kept only if the user chose that (otherwise they were only temporary)
	smbnet::setKeepCache(configInt("keep_cache", 0) != 0);
	if (smbnet::keepCache()) smbnet::cleanIncomplete();
	else smbnet::clearCache();
	Mix_VolumeMusic(g_volume);

	// Queue: the one you chose last time if there was one, otherwise the music folder
	std::vector<std::string> tracks;
	std::vector<char> chosen;
	const bool restoredQueue = readQueueFile(tracks, chosen);
	if (!restoredQueue) {
		scanRecursive(g_musicFolder, tracks, 0);
		if (tracks.empty()) scanRecursive("romfs:/music", tracks, 0);
	}

	// Which track was current? (by index when it still matches, otherwise by path)
	const std::string lastPath = g_extra.count("last_track") ? g_extra["last_track"] : std::string();
	int startIdx = -1;
	if (!lastPath.empty()) {
		const int qi = configInt("last_qi", -1);
		if (qi >= 0 && qi < (int)tracks.size() && tracks[qi] == lastPath) startIdx = qi;
		else for (size_t i = 0; i < tracks.size(); i++) if (tracks[i] == lastPath) { startIdx = (int)i; break; }
	}

	setQueue(tracks, tracks.empty() ? -1 : (startIdx >= 0 ? startIdx : 0), false);
	// The restored queue is kept for next time, but nothing in it counts as "chosen" in THIS session:
	// "Save my queued songs as a playlist" must only contain what you pick now, not last session's album.
	g_keepQueue = restoredQueue;
	g_queueDirty = false;
	g_state = STOPPED;

	// Get the last track ready at its old position (paused, unless "Resume on launch" is set to playing)
	struct stat lst;
	bool lastIsReady = false;
	if (startIdx >= 0) {
		if (smbnet::isRemote(lastPath)) { std::string cachedCopy; lastIsReady = smbnet::cachedFile(lastPath, cachedCopy); } // a network song kept in the cache
		else lastIsReady = stat(lastPath.c_str(), &lst) == 0;
	}
	if (lastIsReady) {
		const int savedVol = g_volume;
		g_volume = 0; // silent while it loads and seeks, so there is no blip of sound
		Mix_VolumeMusic(0);
		if (loadCurrent()) {
			const double pos = configInt("last_pos_ms", 0) / 1000.0;
			if (pos > 1.0) seekTo(pos);
			if (!configInt("resume_play", 0)) pause();
		}
		g_volume = savedVol;
		Mix_VolumeMusic(g_volume);
	}
	printf("Player ready: %zu tracks queued from %s\n", g_queue.size(), g_musicFolder.c_str());
}

void shutdown() {
	saveState(); // last track, position, queue (before the music is freed)
	freeMusic();
	g_state = STOPPED;
	g_queue.clear();
	g_chosen.clear();
	g_order.clear();
}

void update() {
	if (g_loading) pollRemoteLoad();
	else if (g_state == PLAYING && !Mix_PlayingMusic()) onTrackEnded();

	// sleep timer: fade out over the last SLEEP_FADE_MS, then pause
	if (g_sleepOn && !g_sleepAtTrackEnd) {
		const int32_t left = (int32_t)(g_sleepEnd - SDL_GetTicks()); // wrap-safe
		if (left <= 0) {
			if (g_state == PLAYING) pause();
			g_fade = 1.0f; // back to normal volume for when you press play again
			Mix_VolumeMusic(effVol());
			g_sleepOn = false;
			g_sleepFired = true;
		}
		else if (left < (int32_t)SLEEP_FADE_MS && g_state == PLAYING) {
			g_fade = (float)left / (float)SLEEP_FADE_MS;
			Mix_VolumeMusic(effVol());
		}
		else if (g_fade != 1.0f) {
			g_fade = 1.0f;
			Mix_VolumeMusic(effVol());
		}
	}
}

void setSleepTimer(int minutes) {
	g_sleepFired = false;
	g_fade = 1.0f;
	Mix_VolumeMusic(effVol());
	if (minutes == 0) { g_sleepOn = false; g_sleepAtTrackEnd = false; return; }
	g_sleepOn = true;
	g_sleepAtTrackEnd = (minutes < 0);
	if (minutes > 0) g_sleepEnd = SDL_GetTicks() + (Uint32)minutes * 60000u;
}

bool sleepActive() { return g_sleepOn; }
bool sleepAtTrackEnd() { return g_sleepOn && g_sleepAtTrackEnd; }
int sleepSecondsLeft() {
	if (!g_sleepOn || g_sleepAtTrackEnd) return -1;
	const int32_t left = (int32_t)(g_sleepEnd - SDL_GetTicks());
	return left > 0 ? (int)(left / 1000) : 0;
}
bool sleepFading() { return g_sleepOn && !g_sleepAtTrackEnd && g_fade < 1.0f; }
bool consumeSleepFired() {
	const bool f = g_sleepFired;
	g_sleepFired = false;
	return f;
}

int configInt(const char* key, int defaultValue) {
	auto it = g_extra.find(key);
	return it == g_extra.end() ? defaultValue : atoi(it->second.c_str());
}

std::string configString(const char* key, const std::string& defaultValue) {
	auto it = g_extra.find(key);
	return it == g_extra.end() ? defaultValue : it->second;
}

void setConfigString(const char* key, const std::string& value, bool saveNow) {
	g_extra[key] = value;
	if (saveNow) saveConfig();
}

void saveState() {
	const int qi = currentQueueIndex();
	if (qi >= 0 && qi < (int)g_queue.size()) {
		g_extra["last_track"] = g_queue[qi];
		g_extra["last_qi"] = std::to_string(qi);
		g_extra["last_pos_ms"] = std::to_string((long)(position() * 1000.0));
	}
	if (g_queueDirty) { writeQueueFile(); g_queueDirty = false; }
	saveConfig();
}

void setConfigInt(const char* key, int value) {
	g_extra[key] = std::to_string(value);
	saveConfig();
}

// ================================================================== folder / queue actions
const std::string& musicFolder() { return g_musicFolder; }

void setMusicFolder(const std::string& dir) {
	g_musicFolder = dir;
	saveConfig();
	if (g_state == STOPPED) { // nothing is playing: load the new folder as the idle queue
		std::vector<std::string> tracks;
		scanRecursive(dir, tracks, 0);
		if (!tracks.empty()) setQueue(tracks, 0, false); // idle queue, not chosen by the user
	}
}

bool playFolder(const std::string& dir) {
	std::vector<std::string> tracks;
	scanRecursive(dir, tracks, 0);
	if (tracks.empty()) return false;
	setQueue(tracks, 0);
	return loadCurrentOrSkip();
}

bool playFile(const std::string& file) {
	std::string dir = parentPath(file);
	std::vector<std::string> tracks;
	for (const DirEntry& e : listDirectory(dir)) {
		if (!e.isDir) tracks.push_back(e.path);
	}
	int idx = -1;
	for (size_t i = 0; i < tracks.size(); i++) {
		if (tracks[i] == file) { idx = (int)i; break; }
	}
	if (idx < 0) {
		tracks.push_back(file);
		idx = (int)tracks.size() - 1;
	}
	setQueue(tracks, idx);
	return loadCurrentOrSkip();
}

bool playTracks(const std::vector<std::string>& tracks, int startIndex) {
	if (tracks.empty()) return false;
	if (startIndex < 0 || startIndex >= (int)tracks.size()) startIndex = 0;
	setQueue(tracks, startIndex);
	return loadCurrentOrSkip();
}

std::vector<std::string> queuePaths() { return g_queue; }

std::vector<std::string> chosenQueuePaths() {
	std::vector<std::string> out;
	for (size_t i = 0; i < g_queue.size(); i++)
		if (i < g_chosen.size() && g_chosen[i]) out.push_back(g_queue[i]);
	return out;
}

std::vector<std::string> tracksInFolder(const std::string& dir) {
	std::vector<std::string> tracks;
	scanRecursive(dir, tracks, 0);
	return tracks;
}

int enqueue(const std::vector<std::string>& tracks, bool playNext) {
	if (tracks.empty()) return 0;
	const int k = (int)tracks.size();
	if (g_queue.empty()) { // nothing queued: this becomes the queue
		setQueue(tracks, 0);
		return loadCurrentOrSkip() ? k : 0;
	}
	const int n = (int)g_queue.size();
	if (g_mode == MODE_SHUFFLE) {
		// the new tracks go at the end of g_queue; only the play order decides when they are heard
		g_queue.insert(g_queue.end(), tracks.begin(), tracks.end());
		g_chosen.insert(g_chosen.end(), k, 1);
		g_queueDirty = true;
		const int at = (playNext && g_pos >= 0) ? g_pos + 1 : (int)g_order.size();
		std::vector<int> add(k);
		for (int i = 0; i < k; i++) add[i] = n + i;
		g_order.insert(g_order.begin() + at, add.begin(), add.end());
	}
	else {
		// in order modes the play order is the queue itself, so insert straight into it
		const int at = (playNext && g_pos >= 0) ? g_pos + 1 : n;
		g_queue.insert(g_queue.begin() + at, tracks.begin(), tracks.end());
		g_chosen.insert(g_chosen.begin() + at, k, 1);
		g_queueDirty = true;
		g_order.resize(n + k);
		for (int i = 0; i < n + k; i++) g_order[i] = i;
	}
	return k;
}

// ================================================================== transport
void play() {
	if (g_order.empty() || g_loading) return;
	if (g_state == PAUSED) {
		Mix_ResumeMusic();
		g_baseTick = SDL_GetTicks();
		g_state = PLAYING;
	}
	else if (g_state == STOPPED) {
		loadCurrentOrSkip();
	}
}

void pause() {
	if (g_state != PLAYING) return;
	g_basePos = position();
	Mix_PauseMusic();
	g_state = PAUSED;
}

void togglePause() {
	if (g_state == PLAYING) pause();
	else play();
}

void stop() {
	g_loading = false;
	g_curLocal.clear();
	freeMusic();
	g_state = STOPPED;
	fillMetadata();
}

void next() {
	if (g_order.empty()) return;
	if (!step(+1, true)) return;
	loadCurrentOrSkip();
}

void prev() {
	if (g_order.empty()) return;
	if (g_state != STOPPED && position() > SEEK_STEP_RESTART_THRESHOLD) {
		loadCurrentOrSkip(); // restart this track
		return;
	}
	step(-1, g_mode != MODE_ONCE);
	loadCurrentOrSkip();
}

bool canSeek() {
	const int qi = currentQueueIndex();
	if (g_streaming && !smbnet::streamComplete(g_streamPath)) return false; // part of the song has not arrived yet
	return g_music && g_state != STOPPED && qi >= 0 && !isTrackerFormat(g_queue[qi]);
}

void seekTo(double seconds) {
	if (!canSeek()) return;
	if (seconds < 0.0) seconds = 0.0;
	if (g_duration > 1.0 && seconds > g_duration - 0.5) seconds = g_duration - 0.5;
	if (Mix_SetMusicPosition(seconds) == 0) {
		g_basePos = seconds;
		g_baseTick = SDL_GetTicks();
	}
	else {
		printf("Seek not supported for this track: %s\n", Mix_GetError());
	}
}

void seekBy(double seconds) {
	if (!canSeek()) return;
	seekTo(position() + seconds);
}

void setVolume(int v) {
	g_volume = std::max(0, std::min(MIX_MAX_VOLUME, v));
	Mix_VolumeMusic(effVol());
}

void setMode(PlayMode m) {
	if (m == g_mode) return;
	const int cur = currentQueueIndex();
	g_mode = m;
	// reorder, keeping the current track current (switching to/from shuffle)
	rebuildOrder(cur);
	saveConfig();
}

void cycleMode() { setMode((PlayMode)((g_mode + 1) % MODE_COUNT)); }

// ================================================================== state
bool hasTracks() { return !g_queue.empty(); }
bool isPlaying() { return g_state == PLAYING; }
bool isPaused() { return g_state == PAUSED; }
int volume() { return g_volume; }
PlayMode mode() { return g_mode; }

const char* modeName(PlayMode m) {
	switch (m) {
	case MODE_REPEAT_ALL: return "Repeat all";
	case MODE_SHUFFLE: return "Shuffle";
	case MODE_REPEAT_ONE: return "Repeat one";
	default: return "Play once";
	}
}

double position() {
	if (!g_music || g_state == STOPPED) return 0.0;
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
	double p = Mix_GetMusicPosition(g_music);
	if (p >= 0.0) return p;
#endif
	if (g_state == PAUSED) return g_basePos;
	return g_basePos + (SDL_GetTicks() - g_baseTick) / 1000.0;
}

double duration() { return g_duration; }
int queueIndex() { return g_pos; }
int queueSize() { return (int)g_queue.size(); }

const std::string& currentPath() {
	if (!g_curLocal.empty()) return g_curLocal;
	const int qi = currentQueueIndex();
	return qi >= 0 ? g_queue[qi] : g_emptyString;
}
bool isLoading() { return g_loading; }
float loadProgress() { return g_loading ? (g_loadStream ? smbnet::streamProgress(g_loadingPath) : smbnet::fetchProgress(g_loadingPath)) : -1.0f; }

std::string title() {
	if (!g_title.empty()) return g_title;
	const std::string& p = currentPath();
	if (p.empty()) return "";
	return stripExtension(baseName(p));
}

std::string subtitle() {
	std::string s = g_artist;
	if (!g_album.empty()) s += s.empty() ? g_album : "  -  " + g_album;
	if (!s.empty()) return s;
	const std::string& p = currentPath();
	return p.empty() ? "" : baseName(parentPath(p));
}

std::string nowPlayingText() {
	std::string t = title();
	if (t.empty()) return "";
	if (!g_artist.empty()) return g_artist + " - " + t;
	return t;
}

uint32_t trackChangeCounter() { return g_changeCounter; }

} // namespace player
