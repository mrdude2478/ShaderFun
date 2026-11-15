#pragma once
// Scans the SMB share into the database (db.h), in the background, in two passes:
//
// Pass 1 (files): every music file on the share gets a row in the "tracks" table: path, folder, file name, size and
// modified time. Files that did not change since the last scan keep what is already known about them; files that are gone
// are removed when the pass has visited every folder.
//
// Pass 2 (tags): for every row whose tags have not been read yet (tags_state = 0) the start of the song is read over the
// network and its tags (title, artist, album, ...) are stored. tags_state becomes 1 (read) or 2 (the file has no tags).
// A song that could not be opened stays at 0 and is tried again the next time. The pass can be stopped at any time and
// carries on where it left off.
//
// Both passes only work while nothing else needs the network connection. Every scan is noted in smbscan.log.

#include <string>

namespace smbscan {

bool start();                 // false when it cannot start (not set up, database missing, already running)
void cancel();
bool running();
std::string status();         // what it is doing right now, with numbers
void poll();                  // call once per frame: shows a message when a scan has finished

} // namespace smbscan
