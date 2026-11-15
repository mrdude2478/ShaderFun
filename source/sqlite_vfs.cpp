// SQLite file layer for the Switch.
//
// libsqlite3.a was built with -DSQLITE_OS_OTHER=1, which leaves out SQLite's own file / OS code: the application has
// to provide sqlite3_os_init() and sqlite3_os_end() and register a "VFS" (the part that opens, reads and writes files).
// This is that part, written with plain open / read / write / fsync, which the SD card (sdmc:) supports.
//
// Notes:
//  - There is no file locking: only this app opens the database, and only from one thread (the library was built with
//    SQLITE_THREADSAFE=0, so every call must come from the same thread, or be protected by a mutex).
//  - Rollback journals are used (the default). WAL mode is not available because it needs shared memory.

extern "C" {
#include <sqlite3.h>
}

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace {

// What went wrong at the file level (for the test report): the last few failed operations, with errno
char g_log[1024] = "";
void note(const char* what, long long a = 0, long long b = 0) {
	const size_t used = strlen(g_log);
	if (used + 120 >= sizeof(g_log)) return; // keep the first failures: they are the interesting ones
	snprintf(g_log + used, sizeof(g_log) - used, "%s errno=%d(%s) a=%lld b=%lld; ", what, errno, strerror(errno), a, b);
}

struct SwitchFile {
	sqlite3_file base;      // must be first
	int fd;
	sqlite3_int64 size;     // size we know about: lets writes past the end first grow the file
	char* deleteOnClose;    // path to delete when closed (temporary files), or NULL
};

int fsClose(sqlite3_file* f) {
	SwitchFile* p = (SwitchFile*)f;
	int rc = SQLITE_OK;
	if (p->fd >= 0 && close(p->fd) != 0) { note("close"); rc = SQLITE_IOERR_CLOSE; }
	p->fd = -1;
	if (p->deleteOnClose) {
		unlink(p->deleteOnClose);
		free(p->deleteOnClose);
		p->deleteOnClose = NULL;
	}
	return rc;
}

int fsRead(sqlite3_file* f, void* buf, int amt, sqlite3_int64 ofst) {
	SwitchFile* p = (SwitchFile*)f;
	char* out = (char*)buf;
	// The SD card's file system returns an error when asked to read at or past the end of a file, while SQLite relies on
	// getting a "short read" there (for example when it looks at the header of a new, empty database). So only ask for
	// what the file really holds, and fill the rest with zeros.
	sqlite3_int64 avail = p->size - ofst;
	if (avail < 0) avail = 0;
	const int want = (avail >= amt) ? amt : (int)avail;
	int got = 0;
	if (want > 0) {
		if (lseek(p->fd, (off_t)ofst, SEEK_SET) < 0) { note("read-seek", ofst, amt); return SQLITE_IOERR_READ; }
		while (got < want) {
			const ssize_t n = read(p->fd, out + got, (size_t)(want - got));
			if (n < 0) { note("read", ofst, amt); return SQLITE_IOERR_READ; }
			if (n == 0) break; // end of file earlier than we thought
			got += (int)n;
		}
	}
	if (got == amt) return SQLITE_OK;
	memset(out + got, 0, (size_t)(amt - got)); // SQLite expects the unread rest to be zero
	return SQLITE_IOERR_SHORT_READ;
}

int fsWrite(sqlite3_file* f, const void* buf, int amt, sqlite3_int64 ofst) {
	SwitchFile* p = (SwitchFile*)f;
	if (ofst > p->size) { // writing beyond the end: grow the file first (the gap reads as zeros)
		if (ftruncate(p->fd, (off_t)ofst) != 0) { note("grow", ofst, p->size); return SQLITE_FULL; }
		p->size = ofst;
	}
	if (lseek(p->fd, (off_t)ofst, SEEK_SET) < 0) { note("write-seek", ofst, amt); return SQLITE_IOERR_WRITE; }
	const char* in = (const char*)buf;
	int done = 0;
	while (done < amt) {
		const ssize_t n = write(p->fd, in + done, (size_t)(amt - done));
		if (n <= 0) { note("write", ofst, amt); return (errno == ENOSPC) ? SQLITE_FULL : SQLITE_IOERR_WRITE; }
		done += (int)n;
	}
	if (ofst + amt > p->size) p->size = ofst + amt;
	return SQLITE_OK;
}

int fsTruncate(sqlite3_file* f, sqlite3_int64 size) {
	SwitchFile* p = (SwitchFile*)f;
	if (ftruncate(p->fd, (off_t)size) != 0) { note("truncate", size); return SQLITE_IOERR_TRUNCATE; }
	p->size = size;
	return SQLITE_OK;
}

int fsSync(sqlite3_file* f, int) {
	SwitchFile* p = (SwitchFile*)f;
	if (fsync(p->fd) != 0) { note("fsync"); return SQLITE_IOERR_FSYNC; }
	return SQLITE_OK;
}

int fsFileSize(sqlite3_file* f, sqlite3_int64* pSize) {
	SwitchFile* p = (SwitchFile*)f;
	struct stat st;
	if (fstat(p->fd, &st) != 0) { note("fstat"); return SQLITE_IOERR_FSTAT; }
	*pSize = (sqlite3_int64)st.st_size;
	p->size = *pSize;
	return SQLITE_OK;
}

int fsLock(sqlite3_file*, int) { return SQLITE_OK; }
int fsUnlock(sqlite3_file*, int) { return SQLITE_OK; }
int fsCheckReservedLock(sqlite3_file*, int* pResOut) { *pResOut = 0; return SQLITE_OK; }
int fsFileControl(sqlite3_file*, int, void*) { return SQLITE_NOTFOUND; }
int fsSectorSize(sqlite3_file*) { return 4096; }
int fsDeviceCharacteristics(sqlite3_file*) { return 0; }

const sqlite3_io_methods g_ioMethods = {
	1,                      // iVersion
	fsClose, fsRead, fsWrite, fsTruncate, fsSync, fsFileSize,
	fsLock, fsUnlock, fsCheckReservedLock, fsFileControl,
	fsSectorSize, fsDeviceCharacteristics,
	0, 0, 0, 0, 0, 0        // version 2+ (shared memory, mmap): not used
};

int vfsOpen(sqlite3_vfs*, const char* zName, sqlite3_file* f, int flags, int* pOutFlags) {
	SwitchFile* p = (SwitchFile*)f;
	memset(p, 0, sizeof(*p));
	p->fd = -1;

	char tmpName[160];
	const char* path = zName;
	bool isTemp = false;
	if (!path) { // SQLite asks for an anonymous temporary file
		static int counter = 0;
		snprintf(tmpName, sizeof(tmpName), "sdmc:/switch/shaderfun/sqlite_tmp_%d_%d.tmp", (int)time(NULL) & 0xFFFF, counter++);
		path = tmpName;
		isTemp = true;
	}

	int oflags = (flags & SQLITE_OPEN_READWRITE) ? O_RDWR : O_RDONLY;
	if (flags & SQLITE_OPEN_CREATE) oflags |= O_CREAT;
	if (flags & SQLITE_OPEN_EXCLUSIVE) oflags |= O_EXCL;

	const int fd = open(path, oflags, 0644);
	if (fd < 0) { note("open", flags); return SQLITE_CANTOPEN; }

	p->fd = fd;
	struct stat st;
	p->size = (fstat(fd, &st) == 0) ? (sqlite3_int64)st.st_size : 0;
	if (isTemp || (flags & SQLITE_OPEN_DELETEONCLOSE)) p->deleteOnClose = strdup(path);
	p->base.pMethods = &g_ioMethods;
	if (pOutFlags) *pOutFlags = flags;
	return SQLITE_OK;
}

int vfsDelete(sqlite3_vfs*, const char* zName, int) {
	if (unlink(zName) == 0) return SQLITE_OK;
	if (errno != ENOENT) note("delete");
	return (errno == ENOENT) ? SQLITE_IOERR_DELETE_NOENT : SQLITE_IOERR_DELETE;
}

int vfsAccess(sqlite3_vfs*, const char* zName, int flags, int* pResOut) {
	struct stat st;
	const bool exists = (stat(zName, &st) == 0);
	if (flags == SQLITE_ACCESS_EXISTS) *pResOut = exists ? 1 : 0;
	else *pResOut = exists ? 1 : 0; // READ / READWRITE: the SD card has no permissions to check
	return SQLITE_OK;
}

int vfsFullPathname(sqlite3_vfs*, const char* zName, int nOut, char* zOut) {
	sqlite3_snprintf(nOut, zOut, "%s", zName); // paths are always given in full ("sdmc:/...")
	return SQLITE_OK;
}

int vfsRandomness(sqlite3_vfs*, int nByte, char* zOut) {
#ifdef __SWITCH__
	randomGet(zOut, (size_t)nByte);
#else
	for (int i = 0; i < nByte; i++) zOut[i] = (char)(rand() & 0xFF);
#endif
	return nByte;
}

int vfsSleep(sqlite3_vfs*, int microseconds) {
#ifdef __SWITCH__
	svcSleepThread((s64)microseconds * 1000);
#else
	usleep((useconds_t)microseconds);
#endif
	return microseconds;
}

int vfsCurrentTimeInt64(sqlite3_vfs*, sqlite3_int64* pTime) {
	static const sqlite3_int64 unixEpoch = 24405875 * (sqlite3_int64)8640000; // 1970-01-01 in milliseconds since the Julian epoch
	*pTime = unixEpoch + (sqlite3_int64)time(NULL) * 1000;
	return SQLITE_OK;
}

int vfsCurrentTime(sqlite3_vfs* vfs, double* pTime) {
	sqlite3_int64 t = 0;
	vfsCurrentTimeInt64(vfs, &t);
	*pTime = (double)t / 86400000.0;
	return SQLITE_OK;
}

int vfsGetLastError(sqlite3_vfs*, int, char*) { return 0; }

sqlite3_vfs g_vfs = {
	2,                      // iVersion
	(int)sizeof(SwitchFile),
	512,                    // mxPathname
	NULL,                   // pNext
	"switchfs",
	NULL,                   // pAppData
	vfsOpen, vfsDelete, vfsAccess, vfsFullPathname,
	NULL, NULL, NULL, NULL, // dynamic loading: not used
	vfsRandomness, vfsSleep, vfsCurrentTime, vfsGetLastError,
	vfsCurrentTimeInt64,
	NULL, NULL, NULL        // system calls: not used
};

} // namespace

// the failed file operations so far (empty when there were none)
extern "C" const char* switchfs_log(void) { return g_log; }

// Called by SQLite itself when it starts / stops (this is what the linker was missing)
extern "C" int sqlite3_os_init(void) { return sqlite3_vfs_register(&g_vfs, 1); }
extern "C" int sqlite3_os_end(void) { return SQLITE_OK; }
