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
#include "asc/dense/providers/lapack_cholesky_packed_refinement.h"
#include "factorization_support.h"
#include "lapack_build_config.h"
#include "normal_return_guard.h"
#include "packed_cholesky_refinement_faults.h"

namespace {
using asc_packed_cholesky_refinement_test::Fault;
using installed_internal::Take;
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
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout af_layout;
  asc::DenseBlasLayout b_layout;
  asc::DenseBlasLayout x_layout;
};
class Checks {
 public:
  void Expect(bool condition, const char* message) {
    ++checks_;
    if (!condition) {
      if (failures_ < 30) {
        std::fprintf(stderr, "Packed Cholesky refinement failure: %s\n",
                     message);
      }
      ++failures_;
    }
  }
  void ProfileDone() { ++profiles_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Packed Cholesky refinement failure acceptance: %zu profiles, %zu "
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
  constexpr std::array<std::string_view, 4> kNames{"spprfs", "dpprfs", "cpprfs",
                                                   "zpprfs"};
  decltype(asc::LapackReport{}.routine) name{};
  const auto selected = kNames[Routine<T>()];
  std::copy(selected.begin(), selected.end(), name.begin());
  return name;
}

std::size_t Offset(asc::DenseBlasLayout layout, int i, int j) {
  return 1 + static_cast<std::size_t>(layout == kRow ? 4 * i + j : 4 * j + i);
}

template <typename T>
struct Views {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<const T> a;
  asc::DenseBlasPackedMatrixView<const T> af;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> f;
  asc::DenseBlasVectorView<Real> e;
};

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 9> original;
  std::array<T, 9> factors;
  std::array<T, 16> rhs;
  std::array<T, 16> solution;
  std::array<Real, 8> ferr;
  std::array<Real, 8> berr;
  std::array<T, 32> packing;
  std::array<T, 16> scalar;
  std::array<Real, 8> real;
  alignas(std::max_align_t) std::array<std::byte, 64> integers;
  asc::MemorySpace space = kHost;
  asc::extent_t factor_order = 2;
  asc::extent_t b_rows = 2;
  asc::extent_t x_rows = 2;
  asc::extent_t b_columns = 2;
  asc::extent_t x_columns = 2;
  asc::extent_t forward_size = 2;
  asc::extent_t backward_size = 2;
  asc::extent_t b_leading = 4;
  asc::extent_t x_leading = 4;
  asc::index_t error_stride = 1;
  asc::index_t backward_stride = 1;
  explicit Fixture(const Profile& profile) {
    original.fill(T{-19});
    factors.fill(T{-23});
    rhs.fill(T{-29});
    solution.fill(T{-31});
    ferr.fill(Real{-37});
    berr.fill(Real{-41});
    packing.fill(T{-47});
    scalar.fill(T{-43});
    real.fill(Real{-53});
    integers.fill(std::byte{0x5a});
    // L=[[2,0],[1,3]], A=L*L^T=[[4,2],[2,10]]. For n=2 these
    // packed coefficients have the same order in both triangles/layouts.
    original[1] = T{4};
    original[2] = T{2};
    original[3] = T{10};
    factors[1] = T{2};
    factors[2] = T{1};
    factors[3] = T{3};
    const std::array<T, 4> b{T{6}, T{12}, T{-3}, T{3}};
    const std::array<T, 4> x{T{1}, T{1}, T{-1}, T{0.5}};
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 2; ++i) {
        const auto index =
            2 * static_cast<std::size_t>(j) + static_cast<std::size_t>(i);
        rhs[Offset(profile.b_layout, i, j)] = b[index];
        solution[Offset(profile.x_layout, i, j)] = x[index] * Real{0.75};
      }
    }
  }
  Views<T> Operands(const Profile& p) {
    return {Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                original.data() + 1, 2, p.a_layout,
                {original.data(), sizeof(original), space})),
            Take(asc::DenseBlasPackedMatrixView<const T>::Create(
                factors.data() + 1, factor_order, p.af_layout,
                {factors.data(), sizeof(factors), kHost})),
            Take(asc::DenseBlasMatrixView<const T>::Create(
                rhs.data() + 1, b_rows, b_columns, p.b_layout, b_leading,
                {rhs.data(), sizeof(rhs), kHost})),
            Take(asc::DenseBlasMatrixView<T>::Create(
                solution.data() + 1, x_rows, x_columns, p.x_layout, x_leading,
                {solution.data(), sizeof(solution), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                ferr.data() + 1, forward_size, error_stride,
                {ferr.data(), sizeof(ferr), kHost})),
            Take(asc::DenseBlasVectorView<Real>::Create(
                berr.data() + 1, backward_size, backward_stride,
                {berr.data(), sizeof(berr), kHost}))};
  }
  auto Query(const asc::ReferenceLapackProvider& provider, const Profile& p) {
    const auto v = Operands(p);
    return asc::QueryPprfsWorkspace(provider, p.triangle, v.a, v.af, v.b, v.x,
                                    v.f, v.e);
  }
  asc::Status Call(const asc::ReferenceLapackProvider& provider,
                   const Profile& p, const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
    const auto v = Operands(p);
    return asc::Pprfs(provider, p.triangle, v.a, v.af, v.b, v.x, v.f, v.e, plan,
                      workspace, report);
  }
  [[nodiscard]] bool InputsEqual(const Fixture& other) const {
    return SameBytes(original, other.original) &&
           SameBytes(factors, other.factors) && SameBytes(rhs, other.rhs);
  }
  [[nodiscard]] bool NumericBytesEqual(const Fixture& other) const {
    return InputsEqual(other) && SameBytes(solution, other.solution) &&
           SameBytes(ferr, other.ferr) && SameBytes(berr, other.berr) &&
           SameBytes(packing, other.packing) &&
           SameBytes(scalar, other.scalar) && SameBytes(real, other.real) &&
           SameBytes(integers, other.integers);
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kLayout].minimum_entries;
    ASC_CHECK(count >= 0 && count <= 14);
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
  void CheckGuards(const asc::LapackWorkspacePlan& plan, const Profile& profile,
                   Checks& checks) const {
    const std::array kinds{kScalar, kLayout, kReal, kInteger};
    const std::array<std::span<const std::byte>, 4> buffers{
        std::as_bytes(std::span(scalar)), std::as_bytes(std::span(packing)),
        std::as_bytes(std::span(real)), std::as_bytes(std::span(integers))};
    const Fixture expected(profile);
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

    for (std::size_t i = 0; i < ferr.size(); ++i) {
      if (i != 1 && i != 2) {
        checks.Expect(
            ferr[i] == expected.ferr[i] && berr[i] == expected.berr[i],
            "error vector guards");
      }
    }
    for (std::size_t i = 0; i < solution.size(); ++i) {
      if (i != 1 && i != 2 && i != 5 && i != 6) {
        checks.Expect(solution[i] == expected.solution[i],
                      "full X padding guards");
      }
    }
  }
};

template <typename T>
void CheckArguments(Checks& checks, const Fixture<T>& sample, const Profile& p,
                    const asc::LapackWorkspace& workspace) {
  const auto args =
      asc_packed_cholesky_refinement_test::LastArguments(Routine<T>());
  const T* cursor = sample.packing.data() + 1;
  const auto packed = [&cursor](const T* direct, asc::DenseBlasLayout layout,
                                int count) {
    if (layout == kColumn) {
      return direct;
    }
    const auto* result = cursor;
    cursor += count;
    return result;
  };
  const auto* a = packed(sample.original.data() + 1, p.a_layout, 3);
  const auto* af = packed(sample.factors.data() + 1, p.af_layout, 3);
  const auto* b = packed(sample.rhs.data() + 1, p.b_layout, 4);
  const auto* x = packed(sample.solution.data() + 1, p.x_layout, 4);
  const auto extra = asc::DenseBlasComplex<T> ? kReal : kInteger;
  checks.Expect(args.triangle == (p.triangle == kUpper ? 'U' : 'L') &&
                    args.order == 2 && args.right_hand_sides == 2 &&
                    args.character_length == 1 &&
                    args.ldb == (p.b_layout == kRow ? 2 : 4) &&
                    args.ldx == (p.x_layout == kRow ? 2 : 4),
                "all native value arguments and hidden length");
  checks.Expect(args.original == a && args.factor == af && args.rhs == b &&
                    args.solution == x &&
                    args.forward == sample.ferr.data() + 1 &&
                    args.backward == sample.berr.data() + 1 &&
                    args.scalar_work == workspace.regions[kScalar].data() &&
                    args.auxiliary_work == workspace.regions[extra].data(),
                "all native operand and workspace pointers");
  checks.Expect(args.incoming_info == std::numeric_limits<Integer>::min(),
                "native INFO starts at full-width signed minimum");
}

bool Warning(Fault fault) {
  return fault == Fault::kNoForward || fault == Fault::kNoBackward ||
         fault == Fault::kNanForward || fault == Fault::kNanBackward ||
         fault == Fault::kInfiniteForward || fault == Fault::kInfiniteBackward;
}
std::int64_t Raw(Fault fault) {
  if (fault == Fault::kNegative) {
    return -7;
  }
  if (fault == Fault::kPositive) {
    return 1;
  }
  if (Warning(fault) || fault == Fault::kNegativeForward ||
      fault == Fault::kNegativeBackward) {
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
  const bool warning = Warning(fault);
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

template <typename Real>
bool ExpectedError(Real value, Fault fault, bool forward) {
  if (fault == (forward ? Fault::kNoForward : Fault::kNoBackward) ||
      fault == (forward ? Fault::kNanForward : Fault::kNanBackward)) {
    return std::isnan(value);
  }
  if (fault == (forward ? Fault::kInfiniteForward : Fault::kInfiniteBackward)) {
    return value == std::numeric_limits<Real>::infinity();
  }
  if (fault == (forward ? Fault::kNegativeForward : Fault::kNegativeBackward)) {
    return value == Real{-43};
  }
  return value == (forward ? Real{43} : Real{47});
}

template <typename T>
void CheckInjectedOutputs(Checks& checks, const Fixture<T>& sample,
                          const Fixture<T>& before, const Profile& p,
                          Fault fault) {
  for (int j = 0; j < 2; ++j) {
    const auto error = static_cast<std::size_t>(j) + 1;
    checks.Expect(
        ExpectedError(sample.ferr[error], fault, true) &&
            ExpectedError(sample.berr[error], fault, false),
        "retained direct FERR/BERR including missing-write sentinels");
    for (int i = 0; i < 2; ++i) {
      const auto at = Offset(p.x_layout, i, j);
      const bool publish = p.x_layout == kColumn || Warning(fault);
      checks.Expect(
          sample.solution[at] == (publish ? T{37} : before.solution[at]),
          "direct X retained; row X published only on completed refinement");
    }
  }
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
  asc_packed_cholesky_refinement_test::Arm(routine, fault);
  const auto calls = asc_packed_cholesky_refinement_test::Calls(routine);
  asc_lapack_test::BeginAllocationAudit();
  asc_dense_test::AllocationProbe allocations;
  const auto status = sample.Call(provider, profile, plan, workspace, report);
  const auto cpp_allocations = allocations.count();
  const auto libc_allocations = asc_lapack_test::EndAllocationAudit();
  asc_packed_cholesky_refinement_test::Disarm();
  checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0),
                "no observed C++ allocation");
  checks.Expect(libc_allocations == 0, "no wrapped libc allocation");
  checks.Expect(
      asc_packed_cholesky_refinement_test::Calls(routine) == calls + 1,
      "exact selected native route entered once");
  CheckReport(checks, report, status, fault);
  const auto arguments =
      asc_packed_cholesky_refinement_test::LastArguments(routine);
  checks.Expect(
      arguments.triangle == (profile.triangle == kUpper ? 'U' : 'L') &&
          arguments.order == 2 && arguments.character_length == 1,
      "complete native order/triangle/CHARACTER length");
  checks.Expect(report.provider == provider.identity(),
                "injected report preserves exact provider provenance");
  checks.Expect(report.routine == ExpectedName<T>(),
                "injected report names the exact scalar packed routine");
  checks.Expect(sample.InputsEqual(before),
                "immutable AP/AFP/B on injected outcomes");
  CheckInjectedOutputs(checks, sample, before, profile, fault);
  CheckArguments(checks, sample, profile, workspace);
  sample.CheckGuards(plan, profile, checks);
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
  kStaleFactorLayout,
  kStaleRhsLayout,
  kStaleSolutionLayout,
  kFactorAlias,
  kRhsAlias,
  kSolutionAlias,
  kBackwardAlias,
  kStaleOrder,
  kStaleRoutine,
  kFactorOrder,
  kRhsRows,
  kSolutionRows,
  kErrorStride,
  kRhsColumns,
  kSolutionColumns,
  kForwardLength,
  kBackwardLength,
  kBackwardStride,
  kStaleRhsStride,
  kStaleSolutionStride,
  kShortPacking,
  kPackingAlignment,
  kShortAuxiliary,
  kAuxiliaryAlignment,
  kForwardAlias,
  kProviderAlias,
  kByteLimit,
  kRegionAlignment
};

std::size_t TotalCalls() {
  std::size_t total = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    total += asc_packed_cholesky_refinement_test::Calls(i);
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
                           ? std::string_view{"pprfs"}
                           : plan.identity.routine();

  const asc::extent_t order = rejection == Rejection::kStaleOrder ? 1 : 2;
  plan.identity = Take(asc::LapackPlanIdentity::Create(
      routine, scalar,
      std::array<asc::extent_t, 6>{order, 2, profile.b_layout == kRow ? 2 : 4,
                                   profile.x_layout == kRow ? 2 : 4, 2, 2},
      std::array<std::int64_t, 9>{static_cast<std::int64_t>(profile.triangle),
                                  static_cast<std::int64_t>(profile.a_layout),
                                  static_cast<std::int64_t>(profile.af_layout),
                                  static_cast<std::int64_t>(profile.b_layout),
                                  static_cast<std::int64_t>(profile.x_layout),
                                  4, 4, 1, 1},
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
      profile.a_layout = profile.a_layout == kRow ? kColumn : kRow;
      break;
    case Rejection::kStaleFactorLayout:
      profile.af_layout = profile.af_layout == kRow ? kColumn : kRow;
      break;
    case Rejection::kStaleRhsLayout:
      profile.b_layout = profile.b_layout == kRow ? kColumn : kRow;
      break;
    case Rejection::kStaleSolutionLayout:
      profile.x_layout = profile.x_layout == kRow ? kColumn : kRow;
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
std::optional<RejectionExpectation> ConfigureShapeRejection(
    Fixture<T>& sample, Rejection rejection) {
  switch (rejection) {
    case Rejection::kFactorOrder:
      sample.factor_order = 1;
      break;
    case Rejection::kRhsRows:
      sample.b_rows = 1;
      break;
    case Rejection::kSolutionRows:
      sample.x_rows = 1;
      break;
    case Rejection::kErrorStride:
      sample.error_stride = 2;
      return RejectionExpectation{asc::ErrorCode::kInvalidArgument, false};
    case Rejection::kRhsColumns:
      sample.b_columns = 1;
      break;
    case Rejection::kSolutionColumns:
      sample.x_columns = 1;
      break;
    case Rejection::kForwardLength:
      sample.forward_size = 1;
      break;
    case Rejection::kBackwardLength:
      sample.backward_size = 1;
      break;
    case Rejection::kBackwardStride:
      sample.backward_stride = 2;
      return RejectionExpectation{asc::ErrorCode::kInvalidArgument, false};
    case Rejection::kStaleRhsStride:
      sample.b_leading = 5;
      return RejectionExpectation{asc::ErrorCode::kInvalidState, false};
    case Rejection::kStaleSolutionStride:
      sample.x_leading = 5;
      return RejectionExpectation{asc::ErrorCode::kInvalidState, false};
    default:
      return std::nullopt;
  }
  return RejectionExpectation{asc::ErrorCode::kShape, false};
}

template <typename T>
std::optional<RejectionExpectation> ConfigureOperandAlias(
    Fixture<T>& sample, asc::LapackWorkspace& workspace, Rejection rejection) {
  switch (rejection) {
    case Rejection::kOperandAlias:
      workspace.regions[6] = {sample.original.data(), sizeof(sample.original),
                              kHost};
      break;
    case Rejection::kFactorAlias:
      workspace.regions[6] = {sample.factors.data(), sizeof(sample.factors),
                              kHost};
      break;
    case Rejection::kRhsAlias:
      workspace.regions[6] = {sample.rhs.data(), sizeof(sample.rhs), kHost};
      break;
    case Rejection::kSolutionAlias:
      workspace.regions[6] = {sample.solution.data(), sizeof(sample.solution),
                              kHost};
      break;
    case Rejection::kForwardAlias:
      workspace.regions[6] = {sample.ferr.data() + 1,
                              2 * sizeof(sample.ferr[1]), kHost};
      break;
    case Rejection::kBackwardAlias:
      workspace.regions[6] = {sample.berr.data() + 1,
                              2 * sizeof(sample.berr[1]), kHost};
      break;
    default:
      return std::nullopt;
  }
  return RejectionExpectation{asc::ErrorCode::kInvalidArgument, false};
}
std::optional<RejectionExpectation> ConfigurePackingRejection(
    asc::LapackWorkspace& workspace, Rejection rejection) {
  if (rejection != Rejection::kShortPacking &&
      rejection != Rejection::kPackingAlignment) {
    return std::nullopt;
  }
  const auto region = workspace.regions[kLayout];
  ASC_CHECK(region.size() != 0);
  if (rejection == Rejection::kShortPacking) {
    workspace.regions[kLayout] = {region.data(), region.size() - 1, kHost};
  } else {
    workspace.regions[kLayout] = {static_cast<std::byte*>(region.data()) + 1,
                                  region.size(), kHost};
  }
  return RejectionExpectation{asc::ErrorCode::kInvalidArgument, false};
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
  if (auto shape = ConfigureShapeRejection(sample, rejection)) {
    return *shape;
  }
  if (auto alias = ConfigureOperandAlias(sample, workspace, rejection)) {
    return *alias;
  }
  if (auto packing = ConfigurePackingRejection(workspace, rejection)) {
    return *packing;
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
  asc_packed_cholesky_refinement_test::Disarm();
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
                         Rejection::kStaleFactorLayout,
                         Rejection::kStaleRhsLayout,
                         Rejection::kStaleSolutionLayout,
                         Rejection::kFactorAlias,
                         Rejection::kRhsAlias,
                         Rejection::kSolutionAlias,
                         Rejection::kBackwardAlias,
                         Rejection::kStaleProvider,
                         Rejection::kStaleScalar,
                         Rejection::kStaleOrder,
                         Rejection::kStaleRoutine,
                         Rejection::kWorkspaceOverlap,
                         Rejection::kFactorOrder,
                         Rejection::kRhsRows,
                         Rejection::kSolutionRows,
                         Rejection::kErrorStride,
                         Rejection::kRhsColumns,
                         Rejection::kSolutionColumns,
                         Rejection::kForwardLength,
                         Rejection::kBackwardLength,
                         Rejection::kBackwardStride,
                         Rejection::kStaleRhsStride,
                         Rejection::kStaleSolutionStride,
                         Rejection::kShortAuxiliary,
                         Rejection::kAuxiliaryAlignment,
                         Rejection::kForwardAlias,
                         Rejection::kProviderAlias,
                         Rejection::kByteLimit,
                         Rejection::kRegionAlignment}) {
    Reject<T>(checks, provider, profile, rejection);
  }
  Reject<T>(checks, provider, profile, Rejection::kShort);
  Reject<T>(checks, provider, profile, Rejection::kAlignment);
  if (profile.a_layout == kRow || profile.af_layout == kRow ||
      profile.b_layout == kRow || profile.x_layout == kRow) {
    Reject<T>(checks, provider, profile, Rejection::kShortPacking);
    Reject<T>(checks, provider, profile, Rejection::kPackingAlignment);
  }
}

template <typename T>
void CheckRealOutputs(Checks& checks, const Fixture<T>& sample,
                      const Profile& profile) {
  const std::array<T, 4> truth{T{1}, T{1}, T{-1}, T{0.5}};
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 2; ++i) {
      checks.Expect(
          std::abs(sample.solution[Offset(profile.x_layout, i, j)] -
                   truth[2 * static_cast<std::size_t>(j) +
                         static_cast<std::size_t>(i)]) <=
              64 * std::numeric_limits<typename Fixture<T>::Real>::epsilon(),
          "real independently known refined solution");
    }
    const auto at = static_cast<std::size_t>(j) + 1;
    checks.Expect(std::isfinite(sample.ferr[at]) && sample.ferr[at] >= 0 &&
                      std::isfinite(sample.berr[at]) && sample.berr[at] >= 0,
                  "real finite nonnegative error estimates");
  }
}

template <typename T>
void RealCalls(Checks& checks, const asc::ReferenceLapackProvider& provider,
               const Profile& profile) {
  for (int repeat = 0; repeat < 2; ++repeat) {
    Fixture<T> sample(profile);
    const auto before = sample;
    const auto routine = Routine<T>();
    asc_packed_cholesky_refinement_test::Disarm();
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
        asc_packed_cholesky_refinement_test::LastArguments(routine);
    checks.Expect(
        arguments.triangle == (profile.triangle == kUpper ? 'U' : 'L') &&
            arguments.order == 2 && arguments.character_length == 1,
        "real full-width native arguments");
    checks.Expect(report.routine == ExpectedName<T>(),
                  "real report names the exact scalar packed routine");
    checks.Expect(asc_test::ProcessAllocationCountMatches(cpp_allocations, 0) &&
                      libc_allocations == 0,
                  "initial/repeated real call has no observed allocation");
    checks.Expect(asc_packed_cholesky_refinement_test::Calls(routine) != 0 &&
                      TotalCalls() == calls + 1,
                  "initial/repeated call enters exactly one native route");
    checks.Expect(
        status.ok() && report.called_provider && report.native_info == 0 &&
            report.outcome == asc::LapackOutcome::kSuccess &&
            report.output_validity == asc::LapackOutputValidity::kComplete &&
            !report.diagnostic_index && !report.native_argument &&
            !report.factor_family,
        "initial/repeated real success retains exact report semantics");
    checks.Expect(sample.InputsEqual(before),
                  "real call preserves all AP/AFP/B bytes");
    CheckRealOutputs(checks, sample, profile);
    CheckArguments(checks, sample, profile, workspace);
    sample.CheckGuards(plan, profile, checks);
    checks.ProfileDone();
  }
}

template <typename T>
void AliasOperands(Views<T>& views, Fixture<T>& sample, const Profile& profile,
                   std::size_t first, std::size_t second, std::size_t offset) {
  using Real = asc::DenseBlasRealType<T>;
  ASC_CHECK(first < second && second < 6 && offset < 2);
  if (first == 4) {
    views.e = Take(asc::DenseBlasVectorView<Real>::Create(
        sample.ferr.data() + 1 + offset, 2, 1,
        {sample.ferr.data(), sizeof(sample.ferr), kHost}));
    return;
  }
  const std::array<T*, 4> bases{sample.original.data(), sample.factors.data(),
                                sample.rhs.data(), sample.solution.data()};
  const std::array sizes{sizeof(sample.original), sizeof(sample.factors),
                         sizeof(sample.rhs), sizeof(sample.solution)};
  T* data = bases[first] + 1 + offset;
  const asc::MutableMemoryView backing{bases[first], sizes[first], kHost};
  if (second == 1) {
    views.af = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        data, 2, profile.af_layout, backing));
  } else if (second == 2) {
    views.b = Take(asc::DenseBlasMatrixView<const T>::Create(
        data, 2, 2, profile.b_layout, 4, backing));
  } else if (second == 3) {
    views.x = Take(asc::DenseBlasMatrixView<T>::Create(
        data, 2, 2, profile.x_layout, 4, backing));
  } else {
    // C++20's complex array access guarantee permits accessing an array of
    // complex<Real> as interleaved Real components. The backing remains the
    // real containing T array, and preflight reads no numerical components.
    Real* components = reinterpret_cast<Real*>(bases[first] + 1) + offset;
    auto alias =
        Take(asc::DenseBlasVectorView<Real>::Create(components, 2, 1, backing));
    if (second == 4) {
      views.f = alias;
    } else {
      views.e = alias;
    }
  }
}

template <typename T>
void RejectOperands(Checks& checks,
                    const asc::ReferenceLapackProvider& provider,
                    const Profile& profile, std::size_t first,
                    std::size_t second, std::size_t offset) {
  Fixture<T> sample(profile);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider, profile));
  const auto workspace = sample.Workspace(plan);
  auto views = sample.Operands(profile);
  AliasOperands(views, sample, profile, first, second, offset);
  asc_packed_cholesky_refinement_test::Disarm();
  const auto calls = TotalCalls();
  auto report = DirtyReport();
  asc_dense_test::AllocationProbe allocations;
  const auto query =
      asc::QueryPprfsWorkspace(provider, profile.triangle, views.a, views.af,
                               views.b, views.x, views.f, views.e);
  checks.Expect(
      !query.ok() && query.status().code() == asc::ErrorCode::kInvalidArgument,
      "query rejects every exact and partial operand alias class");
  checks.Expect(
      sample.NumericBytesEqual(before) && TotalCalls() == calls,
      "alias query preserves all numeric bytes and makes no foreign call");
  const auto status =
      asc::Pprfs(provider, profile.triangle, views.a, views.af, views.b,
                 views.x, views.f, views.e, plan, workspace, report);
  const auto count = allocations.count();
  checks.Expect(
      status.code() == asc::ErrorCode::kInvalidArgument,
      "execution rejects every exact and partial operand alias class");
  checks.Expect(
      sample.NumericBytesEqual(before) && TotalCalls() == calls,
      "alias execution preserves all numeric bytes and makes no foreign call");
  checks.Expect(asc_test::ProcessAllocationCountMatches(count, 0),
                "mutual alias preflight has no observed C++ allocation");
  checks.Expect(
      !report.called_provider && !report.native_info &&
          report.outcome == asc::LapackOutcome::kNotRun &&
          report.output_validity == asc::LapackOutputValidity::kUnchanged &&
          !report.diagnostic_index && !report.native_argument &&
          !report.factor_family,
      "mutual alias preflight resets dirty diagnostics");
  checks.Expect(report.provider == provider.identity() &&
                    report.routine == ExpectedName<T>(),
                "mutual alias report preserves provider and routine identity");
  checks.ProfileDone();
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const Profile profile{
          triangle, bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      RealCalls<T>(checks, provider, profile);
      for (auto fault :
           {Fault::kNoWrite, Fault::kNegative, Fault::kPositive,
            Fault::kLowZero, Fault::kLowOnes, Fault::kNoForward,
            Fault::kNoBackward, Fault::kNegativeForward,
            Fault::kNegativeBackward, Fault::kNanForward, Fault::kNanBackward,
            Fault::kInfiniteForward, Fault::kInfiniteBackward}) {
        Inject<T>(checks, provider, profile, fault);
      }
      Preflight<T>(checks, provider, profile);
      for (std::size_t first = 0; first < 5; ++first) {
        for (std::size_t second = first + 1; second < 6; ++second) {
          for (std::size_t offset = 0; offset < 2; ++offset) {
            RejectOperands<T>(checks, provider, profile, first, second, offset);
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
