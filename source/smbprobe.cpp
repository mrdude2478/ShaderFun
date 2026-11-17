#include "smbprobe.h"
#include "netstatus.h"
#include "smbnet.h"
#include "ui.h"

#include <SDL2/SDL.h>
#include <atomic>

namespace smbprobe {
namespace {

const uint32_t PROBE_EVERY_UP_MS = 20000;     // while the share answers
const uint32_t PROBE_EVERY_DOWN_MS = 8000;    // while it does not (so it comes back quickly)
const uint32_t PROBE_CONFIRM_MS = 3000;       // after a failure while it was up: look again before giving up on it
const uint32_t STARTUP_GRACE_MS = 15000;      // Wi-Fi and the share need a few seconds after the app starts: no verdict (and no message) before that
const uint32_t PROBE_STARTUP_MS = 2000;       // ... and until the share has answered once, look again soon

// main thread only
bool g_state = true;               // the current answer
bool g_haveVerdict = false;        // a check has answered at least once
bool g_everUp = false;             // the share has answered at least once since the start
bool g_wasWifiOff = false;
bool g_pending = false;            // a check was asked for and has not answered yet
bool g_started = false;
int g_fails = 0;
uint32_t g_startTick = 0, g_lastTick = 0, g_nextProbe = 0, g_seenReal = 0;
std::atomic<uint32_t> g_version(0);

void setState(bool s, bool announce) {
	if (s == g_state) return;
	g_state = s;
	g_version++;
	if (announce) ui::toast(s ? "The network share is reachable again" : "The network share can't be reached - its songs are hidden from the main menu");
}

} // namespace

void poll() {
	const uint32_t now = SDL_GetTicks();
	if (g_started && now - g_lastTick < 1000) return;
	g_lastTick = now;
	if (!g_started) { g_started = true; g_startTick = now; g_nextProbe = now + 500; }

	const smbnet::Config c = smbnet::getConfig();
	if (c.server.empty() || c.share.empty()) { setState(true, false); return; } // no share: nothing to decide

	const bool inGrace = !g_everUp && (now - g_startTick < STARTUP_GRACE_MS);   // just started, the share has not answered yet
	const bool quiet = (now - g_startTick < STARTUP_GRACE_MS);                  // no messages while the app is still starting up

	if (!wifiConnected()) {
		if (inGrace) { g_nextProbe = now; return; } // Wi-Fi is still connecting: keep the menu as it is
		g_wasWifiOff = true; // no Wi-Fi: the share cannot be there, and once it is back it must answer first
		g_haveVerdict = true;
		g_fails = 0;
		setState(false, false);
		g_nextProbe = now;
		return;
	}

	bool ok = false, haveAnswer = false;

	// something the app did itself has just worked (a folder opened, a song fetched): that is an answer too
	const uint32_t real = smbnet::lastSuccessTick();
	if (real != 0 && real != g_seenReal) { g_seenReal = real; ok = true; haveAnswer = true; }

	// the check on the network thread
	bool checkOk = false;
	if (smbnet::takeCheckResult(checkOk)) {
		g_pending = false;
		if (checkOk) { ok = true; haveAnswer = true; }
		else if (!ok) haveAnswer = true;
	}

	if (haveAnswer) {
		if (ok) {
			g_fails = 0;
			g_wasWifiOff = false;
			g_everUp = true;
			setState(true, g_haveVerdict && !quiet);
			g_nextProbe = now + PROBE_EVERY_UP_MS;
			g_haveVerdict = true;
		}
		else if (inGrace) { // too early to say: Wi-Fi / the share may still be coming up. Try again soon, change nothing
			g_nextProbe = now + PROBE_STARTUP_MS;
		}
		else {
			g_fails++;
			if (!g_haveVerdict || !g_state || g_fails >= 2) { // the first real answer counts at once; later one failure is checked again first
				setState(false, true);
				g_nextProbe = now + PROBE_EVERY_DOWN_MS;
			}
			else g_nextProbe = now + PROBE_CONFIRM_MS;
			g_haveVerdict = true;
		}
		g_wasWifiOff = false;
	}

	// ask for the next check
	if (!g_pending && now >= g_nextProbe) {
		g_pending = true;
		g_nextProbe = now + 60000; // (set properly when the answer comes)
		smbnet::requestCheck();
	}
}

bool reachable() { return g_state; }

uint32_t version() { return g_version; }

void shutdown() {}

} // namespace smbprobe
