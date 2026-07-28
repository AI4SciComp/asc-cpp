#include "asc/core/providers/cuda.h"

void CopyCompletionEvent(const asc::CompletionEvent& event) {
  const asc::CompletionEvent copy = event;
  static_cast<void>(copy);
}
