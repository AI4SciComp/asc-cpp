#include <asc/sparse.h>

#include <array>
#include <cstddef>
#include <span>
#include <utility>

namespace consumer {

struct Vector {
  double* data = nullptr;
  asc::extent_t size = 0;
  const void* identity = nullptr;
};

}  // namespace consumer

namespace asc {

template <>
struct ExpressionAdapter<consumer::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const consumer::Vector& vector) noexcept {
    return {vector.size};
  }
  static double Read(const consumer::Vector& vector,
                     std::span<const index_t, 1> coordinate) noexcept {
    return vector.data[static_cast<std::size_t>(coordinate[0])];
  }
  static bool MayAlias(const consumer::Vector& vector,
                       AliasToken alias) noexcept {
    return AliasToken::FromIdentity(vector.identity) == alias;
  }
};

template <>
struct ExpressionPlacementAdapter<consumer::Vector> {
  static MemorySpace Space(const consumer::Vector&) noexcept {
    return MemorySpace::kHost;
  }
};

template <>
struct WritableExpressionAdapter<consumer::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;

  static std::array<extent_t, 1> Shape(
      const consumer::Vector& vector) noexcept {
    return {vector.size};
  }
  static AliasToken Alias(const consumer::Vector& vector) noexcept {
    return AliasToken::FromIdentity(vector.identity);
  }
  static void Write(consumer::Vector& vector,
                    std::span<const index_t, 1> coordinate,
                    double value) noexcept {
    vector.data[static_cast<std::size_t>(coordinate[0])] = value;
  }
};

}  // namespace asc

int main() {
  using MatrixExtents = asc::Extents<2, 2>;
  auto extents = MatrixExtents::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource coordinate_resource;
  auto builder = asc::CoordinateBuilder<double, MatrixExtents>::Create(
      *extents, 3, coordinate_resource);
  if (!builder.ok() ||
      !builder->Add(std::array<asc::index_t, 2>{0, 0}, 2.0).ok() ||
      !builder->Add(std::array<asc::index_t, 2>{0, 1}, 1.0).ok() ||
      !builder->Add(std::array<asc::index_t, 2>{1, 1}, 3.0).ok()) {
    return 2;
  }
  auto coordinate = builder->Finalize(asc::ExecutionContext::Serial(),
                                      asc::DuplicatePolicy::kReject,
                                      asc::ExplicitZeroPolicy::kKeep);
  if (!coordinate.ok()) {
    return 3;
  }
  auto coordinate_view = coordinate->view();
  asc::HostMemoryResource csr_resource;
  auto csr = asc::ToCsr(asc::ExecutionContext::Serial(), *coordinate_view,
                        csr_resource);
  if (!csr.ok()) {
    return 4;
  }
  auto matrix = csr->view();

  std::array<double, 2> input_values = {4.0, 5.0};
  std::array<double, 2> output_values = {-1.0, -1.0};
  int input_identity = 0;
  int output_identity = 0;
  const consumer::Vector input = {input_values.data(), 2, &input_identity};
  consumer::Vector output = {output_values.data(), 2, &output_identity};
  const asc::Status status = asc::Spmv(asc::ExecutionContext::Serial(), 1.0,
                                       *matrix, input, 0.0, output);
  if (!status.ok() || output_values != std::array<double, 2>{13.0, 15.0}) {
    return 5;
  }
  return 0;
}
