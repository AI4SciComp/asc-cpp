#ifndef ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack_tridiagonal.h"

namespace asc_tridiagonal_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = asc::DenseBlasTranspose::kConjugateTranspose;
constexpr std::size_t kCapacity = 64;
constexpr std::size_t kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr std::size_t kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr std::size_t kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr std::size_t kPivot =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kPivotConversion);
constexpr std::size_t kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr std::size_t kScratch =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Unexpected setup error %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
T Value(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
Wide ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

template <typename T>
T Narrow(Wide value) {
  return Value<T>(value.real(), value.imag());
}

inline bool EqualBytes(const void* a, const void* b, std::size_t bytes) {
  return std::memcmp(a, b, bytes) == 0;
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return result;
}

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& data, asc::extent_t n) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data() + 1, n, 1, {data.data(), sizeof(data), kHost}));
}

template <typename T, std::size_t Size>
auto Vector(const std::array<T, Size>& data, asc::extent_t n) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      data.data() + 1, n, 1, {data.data(), sizeof(data), kHost}));
}

template <std::size_t Size>
auto Pivots(const std::array<asc::index_t, Size>& data, asc::extent_t n) {
  return Take(asc::ReferenceTridiagonalPivotView::Create(Vector(data, n)));
}

template <typename T>
struct Tri {
  std::array<T, kCapacity + 2> lower{};
  std::array<T, kCapacity + 2> diagonal{};
  std::array<T, kCapacity + 2> upper{};
  std::array<T, kCapacity + 2> second{};
  asc::extent_t order;
  explicit Tri(asc::extent_t n) : order(n) {
    lower.fill(Value<T>(-101, 7));
    diagonal.fill(Value<T>(-103, 9));
    upper.fill(Value<T>(-105, 11));
    second.fill(Value<T>(-107, 13));
  }
  [[nodiscard]] auto View() {
    return Take(asc::LapackTridiagonalView<T>::Create(
        Vector(lower, std::max<asc::extent_t>(0, order - 1)),
        Vector(diagonal, order),
        Vector(upper, std::max<asc::extent_t>(0, order - 1))));
  }
  [[nodiscard]] auto View() const {
    return Take(asc::LapackTridiagonalView<const T>::Create(
        Vector(lower, std::max<asc::extent_t>(0, order - 1)),
        Vector(diagonal, order),
        Vector(upper, std::max<asc::extent_t>(0, order - 1))));
  }
  [[nodiscard]] auto Factors() {
    return Take(asc::LapackTridiagonalLuStorage<T>::Create(
        View(), Vector(second, std::max<asc::extent_t>(0, order - 2))));
  }
  [[nodiscard]] auto Factors() const {
    return Take(asc::LapackTridiagonalLuStorage<const T>::Create(
        View(), Vector(second, std::max<asc::extent_t>(0, order - 2))));
  }
  void Initialize(int mode, long double scale) {
    for (asc::extent_t i = 0; i < order; ++i) {
      const auto index = static_cast<std::size_t>(i + 1);
      diagonal[index] = mode == 0 ? Value<T>((4 + i % 3) * scale, 0.25L * scale)
                                  : Value<T>(0.125L * scale, 0.0625L * scale);
      if (mode == 2) {
        diagonal[index] = Value<T>((1 + i) * scale);
      }
      if (i + 1 < order) {
        lower[index] =
            mode == 2 ? T{}
                      : Value<T>((mode == 0 ? -0.5L : 2.0L + i % 2) * scale,
                                 0.125L * scale);
        upper[index] = mode == 2 ? T{}
                                 : Value<T>((mode == 0 ? 0.25L : 0.75L) * scale,
                                            -0.25L * scale);
      }
    }
  }
  [[nodiscard]] Wide At(asc::extent_t i, asc::extent_t j) const {
    if (i == j) {
      return ToWide(diagonal[static_cast<std::size_t>(i + 1)]);
    }
    if (i == j + 1) {
      return ToWide(lower[static_cast<std::size_t>(i)]);
    }
    if (j == i + 1) {
      return ToWide(upper[static_cast<std::size_t>(i + 1)]);
    }
    return {};
  }
  [[nodiscard]] Wide Op(asc::extent_t i, asc::extent_t j,
                        asc::DenseBlasTranspose transpose) const {
    if (transpose == kNone) {
      return At(i, j);
    }
    const Wide value = At(j, i);
    return transpose == kConjugate ? std::conj(value) : value;
  }
  void Guards(TestContext& test) const {
    const auto check = [&](const auto& buffer, asc::extent_t n, T sentinel) {
      ASC_DENSE_TEST_EQ(test, buffer.front(), sentinel);
      for (std::size_t i = static_cast<std::size_t>(n + 1); i < buffer.size();
           ++i) {
        ASC_DENSE_TEST_EQ(test, buffer[i], sentinel);
      }
    };
    check(lower, std::max<asc::extent_t>(0, order - 1), Value<T>(-101, 7));
    check(diagonal, order, Value<T>(-103, 9));
    check(upper, std::max<asc::extent_t>(0, order - 1), Value<T>(-105, 11));
    check(second, std::max<asc::extent_t>(0, order - 2), Value<T>(-107, 13));
  }
};

template <typename T>
struct Rhs {
  std::array<T, 1024> data{};
  asc::extent_t rows;
  asc::extent_t columns;
  asc::DenseBlasLayout layout;
  asc::extent_t ld;
  Rhs(asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout format)
      : rows(n),
        columns(nrhs),
        layout(format),
        ld((format == kRow ? nrhs : n) + 3) {
    data.fill(Value<T>(-109, 15));
  }
  [[nodiscard]] auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data() + 1, rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] auto View() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data() + 1, rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(1 +
                                    (layout == kRow ? i * ld + j : j * ld + i));
  }
  T& At(asc::extent_t i, asc::extent_t j) { return data[Index(i, j)]; }
  [[nodiscard]] T At(asc::extent_t i, asc::extent_t j) const {
    return data[Index(i, j)];
  }
  void Guards(TestContext& test) const {
    std::array<bool, 1024> used{};
    for (asc::extent_t j = 0; j < columns; ++j) {
      for (asc::extent_t i = 0; i < rows; ++i) {
        used[Index(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < data.size(); ++i) {
      if (!used[i]) {
        ASC_DENSE_TEST_EQ(test, data[i], Value<T>(-109, 15));
      }
    }
  }
};

template <typename T>
Wide Exact(asc::extent_t i, asc::extent_t j) {
  return ToWide(
      Value<T>(1 + 0.125L * i - 0.0625L * j, 0.25L + 0.0625L * i + 0.125L * j));
}

template <typename T>
void RightHandSides(const Tri<T>& matrix, asc::DenseBlasTranspose transpose,
                    Rhs<T>& rhs) {
  for (asc::extent_t j = 0; j < rhs.columns; ++j) {
    for (asc::extent_t i = 0; i < matrix.order; ++i) {
      Wide sum{};
      for (asc::extent_t k = 0; k < matrix.order; ++k) {
        sum += matrix.Op(i, k, transpose) * Exact<T>(k, j);
      }
      rhs.At(i, j) = Narrow<T>(sum);
    }
  }
}

template <typename T>
long double Residual(const Tri<T>& matrix, asc::DenseBlasTranspose transpose,
                     const Rhs<T>& input, const Rhs<T>& computed) {
  long double result = 0;
  for (asc::extent_t j = 0; j < input.columns; ++j) {
    for (asc::extent_t i = 0; i < matrix.order; ++i) {
      Wide sum = -ToWide(input.At(i, j));
      long double scale = std::abs(ToWide(input.At(i, j)));
      for (asc::extent_t k = 0; k < matrix.order; ++k) {
        const Wide term =
            matrix.Op(i, k, transpose) * ToWide(computed.At(k, j));
        sum += term;
        scale += std::abs(term);
      }
      result =
          std::max(result, scale == 0 ? std::abs(sum) : std::abs(sum) / scale);
    }
  }
  return result;
}

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 512> scalar{}, spare{};
  std::array<Real, 256> real{};
  std::array<T, 4096> packed{};
  alignas(16) std::array<std::byte, 768> integer{}, pivot{};
  Scratch() {
    scalar.fill(Value<T>(-111, 17));
    spare.fill(Value<T>(-113, 19));
    real.fill(Real{-115});
    packed.fill(Value<T>(-117, 21));
    integer.fill(std::byte{0x5a});
    pivot.fill(std::byte{0x6b});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    for (const auto role :
         {kScalar, kReal, kInteger, kPivot, kLayout, kScratch}) {
      const auto& region = plan.regions[role];
      const auto bytes = static_cast<std::size_t>(region.preferred_entries) *
                         region.entry_bytes;
      if (bytes == 0) {
        continue;
      }
      void* pointer = nullptr;
      std::size_t capacity = 0;
      if (role == kScalar) {
        pointer = scalar.data() + 1;
        capacity = (scalar.size() - 2) * sizeof(T);
      } else if (role == kReal) {
        pointer = real.data() + 1;
        capacity = (real.size() - 2) * sizeof(Real);
      } else if (role == kInteger || role == kPivot) {
        auto& data = role == kInteger ? integer : pivot;
        pointer = data.data() + 16;
        capacity = data.size() - 32;
      } else if (role == kLayout) {
        pointer = packed.data() + 1;
        capacity = (packed.size() - 2) * sizeof(T);
      } else {
        pointer = spare.data() + 1;
        capacity = (spare.size() - 2) * sizeof(T);
      }
      if (bytes > capacity) {
        std::abort();
      }
      workspace.regions[role] = {pointer, bytes, kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    const auto check = [&](const auto& data, std::size_t role, auto sentinel) {
      ASC_DENSE_TEST_EQ(test, data.front(), sentinel);
      for (std::size_t i =
               1 + workspace.regions[role].size() / sizeof(sentinel);
           i < data.size(); ++i) {
        ASC_DENSE_TEST_EQ(test, data[i], sentinel);
      }
    };
    check(scalar, kScalar, Value<T>(-111, 17));
    check(spare, kScratch, Value<T>(-113, 19));
    check(real, kReal, Real{-115});
    check(packed, kLayout, Value<T>(-117, 21));
    for (const auto role : {kInteger, kPivot}) {
      const auto& data = role == kInteger ? integer : pivot;
      const auto sentinel =
          role == kInteger ? std::byte{0x5a} : std::byte{0x6b};
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i < 16 || i >= 16 + workspace.regions[role].size()) {
          ASC_DENSE_TEST_EQ(test, data[i], sentinel);
        }
      }
    }
  }
};

}  // namespace asc_tridiagonal_test

#endif  // ASC_TESTS_DENSE_LAPACK_TRIDIAGONAL_TEST_SUPPORT_H_
