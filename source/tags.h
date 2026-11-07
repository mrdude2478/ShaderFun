#pragma once
// Reads metadata and basic technical info from audio files: ID3v2 (2.2/2.3/2.4) + ID3v1 for MP3,
// Vorbis comments for FLAC / Ogg Vorbis / Opus, RIFF INFO for WAV, song titles of tracker modules.
// Plain file I/O only (no SDL), so it works on any path the C library can open.

#include <stdint.h>
#include <string>
#include <vector>

namespace tags {

struct TrackTags {
	bool found = false;            // some metadata (not just file info) was found
	std::string format;            // e.g. "MP3 - MPEG-1 Layer III"
	std::string tagType;           // e.g. "ID3v2.3 + ID3v1"

	// text fields, UTF-8
	std::string title, artist, album, albumArtist, composer, genre, year, track, disc, comment;

	// technical info (0 = unknown)
	int sampleRate = 0;
	int channels = 0;
	int bitsPerSample = 0;
	int bitrateKbps = 0;
	bool vbr = false;
	uint64_t fileSize = 0;

	// embedded cover art (still encoded: JPEG/PNG/...)
	std::vector<uint8_t> cover;
	std::string coverMime;
};

// Returns false if the file can't be opened. durationSeconds (optional) is used to estimate the
// average bitrate of VBR / lossless files.
// light = true: text tags only (no cover art, no MPEG header probing). That is what the library index uses,
// and it avoids reading megabytes of artwork per file.
bool read(const std::string& path, TrackTags& out, double durationSeconds = 0.0, bool light = false);

// exposed for tests
std::string base64Decode(const std::string& in, std::vector<uint8_t>& out);

} // namespace tags
