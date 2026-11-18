#pragma once
// The music catalogue: one SQLite file, sdmc:/switch/shaderfun/library.db.
//
// One background thread owns the database connection (the SQLite library is built single-threaded, so it must never be used
// from two threads at once). Other threads hand it small jobs with post(); the jobs run one after the other.
//
// The file is also meant to be read by a PC program (copy it off the SD card first, for example through the FTP server).
// Tables (see db.cpp for the exact columns):
//   tracks  one row per song: source ('smb', later 'sd' / 'usb'), path, folder, file name, size, modified time,
//           tags (title, artist, album, ...) once they have been read, and when the song was first / last seen.
//   meta    small key / value notes (for example when the share was last scanned).

#include <functional>
#include <string>

struct sqlite3;

namespace db {

bool init(const char* path);   // starts the thread, opens (or creates) the file. false = not available
void shutdown();               // runs the jobs still waiting, then closes the file
bool ready();
bool post(std::function<void(sqlite3*)> job);   // runs later, on the database thread. false = not queued (no database)
// Runs the job on the database thread and waits until it has finished. Never call it from the database thread itself.
// (The job may write into variables of the caller: it is always finished when call() returns true.)
bool call(std::function<void(sqlite3*)> job);

// The play history lives in a second file, history.db, in the same folder. It is opened by the same thread (SQLite is built
// single-threaded) but is never deleted or rebuilt: a file this version cannot read is moved aside (history.db.bad) and a file
// made by a newer version is left alone (the history is then simply not available).
bool postHistory(std::function<void(sqlite3*)> job);   // runs later on the database thread, with the history database. false = no history database
bool historyReady();

// helpers for jobs
bool exec(sqlite3* db, const char* sql);        // false (and a line in the log) on error

} // namespace db
