#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/sparse/compressed.h"

void AttemptSameFormatConversion(const asc::ExecutionContext& context,
                                 asc::CsrView<const double> source,
                                 asc::MemoryResource& resource) {
  (void)asc::CsrArray<double>::FromCompressed(context, source, resource);
}

int main() { return 0; }
