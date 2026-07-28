#include <asc/core/status.h>

int main() {
  const asc::Status status;
  return status.ok() ? 0 : 1;
}
