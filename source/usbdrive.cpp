#include "usbdrive.h"

#include <switch.h>
#include <usbhsfs.h>

#include <mutex>
#include <stdio.h>
#include <string.h>

namespace usbdrive {
	namespace {

		const u32 MAX_DEVICES = 32;

		std::mutex g_mu;
		std::vector<Drive> g_drives;
		UsbHsFsDevice g_raw[MAX_DEVICES];   // the library's own records (needed to unmount)
		u32 g_rawCount = 0;
		uint32_t g_counter = 0;
		bool g_started = false;
		std::string g_error;

		std::string trimmed(const char* s) {
			std::string t = s ? s : "";
			size_t a = 0, b = t.size();
			while (a < b && (t[a] == ' ' || t[a] == '\t')) a++;
			while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) b--;
			return t.substr(a, b - a);
		}

		// Called by the library (on its own thread) whenever a drive is connected, removed or unmounted.
		void populate(const UsbHsFsDevice* devices, u32 count, void*) {
			std::lock_guard<std::mutex> lock(g_mu);
			g_drives.clear();
			g_rawCount = 0;
			for (u32 i = 0; devices && i < count && i < MAX_DEVICES; i++) {
				const UsbHsFsDevice& d = devices[i];
				Drive out;
				out.mount = d.name;
				const std::string volume_label = trimmed(d.volume_label);
				const std::string maker = trimmed(d.manufacturer);
				const std::string product = trimmed(d.product_name);

				if (!volume_label.empty()) {
					out.name = volume_label;
				}
				else {
					out.name = (maker.empty() ? product :
						(product.empty() ? maker : maker + " " + product));
				}

				if (out.name.empty()) {
					out.name = "USB Drive";
				}
				out.fsType = LIBUSBHSFS_FS_TYPE_STR(d.fs_type);
				out.capacity = d.capacity;
				out.writeProtected = d.write_protect;
				g_drives.push_back(out);
				g_raw[g_rawCount++] = d;
			}
			g_counter++;
		}

	} // namespace

	bool start() {
		if (g_started) return true;
		// read-only: nothing is ever written to a drive, and a drive that was not ejected properly does no harm
		usbHsFsSetFileSystemMountFlags(UsbHsFsMountFlags_ReadOnly);
		usbHsFsSetPopulateCallback(populate, nullptr);
		const Result rc = usbHsFsInitialize(0);
		if (R_FAILED(rc)) {
			usbHsFsSetPopulateCallback(nullptr, nullptr);
			char buf[64];
			snprintf(buf, sizeof(buf), "the USB service could not start (0x%x)", (unsigned)rc);
			g_error = buf;
			return false;
		}
		g_started = true;
		g_error.clear();
		return true;
	}

	void stop() {
		if (!g_started) return;
		usbHsFsSetPopulateCallback(nullptr, nullptr);
		usbHsFsExit(); // unmounts whatever is still mounted
		g_started = false;
		std::lock_guard<std::mutex> lock(g_mu);
		g_drives.clear();
		g_rawCount = 0;
	}

	bool available() { return g_started; }

	std::string lastError() { return g_error; }

	std::vector<Drive> drives() {
		std::lock_guard<std::mutex> lock(g_mu);
		return g_drives;
	}

	uint32_t changeCounter() {
		std::lock_guard<std::mutex> lock(g_mu);
		return g_counter;
	}

	bool unmount(const std::string& mountName) {
		UsbHsFsDevice dev;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			bool found = false;
			for (u32 i = 0; i < g_rawCount; i++) {
				if (mountName == g_raw[i].name) { dev = g_raw[i]; found = true; break; }
			}
			if (!found) return false;
		}
		return usbHsFsUnmountDevice(&dev, true); // also triggers a fresh populate() call
	}

	bool isMounted(const std::string& path) {
		if (!isUsbPath(path)) return true; // not our business
		const std::string mount = path.substr(0, path.find(':') + 1);
		std::lock_guard<std::mutex> lock(g_mu);
		for (const Drive& d : g_drives) if (d.mount == mount) return true;
		return false;
	}

} // namespace usbdrive
