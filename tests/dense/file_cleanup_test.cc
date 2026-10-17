#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <span>
#include <utility>

#include "../array_display_test_support.h"
#include "../array_io/file_cleanup_test_support.h"
#include "asc/core/array_io.h"
#include "asc/core/contracts.h"
#include "asc/core/extents.h"
#include "asc/core/matrix_market.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#include "asc/dense/matrix_market.h"

namespace {
using asc_file_cleanup_test::Fault;
using asc_file_cleanup_test::Opener;
using asc_file_cleanup_test::Resource;
using asc_file_cleanup_test::TestContext;

template <typename T, typename Shape>
struct Adapter {
  using Report = asc::ArrayIoReport;
  using Owner = asc::DenseArray<T, Shape>;
  int format;  // 0=text, 1=binary, 2=Matrix Market.
  static const asc::ArrayIoReport& Io(const Report& report) { return report; }
  [[nodiscard]] asc::ArrayIoSection MalformedCloseSection(
      asc::ArrayIoSection original) const {
    return format == 2 ? asc::ArrayIoSection::kTrailer : original;
  }
  asc::Status Save(const std::filesystem::path& path, const Owner& owner,
                   Fault* fault, const asc::ArrayIoLimits& limits,
                   Report& report) const {
    auto view = owner.view();
    ASC_CHECK(view.ok());
    std::array<std::byte, 1024> scratch{};
    constexpr auto kOverwrite = asc::ArrayFileOverwrite::kTruncate;
    constexpr auto kSymmetry = asc::MatrixMarketSymmetry::kGeneral;
    if (format == 2) {
      if (fault == nullptr) {
        return asc::SaveDenseMatrixMarket(path, *view, kSymmetry, kOverwrite,
                                          limits, scratch, report);
      }
      return asc::internal_dense_matrix_market::Save(
          path, *view, kSymmetry, kOverwrite, limits, scratch, report,
          Opener(*fault, true));
    }
    if (fault == nullptr) {
      return format == 1 ? asc::SaveDenseArrayBinary(path, *view, kOverwrite,
                                                     limits, scratch, report)
                         : asc::SaveDenseArrayText(path, *view, kOverwrite,
                                                   limits, scratch, report);
    }
    return asc::internal_dense_io::Save(path, *view, kOverwrite, limits,
                                        scratch, report, format == 1,
                                        Opener(*fault, true));
  }
  asc::Result<Owner> Load(const std::filesystem::path& path, Resource& resource,
                          Fault* fault, Report& report) const {
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, Shape::rank()> metadata{};
    if (format == 2) {
      if (fault == nullptr) {
        return asc::LoadDenseMatrixMarket<T, Shape>(
            path, resource, asc::LayoutLeft{}, scratch, {}, report);
      }
      return asc::internal_dense_matrix_market::Load<T, Shape>(
          path, resource, asc::LayoutLeft{}, scratch, {}, report,
          Opener(*fault, false));
    }
    if (fault == nullptr) {
      return format == 1 ? asc::LoadDenseArrayBinary<T, Shape>(
                               path, resource, asc::LayoutLeft{}, metadata,
                               scratch, {}, report)
                         : asc::LoadDenseArrayText<T, Shape>(
                               path, resource, asc::LayoutLeft{}, metadata,
                               scratch, {}, report);
    }
    return asc::internal_dense_io::Load<T, Shape>(
        path, resource, asc::LayoutLeft{}, metadata, scratch, {}, report,
        format == 1, Opener(*fault, false));
  }
};

template <typename T, std::size_t... Index>
void Ranks(TestContext& test, const std::filesystem::path& path,
           std::index_sequence<Index...> /*indices*/) {
  using Shape =
      asc::Extents<(static_cast<void>(Index), asc::kDynamicExtent)...>;
  constexpr std::size_t kRank = sizeof...(Index);
  for (bool empty : {false, true}) {
    if (empty && kRank == 0) {
      continue;  // A Dense scalar has one value.
    }
    std::array<asc::extent_t, kRank> dimensions{};
    dimensions.fill(2);
    if constexpr (kRank != 0) {
      if (empty) {
        dimensions[0] = 0;
      }
    }
    auto shape = Shape::Create(std::span<const asc::extent_t>(dimensions));
    ASC_CHECK(shape.ok());
    Resource resource;
    {
      auto owner = asc::DenseArray<T, Shape>::Create(resource, *shape);
      ASC_CHECK(owner.ok());
      for (int format : {0, 1, 2}) {
        if (format == 2 && kRank != 2) {
          continue;
        }
        asc_file_cleanup_test::Profile(test, path, Adapter<T, Shape>{format},
                                       *owner);
        std::printf(
            "cleanup dense rank=%zu empty=%d scalar_bytes=%zu format=%d\n",
            kRank, empty, sizeof(T), format);
      }
    }
    test.Check(resource.live == 0, "fixture owner resources released");
  }
}

template <typename T>
void AllRanks(TestContext& test, const std::filesystem::path& path) {
  Ranks<T>(test, path, std::make_index_sequence<0>{});
  Ranks<T>(test, path, std::make_index_sequence<1>{});
  Ranks<T>(test, path, std::make_index_sequence<2>{});
  Ranks<T>(test, path, std::make_index_sequence<3>{});
  Ranks<T>(test, path, std::make_index_sequence<4>{});
}
}  // namespace

int main(int argc, char** argv) {
  ASC_CHECK(argc == 2);
  const std::filesystem::path scratch(argv[1]);
  // CTest supplies a dedicated out-of-source directory; retain files on error.
  ASC_CHECK(std::filesystem::is_directory(scratch));
  TestContext test;
  AllRanks<double>(test, scratch / "dense-cleanup.asc");
  AllRanks<std::complex<double>>(test, scratch / "dense-complex-cleanup.asc");
  return test.Finish();
}
