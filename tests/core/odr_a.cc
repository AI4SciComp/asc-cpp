#include <asc/core.h>

#include <utility>

int CoreOdrA() {
  asc::Result<asc::Buffer<int>> result = asc::Buffer<int>::Allocate(2);
  if (!result.ok()) {
    return -1;
  }
  asc::Buffer<int> buffer = std::move(result).value();
  asc::Result<int*> data = buffer.HostData();
  if (!data.ok()) {
    return -1;
  }
  data.value()[0] = 2;
  data.value()[1] = 3;
  return data.value()[0] + data.value()[1];
}
