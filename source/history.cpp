#include "history.h"
#include "db.h"
#include "library.h"
#include "player.h"
#include "trackindex.h"
#include "usbdrive.h"

#include <SDL2/SDL.h>
#include <sqlite3.h>
#include <time.h>
#include <atomic>

namespace history {
namespace {

// Early versions logged a network song by its copy in the cache folder instead of its smb:// path: those rows cannot be played
// again, so the lists leave them out (they are not deleted).
#define VALID_ROWS "WHERE path NOT LIKE 'sdmc:/switch/shaderfun/cache/%'"

SDL_mutex* g_mu = nullptr;
std::vector<Entry> g_recent, g_most;
int g_total = 0;
std::atomic<uint32_t> g_version(0);

// main thread
uint32_t g_forCounter = 0xFFFFFFFFu;
double g_listened = 0.0;
bool g_logged = true;
std::string g_path;

std::string lowerAscii(const std::string& s) {
	std::string r = s;
	for (char& c : r) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	return r;
}

std::string col(sqlite3_stmt* st, int i) {
	const unsigned char* t = sqlite3_column_text(st, i);
	return t ? std::string((const char*)t) : std::string();
}

std::vector<Entry> readList(sqlite3* h, const char* sql) {
	std::vector<Entry> out;
	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(h, sql, -1, &st, nullptr) != SQLITE_OK) return out;
	while (sqlite3_step(st) == SQLITE_ROW) {
		Entry e;
		e.path = col(st, 0);
		e.title = col(st, 1);
		e.artist = col(st, 2);
		e.album = col(st, 3);
		e.lastPlayed = (int64_t)sqlite3_column_int64(st, 4);
		e.plays = sqlite3_column_int(st, 5);
		out.push_back(std::move(e));
	}
	sqlite3_finalize(st);
	return out;
}

// runs on the database thread: rebuilds the two lists in memory
void recompute(sqlite3* h) {
	// every song once, with its newest play; plays = how many times in all
	std::vector<Entry> recent = readList(h,
		"SELECT p.path,p.title,p.artist,p.album,p.played_at,c.n FROM plays p "
		"JOIN (SELECT song_key, MAX(id) AS mid, COUNT(*) AS n FROM plays " VALID_ROWS " GROUP BY song_key) c ON p.id=c.mid "
		"ORDER BY p.played_at DESC, p.id DESC LIMIT 300;");
	std::vector<Entry> most = readList(h,
		"SELECT p.path,p.title,p.artist,p.album,p.played_at,c.n FROM plays p "
		"JOIN (SELECT song_key, MAX(id) AS mid, COUNT(*) AS n FROM plays " VALID_ROWS " GROUP BY song_key) c ON p.id=c.mid "
		"ORDER BY c.n DESC, p.played_at DESC LIMIT 300;");
	int total = 0;
	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM plays " VALID_ROWS ";", -1, &st, nullptr) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) total = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	SDL_LockMutex(g_mu);
	g_recent.swap(recent);
	g_most.swap(most);
	g_total = total;
	SDL_UnlockMutex(g_mu);
	g_version++;
}

void logPlay(const std::string& realPath, double seconds) {
	std::string path = realPath, source = "sd";
	if (usbdrive::isUsbPath(realPath)) { const std::string st = usbdrive::stablePath(realPath); if (!st.empty()) path = st; source = "usb"; }
	else if (realPath.compare(0, 6, "smb://") == 0) source = "smb";

	std::string title, artist, album;
	const trackindex::Hit hit = trackindex::find(realPath);
	if (hit.snap) {
		const library::Track& t = hit.snap->tracks[hit.track];
		title = library::displayTitle(t);
		artist = t.artist.empty() ? t.albumArtist : t.artist;
		album = library::albumDisplayName(t);
	}
	else title = player::baseName(realPath);
	const std::string key = lowerAscii(artist) + "|" + lowerAscii(title); // the same song from two places counts together
	const int64_t now = (int64_t)time(NULL);
	const int secs = (int)seconds;

	db::postHistory([=](sqlite3* h) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(h, "INSERT INTO plays(played_at,path,song_key,title,artist,album,seconds,source) VALUES(?1,?2,?3,?4,?5,?6,?7,?8);", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, (sqlite3_int64)now);
			sqlite3_bind_text(st, 2, path.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 3, key.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 4, title.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 5, artist.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 6, album.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(st, 7, secs);
			sqlite3_bind_text(st, 8, source.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		recompute(h);
	});
}

} // namespace

void init() {
	if (!g_mu) g_mu = SDL_CreateMutex();
	db::postHistory([](sqlite3* h) { recompute(h); });
}

bool enabled() { return player::configInt("log_plays", 1) != 0; }

void update(float dt) {
	if (!g_mu || !db::historyReady() || !player::hasTracks()) return;
	const uint32_t c = player::trackChangeCounter();
	if (c != g_forCounter) { // another song (or the same one again) started
		g_forCounter = c;
		g_listened = 0.0;
		g_logged = false;
		g_path = player::currentSourcePath(); // (a network song: its smb:// path, not the copy in the cache folder)
	}
	if (g_logged || !enabled()) return;
	if (player::isPlaying() && !player::isPaused()) g_listened += (dt > 0.5f ? 0.5f : dt);
	const double dur = player::duration();
	const double need = dur > 0.0 ? (dur * 0.5 < 240.0 ? dur * 0.5 : 240.0) : 60.0; // half of the song, at most 4 minutes
	if (g_listened >= need && !g_path.empty()) {
		g_logged = true;
		logPlay(g_path, g_listened);
	}
}

std::vector<Entry> recent() {
	if (!g_mu) return {};
	SDL_LockMutex(g_mu);
	std::vector<Entry> r = g_recent;
	SDL_UnlockMutex(g_mu);
	return r;
}

std::vector<Entry> mostPlayed() {
	if (!g_mu) return {};
	SDL_LockMutex(g_mu);
	std::vector<Entry> r = g_most;
	SDL_UnlockMutex(g_mu);
	return r;
}

int totalPlays() { return g_total; }

uint32_t version() { return g_version; }

void clearAll() {
	if (!g_mu) return;
	db::postHistory([](sqlite3* h) {
		db::exec(h, "DELETE FROM plays;");
		recompute(h);
	});
	SDL_LockMutex(g_mu);
	g_recent.clear();
	g_most.clear();
	g_total = 0;
	SDL_UnlockMutex(g_mu);
	g_version++;
}

} // namespace history
