#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_support_test_support.h"
namespace {
namespace support = asc_matrix_support_test;
namespace numerical = asc_matrix_support_numerical;
using support::TestContext;
template <typename T, bool Rows>
int NativeExpected(TestContext& test, const support::Matrix<T>& matrix,
                   int pattern, T marker) {
  if (matrix.rows == 0 || matrix.columns == 0) {
    return 0;
  }
  support::Matrix<T> native(matrix.rows, matrix.columns, false, 2);
  native.Fill(pattern, marker);
  const auto before = numerical::Bits(native.data);
  std::array<lapack_int, 3> m{71, matrix.rows, 73};
  std::array<lapack_int, 3> n{71, matrix.columns, 73};
  std::array<lapack_int, 3> ld{71, native.leading, 73};
  const auto original_m = m;
  const auto original_n = n;
  const auto original_ld = ld;
  const auto result =
      support::Native<T, Rows>(m[1], n[1], native.data.data() + 2, ld[1]);
  ASC_DENSE_TEST_EQ(test, m, original_m);
  ASC_DENSE_TEST_EQ(test, n, original_n);
  ASC_DENSE_TEST_EQ(test, ld, original_ld);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(native.data), before);
  return static_cast<int>(result);
}
template <typename T, bool Rows>
void Report(TestContext& test, const asc::ReferenceLapackProvider& provider,
            const support::Matrix<T>& matrix, const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, report.called_provider,
                    matrix.rows != 0 && matrix.columns != 0);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()),
                    (support::Routine<T, Rows>()));
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
}
template <typename T, bool Rows>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          support::Matrix<T>& matrix, int pattern, T marker, bool fidelity,
          bool audit) {
  matrix.Fill(pattern, marker);
  const auto view = matrix.View();
  const auto query = [&] { return support::Query<T, Rows>(provider, view); };
  const auto plan =
      support::Take(audit ? support::WithoutAllocation(test, query) : query());
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto input_bytes = numerical::Bits(matrix.data);
  const auto provider_bytes = support::Bytes(provider);
  const auto plan_bytes = support::Bytes(plan);
  const auto workspace_bytes = support::Bytes(workspace);
  const int expected =
      fidelity ? NativeExpected<T, Rows>(test, matrix, pattern, marker)
               : numerical::Expected<Rows>(matrix.data.data() + 2, matrix.rows,
                                           matrix.columns, matrix.leading,
                                           matrix.row_major);
  std::array<asc::index_t, 3> result{71, -19, 73};
  asc::LapackReport report;
  ASC_DENSE_TEST_EQ(test, plan.regions[support::kLayout].minimum_entries,
                    matrix.row_major ? matrix.rows * matrix.columns : 0);
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto call = [&] {
      return support::Execute<T, Rows>(provider, view, result[1], plan,
                                       workspace, report);
    };
    const auto status = audit ? support::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, result[1], expected);
    ASC_DENSE_TEST_EQ(test, result[0], 71);
    ASC_DENSE_TEST_EQ(test, result[2], 73);
    Report<T, Rows>(test, provider, matrix, report);
    ASC_DENSE_TEST_EQ(test, numerical::Bits(matrix.data), input_bytes);
    ASC_DENSE_TEST_EQ(test, support::Bytes(provider), provider_bytes);
    ASC_DENSE_TEST_EQ(test, support::Bytes(plan), plan_bytes);
    ASC_DENSE_TEST_EQ(test, support::Bytes(workspace), workspace_bytes);
    ASC_DENSE_TEST_CHECK(test, scratch.Guards(workspace));
  }
}
template <typename T, bool Rows>
void Concurrent(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  std::array<std::thread, 4> threads;
  std::array<int, 4> failures{};
  for (std::size_t worker = 0; worker < threads.size(); ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      for (bool row : {false, true}) {
        support::Matrix<T> matrix(3, 5, row, 2);
        for (int pattern = 0; pattern < 18; ++pattern) {
          Case<T, Rows>(local, provider, matrix, pattern, T{-3}, false, false);
        }
      }
      failures[worker] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (int failure : failures) {
    ASC_DENSE_TEST_EQ(test, failure, 0);
  }
}
template <typename T, bool Rows>
void Run(TestContext& test, std::string_view mode) {
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "concurrency") {
    Concurrent<T, Rows>(test, provider);
    return;
  }
  std::size_t count = 0;
  for (bool row : {false, true}) {
    numerical::ForEach<T>(
        [&](int m, int n, int padding, int pattern, T marker) {
          support::Matrix<T> matrix(m, n, row, padding);
          Case<T, Rows>(test, provider, matrix, pattern, marker,
                        mode == "fidelity", true);
          ++count;
        });
  }
  std::printf("%zu matrix support fixtures; first and reused outputs\n", count);
}
template <typename T>
void Dispatch(TestContext& test, std::string_view operation,
              std::string_view mode) {
  if (operation == "ilalc") {
    Run<T, false>(test, mode);
  } else {
    Run<T, true>(test, mode);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 4) {
    return 2;
  }
  const std::string_view operation(argv[1]);
  const std::string_view scalar(argv[2]);
  const std::string_view mode(argv[3]);
  if ((operation != "ilalc" && operation != "ilalr") ||
      (mode != "mathematical" && mode != "fidelity" && mode != "concurrency")) {
    return 2;
  }
  TestContext test;
  if (scalar == "s") {
    Dispatch<float>(test, operation, mode);
  } else if (scalar == "d") {
    Dispatch<double>(test, operation, mode);
  } else if (scalar == "c") {
    Dispatch<std::complex<float>>(test, operation, mode);
  } else if (scalar == "z") {
    Dispatch<std::complex<double>>(test, operation, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
