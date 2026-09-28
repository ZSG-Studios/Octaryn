#pragma once
#include <stdint.h>

// Version 2, byte size 72. Reservations bound retained collision preparation;
// they are conservative accounting estimates, not OS resident-memory readings.
typedef struct OctarynCollisionResidencyStats {
  uint32_t version, resident, preparing, failed;
  uint64_t reserved_bytes, loads, evictions, cancelled, waits;
  uint64_t resident_bytes, budget_bytes;
} OctarynCollisionResidencyStats;
