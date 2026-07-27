#include <asc/sparse/providers/cuda.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<asc::SparseCudaContext>);
static_assert(std::is_nothrow_move_constructible_v<asc::SparseCudaContext>);

int main() {
  const auto context =
      asc::SparseCudaContext::Create(asc::ExecutionContext::Serial());
  return context.ok() ? 1 : 0;
}
