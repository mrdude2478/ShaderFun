#pragma once
// Online lyrics from lrclib.net (free, no account). Used when a song has no .lrc file and no embedded lyrics.
// The lookup runs on a background thread; found lyrics are kept in sdmc:/switch/shaderfun/lyrics/ so a song is only asked about once.

#include <string>

namespace lrclib {

struct Result {
	std::string path;      // the song the lookup was for
	bool found = false;    // lyrics arrived (in 'text')
	bool timed = false;    // 'text' has [mm:ss.xx] timestamps
	bool offline = false;  // the server could not be reached
	std::string text;      // LRC text (synced) or plain lyrics
	std::string error;     // a message for the user when nothing was found
};

// Lyrics found online earlier for this song (looked up by artist + title from its tags). Cheap: reads one small file.
bool cachedForSong(const std::string& songPath, std::string& text);
std::string cacheFileForSong(const std::string& songPath);   // the saved .lrc file of that song in the lyrics folder ("" if there is none)

// Starts a lookup (the newest request replaces an older one that has not started yet). durationSeconds helps pick the right version.
void request(const std::string& songPath, double durationSeconds);

// true once when a lookup has finished
bool poll(Result& out);

void shutdown();

} // namespace lrclib
