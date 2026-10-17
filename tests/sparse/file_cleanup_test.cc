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
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/io.h"
#include "asc/sparse/matrix_market.h"

namespace {
using asc_file_cleanup_test::Fault;
using asc_file_cleanup_test::Opener;
using asc_file_cleanup_test::Resource;
using asc_file_cleanup_test::TestContext;

template <typename Owner, std::size_t Rank>
struct Adapter {
  using Report = asc::SparseMatrixMarketReport;
  int format;
  static const asc::ArrayIoReport& Io(const Report& report) {
    return report.io;
  }
  static asc::ArrayIoSection MalformedCloseSection(
      asc::ArrayIoSection /*original*/) {
    return asc::ArrayIoSection::kTrailer;
  }
  asc::Status Save(const std::filesystem::path& path, const Owner& owner,
                   Fault* fault, const asc::ArrayIoLimits& limits,
                   Report& report) const {
    auto view = owner.view();
    ASC_CHECK(view.ok());
    std::array<std::byte, 1024> scratch{};
    constexpr auto kOverwrite = asc::ArrayFileOverwrite::kTruncate;
    if constexpr (Rank == 2) {
      if (format == 2) {
        if (fault == nullptr) {
          return asc::SaveSparseMatrixMarket(path, *view, kOverwrite, limits,
                                             {}, scratch, report);
        }
        return asc::internal_sparse_matrix_market::Save(
            path, *view, kOverwrite, limits, {}, scratch, report,
            Opener(*fault, true));
      }
    }
    if (fault == nullptr) {
      return format == 1
                 ? asc::SaveSparseArrayBinary(path, *view, kOverwrite, limits,
                                              scratch, report.io)
                 : asc::SaveSparseArrayText(path, *view, kOverwrite, limits,
                                            scratch, report.io);
    }
    return asc::internal_sparse_io::Save(path, *view, kOverwrite, limits,
                                         scratch, report.io, format == 1,
                                         Opener(*fault, true));
  }
  asc::Result<Owner> Load(const std::filesystem::path& path, Resource& resource,
                          Fault* fault, Report& report) const {
    std::array<std::byte, 1024> scratch{};
    std::array<asc::extent_t, Rank> metadata{};
    if constexpr (Rank == 2) {
      if (format == 2) {
        if (fault == nullptr) {
          return asc::LoadSparseMatrixMarket<Owner>(path, resource, scratch, {},
                                                    {}, report);
        }
        return asc::internal_sparse_matrix_market::Load<Owner>(
            path, resource, scratch, {}, {}, report, Opener(*fault, false));
      }
    }
    if (fault == nullptr) {
      return format == 1
                 ? asc::LoadSparseArrayBinary<Owner>(path, resource, metadata,
                                                     scratch, {}, report.io)
                 : asc::LoadSparseArrayText<Owner>(path, resource, metadata,
                                                   scratch, {}, report.io);
    }
    return asc::internal_sparse_io::Load<Owner>(
        path, resource, metadata, scratch, {}, report.io, format == 1,
        Opener(*fault, false));
  }
};

template <typename Owner, std::size_t Rank>
void Formats(TestContext& test, const std::filesystem::path& path,
             const Owner& owner, bool empty, const char* storage) {
  for (int format : {0, 1, 2}) {
    if (format == 2 && Rank != 2) {
      continue;
    }
    asc_file_cleanup_test::Profile(test, path, Adapter<Owner, Rank>{format},
                                   owner);
    std::printf("cleanup %s rank=%zu empty=%d scalar_bytes=%zu format=%d\n",
                storage, Rank, empty, sizeof(typename Owner::element_type),
                format);
  }
}

template <typename T, std::size_t... Index>
void Coordinate(TestContext& test, const std::filesystem::path& path,
                std::index_sequence<Index...> /*indices*/) {
  using Shape =
      asc::Extents<(static_cast<void>(Index), asc::kDynamicExtent)...>;
  constexpr std::size_t kRank = sizeof...(Index);
  for (bool empty : {false, true}) {
    std::array<asc::extent_t, kRank> dimensions{};
    dimensions.fill(2);
    if constexpr (kRank != 0) {
      if (empty) {
        dimensions[0] = 0;
      }
    }
    auto shape = Shape::Create(std::span<const asc::extent_t>(dimensions));
    ASC_CHECK(shape.ok());
    std::array<asc::index_t, kRank> coordinates{};
    const std::array<T, 1> values{T{-0.0}};
    Resource resource;
    {
      auto owner = asc::CoordinateArray<T, Shape>::Create(
          resource, *shape,
          std::span<const asc::index_t>(coordinates).first(empty ? 0 : kRank),
          std::span<const T>(values).first(empty ? 0 : 1));
      ASC_CHECK(owner.ok());
      Formats<asc::CoordinateArray<T, Shape>, kRank>(test, path, *owner, empty,
                                                     "coo");
    }
    test.Check(resource.live == 0, "COO fixture releases every resource");
  }
}

template <typename T, asc::SparseCompressedFormat Format>
void Compressed(TestContext& test, const std::filesystem::path& path) {
  using Owner = asc::CompressedSparseArray<T, Format>;
  for (bool empty : {false, true}) {
    const std::array<asc::extent_t, 2> shape{empty ? 0 : 2, empty ? 0 : 2};
    const std::array<asc::nnz_t, 3> offsets{0, 1, 1};
    const std::array<asc::index_t, 1> indices{0};
    const std::array<T, 1> values{T{-0.0}};
    Resource resource;
    {
      auto owner = Owner::Create(
          resource, shape,
          std::span<const asc::nnz_t>(offsets).first(empty ? 1 : 3),
          std::span<const asc::index_t>(indices).first(empty ? 0 : 1),
          std::span<const T>(values).first(empty ? 0 : 1));
      ASC_CHECK(owner.ok());
      Formats<Owner, 2>(
          test, path, *owner, empty,
          Format == asc::SparseCompressedFormat::kCsr ? "csr" : "csc");
    }
    test.Check(resource.live == 0,
               "compressed fixture releases every resource");
  }
}

template <typename T>
void AllKinds(TestContext& test, const std::filesystem::path& path) {
  Coordinate<T>(test, path, std::make_index_sequence<0>{});
  Coordinate<T>(test, path, std::make_index_sequence<1>{});
  Coordinate<T>(test, path, std::make_index_sequence<2>{});
  Coordinate<T>(test, path, std::make_index_sequence<3>{});
  Coordinate<T>(test, path, std::make_index_sequence<4>{});
  Compressed<T, asc::SparseCompressedFormat::kCsr>(test, path);
  Compressed<T, asc::SparseCompressedFormat::kCsc>(test, path);
}
}  // namespace

int main(int argc, char** argv) {
  ASC_CHECK(argc == 2);
  const std::filesystem::path scratch(argv[1]);
  ASC_CHECK(std::filesystem::is_directory(scratch));
  TestContext test;
  AllKinds<double>(test, scratch / "sparse-cleanup.asc");
  AllKinds<std::complex<double>>(test, scratch / "sparse-complex-cleanup.asc");
  return test.Finish();
}
