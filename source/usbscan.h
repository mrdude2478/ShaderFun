#pragma once
// Scans a connected USB drive into the database (db.h): every music file gets a row with its tags, marked with the
// drive's id (see usbdrive.h). The drive is read like any local folder, so tags are read right away, in the same pass.
// Files that did not change since the last scan are not read again; files that are gone are removed when a scan has
// visited every folder. Every scan is noted in usbscan.log.

#include <string>

namespace usbscan {

bool start(const std::string& mount);   // mount like "ums0:". false when it cannot start
void cancel();
bool running();
std::string status();                   // "1234 songs in 87 folders so far"
void poll();                            // call once per frame: shows a message when a scan has finished
void shutdown();                        // call before the drive interface and the database are shut down

} // namespace usbscan
