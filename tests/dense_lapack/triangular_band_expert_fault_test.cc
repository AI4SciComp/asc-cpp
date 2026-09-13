#include <algorithm>
#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <span>
#include <type_traits>

#include "../allocation_observation.h"
#include "allocation_audit.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_band_condition.h"
#include "asc/dense/providers/lapack_triangular_band_error_bounds.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "triangular_band_expert_faults.h"

namespace {
using asc_triangular_band_expert_test::Fault;
using installed_internal::Take;
using installed_internal::Value;
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

struct Checks {
  std::size_t profiles = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value, const char* message) {
    ++checks;
    if (!value) {
      if (failures < 30) {
        std::fprintf(stderr, "Triangular band expert fault: %s\n", message);
      }
      ++failures;
    }
  }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Triangular band expert faults: %zu profiles, %zu checks, %zu "
        "failures; "
        "exact C++ observation=%d\n",
        profiles, checks, failures,
        static_cast<int>(asc_test::kHasExactProcessAllocationObservation));
    return failures == 0 ? 0 : 1;
  }
};

template <typename T, std::size_t N>
bool Bytes(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto x = std::as_bytes(std::span(a));
  const auto y = std::as_bytes(std::span(b));
  return std::equal(x.begin(), x.end(), y.begin(), y.end());
}

template <typename T>
struct Workspace {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 18> scalar;
  std::array<Real, 18> real;
  std::array<T, 34> packing;
  alignas(std::max_align_t) std::array<std::byte, 64> integer;
  asc::LapackWorkspace value;
  explicit Workspace(const asc::LapackWorkspacePlan& plan) {
    scalar.fill(Value<T>(-4000, 1000));
    real.fill(Real{-4000});
    packing.fill(Value<T>(-4000, 1000));
    integer.fill(std::byte{0x5a});
    for (const auto kind : {kScalar, kReal, kInteger, kLayout}) {
      const auto& region = plan.regions[kind];
      if (region.minimum_entries == 0) {
        continue;
      }
      void* pointer = nullptr;
      if (kind == kScalar) {
        pointer = scalar.data() + 1;
      }
      if (kind == kReal) {
        pointer = real.data() + 1;
      }
      if (kind == kInteger) {
        pointer = integer.data() + 16;
      }
      if (kind == kLayout) {
        pointer = packing.data() + 1;
      }
      value.regions[kind] = {
          pointer,
          static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes,
          kHost};
    }
  }
  void Guards(Checks& checks, const asc::LapackWorkspacePlan& plan) const {
    for (const auto kind : {kScalar, kLayout}) {
      const auto data = kind == kScalar ? std::span<const T>(scalar)
                                        : std::span<const T>(packing);
      const auto count =
          static_cast<std::size_t>(plan.regions[kind].minimum_entries);
      checks.Expect(
          data.front() == Value<T>(-4000, 1000) &&
              std::all_of(data.begin() + 1 + count, data.end(),
                          [](T x) { return x == Value<T>(-4000, 1000); }),
          "scalar/packing guards");
    }
    const auto reals =
        static_cast<std::size_t>(plan.regions[kReal].minimum_entries);
    checks.Expect(real.front() == Real{-4000} &&
                      std::all_of(real.begin() + 1 + reals, real.end(),
                                  [](Real x) { return x == Real{-4000}; }),
                  "real guards");
    const auto bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    checks.Expect(
        std::all_of(integer.begin(), integer.begin() + 16,
                    [](std::byte x) { return x == std::byte{0x5a}; }) &&
            std::all_of(integer.begin() + 16 + bytes, integer.end(),
                        [](std::byte x) { return x == std::byte{0x5a}; }),
        "integer guards");
  }
};

Integer ExpectedInfo(Fault fault) {
  if (fault == Fault::kNegativeInfo) {
    return -2;
  }
  if (fault == Fault::kPositiveInfo) {
    return 1;
  }
  Integer info = std::numeric_limits<Integer>::min();
  if (fault == Fault::kLowZero || fault == Fault::kLowOnes) {
    auto bytes =
        std::as_writable_bytes(std::span(&info, 1)).first(sizeof(Integer) / 2);
    std::fill(bytes.begin(), bytes.end(),
              fault == Fault::kLowZero ? std::byte{0} : std::byte{0xff});
  }
  if (fault == Fault::kNone || fault == Fault::kNegativeOutput ||
      fault == Fault::kNanOutput || fault == Fault::kInfiniteOutput ||
      fault == Fault::kNoOutput) {
    return 0;
  }
  return info;
}

struct Profile {
  bool condition;
  asc::DenseBlasLayout al;
  asc::DenseBlasLayout bl;
  asc::DenseBlasLayout xl;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasDiagonal diagonal;
  asc::DenseBlasTranspose operation;
  asc::LapackConditionNorm norm;
  asc::extent_t kd;
};

void CheckFaultReport(Checks& checks, const Profile& profile, Fault fault,
                      Integer expected, const asc::Status& status,
                      const asc::LapackReport& report) {
  if (fault == Fault::kNone) {
    checks.Expect(
        status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete,
        "real native return succeeds");
  } else if (expected != 0 || fault == Fault::kNegativeOutput) {
    checks.Expect(
        status.code() == asc::ErrorCode::kProvider &&
            report.output_validity == asc::LapackOutputValidity::kUnusable,
        "invalid INFO/negative estimate is provider defect");
    if (expected < 0 && expected != std::numeric_limits<Integer>::min()) {
      checks.Expect(
          report.native_argument == -static_cast<asc::index_t>(expected),
          "native argument translation avoids minimum negation");
    } else {
      checks.Expect(!report.native_argument, "no invented native argument");
    }
  } else {
    checks.Expect(
        status.code() == asc::ErrorCode::kNumerical &&
            report.outcome == asc::LapackOutcome::kAccuracyWarning &&
            report.output_validity ==
                asc::LapackOutputValidity::kDocumentedPartial,
        "nonfinite/missing estimates remain a visible accuracy warning");
    checks.Expect(profile.condition ? !report.diagnostic_index
                                    : report.diagnostic_index == 0,
                  "warning identifies first RHS only for error bounds");
  }
}

template <typename T>
struct BandPair {
  std::array<T, 64> data{};
  asc::DenseBlasLayout layout;
  asc::DenseBlasTriangle triangle;
  asc::extent_t kd;
  T& At(std::size_t i, std::size_t j) {
    const auto band = static_cast<std::size_t>(kd);
    const auto ld = band + 3;
    const bool column = layout == asc::DenseBlasLayout::kColumnMajor;
    const bool upper = triangle == asc::DenseBlasTriangle::kUpper;
    std::size_t slot = 0;
    if (column) {
      slot = upper ? j * ld + band + i - j : j * ld + i - j;
    } else {
      slot = upper ? i * ld + j - i : i * ld + band + j - i;
    }
    return data[slot];
  }
  auto ConstView() {
    return Take(asc::LapackTriangularBandView<const T>::Create(
        data.data(), 2, kd, triangle, layout, kd + 3,
        {data.data(), sizeof(data), kHost}));
  }
};

template <typename T, typename Call>
void Injected(Checks& checks, std::size_t routine, const Profile& profile,
              const asc::LapackWorkspacePlan& plan, Workspace<T>& work,
              asc::LapackReport& report, const Call& call, const BandPair<T>& a,
              const installed_internal::Matrix<T, 2, 2>& b,
              const installed_internal::Matrix<T, 2, 2>& x) {
  const auto before_a = a.data;
  const auto before_b = b.data;
  const auto before_x = x.data;
  for (const auto fault :
       {Fault::kNone, Fault::kNone, Fault::kNoInfo, Fault::kNegativeInfo,
        Fault::kPositiveInfo, Fault::kLowZero, Fault::kLowOnes,
        Fault::kNegativeOutput, Fault::kNanOutput, Fault::kInfiniteOutput,
        Fault::kNoOutput}) {
    const auto calls = asc_triangular_band_expert_test::Calls(routine);
    asc_triangular_band_expert_test::Arm(routine, fault);
    std::size_t cpp_allocations = 0;
    asc_lapack_test::BeginAllocationAudit();
    const auto status = [&]() {
      const asc_dense_test::AllocationProbe probe;
      auto value = call(plan, work.value);
      cpp_allocations = probe.count();
      return value;
    }();
    const auto foreign_allocations = asc_lapack_test::EndAllocationAudit();
    asc_triangular_band_expert_test::Disarm();
    checks.Expect(
        asc_test::ProcessAllocationCountMatches(cpp_allocations, 0) &&
            foreign_allocations == 0,
        "no observed C++/linked-static allocator call during execution");
    checks.Expect(
        asc_triangular_band_expert_test::Calls(routine) == calls + 1 &&
            report.called_provider,
        "exact selected native route entered once");
    const Integer expected = ExpectedInfo(fault);
    checks.Expect(report.native_info == expected,
                  "complete signed INFO survives fault");
    CheckFaultReport(checks, profile, fault, expected, status, report);
    checks.Expect(Bytes(a.data, before_a) && Bytes(b.data, before_b) &&
                      Bytes(x.data, before_x),
                  "native fault never publishes or changes immutable matrices");
    work.Guards(checks, plan);
    ++checks.profiles;
  }
}

template <std::size_t D, std::size_t O>
asc::LapackPlanIdentity ChangeKey(const asc::LapackWorkspacePlan& plan,
                                  std::array<asc::extent_t, D> dimensions,
                                  std::array<std::int64_t, O> options,
                                  int mutation) {
  auto provider = plan.identity.provider();
  auto scalar = plan.identity.scalar();
  if (mutation == -2) {
    provider.build_sha256[0] ^= std::byte{1};
  } else if (mutation == -1) {
    scalar = asc::LapackScalarKind::kMixedF64F32;
  } else {
    const auto index = static_cast<std::size_t>(mutation) % (D + O);
    if (index < D) {
      ++dimensions[index];
    } else {
      ++options[index - D];
    }
  }
  return Take(asc::LapackPlanIdentity::Create(plan.identity.routine(), scalar,
                                              dimensions, options, provider));
}
asc::LapackPlanIdentity ChangedIdentity(const Profile& profile,
                                        const asc::LapackWorkspacePlan& plan,
                                        int mutation) {
  const auto triangle = static_cast<std::int64_t>(profile.triangle);
  const auto diagonal = static_cast<std::int64_t>(profile.diagonal);
  const auto layout = static_cast<std::int64_t>(profile.al);
  const asc::extent_t ldab = profile.al == asc::DenseBlasLayout::kRowMajor
                                 ? profile.kd + 1
                                 : profile.kd + 3;
  if (profile.condition) {
    return ChangeKey(plan, std::array<asc::extent_t, 3>{2, profile.kd, ldab},
                     std::array{static_cast<std::int64_t>(profile.norm),
                                triangle, diagonal, layout, profile.kd + 3},
                     mutation);
  }
  const asc::extent_t ldb =
      profile.bl == asc::DenseBlasLayout::kRowMajor ? 2 : 3;
  const asc::extent_t ldx =
      profile.xl == asc::DenseBlasLayout::kRowMajor ? 2 : 3;
  return ChangeKey(
      plan, std::array<asc::extent_t, 6>{2, profile.kd, 2, ldab, ldb, ldx},
      std::array<std::int64_t, 9>{
          triangle, diagonal, static_cast<std::int64_t>(profile.operation),
          layout, static_cast<std::int64_t>(profile.bl),
          static_cast<std::int64_t>(profile.xl), profile.kd + 3, 3, 3},
      mutation);
}

asc::ErrorCode ConfigureWorkspaceRejection(int rejection,
                                           const Profile& profile,
                                           asc::LapackWorkspacePlan& plan,
                                           asc::LapackWorkspace& workspace,
                                           asc::LapackReport& report) {
  constexpr auto kScratch =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
  const auto scalar = workspace.regions[kScalar];
  const auto auxiliary =
      plan.regions[kReal].minimum_entries != 0 ? kReal : kInteger;
  const auto secondary = workspace.regions[auxiliary];
  switch (rejection) {
    case 0:
      ++plan.regions[kScalar].minimum_entries;
      return asc::ErrorCode::kInvalidState;
    case 1:
      workspace.regions[kScalar] = {nullptr, 0, kHost};
      break;
    case 2:
      workspace.regions[kScratch] = {&report, sizeof(report), kHost};
      break;
    case 3:
      workspace.regions[kScalar] = {static_cast<std::byte*>(scalar.data()) + 1,
                                    scalar.size(), kHost};
      break;
    case 4:
      workspace.regions[kScratch] = scalar;
      break;
    case 5:
      // The tagged range is live host storage. This tests rejection only,
      // without claiming a real CUDA allocation or a host read of device data.
      workspace.regions[kScalar] = {scalar.data(), scalar.size(),
                                    asc::MemorySpace::kDevice};
      return asc::ErrorCode::kMemoryAccess;
    case 6:
      workspace.regions[auxiliary] = {secondary.data(), secondary.size() - 1,
                                      kHost};
      break;
    case 7:
      workspace.regions[auxiliary] = {
          static_cast<std::byte*>(secondary.data()) + 1, secondary.size(),
          kHost};
      break;
    case 8:
      workspace.regions[kScratch] = {&plan, sizeof(plan), kHost};
      break;
    case 9:
      workspace.regions[kScratch] = {&workspace, sizeof(workspace), kHost};
      break;
    case 10:
      plan.total_byte_limit = 0;
      return asc::ErrorCode::kInvalidState;
    case 11:
    case 12:
      plan.identity = ChangedIdentity(profile, plan, rejection == 12 ? -1 : -2);
      return asc::ErrorCode::kInvalidState;
    default:
      if (rejection >= 13 && rejection < 28) {
        plan.identity = ChangedIdentity(profile, plan, rejection - 13);
        return asc::ErrorCode::kInvalidState;
      }
      return asc::ErrorCode::kInternal;
  }
  return asc::ErrorCode::kInvalidArgument;
}

template <typename T, typename Call>
void Rejected(Checks& checks, std::size_t routine, const Profile& profile,
              const asc::LapackWorkspacePlan& plan, Workspace<T>& work,
              asc::LapackReport& report, const Call& call,
              std::array<asc::DenseBlasRealType<T>, 2>& ferr,
              std::array<asc::DenseBlasRealType<T>, 2>& berr,
              asc::DenseBlasRealType<T>& rcond) {
  using Real = asc::DenseBlasRealType<T>;
  for (int rejection = 0; rejection < 28; ++rejection) {
    auto invalid_plan = plan;
    auto invalid_work = work.value;
    const auto expected = ConfigureWorkspaceRejection(
        rejection, profile, invalid_plan, invalid_work, report);
    report.native_info = 71;
    const auto calls = asc_triangular_band_expert_test::Calls(routine);
    const auto old_scalar = work.scalar;
    const auto old_real = work.real;
    const auto old_packing = work.packing;
    const auto old_integer = work.integer;
    const auto old_ferr = ferr;
    const auto old_berr = berr;
    const auto old_rcond =
        std::bit_cast<std::array<std::byte, sizeof(Real)>>(rcond);
    const auto status = call(invalid_plan, invalid_work);
    checks.Expect(
        !status.ok() &&
            asc_triangular_band_expert_test::Calls(routine) == calls,
        "invalid plan/capacity/metadata alias prevents provider entry");
    checks.Expect(status.code() == expected, "exact structural rejection code");
    checks.Expect(rejection == 2 || rejection == 8 || rejection == 9
                      ? report.native_info == 71
                      : !report.native_info && !report.called_provider,
                  "metadata alias preserves report; other preflight resets it");
    checks.Expect(
        Bytes(work.scalar, old_scalar) && Bytes(work.real, old_real) &&
            Bytes(work.packing, old_packing) &&
            Bytes(work.integer, old_integer) && Bytes(ferr, old_ferr) &&
            Bytes(berr, old_berr) &&
            old_rcond ==
                std::bit_cast<std::array<std::byte, sizeof(Real)>>(rcond),
        "preflight preserves all output/workspace bytes");
    ++checks.profiles;
  }
}

template <typename T>
void Run(Checks& checks, const asc::ReferenceLapackProvider& provider,
         std::size_t routine, const Profile& profile) {
  using Real = asc::DenseBlasRealType<T>;
  BandPair<T> a{{}, profile.al, profile.triangle, profile.kd};
  installed_internal::Matrix<T, 2, 2> b{{}, profile.bl};
  installed_internal::Matrix<T, 2, 2> x{{}, profile.xl};
  a.data.fill(Value<T>(std::numeric_limits<double>::quiet_NaN()));
  b.data.fill(Value<T>(64));
  x.data.fill(Value<T>(128));
  const bool upper = profile.triangle == asc::DenseBlasTriangle::kUpper;
  if (profile.diagonal == asc::DenseBlasDiagonal::kNonUnit) {
    a.At(0, 0) = Value<T>(2);
    a.At(1, 1) = Value<T>(3);
  }
  if (profile.kd != 0) {
    a.At(upper ? 0 : 1, upper ? 1 : 0) = Value<T>(-0.25, 0.125);
  }
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.At(i, j) = Value<T>(1 + static_cast<double>(i + j));
      x.At(i, j) = Value<T>(2 + static_cast<double>(i + j));
    }
  }
  std::array<Real, 2> ferr{Real{-1}, Real{-1}};
  std::array<Real, 2> berr{Real{-2}, Real{-2}};
  Real rcond = Real{-3};
  const auto query = [&]() {
    if (profile.condition) {
      return asc::QueryTbconWorkspace(provider, profile.norm, profile.diagonal,
                                      a.ConstView(), rcond);
    }
    return asc::QueryTbrfsWorkspace(
        provider, profile.diagonal, profile.operation, a.ConstView(),
        b.ConstView(), x.ConstView(), installed_internal::Vector(ferr),
        installed_internal::Vector(berr));
  };
  const auto plan = Take(query());
  Workspace<T> work(plan);
  asc::LapackReport report;
  const auto call = [&](const asc::LapackWorkspacePlan& supplied,
                        const asc::LapackWorkspace& workspace) {
    if (profile.condition) {
      return asc::Tbcon(provider, profile.norm, profile.diagonal, a.ConstView(),
                        rcond, supplied, workspace, report);
    }
    return asc::Tbrfs(
        provider, profile.diagonal, profile.operation, a.ConstView(),
        b.ConstView(), x.ConstView(), installed_internal::Vector(ferr),
        installed_internal::Vector(berr), supplied, workspace, report);
  };
  // The first kNone entry per routine is a real cold call in this process;
  // the second is warm. Faulted calls are separately identified below.
  Injected(checks, routine, profile, plan, work, report, call, a, b, x);
  Rejected(checks, routine, profile, plan, work, report, call, ferr, berr,
           rcond);
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider,
            std::size_t scalar) {
  for (const asc::extent_t kd : {0, 1, 2, 4}) {
    for (const auto al : {asc::DenseBlasLayout::kColumnMajor,
                          asc::DenseBlasLayout::kRowMajor}) {
      for (const auto triangle :
           {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
        for (const auto diagonal : {asc::DenseBlasDiagonal::kUnit,
                                    asc::DenseBlasDiagonal::kNonUnit}) {
          for (const auto norm : {asc::LapackConditionNorm::kOne,
                                  asc::LapackConditionNorm::kInfinity}) {
            Run<T>(checks, provider, 2 * scalar,
                   {true, al, al, al, triangle, diagonal,
                    asc::DenseBlasTranspose::kNone, norm, kd});
          }
          for (const auto bl : {asc::DenseBlasLayout::kColumnMajor,
                                asc::DenseBlasLayout::kRowMajor}) {
            for (const auto xl : {asc::DenseBlasLayout::kColumnMajor,
                                  asc::DenseBlasLayout::kRowMajor}) {
              for (const auto operation :
                   {asc::DenseBlasTranspose::kNone,
                    asc::DenseBlasTranspose::kTranspose,
                    asc::DenseBlasTranspose::kConjugateTranspose}) {
                Run<T>(checks, provider, 2 * scalar + 1,
                       {false, al, bl, xl, triangle, diagonal, operation,
                        asc::LapackConditionNorm::kOne, kd});
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
  const asc_lapack_test::NormalReturnGuard return_guard;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider, 0);
  Scalar<double>(checks, provider, 1);
  Scalar<std::complex<float>>(checks, provider, 2);
  Scalar<std::complex<double>>(checks, provider, 3);
  return checks.Finish();
}
