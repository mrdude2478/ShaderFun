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

std::string configString(const char* key, const std::string& defaultValue = "");
void setConfigString(const char* key, const std::string& value, bool saveNow = true); // saveNow=false: batch several, then call saveState()

// ---- saved state: the last track + position and the queue you chose are restored on the next launch
void saveState();           // writes config.ini (+ last_queue.txt when the queue changed); also called by shutdown()

// ---- sleep timer: fades the music out over its last 15 seconds, then pauses it
const int SLEEP_END_OF_TRACK = -1;
void setSleepTimer(int minutes);   // 0 = off, SLEEP_END_OF_TRACK = pause when the current track ends
bool sleepActive();
bool sleepAtTrackEnd();
int sleepSecondsLeft();            // -1 when off or waiting for the end of the track
bool sleepFading();
bool consumeSleepFired();          // true once, after the timer has paused the music

// ---- music folder (default library + start-up queue)
const std::string& musicFolder();
void setMusicFolder(const std::string& dir); // saved to config; replaces the queue if nothing is playing
bool playFolder(const std::string& dir);     // queue everything inside (recursive) and start playing
bool playFile(const std::string& file);      // queue the tracks of the file's folder, start at this file
bool playTracks(const std::vector<std::string>& tracks, int startIndex); // queue exactly these tracks (albums, playlists...)
std::vector<std::string> queuePaths();       // the current queue in its original order
// Only the queue entries YOU picked (played an album/folder/playlist, or added with enqueue).
// The tracks the player loads by itself at start-up are left out, so they never end up in a saved playlist.
std::vector<std::string> chosenQueuePaths();
std::vector<std::string> tracksInFolder(const std::string& dir); // every music file inside (recursive), in play order
// Add tracks to the running queue without interrupting the current track.
// playNext=true: they play right after the current track; false: at the end of the queue.
// If nothing is queued yet they simply start playing. Returns how many tracks were added.
int enqueue(const std::vector<std::string>& tracks, bool playNext);

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
bool isLoading();            // a network song is being downloaded before it can play
float loadProgress();        // 0..1 while isLoading(), -1 if unknown
uint32_t trackChangeCounter(); // increases whenever a different track starts

} // namespace player
