#pragma once
// The network (SMB) music library, loaded from the database (db.h) into the same kind of snapshot the SD card library uses,
// so the menu can show it with the same album / artist / genre pages. It is kept in memory, built on the database thread
// (so the menu never waits for it), and rebuilt after every scan.

#include "library.h"

#include <memory>
#include <stdint.h>

namespace smblib {

void init();                                        // after db::init(): loads the catalogue
void reload();                                      // the database changed (a scan wrote to it): load it again
std::shared_ptr<const library::Snapshot> snapshot(); // never null; empty until loaded / scanned
bool loading();
uint32_t version();                                 // increases whenever a new snapshot is ready

} // namespace smblib
