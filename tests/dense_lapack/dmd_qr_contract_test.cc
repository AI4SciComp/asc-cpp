#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "asc/dense/providers/lapack_qr.h"
#include "dmd_qr_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace support = asc_dmd_qr_test;
using support::Take;
using support::TestContext;
using Wide = std::complex<long double>;
using Vectors = asc::LapackDmdQrVectors;
using Extra = asc::LapackDmdExtra;
using Svd = asc::LapackDmdSvd;
using Scaling = asc::LapackDmdScaling;
constexpr std::array kLayouts{
    support::kRow, support::kColumn, support::kRow, support::kColumn,
    support::kRow, support::kColumn, support::kRow};
template <typename T>
long double Bound() {
  return 512 * std::numeric_limits<support::Real<T>>::epsilon();
}
template <typename T>
void Q(TestContext& test, const asc::ReferenceLapackProvider& provider,
       const support::Problem<T>& p, bool explicit_q, support::Rhs<T>& q) {
  for (asc::extent_t j = 0; j < q.columns; ++j) {
    for (asc::extent_t i = 0; i < q.rows; ++i) {
      q.At(i, j) =
          explicit_q ? p.f.At(i, j) : T{static_cast<support::Real<T>>(i == j)};
    }
  }
  if (explicit_q) {
    return;
  }
  asc::LapackReport report;
  const auto query = [&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryUnmqrWorkspace(
          provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
          p.f.View(), support::Vector(p.tau, p.f.columns), q.View(), report);
    } else {
      return asc::QueryOrmqrWorkspace(
          provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
          p.f.View(), support::Vector(p.tau, p.f.columns), q.View(), report);
    }
  };
  const auto plan = Take(query());
  support::Scratch<T> scratch(plan, false);
  const auto status = [&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::Unmqr(provider, asc::DenseBlasSide::kLeft,
                        asc::DenseBlasTranspose::kNone, p.f.View(),
                        support::Vector(p.tau, p.f.columns), q.View(), plan,
                        scratch.workspace, report);
    } else {
      return asc::Ormqr(provider, asc::DenseBlasSide::kLeft,
                        asc::DenseBlasTranspose::kNone, p.f.View(),
                        support::Vector(p.tau, p.f.columns), q.View(), plan,
                        scratch.workspace, report);
    }
  }();
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  scratch.Guards(test);
}
template <typename T>
void CheckQr(TestContext& test, const support::Problem<T>& p,
             const support::Problem<T>& before, const support::Rhs<T>& q,
             asc::LapackDmdQrOptions options) {
  const auto bound = Bound<T>();
  for (asc::extent_t j = 0; j < q.columns; ++j) {
    for (asc::extent_t h = 0; h < q.columns; ++h) {
      Wide gram{};
      for (asc::extent_t i = 0; i < q.rows; ++i) {
        gram += std::conj(Wide(q.At(i, h))) * Wide(q.At(i, j));
      }
      ASC_DENSE_TEST_CHECK(test, std::abs(gram - Wide(j == h)) < bound);
    }
    for (asc::extent_t i = 0; i < q.rows; ++i) {
      Wide restored{};
      for (asc::extent_t h = 0; h < q.columns; ++h) {
        T r{};
        if (options.triangular) {
          r = p.y.At(h, j);
        } else if (h <= j) {
          r = p.f.At(h, j);
        }
        restored += Wide(q.At(i, h)) * Wide(r);
      }
      // Packed F retains R unless the explicit-Q option overwrites it.
      if (options.triangular || !options.orthogonal) {
        ASC_DENSE_TEST_CHECK(
            test, std::abs(restored - Wide(before.f.At(i, j))) <
                      bound * std::max(1.L, std::abs(Wide(before.f.At(i, j)))));
      }
    }
  }
}
template <typename T>
void CheckModes(TestContext& test, const support::Problem<T>& p,
                const support::Rhs<T>& q, asc::LapackDmdQrOptions options) {
  const auto bound = Bound<T>();
  const std::array<Wide, 5> diagonal_values{support::Value<T>(2, 1),
                                            support::Value<T>(3, -1)};
  for (asc::extent_t j = 0; j < p.rank; ++j) {
    Wide norm{};
    long double residual = 0;
    for (asc::extent_t i = 0; i < p.f.rows; ++i) {
      Wide mode{};
      if (options.vectors == Vectors::kExplicit) {
        mode = p.z.At(i, j);
      }
      if (options.vectors == Vectors::kPodFactored) {
        for (asc::extent_t h = 0; h < p.rank; ++h) {
          mode += Wide(p.z.At(i, h)) * Wide(p.v.At(h, j));
        }
      }
      const Wide diagonal = diagonal_values[static_cast<std::size_t>(i)];
      norm += std::norm(mode);
      residual += std::norm((diagonal - Wide(p.eigen[j + 1])) * mode);
      if (options.extra != Extra::kNone) {
        Wide extra{};
        Wide pod{};
        for (asc::extent_t h = 0; h < q.columns; ++h) {
          extra += Wide(q.At(i, h)) * Wide(p.b.At(h, j));
          Wide compressed = p.x.At(h, j);
          if (options.extra == Extra::kExact) {
            compressed = 0;
            for (asc::extent_t l = 0; l < p.rank; ++l) {
              compressed += Wide(p.x.At(h, l)) * Wide(p.v.At(l, j));
            }
          }
          pod += Wide(q.At(i, h)) * compressed;
        }
        ASC_DENSE_TEST_CHECK(test, std::abs(extra - diagonal * pod) < bound);
      }
    }
    if (options.vectors != Vectors::kNone) {
      ASC_DENSE_TEST_CHECK(test, std::abs(norm - Wide(1)) < bound &&
                                     std::sqrt(residual) < bound);
    }
    if (options.residuals) {
      ASC_DENSE_TEST_CHECK(
          test, std::abs(p.residual[j + 1] - std::sqrt(residual)) < bound);
    }
  }
}
template <typename T>
void CheckSpectrum(TestContext& test, const support::Problem<T>& p,
                   const support::Problem<T>& before,
                   asc::LapackDmdQrOptions options, support::Real<T> scale) {
  const auto bound = Bound<T>();
  // Two distinct eigenvalues make ordering irrelevant; paired invariants
  // independently identify the projected operator.
  const Wide a = support::Value<T>(2, 1);
  const Wide b = support::Value<T>(3, -1);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(Wide(p.eigen[1]) + Wide(p.eigen[2]) - a - b) < bound);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(Wide(p.eigen[1]) * Wide(p.eigen[2]) - a * b) < bound);
  // The column scaling is defined on the original snapshot pair, and QR
  // preserves its two-norm. Compute the scaled Frobenius invariant without
  // using the returned singular values or provider scratch as the oracle.
  long double frobenius = 0;
  for (asc::extent_t j = 0; j < p.f.columns - 1; ++j) {
    long double snapshot_norm = 0;
    long double scaling_norm = 0;
    const auto selected = options.scaling == Scaling::kSuccessors ? j + 1 : j;
    for (asc::extent_t i = 0; i < p.f.rows; ++i) {
      snapshot_norm += std::norm(Wide(before.f.At(i, j)));
      scaling_norm += std::norm(Wide(before.f.At(i, selected)));
    }
    frobenius += options.scaling == Scaling::kNone
                     ? snapshot_norm
                     : snapshot_norm / scaling_norm;
  }
  long double sigma = 0;
  for (asc::extent_t j = 0; j < p.f.columns - 1; ++j) {
    sigma += p.singular[j + 1] * static_cast<long double>(p.singular[j + 1]);
  }
  ASC_DENSE_TEST_CHECK(test, std::abs(sigma - frobenius) < bound * frobenius);
  if (options.scaling == Scaling::kNone) {
    ASC_DENSE_TEST_CHECK(test, p.singular[1] > scale);
  }
}
template <typename T>
void CheckPublication(TestContext& test, const support::Problem<T>& p,
                      const support::Problem<T>& before,
                      const support::Rhs<T>& q,
                      asc::LapackDmdQrOptions options) {
  // Full caller buffers remain defined objects. Check inactive outputs,
  // rank tails, and padding separately from the mathematical properties.
  if (options.vectors == Vectors::kNone && options.extra != Extra::kExact) {
    ASC_DENSE_TEST_CHECK(test, p.v.data == before.v.data);
  }
  if (!options.triangular) {
    ASC_DENSE_TEST_CHECK(test, p.y.data == before.y.data);
  }
  if (options.vectors == Vectors::kNone) {
    ASC_DENSE_TEST_CHECK(test, p.z.data == before.z.data);
  }
  if (options.extra == Extra::kNone) {
    ASC_DENSE_TEST_CHECK(test, p.b.data == before.b.data);
  }
  if (!options.residuals) {
    ASC_DENSE_TEST_CHECK(test, p.residual == before.residual);
  }
  const auto tails = [&](const auto& actual, const auto& old,
                         asc::extent_t rows, asc::extent_t columns) {
    for (asc::extent_t j = 0; j < actual.columns; ++j) {
      for (asc::extent_t i = 0; i < actual.rows; ++i) {
        if (i >= rows || j >= columns) {
          ASC_DENSE_TEST_EQ(test, actual.At(i, j), old.At(i, j));
        }
      }
    }
  };
  tails(p.x, before.x, p.x.rows, p.rank);
  tails(p.z, before.z, p.z.rows,
        options.vectors == Vectors::kNone ? 0 : p.rank);
  tails(p.b, before.b, p.b.rows, options.extra == Extra::kNone ? 0 : p.rank);
  tails(p.v, before.v, p.rank, p.rank);
  tails(p.s, before.s, p.rank, p.rank);
  const auto vector_tail = [&](const auto& actual, const auto& old,
                               asc::extent_t count) {
    for (std::size_t i = 0; i < actual.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(count)) {
        ASC_DENSE_TEST_EQ(test, actual[i], old[i]);
      }
    }
  };
  vector_tail(p.eigen, before.eigen, p.rank);
  vector_tail(p.singular, before.singular, p.f.columns - 1);
  vector_tail(p.residual, before.residual, options.residuals ? p.rank : 0);
  vector_tail(p.tau, before.tau, p.f.columns);
  p.Guards(test);
  q.Guards(test);
}
template <typename T>
void Check(TestContext& test, const support::Problem<T>& p,
           const support::Problem<T>& before, const support::Rhs<T>& q,
           asc::LapackDmdQrOptions options, support::Real<T> scale) {
  ASC_DENSE_TEST_EQ(test, p.rank, 2);
  if (p.rank != 2) {
    return;
  }
  CheckQr(test, p, before, q, options);
  CheckModes(test, p, q, options);
  CheckSpectrum(test, p, before, options, scale);
  CheckPublication(test, p, before, q, options);
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::LapackDmdQrOptions options, bool reverse, bool preferred,
          const asc::LapackWorkspacePlan* shared = nullptr,
          support::Real<T> scale = 1) {
  auto layouts = kLayouts;
  if (reverse) {
    for (auto& layout : layouts) {
      layout = layout == support::kRow ? support::kColumn : support::kRow;
    }
  }
  support::Problem<T> p(5, 4, layouts);
  p.Initialize();
  for (asc::extent_t j = 0; j < p.f.columns; ++j) {
    for (asc::extent_t i = 0; i < p.f.rows; ++i) {
      p.f.At(i, j) *= scale;
    }
  }
  const auto before = p;
  const auto tolerance =
      support::Real<T>{32} * std::numeric_limits<support::Real<T>>::epsilon();
  const auto local = Take(
      asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), tolerance));
  const auto& plan = shared == nullptr ? local : *shared;
  support::Scratch<T> scratch(plan, preferred);
  asc::LapackReport report;
  const auto status = asc::Gedmdq(provider, options, p.Buffers(), tolerance,
                                  p.rank, plan, scratch.workspace, report);
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  if (!status.ok()) {
    return;
  }
  const auto after = p;
  support::Rhs<T> q(5, 4, reverse ? support::kColumn : support::kRow);
  Q(test, provider, p, options.orthogonal, q);
  ASC_DENSE_TEST_CHECK(test, p.f.data == after.f.data && p.tau == after.tau);
  Check(test, p, before, q, options, scale);
  scratch.Guards(test);
}
template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int svd = 1; svd <= 4; ++svd) {
    for (auto vectors :
         {Vectors::kNone, Vectors::kExplicit, Vectors::kPodFactored}) {
      for (int qr = 0; qr < 4; ++qr) {
        for (asc::index_t rank : {-1, -2, 2}) {
          auto extra = Extra::kNone;
          if (vectors == Vectors::kExplicit) {
            extra = Extra::kRefinement;
          }
          if (vectors == Vectors::kPodFactored) {
            extra = Extra::kExact;
          }
          const asc::LapackDmdQrOptions options{
              static_cast<Scaling>(std::array{'N', 'S', 'C', 'Y'}[qr]),
              vectors,
              extra,
              static_cast<Svd>(svd),
              vectors == Vectors::kExplicit,
              (qr & 1) != 0,
              (qr & 2) != 0,
              rank};
          Case<T>(test, provider, options, (svd & 1) != 0, (qr & 1) != 0);
        }
      }
    }
  }
  const auto options_for = [](std::size_t i) {
    return asc::LapackDmdQrOptions{Scaling::kNone,
                                   Vectors::kExplicit,
                                   Extra::kRefinement,
                                   static_cast<Svd>(i + 1),
                                   true,
                                   false,
                                   true,
                                   -1};
  };
  const auto plan_for = [&](std::size_t i) {
    support::Problem<T> p(5, 4, kLayouts);
    return Take(asc::QueryGedmdqWorkspace(
        provider, options_for(i), p.Buffers(),
        support::Real<T>{32} *
            std::numeric_limits<support::Real<T>>::epsilon()));
  };
  const std::array plans{plan_for(0), plan_for(1), plan_for(2), plan_for(3)};
  std::array<std::thread, 4> workers;
  std::array<int, 4> failures{};
  for (std::size_t i = 0; i < workers.size(); ++i) {
    workers[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      for (int j = 0; j < 4; ++j) {
        Case<T>(local, independent, options_for(i), false, true, &plans[i],
                static_cast<support::Real<T>>(i + 1));
      }
      failures[i] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (int failure : failures) {
    ASC_DENSE_TEST_EQ(test, failure, 0);
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
