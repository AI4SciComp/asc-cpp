#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_expert_info_faults.h"
#include "lu_expert_info_test_support.h"

namespace asc_lu_expert_info_test {
// Test-only routing; no production options or dispatch tables are introduced.
enum class Routine : std::uint8_t {
  kGetrf2,
  kGetf2,
  kGetriQuery,
  kGetri,
  kGesv
};
struct Mode {
  Routine routine;
  int m;
  int n;
  asc::DenseBlasLayout a;
  asc::DenseBlasLayout b = kColumn;
  int nrhs = 0;
  bool singular = false;
  bool preferred = false;
  Fault fault = Fault::kNone;
};
std::size_t g_cases = 0;

std::array<char, 16> Name(char scalar, Routine routine) {
  constexpr std::array<std::string_view, 5> kNames{
      "getrf2", "getf2", "getri.query", "getri", "gesv"};
  std::array<char, 16> result{};
  result[0] = scalar;
  const auto name = kNames[static_cast<std::size_t>(routine)];
  std::copy(name.begin(), name.end(), result.begin() + 1);
  return result;
}

bool Called(const Mode& mode) {
  return mode.routine == Routine::kGetriQuery || (mode.m != 0 && mode.n != 0);
}
bool ProducesFactor(const Mode& mode) {
  return mode.routine == Routine::kGetrf2 || mode.routine == Routine::kGetf2 ||
         mode.routine == Routine::kGesv;
}
int ActualInfo(const Mode& mode) {
  return Called(mode) && mode.singular && mode.routine != Routine::kGetriQuery
             ? std::min(mode.m, mode.n)
             : 0;
}
void LogMode(const Mode& mode, char scalar) {
  const auto name = Name(scalar, mode.routine);
  std::printf(
      "LU_EXPERT_INFO_MODE routine=%s m=%d n=%d a=%d b=%d nrhs=%d singular=%d "
      "preferred=%d fault=%d\n",
      name.data(), mode.m, mode.n, static_cast<int>(mode.a),
      static_cast<int>(mode.b), mode.nrhs, static_cast<int>(mode.singular),
      static_cast<int>(mode.preferred), static_cast<int>(mode.fault));
}

void CheckReport(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const asc::Status& status, const asc::LapackReport& report,
                 const Mode& mode, char scalar) {
  ++g_cases;
  const auto observed = asc_lapack_test::ObserveLuExpertInfo();
  const bool called = Called(mode);
  const bool query = mode.routine == Routine::kGetriQuery;
  const bool defect = called && mode.fault != Fault::kNone;
  const int actual = ActualInfo(mode);
  ASC_DENSE_TEST_EQ(test, observed.calls, called ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, observed.queries, query ? 1U : 0U);
  ASC_DENSE_TEST_EQ(test, observed.actual, actual);
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                    Name(scalar, mode.routine).data());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(),
                    ProducesFactor(mode));
  if (ProducesFactor(mode)) {
    ASC_DENSE_TEST_EQ(test, report.factor_family, kLu);
  }
  if (defect) {
    ASC_DENSE_TEST_EQ(test, observed.incoming, kSentinel);
    ASC_DENSE_TEST_EQ(test, observed.outgoing, kSentinel);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.native_info, kSentinel);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kProviderArgument);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      query ? asc::LapackOutputValidity::kUnchanged
                            : asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  } else {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        actual == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
    if (called) {
      ASC_DENSE_TEST_EQ(test, report.native_info, actual);
    } else {
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    }
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      actual == 0 ? asc::LapackOutcome::kSuccess
                                  : asc::LapackOutcome::kSingular);
    auto validity = asc::LapackOutputValidity::kComplete;
    if (query || (actual != 0 && !ProducesFactor(mode))) {
      validity = asc::LapackOutputValidity::kUnchanged;
    } else if (actual != 0) {
      validity = asc::LapackOutputValidity::kDocumentedPartial;
    }
    ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
    if (actual != 0) {
      ASC_DENSE_TEST_EQ(test, report.diagnostic_index, actual - 1);
    } else {
      ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    }
  }
  std::printf(
      "LU_EXPERT_INFO routine=%s calls=%zu queries=%zu actual=%lld "
      "incoming=%lld outgoing=%lld status=%d\n",
      Name(scalar, mode.routine).data(), observed.calls, observed.queries,
      static_cast<long long>(observed.actual),
      static_cast<long long>(observed.incoming),
      static_cast<long long>(observed.outgoing),
      static_cast<int>(status.code()));
}

asc::LapackReport DirtyReport() {
  asc::LapackReport report;
  report.native_info = 701;
  report.native_argument = 702;
  report.diagnostic_index = 703;
  report.factor_family = kLu;
  return report;
}

template <typename T>
auto FactorPlan(const asc::ReferenceLapackProvider& provider, Matrix<T>& matrix,
                std::array<asc::index_t, 6>& pivots, const Mode& mode) {
  const auto view = Pivots(pivots, std::min(mode.m, mode.n));
  return mode.routine == Routine::kGetrf2
             ? asc::QueryGetrf2Workspace(provider, matrix.View(), view)
             : asc::QueryGetf2Workspace(provider, matrix.View(), view);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   Matrix<T>& matrix, std::array<asc::index_t, 6>& pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report, const Mode& mode) {
  const auto view = Pivots(pivots, std::min(mode.m, mode.n));
  return mode.routine == Routine::kGetrf2
             ? asc::Getrf2(provider, matrix.View(), view, plan, workspace,
                           report)
             : asc::Getf2(provider, matrix.View(), view, plan, workspace,
                          report);
}

template <typename T>
void CheckPivots(TestContext& test, const std::array<asc::index_t, 6>& pivots,
                 const std::array<asc::index_t, 6>& before, int count) {
  ASC_DENSE_TEST_EQ(test, pivots.front(), before.front());
  for (std::size_t i = static_cast<std::size_t>(count) + 1; i < pivots.size();
       ++i) {
    ASC_DENSE_TEST_EQ(test, pivots[i], before[i]);
  }
}

template <typename T>
void FactorCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Mode mode,
                 char scalar) {
  Matrix<T> original(mode.m, mode.n, mode.a);
  Fill(original, mode.singular);
  auto positive = original;
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    mode.fault = fault;
    LogMode(mode, scalar);
    auto factor = original;
    std::array<asc::index_t, 6> pivots;
    pivots.fill(-503);
    const auto before = pivots;
    asc_lapack_test::SetLuExpertInfoFault(fault);
    const auto plan = Take(Observe(
        test, [&] { return FactorPlan(provider, factor, pivots, mode); }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuExpertInfo().calls, 0U);
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = DirtyReport();
    const auto status = Observe(test, [&] {
      return Factor(provider, factor, pivots, plan, workspace, report, mode);
    });
    CheckReport(test, provider, status, report, mode, scalar);
    scratch.CheckGuards(test);
    factor.CheckPadding(test);
    if (fault == Fault::kNone || !Called(mode)) {
      Reconstruction(test, original, factor, pivots);
      CheckPivots<T>(test, pivots, before, std::min(mode.m, mode.n));
      positive = factor;
    } else {
      ASC_DENSE_TEST_EQ(test, pivots, before);
      ASC_DENSE_TEST_EQ(test, factor.data,
                        mode.a == kRow ? original.data : positive.data);
    }
  }
}

template <typename T>
Matrix<T> RightHandSide(const Matrix<T>& matrix, const Mode& mode) {
  Matrix<T> rhs(mode.n, mode.nrhs, mode.b);
  for (int i = 0; i < mode.n; ++i) {
    for (int j = 0; j < mode.nrhs; ++j) {
      T sum{};
      for (int k = 0; k < mode.n; ++k) {
        sum += matrix.At(i, k) * Value<T>(k + j + 1, k - j);
      }
      rhs.At(i, j) = sum;
    }
  }
  return rhs;
}

template <typename T>
void CheckSolution(TestContext& test, const Matrix<T>& rhs) {
  for (int i = 0; i < rhs.rows; ++i) {
    for (int j = 0; j < rhs.columns; ++j) {
      ASC_DENSE_TEST_CHECK(
          test,
          std::abs(Wide(rhs.At(i, j)) - Wide(Value<T>(i + j + 1, i - j))) <=
              1024.L *
                  std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
    }
  }
}

template <typename T>
void DriverCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Mode mode,
                 char scalar) {
  Matrix<T> original(mode.n, mode.n, mode.a);
  Fill(original, mode.singular);
  const auto rhs_original = RightHandSide(original, mode);
  auto positive = original;
  auto positive_rhs = rhs_original;
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    mode.fault = fault;
    LogMode(mode, scalar);
    auto factor = original;
    auto rhs = rhs_original;
    std::array<asc::index_t, 6> pivots;
    pivots.fill(-503);
    const auto before = pivots;
    asc_lapack_test::SetLuExpertInfoFault(fault);
    const auto plan = Take(Observe(test, [&] {
      return asc::QueryGesvWorkspace(provider, factor.View(),
                                     Pivots(pivots, mode.n), rhs.View());
    }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuExpertInfo().calls, 0U);
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    auto report = DirtyReport();
    const auto status = Observe(test, [&] {
      return asc::Gesv(provider, factor.View(), Pivots(pivots, mode.n),
                       rhs.View(), plan, workspace, report);
    });
    CheckReport(test, provider, status, report, mode, scalar);
    scratch.CheckGuards(test);
    factor.CheckPadding(test);
    rhs.CheckPadding(test);
    if (fault == Fault::kNone || !Called(mode)) {
      Reconstruction(test, original, factor, pivots);
      CheckPivots<T>(test, pivots, before, mode.n);
      if (mode.singular) {
        ASC_DENSE_TEST_EQ(test, rhs.data, rhs_original.data);
      } else {
        CheckSolution(test, rhs);
      }
      positive = factor;
      positive_rhs = rhs;
    } else {
      ASC_DENSE_TEST_EQ(test, pivots, before);
      ASC_DENSE_TEST_EQ(test, factor.data,
                        mode.a == kRow ? original.data : positive.data);
      ASC_DENSE_TEST_EQ(test, rhs.data,
                        mode.b == kRow ? rhs_original.data : positive_rhs.data);
    }
  }
}

template <typename T>
void CheckInverse(TestContext& test, const Matrix<T>& matrix,
                  const Matrix<T>& inverse) {
  for (int side : {0, 1}) {
    for (int i = 0; i < matrix.rows; ++i) {
      for (int j = 0; j < matrix.rows; ++j) {
        decltype(Wide(T{})) sum{};
        for (int k = 0; k < matrix.rows; ++k) {
          sum += side == 0 ? Wide(matrix.At(i, k)) * Wide(inverse.At(k, j))
                           : Wide(inverse.At(i, k)) * Wide(matrix.At(k, j));
        }
        ASC_DENSE_TEST_CHECK(
            test,
            std::abs(sum - Wide(i == j ? T{1} : T{})) <=
                1024.L *
                    std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
      }
    }
  }
}

template <typename T>
void InverseExecute(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    const Matrix<T>& original, const Matrix<T>& factors,
                    const std::array<asc::index_t, 6>& pivots,
                    const asc::LapackWorkspacePlan& plan, Mode mode,
                    char scalar) {
  auto positive = factors;
  const auto pivots_before = pivots;
  mode.routine = Routine::kGetri;
  for (bool preferred : {false, true}) {
    mode.preferred = preferred;
    for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
      if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
        continue;
      }
      mode.fault = fault;
      LogMode(mode, scalar);
      auto inverse = factors;
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan, preferred);
      auto report = DirtyReport();
      asc_lapack_test::SetLuExpertInfoFault(fault);
      const auto status = Observe(test, [&] {
        return asc::Getri(provider, inverse.View(), Raw(pivots, mode.n), plan,
                          workspace, report);
      });
      CheckReport(test, provider, status, report, mode, scalar);
      scratch.CheckGuards(test);
      inverse.CheckPadding(test);
      ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
      if (mode.singular || !Called(mode)) {
        ASC_DENSE_TEST_EQ(test, inverse.data, factors.data);
      }
      if (fault == Fault::kNone || !Called(mode)) {
        if (!mode.singular) {
          CheckInverse(test, original, inverse);
        }
        positive = inverse;
      } else {
        ASC_DENSE_TEST_EQ(test, inverse.data,
                          mode.a == kRow ? factors.data : positive.data);
      }
    }
  }
}

template <typename T>
void InverseCases(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, Mode mode,
                  char scalar) {
  Matrix<T> original(mode.n, mode.n, mode.a);
  Fill(original, mode.singular);
  auto factors = original;
  std::array<asc::index_t, 6> pivots;
  pivots.fill(-503);
  asc_lapack_test::SetLuExpertInfoFault(Fault::kNone);
  const auto factor_plan = Take(asc::QueryGetrfWorkspace(
      provider, factors.View(), Pivots(pivots, mode.n)));
  Scratch<T> factor_scratch;
  const auto factor_workspace = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  const auto factor_status = Observe(test, [&] {
    return asc::Getrf(provider, factors.View(), Pivots(pivots, mode.n),
                      factor_plan, factor_workspace, factor_report);
  });
  ASC_DENSE_TEST_EQ(test, factor_status.code(),
                    mode.n != 0 && mode.singular ? asc::ErrorCode::kNumerical
                                                 : asc::ErrorCode::kOk);
  const auto unchanged = factors.data;
  const auto pivots_before = pivots;
  auto good_plan = factor_plan;
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    mode.fault = fault;
    LogMode(mode, scalar);
    Scratch<T> scratch;
    const auto workspace = scratch.QueryWorkspace(mode.n);
    auto report = DirtyReport();
    asc_lapack_test::SetLuExpertInfoFault(fault);
    const auto queried = Observe(test, [&] {
      return asc::QueryGetriWorkspace(provider, factors.View(),
                                      Raw(pivots, mode.n), workspace, report);
    });
    CheckReport(test, provider, queried.status(), report, mode, scalar);
    scratch.CheckGuards(test);
    ASC_DENSE_TEST_EQ(test, factors.data, unchanged);
    ASC_DENSE_TEST_EQ(test, pivots, pivots_before);
    ASC_DENSE_TEST_EQ(test, queried.ok(), fault == Fault::kNone);
    if (fault == Fault::kNone) {
      good_plan = Take(queried);
      ASC_DENSE_TEST_EQ(test, good_plan.regions[kScalar].minimum_entries,
                        std::max(1, mode.n));
      ASC_DENSE_TEST_CHECK(test, good_plan.regions[kScalar].preferred_entries >=
                                     std::max(1, mode.n));
    }
  }
  InverseExecute(test, provider, original, factors, pivots, good_plan, mode,
                 scalar);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         char scalar) {
  for (auto routine : {Routine::kGetrf2, Routine::kGetf2}) {
    for (const auto shape :
         {std::array{1, 1}, std::array{3, 3}, std::array{4, 3},
          std::array{3, 4}, std::array{0, 3}, std::array{3, 0}}) {
      for (auto layout : {kRow, kColumn}) {
        for (bool singular : {false, true}) {
          Mode mode{routine, shape[0], shape[1], layout};
          mode.singular = singular;
          FactorCases<T>(test, provider, mode, scalar);
        }
      }
    }
  }
  for (int n : {0, 1, 3}) {
    for (auto layout : {kRow, kColumn}) {
      for (bool singular : {false, true}) {
        Mode mode{Routine::kGetriQuery, n, n, layout};
        mode.singular = singular;
        InverseCases<T>(test, provider, mode, scalar);
        mode.routine = Routine::kGesv;
        for (auto rhs_layout : {kRow, kColumn}) {
          mode.b = rhs_layout;
          for (int nrhs : {0, 1, 2}) {
            mode.nrhs = nrhs;
            DriverCases<T>(test, provider, mode, scalar);
          }
        }
      }
    }
  }
}
}  // namespace asc_lu_expert_info_test

namespace lu_test = asc_lu_expert_info_test;

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = lu_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  lu_test::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    lu_test::Run<float>(test, provider, 's');
  } else if (scalar == "d") {
    lu_test::Run<double>(test, provider, 'd');
  } else if (scalar == "c") {
    lu_test::Run<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    lu_test::Run<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("LU_EXPERT_INFO_COMPLETED scalar=%s abi=%d cases=%zu\n", argv[1],
              ASC_LAPACK_INTEGER_BITS, lu_test::g_cases);
  return test.Finish();
}
