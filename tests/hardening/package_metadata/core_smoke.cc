#include "asc/core/status.h"

int main() { return asc::Status::Ok().ok() ? 0 : 1; }
