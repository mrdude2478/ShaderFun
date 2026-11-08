#pragma once
// Network music for ShaderFun: browse and play songs from an SMB2/SMB3 share (a router with a USB drive, a NAS, a PC...).
// All network work happens on a background thread, so the visuals never freeze. Songs are downloaded to a cache
// folder on the SD card (the next song is fetched while the current one plays) and played from there.
//
// Settings live in sdmc:/switch/shaderfun/smb.ini (a template is created the first time you open Network).

#include <stdint.h>
#include <string>
#include <vector>

struct SDL_RWops;

namespace smbnet {

struct Entry {
	std::string name;
	bool isDir = false;
	uint64_t size = 0;
};

// ---- settings
struct Config {                       // what smb.ini holds
	std::string server;               // address of the router / NAS / PC
	std::string share;                // share name
	std::string user;                 // empty = guest
	std::string password;
	std::string domain;               // only for some Windows domains
	std::string folder;               // optional start folder inside the share
};
Config getConfig();                   // the current settings (reads smb.ini if that has not happened yet)
bool saveConfig(const Config& c);     // writes smb.ini and uses the new settings from the next connection on
std::string settingsPath();
bool loadSettings();                 // (re)reads smb.ini and writes a template if it does not exist yet. Returns true when server + share are filled in.
bool configured();                   // server and share are set
std::string label();                 // "192.168.0.1/Music" for the menu
std::string startFolder();           // the optional 'folder=' (a start folder inside the share)
void reload();                       // after editing smb.ini: forget the old connection

// ---- remote paths are written "smb://<path inside the share>", e.g. "smb://Music/Album/01 Song.mp3"
bool isRemote(const std::string& path);

// ---- folder listing (asynchronous)
void requestList(const std::string& dir);   // dir = path inside the share ("" = top of the share)
// true once when a listing has finished (ok=false: 'error' says why)
bool pollList(std::string& dir, std::vector<Entry>& entries, std::string& error, bool& ok);

// ---- downloading songs (asynchronous)
enum FetchState { FETCH_NONE, FETCH_BUSY, FETCH_DONE, FETCH_FAILED };
bool cachedFile(const std::string& remotePath, std::string& localPath);   // already downloaded?
void requestFetch(const std::string& remotePath);                         // wanted right now
void prefetch(const std::string& remotePath);                             // wanted soon (the next song)
FetchState fetchState(const std::string& remotePath, std::string& localPath, std::string& error);
float fetchProgress(const std::string& remotePath);                       // 0..1, or -1 when unknown

// ---- streaming: a song starts playing as soon as its first part has arrived, while the rest keeps downloading
enum StreamState { STREAM_NONE, STREAM_PREPARING, STREAM_READY, STREAM_FAILED, STREAM_UNSUPPORTED };
bool isFetching(const std::string& remotePath);          // being downloaded right now (or waiting for its turn)
void requestStream(const std::string& remotePath);
StreamState streamState(const std::string& remotePath, std::string& localPath, std::string& error);
SDL_RWops* openStreamRW(const std::string& remotePath);  // the player reads the song through this (valid once STREAM_READY)
bool streamComplete(const std::string& remotePath);      // everything has arrived
uint64_t streamTotal(const std::string& remotePath);     // size of the song in bytes
float streamProgress(const std::string& remotePath);     // 0..1
std::vector<uint8_t> streamHead(const std::string& remotePath, size_t maxBytes); // the first bytes of the song
void abortStreams();                                      // stop waiting for data (before the music is freed / on exit)

// ---- the cache is temporary: only the song playing and the next one are kept, everything else is deleted
void keepOnly(const std::vector<std::string>& keepRemotePaths);
void clearCache();   // delete everything
void cleanIncomplete(); // delete only half-finished downloads (used when the cache is kept between runs)
void setKeepCache(bool keep); // true: the downloaded songs stay on the SD card when the app exits
bool keepCache();

// ---- messages for the UI (connection problems...)
bool takeError(std::string& message);

// ---- a small log (sdmc:/switch/shaderfun/smb.log, restarted every run) to help find problems
void trace(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void shutdown();

} // namespace smbnet
