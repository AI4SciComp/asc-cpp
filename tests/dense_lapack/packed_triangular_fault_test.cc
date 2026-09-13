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
#include "asc/dense/providers/lapack_triangular_packed.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "packed_triangular_faults.h"

namespace {
using asc_packed_triangular_test::Fault;
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

enum class Kind : std::uint8_t { kTptri, kTptrs };
struct Profile {
  Kind kind;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasDiagonal diagonal;
  asc::DenseBlasTranspose operation;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout b_layout;
  asc::extent_t nrhs;
};
class Checks {
 public:
  void Expect(bool condition, const char* message) {
    ++checks_;
    if (!condition) {
      if (failures_ < 30) {
        std::fprintf(stderr, "Packed triangular failure: %s\n", message);
      }
      ++failures_;
    }
  }
  void ProfileDone() { ++profiles_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Packed triangular failure acceptance: %zu profiles, %zu checks, %zu "
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
std::size_t Routine(Kind kind) {
  std::size_t scalar = 3;
  if constexpr (std::is_same_v<T, float>) {
    scalar = 0;
  } else if constexpr (std::is_same_v<T, double>) {
    scalar = 1;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    scalar = 2;
  }
  return 2 * scalar + static_cast<std::size_t>(kind);
}

template <typename T>
auto ExpectedName(Kind kind) {
  constexpr std::array<std::string_view, 8> kNames{"stptri", "stptrs", "dtptri",
                                                   "dtptrs", "ctptri", "ctptrs",
                                                   "ztptri", "ztptrs"};
  decltype(asc::LapackReport{}.routine) name{};
  const auto selected = kNames[Routine<T>(kind)];
  std::copy(selected.begin(), selected.end(), name.begin());
  return name;
}

template <typename T>
struct PackedPair {
  std::array<T, 9> data{};
  asc::DenseBlasLayout layout;
  asc::MemorySpace space = kHost;
  // Both documented packed orders coincide for a selected 2-by-2 triangle.
  // Larger-order permutation is covered by the independent public consumer.
  T& At(std::size_t i, std::size_t j) { return data[i + j]; }
  auto View() {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        data.data(), 2, layout, {data.data(), sizeof(data), space}));
  }
  auto ConstView() { return asc::DenseBlasPackedMatrixView<const T>(View()); }
};

template <typename T>
struct Fixture {
  PackedPair<T> a;
  installed_internal::Matrix<T, 2, 2> b;
  std::array<T, 10> packing;
  asc::extent_t rhs_rows = 2;
  asc::MemorySpace rhs_space = kHost;
  explicit Fixture(const Profile& profile)
      : a{{}, profile.a_layout}, b{{}, profile.b_layout} {
    a.data.fill(Value<T>(512));
    b.data.fill(Value<T>(1024));
    a.At(0, 0) = Value<T>(profile.diagonal == kUnit ? -99 : 2);
    a.At(1, 1) = Value<T>(profile.diagonal == kUnit ? -97 : 3);
    if (profile.triangle == kUpper) {
      a.At(0, 1) = Value<T>(0.25, 0.125);
    } else {
      a.At(1, 0) = Value<T>(0.25, 0.125);
    }
    for (std::size_t i = 0; i < 2; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        b.At(i, j) = Value<T>(static_cast<double>(i + j + 1));
      }
    }
    packing.fill(Value<T>(-4096, 1024));
  }
  auto B(const Profile& profile) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        b.data.data(), rhs_rows, profile.nrhs, b.layout, 3,
        {b.data.data(), sizeof(b.data), rhs_space}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider,
             const Profile& profile) {
    if (profile.kind == Kind::kTptri) {
      return asc::QueryTptriWorkspace(provider, profile.triangle,
                                      profile.diagonal, a.View());
    }
    return asc::QueryTptrsWorkspace(provider, profile.triangle,
                                    profile.diagonal, profile.operation,
                                    a.ConstView(), B(profile));
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kLayout].minimum_entries;
    ASC_CHECK(count >= 0 && count <= 8);
    if (count != 0) {
      workspace.regions[kLayout] = {packing.data() + 1,
                                    static_cast<std::size_t>(count) * sizeof(T),
                                    kHost};
    }
    return workspace;
  }
  asc::Status Call(const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
    if (profile.kind == Kind::kTptri) {
      return asc::Tptri(provider, profile.triangle, profile.diagonal, a.View(),
                        plan, workspace, report);
    }
    return asc::Tptrs(provider, profile.triangle, profile.diagonal,
                      profile.operation, a.ConstView(), B(profile), plan,
                      workspace, report);
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
  if (fault == Fault::kLowOnes) {
    return minimum +
           static_cast<std::int64_t>(
               (std::uint64_t{1} << (ASC_LAPACK_INTEGER_BITS / 2)) - 1);
  }
  return minimum;
}

void CheckReport(Checks& checks, const asc::LapackReport& report,
                 const asc::Status& status, const Profile& profile,
                 Fault fault) {
  const auto raw = Raw(fault);
  const bool singular =
      fault == Fault::kPositive && profile.diagonal == kNonUnit;
  checks.Expect(report.called_provider && report.native_info == raw,
                "surviving full-width INFO");
  checks.Expect(status.code() == (singular ? asc::ErrorCode::kNumerical
                                           : asc::ErrorCode::kProvider),
                "exact translated status");
  checks.Expect(report.output_validity ==
                    (singular ? asc::LapackOutputValidity::kUnchanged
                              : asc::LapackOutputValidity::kUnusable),
                "diagnostic output validity");
  auto outcome = asc::LapackOutcome::kPartialResult;
  if (singular) {
    outcome = asc::LapackOutcome::kSingular;
  } else if (raw < 0) {
    outcome = asc::LapackOutcome::kProviderArgument;
  }
  checks.Expect(report.outcome == outcome, "exact diagnostic outcome");
  checks.Expect(
      singular ? report.diagnostic_index == 1 : !report.diagnostic_index,
      "diagnostic index");
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
  const auto routine = Routine<T>(profile.kind);
  const auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  asc::LapackReport report;
  asc_packed_triangular_test::Arm(routine, fault);
  const auto calls = asc_packed_triangular_test::Calls(routine);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  const auto cpp_allocations = allocations.count();
  const auto libc_allocations = asc_lapack_test::EndAllocationAudit();
  asc_packed_triangular_test::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0),
                "no observed C++ allocation");
  checks.Expect(libc_allocations == 0, "no wrapped libc allocation");
  checks.Expect(asc_packed_triangular_test::Calls(routine) == calls + 1,
                "exact selected native route entered once");
  CheckReport(checks, report, status, profile, fault);
  checks.Expect(report.routine == ExpectedName<T>(profile.kind),
                "injected report names the exact scalar packed routine");
  checks.Expect(SameBytes(sample.a.data, before.a.data) &&
                    SameBytes(sample.b.data, before.b.data),
                "injected no-output INFO leaves all operand bytes");
  const auto count =
      static_cast<std::size_t>(plan.regions[kLayout].minimum_entries);
  checks.Expect(sample.packing.front() == before.packing.front(),
                "packing front guard");
  checks.Expect(
      std::equal(sample.packing.begin() + 1 + count, sample.packing.end(),
                 before.packing.begin() + 1 + count),
      "packing tail guards");
  checks.ProfileDone();
}

template <typename T>
void Singular(Checks& checks, const asc::ReferenceLapackProvider& provider,
              const Profile& profile, std::size_t diagonal) {
  Fixture<T> sample(profile);
  sample.a.At(diagonal, diagonal) = T{};
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  asc::LapackReport report;
  const auto routine = Routine<T>(profile.kind);
  asc_packed_triangular_test::Disarm();
  const auto calls = asc_packed_triangular_test::Calls(routine);
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  checks.Expect(asc_packed_triangular_test::Calls(routine) == calls + 1,
                "real singular native entry");
  checks.Expect(
      status.code() == asc::ErrorCode::kNumerical && report.called_provider &&
          report.native_info == static_cast<std::int64_t>(diagonal + 1),
      "real zero-diagonal INFO including zero RHS");
  checks.Expect(report.routine == ExpectedName<T>(profile.kind),
                "singular report names the exact scalar packed routine");
  checks.Expect(
      report.outcome == asc::LapackOutcome::kSingular &&
          report.output_validity == asc::LapackOutputValidity::kUnchanged &&
          report.diagnostic_index == static_cast<std::int64_t>(diagonal),
      "real singular diagnostic validity");
  checks.Expect(SameBytes(sample.a.data, before.a.data) &&
                    SameBytes(sample.b.data, before.b.data),
                "real singular leaves full A/B bytes");
  checks.ProfileDone();
}

enum class Rejection : std::uint8_t {
  kTriangle,
  kDiagonal,
  kOperation,
  kPlan,
  kShort,
  kOperandAlias,
  kReportAlias,
  kPlanAlias,
  kWorkspaceAlias,
  kAlignment,
  kWorkspacePlacement,
  kMatrixPlacement,
  kRhsPlacement,
  kShape,
  kStaleTriangle,
  kStaleDiagonal,
  kStaleOperation,
  kStaleProvider,
  kStaleScalar,
  kWorkspaceOverlap
};

std::size_t TotalCalls() {
  std::size_t total = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    total += asc_packed_triangular_test::Calls(i);
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

template <typename T>
void StaleIdentity(const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, Rejection rejection,
                   asc::LapackWorkspacePlan& plan) {
  auto identity = provider.identity();
  if (rejection == Rejection::kStaleProvider) {
    identity.build_sha256[0] ^= std::byte{1};
  }
  // A legitimate separately constructed plan key, without modifying the
  // private representation of either the provider or a checked view.
  const auto scalar = rejection == Rejection::kStaleScalar
                          ? asc::LapackScalarKind::kMixedF64F32
                          : plan.identity.scalar();
  const auto triangle = static_cast<std::int64_t>(profile.triangle);
  const auto diagonal = static_cast<std::int64_t>(profile.diagonal);
  const auto layout = static_cast<std::int64_t>(profile.a_layout);
  if (profile.kind == Kind::kTptri) {
    plan.identity = Take(asc::LapackPlanIdentity::Create(
        plan.identity.routine(), scalar, std::array<asc::extent_t, 1>{2},
        std::array{triangle, diagonal, layout}, identity));
  } else {
    const asc::extent_t leading =
        profile.nrhs == 0 || profile.b_layout == kRow ? 2 : 3;
    plan.identity = Take(asc::LapackPlanIdentity::Create(
        plan.identity.routine(), scalar,
        std::array<asc::extent_t, 3>{2, profile.nrhs, leading},
        std::array<std::int64_t, 6>{
            triangle, diagonal, static_cast<std::int64_t>(profile.operation),
            layout, static_cast<std::int64_t>(profile.b_layout), 3},
        identity));
  }
}

struct RejectionExpectation {
  asc::ErrorCode code;
  bool metadata_alias;
};

template <typename T>
std::optional<RejectionExpectation> ConfigureOptionRejection(
    const asc::ReferenceLapackProvider& provider, Profile& profile,
    Fixture<T>& sample, asc::LapackWorkspacePlan& plan, Rejection rejection) {
  auto expected = asc::ErrorCode::kInvalidArgument;
  switch (rejection) {
    case Rejection::kTriangle:
      profile.triangle = static_cast<asc::DenseBlasTriangle>(9);
      break;
    case Rejection::kDiagonal:
      profile.diagonal = static_cast<asc::DenseBlasDiagonal>(9);
      break;
    case Rejection::kOperation:
      profile.operation = static_cast<asc::DenseBlasTranspose>(9);
      break;
    case Rejection::kPlan:
      ++plan.regions[kLayout].minimum_entries;
      ++plan.regions[kLayout].preferred_entries;
      expected = asc::ErrorCode::kInvalidState;
      break;
    case Rejection::kShape:
      sample.rhs_rows = 1;
      expected = asc::ErrorCode::kShape;
      break;
    case Rejection::kStaleTriangle:
      profile.triangle = profile.triangle == kUpper ? kLower : kUpper;
      expected = asc::ErrorCode::kInvalidState;
      break;
    case Rejection::kStaleDiagonal:
      profile.diagonal = profile.diagonal == kUnit ? kNonUnit : kUnit;
      expected = asc::ErrorCode::kInvalidState;
      break;
    case Rejection::kStaleOperation:
      profile.operation = profile.operation == kNone
                              ? asc::DenseBlasTranspose::kTranspose
                              : kNone;
      expected = asc::ErrorCode::kInvalidState;
      break;
    case Rejection::kStaleProvider:
    case Rejection::kStaleScalar:
      StaleIdentity<T>(provider, profile, rejection, plan);
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
  if (auto option = ConfigureOptionRejection(provider, profile, sample, plan,
                                             rejection)) {
    return *option;
  }
  switch (rejection) {
    case Rejection::kShort: {
      const auto region = workspace.regions[kLayout];
      ASC_CHECK(region.size() != 0);
      workspace.regions[kLayout] = {region.data(), region.size() - 1, kHost};
      break;
    }
    case Rejection::kAlignment: {
      const auto region = workspace.regions[kLayout];
      ASC_CHECK(region.size() != 0);
      workspace.regions[kLayout] = {static_cast<std::byte*>(region.data()) + 1,
                                    region.size(), kHost};
      break;
    }
    case Rejection::kWorkspacePlacement:
      workspace.regions[0] = {sample.packing.data(), sizeof(T),
                              asc::MemorySpace::kDevice};
      expected = asc::ErrorCode::kMemoryAccess;
      break;
    case Rejection::kMatrixPlacement:
      sample.a.space = asc::MemorySpace::kDevice;
      expected = asc::ErrorCode::kMemoryAccess;
      break;
    case Rejection::kRhsPlacement:
      sample.rhs_space = asc::MemorySpace::kDevice;
      expected = asc::ErrorCode::kMemoryAccess;
      break;
    case Rejection::kWorkspaceOverlap:
      workspace.regions[6] = {sample.packing.data(), sizeof(T), kHost};
      workspace.regions[7] = workspace.regions[6];
      break;
    case Rejection::kOperandAlias:
      workspace.regions[0] = {sample.a.data.data(), sizeof(sample.a.data),
                              kHost};
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
  asc_packed_triangular_test::Disarm();
  const auto calls = TotalCalls();
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  checks.Expect(status.code() == expectation.code,
                "exact preflight rejection status");
  checks.Expect(TotalCalls() == calls,
                "preflight calls none of8 native routes");
  checks.Expect(SameBytes(sample.a.data, before.a.data) &&
                    SameBytes(sample.b.data, before.b.data) &&
                    SameBytes(sample.packing, before.packing),
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
  for (auto rejection :
       {Rejection::kTriangle, Rejection::kDiagonal, Rejection::kPlan,
        Rejection::kOperandAlias, Rejection::kReportAlias,
        Rejection::kPlanAlias, Rejection::kWorkspaceAlias,
        Rejection::kWorkspacePlacement, Rejection::kMatrixPlacement,
        Rejection::kStaleTriangle, Rejection::kStaleDiagonal,
        Rejection::kStaleProvider, Rejection::kStaleScalar,
        Rejection::kWorkspaceOverlap}) {
    Reject<T>(checks, provider, profile, rejection);
  }
  if (profile.kind == Kind::kTptrs) {
    for (auto rejection :
         {Rejection::kOperation, Rejection::kShape, Rejection::kStaleOperation,
          Rejection::kRhsPlacement}) {
      Reject<T>(checks, provider, profile, rejection);
    }
  }
  Fixture<T> sample(profile);
  const auto plan = Take(sample.Query(provider, profile));
  if (plan.regions[kLayout].minimum_entries != 0) {
    Reject<T>(checks, provider, profile, Rejection::kShort);
    Reject<T>(checks, provider, profile, Rejection::kAlignment);
  }
}

template <typename T>
void AliasedSolve(Checks& checks, const asc::ReferenceLapackProvider& provider,
                  const Profile& profile) {
  Fixture<T> sample(profile);
  const auto before = sample;
  auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  auto report = DirtyReport();
  // Both descriptors cover the same live scalar array. Their layouts are
  // independent, and the RHS count remains exactly two for this rejection.
  auto b = Take(asc::DenseBlasMatrixView<T>::Create(
      sample.a.data.data(), 2, 2, profile.b_layout, 3,
      {sample.a.data.data(), sizeof(sample.a.data), kHost}));
  asc_packed_triangular_test::Disarm();
  const auto calls = TotalCalls();
  const auto query =
      asc::QueryTptrsWorkspace(provider, profile.triangle, profile.diagonal,
                               profile.operation, sample.a.ConstView(), b);
  const auto status = asc::Tptrs(provider, profile.triangle, profile.diagonal,
                                 profile.operation, sample.a.ConstView(), b,
                                 plan, workspace, report);
  checks.Expect(!query.ok() &&
                    query.status().code() == asc::ErrorCode::kInvalidArgument &&
                    status.code() == asc::ErrorCode::kInvalidArgument,
                "query and execution reject actual A/B overlap");
  checks.Expect(TotalCalls() == calls,
                "overlapping solve makes no native call");
  checks.Expect(SameBytes(sample.a.data, before.a.data) &&
                    SameBytes(sample.b.data, before.b.data) &&
                    SameBytes(sample.packing, before.packing),
                "overlapping solve preserves every numeric and workspace byte");
  checks.Expect(
      !report.called_provider && !report.native_info &&
          report.outcome == asc::LapackOutcome::kNotRun &&
          report.output_validity == asc::LapackOutputValidity::kUnchanged &&
          !report.diagnostic_index && !report.native_argument &&
          !report.factor_family && report.provider == provider.identity(),
      "overlapping solve resets stale report without fabricated INFO");
  checks.ProfileDone();
}

template <typename T>
void RealCalls(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& profile) {
  for (int repeat = 0; repeat < 2; ++repeat) {
    Fixture<T> sample(profile);
    const auto before = sample;
    const auto routine = Routine<T>(profile.kind);
    asc_packed_triangular_test::Disarm();
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
    checks.Expect(report.routine == ExpectedName<T>(profile.kind),
                  "real report names the exact scalar packed routine");
    checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0) &&
                      libc_allocations == 0,
                  "initial/repeated real call has no observed allocation");
    checks.Expect(asc_packed_triangular_test::Calls(routine) != 0 &&
                      TotalCalls() == calls + 1,
                  "initial/repeated call enters exactly one native route");
    checks.Expect(
        status.ok() && report.called_provider && report.native_info == 0 &&
            report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete &&
            !report.diagnostic_index && !report.native_argument &&
            !report.factor_family,
        "initial/repeated real success retains exact report semantics");
    auto expected_a = before.a.data;
    auto expected_b = before.b.data;
    for (std::size_t i = 0; i < 2; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        if (profile.kind == Kind::kTptri &&
            (profile.triangle == kUpper ? i <= j : i >= j) &&
            (i != j || profile.diagonal == kNonUnit)) {
          expected_a[i + j] = sample.a.At(i, j);
        }
        if (profile.kind == Kind::kTptrs && profile.nrhs != 0) {
          expected_b[sample.b.Offset(i, j)] = sample.b.At(i, j);
        }
      }
    }
    checks.Expect(
        SameBytes(sample.a.data, expected_a) &&
            SameBytes(sample.b.data, expected_b),
        "real call preserves all ignored, immutable and padding bytes");
    checks.Expect(sample.packing.front() == before.packing.front() &&
                      sample.packing.back() == before.packing.back(),
                  "real call preserves packing red zones");
    checks.ProfileDone();
  }
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (auto kind : {Kind::kTptri, Kind::kTptrs}) {
    for (auto a_layout : {kColumn, kRow}) {
      for (auto b_layout : {kColumn, kRow}) {
        for (auto triangle : {kUpper, kLower}) {
          for (auto diagonal : {kUnit, kNonUnit}) {
            for (auto operation :
                 {kNone, asc::DenseBlasTranspose::kTranspose,
                  asc::DenseBlasTranspose::kConjugateTranspose}) {
              if (kind != Kind::kTptrs && operation != kNone) {
                continue;
              }
              for (asc::extent_t nrhs : {0, 2}) {
                if (kind != Kind::kTptrs && nrhs == 0) {
                  continue;
                }
                const Profile profile{kind,     triangle, diagonal, operation,
                                      a_layout, b_layout, nrhs};
                RealCalls<T>(checks, provider, profile);
                for (auto fault :
                     {Fault::kNoWrite, Fault::kNegative, Fault::kImpossible,
                      Fault::kPositive, Fault::kLowZero, Fault::kLowOnes}) {
                  Inject<T>(checks, provider, profile, fault);
                }
                Preflight<T>(checks, provider, profile);
                if (kind == Kind::kTptrs && nrhs == 2) {
                  AliasedSolve<T>(checks, provider, profile);
                }
                if (diagonal == kNonUnit) {
                  Singular<T>(checks, provider, profile, 0);
                  Singular<T>(checks, provider, profile, 1);
                }
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
