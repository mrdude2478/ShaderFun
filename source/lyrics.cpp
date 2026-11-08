#include "lyrics.h"
#include "tags.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace lyrics {
namespace {

const size_t MAX_FILE = 256 * 1024;

bool readFile(const std::string& path, std::string& out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	const long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0 || (size_t)size > MAX_FILE) { fclose(f); return false; }
	out.resize((size_t)size);
	const size_t got = fread(&out[0], 1, (size_t)size, f);
	fclose(f);
	out.resize(got);
	return got > 0;
}

void appendUtf8(std::string& s, uint32_t cp) {
	if (cp < 0x80) s += (char)cp;
	else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
	else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
	else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
}

std::string utf16ToUtf8(const std::string& in, size_t start, bool bigEndian) {
	std::string out;
	for (size_t i = start; i + 1 < in.size(); i += 2) {
		const uint8_t a = (uint8_t)in[i], b = (uint8_t)in[i + 1];
		uint32_t u = bigEndian ? (uint32_t)(a << 8 | b) : (uint32_t)(b << 8 | a);
		if (u >= 0xD800 && u < 0xDC00 && i + 3 < in.size()) { // surrogate pair
			const uint8_t c = (uint8_t)in[i + 2], d = (uint8_t)in[i + 3];
			const uint32_t lo = bigEndian ? (uint32_t)(c << 8 | d) : (uint32_t)(d << 8 | c);
			if (lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
		}
		appendUtf8(out, u);
	}
	return out;
}

bool validUtf8(const std::string& s) {
	size_t i = 0;
	while (i < s.size()) {
		const uint8_t c = (uint8_t)s[i];
		size_t n = 0;
		if (c < 0x80) n = 0;
		else if ((c & 0xE0) == 0xC0) n = 1;
		else if ((c & 0xF0) == 0xE0) n = 2;
		else if ((c & 0xF8) == 0xF0) n = 3;
		else return false;
		for (size_t k = 1; k <= n; k++) if (i + k >= s.size() || ((uint8_t)s[i + k] & 0xC0) != 0x80) return false;
		i += n + 1;
	}
	return true;
}

std::string latin1ToUtf8(const std::string& in) {
	std::string out;
	for (unsigned char c : in) appendUtf8(out, c);
	return out;
}

// File bytes -> UTF-8 (byte order marks respected; Latin-1 if it is not valid UTF-8)
std::string toUtf8(const std::string& raw) {
	if (raw.size() >= 3 && (uint8_t)raw[0] == 0xEF && (uint8_t)raw[1] == 0xBB && (uint8_t)raw[2] == 0xBF) return raw.substr(3);
	if (raw.size() >= 2 && (uint8_t)raw[0] == 0xFF && (uint8_t)raw[1] == 0xFE) return utf16ToUtf8(raw, 2, false);
	if (raw.size() >= 2 && (uint8_t)raw[0] == 0xFE && (uint8_t)raw[1] == 0xFF) return utf16ToUtf8(raw, 2, true);
	return validUtf8(raw) ? raw : latin1ToUtf8(raw);
}

std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
	return s.substr(a, b - a);
}

// "[mm:ss]", "[mm:ss.xx]" or "[mm:ss:xx]" starting at s[pos] == '['. On success pos moves past the ']'.
bool parseTimestamp(const std::string& s, size_t& pos, double& out) {
	size_t i = pos + 1;
	if (i >= s.size() || !isdigit((unsigned char)s[i])) return false;
	int mm = 0;
	while (i < s.size() && isdigit((unsigned char)s[i])) mm = mm * 10 + (s[i++] - '0');
	if (i >= s.size() || s[i] != ':') return false;
	i++;
	if (i >= s.size() || !isdigit((unsigned char)s[i])) return false;
	int ss = 0;
	while (i < s.size() && isdigit((unsigned char)s[i])) ss = ss * 10 + (s[i++] - '0');
	double frac = 0.0;
	if (i < s.size() && (s[i] == '.' || s[i] == ':')) {
		i++;
		double scale = 0.1;
		while (i < s.size() && isdigit((unsigned char)s[i])) { frac += (s[i++] - '0') * scale; scale /= 10.0; }
	}
	if (i >= s.size() || s[i] != ']') return false;
	out = mm * 60.0 + ss + frac;
	pos = i + 1;
	return true;
}

// removes enhanced-LRC word stamps such as <00:12.50> and trims
std::string cleanText(const std::string& in) {
	std::string out;
	for (size_t i = 0; i < in.size(); i++) {
		if (in[i] == '<' && i + 1 < in.size() && isdigit((unsigned char)in[i + 1])) {
			const size_t close = in.find('>', i);
			if (close != std::string::npos && close - i <= 12) { i = close; continue; }
		}
		out += in[i];
	}
	return trim(out);
}

void parseText(const std::string& text, Lyrics& L) {
	std::vector<Line> timed;
	std::vector<std::string> allTexts;
	double offsetMs = 0.0;
	// false: a line without a marker is shown in the normal colours.
	// true: it keeps the previous singer until the next marker (the behaviour described on the LRC Wikipedia page).
	const bool kSingerSticks = true;
	char curSinger = 0;
	bool sawSinger = false;

	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find_first_of("\r\n", start);
		if (end == std::string::npos) end = text.size();
		const std::string line = text.substr(start, end - start);
		start = end + 1;
		if (end < text.size() && text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n') start = end + 2;

		size_t pos = 0;
		while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
		std::vector<double> stamps;
		double t;
		while (pos < line.size() && line[pos] == '[' && parseTimestamp(line, pos, t)) stamps.push_back(t);

		if (stamps.empty()) {
			// [ar:Artist], [ti:Title], [offset:+250] ... are information, not lyrics
			if (pos < line.size() && line[pos] == '[') {
				const size_t colon = line.find(':', pos), close = line.find(']', pos);
				if (colon != std::string::npos && close != std::string::npos && colon < close) {
					std::string key = line.substr(pos + 1, colon - pos - 1);
					for (char& c : key) c = (char)tolower((unsigned char)c);
					if (key == "offset") offsetMs = atof(line.substr(colon + 1, close - colon - 1).c_str());
					const std::string value = trim(line.substr(colon + 1, close - colon - 1));
					if (key == "ti" && L.title.empty()) L.title = value;
					if (key == "ar" && L.artist.empty()) L.artist = value;
					if (key == "al" && L.album.empty()) L.album = value;
					bool alpha = !key.empty();
					for (char c : key) if (!isalpha((unsigned char)c)) alpha = false;
					if (alpha && key.size() <= 8) continue;
				}
			}
			allTexts.push_back(cleanText(line));
			continue;
		}
		std::string body = cleanText(line.substr(pos));
		// "M:" / "F:" / "D:" at the start of a line = who sings it; the following lines keep that singer
		char lineSinger = kSingerSticks ? curSinger : 0;
		if (body.size() >= 2 && (body[0] == 'M' || body[0] == 'F' || body[0] == 'D') && body[1] == ':') {
			lineSinger = body[0];
			if (kSingerSticks) curSinger = body[0];
			sawSinger = true;
			body = trim(body.substr(2));
		}
		allTexts.push_back(body);
		for (double st : stamps) { Line l; l.time = st; l.text = body; l.singer = lineSinger; timed.push_back(l); }
	}

	if (timed.size() >= 2) {
		std::stable_sort(timed.begin(), timed.end(), [](const Line& a, const Line& b) { return a.time < b.time; });
		for (Line& l : timed) l.time = std::max(0.0, l.time - offsetMs / 1000.0); // positive offset = lyrics appear sooner
		bool any = false;
		for (const Line& l : timed) if (!l.text.empty()) { any = true; break; }
		if (any) { L.found = true; L.timed = true; L.hasSingers = sawSinger; L.lines = timed; return; }
		return;
	}

	// no timestamps: keep it as plain text (single blank lines only, none at the ends)
	std::vector<std::string> plain;
	for (const std::string& s : allTexts) {
		if (s.empty() && (plain.empty() || plain.back().empty())) continue;
		plain.push_back(s);
	}
	while (!plain.empty() && plain.back().empty()) plain.pop_back();
	if (!plain.empty()) { L.found = true; L.timed = false; L.plain = plain; }
}

} // namespace

bool load(const std::string& trackPath, Lyrics& out) {
	out = Lyrics();
	if (trackPath.empty()) return false;

	// 1. a .lrc file with the same name as the song
	const size_t dot = trackPath.find_last_of('.');
	const size_t slash = trackPath.find_last_of("/\\");
	if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
		const std::string base = trackPath.substr(0, dot);
		static const char* const exts[] = { ".lrc", ".LRC", ".Lrc" };
		for (const char* ext : exts) {
			std::string raw;
			if (!readFile(base + ext, raw)) continue;
			parseText(toUtf8(raw), out);
			if (out.found) { out.source = "lrc file"; return true; }
			out = Lyrics();
		}
	}

	// 2. lyrics embedded in the song's tags (ID3 USLT, Vorbis LYRICS)
	tags::TrackTags t;
	if (tags::read(trackPath, t, 0.0, true, true) && !t.lyrics.empty()) {
		parseText(toUtf8(t.lyrics), out);
		if (out.found) { out.source = "embedded tag"; return true; }
	}
	out = Lyrics();
	return false;
}

int lineAt(const Lyrics& l, double seconds) {
	if (!l.timed || l.lines.empty()) return -1;
	const auto it = std::upper_bound(l.lines.begin(), l.lines.end(), seconds,
		[](double s, const Line& ln) { return s < ln.time; });
	return (int)(it - l.lines.begin()) - 1;
}

} // namespace lyrics
