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
#include "asc/dense/providers/lapack_cholesky_packed_equilibration.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "packed_cholesky_equilibration_faults.h"

namespace {
namespace faults = asc_packed_cholesky_equilibration_test;
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
        std::fprintf(stderr, "Packed Cholesky equilibration failure: %s\n",
                     message);
      }
      ++failures;
    }
  }
};

struct Profile {
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
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
  constexpr std::array<std::string_view, 4> kNames{"sppequ", "dppequ", "cppequ",
                                                   "zppequ"};
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
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 9> a;
  std::array<Real, 8> scales;
  std::array<Real, 3> condition{Real{-29}, Real{-29}, Real{-29}};
  std::array<Real, 3> maximum{Real{-31}, Real{-31}, Real{-31}};
  std::array<T, 16> packing;
  asc::extent_t scale_size = 2;
  asc::stride_t increment = 1;
  asc::MemorySpace a_space = kHost;
  asc::MemorySpace s_space = kHost;
  Fixture() {
    a.fill(T{-91});
    a[1] = Value<T>(4, 29);
    a[2] = Value<T>(0.5, 31);
    a[3] = Value<T>(9, -29);
    scales.fill(Real{19});
    packing.fill(T{-97});
  }
  auto A(const Profile& p) {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        a.data() + 1, 2, p.layout, {a.data(), sizeof(a), a_space}));
  }
  auto S() {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, scale_size, increment,
        {scales.data(), sizeof(scales), s_space}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider, const Profile& p) {
    return asc::QueryPpequWorkspace(provider, p.triangle, A(p), S(),
                                    condition[1], maximum[1]);
  }
  static auto Workspace(const asc::LapackWorkspacePlan& /*plan*/) {
    return asc::LapackWorkspace{};
  }
  auto Call(const asc::ReferenceLapackProvider& provider, const Profile& p,
            const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
    return asc::Ppequ(provider, p.triangle, A(p), S(), condition[1], maximum[1],
                      plan, workspace, report);
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
  if (fault == Fault::kBadScale || fault == Fault::kBadCondition ||
      fault == Fault::kBadMaximum) {
    return 0;
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
void Arguments(Checks& checks, const Profile& p, Fixture<T>& sample) {
  const auto args = faults::LastArguments(Routine<T>());
  const bool upper = (p.triangle == kUpper) == (p.layout == kColumn);
  checks.Expect(
      args.triangle == (upper ? 'U' : 'L') && args.order == 2 &&
          args.length == 1 && args.matrix == sample.a.data() + 1 &&
          args.scales == sample.scales.data() + 1 &&
          args.condition == &sample.condition[1] &&
          args.maximum == &sample.maximum[1],
      "all actual native pointers,order,effective triangle andhidden length");
}

void InjectedReport(Checks& checks, Fault fault, const asc::Status& status,
                    const asc::LapackReport& report) {
  checks.Expect(report.called_provider && report.native_info == Raw(fault),
                "exact full-width native INFO");
  if (fault == Fault::kPositive) {
    checks.Expect(
        status.code() == asc::ErrorCode::kNumerical &&
            report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
            report.output_validity ==
                asc::LapackOutputValidity::kDocumentedPartial &&
            report.diagnostic_index == 1 && !report.native_argument,
        "synthetic partial output withfirst-nonpositive index");
  } else if (Raw(fault) == 0) {
    checks.Expect(
        status.code() == asc::ErrorCode::kNumerical &&
            report.outcome == asc::LapackOutcome::kAccuracyWarning &&
            report.output_validity ==
                asc::LapackOutputValidity::kDocumentedPartial &&
            !report.diagnostic_index && !report.native_argument,
        "INFO0 invalid numeric outputs warn without fabricated native failure");
  } else {
    checks.Expect(
        status.code() == asc::ErrorCode::kProvider &&
            report.output_validity == asc::LapackOutputValidity::kUnusable &&
            report.outcome == (Raw(fault) < 0
                                   ? asc::LapackOutcome::kProviderArgument
                                   : asc::LapackOutcome::kPartialResult) &&
            !report.diagnostic_index,
        "invalid missing orpartial INFO is a provider defect");
    const bool argument =
        Raw(fault) < 0 && Raw(fault) != std::numeric_limits<Integer>::min();
    checks.Expect(argument ? report.native_argument == -Raw(fault)
                           : !report.native_argument,
                  "negative INFO argument conversion is safe");
  }
  checks.Expect(!report.factor_family, "equilibration certifies no factor");
}

template <typename T>
void Inject(Checks& checks, const asc::ReferenceLapackProvider& provider,
            const Profile& p, Fault fault) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> sample;
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, p));
  const auto calls = TotalCalls();
  asc::LapackReport report;
  faults::Arm(Routine<T>(), fault);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, p, plan, {}, report);
  const auto cpp = allocations.count();
  const auto libc = asc_lapack_test::EndAllocationAudit();
  faults::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp, 0) && libc == 0,
                "injected execution observed no allocation");
  checks.Expect(TotalCalls() == calls + 1, "one selected native entry");
  checks.Expect(report.routine == ExpectedName<T>() &&
                    report.provider == provider.identity(),
                "exact routine andprovider identity");
  InjectedReport(checks, fault, status, report);
  Arguments(checks, p, sample);
  auto expected = before;
  if (Raw(fault) == 0) {
    expected.scales[1] = fault == Fault::kBadScale ? Real{} : Real{1};
    expected.scales[2] = Real{1};
    expected.condition[1] = fault == Fault::kBadCondition ? Real{-1} : Real{1};
    expected.maximum[1] = fault == Fault::kBadMaximum
                              ? std::numeric_limits<Real>::infinity()
                              : Real{1};
  } else {
    expected.scales[1] = Real{-37};
    expected.scales[2] = Real{-38};
    expected.maximum[1] = Real{-41};
    if (fault != Fault::kPositive) {
      expected.condition[1] = Real{-43};
    }
  }
  checks.Expect(Same(sample.a, before.a) &&
                    Same(sample.scales, expected.scales) &&
                    Same(sample.condition, expected.condition) &&
                    Same(sample.maximum, expected.maximum) &&
                    Same(sample.packing, before.packing),
                "exact synthetic outputs,input preservation andall guards");
  ++checks.profiles;
}

template <typename T>
void RealCalls(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& p) {
  using Real = asc::DenseBlasRealType<T>;
  faults::Disarm();
  Fixture<T> sample;
  const auto before = sample;
  const auto calls = TotalCalls();
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto query = sample.Query(provider, p);
  const auto query_cpp = allocations.count();
  const auto query_libc = asc_lapack_test::EndAllocationAudit();
  checks.Expect(query.ok() && TotalCalls() == calls,
                "query invokes no provider");
  checks.Expect(
      asc_test::ProcessAllocationCountMatches(query_cpp, 0) && query_libc == 0,
      "query has no observed allocations");
  const auto plan = Take(query);
  for (int repeat = 0; repeat < 2; ++repeat) {
    asc::LapackReport report;
    asc_lapack_test::BeginAllocationAudit();
    const auto status = sample.Call(provider, p, plan, {}, report);
    const auto cpp = allocations.count();
    const auto libc = asc_lapack_test::EndAllocationAudit();
    checks.Expect(asc_test::ProcessAllocationCountMatches(cpp, 0) && libc == 0,
                  "reused plan andnative call observed no allocations");
    checks.Expect(
        status.ok() && report.called_provider && report.native_info == 0 &&
            report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete &&
            report.provider == provider.identity() &&
            report.routine == ExpectedName<T>(),
        "real completion andidentity");
    checks.Expect(TotalCalls() == calls + static_cast<std::size_t>(repeat) + 1,
                  "one actual native entry perexecution");
    auto expected = before;
    expected.scales[1] = Real{0.5};
    expected.scales[2] = Real{1} / Real{3};
    expected.condition[1] = Real{2} / Real{3};
    expected.maximum[1] = Real{9};
    checks.Expect(Same(sample.a, before.a) &&
                      Same(sample.scales, expected.scales) &&
                      Same(sample.condition, expected.condition) &&
                      Same(sample.maximum, expected.maximum) &&
                      Same(sample.packing, before.packing),
                  "independent exact small scales,statistics andall guards");
    Arguments(checks, p, sample);
    ++checks.profiles;
  }
}

enum class Reject : std::uint8_t {
  kTriangle,
  kShape,
  kScaleStride,
  kMatrixSpace,
  kScaleSpace,
  kPlanCapacity,
  kPlanAlignment,
  kPlanRoutine,
  kPlanScalar,
  kPlanProvider,
  kPlanOrder,
  kPlanScaleLength,
  kPlanStride,
  kPlanEffectiveTriangle,
  kTriangleIdentity,
  kLayoutIdentity,
  kWorkspaceSpace,
  kWorkspaceOverlap,
  kMatrixWorkspaceAlias,
  kScaleWorkspaceAlias,
  kConditionWorkspaceAlias,
  kMaximumWorkspaceAlias,
  kReportAlias,
  kPlanAlias,
  kWorkspaceAlias,
  kProviderAlias
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
      kind == Reject::kPlanRoutine ? "ppsv" : plan.identity.routine();
  auto effective =
      (p.triangle == kUpper) == (p.layout == kColumn) ? kUpper : kLower;
  if (kind == Reject::kPlanEffectiveTriangle) {
    effective = effective == kUpper ? kLower : kUpper;
  }
  plan.identity = Take(asc::LapackPlanIdentity::Create(
      name, scalar,
      std::array<asc::extent_t, 2>{kind == Reject::kPlanOrder ? 1 : 2,
                                   kind == Reject::kPlanScaleLength ? 1 : 2},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(p.triangle),
                                  static_cast<std::int64_t>(p.layout),
                                  static_cast<std::int64_t>(effective),
                                  kind == Reject::kPlanStride ? 2 : 1},
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
      // Deliberate representable fixed-enum value outside supported flags.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      p.triangle = static_cast<asc::DenseBlasTriangle>(9);
      break;
    case Reject::kShape:
      f.scale_size = 1;
      expected.code = asc::ErrorCode::kShape;
      break;
    case Reject::kScaleStride:
      f.increment = 2;
      break;
    case Reject::kMatrixSpace:
    case Reject::kScaleSpace:
      (kind == Reject::kMatrixSpace ? f.a_space : f.s_space) =
          asc::MemorySpace::kDevice;
      expected.code = asc::ErrorCode::kMemoryAccess;
      break;
    case Reject::kPlanCapacity:
      ++plan.regions[kPacking].minimum_entries;
      ++plan.regions[kPacking].preferred_entries;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kPlanAlignment:
      plan.regions[kPacking].alignment = 2;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kTriangleIdentity:
      p.triangle = p.triangle == kUpper ? kLower : kUpper;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kLayoutIdentity:
      p.layout = p.layout == kRow ? kColumn : kRow;
      expected.code = asc::ErrorCode::kInvalidState;
      break;
    case Reject::kPlanRoutine:
    case Reject::kPlanScalar:
    case Reject::kPlanProvider:
    case Reject::kPlanOrder:
    case Reject::kPlanScaleLength:
    case Reject::kPlanStride:
    case Reject::kPlanEffectiveTriangle:
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
    case Reject::kWorkspaceSpace:
      w.regions[0] = {f.packing.data(), sizeof(T), asc::MemorySpace::kDevice};
      expected.code = asc::ErrorCode::kMemoryAccess;
      break;
    case Reject::kWorkspaceOverlap:
      w.regions[6] = {f.packing.data(), sizeof(T), kHost};
      w.regions[7] = w.regions[6];
      break;
    case Reject::kMatrixWorkspaceAlias:
      w.regions[0] = {f.a.data() + 1, sizeof(T), kHost};
      break;
    case Reject::kScaleWorkspaceAlias:
      w.regions[0] = {f.scales.data() + 1, sizeof(f.scales[0]), kHost};
      break;
    case Reject::kConditionWorkspaceAlias:
      w.regions[0] = {&f.condition[1], sizeof(f.condition[1]), kHost};
      break;
    case Reject::kMaximumWorkspaceAlias:
      w.regions[0] = {&f.maximum[1], sizeof(f.maximum[1]), kHost};
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
    case Reject::kProviderAlias:
      w.regions[0] = {const_cast<asc::ReferenceLapackProvider*>(&provider),
                      sizeof(provider), kHost};
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
  checks.Expect(Same(sample.a, before.a) &&
                    Same(sample.scales, before.scales) &&
                    Same(sample.condition, before.condition) &&
                    Same(sample.maximum, before.maximum) &&
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
  for (const auto kind : {Reject::kTriangle,
                          Reject::kShape,
                          Reject::kScaleStride,
                          Reject::kMatrixSpace,
                          Reject::kScaleSpace,
                          Reject::kPlanCapacity,
                          Reject::kPlanAlignment,
                          Reject::kPlanRoutine,
                          Reject::kPlanScalar,
                          Reject::kPlanProvider,
                          Reject::kPlanOrder,
                          Reject::kPlanScaleLength,
                          Reject::kPlanStride,
                          Reject::kPlanEffectiveTriangle,
                          Reject::kTriangleIdentity,
                          Reject::kLayoutIdentity,
                          Reject::kWorkspaceSpace,
                          Reject::kWorkspaceOverlap,
                          Reject::kMatrixWorkspaceAlias,
                          Reject::kScaleWorkspaceAlias,
                          Reject::kConditionWorkspaceAlias,
                          Reject::kMaximumWorkspaceAlias,
                          Reject::kReportAlias,
                          Reject::kPlanAlias,
                          Reject::kWorkspaceAlias,
                          Reject::kProviderAlias}) {
    Rejection<T>(checks, provider, p, kind);
  }
}

template <typename T>
void ScalarAliases(Checks& checks, const asc::ReferenceLapackProvider& provider,
                   const Profile& p) {
  for (int kind = 0; kind < 3; ++kind) {
    Fixture<T> sample;
    const auto before = sample;
    const auto plan = Take(sample.Query(provider, p));
    auto& condition = kind == 0 ? sample.scales[1] : sample.condition[1];
    auto* maximum = &sample.maximum[1];
    if (kind == 1) {
      maximum = &sample.scales[2];
    } else if (kind == 2) {
      maximum = &condition;
    }
    const auto calls = TotalCalls();
    asc::LapackReport report;
    asc_dense_test::AllocationProbe allocations;
    const auto query = asc::QueryPpequWorkspace(
        provider, p.triangle, sample.A(p), sample.S(), condition, *maximum);
    const auto status =
        asc::Ppequ(provider, p.triangle, sample.A(p), sample.S(), condition,
                   *maximum, plan, {}, report);
    const auto count = allocations.count();
    checks.Expect(asc_test::ProcessAllocationCountMatches(count, 0),
                  "alias has no observed allocation");
    checks.Expect(
        !query.ok() &&
            query.status().code() == asc::ErrorCode::kInvalidArgument &&
            status.code() == asc::ErrorCode::kInvalidArgument &&
            TotalCalls() == calls,
        "actual S/SCOND/AMAX aliases rejected inboth entry points");
    checks.Expect(Same(sample.a, before.a) &&
                      Same(sample.scales, before.scales) &&
                      Same(sample.condition, before.condition) &&
                      Same(sample.maximum, before.maximum),
                  "actual aliases preserve every numeric byte");
    checks.Expect(
        !report.native_info && !report.called_provider &&
            report.outcome == asc::LapackOutcome::kNotRun &&
            report.output_validity == asc::LapackOutputValidity::kUnchanged,
        "alias failure reset report");
    ++checks.profiles;
  }
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kRow, kColumn}) {
    for (const auto triangle : {kUpper, kLower}) {
      const Profile p{triangle, layout};
      RealCalls<T>(checks, provider, p);
      Preflight<T>(checks, provider, p);
      ScalarAliases<T>(checks, provider, p);
      for (const auto fault :
           {Fault::kNoWrite, Fault::kNegative, Fault::kImpossible,
            Fault::kPositive, Fault::kLowZero, Fault::kLowOnes,
            Fault::kBadScale, Fault::kBadCondition, Fault::kBadMaximum}) {
        Inject<T>(checks, provider, p, fault);
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
  checks.Expect(checks.profiles == 640, "all scheduled profiles executed");
  std::printf(
      "Packed Cholesky equilibration failure: %zu profiles, %zu checks, %zu "
      "failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
