#include "smblib.h"
#include "db.h"

#include <SDL2/SDL.h>
#include <sqlite3.h>
#include <atomic>
#include <vector>

namespace smblib {
namespace {

SDL_mutex* g_mu = nullptr;
std::shared_ptr<const library::Snapshot> g_snap = std::make_shared<library::Snapshot>();
std::atomic<uint32_t> g_version(0);
std::atomic<int> g_pending(0);   // loads queued or running

std::string col(sqlite3_stmt* st, int i) {
	const unsigned char* t = sqlite3_column_text(st, i);
	return t ? std::string((const char*)t) : std::string();
}

// runs on the database thread
void loadJob(sqlite3* d) {
	std::vector<library::Track> tracks;
	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(d, "SELECT path,title,artist,album,albumartist,genre,year,trackno,discno,size,mtime FROM tracks WHERE source='smb';", -1, &st, nullptr) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW) {
			library::Track t;
			t.path = col(st, 0);
			t.title = col(st, 1);
			t.artist = col(st, 2);
			t.album = col(st, 3);
			t.albumArtist = col(st, 4);
			t.genre = col(st, 5);
			t.year = col(st, 6);
			t.trackNo = sqlite3_column_int(st, 7);
			t.discNo = sqlite3_column_int(st, 8);
			t.size = (uint64_t)sqlite3_column_int64(st, 9);
			t.mtime = (int64_t)sqlite3_column_int64(st, 10);
			tracks.push_back(std::move(t));
		}
		sqlite3_finalize(st);
	}
	std::shared_ptr<const library::Snapshot> snap = library::makeSnapshot(std::move(tracks)); // sorts and groups
	SDL_LockMutex(g_mu);
	g_snap = snap;
	SDL_UnlockMutex(g_mu);
	g_version++;
	g_pending--;
}

} // namespace

void init() {
	if (!g_mu) g_mu = SDL_CreateMutex();
	reload();
}

void reload() {
	if (!g_mu) g_mu = SDL_CreateMutex();
	g_pending++;
	if (!db::post(loadJob)) g_pending--; // no database: stays empty
}

std::shared_ptr<const library::Snapshot> snapshot() {
	if (!g_mu) return std::make_shared<library::Snapshot>();
	SDL_LockMutex(g_mu);
	std::shared_ptr<const library::Snapshot> s = g_snap;
	SDL_UnlockMutex(g_mu);
	return s;
}

bool loading() { return g_pending > 0; }

uint32_t version() { return g_version; }

} // namespace smblib
