#include "player.h"

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

void setQueue(const std::vector<std::string>& tracks, int startIndex) {
	g_queue = tracks;
	rebuildOrder(startIndex);
}

void freeMusic() {
	if (g_music) {
		Mix_HaltMusic();
		Mix_FreeMusic(g_music);
		g_music = nullptr;
	}
}

void fillMetadata() {
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
	double d = Mix_MusicDuration(g_music);
	if (d > 0.0) g_duration = d;
#endif
}

// Loads the current queue entry. autoplay=false is not needed: selecting never loads.
bool loadCurrent() {
	freeMusic();
	const int qi = currentQueueIndex();
	if (qi < 0) return false;
	const std::string& path = g_queue[qi];

	g_music = Mix_LoadMUS(path.c_str());
	if (!g_music) {
		printf("Failed to load %s: %s\n", path.c_str(), Mix_GetError());
		return false;
	}
	if (Mix_PlayMusic(g_music, 0) == -1) {
		printf("Failed to play %s: %s\n", path.c_str(), Mix_GetError());
		Mix_FreeMusic(g_music);
		g_music = nullptr;
		return false;
	}
	Mix_VolumeMusic(g_volume);
	fillMetadata();
	g_state = PLAYING;
	g_basePos = 0.0;
	g_baseTick = SDL_GetTicks();
	g_changeCounter++;
	printf("Now playing: %s\n", path.c_str());
	return true;
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
	Mix_VolumeMusic(g_volume);

	std::vector<std::string> tracks;
	scanRecursive(g_musicFolder, tracks, 0);
	if (tracks.empty()) scanRecursive("romfs:/music", tracks, 0);
	setQueue(tracks, tracks.empty() ? -1 : 0);
	g_state = STOPPED;
	printf("Player ready: %zu tracks queued from %s\n", g_queue.size(), g_musicFolder.c_str());
}

void shutdown() {
	saveConfig();
	freeMusic();
	g_state = STOPPED;
	g_queue.clear();
	g_order.clear();
}

void update() {
	if (g_state == PLAYING && !Mix_PlayingMusic()) onTrackEnded();
}

int configInt(const char* key, int defaultValue) {
	auto it = g_extra.find(key);
	return it == g_extra.end() ? defaultValue : atoi(it->second.c_str());
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
		if (!tracks.empty()) setQueue(tracks, 0);
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
		const int at = (playNext && g_pos >= 0) ? g_pos + 1 : (int)g_order.size();
		std::vector<int> add(k);
		for (int i = 0; i < k; i++) add[i] = n + i;
		g_order.insert(g_order.begin() + at, add.begin(), add.end());
	}
	else {
		// in order modes the play order is the queue itself, so insert straight into it
		const int at = (playNext && g_pos >= 0) ? g_pos + 1 : n;
		g_queue.insert(g_queue.begin() + at, tracks.begin(), tracks.end());
		g_order.resize(n + k);
		for (int i = 0; i < n + k; i++) g_order[i] = i;
	}
	return k;
}

// ================================================================== transport
void play() {
	if (g_order.empty()) return;
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
	Mix_VolumeMusic(g_volume);
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
	const int qi = currentQueueIndex();
	return qi >= 0 ? g_queue[qi] : g_emptyString;
}

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
