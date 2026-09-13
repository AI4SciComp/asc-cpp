#include <algorithm>
#include <array>
#include <cmath>
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
#include <utility>

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
#include "asc/dense/providers/lapack_cholesky_packed_condition.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "packed_cholesky_condition_faults.h"

namespace {
using asc_packed_cholesky_condition_test::Fault;
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

struct Profile {
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
};
class Checks {
 public:
  void Expect(bool condition, const char* message) {
    ++checks_;
    if (!condition) {
      if (failures_ < 30) {
        std::fprintf(stderr, "Packed Cholesky condition failure: %s\n",
                     message);
      }
      ++failures_;
    }
  }
  void ProfileDone() { ++profiles_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Packed Cholesky condition failure acceptance: %zu profiles, %zu "
        "checks, "
        "%zu "
        "failures\n",
        profiles_, checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  std::size_t profiles_ = 0;
  std::size_t checks_ = 0;
  std::size_t failures_ = 0;
};

template <typename T, std::size_t N>
bool SameBytes(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto left = std::as_bytes(std::span(a));
  const auto right = std::as_bytes(std::span(b));
  return std::equal(left.begin(), left.end(), right.begin(), right.end());
}

template <typename T>
std::size_t Routine() {
  std::size_t scalar = 3;
  if constexpr (std::is_same_v<T, float>) {
    scalar = 0;
  } else if constexpr (std::is_same_v<T, double>) {
    scalar = 1;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    scalar = 2;
  }
  return scalar;
}

template <typename T>
auto ExpectedName() {
  constexpr std::array<std::string_view, 4> kNames{"sppcon", "dppcon", "cppcon",
                                                   "zppcon"};
  decltype(asc::LapackReport{}.routine) name{};
  const auto selected = kNames[Routine<T>()];
  std::copy(selected.begin(), selected.end(), name.begin());
  return name;
}

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 9> data;
  std::array<T, 10> packing;
  std::array<T, 16> scalar;
  std::array<Real, 8> real;
  alignas(std::max_align_t) std::array<std::byte, 64> integers;
  std::array<Real, 3> condition{Real{-29}, Real{-29}, Real{-29}};
  Real norm = 86;
  asc::MemorySpace space = kHost;
  explicit Fixture(const Profile& /*profile*/) {
    data.fill(Value<T>(512));
    data[0] = T{4};
    data[1] = T{1};
    data[2] = T{9};
    packing.fill(T{-47});
    scalar.fill(T{-43});
    real.fill(Real{-53});
    integers.fill(std::byte{0x5a});
  }
  auto View(const Profile& profile) {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        data.data(), 2, profile.layout, {data.data(), sizeof(data), space}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider,
             const Profile& profile) {
    return asc::QueryPpconWorkspace(provider, profile.triangle, View(profile),
                                    norm, condition[1]);
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kLayout].minimum_entries;
    ASC_CHECK(count == 0 || count == 3);
    if (count != 0) {
      workspace.regions[kLayout] = {packing.data() + 1,
                                    static_cast<std::size_t>(count) * sizeof(T),
                                    kHost};
    }
    const auto scalar_count = asc::DenseBlasComplex<T> ? 4 : 6;
    ASC_CHECK(plan.regions[kScalar].minimum_entries == scalar_count);
    workspace.regions[kScalar] = {scalar.data() + 1, scalar_count * sizeof(T),
                                  kHost};
    if constexpr (asc::DenseBlasComplex<T>) {
      ASC_CHECK(plan.regions[kReal].minimum_entries == 2);
      workspace.regions[kReal] = {real.data() + 1, 2 * sizeof(Real), kHost};
    } else {
      ASC_CHECK(plan.regions[kInteger].minimum_entries == 2);
      workspace.regions[kInteger] = {integers.data() + 8, 2 * sizeof(Integer),
                                     kHost};
    }
    return workspace;
  }
  asc::Status Call(const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
    return asc::Ppcon(provider, profile.triangle, View(profile), norm,
                      condition[1], plan, workspace, report);
  }
  [[nodiscard]] bool NumericBytesEqual(const Fixture& other) const {
    return SameBytes(data, other.data) && SameBytes(packing, other.packing) &&
           SameBytes(scalar, other.scalar) && SameBytes(real, other.real) &&
           SameBytes(integers, other.integers) &&
           SameBytes(condition, other.condition);
  }
  void CheckGuards(const asc::LapackWorkspacePlan& plan, Checks& checks) const {
    const std::array kinds{kScalar, kLayout, kReal, kInteger};
    const std::array<std::span<const std::byte>, 4> buffers{
        std::as_bytes(std::span(scalar)), std::as_bytes(std::span(packing)),
        std::as_bytes(std::span(real)), std::as_bytes(std::span(integers))};
    const Fixture expected(Profile{kUpper, kColumn});
    const std::array<std::span<const std::byte>, 4> originals{
        std::as_bytes(std::span(expected.scalar)),
        std::as_bytes(std::span(expected.packing)),
        std::as_bytes(std::span(expected.real)),
        std::as_bytes(std::span(expected.integers))};
    const std::array offsets{sizeof(T), sizeof(T), sizeof(Real),
                             std::size_t{8}};
    for (std::size_t i = 0; i < kinds.size(); ++i) {
      const auto& region = plan.regions[kinds[i]];
      const auto end =
          offsets[i] +
          static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes;
      bool preserved = true;
      for (std::size_t j = 0; j < buffers[i].size(); ++j) {
        if (j < offsets[i] || j >= end) {
          preserved = preserved && buffers[i][j] == originals[i][j];
        }
      }
      checks.Expect(preserved, "complete workspace guards");
    }
    checks.Expect(
        condition.front() == Real{-29} && condition.back() == Real{-29},
        "complete output guards");
  }
};

template <typename T>
void CheckArguments(Checks& checks, const Fixture<T>& sample,
                    const Profile& profile,
                    const asc::LapackWorkspace& workspace) {
  const auto args =
      asc_packed_cholesky_condition_test::LastArguments(Routine<T>());
  const void* factor =
      profile.layout == kRow ? sample.packing.data() + 1 : sample.data.data();
  const auto extra = asc::DenseBlasComplex<T> ? kReal : kInteger;
  checks.Expect(args.triangle == (profile.triangle == kUpper ? 'U' : 'L') &&
                    args.order == 2 && args.character_length == 1 &&
                    args.norm == 86,
                "all native value arguments and hidden length");
  checks.Expect(args.factor == factor &&
                    args.condition == &sample.condition[1] &&
                    args.scalar_work == workspace.regions[kScalar].data() &&
                    args.auxiliary_work == workspace.regions[extra].data(),
                "all native operand and workspace pointers");
}

std::int64_t Raw(Fault fault) {
  if (fault == Fault::kNegative) {
    return -2;
  }
  if (fault == Fault::kPositive) {
    return 1;
  }
  if (fault == Fault::kNoOutput || fault == Fault::kNegativeOutput ||
      fault == Fault::kNanOutput || fault == Fault::kInfiniteOutput) {
    return 0;
  }
  const auto minimum =
      static_cast<std::int64_t>(std::numeric_limits<Integer>::min());
  if (fault == Fault::kLowOnes) {
    return minimum +
           static_cast<std::int64_t>(
               (std::uint64_t{1} << (ASC_LAPACK_INTEGER_BITS / 2)) - 1);
  }
  return minimum;
}

void CheckReport(Checks& checks, const asc::LapackReport& report,
                 const asc::Status& status, Fault fault) {
  const auto raw = Raw(fault);
  const bool warning = fault == Fault::kNoOutput ||
                       fault == Fault::kNanOutput ||
                       fault == Fault::kInfiniteOutput;
  checks.Expect(report.called_provider && report.native_info == raw,
                "surviving full-width INFO");
  checks.Expect(status.code() == (warning ? asc::ErrorCode::kNumerical
                                          : asc::ErrorCode::kProvider),
                "exact translated status");
  checks.Expect(report.output_validity ==
                    (warning ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kUnusable),
                "diagnostic output validity");
  auto outcome = asc::LapackOutcome::kPartialResult;
  if (warning) {
    outcome = asc::LapackOutcome::kAccuracyWarning;
  } else if (raw < 0) {
    outcome = asc::LapackOutcome::kProviderArgument;
  }
  checks.Expect(report.outcome == outcome, "exact diagnostic outcome");
  checks.Expect(!report.diagnostic_index, "no invented singular-pivot index");
  const bool argument = raw < 0 && raw != std::numeric_limits<Integer>::min();
  checks.Expect(
      argument ? report.native_argument == -raw : !report.native_argument,
      "safe negative-INFO argument");
  checks.Expect(!report.factor_family, "no invented factor family");
}

template <typename T>
void Inject(Checks& checks, const asc::ReferenceLapackProvider& provider,
            const Profile& profile, Fault fault) {
  Fixture<T> sample(profile);
  const auto before = sample;
  const auto routine = Routine<T>();
  const auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  asc::LapackReport report;
  asc_packed_cholesky_condition_test::Arm(routine, fault);
  const auto calls = asc_packed_cholesky_condition_test::Calls(routine);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  const auto cpp_allocations = allocations.count();
  const auto libc_allocations = asc_lapack_test::EndAllocationAudit();
  asc_packed_cholesky_condition_test::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0),
                "no observed C++ allocation");
  checks.Expect(libc_allocations == 0, "no wrapped libc allocation");
  checks.Expect(asc_packed_cholesky_condition_test::Calls(routine) == calls + 1,
                "exact selected native route entered once");
  CheckReport(checks, report, status, fault);
  const auto arguments =
      asc_packed_cholesky_condition_test::LastArguments(routine);
  checks.Expect(
      arguments.triangle == (profile.triangle == kUpper ? 'U' : 'L') &&
          arguments.order == 2 && arguments.character_length == 1,
      "complete native order/triangle/CHARACTER length");
  checks.Expect(report.provider == provider.identity(),
                "injected report preserves exact provider provenance");
  checks.Expect(report.routine == ExpectedName<T>(),
                "injected report names the exact scalar packed routine");
  checks.Expect(SameBytes(sample.data, before.data),
                "immutable factor on injected outcomes");
  using Real = asc::DenseBlasRealType<T>;
  if (fault == Fault::kNoOutput || fault == Fault::kNanOutput) {
    checks.Expect(std::isnan(sample.condition[1]),
                  "retained native or unwritten NaN");
  } else if (fault == Fault::kInfiniteOutput) {
    checks.Expect(sample.condition[1] == std::numeric_limits<Real>::infinity(),
                  "retained infinite native output");
  } else {
    checks.Expect(sample.condition[1] ==
                      (fault == Fault::kNegativeOutput ? Real{-37} : Real{37}),
                  "retained finite synthetic native output");
  }
  CheckArguments(checks, sample, profile, workspace);
  sample.CheckGuards(plan, checks);
  checks.ProfileDone();
}

enum class Rejection : std::uint8_t {
  kTriangle,
  kPlan,
  kShort,
  kOperandAlias,
  kReportAlias,
  kPlanAlias,
  kWorkspaceAlias,
  kAlignment,
  kWorkspacePlacement,
  kMatrixPlacement,
  kStaleTriangle,
  kStaleProvider,
  kStaleScalar,
  kWorkspaceOverlap,
  kStaleLayout,
  kStaleOrder,
  kStaleRoutine,
  kNegativeNorm,
  kNanNorm,
  kInfiniteNorm,
  kStaleActive,
  kShortAuxiliary,
  kAuxiliaryAlignment,
  kConditionAlias,
  kProviderAlias,
  kByteLimit,
  kRegionAlignment
};

std::size_t TotalCalls() {
  std::size_t total = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    total += asc_packed_cholesky_condition_test::Calls(i);
  }
  return total;
}

asc::LapackReport DirtyReport() {
  asc::LapackReport report;
  report.routine.fill('r');
  report.called_provider = true;
  report.native_info = 777;
  report.outcome = asc::LapackOutcome::kAccuracyWarning;
  report.output_validity = asc::LapackOutputValidity::kDocumentedPartial;
  report.diagnostic_index = 9;
  report.native_argument = 8;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  return report;
}

bool SameReport(const asc::LapackReport& a, const asc::LapackReport& b) {
  return a.routine == b.routine && a.provider == b.provider &&
         a.called_provider == b.called_provider &&
         a.native_info == b.native_info && a.outcome == b.outcome &&
         a.output_validity == b.output_validity &&
         a.diagnostic_index == b.diagnostic_index &&
         a.native_argument == b.native_argument &&
         a.factor_family == b.factor_family;
}

void StaleIdentity(const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, Rejection rejection,
                   asc::LapackWorkspacePlan& plan) {
  auto identity = provider.identity();
  if (rejection == Rejection::kStaleProvider) {
    identity.build_sha256[0] ^= std::byte{1};
  }
  const auto scalar = rejection == Rejection::kStaleScalar
                          ? asc::LapackScalarKind::kMixedF64F32
                          : plan.identity.scalar();
  const auto routine = rejection == Rejection::kStaleRoutine
                           ? std::string_view{"ppcon"}
                           : plan.identity.routine();
  const asc::extent_t order = rejection == Rejection::kStaleOrder ? 1 : 2;
  plan.identity = Take(asc::LapackPlanIdentity::Create(
      routine, scalar, std::array{order},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(profile.triangle),
                                  static_cast<std::int64_t>(profile.layout), 1},
      identity));
}

struct RejectionExpectation {
  asc::ErrorCode code;
  bool metadata_alias;
};

std::optional<RejectionExpectation> ConfigureOptionRejection(
    const asc::ReferenceLapackProvider& provider, Profile& profile,
    asc::LapackWorkspacePlan& plan, Rejection rejection) {
  auto expected = asc::ErrorCode::kInvalidState;
  switch (rejection) {
    case Rejection::kTriangle:
      // Explicitly fixed uint8_t enum: representable but not a legal flag.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      profile.triangle = static_cast<asc::DenseBlasTriangle>(9);
      expected = asc::ErrorCode::kInvalidArgument;
      break;
    case Rejection::kPlan:
      ++plan.regions[kLayout].minimum_entries;
      ++plan.regions[kLayout].preferred_entries;
      break;
    case Rejection::kStaleTriangle:
      profile.triangle = profile.triangle == kUpper ? kLower : kUpper;
      break;
    case Rejection::kStaleLayout:
      profile.layout = profile.layout == kRow ? kColumn : kRow;
      break;
    case Rejection::kStaleProvider:
    case Rejection::kStaleScalar:
    case Rejection::kStaleOrder:
    case Rejection::kStaleRoutine:
      StaleIdentity(provider, profile, rejection, plan);
      break;
    case Rejection::kByteLimit:
      ++plan.total_byte_limit;
      expected = asc::ErrorCode::kInvalidState;
      break;
    case Rejection::kRegionAlignment:
      plan.regions[kScalar].alignment *= 2;
      expected = asc::ErrorCode::kInvalidState;
      break;
    default:
      return std::nullopt;
  }
  return RejectionExpectation{expected, false};
}

template <typename T>
std::optional<RejectionExpectation> ConfigureNormRejection(
    Fixture<T>& sample, Rejection rejection) {
  auto expected = asc::ErrorCode::kInvalidArgument;
  switch (rejection) {
    case Rejection::kNegativeNorm:
      sample.norm = -1;
      break;
    case Rejection::kNanNorm:
      sample.norm = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
      break;
    case Rejection::kInfiniteNorm:
      sample.norm = std::numeric_limits<asc::DenseBlasRealType<T>>::infinity();
      break;
    case Rejection::kStaleActive:
      sample.norm = 0;
      expected = asc::ErrorCode::kInvalidState;
      break;
    default:
      return std::nullopt;
  }
  return RejectionExpectation{expected, false};
}

template <typename T>
RejectionExpectation ConfigureRejection(
    const asc::ReferenceLapackProvider& provider, Profile& profile,
    Fixture<T>& sample, asc::LapackWorkspacePlan& plan,
    asc::LapackWorkspace& workspace, asc::LapackReport& report,
    Rejection rejection) {
  auto expected = asc::ErrorCode::kInvalidArgument;
  bool metadata_alias = false;
  if (auto option =
          ConfigureOptionRejection(provider, profile, plan, rejection)) {
    return *option;
  }
  if (auto norm = ConfigureNormRejection(sample, rejection)) {
    return *norm;
  }
  switch (rejection) {
    case Rejection::kShort: {
      const auto region = workspace.regions[kScalar];
      ASC_CHECK(region.size() != 0);
      workspace.regions[kScalar] = {region.data(), region.size() - 1, kHost};
      break;
    }
    case Rejection::kAlignment: {
      const auto region = workspace.regions[kScalar];
      ASC_CHECK(region.size() != 0);
      workspace.regions[kScalar] = {static_cast<std::byte*>(region.data()) + 1,
                                    region.size(), kHost};
      break;
    }
    case Rejection::kWorkspacePlacement:
      workspace.regions[kScalar] = {workspace.regions[kScalar].data(),
                                    workspace.regions[kScalar].size(),
                                    asc::MemorySpace::kDevice};
      expected = asc::ErrorCode::kMemoryAccess;
      break;
    case Rejection::kMatrixPlacement:
      sample.space = asc::MemorySpace::kDevice;
      expected = asc::ErrorCode::kMemoryAccess;
      break;
    case Rejection::kWorkspaceOverlap:
      workspace.regions[6] = {sample.packing.data(), sizeof(T), kHost};
      workspace.regions[7] = workspace.regions[6];
      break;
    case Rejection::kOperandAlias:
      workspace.regions[0] = {sample.data.data(), sizeof(sample.data), kHost};
      break;
    case Rejection::kReportAlias:
      workspace.regions[0] = {&report, sizeof(report), kHost};
      metadata_alias = true;
      break;
    case Rejection::kPlanAlias:
      workspace.regions[0] = {&plan, sizeof(plan), kHost};
      metadata_alias = true;
      break;
    case Rejection::kWorkspaceAlias:
      workspace.regions[0] = {&workspace, sizeof(workspace), kHost};
      metadata_alias = true;
      break;
    case Rejection::kShortAuxiliary:
    case Rejection::kAuxiliaryAlignment: {
      const auto kind = asc::DenseBlasComplex<T> ? kReal : kInteger;
      const auto region = workspace.regions[kind];
      if (rejection == Rejection::kShortAuxiliary) {
        workspace.regions[kind] = {region.data(), region.size() - 1, kHost};
      } else {
        workspace.regions[kind] = {static_cast<std::byte*>(region.data()) + 1,
                                   region.size(), kHost};
      }
      break;
    }
    case Rejection::kConditionAlias:
      workspace.regions[6] = {&sample.condition[1], sizeof(sample.condition[1]),
                              kHost};
      break;
    case Rejection::kProviderAlias:
      // Expose only a borrowed byte view; preflight rejects it without writes.
      workspace.regions[0] = {
          const_cast<asc::ReferenceLapackProvider*>(&provider),
          sizeof(provider), kHost};
      metadata_alias = true;
      break;
    default:
      ASC_CHECK(false);
  }
  return {expected, metadata_alias};
}

template <typename T>
void Reject(Checks& checks, const asc::ReferenceLapackProvider& provider,
            Profile profile, Rejection rejection) {
  Fixture<T> sample(profile);
  const auto before = sample;
  auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  auto report = DirtyReport();
  const auto old_report = report;
  const auto expectation = ConfigureRejection(provider, profile, sample, plan,
                                              workspace, report, rejection);
  // Inaccessible placement cases supply rejected metadata tags on live host
  // backing arrays; no device storage or CUDA execution is claimed.
  // All metadata aliases are live mutable byte ranges. Numerical operands
  // retain their real owning arrays and exact reachable spans.
  asc_packed_cholesky_condition_test::Disarm();
  const auto calls = TotalCalls();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  const auto cpp_allocations = allocations.count();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0),
                "preflight has no observed C++ allocation");
  checks.Expect(status.code() == expectation.code,
                "exact preflight rejection status");
  checks.Expect(TotalCalls() == calls,
                "preflight calls none of4 native routes");
  checks.Expect(sample.NumericBytesEqual(before),
                "rejection preserves every numeric/workspace byte");
  if (expectation.metadata_alias) {
    checks.Expect(SameReport(report, old_report),
                  "live metadata alias preserves every report field");
  } else {
    checks.Expect(
        !report.called_provider && !report.native_info &&
            report.outcome == asc::LapackOutcome::kNotRun &&
            report.output_validity == asc::LapackOutputValidity::kUnchanged &&
            !report.diagnostic_index && !report.native_argument &&
            !report.factor_family,
        "ordinary preflight resets dirty report diagnostics");
    checks.Expect(report.provider == provider.identity(),
                  "preflight copies exact provider provenance");
  }
  checks.ProfileDone();
}

template <typename T>
void Preflight(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& profile) {
  for (auto rejection : {Rejection::kTriangle,
                         Rejection::kPlan,
                         Rejection::kOperandAlias,
                         Rejection::kReportAlias,
                         Rejection::kPlanAlias,
                         Rejection::kWorkspaceAlias,
                         Rejection::kWorkspacePlacement,
                         Rejection::kMatrixPlacement,
                         Rejection::kStaleTriangle,
                         Rejection::kStaleLayout,
                         Rejection::kStaleProvider,
                         Rejection::kStaleScalar,
                         Rejection::kStaleOrder,
                         Rejection::kStaleRoutine,
                         Rejection::kWorkspaceOverlap,
                         Rejection::kNegativeNorm,
                         Rejection::kNanNorm,
                         Rejection::kInfiniteNorm,
                         Rejection::kStaleActive,
                         Rejection::kShortAuxiliary,
                         Rejection::kAuxiliaryAlignment,
                         Rejection::kConditionAlias,
                         Rejection::kProviderAlias,
                         Rejection::kByteLimit,
                         Rejection::kRegionAlignment}) {
    Reject<T>(checks, provider, profile, rejection);
  }
  Reject<T>(checks, provider, profile, Rejection::kShort);
  Reject<T>(checks, provider, profile, Rejection::kAlignment);
}

template <typename T>
void RealCalls(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& profile) {
  for (int repeat = 0; repeat < 2; ++repeat) {
    Fixture<T> sample(profile);
    const auto before = sample;
    const auto routine = Routine<T>();
    asc_packed_cholesky_condition_test::Disarm();
    const auto calls = TotalCalls();
    auto query = [&] {
      asc_lapack_test::BeginAllocationAudit();
      asc_dense_test::AllocationProbe query_allocations;
      auto result = sample.Query(provider, profile);
      const auto query_cpp = query_allocations.count();
      const auto query_c = asc_lapack_test::EndAllocationAudit();
      checks.Expect(
          asc_test::ProcessAllocationCountMatches(query_cpp, 0) && query_c == 0,
          "query has no observed C++ or wrapped C allocation");
      return result;
    }();
    checks.Expect(query.ok() && TotalCalls() == calls,
                  "formula query succeeds without a native call");
    const auto plan = Take(std::move(query));
    auto workspace = sample.Workspace(plan);
    asc::LapackReport report;
    asc_lapack_test::BeginAllocationAudit();
    asc_dense_test::AllocationProbe allocations;
    const auto status = sample.Call(provider, profile, plan, workspace, report);
    const auto cpp_allocations = allocations.count();
    const auto libc_allocations = asc_lapack_test::EndAllocationAudit();
    const auto arguments =
        asc_packed_cholesky_condition_test::LastArguments(routine);
    checks.Expect(
        arguments.triangle == (profile.triangle == kUpper ? 'U' : 'L') &&
            arguments.order == 2 && arguments.character_length == 1,
        "real full-width native arguments");
    checks.Expect(report.routine == ExpectedName<T>(),
                  "real report names the exact scalar packed routine");
    checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0) &&
                      libc_allocations == 0,
                  "initial/repeated real call has no observed allocation");
    checks.Expect(asc_packed_cholesky_condition_test::Calls(routine) != 0 &&
                      TotalCalls() == calls + 1,
                  "initial/repeated call enters exactly one native route");
    checks.Expect(
        status.ok() && report.called_provider && report.native_info == 0 &&
            report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete &&
            !report.diagnostic_index && !report.native_argument &&
            !report.factor_family,
        "initial/repeated real success retains exact report semantics");
    checks.Expect(SameBytes(sample.data, before.data),
                  "real call preserves every factor byte");
    CheckArguments(checks, sample, profile, workspace);
    sample.CheckGuards(plan, checks);
    checks.ProfileDone();
  }
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (auto layout : {kColumn, kRow}) {
    for (auto triangle : {kUpper, kLower}) {
      const Profile profile{triangle, layout};
      RealCalls<T>(checks, provider, profile);
      for (auto fault : {Fault::kNoWrite, Fault::kNegative, Fault::kPositive,
                         Fault::kLowZero, Fault::kLowOnes, Fault::kNoOutput,
                         Fault::kNegativeOutput, Fault::kNanOutput,
                         Fault::kInfiniteOutput}) {
        Inject<T>(checks, provider, profile, fault);
      }
      Preflight<T>(checks, provider, profile);
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  return checks.Finish();
}
