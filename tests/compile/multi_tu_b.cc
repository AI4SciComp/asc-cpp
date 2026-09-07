#include "asc/core.h"  // NOLINT(misc-include-cleaner): umbrella-header probe
#include "asc/core/types.h"
#include "multi_tu.h"

int CoreMultiTuB() { return static_cast<int>(sizeof(asc::extent_t)); }
