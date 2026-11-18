#pragma once
// The play history: what you listened to, kept in its own file (sdmc:/switch/shaderfun/history.db) that is never rebuilt,
// replaced or emptied by a library scan. (The music catalogue, library.db, can always be made again; a listening history cannot.)
//
// A play is logged once the song has really been listened to: half of it, or 4 minutes, whichever comes first (a song that is
// skipped after a few seconds does not count). The title, artist and album are copied into the log when the play is logged,
// so the history stays readable even if a song later leaves the library.

#include <stdint.h>
#include <string>
#include <vector>

namespace history {

struct Entry {
	std::string path;          // how to play it (a USB song as usb://<drive id>/..., see usbdrive.h)
	std::string title, artist, album;
	int plays = 0;
	int64_t lastPlayed = 0;    // seconds since 1970
};

void init();                   // after db::init(): loads the lists
void update(float dt);         // every frame: counts the time you listen, logs a play when it is reached
bool enabled();                // Settings: "Remember what I play"
std::vector<Entry> recent();       // the last songs played, newest first (every song once)
std::vector<Entry> mostPlayed();   // the songs with the most plays
int totalPlays();
uint32_t version();            // increases whenever the lists changed
void clearAll();               // deletes the whole history

} // namespace history
