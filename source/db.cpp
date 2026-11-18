#include "db.h"

#include <SDL2/SDL.h>
#include <sqlite3.h>
#include <stdio.h>
#include <deque>
#include <memory>

namespace db {
namespace {

const int SCHEMA_VERSION = 2; // stored in the file (PRAGMA user_version); a different number means "start again"

SDL_Thread* g_thread = nullptr;
SDL_mutex* g_mu = nullptr;
SDL_cond* g_cv = nullptr;
struct Job { std::function<void(sqlite3*)> fn; bool history = false; };
std::deque<Job> g_jobs;
bool g_histReady = false;
bool g_quit = false;
bool g_opened = false;   // the thread has finished trying to open the file
bool g_ready = false;    // ... and it worked
std::string g_path;

// USB drives that have been scanned. The id is made from the label, maker, product, file system and size of the drive
// (the serial number is not used: not every drive can report it), and is part of every song path: usb://<id>/Music/a.mp3
bool createDrivesTable(sqlite3* d) {
	bool ok = exec(d,
		"CREATE TABLE IF NOT EXISTS drives("
		" id TEXT PRIMARY KEY,"
		" label TEXT,"                     // the name shown in the menu (the volume label)
		" maker TEXT, product TEXT, fs_type TEXT,"
		" capacity INTEGER,"
		" first_seen INTEGER, last_seen INTEGER,"
		" last_scan INTEGER,"              // when the last complete scan finished
		" songs INTEGER);");
	ok = ok && exec(d, "CREATE INDEX IF NOT EXISTS tracks_drive ON tracks(drive_id);");
	return ok;
}

bool createSchema(sqlite3* d) {
	bool ok = true;
	ok = ok && exec(d, "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);");
	ok = ok && exec(d,
		"CREATE TABLE IF NOT EXISTS tracks("
		" id INTEGER PRIMARY KEY,"
		" source TEXT NOT NULL,"           // 'smb' or 'usb' (later 'sd')
		" drive_id TEXT,"                  // usb: which drive (see the drives table), otherwise NULL
		" path TEXT NOT NULL,"             // the path as the app uses it, e.g. smb://Music/Artist/Album/01 Song.mp3
		" folder TEXT NOT NULL,"           // the path without the file name
		" filename TEXT NOT NULL,"
		" ext TEXT,"                       // lower case, with the dot
		" size INTEGER,"
		" mtime INTEGER,"                  // modified time (seconds since 1970)
		" tags_state INTEGER NOT NULL DEFAULT 0,"   // 0 = tags not read yet, 1 = read, 2 = could not be read
		" title TEXT, artist TEXT, album TEXT, albumartist TEXT, genre TEXT, year TEXT,"
		" trackno INTEGER, discno INTEGER,"
		" format TEXT, bitrate INTEGER, samplerate INTEGER, channels INTEGER,"
		" first_seen INTEGER,"             // time of the scan that found it first
		" last_seen INTEGER,"
		" scan_id INTEGER,"                // the scan that saw it last (a finished scan removes the rows it did not see)
		" UNIQUE(source, path));");
	ok = ok && exec(d, "CREATE INDEX IF NOT EXISTS tracks_folder ON tracks(source, folder);");
	ok = ok && exec(d, "CREATE INDEX IF NOT EXISTS tracks_tags ON tracks(source, tags_state);");
	ok = ok && createDrivesTable(d);
	ok = ok && exec(d, ("PRAGMA user_version=" + std::to_string(SCHEMA_VERSION) + ";").c_str());
	return ok;
}

int readUserVersion(sqlite3* d) {
	sqlite3_stmt* st = nullptr;
	int v = -1;
	if (sqlite3_prepare_v2(d, "PRAGMA user_version;", -1, &st, nullptr) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
	}
	if (st) sqlite3_finalize(st);
	return v;
}

const int HISTORY_VERSION = 1;

std::string historyPath() {
	const size_t slash = g_path.rfind('/');
	return (slash == std::string::npos ? std::string() : g_path.substr(0, slash + 1)) + "history.db";
}

bool createHistorySchema(sqlite3* d) {
	bool ok = exec(d,
		"CREATE TABLE IF NOT EXISTS plays("
		" id INTEGER PRIMARY KEY,"
		" played_at INTEGER NOT NULL,"    // seconds since 1970, when the play was logged
		" path TEXT NOT NULL,"            // how to play it (usb songs: usb://<drive id>/...)
		" song_key TEXT NOT NULL,"        // lower case artist|title: the same song from two places counts together
		" title TEXT, artist TEXT, album TEXT,"   // copied when the play was logged
		" seconds INTEGER,"               // how long it had been listened to
		" source TEXT);");                // sd / smb / usb
	ok = ok && exec(d, "CREATE INDEX IF NOT EXISTS plays_key ON plays(song_key);");
	ok = ok && exec(d, "CREATE INDEX IF NOT EXISTS plays_time ON plays(played_at);");
	ok = ok && exec(d, ("PRAGMA user_version=" + std::to_string(HISTORY_VERSION) + ";").c_str());
	return ok;
}

// The history is irreplaceable: it is never deleted. A file that cannot be read is renamed, one from a newer version is left alone.
sqlite3* openHistory() {
	const std::string path = historyPath();
	for (int attempt = 0; attempt < 2; attempt++) {
		sqlite3* hd = nullptr;
		if (sqlite3_open_v2(path.c_str(), &hd, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK) {
			exec(hd, "PRAGMA journal_mode=DELETE;");
			exec(hd, "PRAGMA synchronous=FULL;"); // a play is rare and precious: wait until it is really on the card
			const int ver = readUserVersion(hd);
			if (ver == HISTORY_VERSION) return hd;
			if (ver == 0 && createHistorySchema(hd)) return hd; // a new file
			if (ver > HISTORY_VERSION) {
				printf("db: %s was made by a newer version (%d): the history is not used, the file is left alone\n", path.c_str(), ver);
				sqlite3_close(hd);
				return nullptr;
			}
		}
		if (hd) sqlite3_close(hd);
		if (attempt == 0) { // unreadable: keep it for a look, start a new one
			const std::string bad = path + ".bad";
			remove(bad.c_str());
			rename(path.c_str(), bad.c_str());
			remove((path + "-journal").c_str());
			printf("db: %s could not be read, moved to %s\n", path.c_str(), bad.c_str());
		}
	}
	return nullptr;
}

// Opens the file, or starts a new one when it is damaged or was made by another version of the app.
sqlite3* openDatabase() {
	for (int attempt = 0; attempt < 2; attempt++) {
		sqlite3* d = nullptr;
		if (sqlite3_open_v2(g_path.c_str(), &d, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK) {
			exec(d, "PRAGMA journal_mode=DELETE;");
			exec(d, "PRAGMA synchronous=NORMAL;");
			exec(d, "PRAGMA temp_store=MEMORY;");
			exec(d, "PRAGMA cache_size=-4096;"); // 4 MB
			const int ver = readUserVersion(d);
			if (ver == SCHEMA_VERSION) return d;
			if (ver == 0 && createSchema(d)) return d; // a new file
			if (ver == 1) { // made by the previous version: add what is new and keep everything else (no rescan of the share)
				if (exec(d, "ALTER TABLE tracks ADD COLUMN drive_id TEXT;") && createDrivesTable(d) && exec(d, "PRAGMA user_version=2;")) return d;
			}
			printf("db: %s is not usable (version %d), starting a new one\n", g_path.c_str(), ver);
		}
		if (d) sqlite3_close(d);
		remove(g_path.c_str());
		remove((g_path + "-journal").c_str());
	}
	return nullptr;
}

int threadMain(void*) {
	sqlite3* d = openDatabase();
	sqlite3* hist = d ? openHistory() : nullptr;
	SDL_LockMutex(g_mu);
	g_ready = (d != nullptr);
	g_histReady = (hist != nullptr);
	g_opened = true;
	SDL_CondBroadcast(g_cv);
	SDL_UnlockMutex(g_mu);
	if (!d) return 0;

	SDL_LockMutex(g_mu);
	while (true) {
		while (g_jobs.empty() && !g_quit) SDL_CondWait(g_cv, g_mu);
		if (g_jobs.empty() && g_quit) break; // finished: everything that was posted has run
		Job job = std::move(g_jobs.front());
		g_jobs.pop_front();
		SDL_UnlockMutex(g_mu);
		if (!job.history) job.fn(d);
		else if (hist) job.fn(hist);
		SDL_LockMutex(g_mu);
	}
	g_ready = false;
	g_histReady = false;
	SDL_UnlockMutex(g_mu);
	if (hist) sqlite3_close(hist);
	sqlite3_close(d);
	return 0;
}

} // namespace

bool exec(sqlite3* d, const char* sql) {
	char* err = nullptr;
	if (sqlite3_exec(d, sql, nullptr, nullptr, &err) != SQLITE_OK) {
		printf("db: SQL error: %s [%s]\n", err ? err : "?", sql);
		sqlite3_free(err);
		return false;
	}
	return true;
}

bool init(const char* path) {
	if (g_thread) return g_ready;
	g_path = path;
	g_mu = SDL_CreateMutex();
	g_cv = SDL_CreateCond();
	g_quit = false;
	g_opened = false;
	g_ready = false;
	g_thread = SDL_CreateThread(threadMain, "db-worker", nullptr);
	if (!g_thread) return false;
	SDL_LockMutex(g_mu);
	while (!g_opened) {
		if (SDL_CondWaitTimeout(g_cv, g_mu, 15000) == SDL_MUTEX_TIMEDOUT) break;
	}
	const bool ok = g_ready;
	SDL_UnlockMutex(g_mu);
	printf("db: %s %s\n", g_path.c_str(), ok ? "ready" : "NOT available");
	return ok;
}

bool ready() {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool r = g_ready && !g_quit;
	SDL_UnlockMutex(g_mu);
	return r;
}

bool post(std::function<void(sqlite3*)> job) {
	if (!g_mu) return false;
	bool queued = false;
	SDL_LockMutex(g_mu);
	if (g_ready && !g_quit) {
		g_jobs.push_back(Job{ std::move(job), false });
		SDL_CondSignal(g_cv);
		queued = true;
	}
	SDL_UnlockMutex(g_mu);
	return queued;
}

bool postHistory(std::function<void(sqlite3*)> job) {
	if (!g_mu) return false;
	bool queued = false;
	SDL_LockMutex(g_mu);
	if (g_ready && g_histReady && !g_quit) {
		g_jobs.push_back(Job{ std::move(job), true });
		SDL_CondSignal(g_cv);
		queued = true;
	}
	SDL_UnlockMutex(g_mu);
	return queued;
}

bool historyReady() {
	if (!g_mu) return false;
	SDL_LockMutex(g_mu);
	const bool r = g_histReady && !g_quit;
	SDL_UnlockMutex(g_mu);
	return r;
}

namespace {
struct Sync {
	SDL_mutex* mu = SDL_CreateMutex();
	SDL_cond* cv = SDL_CreateCond();
	bool done = false;
	~Sync() { SDL_DestroyCond(cv); SDL_DestroyMutex(mu); }
};
}

bool call(std::function<void(sqlite3*)> job) {
	std::shared_ptr<Sync> sync = std::make_shared<Sync>();
	const bool queued = post([job, sync](sqlite3* d) {
		job(d);
		SDL_LockMutex(sync->mu);
		sync->done = true;
		SDL_CondBroadcast(sync->cv);
		SDL_UnlockMutex(sync->mu);
	});
	if (!queued) return false;
	SDL_LockMutex(sync->mu);
	while (!sync->done) SDL_CondWait(sync->cv, sync->mu);
	SDL_UnlockMutex(sync->mu);
	return true;
}

void shutdown() {
	if (!g_thread) return;
	SDL_LockMutex(g_mu);
	g_quit = true;
	SDL_CondBroadcast(g_cv);
	SDL_UnlockMutex(g_mu);
	SDL_WaitThread(g_thread, nullptr);
	g_thread = nullptr;
}

} // namespace db
