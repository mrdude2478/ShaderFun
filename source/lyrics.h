#pragma once
// Song lyrics for ShaderFun: reads a ".lrc" file next to the song (same name) or lyrics embedded in the file's tags.
// Timed lyrics ([mm:ss.xx] lines) can follow the music; plain lyrics are shown as a page of text.

#include <string>
#include <vector>

namespace lyrics {

struct Line {
	double time = 0.0;   // seconds (timed lyrics only)
	std::string text;    // UTF-8, may be empty (instrumental gap)
};

struct Lyrics {
	bool found = false;
	bool timed = false;                 // true: 'lines' has timestamps and follows the music
	std::string source;                 // "lrc file" or "embedded tag"
	std::vector<Line> lines;            // timed lyrics, sorted by time
	std::vector<std::string> plain;     // untimed lyrics, one entry per line
};

// Looks for "<song>.lrc" first, then lyrics inside the file's own tags. Returns true if lyrics were found.
bool load(const std::string& trackPath, Lyrics& out);

// Index of the line that should be showing at 'seconds' (-1 before the first line, or if not timed).
int lineAt(const Lyrics& l, double seconds);

} // namespace lyrics
