#include "lrclib.h"
#include "smbnet.h" // trace(): the log
#include "tags.h"

#include <SDL2/SDL.h>
#include <switch.h>
#include <curl/curl.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <vector>

namespace lrclib {
namespace {

const char* const LYRICS_DIR = "sdmc:/switch/shaderfun/lyrics";
const char* const USER_AGENT = "ShaderFun-Switch/1.0";
const size_t MAX_BODY = 2 * 1024 * 1024;
const Uint32 OFFLINE_RETRY_MS = 5 * 60 * 1000;

// ---- shared with the UI thread (g_mu)
SDL_mutex* g_mu = nullptr;
SDL_cond* g_cv = nullptr;
SDL_Thread* g_thread = nullptr;
bool g_quit = false;
bool g_hasJob = false;
std::string g_jobPath;
double g_jobDuration = 0.0;
bool g_resReady = false;
Result g_res;
volatile int g_exited = 0;
Uint32 g_offlineUntil = 0; // worker thread only

// ------------------------------------------------------------------ helpers
bool fileExists(const std::string& p) {
	struct stat st;
	return stat(p.c_str(), &st) == 0;
}

std::string readSmallFile(const std::string& path) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return std::string();
	std::string out;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < MAX_BODY) out.append(buf, n);
	fclose(f);
	return out;
}

void ensureDirs() {
	mkdir("sdmc:/switch", 0777);
	mkdir("sdmc:/switch/shaderfun", 0777);
	mkdir(LYRICS_DIR, 0777);
}

// letters and digits only, lower case (non-ASCII bytes are kept): "AC/DC - Thunderstruck" ~ "acdcthunderstruck"
std::string squash(const std::string& s) {
	std::string out;
	for (unsigned char c : s) {
		if (c >= 0x80) out += (char)c;
		else if (isalnum(c)) out += (char)tolower(c);
	}
	return out;
}

std::string cacheBase(const std::string& artist, const std::string& title) {
	uint32_t h = 2166136261u;
	const std::string key = squash(artist) + "|" + squash(title);
	for (unsigned char c : key) { h ^= c; h *= 16777619u; }
	char hex[16];
	snprintf(hex, sizeof(hex), "%08x", h);
	return std::string(LYRICS_DIR) + "/" + hex;
}

// the artist / title / album of a song: from its tags, the title falls back to the file name ("01 - Name.mp3" -> "Name")
bool songInfo(const std::string& path, std::string& artist, std::string& title, std::string& album) {
	tags::TrackTags t;
	tags::read(path, t, 0.0, true, false);
	artist = t.artist.empty() ? t.albumArtist : t.artist;
	title = t.title;
	album = t.album;
	if (title.empty()) {
		const size_t slash = path.find_last_of("/\\");
		std::string n = slash == std::string::npos ? path : path.substr(slash + 1);
		const size_t dot = n.find_last_of('.');
		if (dot != std::string::npos && dot > 0) n = n.substr(0, dot);
		size_t i = 0;
		while (i < n.size() && (isdigit((unsigned char)n[i]) || n[i] == ' ' || n[i] == '-' || n[i] == '.' || n[i] == '_')) i++;
		title = i < n.size() ? n.substr(i) : n;
	}
	return !artist.empty() && !title.empty();
}

bool looksTimed(const std::string& text) { // [mm:ss...] near the start
	for (size_t i = 0; i + 5 < text.size() && i < 1000; i++)
		if (text[i] == '[' && isdigit((unsigned char)text[i + 1]) && isdigit((unsigned char)text[i + 2]) && text[i + 3] == ':') return true;
	return false;
}

// "[ti:Title]" and "[ar:Artist]" lines, which the lyrics reader shows as the header above the lyrics
std::string headerTags(const std::string& title, const std::string& artist) {
	auto clean = [](std::string t) {
		for (char& c : t) if (c == ']' || c == '\n' || c == '\r') c = ' ';
		return t;
	};
	std::string out;
	if (!title.empty()) out += "[ti:" + clean(title) + "]\n";
	if (!artist.empty()) out += "[ar:" + clean(artist) + "]\n";
	return out;
}

bool hasInfoTags(const std::string& text) { return text.find("[ti:") != std::string::npos || text.find("[ar:") != std::string::npos; }

std::string urlEncode(const std::string& s) {
	static const char* const hex = "0123456789ABCDEF";
	std::string out;
	for (unsigned char c : s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
		else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
	}
	return out;
}

// ------------------------------------------------------------------ HTTP (libcurl)
size_t onData(char* ptr, size_t size, size_t nmemb, void* user) {
	std::string* body = (std::string*)user;
	const size_t n = size * nmemb;
	if (body->size() + n > MAX_BODY) return 0; // far bigger than any lyrics: give up
	body->append(ptr, n);
	return n;
}

// returns the curl result code (0 = a response arrived; 'status' is the HTTP status)
int httpGet(const std::string& url, std::string& body, long& status, std::string& err) {
	body.clear();
	status = 0;
	CURL* c = curl_easy_init();
	if (!c) { err = "curl could not start"; return -1; }
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, onData);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 3L);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 6L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
	// With a certificate bundle in the app (romfs:/cacert.pem) the server is checked; without it the connection is not verified.
	static const bool haveCa = fileExists("romfs:/cacert.pem");
	if (haveCa) {
		curl_easy_setopt(c, CURLOPT_CAINFO, "romfs:/cacert.pem");
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
	}
	else {
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
	}
	const CURLcode rc = curl_easy_perform(c);
	if (rc != CURLE_OK) {
		err = curl_easy_strerror(rc);
		curl_easy_cleanup(c);
		return (int)rc;
	}
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(c);
	return 0;
}

bool isOfflineError(int rc) {
	return rc == CURLE_COULDNT_RESOLVE_HOST || rc == CURLE_COULDNT_CONNECT || rc == CURLE_OPERATION_TIMEDOUT ||
		rc == CURLE_SSL_CONNECT_ERROR || rc == CURLE_GOT_NOTHING || rc == CURLE_SEND_ERROR || rc == CURLE_RECV_ERROR;
}

// ------------------------------------------------------------------ a little JSON
void appendUtf8(std::string& s, uint32_t cp) {
	if (cp < 0x80) s += (char)cp;
	else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
	else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
	else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
}

int hexVal(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// the text of a JSON string starting at s[p] == '"'; p ends up after the closing quote
std::string readJsonString(const std::string& s, size_t& p) {
	std::string out;
	p++; // opening quote
	while (p < s.size() && s[p] != '"') {
		char c = s[p++];
		if (c != '\\' || p >= s.size()) { out += c; continue; }
		const char e = s[p++];
		switch (e) {
		case 'n': out += '\n'; break;
		case 'r': break;
		case 't': out += '\t'; break;
		case 'b': case 'f': break;
		case 'u': {
			uint32_t cp = 0;
			for (int k = 0; k < 4 && p < s.size(); k++) { const int h = hexVal(s[p++]); if (h >= 0) cp = cp * 16 + (uint32_t)h; }
			if (cp >= 0xD800 && cp < 0xDC00 && p + 5 < s.size() && s[p] == '\\' && s[p + 1] == 'u') { // surrogate pair
				uint32_t lo = 0;
				for (int k = 0; k < 4; k++) { const int h = hexVal(s[p + 2 + k]); if (h >= 0) lo = lo * 16 + (uint32_t)h; }
				if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
			}
			appendUtf8(out, cp);
			break;
		}
		default: out += e; break; // \" \\ \/
		}
	}
	if (p < s.size()) p++; // closing quote
	return out;
}

// the value of "key" inside one JSON object: strings are unescaped, numbers / true / false come back as text, null as ""
std::string jsonField(const std::string& obj, const char* key) {
	const std::string needle = std::string("\"") + key + "\"";
	size_t p = obj.find(needle);
	if (p == std::string::npos) return std::string();
	p += needle.size();
	while (p < obj.size() && (obj[p] == ' ' || obj[p] == ':' || obj[p] == '\n' || obj[p] == '\t')) p++;
	if (p >= obj.size()) return std::string();
	if (obj[p] == '"') return readJsonString(obj, p);
	size_t e = p;
	while (e < obj.size() && obj[e] != ',' && obj[e] != '}' && obj[e] != ' ' && obj[e] != '\n') e++;
	const std::string v = obj.substr(p, e - p);
	return v == "null" ? std::string() : v;
}

// the top-level {...} objects of a JSON array
std::vector<std::string> splitObjects(const std::string& json) {
	std::vector<std::string> out;
	int depth = 0;
	bool inStr = false, esc = false;
	size_t start = 0;
	for (size_t i = 0; i < json.size(); i++) {
		const char c = json[i];
		if (inStr) {
			if (esc) esc = false;
			else if (c == '\\') esc = true;
			else if (c == '"') inStr = false;
			continue;
		}
		if (c == '"') inStr = true;
		else if (c == '{') { if (depth++ == 0) start = i; }
		else if (c == '}') { if (--depth == 0) out.push_back(json.substr(start, i - start + 1)); }
	}
	return out;
}

struct Candidate {
	std::string track, artist, synced, plain;
	double duration = 0.0;
	bool instrumental = false;
};

Candidate toCandidate(const std::string& obj) {
	Candidate c;
	c.track = jsonField(obj, "trackName");
	c.artist = jsonField(obj, "artistName");
	c.synced = jsonField(obj, "syncedLyrics");
	c.plain = jsonField(obj, "plainLyrics");
	c.duration = atof(jsonField(obj, "duration").c_str());
	c.instrumental = jsonField(obj, "instrumental") == "true";
	return c;
}

bool similar(const std::string& a, const std::string& b) { // one name contains the other once punctuation is ignored
	const std::string x = squash(a), y = squash(b);
	if (x.empty() || y.empty()) return false;
	return x.find(y) != std::string::npos || y.find(x) != std::string::npos;
}

// ------------------------------------------------------------------ the lookup (worker thread)
void rememberMissing(const std::string& artist, const std::string& title) {
	ensureDirs();
	FILE* f = fopen((cacheBase(artist, title) + ".none").c_str(), "wb");
	if (f) fclose(f);
}

Result lookup(const std::string& path, double duration) {
	Result r;
	r.path = path;
	std::string artist, title, album;
	if (!songInfo(path, artist, title, album)) { r.error = "Online lyrics need the artist and title tags"; return r; }

	std::string cached;
	if (cachedForSong(path, cached)) { r.found = true; r.text = cached; r.timed = looksTimed(cached); return r; }
	if (fileExists(cacheBase(artist, title) + ".none")) { r.error = "No lyrics found online (looked before)"; return r; }
	if (SDL_GetTicks() < g_offlineUntil) { r.offline = true; r.error = "Offline - online lyrics will be tried again in a few minutes"; return r; }

	smbnet::trace("lrclib: looking for '%s' by '%s' (%.0f s)", title.c_str(), artist.c_str(), duration);

	std::string body, err;
	long status = 0;
	std::vector<Candidate> cands;
	bool serverAnswered = false;

	// 1. the exact lookup (needs album and length)
	if (!album.empty() && duration > 0.0) {
		const std::string url = "https://lrclib.net/api/get?artist_name=" + urlEncode(artist) + "&track_name=" + urlEncode(title) +
			"&album_name=" + urlEncode(album) + "&duration=" + std::to_string((int)(duration + 0.5));
		const int rc = httpGet(url, body, status, err);
		if (rc != 0) {
			smbnet::trace("lrclib: request failed: %s", err.c_str());
			if (isOfflineError(rc)) { g_offlineUntil = SDL_GetTicks() + OFFLINE_RETRY_MS; r.offline = true; r.error = "Could not reach lrclib.net (" + err + ")"; return r; }
			r.error = "Online lyrics failed: " + err;
			return r;
		}
		serverAnswered = true;
		if (status == 200) cands.push_back(toCandidate(body));
	}

	// 2. otherwise search by artist and title
	if (cands.empty()) {
		const std::string url = "https://lrclib.net/api/search?artist_name=" + urlEncode(artist) + "&track_name=" + urlEncode(title);
		const int rc = httpGet(url, body, status, err);
		if (rc != 0) {
			smbnet::trace("lrclib: search failed: %s", err.c_str());
			if (isOfflineError(rc)) { g_offlineUntil = SDL_GetTicks() + OFFLINE_RETRY_MS; r.offline = true; r.error = "Could not reach lrclib.net (" + err + ")"; return r; }
			r.error = "Online lyrics failed: " + err;
			return r;
		}
		serverAnswered = true;
		if (status == 200) for (const std::string& obj : splitObjects(body)) cands.push_back(toCandidate(obj));
		else if (status != 404) { r.error = "lrclib.net answered with an error (" + std::to_string(status) + ")"; return r; }
	}

	// choose the best: right song, right length, synced lyrics preferred
	int best = -1;
	double bestScore = 1e18;
	for (size_t i = 0; i < cands.size(); i++) {
		const Candidate& c = cands[i];
		if (c.instrumental || (c.synced.empty() && c.plain.empty())) continue;
		if (!similar(c.track, title) || !similar(c.artist, artist)) continue;
		const double diff = duration > 0.0 && c.duration > 0.0 ? fabs(c.duration - duration) : 0.0;
		if (duration > 0.0 && c.duration > 0.0 && diff > 4.0) continue; // another version (live, remix...): the timing would be wrong
		const double score = (c.synced.empty() ? 1000.0 : 0.0) + diff * 10.0 + (double)i * 0.01;
		if (score < bestScore) { bestScore = score; best = (int)i; }
	}
	if (best < 0) {
		smbnet::trace("lrclib: nothing suitable among %d result(s)", (int)cands.size());
		if (serverAnswered) rememberMissing(artist, title);
		r.error = "No lyrics found online";
		return r;
	}

	const Candidate& c = cands[(size_t)best];
	r.found = true;
	r.timed = !c.synced.empty();
	r.text = headerTags(c.track.empty() ? title : c.track, c.artist.empty() ? artist : c.artist) + (r.timed ? c.synced : c.plain);
	ensureDirs();
	FILE* f = fopen((cacheBase(artist, title) + ".lrc").c_str(), "wb");
	if (f) { fwrite(r.text.data(), 1, r.text.size(), f); fclose(f); }
	smbnet::trace("lrclib: found %s lyrics (%d bytes)", r.timed ? "synced" : "plain", (int)r.text.size());
	return r;
}

int workerMain(void*) {
	socketInitializeDefault(); // the network service (already running if the SMB code started it)
	curl_global_init(CURL_GLOBAL_DEFAULT);
	while (true) {
		SDL_LockMutex(g_mu);
		while (!g_quit && !g_hasJob) SDL_CondWait(g_cv, g_mu);
		if (g_quit) { SDL_UnlockMutex(g_mu); break; }
		const std::string path = g_jobPath;
		const double dur = g_jobDuration;
		g_hasJob = false;
		SDL_UnlockMutex(g_mu);

		Result r = lookup(path, dur);

		SDL_LockMutex(g_mu);
		if (!g_quit) { g_res = r; g_resReady = true; }
		SDL_UnlockMutex(g_mu);
	}
	curl_global_cleanup();
	g_exited = 1;
	return 0;
}

} // namespace

// ================================================================== public API
bool cachedForSong(const std::string& songPath, std::string& text) {
	std::string artist, title, album;
	if (!songInfo(songPath, artist, title, album)) return false;
	std::string t = readSmallFile(cacheBase(artist, title) + ".lrc");
	if (t.empty()) return false;
	if (!hasInfoTags(t)) t = headerTags(title, artist) + t; // lyrics saved without a header: add it from the song's tags
	text = t;
	return true;
}

std::string cacheFileForSong(const std::string& songPath) {
	std::string artist, title, album;
	if (!songInfo(songPath, artist, title, album)) return "";
	const std::string p = cacheBase(artist, title) + ".lrc";
	return fileExists(p) ? p : std::string();
}

void request(const std::string& songPath, double durationSeconds) {
	if (!g_mu) { g_mu = SDL_CreateMutex(); g_cv = SDL_CreateCond(); }
	SDL_LockMutex(g_mu);
	const bool start = !g_thread;
	SDL_UnlockMutex(g_mu);
	if (start) g_thread = SDL_CreateThread(workerMain, "lrclib-worker", nullptr);
	SDL_LockMutex(g_mu);
	g_jobPath = songPath;
	g_jobDuration = durationSeconds;
	g_hasJob = true;
	SDL_CondSignal(g_cv);
	SDL_UnlockMutex(g_mu);
}

bool poll(Result& out) {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool ready = g_resReady;
	if (ready) { out = g_res; g_resReady = false; }
	SDL_UnlockMutex(g_mu);
	return ready;
}

void shutdown() {
	if (!g_mu) return;
	SDL_LockMutex(g_mu);
	g_quit = true;
	SDL_CondSignal(g_cv);
	SDL_UnlockMutex(g_mu);
	if (g_thread) {
		for (int i = 0; i < 150 && !g_exited; i++) SDL_Delay(10); // never wait long: a request may be stuck on the network
		if (g_exited) SDL_WaitThread(g_thread, nullptr);
		else SDL_DetachThread(g_thread);
		g_thread = nullptr;
	}
}

} // namespace lrclib
