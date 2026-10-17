#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_equilibration_radix.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kHost = asc::MemorySpace::kHost;

class Checks {
 public:
  void Expect(bool condition, const char* description) {
    ++checks_;
    if (!condition) {
      ++failures_;
      std::fprintf(stderr, "GBEQUB public consumer: %s\n", description);
    }
  }
  void Workflow() { ++workflows_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "GBEQUB public consumer: %d workflows, %d checks, %d failures; "
        "required upstream mathematical gates remain separate.\n",
        workflows_, checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  int checks_ = 0;
  int failures_ = 0;
  int workflows_ = 0;
};

template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "sgbequb";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dgbequb";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "cgbequb";
  } else {
    return "zgbequb";
  }
}

template <typename T>
constexpr asc::LapackScalarKind ScalarKind() {
  if constexpr (std::is_same_v<T, float>) {
    return asc::LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<T, double>) {
    return asc::LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return asc::LapackScalarKind::kC64;
  } else {
    return asc::LapackScalarKind::kC128;
  }
}

struct Bandwidth {
  asc::extent_t lower;
  asc::extent_t upper;
  asc::stride_t leading;
};

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  using Statistics = asc::LapackEquilibrationStatistics<Real>;
  // The largest fixture has ld*n=30 live coefficients, plus two guards.
  std::array<T, 32> matrix;
  std::array<Real, 5> rows;
  std::array<Real, 5> columns;
  std::array<Statistics, 3> statistics;
  Bandwidth bandwidth;
  asc::extent_t m;
  asc::extent_t n;

  Fixture(Bandwidth selected, T phase, asc::extent_t row_count = 3,
          asc::extent_t column_count = 3)
      : bandwidth(selected), m(row_count), n(column_count) {
    matrix.fill(Value<T>(1024));
    rows.fill(Real{-101});
    columns.fill(Real{-103});
    statistics.fill({Real{-401}, Real{-409}, Real{-419}});
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = std::max<asc::extent_t>(0, j - bandwidth.upper);
           i < std::min(m, j + bandwidth.lower + 1); ++i) {
        Coefficient(i, j) = T{};
      }
    }
    constexpr std::array<Real, 3> kDiagonal{2, 8, 32};
    for (asc::extent_t i = 0; i < std::min(m, n); ++i) {
      Coefficient(i, i) = phase * kDiagonal[static_cast<std::size_t>(i)];
    }
  }

  T& Coefficient(asc::extent_t i, asc::extent_t j) {
    const auto offset = 1 + bandwidth.upper + i - j + bandwidth.leading * j;
    return matrix[static_cast<std::size_t>(offset)];
  }
  [[nodiscard]] auto Matrix() const {
    return Take(asc::ReferenceGeneralBandView<const T>::Create(
        matrix.data() + 1, m, n, bandwidth.lower, bandwidth.upper,
        bandwidth.leading, {matrix.data(), sizeof(matrix), kHost}));
  }
  auto Rows(asc::extent_t count = -1) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        rows.data() + 1, count < 0 ? m : count, 1,
        {rows.data(), sizeof(rows), kHost}));
  }
  auto Columns() {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        columns.data() + 1, n, 1, {columns.data(), sizeof(columns), kHost}));
  }
};

template <typename Real>
bool SameStatistics(const asc::LapackEquilibrationStatistics<Real>& left,
                    const asc::LapackEquilibrationStatistics<Real>& right) {
  return left.row_condition == right.row_condition &&
         left.column_condition == right.column_condition &&
         left.absolute_maximum == right.absolute_maximum;
}

template <typename T>
void Unchanged(Checks& checks, const Fixture<T>& sample,
               const Fixture<T>& before) {
  checks.Expect(sample.matrix == before.matrix,
                "all AB entries/guards unchanged");
  checks.Expect(sample.rows == before.rows, "all R entries unchanged");
  checks.Expect(sample.columns == before.columns, "all C entries unchanged");
  for (std::size_t i = 0; i < sample.statistics.size(); ++i) {
    checks.Expect(SameStatistics(sample.statistics[i], before.statistics[i]),
                  "statistics and guards unchanged");
  }
}

template <typename T>
void Guards(Checks& checks, const Fixture<T>& sample,
            const Fixture<T>& before) {
  checks.Expect(sample.matrix == before.matrix, "immutable AB and all padding");
  for (std::size_t i = 0; i < sample.rows.size(); ++i) {
    if (i == 0 || i > static_cast<std::size_t>(sample.m)) {
      checks.Expect(sample.rows[i] == before.rows[i], "R surrounding guard");
    }
    if (i == 0 || i > static_cast<std::size_t>(sample.n)) {
      checks.Expect(sample.columns[i] == before.columns[i],
                    "C surrounding guard");
    }
  }
  for (const std::size_t i : {0, 2}) {
    checks.Expect(SameStatistics(sample.statistics[i], before.statistics[i]),
                  "statistics surrounding guard");
  }
}

template <typename T>
asc::LapackWorkspacePlan Query(Checks& checks,
                               const asc::ReferenceLapackProvider& provider,
                               Fixture<T>& sample) {
  const auto before = sample;
  auto plan =
      Take(asc::QueryGbequbWorkspace(provider, sample.Matrix(), sample.Rows(),
                                     sample.Columns(), sample.statistics[1]));
  Unchanged(checks, sample, before);
  const auto identity = Take(asc::LapackPlanIdentity::Create(
      Routine<T>(), ScalarKind<T>(),
      std::array{sample.m, sample.n, sample.bandwidth.lower,
                 sample.bandwidth.upper, sample.bandwidth.leading, sample.m,
                 asc::extent_t{1}, sample.n, asc::extent_t{1}},
      std::array<std::int64_t, 0>{}, provider.identity()));
  checks.Expect(plan.identity == identity, "complete query identity");
  checks.Expect(
      plan.total_byte_limit == std::numeric_limits<std::size_t>::max(),
      "zero-workspace byte budget");
  for (const auto& region : plan.regions) {
    checks.Expect(region.minimum_entries == 0 &&
                      region.preferred_entries == 0 &&
                      region.entry_bytes == 1 && region.alignment == 1,
                  "exact zero-workspace region");
  }
  return plan;
}

asc::LapackReport PoisonedReport() {
  asc::LapackReport report;
  report.routine.fill('!');
  report.called_provider = true;
  report.native_info = 777;
  report.outcome = asc::LapackOutcome::kSuccess;
  report.output_validity = asc::LapackOutputValidity::kComplete;
  report.diagnostic_index = 99;
  report.native_argument = 11;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  return report;
}

template <typename T>
void Report(Checks& checks, const asc::ReferenceLapackProvider& provider,
            const asc::LapackReport& report, std::optional<std::int64_t> info,
            asc::LapackOutcome outcome, asc::LapackOutputValidity validity,
            std::optional<asc::index_t> diagnostic = std::nullopt) {
  std::array<char, 32> name{};
  const auto routine = Routine<T>();
  std::copy(routine.begin(), routine.end(), name.begin());
  checks.Expect(report.routine == name,
                "complete report routine and terminator");
  checks.Expect(report.provider == provider.identity(),
                "complete provider identity");
  checks.Expect(report.called_provider == info.has_value(),
                "provider call flag");
  checks.Expect(report.native_info == info, "exact raw INFO presence/value");
  checks.Expect(report.outcome == outcome, "report numerical outcome");
  checks.Expect(report.output_validity == validity, "report output validity");
  checks.Expect(report.diagnostic_index == diagnostic,
                "report diagnostic index");
  checks.Expect(!report.native_argument.has_value(),
                "no stale native argument");
  checks.Expect(!report.factor_family.has_value(), "no factor certification");
}

template <typename T>
void Regular(Checks& checks, const asc::ReferenceLapackProvider& provider,
             Bandwidth bandwidth, T phase) {
  Fixture<T> sample(bandwidth, phase);
  const auto before = sample;
  const auto plan = Query(checks, provider, sample);
  asc::LapackWorkspace workspace;
  auto report = PoisonedReport();
  const auto status =
      asc::Gbequb(provider, sample.Matrix(), sample.Rows(), sample.Columns(),
                  sample.statistics[1], plan, workspace, report);
  checks.Expect(status.ok(), "power-of-two equilibration succeeds");
  Report<T>(checks, provider, report, 0, asc::LapackOutcome::kSuccess,
            asc::LapackOutputValidity::kComplete);
  constexpr std::array<long double, 3> kScales{0.5L, 0.125L, 0.03125L};
  for (std::size_t i = 0; i < kScales.size(); ++i) {
    checks.Expect(sample.rows[i + 1] == kScales[i], "independent exact R");
    checks.Expect(sample.columns[i + 1] == 1, "independent exact C");
  }
  const auto& stats = sample.statistics[1];
  checks.Expect(stats.row_condition == 0.0625L, "independent min(R)/max(R)");
  checks.Expect(stats.column_condition == 1, "independent min(C)/max(C)");
  checks.Expect(stats.absolute_maximum == 32,
                "independent original abs1 maximum");
  Guards(checks, sample, before);
  checks.Workflow();
}

template <typename T>
void Empty(Checks& checks, const asc::ReferenceLapackProvider& provider,
           asc::extent_t m, asc::extent_t n) {
  Fixture<T> sample({4, 3, 10}, Value<T>(1), m, n);
  const auto before = sample;
  const auto plan = Query(checks, provider, sample);
  auto report = PoisonedReport();
  const asc::LapackWorkspace workspace;
  const auto status =
      asc::Gbequb(provider, sample.Matrix(), sample.Rows(), sample.Columns(),
                  sample.statistics[1], plan, workspace, report);
  checks.Expect(status.ok(), "empty shape succeeds with full ld*n backing");
  Report<T>(checks, provider, report, 0, asc::LapackOutcome::kSuccess,
            asc::LapackOutputValidity::kComplete);
  checks.Expect(sample.rows == before.rows, "empty R unchanged");
  checks.Expect(sample.columns == before.columns, "empty C unchanged");
  const auto& stats = sample.statistics[1];
  checks.Expect(stats.row_condition == 1 && stats.column_condition == 1 &&
                    stats.absolute_maximum == 0,
                "empty native diagnostics");
  Guards(checks, sample, before);
  checks.Workflow();
}

template <typename T>
void Partial(Checks& checks, const asc::ReferenceLapackProvider& provider,
             bool zero_column) {
  Fixture<T> sample({1, 1, 5}, Value<T>(1));
  sample.Coefficient(2, 2) = T{};
  if (zero_column) {
    sample.Coefficient(2, 1) = Value<T>(32);
  }
  const auto before = sample;
  const auto plan = Query(checks, provider, sample);
  const asc::LapackWorkspace workspace;
  auto report = PoisonedReport();
  const auto status =
      asc::Gbequb(provider, sample.Matrix(), sample.Rows(), sample.Columns(),
                  sample.statistics[1], plan, workspace, report);
  checks.Expect(status.code() == asc::ErrorCode::kNumerical,
                "zero-scale status");
  Report<T>(checks, provider, report, zero_column ? 6 : 3,
            asc::LapackOutcome::kSingular,
            asc::LapackOutputValidity::kDocumentedPartial, 2);
  const auto& stats = sample.statistics[1];
  checks.Expect(stats.column_condition == -409, "partial COLCND unchanged");
  if (zero_column) {
    checks.Expect(sample.rows[1] == 0.5L && sample.rows[2] == 0.125L &&
                      sample.rows[3] == 0.03125L,
                  "zero-column R remains usable");
    checks.Expect(
        stats.row_condition == 0.0625L && stats.absolute_maximum == 32,
        "zero-column ROWCND/AMAX remain usable");
  } else {
    checks.Expect(sample.columns == before.columns, "zero-row C unchanged");
    checks.Expect(stats.row_condition == -401 && stats.absolute_maximum == 8,
                  "zero-row ROWCND unchanged and AMAX available");
  }
  Guards(checks, sample, before);
  checks.Workflow();
}

template <typename T>
void Preflight(Checks& checks, const asc::ReferenceLapackProvider& provider,
               bool stale) {
  Fixture<T> sample({1, 1, 5}, Value<T>(1));
  const auto before = sample;
  auto plan = Query(checks, provider, sample);
  if (stale) {
    --plan.total_byte_limit;
  }
  const asc::LapackWorkspace workspace;
  auto report = PoisonedReport();
  const auto status = asc::Gbequb(
      provider, sample.Matrix(), sample.Rows(stale ? 3 : 2), sample.Columns(),
      sample.statistics[1], plan, workspace, report);
  checks.Expect(status.code() == (stale ? asc::ErrorCode::kInvalidState
                                        : asc::ErrorCode::kShape),
                "stale-plan or live short-vector rejection");
  Report<T>(checks, provider, report, std::nullopt, asc::LapackOutcome::kNotRun,
            asc::LapackOutputValidity::kUnchanged);
  Unchanged(checks, sample, before);
  checks.Workflow();
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  const std::array phases{Value<T>(1), Value<T>(-1), Value<T>(0, 1),
                          Value<T>(0, -1)};
  const std::size_t count = asc::DenseBlasComplex<T> ? 4 : 2;
  for (const auto bandwidth :
       {Bandwidth{0, 0, 3}, Bandwidth{1, 1, 5}, Bandwidth{4, 3, 10}}) {
    for (std::size_t i = 0; i < count; ++i) {
      Regular(checks, provider, bandwidth, phases[i]);
    }
  }
  Empty<T>(checks, provider, 0, 3);
  Empty<T>(checks, provider, 3, 0);
  Empty<T>(checks, provider, 0, 0);
  Partial<T>(checks, provider, false);
  Partial<T>(checks, provider, true);
  Preflight<T>(checks, provider, false);
  Preflight<T>(checks, provider, true);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  return checks.Finish();
}
