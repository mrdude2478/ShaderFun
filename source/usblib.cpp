#include "usblib.h"
#include "db.h"
#include "usbdrive.h"

#include <SDL2/SDL.h>
#include <sqlite3.h>
#include <algorithm>
#include <atomic>
#include <map>

namespace usblib {
namespace {

SDL_mutex* g_mu = nullptr;
std::vector<DriveInfo> g_drives;
std::map<std::string, std::shared_ptr<const library::Snapshot>> g_snaps;
std::shared_ptr<const library::Snapshot> g_empty = std::make_shared<library::Snapshot>();
std::atomic<uint32_t> g_version(0);
std::atomic<int> g_pending(0);

std::string col(sqlite3_stmt* st, int i) {
	const unsigned char* t = sqlite3_column_text(st, i);
	return t ? std::string((const char*)t) : std::string();
}

// runs on the database thread
void loadJob(sqlite3* d) {
	std::vector<DriveInfo> drives;
	std::map<std::string, std::vector<library::Track>> tracks;

	sqlite3_stmt* st = nullptr;
	if (sqlite3_prepare_v2(d, "SELECT id,label,fs_type,capacity,last_scan,songs FROM drives;", -1, &st, nullptr) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW) {
			DriveInfo di;
			di.id = col(st, 0);
			di.label = col(st, 1);
			di.fsType = col(st, 2);
			di.capacity = (uint64_t)sqlite3_column_int64(st, 3);
			di.lastScan = (int64_t)sqlite3_column_int64(st, 4);
			di.songs = sqlite3_column_int(st, 5);
			drives.push_back(std::move(di));
		}
		sqlite3_finalize(st);
	}
	st = nullptr;
	if (sqlite3_prepare_v2(d, "SELECT drive_id,path,title,artist,album,albumartist,genre,year,trackno,discno,size,mtime FROM tracks WHERE source='usb';", -1, &st, nullptr) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW) {
			library::Track t;
			const std::string id = col(st, 0);
			t.path = col(st, 1);
			t.title = col(st, 2);
			t.artist = col(st, 3);
			t.album = col(st, 4);
			t.albumArtist = col(st, 5);
			t.genre = col(st, 6);
			t.year = col(st, 7);
			t.trackNo = sqlite3_column_int(st, 8);
			t.discNo = sqlite3_column_int(st, 9);
			t.size = (uint64_t)sqlite3_column_int64(st, 10);
			t.mtime = (int64_t)sqlite3_column_int64(st, 11);
			tracks[id].push_back(std::move(t));
		}
		sqlite3_finalize(st);
	}

	std::map<std::string, std::shared_ptr<const library::Snapshot>> snaps;
	for (DriveInfo& di : drives) {
		auto it = tracks.find(di.id);
		if (it != tracks.end()) {
			di.songs = (int)it->second.size();
			snaps[di.id] = library::makeSnapshot(std::move(it->second)); // sorts and groups
		}
	}
	std::sort(drives.begin(), drives.end(), [](const DriveInfo& a, const DriveInfo& b) { return a.label < b.label; });

	SDL_LockMutex(g_mu);
	g_drives.swap(drives);
	g_snaps.swap(snaps);
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
	if (!db::post(loadJob)) g_pending--;
}

std::vector<DriveInfo> drives() {
	if (!g_mu) return {};
	SDL_LockMutex(g_mu);
	std::vector<DriveInfo> r = g_drives;
	SDL_UnlockMutex(g_mu);
	return r;
}

std::shared_ptr<const library::Snapshot> snapshot(const std::string& driveId) {
	if (!g_mu) return g_empty;
	SDL_LockMutex(g_mu);
	auto it = g_snaps.find(driveId);
	std::shared_ptr<const library::Snapshot> s = (it != g_snaps.end()) ? it->second : g_empty;
	SDL_UnlockMutex(g_mu);
	return s;
}

std::string label(const std::string& driveId) {
	if (!g_mu) return driveId;
	SDL_LockMutex(g_mu);
	std::string r = driveId;
	for (const DriveInfo& d : g_drives) if (d.id == driveId) { r = d.label; break; }
	SDL_UnlockMutex(g_mu);
	return r;
}

bool connected(const std::string& driveId) {
	for (const usbdrive::Drive& d : usbdrive::drives()) if (d.id == driveId) return true;
	return false;
}

void forgetDrive(const std::string& driveId) {
	if (!g_mu) return;
	SDL_LockMutex(g_mu); // gone from the menu at once; the database follows
	g_drives.erase(std::remove_if(g_drives.begin(), g_drives.end(), [&](const DriveInfo& d) { return d.id == driveId; }), g_drives.end());
	g_snaps.erase(driveId);
	SDL_UnlockMutex(g_mu);
	g_version++;
	db::post([driveId](sqlite3* d) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(d, "DELETE FROM tracks WHERE source='usb' AND drive_id=?1;", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, driveId.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		if (sqlite3_prepare_v2(d, "DELETE FROM drives WHERE id=?1;", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, driveId.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	});
	reload();
}

bool loading() { return g_pending > 0; }

uint32_t version() { return g_version; }

} // namespace usblib
