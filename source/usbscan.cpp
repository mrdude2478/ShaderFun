#include "usbscan.h"
#include "db.h"
#include "player.h"
#include "tags.h"
#include "ui.h"
#include "usbdrive.h"
#include "usblib.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

namespace usbscan {
namespace {

const char* const LOG_PATH = "sdmc:/switch/shaderfun/usbscan.log";

struct Known { int64_t size = 0, mtime = 0; int state = 0; };

struct Row {
	std::string path, folder, filename, ext;
	int64_t size = 0, mtime = 0;
	bool tagsRead = false;                 // false: unchanged file, only "seen again" is recorded
	int state = 0;                         // 1 = tags read, 2 = the file has none
	std::string title, artist, album, albumArtist, genre, year, format;
	int track = 0, disc = 0, bitrate = 0, sampleRate = 0, channels = 0;
};

SDL_Thread* g_thread = nullptr;
SDL_mutex* g_mu = nullptr;
std::atomic<bool> g_running(false), g_cancel(false);
std::atomic<int> g_files(0), g_folders(0), g_read(0);
std::string g_result;
bool g_resultFresh = false;

usbdrive::Drive g_drive;
int64_t g_scanId = 0;
uint32_t g_startTicks = 0;

void ensureMutex() { if (!g_mu) g_mu = SDL_CreateMutex(); }

void setResult(const std::string& m) {
	ensureMutex();
	SDL_LockMutex(g_mu);
	g_result = m;
	g_resultFresh = true;
	SDL_UnlockMutex(g_mu);
}

std::string clockText(time_t t) {
	char buf[32] = "?";
	struct tm tmv;
	struct tm* p = localtime(&t);
	if (p) { tmv = *p; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv); }
	return buf;
}

void logLine(const std::string& text) {
	ensureMutex();
	SDL_LockMutex(g_mu);
	if (FILE* f = fopen(LOG_PATH, "a")) { fputs(text.c_str(), f); fputc('\n', f); fclose(f); }
	SDL_UnlockMutex(g_mu);
}

std::string elapsedText(uint32_t ms) {
	const uint32_t total = ms / 1000;
	char buf[64];
	snprintf(buf, sizeof(buf), "%u min %u s", total / 60, total % 60);
	return buf;
}

void bindText(sqlite3_stmt* st, int i, const std::string& v) {
	if (v.empty()) sqlite3_bind_null(st, i); else sqlite3_bind_text(st, i, v.c_str(), -1, SQLITE_TRANSIENT);
}
void bindInt(sqlite3_stmt* st, int i, int v) {
	if (v <= 0) sqlite3_bind_null(st, i); else sqlite3_bind_int(st, i, v);
}

std::string lowerExt(const std::string& name) {
	const size_t dot = name.rfind('.');
	if (dot == std::string::npos) return "";
	std::string e = name.substr(dot);
	for (char& c : e) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	return e;
}

// runs on the database thread
void writeRows(sqlite3* d, const std::vector<Row>& rows, const std::string& driveId, int64_t scanId) {
	sqlite3_stmt* full = nullptr;
	sqlite3_stmt* seen = nullptr;
	const char* fullSql =
		"INSERT INTO tracks(source,drive_id,path,folder,filename,ext,size,mtime,tags_state,title,artist,album,albumartist,genre,year,"
		" trackno,discno,format,bitrate,samplerate,channels,first_seen,last_seen,scan_id) "
		"VALUES('usb',?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?21,?21) "
		"ON CONFLICT(source,path) DO UPDATE SET drive_id=excluded.drive_id, folder=excluded.folder, filename=excluded.filename, ext=excluded.ext,"
		" size=excluded.size, mtime=excluded.mtime, tags_state=excluded.tags_state, title=excluded.title, artist=excluded.artist,"
		" album=excluded.album, albumartist=excluded.albumartist, genre=excluded.genre, year=excluded.year, trackno=excluded.trackno,"
		" discno=excluded.discno, format=excluded.format, bitrate=excluded.bitrate, samplerate=excluded.samplerate,"
		" channels=excluded.channels, last_seen=excluded.last_seen, scan_id=excluded.scan_id;";
	if (sqlite3_prepare_v2(d, fullSql, -1, &full, nullptr) != SQLITE_OK) { printf("usbscan: prepare failed: %s\n", sqlite3_errmsg(d)); return; }
	sqlite3_prepare_v2(d, "UPDATE tracks SET last_seen=?1, scan_id=?1 WHERE source='usb' AND path=?2;", -1, &seen, nullptr);
	db::exec(d, "BEGIN;");
	for (const Row& r : rows) {
		if (!r.tagsRead) {
			if (!seen) continue;
			sqlite3_bind_int64(seen, 1, (sqlite3_int64)scanId);
			sqlite3_bind_text(seen, 2, r.path.c_str(), -1, SQLITE_STATIC);
			sqlite3_step(seen);
			sqlite3_reset(seen);
			continue;
		}
		sqlite3_bind_text(full, 1, driveId.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(full, 2, r.path.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(full, 3, r.folder.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(full, 4, r.filename.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(full, 5, r.ext.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int64(full, 6, (sqlite3_int64)r.size);
		sqlite3_bind_int64(full, 7, (sqlite3_int64)r.mtime);
		sqlite3_bind_int(full, 8, r.state);
		bindText(full, 9, r.title);
		bindText(full, 10, r.artist);
		bindText(full, 11, r.album);
		bindText(full, 12, r.albumArtist);
		bindText(full, 13, r.genre);
		bindText(full, 14, r.year);
		bindInt(full, 15, r.track);
		bindInt(full, 16, r.disc);
		bindText(full, 17, r.format);
		bindInt(full, 18, r.bitrate);
		bindInt(full, 19, r.sampleRate);
		bindInt(full, 20, r.channels);
		sqlite3_bind_int64(full, 21, (sqlite3_int64)scanId);
		if (sqlite3_step(full) != SQLITE_DONE) printf("usbscan: insert failed: %s\n", sqlite3_errmsg(d));
		sqlite3_reset(full);
	}
	db::exec(d, "COMMIT;");
	sqlite3_finalize(full);
	if (seen) sqlite3_finalize(seen);
}

void flush(std::vector<Row>& buf) {
	if (buf.empty()) return;
	std::shared_ptr<std::vector<Row>> rows = std::make_shared<std::vector<Row>>();
	rows->swap(buf);
	const std::string id = g_drive.id;
	const int64_t scanId = g_scanId;
	db::post([rows, id, scanId](sqlite3* d) { writeRows(d, *rows, id, scanId); });
}

int leadingNumber(const std::string& s) { return atoi(s.c_str()); } // "3/12" -> 3

bool skipFolder(const std::string& name) {
	return name.empty() || name[0] == '.' || name == "System Volume Information" || name == "$RECYCLE.BIN" || name == "lost+found";
}

int threadMain(void*) {
	const usbdrive::Drive drive = g_drive;
	const std::string root = drive.mount + "/";   // "ums0:/"
	const std::string idPrefix = "usb://" + drive.id;

	// what the database already knows about this drive: unchanged files are not read again
	std::unordered_map<std::string, Known> known;
	db::call([&known, &drive](sqlite3* d) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(d, "SELECT path,size,mtime,tags_state FROM tracks WHERE source='usb' AND drive_id=?1;", -1, &st, nullptr) != SQLITE_OK) return;
		sqlite3_bind_text(st, 1, drive.id.c_str(), -1, SQLITE_STATIC);
		while (sqlite3_step(st) == SQLITE_ROW) {
			const unsigned char* p = sqlite3_column_text(st, 0);
			Known k;
			k.size = (int64_t)sqlite3_column_int64(st, 1);
			k.mtime = (int64_t)sqlite3_column_int64(st, 2);
			k.state = sqlite3_column_int(st, 3);
			if (p) known[(const char*)p] = k;
		}
		sqlite3_finalize(st);
	});

	std::vector<Row> buf;
	std::vector<std::string> stack; // folders still to visit, as paths relative to the drive root ("" = the root)
	stack.push_back("");
	int folderErrors = 0;
	bool aborted = false;
	std::string abortMsg;

	while (!stack.empty() && !g_cancel) {
		const std::string rel = stack.back();
		stack.pop_back();
		const std::string dirPath = root + rel;
		DIR* dir = opendir(dirPath.c_str());
		if (!dir) {
			if (rel.empty()) { aborted = true; abortMsg = "The drive could not be opened (was it removed?)"; break; }
			folderErrors++;
			continue;
		}
		g_folders++;
		while (struct dirent* e = readdir(dir)) {
			if (g_cancel) break;
			const std::string name = e->d_name;
			if (name == "." || name == "..") continue;
			const std::string relPath = rel.empty() ? name : rel + "/" + name;
			const std::string full = root + relPath;
			struct stat st;
			if (stat(full.c_str(), &st) != 0) continue;
			if (S_ISDIR(st.st_mode)) {
				if (!skipFolder(name)) stack.push_back(relPath);
				continue;
			}
			if (!player::isMusicFile(name)) continue;

			Row r;
			r.path = idPrefix + "/" + relPath;
			r.folder = idPrefix + (rel.empty() ? std::string() : "/" + rel);
			r.filename = name;
			r.ext = lowerExt(name);
			r.size = (int64_t)st.st_size;
			r.mtime = (int64_t)st.st_mtime;
			g_files++;

			auto k = known.find(r.path);
			if (k != known.end() && k->second.size == r.size && k->second.mtime == r.mtime && k->second.state != 0) {
				r.tagsRead = false; // unchanged
			}
			else {
				tags::TrackTags t;
				const bool ok = tags::read(full, t, 0.0, true, false);
				r.tagsRead = true;
				r.state = (ok && t.found) ? 1 : 2;
				if (ok) {
					r.title = t.title; r.artist = t.artist; r.album = t.album; r.albumArtist = t.albumArtist;
					r.genre = t.genre; r.year = t.year; r.format = t.format;
					r.track = leadingNumber(t.track); r.disc = leadingNumber(t.disc);
					r.bitrate = t.bitrateKbps; r.sampleRate = t.sampleRate; r.channels = t.channels;
				}
				g_read++;
				SDL_Delay(1); // be gentle: a song may be playing from this same drive
			}
			buf.push_back(std::move(r));
			if (buf.size() >= 100) flush(buf);
		}
		closedir(dir);
	}
	flush(buf);

	const bool completed = !g_cancel && !aborted;
	const int files = g_files, folders = g_folders, read = g_read;
	const int64_t scanId = g_scanId;
	const bool clean = completed && folderErrors == 0;

	// the drive's row in the drives table; and, when every folder was visited, the songs that are no longer on the drive are removed
	const usbdrive::Drive dr = drive;
	db::post([dr, scanId, clean, files](sqlite3* d) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(d,
			"INSERT INTO drives(id,label,maker,product,fs_type,capacity,first_seen,last_seen,last_scan,songs) VALUES(?1,?2,?3,?4,?5,?6,?7,?7,?8,?9) "
			"ON CONFLICT(id) DO UPDATE SET label=excluded.label, maker=excluded.maker, product=excluded.product, fs_type=excluded.fs_type,"
			" capacity=excluded.capacity, last_seen=excluded.last_seen,"
			" last_scan=CASE WHEN ?10 THEN excluded.last_scan ELSE drives.last_scan END,"
			" songs=CASE WHEN ?10 THEN excluded.songs ELSE drives.songs END;", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, dr.id.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 2, dr.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 3, dr.maker.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 4, dr.product.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 5, dr.fsType.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 6, (sqlite3_int64)dr.capacity);
			sqlite3_bind_int64(st, 7, (sqlite3_int64)scanId);
			sqlite3_bind_int64(st, 8, (sqlite3_int64)time(NULL));
			sqlite3_bind_int(st, 9, files);
			sqlite3_bind_int(st, 10, clean ? 1 : 0);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		if (clean && sqlite3_prepare_v2(d, "DELETE FROM tracks WHERE source='usb' AND drive_id=?1 AND scan_id<>?2;", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, dr.id.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, (sqlite3_int64)scanId);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	});

	usblib::reload(); // the menu shows the new songs (queued after the writes above)
	const std::string how = aborted ? "stopped: " + abortMsg : g_cancel ? "stopped" : (folderErrors == 0 ? "finished" : "finished with problems");
	logLine("END    " + clockText(time(NULL)) + "  elapsed " + elapsedText(SDL_GetTicks() - g_startTicks) + "  songs " + std::to_string(files)
		+ "  folders " + std::to_string(folders) + "  tags read " + std::to_string(read) + "  folder errors " + std::to_string(folderErrors) + "  " + how + "\n");
	if (aborted) setResult(abortMsg);
	else if (g_cancel) setResult("USB scan stopped");
	else if (folderErrors > 0) setResult("USB scan finished with problems: " + std::to_string(folderErrors) + " folders could not be read (nothing was removed)");
	else setResult("USB scan finished: " + std::to_string(files) + " songs in " + std::to_string(folders) + " folders (" + std::to_string(read) + " new or changed)");
	g_running = false;
	return 0;
}

} // namespace

bool start(const std::string& mount) {
	if (g_running) return false;
	ensureMutex();
	usbdrive::Drive d;
	if (!usbdrive::findDrive(mount, d)) { setResult("That drive is not connected any more"); return false; }
	if (!db::ready()) { setResult("The database is not available"); return false; }
	if (g_thread) { SDL_WaitThread(g_thread, nullptr); g_thread = nullptr; } // the previous scan has finished
	g_drive = d;
	g_scanId = (int64_t)time(NULL);
	g_files = 0; g_folders = 0; g_read = 0;
	g_cancel = false;
	g_running = true;
	g_startTicks = SDL_GetTicks();
	logLine("START  " + clockText(time(NULL)) + "  " + d.name + "  (id " + d.id + ", " + d.fsType + ")");
	g_thread = SDL_CreateThread(threadMain, "usb-scan", nullptr);
	if (!g_thread) { g_running = false; setResult("The scan could not be started"); return false; }
	return true;
}

void cancel() { g_cancel = true; }

bool running() { return g_running; }

std::string status() {
	return std::to_string((int)g_files) + " songs in " + std::to_string((int)g_folders) + " folders so far";
}

void poll() {
	if (!g_mu) return;
	std::string msg;
	SDL_LockMutex(g_mu);
	if (g_resultFresh) { msg = g_result; g_resultFresh = false; }
	SDL_UnlockMutex(g_mu);
	if (!msg.empty()) ui::toast(msg);
}

void shutdown() {
	g_cancel = true;
	if (g_thread) { SDL_WaitThread(g_thread, nullptr); g_thread = nullptr; }
}

} // namespace usbscan
