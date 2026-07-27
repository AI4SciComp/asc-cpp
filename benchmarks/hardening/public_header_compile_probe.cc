#include <cstddef>
#include <type_traits>

#include "asc/core.h"
#include "asc/dense.h"
#include "asc/expression.h"
#include "asc/random.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "asc/sparse.h"
#include "asc/utilities.h"

namespace {

using Shape = asc::Extents<asc::kDynamicExtent, 3>;
using DenseOwner = asc::DenseArray<double, Shape>;
using SparseOwner = asc::CoordinateArray<double, Shape>;

static_assert(!std::is_copy_constructible_v<DenseOwner>);
static_assert(!std::is_copy_constructible_v<SparseOwner>);
static_assert(std::is_trivially_copyable_v<asc::DenseView<double, 2>>);
static_assert(std::is_trivially_copyable_v<asc::CoordinateView<double, 2>>);

}  // namespace

extern "C" std::size_t AscCppM8PublicHeaderObjectProbe() {
  return sizeof(asc::Status) + sizeof(asc::ExecutionContext) +
         sizeof(asc::DenseView<double, 2>) +
         sizeof(asc::CoordinateView<double, 2>) + sizeof(asc::CsrView<double>) +
         sizeof(asc::Philox4x32Counter);
}
