#pragma once
// Music library index for ShaderFun: reads the tags of every track in the music folder once (in a background
// thread), caches them on the SD card, and groups them into albums / artists / genres for the library menu.
// Also reads and writes simple M3U playlists.

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

namespace library {

struct Track {
	std::string path;
	std::string title, artist, album, albumArtist, genre, year;
	int trackNo = 0;
	int discNo = 0;
	uint64_t size = 0;  // together with mtime: detects changed files so only those are re-read
	int64_t mtime = 0;
};

// An album, an artist or a genre
struct Group {
	std::string name;           // display name
	std::string sub;            // albums: the album artist
	std::vector<int> tracks;    // indices into Snapshot::tracks, already in play order
};

// An immutable view of the library. New ones are published as the background scan makes progress.
struct Snapshot {
	std::vector<Track> tracks;  // sorted by artist, album, disc, track
	std::vector<Group> albums, artists, genres;
};

struct Playlist {
	std::string name;
	std::string path;
};

// ---- lifecycle
void init(const char* cachePath);
void shutdown();                                // stops the scan thread
void startScan(const std::string& musicRoot);   // (re)scans in the background; the cache makes repeats fast

// ---- state
bool scanning();
int scanDone();
int scanTotal();
uint32_t version();                             // increases whenever a new snapshot is published
std::shared_ptr<const Snapshot> snapshot();

// ---- helpers for the menu
std::string displayTitle(const Track& t);                                      // tag title, else the file name
std::string albumDisplayName(const Track& t);                                  // tag album, else the folder name
std::vector<Group> groupAlbums(const Snapshot& s, const std::vector<int>& trackIdx); // albums within a subset of tracks

// ---- playlists (.m3u / .m3u8)
std::string playlistsDir();
std::vector<Playlist> listPlaylists(const std::string& musicRoot);
std::vector<std::string> loadPlaylist(const std::string& path);                // only files that exist, in order
bool savePlaylist(const std::string& path, const std::vector<std::string>& tracks);

} // namespace library
