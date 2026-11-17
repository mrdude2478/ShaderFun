#include "alllib.h"
#include "db.h"
#include "smbprobe.h"
#include "smblib.h"
#include "smbnet.h"
#include "usbdrive.h"
#include "usblib.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <atomic>
#include <unordered_set>
#include <vector>

namespace alllib {
namespace {

SDL_mutex* g_mu = nullptr;
std::shared_ptr<const library::Snapshot> g_merged;   // null: use the SD card library
std::string g_key;                                   // what the current merged library was built from
std::atomic<bool> g_building(false);
std::atomic<uint32_t> g_version(0);
uint32_t g_lastCheck = 0;

// the same song on two sources: same artist, album, title and numbers (not possible for songs without tags)
std::string dedupeKey(const library::Track& t) {
	if (t.title.empty() || t.album.empty()) return "";
	std::string k;
	k.reserve(t.artist.size() + t.album.size() + t.title.size() + 16);
	auto add = [&k](const std::string& s) { for (char c : s) k += (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; k += '|'; };
	add(t.artist);
	add(t.album);
	add(t.title);
	k += std::to_string(t.trackNo) + "|" + std::to_string(t.discNo);
	return k;
}

void publish(std::shared_ptr<const library::Snapshot> s) {
	SDL_LockMutex(g_mu);
	g_merged = std::move(s);
	SDL_UnlockMutex(g_mu);
	g_version++;
}

} // namespace

void poll() {
	if (!g_mu) g_mu = SDL_CreateMutex();
	const uint32_t now = SDL_GetTicks();
	if (now - g_lastCheck < 500) return;
	g_lastCheck = now;
	if (g_building) return;

	// which sources are in right now
	const smbnet::Config cfg = smbnet::getConfig();
	const bool smbOn = !cfg.server.empty() && !cfg.share.empty() && smbprobe::reachable(); // (Wi-Fi on and the share's server answers)
	std::vector<std::string> ids;
	for (const usbdrive::Drive& d : usbdrive::drives()) ids.push_back(d.id);
	std::sort(ids.begin(), ids.end());
	std::string key = std::to_string(library::version()) + "|" + std::to_string(smblib::version()) + "|" + std::to_string(usblib::version()) + "|" + (smbOn ? "n" : "-");
	for (const std::string& id : ids) key += "|" + id;
	if (key == g_key) return;
	g_key = key;

	std::shared_ptr<const library::Snapshot> sd = library::snapshot();
	std::shared_ptr<const library::Snapshot> smb = smbOn ? smblib::snapshot() : nullptr;
	std::vector<std::shared_ptr<const library::Snapshot>> usb;
	for (const std::string& id : ids) {
		std::shared_ptr<const library::Snapshot> s = usblib::snapshot(id);
		if (s && !s->tracks.empty()) usb.push_back(std::move(s));
	}
	const bool smbHas = smb && !smb->tracks.empty();
	if (!smbHas && usb.empty()) { publish(sd); return; }   // nothing to add: it is just the SD card library
	if (!db::ready()) { publish(sd); return; }

	g_building = true;
	const bool queued = db::post([sd, smb, usb, smbHas](sqlite3*) {
		// sorting and grouping thousands of songs takes a moment: this runs on the database thread, not in the menu
		std::vector<library::Track> all;
		size_t total = sd->tracks.size() + (smbHas ? smb->tracks.size() : 0);
		for (const auto& u : usb) total += u->tracks.size();
		all.reserve(total);
		std::unordered_set<std::string> seen;
		seen.reserve(total);
		auto addAll = [&](const library::Snapshot& s) {
			for (const library::Track& t : s.tracks) {
				const std::string k = dedupeKey(t);
				if (!k.empty() && !seen.insert(k).second) continue; // already there from a source that comes first
				all.push_back(t);
			}
		};
		addAll(*sd);                       // the SD card first, then USB drives, then the network share
		for (const auto& u : usb) addAll(*u);
		if (smbHas) addAll(*smb);
		publish(library::makeSnapshot(std::move(all)));
		g_building = false;
	});
	if (!queued) { g_building = false; publish(sd); }
}

std::shared_ptr<const library::Snapshot> snapshot() {
	if (g_mu) {
		SDL_LockMutex(g_mu);
		std::shared_ptr<const library::Snapshot> s = g_merged;
		SDL_UnlockMutex(g_mu);
		if (s) return s;
	}
	return library::snapshot();
}

uint32_t version() { return g_version; }

} // namespace alllib
