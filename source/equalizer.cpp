#include "equalizer.h"

#include <atomic>
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equalizer {

namespace {

struct Coef { double b0, b1, b2, a1, a2; };   // normalised so a0 == 1

// Octave-spaced centre frequencies, like the classic 10-band graphic equalizer.
const double kFreq[BAND_COUNT] = { 31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };
const double kQ = 1.41;           // one octave wide per band
const char* const kNames[BAND_COUNT] = { "31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz" };
const char* const kShort[BAND_COUNT] = { "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };

const int PRESETS[PRESET_COUNT][BAND_COUNT] = {
	//   31  62 125 250 500  1k  2k  4k  8k 16k
	{    0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },   // Flat
	{    7,  6,  5,  3,  1,  0,  0,  0,  0,  0 },   // Bass boost
	{   -6, -5, -4, -2,  0,  0,  0,  0,  0,  0 },   // Less bass
	{    0,  0,  0,  0,  0,  1,  3,  5,  6,  7 },   // Treble boost
	{   -3, -2, -1,  1,  3,  4,  4,  3,  1,  0 },   // Vocal
	{    6,  5,  2,  0, -1, -1,  0,  2,  5,  6 },   // Loudness (a gentle smile)
	{    5,  4,  3,  1, -1, -1,  1,  3,  4,  5 },   // Rock
	{   -1,  2,  4,  5,  3,  0, -1, -1, -1, -1 },   // Pop
	{    4,  3,  1,  2, -2, -2,  0,  1,  3,  4 },   // Jazz
	{    3,  2,  1,  0, -1, -1,  0,  1,  2,  3 },   // Classical
	{    6,  5,  3,  0,  0, -2, -3, -3,  0,  0 },   // Dance
	{    5,  4,  1, -1, -2,  1,  0,  1,  4,  5 },   // Electronic
	{    6,  5,  2,  3, -1, -1,  1, -1,  2,  3 },   // Hip-hop
	{    4,  4,  3,  1,  1,  1,  2,  3,  3,  2 },   // Acoustic
	{   -6, -4, -1,  2,  3,  1,  0,  1,  2,  2 },   // Handheld speakers (no deep bass to waste, a lift where small speakers are weak)
};

const char* const kPresetNames[PRESET_COUNT] = {
	"Flat", "Bass boost", "Less bass", "Treble boost", "Vocal", "Loudness", "Rock", "Pop",
	"Jazz", "Classical", "Dance", "Electronic", "Hip-hop", "Acoustic", "Handheld speakers"
};

// ------------------------------------------------------------------ control side (main thread only)
int g_gainDb[BAND_COUNT] = {};
int g_rate = 44100;
bool g_ready = false;
bool g_bypassCtl = false;
double g_preampDb = 0.0;

// ------------------------------------------------------------------ shared between the threads
// A seqlock: the writer makes the counter odd, writes, makes it even again. The reader copies and checks the counter did not change.
std::atomic<uint32_t> g_seq{ 0 };
std::atomic<double> g_shared[BAND_COUNT][5];
std::atomic<double> g_sharedPreamp{ 1.0 };
std::atomic<uint32_t> g_sharedActive{ 0 };   // bit b set = band b is not flat
std::atomic<bool> g_bypass{ false };
std::atomic<bool> g_resetState{ false };

// ------------------------------------------------------------------ audio side (audio thread only)
Coef a_coef[BAND_COUNT];
double a_preamp = 1.0;
uint32_t a_active = 0;
uint32_t a_seq = 0xFFFFFFFFu;                 // never equal to a real counter: forces the first copy
double a_z1[2][BAND_COUNT], a_z2[2][BAND_COUNT];   // filter memory (transposed direct form II), owned by the audio thread
bool a_wasBypassed = true;

// One peaking filter (RBJ Audio EQ Cookbook).
Coef design(double freq, double gainDb) {
	const double A = pow(10.0, gainDb / 40.0);
	const double w0 = 2.0 * M_PI * freq / (double)g_rate;
	const double alpha = sin(w0) / (2.0 * kQ);
	const double cosw = cos(w0);
	const double a0 = 1.0 + alpha / A;
	Coef c;
	c.b0 = (1.0 + alpha * A) / a0;
	c.b1 = (-2.0 * cosw) / a0;
	c.b2 = (1.0 - alpha * A) / a0;
	c.a1 = (-2.0 * cosw) / a0;
	c.a2 = (1.0 - alpha / A) / a0;
	return c;
}

// the magnitude of one filter at the angular frequency w
double magnitude(const Coef& c, double w) {
	const double cw = cos(w), sw = sin(w), c2 = cos(2.0 * w), s2 = sin(2.0 * w);
	const double nr = c.b0 + c.b1 * cw + c.b2 * c2, ni = -(c.b1 * sw + c.b2 * s2);
	const double dr = 1.0 + c.a1 * cw + c.a2 * c2, di = -(c.a1 * sw + c.a2 * s2);
	const double den = dr * dr + di * di;
	return den > 0.0 ? sqrt((nr * nr + ni * ni) / den) : 1.0;
}

// bands above this would sit too close to half the sample rate (only happens at low sample rates)
bool usable(int band) { return kFreq[band] < 0.45 * (double)g_rate; }

// Works out the coefficients for the current gains and hands them to the audio thread.
void publish() {
	Coef c[BAND_COUNT];
	uint32_t active = 0;
	for (int b = 0; b < BAND_COUNT; b++) {
		if (g_gainDb[b] != 0 && usable(b)) { c[b] = design(kFreq[b], g_gainDb[b]); active |= 1u << b; }
		else c[b] = { 1.0, 0.0, 0.0, 0.0, 0.0 };
	}

	// headroom: the loudest point of the combined response decides how far the level is lowered, so boosts cannot clip
	double peak = 1.0;
	if (active) {
		const double top = fmin(20000.0, 0.45 * (double)g_rate);
		for (int k = 0; k < 128; k++) {
			const double hz = 20.0 * pow(top / 20.0, k / 127.0);
			const double w = 2.0 * M_PI * hz / (double)g_rate;
			double m = 1.0;
			for (int b = 0; b < BAND_COUNT; b++) if (active & (1u << b)) m *= magnitude(c[b], w);
			if (m > peak) peak = m;
		}
	}
	const double preamp = 1.0 / peak;
	g_preampDb = -20.0 * log10(peak);

	const uint32_t s = g_seq.load(std::memory_order_relaxed);
	g_seq.store(s + 1, std::memory_order_relaxed);          // odd: being written
	std::atomic_thread_fence(std::memory_order_release);
	for (int b = 0; b < BAND_COUNT; b++) {
		g_shared[b][0].store(c[b].b0, std::memory_order_relaxed);
		g_shared[b][1].store(c[b].b1, std::memory_order_relaxed);
		g_shared[b][2].store(c[b].b2, std::memory_order_relaxed);
		g_shared[b][3].store(c[b].a1, std::memory_order_relaxed);
		g_shared[b][4].store(c[b].a2, std::memory_order_relaxed);
	}
	g_sharedPreamp.store(preamp, std::memory_order_relaxed);
	g_sharedActive.store(active, std::memory_order_relaxed);
	g_seq.store(s + 2, std::memory_order_release);          // even: complete
}

// Audio thread: take over new coefficients if there are any. If the writer is busy right now, just keep the old ones
// until the next buffer: the audio is never skipped.
void refreshCoefs() {
	if (g_seq.load(std::memory_order_acquire) == a_seq) return;
	for (int attempt = 0; attempt < 6; attempt++) {
		const uint32_t s = g_seq.load(std::memory_order_acquire);
		if (s & 1u) continue;
		Coef tmp[BAND_COUNT];
		for (int b = 0; b < BAND_COUNT; b++) {
			tmp[b].b0 = g_shared[b][0].load(std::memory_order_relaxed);
			tmp[b].b1 = g_shared[b][1].load(std::memory_order_relaxed);
			tmp[b].b2 = g_shared[b][2].load(std::memory_order_relaxed);
			tmp[b].a1 = g_shared[b][3].load(std::memory_order_relaxed);
			tmp[b].a2 = g_shared[b][4].load(std::memory_order_relaxed);
		}
		const double pre = g_sharedPreamp.load(std::memory_order_relaxed);
		const uint32_t act = g_sharedActive.load(std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_acquire);
		if (g_seq.load(std::memory_order_relaxed) != s) continue; // it changed while we copied: try again
		for (int b = 0; b < BAND_COUNT; b++) a_coef[b] = tmp[b];
		// a band that switches on or off starts from a clean state
		const uint32_t changed = act ^ a_active;
		for (int b = 0; b < BAND_COUNT; b++)
			if (changed & (1u << b)) for (int c = 0; c < 2; c++) a_z1[c][b] = a_z2[c][b] = 0.0;
		a_active = act;
		a_preamp = pre;
		a_seq = s;
		return;
	}
}

inline int sat16(double v) {
	if (v > 32767.0) return 32767;
	if (v < -32768.0) return -32768;
	return (int)(v + (v >= 0.0 ? 0.5 : -0.5));
}

} // namespace

const char* bandName(int band) { return (band >= 0 && band < BAND_COUNT) ? kNames[band] : ""; }
const char* bandShortName(int band) { return (band >= 0 && band < BAND_COUNT) ? kShort[band] : ""; }
const char* presetName(int p) { return (p >= 0 && p < PRESET_COUNT) ? kPresetNames[p] : "Custom"; }

void init(int sampleRate) {
	g_rate = (sampleRate > 0) ? sampleRate : 44100;
	memset(a_z1, 0, sizeof(a_z1));
	memset(a_z2, 0, sizeof(a_z2));
	a_seq = 0xFFFFFFFFu;
	a_active = 0;
	a_preamp = 1.0;
	a_wasBypassed = true;
	g_bypass.store(g_bypassCtl);
	g_ready = true;
	publish();
}

void shutdown() { g_ready = false; }

// The audio callback runs on the mixer's thread: no locks, no allocation, and the audio is never skipped.
void process(short* buf, int frames) {
	if (!g_ready || !buf || frames <= 0) return;
	if (g_bypass.load(std::memory_order_relaxed)) { a_wasBypassed = true; return; }
	const bool resetRequested = g_resetState.exchange(false);
	if (a_wasBypassed || resetRequested) { // coming out of bypass: no stale tail from before
		memset(a_z1, 0, sizeof(a_z1));
		memset(a_z2, 0, sizeof(a_z2));
		a_wasBypassed = false;
	}
	refreshCoefs();
	if (a_active == 0) return; // every band flat: leave the audio exactly as it is

	int bands[BAND_COUNT], n = 0;
	for (int b = 0; b < BAND_COUNT; b++) if (a_active & (1u << b)) bands[n++] = b;
	const double pre = a_preamp;

	for (int i = 0; i < frames; i++) {
		for (int c = 0; c < 2; c++) {
			double x = (double)buf[i * 2 + c] * pre;
			for (int k = 0; k < n; k++) {
				const int b = bands[k];
				const Coef& q = a_coef[b];
				const double y = q.b0 * x + a_z1[c][b];
				a_z1[c][b] = q.b1 * x - q.a1 * y + a_z2[c][b];
				a_z2[c][b] = q.b2 * x - q.a2 * y;
				x = y;
			}
			buf[i * 2 + c] = (short)sat16(x);
		}
	}
	// tiny leftovers in the filter memory slow some processors down: flush them
	for (int c = 0; c < 2; c++)
		for (int k = 0; k < n; k++) {
			const int b = bands[k];
			if (fabs(a_z1[c][b]) < 1e-20) a_z1[c][b] = 0.0;
			if (fabs(a_z2[c][b]) < 1e-20) a_z2[c][b] = 0.0;
		}
}

int gain(int band) {
	if (band < 0 || band >= BAND_COUNT) return 0;
	return g_gainDb[band];
}

void setGain(int band, int db) {
	if (band < 0 || band >= BAND_COUNT) return;
	if (db < MIN_DB) db = MIN_DB;
	if (db > MAX_DB) db = MAX_DB;
	if (db == g_gainDb[band]) return;
	g_gainDb[band] = db;
	publish();
}

bool bypass() { return g_bypassCtl; }

void setBypass(bool b) {
	if (g_bypassCtl == b) return;
	g_bypassCtl = b;
	if (!b) g_resetState.store(true); // the audio thread clears its filter memory when it comes back
	g_bypass.store(b);
}

void applyPreset(int preset) {
	if (preset < 0 || preset >= PRESET_COUNT) return;
	for (int b = 0; b < BAND_COUNT; b++) g_gainDb[b] = PRESETS[preset][b];
	publish();
}

int currentPreset() {
	for (int p = 0; p < PRESET_COUNT; p++) {
		bool same = true;
		for (int b = 0; b < BAND_COUNT && same; b++) if (g_gainDb[b] != PRESETS[p][b]) same = false;
		if (same) return p;
	}
	return PRESET_CUSTOM;
}

double responseDb(double hz) {
	const double w = 2.0 * M_PI * hz / (double)g_rate;
	double m = 1.0;
	for (int b = 0; b < BAND_COUNT; b++)
		if (g_gainDb[b] != 0 && usable(b)) m *= magnitude(design(kFreq[b], g_gainDb[b]), w);
	return 20.0 * log10(m);
}

int levelReductionDb() { return (int)lround(g_preampDb); } // e.g. -6.4 dB -> -6

} // namespace equalizer
