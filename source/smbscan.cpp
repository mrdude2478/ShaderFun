#include "smbscan.h"
#include "db.h"
#include "player.h"
#include "smblib.h"
#include "smbnet.h"
#include "tags.h"
#include "ui.h"

#include <SDL2/SDL.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <deque>
#include <memory>
#include <time.h>
#include <vector>

namespace smbscan {
namespace {

// ------------------------------------------------------------------ shared
std::atomic<bool> g_running(false);
std::atomic<int> g_phase(0);            // 0 idle, 1 listing files, 2 reading tags
SDL_mutex* g_mu = nullptr;
std::string g_result;                   // a message waiting to be shown
bool g_resultFresh = false;

const char* const LOG_PATH = "sdmc:/switch/shaderfun/smbscan.log"; // a few lines per scan, kept between runs

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

// bind a text value, or NULL when it is empty / a number, or NULL when it is 0
void bindText(sqlite3_stmt* st, int i, const std::string& v) {
	if (v.empty()) sqlite3_bind_null(st, i); else sqlite3_bind_text(st, i, v.c_str(), -1, SQLITE_TRANSIENT);
}
void bindInt(sqlite3_stmt* st, int i, int v) {
	if (v <= 0) sqlite3_bind_null(st, i); else sqlite3_bind_int(st, i, v);
}

// ------------------------------------------------------------------ pass 1: files
struct Row {
	std::string path, folder, filename, ext;
	int64_t size = 0, mtime = 0;
};

std::vector<Row> g_buf;                 // collected on the network thread, handed to the database in batches
std::atomic<int> g_files(0), g_folders(0);
int64_t g_scanId = 0;                   // the start time of the scan: marks the rows this scan has seen
std::string g_prefix;                   // "smb://<start folder>": only rows below it may be removed
uint32_t g_startTicks = 0;

std::string lowerExt(const std::string& name) {
	const size_t dot = name.rfind('.');
	if (dot == std::string::npos) return "";
	std::string e = name.substr(dot);
	for (char& c : e) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	return e;
}

// runs on the database thread
void upsertRows(sqlite3* d, const std::vector<Row>& rows, int64_t scanId) {
	sqlite3_stmt* st = nullptr;
	const char* sql =
		"INSERT INTO tracks(source,path,folder,filename,ext,size,mtime,first_seen,last_seen,scan_id) "
		"VALUES('smb',?1,?2,?3,?4,?5,?6,?7,?7,?7) "
		"ON CONFLICT(source,path) DO UPDATE SET "
		" folder=excluded.folder, filename=excluded.filename, ext=excluded.ext,"
		" tags_state=CASE WHEN tracks.size IS NOT excluded.size OR tracks.mtime IS NOT excluded.mtime THEN 0 ELSE tracks.tags_state END,"
		" size=excluded.size, mtime=excluded.mtime, last_seen=excluded.last_seen, scan_id=excluded.scan_id;";
	if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) != SQLITE_OK) { printf("scan: prepare failed: %s\n", sqlite3_errmsg(d)); return; }
	db::exec(d, "BEGIN;");
	for (const Row& r : rows) {
		sqlite3_bind_text(st, 1, r.path.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 2, r.folder.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 3, r.filename.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 4, r.ext.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int64(st, 5, (sqlite3_int64)r.size);
		sqlite3_bind_int64(st, 6, (sqlite3_int64)r.mtime);
		sqlite3_bind_int64(st, 7, (sqlite3_int64)scanId);
		if (sqlite3_step(st) != SQLITE_DONE) printf("scan: insert failed: %s\n", sqlite3_errmsg(d));
		sqlite3_reset(st);
	}
	db::exec(d, "COMMIT;");
	sqlite3_finalize(st);
}

void flushRows() {
	if (g_buf.empty()) return;
	std::shared_ptr<std::vector<Row>> rows = std::make_shared<std::vector<Row>>();
	rows->swap(g_buf);
	const int64_t scanId = g_scanId;
	db::post([rows, scanId](sqlite3* d) { upsertRows(d, *rows, scanId); });
}

// runs on the network thread, for every folder
void onDir(const std::string& dir, const std::vector<smbnet::Entry>& entries) {
	g_folders++;
	const std::string folderPath = "smb://" + dir;
	for (const smbnet::Entry& e : entries) {
		if (e.isDir || !player::isMusicFile(e.name)) continue;
		Row r;
		r.folder = folderPath;
		r.filename = e.name;
		r.path = (dir.empty() ? std::string("smb://") : folderPath + "/") + e.name;
		r.ext = lowerExt(e.name);
		r.size = (int64_t)e.size;
		r.mtime = e.mtime;
		g_buf.push_back(std::move(r));
		g_files++;
	}
	if (g_buf.size() >= 300) flushRows(); // one transaction per batch: each commit takes about 50 ms on the SD card
}

// ------------------------------------------------------------------ pass 2: tags
struct Pending {
	int64_t id = 0;
	std::string path;
	uint64_t size = 0;
};

struct TagRow {
	int64_t id = 0;
	int state = 0;                       // 1 = tags read, 2 = the file has none
	std::string title, artist, album, albumArtist, genre, year, format;
	int track = 0, disc = 0, bitrate = 0, sampleRate = 0, channels = 0;
};

std::deque<Pending> g_pending;           // network thread only
int64_t g_lastId = 0;
std::vector<TagRow> g_tagBuf;
std::atomic<int> g_tagTotal(0), g_tagDone(0), g_tagNone(0), g_tagFailed(0);
int g_consecutiveFails = 0;
bool g_tagAbort = false;
std::string g_tagAbortMsg;
uint32_t g_tagStartTicks = 0;

int leadingNumber(const std::string& s) { return atoi(s.c_str()); } // "3/12" -> 3

std::string fileNameOf(const std::string& path) {
	const size_t p = path.rfind('/');
	return p == std::string::npos ? path : path.substr(p + 1);
}

// runs on the database thread
void updateTags(sqlite3* d, const std::vector<TagRow>& rows) {
	sqlite3_stmt* st = nullptr;
	const char* sql = "UPDATE tracks SET tags_state=?1, title=?2, artist=?3, album=?4, albumartist=?5, genre=?6, year=?7,"
		" trackno=?8, discno=?9, format=?10, bitrate=?11, samplerate=?12, channels=?13 WHERE id=?14;";
	if (sqlite3_prepare_v2(d, sql, -1, &st, nullptr) != SQLITE_OK) { printf("scan: prepare failed: %s\n", sqlite3_errmsg(d)); return; }
	db::exec(d, "BEGIN;");
	for (const TagRow& r : rows) {
		sqlite3_bind_int(st, 1, r.state);
		bindText(st, 2, r.title);
		bindText(st, 3, r.artist);
		bindText(st, 4, r.album);
		bindText(st, 5, r.albumArtist);
		bindText(st, 6, r.genre);
		bindText(st, 7, r.year);
		bindInt(st, 8, r.track);
		bindInt(st, 9, r.disc);
		bindText(st, 10, r.format);
		bindInt(st, 11, r.bitrate);
		bindInt(st, 12, r.sampleRate);
		bindInt(st, 13, r.channels);
		sqlite3_bind_int64(st, 14, (sqlite3_int64)r.id);
		if (sqlite3_step(st) != SQLITE_DONE) printf("scan: update failed: %s\n", sqlite3_errmsg(d));
		sqlite3_reset(st);
	}
	db::exec(d, "COMMIT;");
	sqlite3_finalize(st);
}

void flushTags() {
	if (g_tagBuf.empty()) return;
	std::shared_ptr<std::vector<TagRow>> rows = std::make_shared<std::vector<TagRow>>();
	rows->swap(g_tagBuf);
	db::post([rows](sqlite3* d) { updateTags(d, *rows); });
}

// the next batch of songs without tags (network thread; waits for the database thread)
bool loadPending() {
	std::vector<Pending> rows;
	const int64_t after = g_lastId;
	const bool ok = db::call([&rows, after](sqlite3* d) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(d, "SELECT id,path,size FROM tracks WHERE source='smb' AND tags_state=0 AND id>?1 ORDER BY id LIMIT 40;", -1, &st, nullptr) != SQLITE_OK) return;
		sqlite3_bind_int64(st, 1, (sqlite3_int64)after);
		while (sqlite3_step(st) == SQLITE_ROW) {
			Pending p;
			p.id = (int64_t)sqlite3_column_int64(st, 0);
			const unsigned char* t = sqlite3_column_text(st, 1);
			p.path = t ? (const char*)t : "";
			p.size = (uint64_t)sqlite3_column_int64(st, 2);
			rows.push_back(std::move(p));
		}
		sqlite3_finalize(st);
	});
	if (!ok || rows.empty()) return false;
	g_lastId = rows.back().id;
	for (Pending& p : rows) g_pending.push_back(std::move(p));
	return true;
}

// One song per step, so playing and browsing can get in between.
bool tagStep() {
	if (g_pending.empty() && !loadPending()) return false; // nothing left to do
	const Pending p = g_pending.front();
	g_pending.pop_front();

	smbnet::RemoteFile rf;
	uint64_t size = 0;
	if (!rf.open(p.path, &size)) {
		g_tagFailed++;
		if (++g_consecutiveFails >= 10) { g_tagAbort = true; g_tagAbortMsg = rf.error.empty() ? std::string("Too many songs could not be opened") : rf.error; return false; }
		return true; // stays "not read yet": the next scan tries it again
	}
	g_consecutiveFails = 0;
	if (size == 0) size = p.size;

	// Reading from the network in big pieces: the tag reader asks for small bits, which are served from two windows
	// (one near the start, one at the end of the file) so a song needs only a few network reads.
	struct Win { uint64_t start = 0; std::vector<uint8_t> data; };
	Win head, tail;
	const uint64_t TAIL = 16384;
	const uint64_t tailStart = size > TAIL ? size - TAIL : 0;
	auto fetch = [&](uint64_t off, void* buf, size_t n) -> bool {
		for (Win* w : { &head, &tail }) {
			if (!w->data.empty() && off >= w->start && off + n <= w->start + w->data.size()) {
				memcpy(buf, w->data.data() + (off - w->start), n);
				return true;
			}
		}
		Win& w = (off >= tailStart) ? tail : head;
		uint64_t start = off;
		size_t len;
		if (off >= tailStart) { start = tailStart; len = (size_t)(size - tailStart); }
		else { len = (off < (1u << 20)) ? 65536 : 16384; if (len < n) len = n; if (start + len > size) len = (size_t)(size - start); }
		if (len < n) return false;
		if (!rf.read(start, len, w.data) || w.data.size() < n + (size_t)(off - start)) { w.data.clear(); return false; }
		w.start = start;
		memcpy(buf, w.data.data() + (off - start), n);
		return true;
	};

	tags::TrackTags t;
	TagRow row;
	row.id = p.id;
	const bool ok = tags::readRemote(fileNameOf(p.path), size, fetch, t);
	if (ok && t.found) {
		row.state = 1;
		row.title = t.title; row.artist = t.artist; row.album = t.album; row.albumArtist = t.albumArtist;
		row.genre = t.genre; row.year = t.year;
		row.track = leadingNumber(t.track); row.disc = leadingNumber(t.disc);
	}
	else {
		row.state = 2;
		g_tagNone++;
	}
	if (ok) { row.format = t.format; row.bitrate = t.bitrateKbps; row.sampleRate = t.sampleRate; row.channels = t.channels; }
	g_tagBuf.push_back(std::move(row));
	g_tagDone++;
	if (g_tagBuf.size() >= 25) flushTags();
	return true;
}

void onTagsDone(bool cancelled, const std::string& message) {
	flushTags();
	smblib::reload(); // the menu's network library shows the new tags
	const std::string how = g_tagAbort ? "stopped: " + g_tagAbortMsg : cancelled ? "stopped" : "finished";
	logLine("TAGS   END    " + clockText(time(NULL)) + "  elapsed " + elapsedText(SDL_GetTicks() - g_tagStartTicks) + "  songs read " + std::to_string((int)g_tagDone)
		+ "  without tags " + std::to_string((int)g_tagNone) + "  could not be opened " + std::to_string((int)g_tagFailed) + "  " + how + "\n");
	if (g_tagAbort) setResult("Reading tags stopped: " + g_tagAbortMsg);
	else if (cancelled) setResult("Reading tags stopped after " + std::to_string((int)g_tagDone) + " songs (it carries on next time)");
	else setResult("Tags read for " + std::to_string((int)g_tagDone) + " songs (" + std::to_string((int)g_tagNone) + " have no tags)");
	g_phase = 0;
	g_running = false;
}

// starts pass 2 (network thread, at the end of pass 1)
void startTags() {
	int total = 0;
	db::call([&total](sqlite3* d) {
		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(d, "SELECT COUNT(*) FROM tracks WHERE source='smb' AND tags_state=0;", -1, &st, nullptr) == SQLITE_OK) {
			if (sqlite3_step(st) == SQLITE_ROW) total = sqlite3_column_int(st, 0);
			sqlite3_finalize(st);
		}
	});
	g_tagTotal = total;
	g_tagDone = 0; g_tagNone = 0; g_tagFailed = 0;
	g_pending.clear();
	g_lastId = 0;
	g_tagBuf.clear();
	g_consecutiveFails = 0;
	g_tagAbort = false;
	g_tagAbortMsg.clear();
	if (total == 0) {
		logLine("TAGS   nothing to read (all songs already have their tags)\n");
		g_phase = 0;
		g_running = false;
		return;
	}
	g_tagStartTicks = SDL_GetTicks();
	g_phase = 2;
	logLine("TAGS   START  " + clockText(time(NULL)) + "  songs to read " + std::to_string(total));
	if (!smbnet::startTask(tagStep, onTagsDone)) {
		logLine("TAGS   could not start (another task is running)\n");
		g_phase = 0;
		g_running = false;
	}
}

// ------------------------------------------------------------------ end of pass 1
void onDone(bool completed, int folderErrors, const std::string& message) {
	flushRows();
	const int files = g_files, folders = g_folders;
	logLine("END    " + clockText(time(NULL)) + "  elapsed " + elapsedText(SDL_GetTicks() - g_startTicks) + "  songs " + std::to_string(files)
		+ "  folders " + std::to_string(folders) + "  folder errors " + std::to_string(folderErrors) + "  "
		+ (completed ? (folderErrors == 0 ? "finished" : "finished with problems") : (message.empty() ? "stopped" : "stopped: " + message)));
	if (!completed) {
		smblib::reload();
		logLine("");
		setResult(message.empty() ? std::string("SMB scan stopped") : message);
		g_phase = 0;
		g_running = false;
		return;
	}
	if (folderErrors == 0) {
		// everything was visited: rows this scan did not see are songs that no longer exist
		const int64_t scanId = g_scanId;
		const std::string prefix = g_prefix;
		db::post([scanId, prefix, files](sqlite3* d) {
			sqlite3_stmt* st = nullptr;
			if (sqlite3_prepare_v2(d, "DELETE FROM tracks WHERE source='smb' AND scan_id<>?1 AND substr(path,1,length(?2))=?2;", -1, &st, nullptr) == SQLITE_OK) {
				sqlite3_bind_int64(st, 1, (sqlite3_int64)scanId);
				sqlite3_bind_text(st, 2, prefix.c_str(), -1, SQLITE_STATIC);
				sqlite3_step(st);
				sqlite3_finalize(st);
			}
			const std::string t = std::to_string((long long)scanId);
			const std::string f = std::to_string(files);
			sqlite3_stmt* m = nullptr;
			if (sqlite3_prepare_v2(d, "INSERT OR REPLACE INTO meta(key,value) VALUES(?1,?2);", -1, &m, nullptr) == SQLITE_OK) {
				sqlite3_bind_text(m, 1, "smb_last_scan", -1, SQLITE_STATIC); sqlite3_bind_text(m, 2, t.c_str(), -1, SQLITE_STATIC); sqlite3_step(m); sqlite3_reset(m);
				sqlite3_bind_text(m, 1, "smb_last_scan_files", -1, SQLITE_STATIC); sqlite3_bind_text(m, 2, f.c_str(), -1, SQLITE_STATIC); sqlite3_step(m);
				sqlite3_finalize(m);
			}
		});
	}
	else {
		setResult("SMB scan finished with problems: " + std::to_string(folderErrors) + " folders could not be read (nothing was removed)");
	}
	smblib::reload(); // the network library in the menu shows the new file list right away
	startTags(); // pass 2: read the tags of everything that has none yet
}

} // namespace

bool start() {
	if (g_running) return false;
	smbnet::loadSettings();
	if (!smbnet::configured()) { setResult("Set up the network share first"); return false; }
	if (!db::ready()) { logLine("Could not start a scan at " + clockText(time(NULL)) + ": the database is not available\n"); setResult("The database is not available"); return false; }
	g_scanId = (int64_t)time(NULL);
	g_files = 0;
	g_folders = 0;
	g_buf.clear();
	const std::string startDir = smbnet::startFolder();
	g_prefix = "smb://" + startDir;
	g_running = true;
	g_phase = 1;
	g_startTicks = SDL_GetTicks();
	logLine("START  " + clockText(time(NULL)) + "  " + smbnet::label() + (startDir.empty() ? std::string() : " / " + startDir));
	if (!smbnet::startTreeScan(startDir, onDir, onDone)) { g_phase = 0; g_running = false; return false; }
	return true;
}

void cancel() {
	smbnet::cancelTreeScan();
	smbnet::cancelTask();
}

bool running() { return g_running; }

std::string status() {
	if (g_phase == 2) return "reading tags: " + std::to_string((int)g_tagDone) + " of " + std::to_string((int)g_tagTotal);
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

} // namespace smbscan
