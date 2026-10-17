#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <span>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_band_condition.h"
#include "asc/dense/providers/lapack_triangular_band_error_bounds.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
#include "test_support.h"
#include "triangular_band_expert_faults.h"

namespace {
using asc_dense_test::TestContext;
using installed_internal::Take;
namespace observation = asc_triangular_band_expert_test;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kDevice = asc::MemorySpace::kDevice;
constexpr auto kInvalid = asc::ErrorCode::kInvalidArgument;

template <typename T, std::size_t N>
bool Bytes(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto x = std::as_bytes(std::span(a));
  const auto y = std::as_bytes(std::span(b));
  return std::equal(x.begin(), x.end(), y.begin(), y.end());
}

template <typename T>
struct Work {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 64> scalar{};
  std::array<T, 64> packing{};
  std::array<Real, 64> real{};
  alignas(std::max_align_t) std::array<std::byte, 128> integer{};
  asc::LapackWorkspace value;
  explicit Work(const asc::LapackWorkspacePlan& plan) {
    scalar.fill(T{-17});
    packing.fill(T{-19});
    real.fill(Real{-23});
    integer.fill(std::byte{0x5a});
    for (std::size_t k = 0; k < plan.regions.size(); ++k) {
      const auto& region = plan.regions[k];
      if (region.minimum_entries == 0) {
        continue;
      }
      void* pointer = nullptr;
      using Kind = asc::LapackWorkspaceKind;
      if (k == static_cast<std::size_t>(Kind::kScalar)) {
        pointer = scalar.data();
      } else if (k == static_cast<std::size_t>(Kind::kLayoutConversion)) {
        pointer = packing.data();
      } else if (k == static_cast<std::size_t>(Kind::kReal)) {
        pointer = real.data();
      } else if (k == static_cast<std::size_t>(Kind::kInteger)) {
        pointer = integer.data();
      }
      value.regions[k] = {
          pointer,
          static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes,
          kHost};
    }
  }
  [[nodiscard]] bool Same(const Work& old) const {
    return Bytes(scalar, old.scalar) && Bytes(packing, old.packing) &&
           Bytes(real, old.real) && Bytes(integer, old.integer);
  }
};

struct Profile {
  asc::extent_t kd;
  asc::DenseBlasLayout al;
  asc::DenseBlasLayout bl;
  asc::DenseBlasLayout xl;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasDiagonal diagonal;
  asc::DenseBlasTranspose operation;
};

// These enums have fixed uint8_t underlying types. A representable unnamed
// value deliberately tests invalid public flags.
template <typename Enum>
Enum InvalidFlag() {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Enum>(99);
}

template <typename T>
class Fixture {
 public:
  using Real = asc::DenseBlasRealType<T>;
  Fixture(TestContext& test, const asc::ReferenceLapackProvider& provider,
          const Profile& profile, std::size_t scalar, std::size_t& profiles)
      : test_(test),
        provider_(provider),
        profile_(profile),
        scalar_(scalar),
        profiles_(profiles) {
    a_.fill(T{7});
    b_.fill(T{11});
    x_.fill(T{13});
    ferr_.fill(Real{-29});
    berr_.fill(Real{-31});
  }
  void Run() {
    Condition();
    Errors();
  }

 private:
  auto Band() {
    return Take(asc::LapackTriangularBandView<const T>::Create(
        a_.data(), 2, profile_.kd, profile_.triangle, profile_.al,
        profile_.kd + 3, {a_.data(), sizeof(a_), kHost}));
  }
  static auto Matrix(std::array<T, 64>& data, asc::extent_t rows,
                     asc::extent_t columns, asc::DenseBlasLayout layout,
                     asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data(), rows, columns, layout, 4,
        {data.data(), sizeof(data), space}));
  }
  static auto Vector(std::array<Real, 8>& data, asc::extent_t size = 2,
                     asc::index_t increment = 1,
                     asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        data.data(), size, increment, {data.data(), sizeof(data), space}));
  }
  // The band descriptor rejects device tags at construction, before an ASC
  // estimator query can exist. Do not forge an impossible checked descriptor.
  void DeviceBand() {
    const auto before = a_;
    const auto condition_calls = observation::Calls(2 * scalar_);
    const auto error_calls = observation::Calls(2 * scalar_ + 1);
    const asc_dense_test::AllocationProbe probe;
    const auto result = asc::LapackTriangularBandView<const T>::Create(
        a_.data(), 2, profile_.kd, profile_.triangle, profile_.al,
        profile_.kd + 3, {a_.data(), sizeof(a_), kDevice});
    ASC_DENSE_TEST_CHECK(test_, !result.ok());
    ASC_DENSE_TEST_EQ(test_, result.status().code(),
                      asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(test_, Bytes(a_, before));
    ASC_DENSE_TEST_EQ(test_, observation::Calls(2 * scalar_), condition_calls);
    ASC_DENSE_TEST_EQ(test_, observation::Calls(2 * scalar_ + 1), error_calls);
    ASC_DENSE_TEST_EQ(test_, probe.count(), std::size_t{0});
    ++profiles_;
  }
  template <typename Query, typename Call>
  void Check(const Query& query, const Call& call, asc::ErrorCode expected,
             std::size_t routine, const asc::LapackWorkspacePlan& plan) {
    Work<T> work(plan);
    const auto old_work = work;
    const auto old_a = a_;
    const auto old_b = b_;
    const auto old_x = x_;
    const auto old_ferr = ferr_;
    const auto old_berr = berr_;
    asc::LapackReport report;
    report.native_info = 71;
    const auto calls = observation::Calls(routine);
    // Count accidental entry without allowing invalid arguments into Fortran.
    observation::Arm(routine, observation::Fault::kNoInfo);
    {
      const asc_dense_test::AllocationProbe probe;
      const auto result = query();
      ASC_DENSE_TEST_CHECK(test_, !result.ok());
      ASC_DENSE_TEST_EQ(test_, result.status().code(), expected);
      const auto status = call(plan, work.value, report);
      ASC_DENSE_TEST_EQ(test_, status.code(), expected);
      ASC_DENSE_TEST_EQ(test_, probe.count(), std::size_t{0});
    }
    observation::Disarm();
    ASC_DENSE_TEST_EQ(test_, observation::Calls(routine), calls);
    ASC_DENSE_TEST_CHECK(test_, !report.native_info && !report.called_provider);
    ASC_DENSE_TEST_CHECK(test_, work.Same(old_work));
    ASC_DENSE_TEST_CHECK(
        test_, Bytes(a_, old_a) && Bytes(b_, old_b) && Bytes(x_, old_x) &&
                   Bytes(ferr_, old_ferr) && Bytes(berr_, old_berr));
    ASC_DENSE_TEST_EQ(test_, rcond_, Real{-37});
    ++profiles_;
  }
  void Condition() {
    const auto a = Band();
    for (auto norm : {asc::LapackConditionNorm::kOne,
                      asc::LapackConditionNorm::kInfinity}) {
      const auto plan = Take(asc::QueryTbconWorkspace(
          provider_, norm, profile_.diagonal, a, rcond_));
      for (int rejection = 0; rejection < 2; ++rejection) {
        const auto selected_norm =
            rejection == 0 ? InvalidFlag<asc::LapackConditionNorm>() : norm;
        const auto diagonal = rejection == 1
                                  ? InvalidFlag<asc::DenseBlasDiagonal>()
                                  : profile_.diagonal;
        Check(
            [&] {
              return asc::QueryTbconWorkspace(provider_, selected_norm,
                                              diagonal, a, rcond_);
            },
            [&](const auto& p, const auto& w, auto& report) {
              return asc::Tbcon(provider_, selected_norm, diagonal, a, rcond_,
                                p, w, report);
            },
            kInvalid, 2 * scalar_, plan);
      }
      DeviceBand();
    }
  }
  struct Inputs {
    asc::DenseBlasMatrixView<const T> b;
    asc::DenseBlasMatrixView<const T> x;
    asc::DenseBlasVectorView<Real> ferr;
    asc::DenseBlasVectorView<Real> berr;
    asc::DenseBlasDiagonal diagonal;
    asc::DenseBlasTranspose operation;
    asc::ErrorCode expected = kInvalid;
  };
  void Dimensions(int rejection, Inputs& inputs) {
    if (rejection == 0) {
      inputs.diagonal = InvalidFlag<asc::DenseBlasDiagonal>();
    }
    if (rejection == 1) {
      inputs.operation = InvalidFlag<asc::DenseBlasTranspose>();
    }
    if (rejection == 2) {
      inputs.b = Matrix(b_, 1, 2, profile_.bl);
    }
    if (rejection == 3) {
      inputs.x = Matrix(x_, 1, 2, profile_.xl);
    }
    if (rejection == 4) {
      inputs.x = Matrix(x_, 2, 1, profile_.xl);
    }
    if (rejection == 5) {
      inputs.ferr = Vector(ferr_, 1);
    }
    if (rejection == 6) {
      inputs.berr = Vector(berr_, 1);
    }
    if (rejection == 7) {
      inputs.ferr = Vector(ferr_, 2, 2);
    }
    if (rejection == 8) {
      inputs.berr = Vector(berr_, 2, 2);
    }
    if (rejection >= 2 && rejection <= 6) {
      inputs.expected = asc::ErrorCode::kShape;
    }
  }
  void PlacementAndAliases(int rejection, Inputs& inputs) {
    if (rejection == 10) {
      inputs.b = Matrix(b_, 2, 2, profile_.bl, kDevice);
    }
    if (rejection == 11) {
      inputs.x = Matrix(x_, 2, 2, profile_.xl, kDevice);
    }
    if (rejection == 12) {
      inputs.ferr = Vector(ferr_, 2, 1, kDevice);
    }
    if (rejection == 13) {
      inputs.berr = Vector(berr_, 2, 1, kDevice);
    }
    if (rejection == 14) {
      inputs.b = Matrix(a_, 2, 2, profile_.bl);
    }
    if (rejection == 15) {
      inputs.x = Matrix(a_, 2, 2, profile_.xl);
    }
    if (rejection == 16) {
      inputs.x = Matrix(b_, 2, 2, profile_.xl);
    }
    if (rejection == 17) {
      inputs.berr = Vector(ferr_);
    }
    if (rejection == 18) {
      inputs.ferr = Vector(berr_);
    }
    if (rejection >= 10 && rejection <= 13) {
      inputs.expected = asc::ErrorCode::kMemoryAccess;
    }
  }
  void Errors() {
    const auto a = Band();
    const Inputs valid{Matrix(b_, 2, 2, profile_.bl),
                       Matrix(x_, 2, 2, profile_.xl),
                       Vector(ferr_),
                       Vector(berr_),
                       profile_.diagonal,
                       profile_.operation};
    const auto plan = Take(
        asc::QueryTbrfsWorkspace(provider_, valid.diagonal, valid.operation, a,
                                 valid.b, valid.x, valid.ferr, valid.berr));
    for (int rejection = 0; rejection < 19; ++rejection) {
      if (rejection == 9) {
        DeviceBand();
        continue;
      }
      auto inputs = valid;
      Dimensions(rejection, inputs);
      PlacementAndAliases(rejection, inputs);
      Check(
          [&] {
            return asc::QueryTbrfsWorkspace(provider_, inputs.diagonal,
                                            inputs.operation, a, inputs.b,
                                            inputs.x, inputs.ferr, inputs.berr);
          },
          [&](const auto& p, const auto& w, auto& report) {
            return asc::Tbrfs(provider_, inputs.diagonal, inputs.operation, a,
                              inputs.b, inputs.x, inputs.ferr, inputs.berr, p,
                              w, report);
          },
          inputs.expected, 2 * scalar_ + 1, plan);
    }
  }
  TestContext& test_;
  const asc::ReferenceLapackProvider& provider_;
  const Profile& profile_;
  std::size_t scalar_;
  std::size_t& profiles_;
  std::array<T, 64> a_;
  std::array<T, 64> b_;
  std::array<T, 64> x_;
  std::array<Real, 8> ferr_;
  std::array<Real, 8> berr_;
  Real rcond_ = Real{-37};
};

template <typename T>
void Scalar(TestContext& test, const asc::ReferenceLapackProvider& provider,
            std::size_t scalar, std::size_t& profiles) {
  for (const asc::extent_t kd : {0, 1, 4}) {
    for (auto al : {asc::DenseBlasLayout::kColumnMajor,
                    asc::DenseBlasLayout::kRowMajor}) {
      for (auto bl : {asc::DenseBlasLayout::kColumnMajor,
                      asc::DenseBlasLayout::kRowMajor}) {
        for (auto xl : {asc::DenseBlasLayout::kColumnMajor,
                        asc::DenseBlasLayout::kRowMajor}) {
          for (auto triangle : {asc::DenseBlasTriangle::kUpper,
                                asc::DenseBlasTriangle::kLower}) {
            for (auto diagonal : {asc::DenseBlasDiagonal::kUnit,
                                  asc::DenseBlasDiagonal::kNonUnit}) {
              for (auto op : {asc::DenseBlasTranspose::kNone,
                              asc::DenseBlasTranspose::kTranspose,
                              asc::DenseBlasTranspose::kConjugateTranspose}) {
                const Profile profile{kd, al, bl, xl, triangle, diagonal, op};
                Fixture<T> fixture(test, provider, profile, scalar, profiles);
                fixture.Run();
              }
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  std::size_t profiles = 0;
  Scalar<float>(test, provider, 0, profiles);
  Scalar<double>(test, provider, 1, profiles);
  Scalar<std::complex<float>>(test, provider, 2, profiles);
  Scalar<std::complex<double>>(test, provider, 3, profiles);
  ASC_DENSE_TEST_EQ(test, profiles, std::size_t{28800});
  std::printf("Triangular band expert operand preflight profiles: %zu\n",
              profiles);
  return test.Finish();
}
