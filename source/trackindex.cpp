#include "trackindex.h"
#include "alllib.h"
#include "smblib.h"
#include "usbdrive.h"
#include "usblib.h"

#include <unordered_map>
#include <vector>

namespace trackindex {
namespace {

std::vector<std::shared_ptr<const library::Snapshot>> g_snaps;
std::unordered_map<std::string, std::pair<int, int>> g_map;   // path -> (snapshot, track)
std::unordered_map<std::string, std::pair<int, int>> g_fold;  // the same with the paths "folded" (see foldKey): the fallback when a path is written slightly differently
uint32_t g_v[4] = { 0xFFFFFFFFu, 0, 0, 0 };

// A path without the things that can differ between two ways of building the same path: upper / lower case, backslashes,
// repeated slashes ("ums0://a.mp3") and a slash at the end. The "://" of smb:// and usb:// is kept.
std::string foldKey(const std::string& p) {
	std::string r;
	r.reserve(p.size());
	for (char c : p) {
		if (c == '\\') c = '/';
		if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		if (c == '/' && !r.empty() && r.back() == '/' && !(r.size() >= 2 && r[r.size() - 2] == ':')) continue;
		r += c;
	}
	while (r.size() > 1 && r.back() == '/') r.pop_back();
	return r;
}

void ensure() {
	const uint32_t v[4] = { library::version(), smblib::version(), usblib::version(), alllib::version() };
	if (v[0] == g_v[0] && v[1] == g_v[1] && v[2] == g_v[2] && v[3] == g_v[3]) return;
	for (int i = 0; i < 4; i++) g_v[i] = v[i];
	g_snaps.clear();
	g_map.clear();
	g_fold.clear();
	g_snaps.push_back(alllib::snapshot());     // first: what the main menu shows
	g_snaps.push_back(library::snapshot());
	g_snaps.push_back(smblib::snapshot());
	for (const usblib::DriveInfo& d : usblib::drives()) g_snaps.push_back(usblib::snapshot(d.id));
	size_t total = 0;
	for (const auto& s : g_snaps) total += s->tracks.size();
	g_map.reserve(total);
	g_fold.reserve(total);
	for (size_t si = 0; si < g_snaps.size(); si++) {
		const std::vector<library::Track>& tr = g_snaps[si]->tracks;
		for (size_t i = 0; i < tr.size(); i++) {
			g_map.emplace(tr[i].path, std::make_pair((int)si, (int)i)); // (the first one found stays)
			g_fold.emplace(foldKey(tr[i].path), std::make_pair((int)si, (int)i));
		}
	}
}

int groupOf(const std::vector<library::Group>& groups, int track) {
	for (size_t g = 0; g < groups.size(); g++)
		for (int t : groups[g].tracks) if (t == track) return (int)g;
	return -1;
}

} // namespace

Hit find(const std::string& path) {
	ensure();
	Hit h;
	std::string key = path;
	if (usbdrive::isUsbPath(key)) { const std::string st = usbdrive::stablePath(key); if (!st.empty()) key = st; }
	auto it = g_map.find(key);
	if (it != g_map.end()) {
		h.snap = g_snaps[it->second.first];
		h.track = it->second.second;
		return h;
	}
	auto fi = g_fold.find(foldKey(key)); // not exactly the same path: try again ignoring case, slashes and the like
	if (fi == g_fold.end()) return h;
	h.snap = g_snaps[fi->second.first];
	h.track = fi->second.second;
	return h;
}

int albumGroup(const Hit& h) { return h.snap ? groupOf(h.snap->albums, h.track) : -1; }
int artistGroup(const Hit& h) { return h.snap ? groupOf(h.snap->artists, h.track) : -1; }

} // namespace trackindex
