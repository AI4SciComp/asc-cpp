#include "asc/expression/writable.h"

struct M4UnadaptedDestination {};

static_assert(asc::WritableExpression<M4UnadaptedDestination>);

int main() { return 0; }
