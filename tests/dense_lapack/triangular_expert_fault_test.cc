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
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_condition.h"
#include "asc/dense/providers/lapack_triangular_error_bounds.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "triangular_expert_faults.h"

namespace {
using asc_triangular_expert_test::Fault;
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
        std::fprintf(stderr, "Triangular expert fault: %s\n", message);
      }
      ++failures;
    }
  }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Triangular expert faults: %zu profiles, %zu checks, %zu failures; "
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
  std::array<T, 18> packing;
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
      const auto& data = kind == kScalar ? scalar : packing;
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

template <typename T, typename Call>
void Injected(Checks& checks, std::size_t routine, const Profile& profile,
              const asc::LapackWorkspacePlan& plan, Workspace<T>& work,
              asc::LapackReport& report, const Call& call,
              const installed_internal::Matrix<T, 2, 2>& a,
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
    const auto calls = asc_triangular_expert_test::Calls(routine);
    asc_triangular_expert_test::Arm(routine, fault);
    std::size_t cpp_allocations = 0;
    asc_lapack_test::BeginAllocationAudit();
    const auto status = [&]() {
      const asc_dense_test::AllocationProbe probe;
      auto value = call(plan, work.value);
      cpp_allocations = probe.count();
      return value;
    }();
    const auto foreign_allocations = asc_lapack_test::EndAllocationAudit();
    asc_triangular_expert_test::Disarm();
    checks.Expect(
        asc_test::ProcessAllocationCountMatches(cpp_allocations, 0) &&
            foreign_allocations == 0,
        "no observed C++/linked-static allocator call during execution");
    checks.Expect(asc_triangular_expert_test::Calls(routine) == calls + 1 &&
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

template <typename T, typename Call>
void Rejected(Checks& checks, std::size_t routine,
              const asc::LapackWorkspacePlan& plan, Workspace<T>& work,
              asc::LapackReport& report, const Call& call,
              std::array<asc::DenseBlasRealType<T>, 2>& ferr,
              std::array<asc::DenseBlasRealType<T>, 2>& berr,
              asc::DenseBlasRealType<T>& rcond) {
  using Real = asc::DenseBlasRealType<T>;
  for (const int rejection : {0, 1, 2}) {
    auto invalid_plan = plan;
    auto invalid_work = work.value;
    if (rejection == 0) {
      ++invalid_plan.regions[kScalar].minimum_entries;
    }
    if (rejection == 1) {
      invalid_work.regions[kScalar] = {nullptr, 0, kHost};
    }
    if (rejection == 2) {
      invalid_work.regions[static_cast<std::size_t>(
          asc::LapackWorkspaceKind::kScratch)] = {&report, sizeof(report),
                                                  kHost};
    }
    report.native_info = 71;
    const auto calls = asc_triangular_expert_test::Calls(routine);
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
        !status.ok() && asc_triangular_expert_test::Calls(routine) == calls,
        "invalid plan/capacity/metadata alias prevents provider entry");
    checks.Expect(rejection == 2
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
  installed_internal::Matrix<T, 2, 2> a{{}, profile.al};
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
  a.At(upper ? 0 : 1, upper ? 1 : 0) = Value<T>(-0.25, 0.125);
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
      return asc::QueryTrconWorkspace(provider, profile.norm, profile.triangle,
                                      profile.diagonal, a.ConstView(), rcond);
    }
    return asc::QueryTrrfsWorkspace(
        provider, profile.triangle, profile.diagonal, profile.operation,
        a.ConstView(), b.ConstView(), x.ConstView(),
        installed_internal::Vector(ferr), installed_internal::Vector(berr));
  };
  const auto plan = Take(query());
  Workspace<T> work(plan);
  asc::LapackReport report;
  const auto call = [&](const asc::LapackWorkspacePlan& supplied,
                        const asc::LapackWorkspace& workspace) {
    if (profile.condition) {
      return asc::Trcon(provider, profile.norm, profile.triangle,
                        profile.diagonal, a.ConstView(), rcond, supplied,
                        workspace, report);
    }
    return asc::Trrfs(provider, profile.triangle, profile.diagonal,
                      profile.operation, a.ConstView(), b.ConstView(),
                      x.ConstView(), installed_internal::Vector(ferr),
                      installed_internal::Vector(berr), supplied, workspace,
                      report);
  };
  // The first kNone entry per routine is a real cold call in this process;
  // the second is warm. Faulted calls are separately identified below.
  Injected(checks, routine, profile, plan, work, report, call, a, b, x);
  Rejected(checks, routine, plan, work, report, call, ferr, berr, rcond);
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider,
            std::size_t scalar) {
  for (const auto al :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (const auto triangle :
         {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
      for (const auto diagonal :
           {asc::DenseBlasDiagonal::kUnit, asc::DenseBlasDiagonal::kNonUnit}) {
        for (const auto norm : {asc::LapackConditionNorm::kOne,
                                asc::LapackConditionNorm::kInfinity}) {
          Run<T>(checks, provider, 2 * scalar,
                 {true, al, al, al, triangle, diagonal,
                  asc::DenseBlasTranspose::kNone, norm});
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
                      asc::LapackConditionNorm::kOne});
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
