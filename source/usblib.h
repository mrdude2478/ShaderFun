#pragma once
// The USB libraries: every drive that has been scanned (usbscan.h) with its songs, loaded from the database (db.h) into
// the same kind of snapshot the SD card and network libraries use, so the menu can show a drive with the usual album /
// artist / genre pages. Kept in memory and rebuilt on the database thread after every scan. Drives that are not plugged
// in right now are still listed and can be browsed; only playing needs the drive.

#include "library.h"

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace usblib {

struct DriveInfo {
	std::string id;          // see usbdrive.h
	std::string label;       // the name shown in the menu
	std::string fsType;
	uint64_t capacity = 0;
	int64_t lastScan = 0;    // 0 = never scanned completely
	int songs = 0;
};

void init();                                          // after db::init()
void reload();                                        // the database changed: load it again (any thread)
std::vector<DriveInfo> drives();                      // every drive in the database, sorted by name
std::shared_ptr<const library::Snapshot> snapshot(const std::string& driveId);   // never null
std::string label(const std::string& driveId);
bool connected(const std::string& driveId);           // is that drive plugged in right now?
void forgetDrive(const std::string& driveId);         // removes the drive and all its songs from the database (it is listed again after its next scan)
bool loading();
uint32_t version();                                   // increases whenever new data is ready

} // namespace usblib
