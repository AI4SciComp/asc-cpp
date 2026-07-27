#include "asc/core/providers/cuda.h"
#include "m6_provider_multi_tu.h"

int main() {
  const auto count = M6CudaCountFromFirstTranslationUnit();
  if (!count.ok() || *count <= 0) {
    return 1;
  }
  const auto context = M6CudaContextFromSecondTranslationUnit();
  if (!context.ok()) {
    return 2;
  }
  auto event = asc::RecordCudaEvent(*context);
  if (!event.ok()) {
    return 3;
  }
  return event->Wait().ok() ? 0 : 4;
}
