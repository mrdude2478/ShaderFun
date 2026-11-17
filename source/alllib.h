#pragma once
// The library the main menu shows: the SD card music together with everything that is reachable right now: the network
// share (when Wi-Fi is on) and the USB drives that are plugged in. Music of a source that is away is simply not in it,
// so it disappears from the main menu pages (Albums, Artists, Genres, All songs, Search) and comes back with the source.
// (The "Network library" and "USB library" sections still show everything that has been scanned, connected or not.)
// A song that exists on more than one source (same artist, album, title and track number) is listed once.
//
// Built on the database thread whenever one of the libraries or the set of connected sources changes.

#include "library.h"

#include <memory>
#include <stdint.h>

namespace alllib {

void poll();                                           // call once per frame (it only looks every half second)
std::shared_ptr<const library::Snapshot> snapshot();   // never null; the SD card library until the first merge
uint32_t version();                                    // increases whenever a new merged library is ready

} // namespace alllib
