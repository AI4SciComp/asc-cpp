#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>
#include <string_view>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/blas.h"
#include "asc/dense/io.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/print.h"
#include "asc/dense/view.h"

#if defined(ASC_CPP_EXAMPLE_LAPACK)
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#endif

namespace {

using MatrixShape = asc::Extents<3, 3>;
using RhsShape = asc::Extents<3, 2>;
using RhsOwner = asc::DenseArray<double, RhsShape>;

class StandardOutput final : public asc::ByteSink {
 public:
  asc::Result<std::size_t> WriteSome(
      std::span<const std::byte> bytes) override {
    if (bytes.empty()) {
      return std::size_t{0};
    }
    const auto count = std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    if (count == 0) {
      return asc::Status(asc::ErrorCode::kIo);
    }
    return count;
  }
};

bool Check(const asc::Status& status, const char* operation) {
  if (status.ok()) {
    return true;
  }
  const auto name = asc::ErrorCodeName(status.code());
  std::fprintf(stderr, "%s failed: %.*s\n", operation,
               static_cast<int>(name.size()), name.data());
  return false;
}

template <typename T>
bool Check(const asc::Result<T>& result, const char* operation) {
  return Check(result.status(), operation);
}

template <typename T, std::size_t Size>
asc::Result<asc::DenseBlasMatrixView<T>> Matrix(std::span<T, Size> values,
                                                asc::extent_t columns) {
  return asc::DenseBlasMatrixView<T>::Create(
      values.data(), 3, columns, asc::DenseBlasLayout::kColumnMajor, 3,
      {values.data(), values.size_bytes(), asc::MemorySpace::kHost});
}

asc::Result<asc::DenseBlasVectorView<asc::index_t>> PivotOutput(
    std::array<asc::index_t, 3>& pivots) {
  return asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data(), 3, 1,
      {pivots.data(), sizeof(pivots), asc::MemorySpace::kHost});
}

asc::Result<asc::LapackLuFactorView<double>> SuccessfulFactor(
    asc::DenseBlasMatrixView<const double> matrix,
    const std::array<asc::index_t, 3>& pivots,
    const asc::LapackReport& report) {
  auto raw = asc::RawLapackPivotView::Create(
      pivots.data(), 3, asc::LapackFactorFamily::kLuPartialPivot,
      {pivots.data(), sizeof(pivots), asc::MemorySpace::kHost});
  if (!raw.ok()) {
    return raw.status();
  }
  return asc::LapackLuFactorView<double>::Create(matrix, *raw, report);
}

bool PrintReport(const char* label, const asc::LapackReport& report) {
  if (std::printf("%s: outcome=%u complete=%s called_provider=%s INFO=", label,
                  static_cast<unsigned>(report.outcome),
                  report.output_validity == asc::LapackOutputValidity::kComplete
                      ? "yes"
                      : "no",
                  report.called_provider ? "yes" : "no") < 0) {
    return false;
  }
  if (report.native_info.has_value()) {
    if (std::printf("%lld", static_cast<long long>(*report.native_info)) < 0) {
      return false;
    }
  } else if (std::fputs("absent", stdout) == EOF) {
    return false;
  }
  if (report.diagnostic_index.has_value() &&
      std::printf(" diagnostic_index=%lld",
                  static_cast<long long>(*report.diagnostic_index)) < 0) {
    return false;
  }
  return std::fputc('\n', stdout) != EOF;
}

asc::Status Factor(const asc::ExecutionContext& context,
                   asc::DenseBlasMatrixView<double> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::LapackReport& report) {
  return asc::Getrf(context, matrix, pivots, report);
}

asc::Status Solve(const asc::ExecutionContext& context,
                  asc::LapackLuFactorView<double> factor,
                  asc::DenseBlasMatrixView<double> rhs,
                  asc::LapackReport& report) {
  return asc::Getrs(context, asc::DenseBlasTranspose::kNone, factor, rhs,
                    report);
}

#if defined(ASC_CPP_EXAMPLE_LAPACK)
struct WorkspaceBytes {
  // Fixed caller storage for this tiny example, not a general query allocator.
  alignas(std::max_align_t) std::array<std::byte, 512> values{};
};

asc::Result<asc::LapackWorkspace> Workspace(
    const asc::LapackWorkspacePlan& plan, WorkspaceBytes& bytes) {
  asc::LapackWorkspace workspace;
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    const auto& requirement = plan.regions[role];
    auto capacity = asc::CheckedMultiply(
        static_cast<std::size_t>(requirement.preferred_entries),
        requirement.entry_bytes);
    if (!capacity.ok()) {
      return capacity.status();
    }
    if (*capacity > 64 || requirement.alignment > alignof(std::max_align_t)) {
      return asc::Status(asc::ErrorCode::kAllocation);
    }
    if (*capacity != 0) {
      workspace.regions[role] = asc::MutableMemoryView(
          bytes.values.data() + role * 64, *capacity, asc::MemorySpace::kHost);
    }
  }
  return workspace;
}

asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasMatrixView<double> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::LapackReport& report) {
  auto plan = asc::QueryGetrfWorkspace(provider, matrix, pivots);
  if (!plan.ok()) {
    return plan.status();
  }
  WorkspaceBytes bytes;
  auto workspace = Workspace(*plan, bytes);
  if (!workspace.ok()) {
    return workspace.status();
  }
  return asc::Getrf(provider, matrix, pivots, *plan, *workspace, report);
}

asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::LapackLuFactorView<double> factor,
                  asc::DenseBlasMatrixView<double> rhs,
                  asc::LapackReport& report) {
  auto plan = asc::QueryGetrsWorkspace(provider, asc::DenseBlasTranspose::kNone,
                                       factor, rhs);
  if (!plan.ok()) {
    return plan.status();
  }
  WorkspaceBytes bytes;
  auto workspace = Workspace(*plan, bytes);
  if (!workspace.ok()) {
    return workspace.status();
  }
  return asc::Getrs(provider, asc::DenseBlasTranspose::kNone, factor, rhs,
                    *plan, *workspace, report);
}

bool PrintIdentity(const asc::ReferenceLapackProvider& provider) {
  const auto& identity = provider.identity();
  if (std::printf("Reference LAPACK %u.%u.%u integer_abi=%u\nsource_sha256=",
                  identity.version[0], identity.version[1], identity.version[2],
                  static_cast<unsigned>(identity.integer_abi)) < 0) {
    return false;
  }
  for (std::byte byte : identity.source_sha256) {
    if (std::printf("%02x", std::to_integer<unsigned>(byte)) < 0) {
      return false;
    }
  }
  if (std::fputs("\nbuild_sha256=", stdout) == EOF) {
    return false;
  }
  for (std::byte byte : identity.build_sha256) {
    if (std::printf("%02x", std::to_integer<unsigned>(byte)) < 0) {
      return false;
    }
  }
  return std::fputc('\n', stdout) != EOF;
}
#endif

template <std::size_t Size>
bool SameBits(std::span<const double, Size> left,
              std::span<const double, Size> right) {
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (std::bit_cast<std::uint64_t>(left[i]) !=
        std::bit_cast<std::uint64_t>(right[i])) {
      return false;
    }
  }
  return true;
}

long double ScaledResidual(std::span<const double, 9> matrix,
                           std::span<const double, 6> rhs,
                           std::span<const double, 6> solution) {
  long double norm_a = 0;
  long double norm_b = 0;
  long double norm_x = 0;
  long double norm_r = 0;
  for (std::size_t row = 0; row < 3; ++row) {
    long double a_sum = 0;
    long double b_sum = 0;
    long double x_sum = 0;
    long double r_sum = 0;
    for (std::size_t column = 0; column < 3; ++column) {
      a_sum += std::abs(static_cast<long double>(matrix[row + 3 * column]));
    }
    for (std::size_t column = 0; column < 2; ++column) {
      long double product = 0;
      for (std::size_t inner = 0; inner < 3; ++inner) {
        product += static_cast<long double>(matrix[row + 3 * inner]) *
                   solution[inner + 3 * column];
      }
      r_sum += std::abs(product - rhs[row + 3 * column]);
      b_sum += std::abs(static_cast<long double>(rhs[row + 3 * column]));
      x_sum += std::abs(static_cast<long double>(solution[row + 3 * column]));
    }
    norm_a = std::max(norm_a, a_sum);
    norm_b = std::max(norm_b, b_sum);
    norm_x = std::max(norm_x, x_sum);
    norm_r = std::max(norm_r, r_sum);
  }
  const long double scale = norm_a * norm_x + norm_b;
  if (scale == 0) {
    return norm_r == 0 ? 0 : std::numeric_limits<long double>::infinity();
  }
  return norm_r / scale;
}

bool VerifyNumerics(std::span<const double, 9> original_a,
                    std::span<const double, 6> original_b,
                    std::span<const double, 6> solution,
                    std::span<const double, 6> repeated) {
  constexpr std::array<double, 6> kExpected{1, 2, -1, -2, 0, 3};
  constexpr double kTolerance = 256 * std::numeric_limits<double>::epsilon();
  const auto residual = ScaledResidual(original_a, original_b, solution);
  const auto repeated_residual =
      ScaledResidual(original_a, original_b, repeated);
  if (!std::isfinite(residual) || residual > kTolerance ||
      !std::isfinite(repeated_residual) || repeated_residual > kTolerance) {
    return false;
  }
  for (std::size_t i = 0; i < kExpected.size(); ++i) {
    if (!std::isfinite(solution[i]) || !std::isfinite(repeated[i]) ||
        std::abs(solution[i] - kExpected[i]) > 3 * kTolerance ||
        std::abs(solution[i] - repeated[i]) > 3 * kTolerance) {
      return false;
    }
  }
  return std::printf("scaled infinity-norm residual = %.5Le; reuse = %.5Le\n",
                     residual, repeated_residual) >= 0;
}

template <typename Owner>
bool PrintInput(const Owner& owner, const char* label) {
  StandardOutput output;
  std::array<std::byte, 2048> scratch{};
  asc::ArrayPrintReport report;
  return std::printf("%s\n", label) >= 0 &&
         Check(asc::PrintArray(owner, output, {}, scratch, report), label) &&
         !report.truncated;
}

bool PrintAndRoundTrip(const RhsOwner& owner, const std::filesystem::path& path,
                       asc::MemoryResource& resource) {
  std::array<std::byte, 2048> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoReport io_report;
  asc::ArrayPrintReport print_report;
  StandardOutput output;
  auto view = owner.view();
  if (!Check(view, "solution view")) {
    return false;
  }
  if (!Check(asc::PrintArray(owner, output, {}, scratch, print_report),
             "print X") ||
      print_report.truncated) {
    return false;
  }
  if (!Check(asc::SaveDenseArrayBinary(path, *view,
                                       asc::ArrayFileOverwrite::kTruncate, {},
                                       scratch, io_report),
             "save X (explicit truncate)")) {
    return false;
  }
  const auto saved_bytes = io_report.output_bytes;
  auto restored = asc::LoadDenseArrayBinary<double, RhsShape>(
      path, resource, asc::LayoutLeft{}, metadata, scratch, {}, io_report);
  if (!Check(restored, "reload X") || !io_report.committed) {
    return false;
  }
  auto copy = restored->view();
  if (!Check(copy, "reloaded X view")) {
    return false;
  }
  if (!SameBits(std::span<const double, 6>(view->data(), 6),
                std::span<const double, 6>(copy->data(), 6))) {
    return false;
  }
  return std::printf("binary round-trip verified: saved=%zu read=%zu bytes\n",
                     saved_bytes, io_report.input_bytes) >= 0;
}

bool VerifyRollback(RhsOwner& owner, const std::filesystem::path& corrupt) {
  auto view = owner.view();
  if (!Check(view, "rollback target view")) {
    return false;
  }
  std::array<double, 6> original{};
  std::copy_n(view->data(), original.size(), original.begin());
  std::array<double, 6> staging{};
  std::array<std::byte, 2048> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoReport report;
  auto file = asc::File::OpenRead(corrupt);
  if (!Check(file, "open corrupt fixture")) {
    return false;
  }
  auto reader = asc::DenseArrayReader::PrepareText(*file, metadata, scratch, {},
                                                   report, true);
  if (!reader.ok()) {
    const auto close = file->Close();
    Check(close, "close rejected fixture");
    return Check(reader, "prepare corrupt fixture");
  }
  const auto status =
      asc::ReadDenseArrayInto(*reader, *view, std::span(staging));
  const auto close = file->Close();
  if (!Check(close, "close corrupt fixture")) {
    return false;
  }
  if (status.ok() || report.committed || report.values_processed != 6 ||
      report.section != asc::ArrayIoSection::kTrailer ||
      !SameBits(std::span<const double, 6>(original),
                std::span<const double, 6>(view->data(), 6))) {
    return false;
  }
  return std::printf("staged rejection preserved X: consumed=%zu bytes\n",
                     report.input_bytes) >= 0;
}

template <typename Route>
bool VerifySingular(const Route& route) {
  // Separate failure demonstration; this is not a refactorization of A.
  std::array<double, 9> singular{1, 2, 1, 2, 4, 1, 3, 6, 1};
  std::array<asc::index_t, 3> pivots{};
  auto matrix = Matrix(std::span(singular), 3);
  auto immutable = Matrix(std::span<const double, 9>(singular), 3);
  auto pivot_output = PivotOutput(pivots);
  if (!Check(matrix, "singular matrix descriptor") ||
      !Check(immutable, "singular const descriptor") ||
      !Check(pivot_output, "singular pivots")) {
    return false;
  }
  asc::LapackReport report;
  const auto status = Factor(route, *matrix, *pivot_output, report);
  if (!PrintReport("separate singular GETRF", report)) {
    return false;
  }
  if (status.code() != asc::ErrorCode::kNumerical ||
      report.outcome != asc::LapackOutcome::kSingular ||
      report.output_validity != asc::LapackOutputValidity::kDocumentedPartial) {
    return false;
  }
  return !SuccessfulFactor(*immutable, pivots, report).ok();
}

template <typename Route>
bool FactorAndSolve(const Route& route, asc::DenseView<double, 2> factors,
                    asc::DenseView<double, 2> solution,
                    asc::DenseView<double, 2> repeated) {
  auto matrix = Matrix(std::span<double, 9>(factors.data(), 9), 3);
  auto immutable = Matrix(std::span<const double, 9>(factors.data(), 9), 3);
  auto rhs = Matrix(std::span<double, 6>(solution.data(), 6), 2);
  auto again = Matrix(std::span<double, 6>(repeated.data(), 6), 2);
  std::array<asc::index_t, 3> pivots{};
  auto pivot_output = PivotOutput(pivots);
  if (!Check(matrix, "A descriptor") ||
      !Check(immutable, "const A descriptor") ||
      !Check(rhs, "two-RHS descriptor") ||
      !Check(again, "repeated B descriptor") ||
      !Check(pivot_output, "pivot descriptor")) {
    return false;
  }
  asc::LapackReport report;
  const auto factored = Factor(route, *matrix, *pivot_output, report);
  if (!PrintReport("GETRF (once)", report) || !Check(factored, "GETRF")) {
    return false;
  }
  auto factor = SuccessfulFactor(*immutable, pivots, report);
  if (!Check(factor, "successful reusable LU factor")) {
    return false;
  }
  auto solved = Solve(route, *factor, *rhs, report);
  if (!PrintReport("GETRS (two RHS)", report) || !Check(solved, "GETRS")) {
    return false;
  }
  solved = Solve(route, *factor, *again, report);
  return PrintReport("GETRS (reuse, original two RHS)", report) &&
         Check(solved, "repeated GETRS");
}

template <typename Route>
bool Run(const Route& route, const std::filesystem::path& a_path,
         const std::filesystem::path& b_path,
         const std::filesystem::path& corrupt,
         const std::filesystem::path& output) {
  // Every owner and clone explicitly uses this resource, retained until last.
  asc::HostMemoryResource resource;
  const auto context = asc::ExecutionContext::Serial();
  std::array<std::byte, 2048> scratch{};
  std::array<asc::extent_t, 2> metadata{};
  asc::ArrayIoReport report;
  auto original_a = asc::LoadDenseArrayText<double, MatrixShape>(
      a_path, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  if (!Check(original_a, "read A") || !report.committed) {
    return false;
  }
  auto original_b = asc::LoadDenseArrayText<double, RhsShape>(
      b_path, resource, asc::LayoutLeft{}, metadata, scratch, {}, report);
  if (!Check(original_b, "read B") || !report.committed) {
    return false;
  }
  if (!PrintInput(*original_a, "A") || !PrintInput(*original_b, "B")) {
    return false;
  }
  auto a = original_a->view();
  auto b = original_b->view();
  if (!Check(a, "original A view") || !Check(b, "original B view")) {
    return false;
  }
  std::array<double, 9> saved_a{};
  std::array<double, 6> saved_b{};
  std::copy_n(a->data(), saved_a.size(), saved_a.begin());
  std::copy_n(b->data(), saved_b.size(), saved_b.begin());
  auto factors = original_a->Clone(resource, context);
  auto solution = original_b->Clone(resource, context);
  auto repeated = original_b->Clone(resource, context);
  if (!Check(factors, "clone A") || !Check(solution, "clone B") ||
      !Check(repeated, "clone preserved B for reuse")) {
    return false;
  }
  auto factor_view = factors->view();
  auto solution_view = solution->view();
  auto repeated_view = repeated->view();
  if (!Check(factor_view, "factor view") || !Check(solution_view, "X view") ||
      !Check(repeated_view, "repeated X view")) {
    return false;
  }
  if (!FactorAndSolve(route, *factor_view, *solution_view, *repeated_view) ||
      !VerifyNumerics(saved_a, saved_b,
                      std::span<const double, 6>(solution_view->data(), 6),
                      std::span<const double, 6>(repeated_view->data(), 6))) {
    return false;
  }
  if (!SameBits(std::span<const double, 9>(saved_a),
                std::span<const double, 9>(a->data(), 9)) ||
      !SameBits(std::span<const double, 6>(saved_b),
                std::span<const double, 6>(b->data(), 6))) {
    return false;
  }
  return PrintAndRoundTrip(*solution, output, resource) &&
         VerifyRollback(*solution, corrupt) && VerifySingular(route);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 7 || std::string_view(argv[6]) != "--truncate") {
    std::fprintf(stderr,
                 "usage: %s native|reference A.asc B.asc corrupt.asc X.ascb "
                 "--truncate\n",
                 argv[0]);
    return 2;
  }
  const std::string_view route(argv[1]);
  bool success = false;
  if (route == "native") {
    if (std::fputs("Explicit native LU; no reference provider selected\n",
                   stdout) == EOF) {
      return 1;
    }
    success = Run(asc::ExecutionContext::Serial(), argv[2], argv[3], argv[4],
                  argv[5]);
  } else if (route == "reference") {
#if defined(ASC_CPP_EXAMPLE_LAPACK)
    auto provider =
        asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial());
    if (!Check(provider, "select reference provider") ||
        !PrintIdentity(*provider)) {
      return 1;
    }
    success = Run(*provider, argv[2], argv[3], argv[4], argv[5]);
#else
    std::fputs("Reference route was not explicitly built; no native fallback\n",
               stderr);
    return 2;
#endif
  } else {
    std::fputs("Unknown route; select native or reference explicitly\n",
               stderr);
    return 2;
  }
  if (!success || std::fflush(stdout) != 0) {
    return 1;
  }
  return 0;
}
