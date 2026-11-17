#pragma once
// USB drives for ShaderFun (docked): uses libusbhsfs to mount the filesystems of USB Mass Storage devices.
// A mounted filesystem looks like a normal drive to the rest of the program: "ums0:/Music/song.mp3".
// Drives are mounted read-only, because the player never writes to them.

#include <stdint.h>
#include <string>
#include <vector>

namespace usbdrive {

struct Drive {
	std::string mount;       // "ums0:"
	std::string name;        // "USB-Music" (the volume label), else "SanDisk Ultra" (manufacturer + product), else "USB Drive"
	std::string label, maker, product;   // as reported by the drive (may be empty)
	std::string id;          // stable: 8 hex digits made from label, maker, product, file system and size. (No serial number: not every drive can report one.)
	std::string fsType;      // "exFAT", "FAT32", "NTFS", "EXT4"...
	uint64_t capacity = 0;   // bytes (of the whole device)
	bool writeProtected = false;
};

bool start();                       // starts the USB host interface. false if it is not available (see lastError())
void stop();                        // unmounts everything and shuts the interface down
bool available();                   // start() worked
std::string lastError();

std::vector<Drive> drives();        // the filesystems that are mounted right now
uint32_t changeCounter();           // goes up every time a drive is connected or removed
bool unmount(const std::string& mountName);   // safely remove the physical device this mount belongs to

// "ums0:/Music/a.mp3" or "ums1:" -> true
inline bool isUsbPath(const std::string& p) {
	const size_t colon = p.find(':');
	return p.size() > 4 && p.compare(0, 3, "ums") == 0 && colon != std::string::npos && colon <= 7;
}
// Paths that stay valid whichever mount name ("ums0:", "ums1:") a drive gets: "usb://<id>/Music/a.mp3"
inline bool isStablePath(const std::string& p) { return p.compare(0, 6, "usb://") == 0; }
std::string stablePath(const std::string& realPath);   // "ums0:/Music/a.mp3" -> "usb://<id>/Music/a.mp3" ("" if it is not a path on a connected drive)
std::string resolvePath(const std::string& stable);    // the opposite; "" when that drive is not connected right now
bool findDrive(const std::string& mount, Drive& out);  // mount like "ums0:"
bool isMounted(const std::string& path);      // is the drive this path belongs to still connected?

} // namespace usbdrive
