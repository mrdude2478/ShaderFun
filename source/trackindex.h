#pragma once
// Finds a song by its path in the libraries the menu knows (SD card, network share, USB drives) and tells which album and
// artist page it belongs to. Used for the "go to album / artist" actions and by the play history.
// Main thread only. The index is rebuilt (a few milliseconds) the first time it is used after a library changed.

#include "library.h"

#include <memory>
#include <string>

namespace trackindex {

struct Hit {
	std::shared_ptr<const library::Snapshot> snap;   // null: the song is not in any library
	int track = -1;                                  // index into snap->tracks
};

Hit find(const std::string& path);                   // a path as the player uses it (a USB drive's current "ums0:" paths are understood too)
int albumGroup(const Hit& h);                        // index into snap->albums, or -1
int artistGroup(const Hit& h);                       // index into snap->artists, or -1

} // namespace trackindex
