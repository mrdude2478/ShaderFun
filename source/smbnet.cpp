#include "smbnet.h"

#include <SDL2/SDL.h>
#include <switch.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>

#include <stdarg.h>

#include <algorithm>
#include <deque>
#include <map>
#include <memory>

namespace smbnet {
namespace {

const char* const INI_PATH = "sdmc:/switch/shaderfun/smb.ini";
const char* const CACHE_DIR = "sdmc:/switch/shaderfun/cache";
const size_t CACHE_MAX_FILES = 8;  // safety net only: keepOnly() normally leaves just 2 songs
const uint64_t CACHE_MAX_BYTES = 600ull * 1024ull * 1024ull; // 600 MB

typedef Config Settings;

struct FetchInfo {
	FetchState state = FETCH_NONE;
	std::string local, error;
	uint64_t done = 0, total = 0;
};

// ---- shared state (everything below is guarded by g_mu)
SDL_mutex* g_mu = nullptr;
SDL_cond* g_cv = nullptr;
SDL_Thread* g_thread = nullptr;
bool g_quit = false;
bool g_needReconnect = true;
Settings g_settings;

bool g_listPending = false;
std::string g_listDir;
bool g_listFinished = false, g_listOk = false;
std::string g_listResultDir, g_listError;
std::vector<Entry> g_listEntries;

std::deque<std::string> g_urgent, g_background;
std::map<std::string, FetchInfo> g_fetch;
std::string g_wanted;  // the song the user is waiting for right now
std::string g_current; // the song the worker is downloading right now

std::string g_message;
bool g_messageFresh = false;

// ---- streaming. The song is collected in memory (data) so the player can read it while it arrives; the cache file
// on the SD card is only written once (the first part early, for tags/cover art, and the whole file at the end).
const uint64_t STREAM_HEAD = 1024ull * 1024ull;          // what must have arrived before playing starts
const uint64_t STREAM_TAIL = 256ull * 1024ull;           // fetched first: decoders look at the end of the file (ID3v1, Ogg length...)
const uint64_t STREAM_MAX_BYTES = 64ull * 1024ull * 1024ull; // bigger songs are downloaded completely before playing

struct Stream {
	std::string remote, local, marker;
	uint64_t total = 0, written = 0, tailStart = 0, pos = 0;
	uint8_t* data = nullptr;
	bool tailReady = false, headReady = false, complete = false, failed = false, cancelled = false, unsupported = false;
	std::string error;
	~Stream() { free(data); }
};
std::shared_ptr<Stream> g_stream;   // the song being streamed
std::string g_streamPending;        // waiting for the worker to pick it up
SDL_cond* g_streamCv = nullptr;     // readers wait here for more data
volatile int g_workerExited = 0;
SDL_mutex* g_logMu = nullptr;
FILE* g_log = nullptr;

// ---- only the worker thread touches these
smb2_context* g_ctx = nullptr;
bool g_socketsStarted = false;

void initSync() {
	if (!g_mu) {
		g_mu = SDL_CreateMutex();
		g_cv = SDL_CreateCond();
		g_streamCv = SDL_CreateCond();
		g_logMu = SDL_CreateMutex();
	}
}

// ------------------------------------------------------------------ helpers
std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
	return s.substr(a, b - a);
}

std::string lowerStr(std::string s) {
	for (char& c : s) c = (char)tolower((unsigned char)c);
	return s;
}

bool fileExists(const std::string& p) {
	struct stat st;
	return stat(p.c_str(), &st) == 0;
}

std::string baseName(const std::string& p) {
	const size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? p : p.substr(s + 1);
}

std::string withoutExt(const std::string& p) {
	const size_t dot = p.find_last_of('.');
	const size_t slash = p.find_last_of("/\\");
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return p;
	return p.substr(0, dot);
}

void ensureDirs() {
	mkdir("sdmc:/switch", 0777);
	mkdir("sdmc:/switch/shaderfun", 0777);
	mkdir(CACHE_DIR, 0777);
}

void setMessage(const std::string& m) {
	g_message = m;
	g_messageFresh = true;
}

// where a remote song is stored: <hash>_<name>.<ext> (the hash keeps same-named songs from different folders apart)
std::string cachePath(const std::string& remote) {
	uint32_t h = 2166136261u;
	for (unsigned char c : remote) { h ^= c; h *= 16777619u; }
	char hex[16];
	snprintf(hex, sizeof(hex), "%08x", h);
	std::string name = baseName(remote);
	for (char& c : name) if (strchr("/\\:*?\"<>|", c)) c = '_';
	return std::string(CACHE_DIR) + "/" + hex + "_" + name;
}

std::string remoteInner(const std::string& remote) { // "smb://a/b.mp3" -> "a/b.mp3"
	return remote.size() > 6 ? remote.substr(6) : std::string();
}

// keeps the cache from growing without limit: newest files stay
void trimCache() {
	struct Item { std::string path; time_t mtime; uint64_t size; };
	std::vector<Item> items;
	DIR* d = opendir(CACHE_DIR);
	if (!d) return;
	while (dirent* e = readdir(d)) {
		if (e->d_name[0] == '.') continue;
		const std::string p = std::string(CACHE_DIR) + "/" + e->d_name;
		struct stat st;
		if (stat(p.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) continue;
		items.push_back({ p, st.st_mtime, (uint64_t)st.st_size });
	}
	closedir(d);
	std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.mtime > b.mtime; });
	uint64_t total = 0;
	for (size_t i = 0; i < items.size(); i++) {
		total += items[i].size;
		if (i >= CACHE_MAX_FILES || total > CACHE_MAX_BYTES) remove(items[i].path.c_str());
	}
}

// ------------------------------------------------------------------ settings file
bool writeIni(const Settings& s) {
	ensureDirs();
	FILE* f = fopen(INI_PATH, "w");
	if (!f) return false;
	fprintf(f,
		"# ShaderFun - music from a network share (SMB2 / SMB3; the old SMB1 is not supported)\n"
		"# You can change all of this in the app (library menu > Network > Network settings), or edit this file.\n"
		"#\n"
		"# server: address of the router / NAS / PC\n"
		"server=%s\n"
		"# share: the share name, as shown under \\\\server in Windows Explorer (for a USB drive on a router, often the drive's name)\n"
		"share=%s\n"
		"# user / password: leave both empty to connect as a guest\n"
		"user=%s\n"
		"password=%s\n"
		"# domain: only needed for some Windows domains\n"
		"domain=%s\n"
		"# folder: optional folder inside the share to start in, for example Music\n"
		"folder=%s\n",
		s.server.c_str(), s.share.c_str(), s.user.c_str(), s.password.c_str(), s.domain.c_str(), s.folder.c_str());
	fclose(f);
	return true;
}

void writeTemplate() { writeIni(Settings()); }

bool readSettings(Settings& s) {
	FILE* f = fopen(INI_PATH, "r");
	if (!f) return false;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		const std::string l = trim(line);
		if (l.empty() || l[0] == '#' || l[0] == ';') continue;
		const size_t eq = l.find('=');
		if (eq == std::string::npos) continue;
		const std::string key = lowerStr(trim(l.substr(0, eq)));
		std::string val = trim(l.substr(eq + 1));
		if (key == "server") s.server = val;
		else if (key == "share") s.share = val;
		else if (key == "user") s.user = val;
		else if (key == "password") s.password = val;
		else if (key == "domain") s.domain = val;
		else if (key == "folder") {
			while (!val.empty() && (val[0] == '/' || val[0] == '\\')) val.erase(0, 1);
			while (!val.empty() && (val.back() == '/' || val.back() == '\\')) val.pop_back();
			s.folder = val;
		}
	}
	fclose(f);
	return true;
}

// ------------------------------------------------------------------ connection (worker thread)
void disconnect(bool graceful = true) {
	if (g_ctx) {
		if (graceful) smb2_disconnect_share(g_ctx); // not when quitting: the network may be gone and the call could hang
		smb2_destroy_context(g_ctx);
		g_ctx = nullptr;
	}
}

bool ensureConnected(std::string& err) {
	SDL_LockMutex(g_mu);
	const bool reconnect = g_needReconnect;
	g_needReconnect = false;
	Settings s = g_settings;
	SDL_UnlockMutex(g_mu);
	if (s.server.empty() || s.share.empty()) { // not loaded yet (Network was not opened in this run): read smb.ini now
		Settings fresh;
		if (readSettings(fresh) && !fresh.server.empty() && !fresh.share.empty()) {
			SDL_LockMutex(g_mu);
			g_settings = fresh;
			SDL_UnlockMutex(g_mu);
			s = fresh;
		}
	}

	if (reconnect) disconnect();
	if (g_ctx) return true;

	if (s.server.empty() || s.share.empty()) {
		err = "Set up the network share first (library menu > Network)";
		return false;
	}
	trace("connecting to %s / %s as %s", s.server.c_str(), s.share.c_str(), s.user.empty() ? "guest" : s.user.c_str());
	g_ctx = smb2_init_context();
	if (!g_ctx) { err = "Out of memory"; return false; }
	smb2_set_security_mode(g_ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
	smb2_set_timeout(g_ctx, 15);
	smb2_set_password(g_ctx, s.password.c_str());
	if (!s.domain.empty()) smb2_set_domain(g_ctx, s.domain.c_str());
	const std::string user = s.user.empty() ? "guest" : s.user;
	if (smb2_connect_share(g_ctx, s.server.c_str(), s.share.c_str(), user.c_str()) < 0) {
		err = std::string("Could not connect: ") + smb2_get_error(g_ctx);
		trace("%s", err.c_str());
		smb2_destroy_context(g_ctx);
		g_ctx = nullptr;
		return false;
	}
	trace("connected");
	return true;
}

void markBroken() {
	SDL_LockMutex(g_mu);
	g_needReconnect = true;
	SDL_UnlockMutex(g_mu);
}

// ------------------------------------------------------------------ jobs (worker thread)
void doList(const std::string& dir) {
	std::vector<Entry> out;
	std::string err;
	bool ok = false;
	for (int attempt = 0; attempt < 2 && !ok; attempt++) {
		if (!ensureConnected(err)) break;
		smb2dir* d = smb2_opendir(g_ctx, dir.c_str());
		if (!d) {
			err = std::string("Could not open the folder: ") + smb2_get_error(g_ctx);
			markBroken();
			continue;
		}
		out.clear();
		while (smb2dirent* ent = smb2_readdir(g_ctx, d)) {
			const std::string name = ent->name ? ent->name : "";
			if (name.empty() || name[0] == '.' || name == "System Volume Information" || name == "$RECYCLE.BIN" || name == "lost+found") continue;
			Entry e;
			e.name = name;
			if (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) e.isDir = true;
			else if (ent->st.smb2_type != SMB2_TYPE_FILE) continue;
			e.size = ent->st.smb2_size;
			out.push_back(e);
		}
		smb2_closedir(g_ctx, d);
		ok = true;
	}
	SDL_LockMutex(g_mu);
	g_listFinished = true;
	g_listOk = ok;
	g_listResultDir = dir;
	g_listError = ok ? std::string() : err;
	g_listEntries = ok ? out : std::vector<Entry>();
	SDL_UnlockMutex(g_mu);
}

// runs a waiting folder listing (also called in the middle of a download, so browsing stays quick)
void serveList() {
	SDL_LockMutex(g_mu);
	const bool pending = g_listPending;
	const std::string dir = g_listDir;
	g_listPending = false;
	SDL_UnlockMutex(g_mu);
	if (pending) doList(dir);
}

// copies one remote file into 'localPath' (via a .part file). Returns 1 = done, 0 = failed (err set), -1 = given up on purpose
int downloadFile(const std::string& inner, const std::string& localPath, const std::string& infoKey, bool background, bool optionalFile, std::string& err) {
	static uint8_t* buf = nullptr;
	const size_t BUF = 256 * 1024;
	if (!buf) buf = (uint8_t*)malloc(BUF);
	if (!buf) { err = "Out of memory"; return 0; }

	for (int attempt = 0; attempt < 2; attempt++) {
		if (!ensureConnected(err)) return 0;
		smb2fh* fh = smb2_open(g_ctx, inner.c_str(), O_RDONLY);
		if (!fh) {
			err = std::string("Could not open the song: ") + smb2_get_error(g_ctx);
			if (optionalFile) return 0; // e.g. no .lrc next to the song: that is normal
			markBroken();
			continue;
		}
		smb2_stat_64 st;
		uint64_t total = 0;
		if (smb2_fstat(g_ctx, fh, &st) == 0) total = st.smb2_size;
		const std::string part = localPath + ".part";
		FILE* f = fopen(part.c_str(), "wb");
		if (!f) { smb2_close(g_ctx, fh); err = "Could not write to the SD card cache"; return 0; }

		uint64_t done = 0;
		bool failed = false, gaveUp = false;
		while (true) {
			SDL_LockMutex(g_mu);
			const bool quit = g_quit;
			const bool listWaiting = g_listPending;
			const bool urgentWaiting = !g_urgent.empty() || !g_streamPending.empty();
			const bool stillWanted = (infoKey == g_wanted);
			if (!infoKey.empty() && g_fetch.count(infoKey)) { g_fetch[infoKey].done = done; g_fetch[infoKey].total = total; }
			SDL_UnlockMutex(g_mu);
			if (quit) { gaveUp = true; break; }
			if (background && urgentWaiting) { gaveUp = true; trace("background download: paused for something more important"); break; } // the user wants something else first
			if (!background && !infoKey.empty() && !stillWanted) { gaveUp = true; break; } // they moved on to another song
			if (listWaiting) serveList();
			const int n = smb2_read(g_ctx, fh, buf, (uint32_t)BUF);
			if (n < 0) { err = std::string("Download failed: ") + smb2_get_error(g_ctx); failed = true; break; }
			if (n == 0) break;
			if (fwrite(buf, 1, (size_t)n, f) != (size_t)n) { err = "SD card is full or not writable"; failed = true; break; }
			done += (uint64_t)n;
		}
		fclose(f);
		smb2_close(g_ctx, fh);
		trace("download of %s: %llu of %llu bytes, %s", inner.c_str(), (unsigned long long)done, (unsigned long long)total, gaveUp ? "stopped" : (failed ? "failed" : "complete"));
		if (gaveUp) { remove(part.c_str()); return -1; }
		if (failed) {
			remove(part.c_str());
			markBroken();
			continue;
		}
		remove(localPath.c_str());
		if (rename(part.c_str(), localPath.c_str()) != 0) { err = "Could not finish the download"; remove(part.c_str()); return 0; }
		return 1;
	}
	return 0;
}

void doFetch(const std::string& remote, bool background) {
	const std::string local = cachePath(remote);
	std::string err;
	int result;
	if (fileExists(local)) {
		result = 1;
	}
	else {
		ensureDirs();
		result = downloadFile(remoteInner(remote), local, remote, background, false, err);
	}
	if (result == 1) {
		// a .lrc file next to the song comes along, so lyrics work for network songs too
		const std::string lrcLocal = withoutExt(local) + ".lrc";
		if (!fileExists(lrcLocal)) {
			std::string ignore;
			downloadFile(withoutExt(remoteInner(remote)) + ".lrc", lrcLocal, std::string(), true, true, ignore);
		}
		trimCache();
	}
	trace("download of %s: %s", remote.c_str(), result == 1 ? "done" : (result == 0 ? ("FAILED: " + err).c_str() : "given up"));
	SDL_LockMutex(g_mu);
	FetchInfo& info = g_fetch[remote];
	if (result == 1) { info.state = FETCH_DONE; info.local = local; info.error.clear(); }
	else if (result == 0) { info.state = FETCH_FAILED; info.error = err; setMessage(err); }
	else info.state = FETCH_NONE; // given up on purpose
	SDL_UnlockMutex(g_mu);
}

// Downloads a song into memory for streaming (worker thread)
void doStream(std::shared_ptr<Stream> sp) {
	std::string err;
	auto fail = [&](const std::string& e) {
		SDL_LockMutex(g_mu);
		sp->failed = true;
		sp->error = e;
		setMessage(e);
		SDL_CondBroadcast(g_streamCv);
		SDL_UnlockMutex(g_mu);
	};
	const std::string inner = remoteInner(sp->remote);
	ensureDirs();

	// the .lrc file first (it is tiny), so lyrics are ready when the song starts
	{
		const std::string lrcLocal = withoutExt(sp->local) + ".lrc";
		if (!fileExists(lrcLocal)) {
			std::string ignore;
			downloadFile(withoutExt(inner) + ".lrc", lrcLocal, std::string(), true, true, ignore);
		}
	}

	smb2fh* fh = nullptr;
	for (int attempt = 0; attempt < 2 && !fh; attempt++) {
		if (!ensureConnected(err)) { fail(err); return; }
		fh = smb2_open(g_ctx, inner.c_str(), O_RDONLY);
		if (!fh) { err = std::string("Could not open the song: ") + smb2_get_error(g_ctx); markBroken(); }
	}
	if (!fh) { fail(err); return; }
	smb2_stat_64 st;
	uint64_t total = 0;
	if (smb2_fstat(g_ctx, fh, &st) == 0) total = st.smb2_size;
	if (total == 0) { smb2_close(g_ctx, fh); fail("The song is empty or its size is unknown"); return; }

	uint8_t* mem = total <= STREAM_MAX_BYTES ? (uint8_t*)malloc((size_t)total) : nullptr;
	if (!mem) { // too big (or no memory): the player downloads it completely instead
		smb2_close(g_ctx, fh);
		SDL_LockMutex(g_mu);
		sp->unsupported = true;
		SDL_CondBroadcast(g_streamCv);
		SDL_UnlockMutex(g_mu);
		return;
	}
	SDL_LockMutex(g_mu);
	sp->data = mem;
	sp->total = total;
	SDL_UnlockMutex(g_mu);

	// the end of the file first
	if (total > STREAM_HEAD + STREAM_TAIL) {
		uint64_t got = 0;
		const uint64_t start = total - STREAM_TAIL;
		while (got < STREAM_TAIL) {
			const int n = smb2_pread(g_ctx, fh, mem + start + got, (uint32_t)(STREAM_TAIL - got), start + got);
			if (n <= 0) break;
			got += (uint64_t)n;
		}
		if (got != STREAM_TAIL) {
			err = std::string("Download failed: ") + smb2_get_error(g_ctx);
			smb2_close(g_ctx, fh);
			markBroken();
			fail(err);
			return;
		}
		SDL_LockMutex(g_mu);
		sp->tailStart = start;
		sp->tailReady = true;
		SDL_UnlockMutex(g_mu);
	}

	// then from the start, in order
	static uint8_t* buf = nullptr;
	const size_t BUF = 256 * 1024;
	if (!buf) buf = (uint8_t*)malloc(BUF);
	uint64_t off = 0;
	bool ok = (buf != nullptr), aborted = false, headFileWritten = false;
	if (!ok) err = "Out of memory";
	while (ok && off < total) {
		SDL_LockMutex(g_mu);
		const bool stop = g_quit || sp->cancelled || g_wanted != sp->remote;
		const bool listWaiting = g_listPending;
		SDL_UnlockMutex(g_mu);
		if (stop) { aborted = true; trace("stream: stopped at %llu of %llu bytes", (unsigned long long)off, (unsigned long long)total); break; }
		if (listWaiting) serveList();
		const uint32_t want = (uint32_t)std::min<uint64_t>(BUF, total - off);
		const int n = smb2_pread(g_ctx, fh, buf, want, off);
		if (n < 0) { err = std::string("Download failed: ") + smb2_get_error(g_ctx); ok = false; break; }
		if (n == 0) { err = "The song ended early"; ok = false; break; }
		memcpy(mem + off, buf, (size_t)n);
		off += (uint64_t)n;
		SDL_LockMutex(g_mu);
		sp->written = off;
		const bool justReady = !sp->headReady && (off >= STREAM_HEAD || off >= total);
		if (justReady) sp->headReady = true;
		SDL_CondBroadcast(g_streamCv);
		SDL_UnlockMutex(g_mu);
		if (justReady) trace("stream: ready to play (%llu of %llu bytes)", (unsigned long long)off, (unsigned long long)total);
		if (justReady && !headFileWritten) {
			// the first part goes to the SD card now, so tags and cover art can be read while the song plays
			headFileWritten = true;
			FILE* m = fopen(sp->marker.c_str(), "wb");
			if (m) fclose(m);
			FILE* f = fopen(sp->local.c_str(), "wb");
			if (f) { fwrite(mem, 1, (size_t)off, f); fclose(f); }
		}
	}
	smb2_close(g_ctx, fh);

	if (ok && !aborted) {
		trace("stream: all %llu bytes arrived", (unsigned long long)total);
		// everything is here: write the complete file so the song can be replayed from the cache
		bool saved = false;
		for (int attempt = 0; attempt < 5 && !saved; attempt++) {
			FILE* f = fopen(sp->local.c_str(), "wb");
			if (f) {
				saved = fwrite(mem, 1, (size_t)total, f) == (size_t)total;
				fclose(f);
			}
			if (!saved) SDL_Delay(30);
		}
		if (saved) remove(sp->marker.c_str());
		else { remove(sp->local.c_str()); remove(sp->marker.c_str()); } // not cached, but it plays fine from memory
		SDL_LockMutex(g_mu);
		sp->complete = true;
		SDL_CondBroadcast(g_streamCv);
		SDL_UnlockMutex(g_mu);
		trimCache();
		return;
	}
	if (!aborted) { markBroken(); fail(err); }
	SDL_LockMutex(g_mu);
	sp->cancelled = true; // readers stop waiting
	SDL_CondBroadcast(g_streamCv);
	SDL_UnlockMutex(g_mu);
	remove(sp->local.c_str());
	remove(sp->marker.c_str());
}

int workerMain(void*) {
	g_socketsStarted = R_SUCCEEDED(socketInitializeDefault());
	trimCache();
	while (true) {
		SDL_LockMutex(g_mu);
		if (!g_quit && !g_listPending && g_streamPending.empty() && g_urgent.empty() && g_background.empty()) trace("worker: idle");
		while (!g_quit && !g_listPending && g_streamPending.empty() && g_urgent.empty() && g_background.empty()) SDL_CondWait(g_cv, g_mu);
		if (g_quit) { SDL_UnlockMutex(g_mu); break; }
		if (g_listPending) { SDL_UnlockMutex(g_mu); serveList(); continue; }
		if (!g_streamPending.empty()) {
			std::shared_ptr<Stream> sp = g_stream;
			const std::string path = g_streamPending;
			g_streamPending.clear();
			if (sp && sp->remote == path) {
				g_current = path;
				SDL_UnlockMutex(g_mu);
				trace("stream: start %s", path.c_str());
				doStream(sp);
				trace("stream: finished %s", path.c_str());
				SDL_LockMutex(g_mu);
				g_current.clear();
			}
			SDL_UnlockMutex(g_mu);
			continue;
		}
		std::string path;
		bool background = false;
		if (!g_urgent.empty()) { path = g_urgent.front(); g_urgent.pop_front(); }
		else { path = g_background.front(); g_background.pop_front(); background = true; }
		g_current = path;
		SDL_UnlockMutex(g_mu);
		trace("%s download: start %s", background ? "background" : "urgent", path.c_str());
		doFetch(path, background);
		SDL_LockMutex(g_mu);
		g_current.clear();
		SDL_UnlockMutex(g_mu);
	}
	trace("worker: leaving");
	// leaving: no network goodbyes (the connection may already be gone), and the socket service is left alone
	disconnect(false);
	g_workerExited = 1;
	return 0;
}

void ensureThread() { // call with g_mu NOT held
	initSync();
	SDL_LockMutex(g_mu);
	const bool start = !g_thread;
	SDL_UnlockMutex(g_mu);
	if (start) g_thread = SDL_CreateThread(workerMain, "smb-worker", nullptr);
}

} // namespace

// ================================================================== public API
std::string settingsPath() { return INI_PATH; }

bool loadSettings() {
	initSync();
	Settings s;
	if (!readSettings(s)) {
		writeTemplate();
		readSettings(s);
	}
	SDL_LockMutex(g_mu);
	g_settings = s;
	g_needReconnect = true;
	SDL_UnlockMutex(g_mu);
	return !s.server.empty() && !s.share.empty();
}

bool configured() {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool ok = !g_settings.server.empty() && !g_settings.share.empty();
	SDL_UnlockMutex(g_mu);
	return ok;
}

// read smb.ini once without creating it, so the menu can show the server before Network was ever opened
static void peekSettings() {
	static bool peeked = false;
	if (peeked) return;
	peeked = true;
	initSync();
	Settings s;
	if (readSettings(s)) {
		SDL_LockMutex(g_mu);
		g_settings = s;
		SDL_UnlockMutex(g_mu);
	}
}

Config getConfig() {
	peekSettings();
	SDL_LockMutex(g_mu);
	const Config c = g_settings;
	SDL_UnlockMutex(g_mu);
	return c;
}

bool saveConfig(const Config& in) {
	initSync();
	Config c = in;
	while (!c.folder.empty() && (c.folder[0] == '/' || c.folder[0] == '\\')) c.folder.erase(0, 1);
	while (!c.folder.empty() && (c.folder.back() == '/' || c.folder.back() == '\\')) c.folder.pop_back();
	const bool ok = writeIni(c);
	SDL_LockMutex(g_mu);
	g_settings = c;
	g_needReconnect = true; // the next connection uses the new details
	SDL_UnlockMutex(g_mu);
	trace("settings saved (server %s, share %s, user %s)", c.server.c_str(), c.share.c_str(), c.user.empty() ? "guest" : c.user.c_str());
	return ok;
}

std::string label() {
	peekSettings();
	SDL_LockMutex(g_mu);
	const std::string l = (g_settings.server.empty() || g_settings.share.empty()) ? std::string("not set up yet")
		: g_settings.server + "/" + g_settings.share;
	SDL_UnlockMutex(g_mu);
	return l;
}

std::string startFolder() {
	if (!g_mu) return "";
	SDL_LockMutex(g_mu);
	const std::string f = g_settings.folder;
	SDL_UnlockMutex(g_mu);
	return f;
}

void reload() { loadSettings(); }

bool isRemote(const std::string& path) { return path.compare(0, 6, "smb://") == 0; }

void requestList(const std::string& dir) {
	ensureThread();
	SDL_LockMutex(g_mu);
	g_listPending = true;
	g_listDir = dir;
	SDL_CondSignal(g_cv);
	SDL_UnlockMutex(g_mu);
}

bool pollList(std::string& dir, std::vector<Entry>& entries, std::string& error, bool& ok) {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool finished = g_listFinished;
	if (finished) {
		dir = g_listResultDir;
		entries.swap(g_listEntries);
		error = g_listError;
		ok = g_listOk;
		g_listFinished = false;
	}
	SDL_UnlockMutex(g_mu);
	return finished;
}

bool cachedFile(const std::string& remotePath, std::string& localPath) {
	const std::string p = cachePath(remotePath);
	if (!fileExists(p) || fileExists(p + ".incomplete")) return false;
	utime(p.c_str(), nullptr); // used just now: keep it in the cache
	localPath = p;
	return true;
}

void requestFetch(const std::string& remotePath) {
	ensureThread();
	SDL_LockMutex(g_mu);
	g_wanted = remotePath;
	// songs the user asked for earlier but that have not started yet are no longer interesting
	for (const std::string& old : g_urgent) if (old != remotePath) g_fetch[old].state = FETCH_NONE;
	g_urgent.erase(std::remove_if(g_urgent.begin(), g_urgent.end(), [&](const std::string& s) { return s != remotePath; }), g_urgent.end());
	g_background.erase(std::remove(g_background.begin(), g_background.end(), remotePath), g_background.end());
	FetchInfo& info = g_fetch[remotePath];
	if (g_current != remotePath) { // not already being downloaded: queue it
		info.state = FETCH_BUSY;
		info.error.clear();
		info.done = 0;
		g_urgent.push_front(remotePath);
	}
	SDL_CondSignal(g_cv);
	SDL_UnlockMutex(g_mu);
}

void prefetch(const std::string& remotePath) {
	std::string local;
	if (cachedFile(remotePath, local)) { trace("prefetch skipped (already in the cache): %s", remotePath.c_str()); return; }
	ensureThread();
	SDL_LockMutex(g_mu);
	FetchInfo& info = g_fetch[remotePath];
	if (info.state == FETCH_BUSY) trace("prefetch skipped (marked busy already): %s", remotePath.c_str());
	if (info.state != FETCH_BUSY) {
		info.state = FETCH_BUSY;
		info.error.clear();
		info.done = 0;
		g_background.push_back(remotePath);
		SDL_CondSignal(g_cv);
		trace("prefetch queued: %s", remotePath.c_str());
	}
	SDL_UnlockMutex(g_mu);
}

FetchState fetchState(const std::string& remotePath, std::string& localPath, std::string& error) {
	if (!g_mu) return FETCH_NONE;
	SDL_LockMutex(g_mu);
	FetchState st = FETCH_NONE;
	auto it = g_fetch.find(remotePath);
	if (it != g_fetch.end()) {
		st = it->second.state;
		localPath = it->second.local;
		error = it->second.error;
	}
	SDL_UnlockMutex(g_mu);
	return st;
}

float fetchProgress(const std::string& remotePath) {
	if (!g_mu) return -1.0f;
	SDL_LockMutex(g_mu);
	float p = -1.0f;
	auto it = g_fetch.find(remotePath);
	if (it != g_fetch.end() && it->second.total > 0) p = (float)((double)it->second.done / (double)it->second.total);
	SDL_UnlockMutex(g_mu);
	return p;
}

bool takeError(std::string& message) {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool fresh = g_messageFresh;
	if (fresh) { message = g_message; g_messageFresh = false; }
	SDL_UnlockMutex(g_mu);
	return fresh;
}

// ------------------------------------------------------------------ streaming API
// The decoder (SDL_mixer) reads the song through an SDL_RWops. A read waits until the bytes it wants have arrived.
static Sint64 SDLCALL rwSize(SDL_RWops* rw) {
	auto* sp = (std::shared_ptr<Stream>*)rw->hidden.unknown.data1;
	return (Sint64)(*sp)->total;
}

static Sint64 SDLCALL rwSeek(SDL_RWops* rw, Sint64 offset, int whence) {
	Stream* s = ((std::shared_ptr<Stream>*)rw->hidden.unknown.data1)->get();
	Sint64 p = 0;
	if (whence == RW_SEEK_SET) p = offset;
	else if (whence == RW_SEEK_CUR) p = (Sint64)s->pos + offset;
	else p = (Sint64)s->total + offset;
	if (p < 0) p = 0;
	if (p > (Sint64)s->total) p = (Sint64)s->total;
	s->pos = (uint64_t)p;
	return p;
}

static size_t SDLCALL rwRead(SDL_RWops* rw, void* ptr, size_t size, size_t maxnum) {
	Stream* s = ((std::shared_ptr<Stream>*)rw->hidden.unknown.data1)->get();
	if (size == 0 || maxnum == 0 || s->pos >= s->total) return 0;
	uint64_t want = (uint64_t)size * (uint64_t)maxnum;
	want = std::min<uint64_t>(want, s->total - s->pos);
	want = (want / size) * size; // whole items only
	if (want == 0) return 0;

	uint8_t* out = (uint8_t*)ptr;
	uint64_t done = 0;
	while (done < want) {
		const uint64_t p = s->pos + done;
		uint64_t avail = 0;
		SDL_LockMutex(g_mu);
		while (true) {
			if (s->failed || s->cancelled || g_quit) break;
			if (p < s->written) { avail = std::min<uint64_t>(want - done, s->written - p); break; }
			if (s->tailReady && p >= s->tailStart) { avail = want - done; break; }
			if (s->complete) break;
			SDL_CondWaitTimeout(g_streamCv, g_mu, 100); // the data has not arrived yet
		}
		SDL_UnlockMutex(g_mu);
		if (avail == 0) break;
		memcpy(out + done, s->data + p, (size_t)avail);
		done += avail;
	}
	s->pos += done;
	return (size_t)(done / size);
}

static size_t SDLCALL rwWrite(SDL_RWops*, const void*, size_t, size_t) { return 0; }

static int SDLCALL rwClose(SDL_RWops* rw) {
	auto* sp = (std::shared_ptr<Stream>*)rw->hidden.unknown.data1;
	SDL_LockMutex(g_mu);
	if (!(*sp)->complete) (*sp)->cancelled = true; // closed before everything arrived: stop downloading it
	SDL_CondBroadcast(g_streamCv);
	SDL_UnlockMutex(g_mu);
	delete sp;
	SDL_FreeRW(rw);
	return 0;
}

bool isFetching(const std::string& remotePath) {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool busy = (g_current == remotePath) || (g_streamPending == remotePath); // only what is actually in progress
	SDL_UnlockMutex(g_mu);
	return busy;
}

void requestStream(const std::string& remotePath) {
	ensureThread();
	SDL_LockMutex(g_mu);
	if (g_stream && !g_stream->complete) { g_stream->cancelled = true; SDL_CondBroadcast(g_streamCv); } // the previous one is not wanted any more
	g_stream = std::make_shared<Stream>();
	g_stream->remote = remotePath;
	g_stream->local = cachePath(remotePath);
	g_stream->marker = g_stream->local + ".incomplete";
	g_streamPending = remotePath;
	g_wanted = remotePath;
	for (const std::string& old : g_urgent) g_fetch[old].state = FETCH_NONE;
	g_urgent.clear();
	for (const std::string& old : g_background) g_fetch[old].state = FETCH_NONE; // earlier 'next song' guesses are stale now
	g_background.clear();
	trace("stream requested: %s", remotePath.c_str());
	SDL_CondSignal(g_cv);
	SDL_UnlockMutex(g_mu);
}

StreamState streamState(const std::string& remotePath, std::string& localPath, std::string& error) {
	if (!g_mu) return STREAM_NONE;
	SDL_LockMutex(g_mu);
	StreamState st = STREAM_NONE;
	if (g_stream && g_stream->remote == remotePath) {
		const Stream& s = *g_stream;
		if (s.unsupported) st = STREAM_UNSUPPORTED;
		else if (s.failed) { st = STREAM_FAILED; error = s.error; }
		else if (s.cancelled && !s.complete) st = STREAM_FAILED;
		else if (s.headReady && (s.tailReady || s.total <= STREAM_HEAD + STREAM_TAIL)) { st = STREAM_READY; localPath = s.local; }
		else st = STREAM_PREPARING;
	}
	SDL_UnlockMutex(g_mu);
	return st;
}

SDL_RWops* openStreamRW(const std::string& remotePath) {
	if (!g_mu) return nullptr;
	SDL_LockMutex(g_mu);
	std::shared_ptr<Stream> sp = (g_stream && g_stream->remote == remotePath) ? g_stream : nullptr;
	SDL_UnlockMutex(g_mu);
	if (!sp) return nullptr;
	SDL_RWops* rw = SDL_AllocRW();
	if (!rw) return nullptr;
	rw->size = rwSize;
	rw->seek = rwSeek;
	rw->read = rwRead;
	rw->write = rwWrite;
	rw->close = rwClose;
	rw->type = SDL_RWOPS_UNKNOWN;
	rw->hidden.unknown.data1 = new std::shared_ptr<Stream>(sp);
	return rw;
}

bool streamComplete(const std::string& remotePath) {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool done = g_stream && g_stream->remote == remotePath && g_stream->complete;
	SDL_UnlockMutex(g_mu);
	return done;
}

uint64_t streamTotal(const std::string& remotePath) {
	if (!g_mu) return 0;
	SDL_LockMutex(g_mu);
	const uint64_t t = (g_stream && g_stream->remote == remotePath) ? g_stream->total : 0;
	SDL_UnlockMutex(g_mu);
	return t;
}

float streamProgress(const std::string& remotePath) {
	if (!g_mu) return -1.0f;
	SDL_LockMutex(g_mu);
	float p = -1.0f;
	if (g_stream && g_stream->remote == remotePath && g_stream->total > 0) p = (float)((double)g_stream->written / (double)g_stream->total);
	SDL_UnlockMutex(g_mu);
	return p;
}

std::vector<uint8_t> streamHead(const std::string& remotePath, size_t maxBytes) {
	std::vector<uint8_t> out;
	if (!g_mu) return out;
	SDL_LockMutex(g_mu);
	if (g_stream && g_stream->remote == remotePath && g_stream->data) {
		const size_t n = (size_t)std::min<uint64_t>(std::min<uint64_t>(g_stream->written, g_stream->total), (uint64_t)maxBytes);
		out.assign(g_stream->data, g_stream->data + n);
	}
	SDL_UnlockMutex(g_mu);
	return out;
}

void abortStreams() {
	if (!g_mu) return;
	SDL_LockMutex(g_mu);
	if (g_stream && !g_stream->complete) g_stream->cancelled = true;
	SDL_CondBroadcast(g_streamCv);
	SDL_UnlockMutex(g_mu);
}

void trace(const char* fmt, ...) {
	initSync();
	SDL_LockMutex(g_logMu);
	if (!g_log) {
		mkdir("sdmc:/switch", 0777);
		mkdir("sdmc:/switch/shaderfun", 0777);
		g_log = fopen("sdmc:/switch/shaderfun/smb.log", "w");
	}
	if (g_log) {
		fprintf(g_log, "%8u ms  ", (unsigned)SDL_GetTicks());
		va_list ap;
		va_start(ap, fmt);
		vfprintf(g_log, fmt, ap);
		va_end(ap);
		fputc('\n', g_log);
		fflush(g_log);
	}
	SDL_UnlockMutex(g_logMu);
}

void keepOnly(const std::vector<std::string>& keepRemotePaths) {
	std::vector<std::string> keepStems; // "<hash>_<name>" without the extension: the song and its .lrc share it
	for (const std::string& r : keepRemotePaths) keepStems.push_back(withoutExt(baseName(cachePath(r))));
	DIR* d = opendir(CACHE_DIR);
	if (!d) return;
	std::vector<std::string> doomed;
	while (dirent* e = readdir(d)) {
		const std::string name = e->d_name;
		if (name.empty() || name[0] == '.') continue;
		if (name.size() > 5 && name.compare(name.size() - 5, 5, ".part") == 0) continue; // being downloaded right now
		if (name.size() > 11 && name.compare(name.size() - 11, 11, ".incomplete") == 0) continue;
		if (std::find(keepStems.begin(), keepStems.end(), withoutExt(name)) == keepStems.end()) doomed.push_back(name);
	}
	closedir(d);
	for (const std::string& n : doomed) {
		trace("cache: deleting %s", n.c_str());
		remove((std::string(CACHE_DIR) + "/" + n).c_str());
	}
}

static bool g_keepCache = false;
void setKeepCache(bool keep) { g_keepCache = keep; }
bool keepCache() { return g_keepCache; }

// half-finished downloads (.part files, and songs whose ".incomplete" marker is still there) are never worth keeping
void cleanIncomplete() {
	DIR* d = opendir(CACHE_DIR);
	if (!d) return;
	std::vector<std::string> names;
	while (dirent* e = readdir(d)) {
		if (e->d_name[0] != '.') names.push_back(e->d_name);
	}
	closedir(d);
	auto endsWith = [](const std::string& s, const char* suffix) {
		const size_t n = strlen(suffix);
		return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
	};
	for (const std::string& n : names) {
		if (endsWith(n, ".part")) remove((std::string(CACHE_DIR) + "/" + n).c_str());
		else if (endsWith(n, ".incomplete")) {
			remove((std::string(CACHE_DIR) + "/" + n).c_str());                          // the marker
			remove((std::string(CACHE_DIR) + "/" + n.substr(0, n.size() - 11)).c_str()); // and the unfinished song it belongs to
		}
	}
}

void clearCache() {
	DIR* d = opendir(CACHE_DIR);
	if (!d) return;
	std::vector<std::string> all;
	while (dirent* e = readdir(d)) {
		if (e->d_name[0] != '.') all.push_back(e->d_name);
	}
	closedir(d);
	for (const std::string& n : all) remove((std::string(CACHE_DIR) + "/" + n).c_str());
}

void shutdown() {
	trace("shutdown: begin");
	if (g_mu) {
		SDL_LockMutex(g_mu);
		g_quit = true;
		if (g_stream && !g_stream->complete) g_stream->cancelled = true;
		SDL_CondSignal(g_cv);
		SDL_CondBroadcast(g_streamCv);
		SDL_UnlockMutex(g_mu);
		if (g_thread) {
			// wait for the worker, but never for long: it may be stuck inside a network call
			for (int i = 0; i < 150 && !g_workerExited; i++) SDL_Delay(10);
			if (g_workerExited) SDL_WaitThread(g_thread, nullptr);
			else SDL_DetachThread(g_thread);
			g_thread = nullptr;
		}
	}
	if (g_keepCache) cleanIncomplete(); // the finished songs stay, so the last song is ready next time
	else clearCache();                  // the downloads were only there to be played
	trace("shutdown: done (cache %s)", g_keepCache ? "kept" : "cleared");
}

} // namespace smbnet
