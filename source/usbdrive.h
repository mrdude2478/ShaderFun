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
	std::string name;        // "SanDisk Ultra" (manufacturer + product, or "USB drive")
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
bool isMounted(const std::string& path);      // is the drive this path belongs to still connected?

} // namespace usbdrive
