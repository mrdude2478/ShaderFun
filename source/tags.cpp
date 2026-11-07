#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <algorithm>

namespace tags {

namespace {

const size_t MAX_TAG_BYTES = 48u * 1024u * 1024u; // refuse absurd sizes (corrupt files)
const size_t MAX_COVER_BYTES = 24u * 1024u * 1024u;

// ------------------------------------------------------------------ file access
struct Reader {
	FILE* f = nullptr;
	uint64_t size = 0;

	bool open(const std::string& path) {
		f = fopen(path.c_str(), "rb");
		if (!f) return false;
		fseek(f, 0, SEEK_END);
		long s = ftell(f);
		size = s > 0 ? (uint64_t)s : 0;
		fseek(f, 0, SEEK_SET);
		return true;
	}
	~Reader() { if (f) fclose(f); }

	bool read(uint64_t off, void* buf, size_t n) {
		if (off + n > size) return false;
		if (fseek(f, (long)off, SEEK_SET) != 0) return false;
		return fread(buf, 1, n, f) == n;
	}
	bool readVec(uint64_t off, size_t n, std::vector<uint8_t>& v) {
		if (n > MAX_TAG_BYTES || off + n > size) return false;
		v.resize(n);
		return n == 0 || read(off, v.data(), n);
	}
};

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
uint32_t le32(const uint8_t* p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[1] << 8 | p[0]); }
uint32_t syncsafe(const uint8_t* p) { return (uint32_t)(p[0] & 0x7F) << 21 | (uint32_t)(p[1] & 0x7F) << 14 | (uint32_t)(p[2] & 0x7F) << 7 | (p[3] & 0x7F); }

// ------------------------------------------------------------------ text conversion
void appendUtf8(std::string& s, uint32_t cp) {
	if (cp < 0x80) s += (char)cp;
	else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
	else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
	else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
}

std::string latin1ToUtf8(const uint8_t* p, size_t n) {
	std::string s;
	for (size_t i = 0; i < n; i++) appendUtf8(s, p[i]);
	return s;
}

std::string utf16ToUtf8(const uint8_t* p, size_t n, bool bigEndian) {
	std::string s;
	size_t i = 0;
	if (n >= 2) { // BOM
		if (p[0] == 0xFF && p[1] == 0xFE) { bigEndian = false; i = 2; }
		else if (p[0] == 0xFE && p[1] == 0xFF) { bigEndian = true; i = 2; }
	}
	auto unit = [&](size_t k) -> uint32_t { return bigEndian ? (uint32_t)(p[k] << 8 | p[k + 1]) : (uint32_t)(p[k + 1] << 8 | p[k]); };
	for (; i + 1 < n; i += 2) {
		uint32_t u = unit(i);
		if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) {
			uint32_t lo = unit(i + 2);
			if (lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
		}
		appendUtf8(s, u);
	}
	return s;
}

// Keeps valid UTF-8 as is; anything that isn't valid UTF-8 is treated as Latin-1.
std::string sanitizeUtf8(const uint8_t* p, size_t n) {
	size_t i = 0;
	bool ok = true;
	while (i < n && ok) {
		uint8_t c = p[i];
		int len = c < 0x80 ? 1 : (c >= 0xC2 && c < 0xE0) ? 2 : (c >= 0xE0 && c < 0xF0) ? 3 : (c >= 0xF0 && c < 0xF5) ? 4 : 0;
		if (len == 0 || i + len > n) { ok = false; break; }
		for (int k = 1; k < len; k++) if ((p[i + k] & 0xC0) != 0x80) ok = false;
		i += len;
	}
	return ok ? std::string((const char*)p, n) : latin1ToUtf8(p, n);
}

std::string trim(std::string s) {
	while (!s.empty() && (s.back() == '\0' || s.back() == ' ' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
	size_t a = 0;
	while (a < s.size() && (s[a] == ' ' || s[a] == '\0')) a++;
	return s.substr(a);
}

// ID3 text encodings: 0 Latin-1, 1 UTF-16 (BOM), 2 UTF-16BE, 3 UTF-8
std::string decodeId3(const uint8_t* p, size_t n, int enc) {
	switch (enc) {
	case 0: return latin1ToUtf8(p, n);
	case 1: return utf16ToUtf8(p, n, false);
	case 2: return utf16ToUtf8(p, n, true);
	default: return sanitizeUtf8(p, n);
	}
}

// Finds the string terminator at or after 'from'. Returns the index of the terminator (or n) and its width.
size_t findTerminator(const uint8_t* p, size_t n, size_t from, int enc, size_t& width) {
	if (enc == 1 || enc == 2) {
		width = 2;
		for (size_t i = from; i + 1 < n; i += 2) if (p[i] == 0 && p[i + 1] == 0) return i;
		return n;
	}
	width = 1;
	for (size_t i = from; i < n; i++) if (p[i] == 0) return i;
	return n;
}

// All values of a text frame (v2.4 allows several, separated by terminators), joined with ", "
std::string textFrameValue(const uint8_t* d, size_t n) {
	if (n < 1) return "";
	const int enc = d[0];
	std::string out;
	size_t pos = 1;
	while (pos < n) {
		size_t w;
		size_t end = findTerminator(d, n, pos, enc, w);
		std::string v = trim(decodeId3(d + pos, end - pos, enc));
		if (!v.empty()) out += (out.empty() ? "" : ", ") + v;
		if (end >= n) break;
		pos = end + w;
	}
	return out;
}

// ------------------------------------------------------------------ genres
const char* const GENRES[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave", "Showtunes",
	"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock"
};
const int GENRE_COUNT = (int)(sizeof(GENRES) / sizeof(GENRES[0]));

std::string genreFromId3(const std::string& raw) {
	// "(13)", "(13)Pop", "13" or plain text
	std::string s = trim(raw);
	if (s.empty()) return s;
	size_t i = 0;
	std::string rest = s;
	int num = -1;
	if (s[0] == '(') {
		size_t close = s.find(')');
		if (close != std::string::npos && close > 1) {
			bool digits = true;
			for (size_t k = 1; k < close; k++) if (!isdigit((unsigned char)s[k])) digits = false;
			if (digits) { num = atoi(s.substr(1, close - 1).c_str()); rest = trim(s.substr(close + 1)); }
		}
	}
	else {
		bool digits = true;
		for (i = 0; i < s.size(); i++) if (!isdigit((unsigned char)s[i])) digits = false;
		if (digits) { num = atoi(s.c_str()); rest.clear(); }
	}
	if (!rest.empty()) return rest;
	if (num >= 0 && num < GENRE_COUNT) return GENRES[num];
	return num >= 0 ? "Genre " + std::to_string(num) : s;
}

// ------------------------------------------------------------------ base64
std::string b64decode(const std::string& in, std::vector<uint8_t>& out) {
	static int8_t T[256];
	static bool init = false;
	if (!init) {
		memset(T, -1, sizeof(T));
		const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		for (int i = 0; i < 64; i++) T[(unsigned char)a[i]] = (int8_t)i;
		init = true;
	}
	out.clear();
	uint32_t acc = 0;
	int bits = 0;
	for (unsigned char c : in) {
		if (c == '=') break;
		int v = T[c];
		if (v < 0) continue;
		acc = (acc << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((acc >> bits) & 0xFF)); }
	}
	return "";
}

// ------------------------------------------------------------------ small helpers
void setIfEmpty(std::string& dst, const std::string& v) { if (dst.empty() && !trim(v).empty()) dst = trim(v); }

void addTagType(TrackTags& t, const std::string& s) { t.tagType += (t.tagType.empty() ? "" : " + ") + s; }

std::string upper(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)toupper(c); });
	return s;
}

// ------------------------------------------------------------------ ID3v2
void deunsync(std::vector<uint8_t>& v) {
	std::vector<uint8_t> o;
	o.reserve(v.size());
	for (size_t i = 0; i < v.size(); i++) {
		o.push_back(v[i]);
		if (v[i] == 0xFF && i + 1 < v.size() && v[i + 1] == 0x00) i++;
	}
	v.swap(o);
}

std::string normalizeFrameId(const std::string& id) {
	static const char* const map[][2] = {
		{"TT2","TIT2"}, {"TP1","TPE1"}, {"TAL","TALB"}, {"TP2","TPE2"}, {"TRK","TRCK"}, {"TPA","TPOS"},
		{"TYE","TYER"}, {"TCO","TCON"}, {"TCM","TCOM"}, {"COM","COMM"}, {"PIC","APIC"} };
	for (auto& m : map) if (id == m[0]) return m[1];
	return id;
}

void handleApic(const uint8_t* d, size_t n, bool v22, TrackTags& t, int& coverType) {
	if (n < 4) return;
	const int enc = d[0];
	size_t pos = 1;
	std::string mime;
	if (v22) {
		mime = (d[1] == 'P' && d[2] == 'N' && d[3] == 'G') ? "image/png" : "image/jpeg";
		pos = 4;
	}
	else {
		size_t w;
		size_t end = findTerminator(d, n, 1, 0, w);
		mime = std::string((const char*)d + 1, end - 1);
		pos = end + 1;
	}
	if (pos >= n) return;
	const int picType = d[pos++];
	size_t w;
	size_t dend = findTerminator(d, n, pos, enc, w);
	pos = dend + w;
	if (pos >= n || n - pos > MAX_COVER_BYTES) return;
	// prefer the front cover (type 3), otherwise keep the first picture
	if (t.cover.empty() || (picType == 3 && coverType != 3)) {
		t.cover.assign(d + pos, d + n);
		t.coverMime = mime;
		coverType = picType;
	}
}

void handleComm(const uint8_t* d, size_t n, TrackTags& t) {
	if (n < 5) return;
	const int enc = d[0];
	size_t w;
	size_t dend = findTerminator(d, n, 4, enc, w);
	std::string desc = trim(decodeId3(d + 4, dend - 4, enc));
	size_t tpos = dend + w;
	if (tpos > n) return;
	std::string text = trim(decodeId3(d + tpos, n - tpos, enc));
	if (text.empty() || desc.compare(0, 4, "iTun") == 0) return;
	if (t.comment.empty() || desc.empty()) t.comment = text;
}

void handleTxxx(const uint8_t* d, size_t n, TrackTags& t) {
	if (n < 3) return;
	const int enc = d[0];
	size_t w;
	size_t dend = findTerminator(d, n, 1, enc, w);
	std::string desc = upper(trim(decodeId3(d + 1, dend - 1, enc)));
	size_t tpos = dend + w;
	if (tpos >= n) return;
	std::string val = trim(decodeId3(d + tpos, n - tpos, enc));
	if (val.empty()) return;
	if (desc == "COMMENT" || desc == "COMMENTS") setIfEmpty(t.comment, val);
	else if (desc == "ALBUMARTIST" || desc == "ALBUM ARTIST") setIfEmpty(t.albumArtist, val);
	else if (desc == "COMPOSER") setIfEmpty(t.composer, val);
	else if (desc == "TRACKTOTAL" || desc == "TOTALTRACKS") { if (!t.track.empty() && t.track.find('/') == std::string::npos) t.track += "/" + val; }
}

// Returns the total number of bytes taken by the tag (header + body + footer), 0 if there is none.
// Frames are read one at a time, and frames we don't need (including cover art when wantCover is false)
// are skipped without being read, so indexing a library doesn't pull megabytes of artwork off the SD card.
uint32_t parseId3v2(Reader& r, TrackTags& t, bool wantCover) {
	uint8_t h[10];
	if (!r.read(0, h, 10) || memcmp(h, "ID3", 3) != 0) return 0;
	const int ver = h[3];
	if (ver < 2 || ver > 4 || (h[6] & 0x80) || (h[7] & 0x80) || (h[8] & 0x80) || (h[9] & 0x80)) return 0;
	const uint32_t bodySize = syncsafe(h + 6);
	const bool footer = (ver == 4) && (h[5] & 0x10);
	const uint32_t total = 10 + bodySize + (footer ? 10 : 0);

	// Whole-tag unsynchronisation changes the byte layout, so that (rare) case loads the body and decodes it first
	const bool preload = (h[5] & 0x80) != 0;
	std::vector<uint8_t> body;
	if (preload) {
		if (!r.readVec(10, bodySize, body)) return total;
		deunsync(body);
	}
	const uint64_t bodyLen = preload ? body.size() : bodySize;
	auto fetch = [&](uint64_t off, size_t n, std::vector<uint8_t>& out) -> bool {
		if (off + n > bodyLen) return false;
		if (preload) { out.assign(body.begin() + off, body.begin() + off + n); return true; }
		return r.readVec(10 + off, n, out);
	};

	uint64_t pos = 0;
	if (h[5] & 0x40) { // extended header
		std::vector<uint8_t> eh;
		if (fetch(0, 4, eh)) {
			const uint32_t es = (ver == 4) ? syncsafe(eh.data()) : be32(eh.data()) + 4;
			pos = std::min<uint64_t>(es, bodyLen);
		}
	}

	static const char* const interesting[] = { "TIT2", "TPE1", "TALB", "TPE2", "TCOM", "TRCK", "TPOS", "TCON",
		"TYER", "TDRC", "TDRL", "COMM", "TXXX", "APIC" };
	const size_t hdr = (ver == 2) ? 6 : 10;
	int coverType = -1;
	std::string yearTyer, yearTdrc;
	bool any = false;
	std::vector<uint8_t> fh;

	while (pos + hdr <= bodyLen) {
		if (!fetch(pos, hdr, fh)) break;
		if (fh[0] == 0) break; // padding
		std::string id((const char*)fh.data(), ver == 2 ? 3 : 4);
		size_t size;
		uint8_t fl1 = 0;
		if (ver == 2) size = (size_t)fh[3] << 16 | (size_t)fh[4] << 8 | fh[5];
		else {
			size = (ver == 4) ? syncsafe(&fh[4]) : be32(&fh[4]);
			fl1 = fh[9];
		}
		pos += hdr;
		if (size > bodyLen - pos) break;
		const uint64_t dataPos = pos;
		pos += size;

		id = normalizeFrameId(id);
		bool want = false;
		for (const char* f : interesting) if (id == f) want = true;
		if (id == "APIC" && !wantCover) want = false;
		if (!want || size == 0) continue;

		std::vector<uint8_t> data;
		if (!fetch(dataPos, size, data)) continue;

		bool skip = false;
		if (ver == 3) {
			if (fl1 & 0xC0) skip = true;                 // compressed / encrypted
			else if ((fl1 & 0x20) && !data.empty()) data.erase(data.begin()); // grouping byte
		}
		else if (ver == 4) {
			if (fl1 & 0x0C) skip = true;                 // compressed / encrypted
			else {
				if ((fl1 & 0x40) && !data.empty()) data.erase(data.begin());                     // grouping
				if ((fl1 & 0x01) && data.size() >= 4) data.erase(data.begin(), data.begin() + 4); // data length indicator
				if (fl1 & 0x02) deunsync(data);                                                   // per-frame unsync
			}
		}
		if (skip || data.empty()) continue;

		const uint8_t* d = data.data();
		const size_t n = data.size();
		if (id == "TIT2") { setIfEmpty(t.title, textFrameValue(d, n)); any = true; }
		else if (id == "TPE1") { setIfEmpty(t.artist, textFrameValue(d, n)); any = true; }
		else if (id == "TALB") { setIfEmpty(t.album, textFrameValue(d, n)); any = true; }
		else if (id == "TPE2") { setIfEmpty(t.albumArtist, textFrameValue(d, n)); any = true; }
		else if (id == "TCOM") { setIfEmpty(t.composer, textFrameValue(d, n)); any = true; }
		else if (id == "TRCK") { setIfEmpty(t.track, textFrameValue(d, n)); any = true; }
		else if (id == "TPOS") { setIfEmpty(t.disc, textFrameValue(d, n)); any = true; }
		else if (id == "TCON") { setIfEmpty(t.genre, genreFromId3(textFrameValue(d, n))); any = true; }
		else if (id == "TYER") { setIfEmpty(yearTyer, textFrameValue(d, n)); any = true; }
		else if (id == "TDRC" || id == "TDRL") { setIfEmpty(yearTdrc, textFrameValue(d, n)); any = true; }
		else if (id == "COMM") { handleComm(d, n, t); any = true; }
		else if (id == "TXXX") { handleTxxx(d, n, t); }
		else if (id == "APIC") { handleApic(d, n, ver == 2, t, coverType); any = true; }
	}
	if (!yearTdrc.empty()) setIfEmpty(t.year, yearTdrc.substr(0, 10));
	if (!yearTyer.empty()) setIfEmpty(t.year, yearTyer);
	if (any) {
		t.found = true;
		addTagType(t, "ID3v2." + std::to_string(ver));
	}
	return total;
}

void parseId3v1(Reader& r, TrackTags& t) {
	if (r.size < 128) return;
	uint8_t b[128];
	if (!r.read(r.size - 128, b, 128) || memcmp(b, "TAG", 3) != 0) return;
	auto field = [&](int off, int len) { return trim(sanitizeUtf8(b + off, strnlen((const char*)b + off, len))); };
	setIfEmpty(t.title, field(3, 30));
	setIfEmpty(t.artist, field(33, 30));
	setIfEmpty(t.album, field(63, 30));
	setIfEmpty(t.year, field(93, 4));
	if (b[125] == 0 && b[126] != 0) { // ID3v1.1: track number in the last comment byte
		setIfEmpty(t.comment, field(97, 28));
		setIfEmpty(t.track, std::to_string((int)b[126]));
	}
	else {
		setIfEmpty(t.comment, field(97, 30));
	}
	if (t.genre.empty() && b[127] < GENRE_COUNT) t.genre = GENRES[b[127]];
	t.found = true;
	addTagType(t, "ID3v1");
}

// ------------------------------------------------------------------ MP3 frame header
void parseMpegHeader(Reader& r, uint64_t start, TrackTags& t) {
	std::vector<uint8_t> buf;
	size_t want = (size_t)std::min<uint64_t>(r.size - std::min(r.size, start), 70000);
	if (want < 8 || !r.readVec(start, want, buf)) return;

	static const int br[2][3][15] = { // [MPEG1=0 / MPEG2+=1][layer 1..3][index]
		{ {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
		  {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
		  {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320} },
		{ {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256},
		  {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
		  {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160} } };
	static const int sr[3][3] = { {44100,48000,32000}, {22050,24000,16000}, {11025,12000,8000} };

	struct Hdr { int ver, layer, bitrate, rate, mode, frameLen; bool ok; };
	auto parse = [&](size_t i) -> Hdr {
		Hdr h = { 0, 0, 0, 0, 0, 0, false };
		if (i + 4 > buf.size() || buf[i] != 0xFF || (buf[i + 1] & 0xE0) != 0xE0) return h;
		const int vbits = (buf[i + 1] >> 3) & 3, lbits = (buf[i + 1] >> 1) & 3;
		const int bi = buf[i + 2] >> 4, si = (buf[i + 2] >> 2) & 3;
		if (vbits == 1 || lbits == 0 || bi == 0 || bi == 15 || si == 3) return h;
		h.ver = (vbits == 3) ? 1 : (vbits == 2) ? 2 : 3; // 1, 2, 2.5 (3 = 2.5)
		h.layer = 4 - lbits;
		h.bitrate = br[h.ver == 1 ? 0 : 1][h.layer - 1][bi];
		h.rate = sr[h.ver - 1][si];
		h.mode = buf[i + 3] >> 6;
		const int pad = (buf[i + 2] >> 1) & 1;
		if (h.layer == 1) h.frameLen = (12 * h.bitrate * 1000 / h.rate + pad) * 4;
		else if (h.layer == 3 && h.ver != 1) h.frameLen = 72 * h.bitrate * 1000 / h.rate + pad;
		else h.frameLen = 144 * h.bitrate * 1000 / h.rate + pad;
		h.ok = h.frameLen > 4;
		return h;
	};

	for (size_t i = 0; i + 4 < buf.size(); i++) {
		Hdr h = parse(i);
		if (!h.ok) continue;
		// confirm with the following frame to avoid false sync hits
		const size_t next = i + (size_t)h.frameLen;
		if (next + 4 <= buf.size() && !parse(next).ok) continue;

		t.sampleRate = h.rate;
		t.channels = (h.mode == 3) ? 1 : 2;
		t.bitrateKbps = h.bitrate;
		static const char* const layerName[] = { "", "I", "II", "III" };
		const char* verName = (h.ver == 1) ? "1" : (h.ver == 2) ? "2" : "2.5";
		t.format = std::string("MPEG-") + verName + " Layer " + layerName[h.layer]; // Layer III is what everyone calls MP3

		// Xing / Info / VBRI headers
		const size_t side = (h.ver == 1) ? (h.mode == 3 ? 17 : 32) : (h.mode == 3 ? 9 : 17);
		const size_t xo = i + 4 + side;
		if (xo + 4 <= buf.size() && memcmp(&buf[xo], "Xing", 4) == 0) t.vbr = true;
		if (i + 36 + 4 <= buf.size() && memcmp(&buf[i + 36], "VBRI", 4) == 0) t.vbr = true;
		return;
	}
}

// ------------------------------------------------------------------ Vorbis comments (FLAC / Ogg)
bool parseFlacPicture(const uint8_t* d, size_t n, TrackTags& t, int& coverType) {
	if (n < 32) return false;
	const uint32_t type = be32(d);
	const uint32_t mlen = be32(d + 4);
	size_t pos = 8;
	if (mlen > n - pos) return false;
	std::string mime((const char*)d + pos, mlen);
	pos += mlen;
	if (pos + 4 > n) return false;
	const uint32_t dlen = be32(d + pos);
	pos += 4;
	if (dlen > n - pos) return false;
	pos += dlen;
	pos += 16; // width, height, depth, colours
	if (pos + 4 > n) return false;
	const uint32_t len = be32(d + pos);
	pos += 4;
	if (len > n - pos || len > MAX_COVER_BYTES) return false;
	if (t.cover.empty() || ((int)type == 3 && coverType != 3)) {
		t.cover.assign(d + pos, d + pos + len);
		t.coverMime = mime;
		coverType = (int)type;
	}
	return true;
}

void parseVorbisComments(const uint8_t* p, size_t n, TrackTags& t, bool wantCover) {
	if (n < 8) return;
	size_t pos = 0;
	const uint32_t vlen = le32(p);
	pos += 4;
	if (vlen > n - pos) return;
	pos += vlen;
	if (pos + 4 > n) return;
	const uint32_t count = le32(p + pos);
	pos += 4;

	std::string trackTotal, discTotal, date;
	int coverType = -1;
	auto add = [](std::string& dst, const std::string& v) { if (!v.empty()) dst += (dst.empty() ? "" : ", ") + v; };

	for (uint32_t i = 0; i < count && pos + 4 <= n; i++) {
		const uint32_t len = le32(p + pos);
		pos += 4;
		if (len > n - pos) break;
		std::string entry = sanitizeUtf8(p + pos, len);
		pos += len;
		size_t eq = entry.find('=');
		if (eq == std::string::npos) continue;
		std::string key = upper(entry.substr(0, eq));
		std::string val = trim(entry.substr(eq + 1));
		if (val.empty()) continue;

		if (key == "TITLE") add(t.title, val);
		else if (key == "ARTIST") add(t.artist, val);
		else if (key == "ALBUM") add(t.album, val);
		else if (key == "ALBUMARTIST" || key == "ALBUM ARTIST") add(t.albumArtist, val);
		else if (key == "COMPOSER") add(t.composer, val);
		else if (key == "GENRE") add(t.genre, val);
		else if (key == "DATE" || key == "YEAR") { if (date.empty()) date = val; }
		else if (key == "TRACKNUMBER") setIfEmpty(t.track, val);
		else if (key == "TRACKTOTAL" || key == "TOTALTRACKS") trackTotal = val;
		else if (key == "DISCNUMBER") setIfEmpty(t.disc, val);
		else if (key == "DISCTOTAL" || key == "TOTALDISCS") discTotal = val;
		else if (key == "COMMENT" || key == "DESCRIPTION") setIfEmpty(t.comment, val);
		else if (key == "METADATA_BLOCK_PICTURE" && wantCover) {
			std::vector<uint8_t> pic;
			b64decode(val, pic);
			parseFlacPicture(pic.data(), pic.size(), t, coverType);
		}
	}
	if (!date.empty()) t.year = date.substr(0, 10);
	if (!trackTotal.empty() && t.track.find('/') == std::string::npos && !t.track.empty()) t.track += "/" + trackTotal;
	if (!discTotal.empty() && t.disc.find('/') == std::string::npos && !t.disc.empty()) t.disc += "/" + discTotal;
	t.found = true;
}

// ------------------------------------------------------------------ FLAC
void parseFlac(Reader& r, TrackTags& t, bool wantCover) {
	t.format = "FLAC";
	uint64_t pos = 4;
	int coverType = -1;
	for (int blocks = 0; blocks < 64; blocks++) {
		uint8_t h[4];
		if (!r.read(pos, h, 4)) return;
		const bool last = h[0] & 0x80;
		const int type = h[0] & 0x7F;
		const uint32_t len = (uint32_t)h[1] << 16 | (uint32_t)h[2] << 8 | h[3];
		pos += 4;
		if (type == 0 && len >= 18) { // STREAMINFO
			uint8_t si[18];
			if (r.read(pos, si, 18)) {
				t.sampleRate = (si[10] << 12) | (si[11] << 4) | (si[12] >> 4);
				t.channels = ((si[12] >> 1) & 7) + 1;
				t.bitsPerSample = (((si[12] & 1) << 4) | (si[13] >> 4)) + 1;
			}
		}
		else if (type == 4) { // VORBIS_COMMENT
			std::vector<uint8_t> v;
			if (r.readVec(pos, len, v)) { parseVorbisComments(v.data(), v.size(), t, wantCover); addTagType(t, "Vorbis comments"); }
		}
		else if (type == 6 && wantCover) { // PICTURE
			std::vector<uint8_t> v;
			if (len <= MAX_COVER_BYTES + 4096 && r.readVec(pos, len, v)) { parseFlacPicture(v.data(), v.size(), t, coverType); t.found = true; }
		}
		pos += len;
		if (last) break;
	}
}

// ------------------------------------------------------------------ Ogg (Vorbis / Opus)
void parseOgg(Reader& r, TrackTags& t, bool wantCover) {
	t.format = "Ogg";
	std::vector<uint8_t> packet;
	std::vector<std::vector<uint8_t>> packets;
	uint64_t pos = 0;
	for (int pages = 0; pages < 4096 && packets.size() < 2; pages++) {
		uint8_t h[27];
		if (!r.read(pos, h, 27) || memcmp(h, "OggS", 4) != 0) break;
		const int nseg = h[26];
		uint8_t lacing[255];
		if (!r.read(pos + 27, lacing, nseg)) break;
		uint64_t dpos = pos + 27 + nseg;
		for (int s = 0; s < nseg && packets.size() < 2; s++) {
			const size_t before = packet.size();
			if (before + lacing[s] > MAX_TAG_BYTES) return;
			packet.resize(before + lacing[s]);
			if (lacing[s] && !r.read(dpos, &packet[before], lacing[s])) return;
			dpos += lacing[s];
			if (lacing[s] < 255) { packets.push_back(std::move(packet)); packet.clear(); }
		}
		pos = dpos;
	}
	if (packets.empty()) return;

	const std::vector<uint8_t>& id = packets[0];
	if (id.size() >= 30 && id[0] == 1 && memcmp(&id[1], "vorbis", 6) == 0) {
		t.format = "Ogg Vorbis";
		t.channels = id[11];
		t.sampleRate = (int)le32(&id[12]);
		const int32_t nominal = (int32_t)le32(&id[20]);
		if (nominal > 0) t.bitrateKbps = (nominal + 500) / 1000;
		if (packets.size() > 1 && packets[1].size() > 7 && packets[1][0] == 3 && memcmp(&packets[1][1], "vorbis", 6) == 0) {
			parseVorbisComments(&packets[1][7], packets[1].size() - 7, t, wantCover);
			addTagType(t, "Vorbis comments");
		}
	}
	else if (id.size() >= 19 && memcmp(id.data(), "OpusHead", 8) == 0) {
		t.format = "Opus";
		t.channels = id[9];
		t.sampleRate = (int)le32(&id[12]);
		if (packets.size() > 1 && packets[1].size() > 8 && memcmp(packets[1].data(), "OpusTags", 8) == 0) {
			parseVorbisComments(&packets[1][8], packets[1].size() - 8, t, wantCover);
			addTagType(t, "Opus tags");
		}
	}
}

// ------------------------------------------------------------------ WAV
void parseWav(Reader& r, TrackTags& t) {
	t.format = "WAV";
	uint64_t pos = 12;
	for (int chunks = 0; chunks < 64; chunks++) {
		uint8_t h[8];
		if (!r.read(pos, h, 8)) break;
		const uint32_t len = le32(h + 4);
		pos += 8;
		if (memcmp(h, "fmt ", 4) == 0 && len >= 16) {
			uint8_t f[16];
			if (r.read(pos, f, 16)) {
				const int tag = le16(f);
				t.channels = le16(f + 2);
				t.sampleRate = (int)le32(f + 4);
				t.bitrateKbps = (int)(le32(f + 8) * 8 / 1000);
				t.bitsPerSample = le16(f + 14);
				t.format = (tag == 1) ? "WAV - PCM" : (tag == 3) ? "WAV - float" : (tag == 0xFFFE) ? "WAV - extensible" : "WAV - compressed";
			}
		}
		else if (memcmp(h, "LIST", 4) == 0 && len >= 4 && len < 1u << 20) {
			std::vector<uint8_t> v;
			if (r.readVec(pos, len, v) && memcmp(v.data(), "INFO", 4) == 0) {
				size_t p = 4;
				while (p + 8 <= v.size()) {
					const uint32_t sl = le32(&v[p + 4]);
					if (sl > v.size() - p - 8) break;
					std::string id((const char*)&v[p], 4);
					std::string val = trim(sanitizeUtf8(&v[p + 8], strnlen((const char*)&v[p + 8], sl)));
					if (id == "INAM") setIfEmpty(t.title, val);
					else if (id == "IART") setIfEmpty(t.artist, val);
					else if (id == "IPRD") setIfEmpty(t.album, val);
					else if (id == "ICRD") setIfEmpty(t.year, val.substr(0, 10));
					else if (id == "IGNR") setIfEmpty(t.genre, val);
					else if (id == "ICMT") setIfEmpty(t.comment, val);
					else if (id == "ITRK") setIfEmpty(t.track, val);
					else if (id == "ICOP" || id == "IENG") {}
					p += 8 + sl + (sl & 1);
				}
				t.found = true;
				addTagType(t, "RIFF INFO");
			}
		}
		pos += len + (len & 1);
	}
}

// ------------------------------------------------------------------ tracker modules
std::string cstr(const uint8_t* p, size_t n) { return trim(sanitizeUtf8(p, strnlen((const char*)p, n))); }

bool parseTracker(Reader& r, const uint8_t* head, TrackTags& t, const std::string& ext) {
	if (memcmp(head, "Extended Module", 15) == 0) {
		uint8_t b[20];
		t.format = "XM tracker module";
		if (r.read(17, b, 20)) setIfEmpty(t.title, cstr(b, 20));
		uint8_t ch[2];
		if (r.read(68, ch, 2)) t.channels = le16(ch);
	}
	else if (memcmp(head, "IMPM", 4) == 0) {
		uint8_t b[26];
		t.format = "IT tracker module";
		if (r.read(4, b, 26)) setIfEmpty(t.title, cstr(b, 26));
	}
	else {
		uint8_t sig[4];
		if (r.read(44, sig, 4) && memcmp(sig, "SCRM", 4) == 0) {
			uint8_t b[28];
			t.format = "S3M tracker module";
			if (r.read(0, b, 28)) setIfEmpty(t.title, cstr(b, 28));
		}
		else if (ext == "mod" && r.read(1080, sig, 4)) {
			uint8_t b[20];
			t.format = "MOD tracker module";
			if (r.read(0, b, 20)) setIfEmpty(t.title, cstr(b, 20));
			if (memcmp(sig, "M.K.", 4) == 0 || memcmp(sig, "FLT4", 4) == 0 || memcmp(sig, "M!K!", 4) == 0) t.channels = 4;
			else if (memcmp(sig, "FLT8", 4) == 0 || memcmp(sig, "8CHN", 4) == 0) t.channels = 8;
			else if (isdigit(sig[0]) && memcmp(sig + 1, "CHN", 3) == 0) t.channels = sig[0] - '0';
		}
		else {
			return false;
		}
	}
	if (!t.title.empty()) { t.found = true; t.tagType = "Module header"; }
	return true;
}

std::string extensionOf(const std::string& path) {
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) return "";
	std::string e = path.substr(dot + 1);
	std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)tolower(c); });
	return e;
}

} // namespace

std::string base64Decode(const std::string& in, std::vector<uint8_t>& out) { return b64decode(in, out); }

bool read(const std::string& path, TrackTags& t, double durationSeconds, bool light) {
	const bool wantCover = !light;
	t = TrackTags();
	Reader r;
	if (!r.open(path)) return false;
	t.fileSize = r.size;

	uint8_t head[16] = { 0 };
	r.read(0, head, (size_t)std::min<uint64_t>(16, r.size));
	const std::string ext = extensionOf(path);

	uint64_t audioStart = 0;
	if (memcmp(head, "fLaC", 4) == 0) {
		parseFlac(r, t, wantCover);
	}
	else if (memcmp(head, "OggS", 4) == 0) {
		parseOgg(r, t, wantCover);
	}
	else if (memcmp(head, "RIFF", 4) == 0 && memcmp(head + 8, "WAVE", 4) == 0) {
		parseWav(r, t);
	}
	else if (memcmp(head, "FORM", 4) == 0 && (memcmp(head + 8, "AIFF", 4) == 0 || memcmp(head + 8, "AIFC", 4) == 0)) {
		t.format = "AIFF";
	}
	else if (memcmp(head, "MThd", 4) == 0) {
		t.format = "MIDI";
	}
	else if (parseTracker(r, head, t, ext)) {
		// done
	}
	else { // MP3 (with or without ID3v2)
		audioStart = parseId3v2(r, t, wantCover);
		parseId3v1(r, t);
		if (!light) parseMpegHeader(r, audioStart, t); // frame header probing is only needed for the info panel
		if (t.format.empty()) t.format = (ext == "mp3") ? "MP3" : ext.empty() ? "Audio" : ext;
		if (t.found && t.tagType.empty()) t.tagType = "ID3";
	}
	if (t.format.empty()) t.format = ext.empty() ? "Audio" : ext;

	// average bitrate for VBR / lossless / unknown
	const bool isModuleOrMidi = t.format.find("tracker") != std::string::npos || t.format == "MIDI";
	if (!isModuleOrMidi && durationSeconds > 1.0 && (t.vbr || t.bitrateKbps == 0 || t.format.compare(0, 4, "FLAC") == 0 || t.format.compare(0, 4, "Opus") == 0)) {
		const uint64_t audioBytes = t.fileSize > audioStart ? t.fileSize - audioStart : t.fileSize;
		t.bitrateKbps = (int)((double)audioBytes * 8.0 / durationSeconds / 1000.0 + 0.5);
	}
	return true;
}

} // namespace tags
