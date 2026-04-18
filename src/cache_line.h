#pragma once

#include <cstdint>

// Represents a single cache block/line within a cache set.
struct CacheLine {
    uint64_t tag   = 0;      // Tag portion of the address
    bool     valid = false;  // Whether this line holds valid data
    bool     dirty = false;  // Whether this line has been written to (write-back)
};
