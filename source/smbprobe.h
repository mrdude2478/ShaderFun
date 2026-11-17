#pragma once
// Is the network share reachable right now? The network thread checks it with its real connection (the same way songs and
// folders are fetched), at low priority and every few seconds, so the menu never waits for it. The main menu library uses the
// answer to show or hide the network songs.
//
// Right after the app starts there is a grace period: the answer is "yes" (as long as nothing says otherwise) while Wi-Fi and the
// share come up, so the menu is complete from the start and there is no false alarm.

#include <stdint.h>

namespace smbprobe {

void poll();            // main thread, once per frame (it only acts about once a second)
bool reachable();       // Wi-Fi is on and the share answers
uint32_t version();     // increases whenever reachable() changes
void shutdown();        // (nothing to stop: the check runs on the network thread)

} // namespace smbprobe
