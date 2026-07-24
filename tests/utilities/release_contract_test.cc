#include <asc/core/contracts.h>
#include <asc/utilities/timer.h>

int main() {
  asc::Timer timer;
  try {
    static_cast<void>(timer.Stop());
  } catch (const asc::ContractException&) {
    return 0;
  }
  return 1;
}
