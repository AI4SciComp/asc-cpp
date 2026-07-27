#include "asc/core/execution.h"

void CopyEvent(asc::CompletionEvent& event) {
  asc::CompletionEvent copy(event);
  static_cast<void>(copy);
}
