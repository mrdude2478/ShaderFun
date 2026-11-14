#pragma once
// 10-band graphic equalizer for ShaderFun: ten peaking filters in series (octave spacing, 31 Hz .. 16 kHz), applied to the
// final mixer output in audioEffectCallback (main.cpp). The UI changes the gains through the setters, which are safe to call
// from the main thread while the audio thread is processing.
//
// How it stays click-free: the audio thread owns all filter memory and only ever copies new coefficients (never the other way
// round), so changing a gain does not reset the filters. The coefficients travel between the threads in a seqlock.
// Boosts are compensated by an automatic level reduction (preamp), so the equalizer cannot make the audio clip.

namespace equalizer {

const int BAND_COUNT = 10;
const int MAX_DB = 12;            // gain range: -12 .. +12 dB
const int MIN_DB = -12;
const char* bandName(int band);        // "31 Hz", "62 Hz", "125 Hz" ... "16 kHz"
const char* bandShortName(int band);   // "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"

// Presets. PRESET_CUSTOM = the gains no longer match any preset
enum Preset {
	PRESET_FLAT = 0, PRESET_BASS, PRESET_LESS_BASS, PRESET_TREBLE, PRESET_VOCAL, PRESET_LOUDNESS, PRESET_ROCK, PRESET_POP,
	PRESET_JAZZ, PRESET_CLASSICAL, PRESET_DANCE, PRESET_ELECTRONIC, PRESET_HIPHOP, PRESET_ACOUSTIC, PRESET_HANDHELD,
	PRESET_COUNT, PRESET_CUSTOM = -1
};
const char* presetName(int preset);

// Call once after Mix_OpenAudio succeeds, with the sample rate the mixer is really using.
void init(int sampleRate);
void shutdown();

// Called from the post-mix effect. buf = interleaved 16-bit stereo samples, frames = number of stereo pairs.
// Cheap: when the equalizer is bypassed, or every band is at 0 dB, the buffer is left untouched.
void process(short* buf, int frames);

// ---- the controls (safe from the main thread)
void setGain(int band, int db);   // clamped to -12..+12
int  gain(int band);
void setBypass(bool bypass);
bool bypass();
void applyPreset(int preset);     // sets all the gains at once (PRESET_CUSTOM is ignored)
int  currentPreset();             // the matching preset, or PRESET_CUSTOM

// For the UI
double responseDb(double hz);     // the combined boost / cut at a frequency, before the automatic level reduction
int    levelReductionDb();        // how much the overall level is lowered to leave room for the boosts (0 or negative)

} // namespace equalizer
