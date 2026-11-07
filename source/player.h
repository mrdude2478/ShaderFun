#pragma once
// Music player core for ShaderFun: queue, shuffle/repeat, transport, position/duration,
// music-folder setting (saved in config.ini). Wraps SDL_mixer's music API.
// Mix_OpenAudio() must have been called before player::init().

#include <stdint.h>
#include <string>
#include <vector>

namespace player {

enum PlayMode {
	MODE_REPEAT_ALL = 0, // play in order, loop the queue (default, like the original behaviour)
	MODE_SHUFFLE,        // random order, reshuffled every loop
	MODE_REPEAT_ONE,     // loop the current track
	MODE_ONCE,           // play in order and stop at the end
	MODE_COUNT
};

struct DirEntry {
	std::string name;
	std::string path;
	bool isDir = false;
};

// ---- file helpers (also used by the library browser)
bool isMusicFile(const std::string& name);
std::vector<DirEntry> listDirectory(const std::string& dir); // folders first, then music files, natural sort
std::string parentPath(const std::string& path);             // "" when there is no parent
std::string baseName(const std::string& path);               // last path component
std::string joinPath(const std::string& dir, const std::string& name);

// ---- lifecycle
void init(const char* configPath);
void shutdown();            // saves the config
void update();              // call once per frame: handles end-of-track

// ---- other settings stored in config.ini (the player owns the file; this keeps unknown keys when it is rewritten)
int configInt(const char* key, int defaultValue);
void setConfigInt(const char* key, int value);   // saved immediately

// ---- music folder (default library + start-up queue)
const std::string& musicFolder();
void setMusicFolder(const std::string& dir); // saved to config; replaces the queue if nothing is playing
bool playFolder(const std::string& dir);     // queue everything inside (recursive) and start playing
bool playFile(const std::string& file);      // queue the tracks of the file's folder, start at this file
bool playTracks(const std::vector<std::string>& tracks, int startIndex); // queue exactly these tracks (albums, playlists...)
std::vector<std::string> queuePaths();       // the current queue in its original order

// ---- transport
void play();                 // start/resume (starts the current queue entry if stopped)
void pause();
void togglePause();
void stop();
void next();
void prev();                 // restarts the track if it's already past 3 seconds
void seekBy(double seconds);
void seekTo(double seconds);
void setVolume(int volume);  // 0..128
void setMode(PlayMode mode);
void cycleMode();

// ---- state
bool hasTracks();
bool isPlaying();
bool isPaused();
bool canSeek();
int volume();
PlayMode mode();
const char* modeName(PlayMode m);
double position();           // seconds
double duration();           // seconds, 0 when unknown
int queueIndex();            // 0-based position in play order, -1 if none
int queueSize();
const std::string& currentPath();
std::string title();         // tag title or file name
std::string subtitle();      // artist (- album) or the folder name
std::string nowPlayingText();// "Artist - Title" for the scroller
uint32_t trackChangeCounter(); // increases whenever a different track starts

} // namespace player
