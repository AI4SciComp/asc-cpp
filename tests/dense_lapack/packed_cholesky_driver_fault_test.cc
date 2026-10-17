#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "../allocation_observation.h"
#include "allocation_audit.h"
#include "allocation_probe.h"
#include "asc/core/contracts.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_driver.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "packed_cholesky_driver_faults.h"

namespace {
namespace faults = asc_packed_cholesky_driver_test;
using faults::Fault;
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

struct Checks {
  std::size_t checks = 0;
  std::size_t failures = 0;
  std::size_t profiles = 0;
  void Expect(bool condition, const char* message) {
    ++checks;
    if (!condition) {
      if (failures < 30) {
        std::fprintf(stderr, "Packed Cholesky driver failure: %s\n", message);
      }
      ++failures;
    }
  }
};

struct Profile {
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout b_layout;
  asc::extent_t nrhs;
};

template <typename T, std::size_t N>
bool Same(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto x = std::as_bytes(std::span(a));
  const auto y = std::as_bytes(std::span(b));
  return std::equal(x.begin(), x.end(), y.begin(), y.end());
}

template <typename T>
constexpr std::size_t Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return 0;
  } else if constexpr (std::is_same_v<T, double>) {
    return 1;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return 2;
  } else {
    return 3;
  }
}

template <typename T>
auto ExpectedName() {
  constexpr std::array<std::string_view, 4> kNames{"sppsv", "dppsv", "cppsv",
                                                   "zppsv"};
  decltype(asc::LapackReport{}.routine) result{};
  const auto name = kNames[Routine<T>()];
  std::copy(name.begin(), name.end(), result.begin());
  return result;
}

std::size_t TotalCalls() {
  std::size_t result = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    result += faults::Calls(i);
  }
  return result;
}

template <typename T>
struct Fixture {
  std::array<T, 9> a;
  std::array<T, 10> b;
  std::array<T, 16> packing;
  asc::extent_t rows = 2;
  asc::MemorySpace a_space = kHost;
  asc::MemorySpace b_space = kHost;
  Fixture() {
    a.fill(T{-91});
    a[1] = Value<T>(2, 0.25);
    a[2] = Value<T>(0.5, 0.125);
    a[3] = Value<T>(3, -0.25);
    b.fill(T{19});
    packing.fill(T{-97});
  }
  auto A(const Profile& p) {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        a.data() + 1, 2, p.a_layout, {a.data(), sizeof(a), a_space}));
  }
  auto B(const Profile& p) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        b.data() + 1, rows, p.nrhs, p.b_layout, 3,
        {b.data(), sizeof(b), b_space}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider, const Profile& p) {
    return asc::QueryPpsvWorkspace(provider, p.triangle, A(p), B(p));
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    const auto count = plan.regions[kPacking].minimum_entries;
    if (count != 0) {
      result.regions[kPacking] = {packing.data() + 1,
                                  static_cast<std::size_t>(count) * sizeof(T),
                                  kHost};
    }
    return result;
  }
  auto Call(const asc::ReferenceLapackProvider& provider, const Profile& p,
            const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
    return asc::Ppsv(provider, p.triangle, A(p), B(p), plan, workspace, report);
  }
};

std::int64_t Raw(Fault fault) {
  if (fault == Fault::kNegative) {
    return -2;
  }
  if (fault == Fault::kImpossible) {
    return 3;
  }
  if (fault == Fault::kPositive) {
    return 2;
  }
  const auto minimum =
      static_cast<std::int64_t>(std::numeric_limits<Integer>::min());
  return fault == Fault::kLowOnes
             ? minimum +
                   static_cast<std::int64_t>(
                       (std::uint64_t{1} << (ASC_LAPACK_INTEGER_BITS / 2)) - 1)
             : minimum;
}

template <typename T>
void Inject(Checks& checks, const asc::ReferenceLapackProvider& provider,
            const Profile& p, Fault fault) {
  Fixture<T> sample;
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, p));
  const auto workspace = sample.Workspace(plan);
  const auto calls = TotalCalls();
  asc::LapackReport report;
  faults::Arm(Routine<T>(), fault);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, p, plan, workspace, report);
  const auto cpp = allocations.count();
  const auto libc = asc_lapack_test::EndAllocationAudit();
  faults::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp, 0) && libc == 0,
                "injected execution allocates nothing observed");
  checks.Expect(TotalCalls() == calls + 1, "one selected native entry");
  checks.Expect(report.routine == ExpectedName<T>() &&
                    report.provider == provider.identity(),
                "exact injected routine/provider identity");
  const bool positive = fault == Fault::kPositive;
  checks.Expect(report.called_provider && report.native_info == Raw(fault),
                "exact surviving native INFO");
  if (positive) {
    checks.Expect(
        status.code() == asc::ErrorCode::kNumerical &&
            report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
            report.output_validity ==
                asc::LapackOutputValidity::kDocumentedPartial &&
            report.diagnostic_index == 1 && !report.native_argument,
        "injected positive INFO retains partial factors andminor index");
  } else {
    checks.Expect(
        status.code() == asc::ErrorCode::kProvider &&
            report.output_validity == asc::LapackOutputValidity::kUnusable &&
            report.outcome == (Raw(fault) < 0
                                   ? asc::LapackOutcome::kProviderArgument
                                   : asc::LapackOutcome::kPartialResult) &&
            !report.diagnostic_index,
        "invalid missing orpartial INFO reports a provider defect");
    const bool argument =
        Raw(fault) < 0 && Raw(fault) != std::numeric_limits<Integer>::min();
    checks.Expect(argument ? report.native_argument == -Raw(fault)
                           : !report.native_argument,
                  "negative INFO argument conversion is safe");
  }
  checks.Expect(!report.factor_family,
                "raw partial factors carry no provenance certificate");
  const auto args = faults::LastArguments(Routine<T>());
  checks.Expect(args.triangle == (p.triangle == kUpper ? 'U' : 'L') &&
                    args.order == 2 && args.rhs == p.nrhs && args.length == 1 &&
                    args.leading == (p.nrhs <= 1 || p.b_layout == kRow ? 2 : 3),
                "exact native dimensions,triangle andhidden character length");
  auto expected_a = before.a;
  auto expected_b = before.b;
  if (positive) {
    expected_a[1] = T{-37};
    expected_a[2] = T{-38};
    expected_a[3] = T{-39};
  } else {
    if (p.a_layout == kColumn) {
      expected_a[1] = T{-37};
    }
    if (p.nrhs > 0 && p.b_layout == kColumn) {
      expected_b[1] = T{-41};
    }
  }
  checks.Expect(
      Same(sample.a, expected_a) && Same(sample.b, expected_b),
      "partial-factor publication,B preservation anddirect defect writes");
  const auto used = plan.regions[kPacking].minimum_entries;
  checks.Expect(
      sample.packing.front() == T{-97} &&
          std::all_of(sample.packing.begin() + 1 + used, sample.packing.end(),
                      [](T value) { return value == T{-97}; }),
      "scratch red zones");
  ++checks.profiles;
}

template <typename T>
void RealCalls(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& p) {
  faults::Disarm();
  for (int repeat = 0; repeat < 2; ++repeat) {
    Fixture<T> sample;
    const auto before = sample;
    const auto calls = TotalCalls();
    asc_dense_test::AllocationProbe allocations;
    asc_lapack_test::BeginAllocationAudit();
    const auto query = sample.Query(provider, p);
    const auto query_cpp = allocations.count();
    const auto query_libc = asc_lapack_test::EndAllocationAudit();
    checks.Expect(query.ok() && TotalCalls() == calls,
                  "query calls no provider");
    checks.Expect(asc_test::ProcessAllocationCountMatches(query_cpp, 0) &&
                      query_libc == 0,
                  "query has no observed C++ or wrapped C allocation");
    const auto plan = Take(query);
    const auto workspace = sample.Workspace(plan);
    asc::LapackReport report;
    asc_lapack_test::BeginAllocationAudit();
    const auto status = sample.Call(provider, p, plan, workspace, report);
    const auto cpp = allocations.count();
    const auto libc = asc_lapack_test::EndAllocationAudit();
    const bool active = true;  // N=2 factors even with zero RHS.
    checks.Expect(report.routine == ExpectedName<T>() &&
                      report.provider == provider.identity(),
                  "exact real/local routine andprovider identity");
    checks.Expect(
        asc_test::ProcessAllocationCountMatches(cpp, 0) && libc == 0,
        "initial/repeated real selected calls allocate nothing observed");
    checks.Expect(TotalCalls() == calls + (active ? 1 : 0) &&
                      report.called_provider == active,
                  "exact real or local entry count");
    checks.Expect(active ? report.native_info == 0 : !report.native_info,
                  "local INFO absent");
    checks.Expect(
        status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete,
        "native/local completion report");
    auto expected = before.b;
    for (asc::extent_t i = 0; i < 2; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        const auto slot = 1 + (p.b_layout == kColumn ? j * 3 + i : i * 3 + j);
        expected[slot] = sample.b[slot];
      }
    }
    auto expected_a = before.a;
    for (std::size_t i = 1; i <= 3; ++i) {
      expected_a[i] = sample.a[i];
    }
    checks.Expect(Same(sample.a, expected_a) && Same(sample.b, expected),
                  "factor guards,inactive RHS andpadding preserved");
    const auto args = faults::LastArguments(Routine<T>());
    checks.Expect(
        args.triangle == (p.triangle == kUpper ? 'U' : 'L') &&
            args.order == 2 && args.rhs == p.nrhs && args.length == 1 &&
            args.leading == (p.nrhs <= 1 || p.b_layout == kRow ? 2 : 3),
        "real driver exact native argument mapping");
    ++checks.profiles;
  }
}

enum class Reject : std::uint8_t {
  kTriangle,
  kShape,
  kMatrixSpace,
  kRhsSpace,
  kPlanCapacity,
  kPlanRoutine,
  kPlanScalar,
  kPlanProvider,
  kPlanOrder,
  kPlanRhs,
  kPlanLeading,
  kPlanStride,
  kTriangleIdentity,
  kAIdentity,
  kBIdentity,
  kShort,
  kAlignment,
  kWorkspaceSpace,
  kWorkspaceOverlap,
  kOperandAlias,
  kReportAlias,
  kPlanAlias,
  kWorkspaceAlias
};

void ChangeIdentity(const asc::ReferenceLapackProvider& provider,
                    const Profile& p, Reject kind,
                    asc::LapackWorkspacePlan& plan) {
  auto identity = provider.identity();
  if (kind == Reject::kPlanProvider) {
    identity.build_sha256[0] ^= std::byte{1};
  }
  const auto scalar = kind == Reject::kPlanScalar
                          ? asc::LapackScalarKind::kMixedF64F32
                          : plan.identity.scalar();
  const auto name =
      kind == Reject::kPlanRoutine ? "pptrf" : plan.identity.routine();
  const asc::extent_t ldb = p.nrhs <= 1 || p.b_layout == kRow ? 2 : 3;
  plan.identity = Take(asc::LapackPlanIdentity::Create(
      name, scalar,
      std::array<asc::extent_t, 3>{
          kind == Reject::kPlanOrder ? 1 : 2,
          p.nrhs + (kind == Reject::kPlanRhs ? 1 : 0),
          ldb + (kind == Reject::kPlanLeading ? 1 : 0)},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(p.triangle),
                                  static_cast<std::int64_t>(p.a_layout),
                                  static_cast<std::int64_t>(p.b_layout),
                                  kind == Reject::kPlanStride ? 4 : 3},
      identity));
}

struct Expected {
  asc::ErrorCode code = asc::ErrorCode::kInvalidArgument;
  bool metadata_alias = false;
};

template <typename T>
std::optional<Expected> ChangeOptions(
    const asc::ReferenceLapackProvider& provider, Fixture<T>& f, Profile& p,
    asc::LapackWorkspacePlan& plan, Reject kind) {
  Expected expected;
  switch (kind) {
    case Reject::kTriangle:
      // Fixed uint8_t enum: representable value outside the supported flags.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      p.triangle = static_cast<asc::DenseBlasTriangle>(9);
      break;
    case Reject::kShape:
      f.rows = 1;
      expected.code = asc::ErrorCode::kShape;
      break;
    case Reject::kMatrixSpace:
    case Reject::kRhsSpace:
      (kind == Reject::kMatrixSpace ? f.a_space : f.b_space) =
          asc::MemorySpace::kDevice;
      expected.code = asc::ErrorCode::kMemoryAccess;
      break;
    case Reject::kPlanCapacity:
      ++plan.regions[kPacking].minimum_entries;
      ++plan.regions[kPacking].preferred_entries;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kTriangleIdentity:
      p.triangle = p.triangle == kUpper ? kLower : kUpper;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kAIdentity:
    case Reject::kBIdentity: {
      auto& layout = kind == Reject::kAIdentity ? p.a_layout : p.b_layout;
      layout = layout == kRow ? kColumn : kRow;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    }
    case Reject::kPlanRoutine:
    case Reject::kPlanScalar:
    case Reject::kPlanProvider:
    case Reject::kPlanOrder:
    case Reject::kPlanRhs:
    case Reject::kPlanLeading:
    case Reject::kPlanStride:
      ChangeIdentity(provider, p, kind, plan);
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    default:
      return std::nullopt;
  }
  return expected;
}

template <typename T>
Expected Change(const asc::ReferenceLapackProvider& provider, Fixture<T>& f,
                Profile& p, asc::LapackWorkspacePlan& plan,
                asc::LapackWorkspace& w, asc::LapackReport& report,
                Reject kind) {
  if (auto option = ChangeOptions(provider, f, p, plan, kind)) {
    return *option;
  }
  Expected expected;
  switch (kind) {
    case Reject::kShort:
      w.regions[kPacking] = {w.regions[kPacking].data(),
                             w.regions[kPacking].size() - 1, kHost};
      break;
    case Reject::kAlignment:
      w.regions[kPacking] = {
          static_cast<std::byte*>(w.regions[kPacking].data()) + 1,
          w.regions[kPacking].size(), kHost};
      break;
    case Reject::kWorkspaceSpace:
      w.regions[0] = {f.packing.data(), sizeof(T), asc::MemorySpace::kDevice};
      expected.code = asc::ErrorCode::kMemoryAccess;
      break;
    case Reject::kWorkspaceOverlap:
      w.regions[6] = {f.packing.data(), sizeof(T), kHost};
      w.regions[7] = w.regions[6];
      break;
    case Reject::kOperandAlias:
      w.regions[0] = {f.a.data() + 1, sizeof(T), kHost};
      break;
    case Reject::kReportAlias:
      w.regions[0] = {&report, sizeof(report), kHost};
      expected.metadata_alias = true;
      break;
    case Reject::kPlanAlias:
      w.regions[0] = {&plan, sizeof(plan), kHost};
      expected.metadata_alias = true;
      break;
    case Reject::kWorkspaceAlias:
      w.regions[0] = {&w, sizeof(w), kHost};
      expected.metadata_alias = true;
      break;
    default:
      ASC_CHECK(false);
  }
  return expected;
}

template <typename T>
void Rejection(Checks& checks, const asc::ReferenceLapackProvider& provider,
               Profile p, Reject kind) {
  Fixture<T> sample;
  const auto before = sample;
  auto plan = Take(sample.Query(provider, p));
  auto workspace = sample.Workspace(plan);
  asc::LapackReport report;
  report.native_info = 777;
  report.called_provider = true;
  report.diagnostic_index = 7;
  report.native_argument = 8;
  report.outcome = asc::LapackOutcome::kAccuracyWarning;
  report.output_validity = asc::LapackOutputValidity::kDocumentedPartial;
  report.routine.fill('r');
  report.provider = provider.identity();
  report.provider.build_sha256[0] ^= std::byte{1};
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  const auto original_report = report;
  const auto expected =
      Change(provider, sample, p, plan, workspace, report, kind);
  faults::Disarm();
  const auto calls = TotalCalls();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, p, plan, workspace, report);
  const auto count = allocations.count();
  checks.Expect(asc_test::ProcessAllocationCountMatches(count, 0),
                "rejection allocates nothing observed");
  checks.Expect(status.code() == expected.code && TotalCalls() == calls,
                "exact rejection before native entry");
  checks.Expect(Same(sample.a, before.a) && Same(sample.b, before.b) &&
                    Same(sample.packing, before.packing),
                "all numeric and scratch bytes survive rejection");
  if (expected.metadata_alias) {
    checks.Expect(report.routine == original_report.routine &&
                      report.provider == original_report.provider &&
                      report.factor_family == original_report.factor_family,
                  "metadata alias preserves exact identity fields");
    checks.Expect(report.native_info == 777 && report.called_provider &&
                      report.diagnostic_index == 7 &&
                      report.native_argument == 8 &&
                      report.outcome == asc::LapackOutcome::kAccuracyWarning &&
                      report.output_validity ==
                          asc::LapackOutputValidity::kDocumentedPartial,
                  "metadata alias preserves report");
  } else {
    checks.Expect(
        !report.native_info && !report.called_provider &&
            !report.diagnostic_index && !report.native_argument &&
            report.outcome == asc::LapackOutcome::kNotRun &&
            report.output_validity == asc::LapackOutputValidity::kUnchanged &&
            report.provider == provider.identity(),
        "ordinary preflight resets report");
  }
  ++checks.profiles;
}

template <typename T>
void Preflight(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& p) {
  for (auto kind : {Reject::kTriangle,         Reject::kShape,
                    Reject::kMatrixSpace,      Reject::kRhsSpace,
                    Reject::kPlanCapacity,     Reject::kPlanRoutine,
                    Reject::kPlanScalar,       Reject::kPlanProvider,
                    Reject::kPlanOrder,        Reject::kPlanRhs,
                    Reject::kPlanLeading,      Reject::kPlanStride,
                    Reject::kTriangleIdentity, Reject::kAIdentity,
                    Reject::kBIdentity,        Reject::kWorkspaceSpace,
                    Reject::kWorkspaceOverlap, Reject::kOperandAlias,
                    Reject::kReportAlias,      Reject::kPlanAlias,
                    Reject::kWorkspaceAlias}) {
    Rejection<T>(checks, provider, p, kind);
  }
  if (p.a_layout == kRow || (p.nrhs != 0 && p.b_layout == kRow)) {
    Rejection<T>(checks, provider, p, Reject::kShort);
    Rejection<T>(checks, provider, p, Reject::kAlignment);
  }
}

template <typename T>
void AliasedOperands(Checks& checks,
                     const asc::ReferenceLapackProvider& provider,
                     const Profile& p) {
  Fixture<T> sample;
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, p));
  const auto workspace = sample.Workspace(plan);
  // Both checked views name the same live containing T array, with real
  // backing capacity for every selected RHS layout. No view is fabricated.
  const auto b = Take(asc::DenseBlasMatrixView<T>::Create(
      sample.a.data() + 1, 2, p.nrhs, p.b_layout, 3,
      {sample.a.data(), sizeof(sample.a), kHost}));
  asc::LapackReport report;
  report.native_info = 777;
  report.called_provider = true;
  faults::Disarm();
  const auto calls = TotalCalls();
  asc_dense_test::AllocationProbe allocations;
  const auto query =
      asc::QueryPpsvWorkspace(provider, p.triangle, sample.A(p), b);
  const auto status =
      asc::Ppsv(provider, p.triangle, sample.A(p), b, plan, workspace, report);
  const auto count = allocations.count();
  checks.Expect(asc_test::ProcessAllocationCountMatches(count, 0),
                "alias preflight allocation");
  checks.Expect(
      !query.ok() &&
          query.status().code() == asc::ErrorCode::kInvalidArgument &&
          status.code() == asc::ErrorCode::kInvalidArgument &&
          TotalCalls() == calls,
      "both entry points reject actual A/B overlap before native entry");
  checks.Expect(Same(sample.a, before.a) && Same(sample.b, before.b) &&
                    Same(sample.packing, before.packing),
                "actual A/B overlap preserves every numeric and scratch byte");
  checks.Expect(
      !report.native_info && !report.called_provider &&
          report.outcome == asc::LapackOutcome::kNotRun &&
          report.output_validity == asc::LapackOutputValidity::kUnchanged,
      "alias rejection resets old native report");
  ++checks.profiles;
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (auto a_layout : {kColumn, kRow}) {
    for (auto b_layout : {kColumn, kRow}) {
      for (auto triangle : {kUpper, kLower}) {
        for (const asc::extent_t nrhs : {0, 1, 2}) {
          const Profile p{triangle, a_layout, b_layout, nrhs};
          RealCalls<T>(checks, provider, p);
          Preflight<T>(checks, provider, p);
          if (nrhs != 0) {
            AliasedOperands<T>(checks, provider, p);
          }
          for (auto fault :
               {Fault::kNoWrite, Fault::kNegative, Fault::kImpossible,
                Fault::kPositive, Fault::kLowZero, Fault::kLowOnes}) {
            Inject<T>(checks, provider, p, fault);
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  checks.Expect(checks.profiles == 2976, "all scheduled profiles executed");
  std::printf(
      "Packed Cholesky driver failure: %zu profiles, %zu checks, %zu "
      "failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
