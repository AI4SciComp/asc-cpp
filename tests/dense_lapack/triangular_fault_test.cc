#include <algorithm>
#include <array>
#include <cmath>
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
#include "asc/dense/providers/lapack_triangular.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "triangular_faults.h"

namespace {
using asc_triangular_test::Fault;
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

enum class Kind : std::uint8_t { kTrtri, kTrti2, kTrtrs };
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
        std::fprintf(stderr, "Triangular failure: %s\n", message);
      }
      ++failures_;
    }
  }
  void ProfileDone() { ++profiles_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Triangular failure acceptance: %zu profiles, %zu checks, %zu "
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
  return 3 * scalar + static_cast<std::size_t>(kind);
}

template <typename T>
struct Fixture {
  installed_internal::Matrix<T, 2, 2> a;
  installed_internal::Matrix<T, 2, 2> b;
  std::array<T, 10> packing;
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
        b.data.data(), 2, profile.nrhs, b.layout, 3,
        {b.data.data(), sizeof(b.data), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider,
             const Profile& profile) {
    if (profile.kind == Kind::kTrtri) {
      return asc::QueryTrtriWorkspace(provider, profile.triangle,
                                      profile.diagonal, a.View());
    }
    if (profile.kind == Kind::kTrti2) {
      return asc::QueryTrti2Workspace(provider, profile.triangle,
                                      profile.diagonal, a.View());
    }
    return asc::QueryTrtrsWorkspace(provider, profile.triangle,
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
    if (profile.kind == Kind::kTrtri) {
      return asc::Trtri(provider, profile.triangle, profile.diagonal, a.View(),
                        plan, workspace, report);
    }
    if (profile.kind == Kind::kTrti2) {
      return asc::Trti2(provider, profile.triangle, profile.diagonal, a.View(),
                        plan, workspace, report);
    }
    return asc::Trtrs(provider, profile.triangle, profile.diagonal,
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
  const bool singular = fault == Fault::kPositive &&
                        profile.kind != Kind::kTrti2 &&
                        profile.diagonal == kNonUnit;
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
  asc_triangular_test::Arm(routine, fault);
  const auto calls = asc_triangular_test::Calls(routine);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  const auto cpp_allocations = allocations.count();
  const auto libc_allocations = asc_lapack_test::EndAllocationAudit();
  asc_triangular_test::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0),
                "no observed C++ allocation");
  checks.Expect(libc_allocations == 0, "no wrapped libc allocation");
  checks.Expect(asc_triangular_test::Calls(routine) == calls + 1,
                "exact selected native route entered once");
  CheckReport(checks, report, status, profile, fault);
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
  asc_triangular_test::Disarm();
  const auto calls = asc_triangular_test::Calls(routine);
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  checks.Expect(asc_triangular_test::Calls(routine) == calls + 1,
                "real singular native entry");
  checks.Expect(
      status.code() == asc::ErrorCode::kNumerical && report.called_provider &&
          report.native_info == static_cast<std::int64_t>(diagonal + 1),
      "real zero-diagonal INFO including zero RHS");
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
  kWorkspaceAlias
};

std::size_t TotalCalls() {
  std::size_t total = 0;
  for (std::size_t i = 0; i < 12; ++i) {
    total += asc_triangular_test::Calls(i);
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
void Reject(Checks& checks, const asc::ReferenceLapackProvider& provider,
            Profile profile, Rejection rejection) {
  Fixture<T> sample(profile);
  const auto before = sample;
  auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  auto report = DirtyReport();
  const auto old_report = report;
  auto expected = asc::ErrorCode::kInvalidArgument;
  bool metadata_alias = false;
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
    case Rejection::kShort: {
      const auto region = workspace.regions[kLayout];
      ASC_CHECK(region.size() != 0);
      workspace.regions[kLayout] = {region.data(), region.size() - 1, kHost};
      break;
    }
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
  }
  // All metadata aliases are live mutable byte ranges. Numerical operands
  // retain their real owning arrays and exact reachable spans.
  asc_triangular_test::Disarm();
  const auto calls = TotalCalls();
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  checks.Expect(status.code() == expected, "exact preflight rejection status");
  checks.Expect(TotalCalls() == calls,
                "preflight calls none of12 native routes");
  checks.Expect(SameBytes(sample.a.data, before.a.data) &&
                    SameBytes(sample.b.data, before.b.data) &&
                    SameBytes(sample.packing, before.packing),
                "rejection preserves every numeric/workspace byte");
  if (metadata_alias) {
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
        Rejection::kPlanAlias, Rejection::kWorkspaceAlias}) {
    Reject<T>(checks, provider, profile, rejection);
  }
  if (profile.kind == Kind::kTrtrs) {
    Reject<T>(checks, provider, profile, Rejection::kOperation);
  }
  Fixture<T> sample(profile);
  const auto plan = Take(sample.Query(provider, profile));
  if (plan.regions[kLayout].minimum_entries != 0) {
    Reject<T>(checks, provider, profile, Rejection::kShort);
  }
}

// TRTI2 deliberately has no singularity scan. These are source-fidelity
// checks for a singular input, not evidence of a mathematically valid inverse.
template <typename T>
void UncheckedZero(Checks& checks, const asc::ReferenceLapackProvider& provider,
                   const Profile& profile, std::size_t zero) {
  Fixture<T> sample(profile);
  sample.a.At(zero, zero) = Value<T>(0);
  const auto before = sample;
  auto plan = Take(sample.Query(provider, profile));
  auto workspace = sample.Workspace(plan);
  auto report = DirtyReport();
  asc_triangular_test::Disarm();
  const auto calls = TotalCalls();
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  checks.Expect(TotalCalls() == calls + 1, "zero diagonal enters real TRTI2");
  checks.Expect(installed_internal::Succeeded(status, report),
                "TRTI2 retains actual zero INFO without invented singularity");
  checks.Expect(!std::isfinite(std::abs(
                    installed_internal::Widen(sample.a.At(zero, zero)))),
                "unchecked zero diagonal produces nonfinite reciprocal");
  checks.Expect(!report.diagnostic_index && !report.native_argument &&
                    !report.factor_family &&
                    report.provider == provider.identity(),
                "unchecked inverse has no invented factor or error diagnostic");
  auto preserved = before.a.data;
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      if (profile.triangle == kUpper ? i <= j : i >= j) {
        preserved[sample.a.Offset(i, j)] = sample.a.At(i, j);
      }
    }
  }
  checks.Expect(SameBytes(preserved, sample.a.data) &&
                    SameBytes(sample.b.data, before.b.data),
                "unchecked inverse preserves unused triangle padding and RHS");
  checks.Expect(sample.packing.front() == before.packing.front() &&
                    sample.packing.back() == before.packing.back(),
                "unchecked inverse preserves workspace red zones");
  checks.ProfileDone();
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
  asc_triangular_test::Disarm();
  const auto calls = TotalCalls();
  const auto query =
      asc::QueryTrtrsWorkspace(provider, profile.triangle, profile.diagonal,
                               profile.operation, sample.a.ConstView(), b);
  const auto status = asc::Trtrs(provider, profile.triangle, profile.diagonal,
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
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (auto kind : {Kind::kTrtri, Kind::kTrti2, Kind::kTrtrs}) {
    for (auto a_layout : {kColumn, kRow}) {
      for (auto b_layout : {kColumn, kRow}) {
        for (auto triangle : {kUpper, kLower}) {
          for (auto diagonal : {kUnit, kNonUnit}) {
            for (auto operation :
                 {kNone, asc::DenseBlasTranspose::kTranspose,
                  asc::DenseBlasTranspose::kConjugateTranspose}) {
              if (kind != Kind::kTrtrs && operation != kNone) {
                continue;
              }
              for (asc::extent_t nrhs : {0, 2}) {
                if (kind != Kind::kTrtrs && nrhs == 0) {
                  continue;
                }
                const Profile profile{kind,     triangle, diagonal, operation,
                                      a_layout, b_layout, nrhs};
                for (auto fault :
                     {Fault::kNoWrite, Fault::kNegative, Fault::kImpossible,
                      Fault::kPositive, Fault::kLowZero, Fault::kLowOnes}) {
                  Inject<T>(checks, provider, profile, fault);
                }
                Preflight<T>(checks, provider, profile);
                if (kind == Kind::kTrti2 && diagonal == kNonUnit) {
                  UncheckedZero<T>(checks, provider, profile, 0);
                  UncheckedZero<T>(checks, provider, profile, 1);
                }
                if (kind == Kind::kTrtrs && nrhs == 2) {
                  AliasedSolve<T>(checks, provider, profile);
                }
                if (kind != Kind::kTrti2 && diagonal == kNonUnit) {
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
