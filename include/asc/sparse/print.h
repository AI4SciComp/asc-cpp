#ifndef ASC_SPARSE_PRINT_H_
#define ASC_SPARSE_PRINT_H_

/**
 * @file
 * @brief Bounded host previews of finalized Sparse coordinates and values.
 *
 * ByteSink failures preserve ErrorCode/native_code, not owning diagnostic
 * strings; see the bounded stream policy in asc/core/array_format.h.
 * @ingroup asc_sparse
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/array_format.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_sparse_print {

template <typename View>
Status Validate(const View& view, std::span<std::byte> scratch) {
  if (view.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (!view.canonical_structure_trusted()) {
    return Status(ErrorCode::kInvalidState);
  }
  auto values = CheckedByteCount(view.nnz(), sizeof(typename View::value_type));
  if (!values.ok()) {
    return values.status();
  }
  if (internal_array_format::Overlaps(view.values(), *values, scratch.data(),
                                      scratch.size()) ||
      internal_array_format::Overlaps(&view, sizeof(view), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

template <typename View>
Status Header(const View& view, std::string_view kind,
              const ArrayPrintOptions& options, std::span<char> characters,
              internal_array_format::Output& out) {
  using internal_array_format::kTruncation;
  using internal_array_format::TextBuffer;
  const std::size_t count = static_cast<std::size_t>(view.nnz());
  if (options.show_metadata) {
    TextBuffer header(characters);
    header.Write(kind);
    header.Write(" scalar=");
    header.Write(
        internal_array_format::ScalarName<typename View::value_type>());
    header.Write(" shape=(");
    for (std::size_t dimension = 0; dimension < View::kRank; ++dimension) {
      if (dimension != 0) {
        header.Write(",");
      }
      header.Integer(view.extents()[dimension]);
    }
    header.Write(") stored=");
    header.Integer(view.nnz());
    header.Write("\n");
    if (!header.ok()) {
      return Status(ErrorCode::kAllocation);
    }
    if (!out.Fits(header.size(), count == 0 ? 3 : kTruncation.size())) {
      return Status(ErrorCode::kAllocation);
    }
    if (!out.Write(header.text())) {
      return out.status();
    }
  } else if (!out.Fits(count == 0 ? 3 : kTruncation.size())) {
    return Status(ErrorCode::kAllocation);
  }
  return Status::Ok();
}

template <typename View, typename Coordinate>
Status Print(const View& view, std::string_view kind, Coordinate coordinate,
             ByteSink& sink, const ArrayPrintOptions& options,
             std::span<std::byte> scratch, ArrayPrintReport& report) {
  using internal_array_format::kTruncation;
  using internal_array_format::Output;
  using internal_array_format::TextBuffer;
  const std::size_t count = static_cast<std::size_t>(view.nnz());
  Output out(&sink, options.max_output_bytes, report);
  const std::span<char> characters(reinterpret_cast<char*>(scratch.data()),
                                   scratch.size());
  Status header = Header(view, kind, options, characters, out);
  if (!header.ok()) {
    return header;
  }
  if (count == 0) {
    out.Write("[]\n");
    return out.status();
  }
  const std::size_t selected =
      internal_array_format::SelectedCount(count, options.max_elements);
  report.truncated = selected < count;
  std::size_t previous = 0;
  for (std::size_t ordinal = 0; ordinal < selected; ++ordinal) {
    const std::size_t position = internal_array_format::SelectedIndex(
        ordinal, count, options.max_elements, options.edge_preview);
    const auto indices = coordinate(position);
    TextBuffer line(characters);
    line.Write("(");
    for (std::size_t dimension = 0; dimension < View::kRank; ++dimension) {
      if (dimension != 0) {
        line.Write(",");
      }
      line.Integer(indices[dimension]);
    }
    line.Write(") = ");
    const std::size_t value_begin = line.size();
    line.Scalar(view.values()[position], options.float_format,
                options.precision);
    const std::size_t value_end = line.size();
    line.Write("\n");
    if (!line.ok()) {
      return Status(ErrorCode::kAllocation);
    }
    const bool gap = (ordinal == 0 ? position != 0 : position != previous + 1);
    const std::size_t reserve =
        (report.truncated || ordinal + 1 < selected) ? kTruncation.size() : 0;
    if (!out.Fits(line.size(), reserve + (gap ? 4 : 0))) {
      report.truncated = true;
      break;
    }
    if (gap && !out.Write("...\n")) {
      return out.status();
    }
    if (!out.Write(line.text().substr(0, value_begin))) {
      return out.status();
    }
    if (!out.Write(line.text().substr(value_begin, value_end - value_begin))) {
      return out.status();
    }
    ++report.values_displayed;
    if (!out.Write("\n")) {
      return out.status();
    }
    previous = position;
  }
  if (report.values_displayed < count) {
    report.truncated = true;
  }
  if (report.truncated) {
    out.Write(kTruncation);
  }
  return out.status();
}

}  // namespace internal_sparse_print

/**
 * @brief Prints canonical COO stored coordinates and values without densifying.
 * @tparam Element Exact supported integer, real or complex scalar; may be
 * const.
 * @tparam Rank Compile-time coordinate rank, including rank zero.
 * @param[in] view Borrowed finalized host view, alive and unmodified for the
 * call.
 * @param[in,out] sink Explicit synchronous sink; may retain a prefix on
 * failure.
 * @param[in] options Bounded preview policy; Sparse ignores row/column/slice
 * caps.
 * @param[in,out] scratch Caller bytes, disjoint from all values and
 * coordinates; must fit one complete metadata/coordinate/value line. No
 * allocation occurs.
 * @param[out] report Initialized before validation; records accepted bytes and
 * complete scalar tokens, including progress before a sink failure.
 * @return OK for a complete or visibly truncated preview; otherwise invalid
 * policy/alias, placement, capacity or sink status. Input storage is unchanged.
 * @par Complexity
 * O(rank metadata + displayed entries * rank), with no absent-entry reads.
 * Calls are reentrant with separate scratch/reports; serialize a shared sink.
 * Device/managed storage is rejected before value reads; no transfer occurs.
 * @ingroup asc_sparse
 */
template <typename Element, std::size_t Rank>
Status PrintArray(const CoordinateView<Element, Rank>& view, ByteSink& sink,
                  const ArrayPrintOptions& options,
                  std::span<std::byte> scratch, ArrayPrintReport& report) {
  report = {};
  if constexpr (!internal_array_format::kWireScalar<
                    std::remove_const_t<Element>>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    Status status = internal_array_format::ValidateOptions(options);
    if (!status.ok()) {
      return status;
    }
    status = internal_sparse_print::Validate(view, scratch);
    if (!status.ok()) {
      return status;
    }
    auto bytes = CheckedMultiply(static_cast<std::size_t>(view.nnz()), Rank);
    if (!bytes.ok()) {
      return bytes.status();
    }
    bytes = CheckedMultiply(*bytes, sizeof(index_t));
    if (!bytes.ok()) {
      return bytes.status();
    }
    if (internal_array_format::Overlaps(view.coordinates(), *bytes,
                                        scratch.data(), scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    auto coordinate = [&view](std::size_t position) {
      std::array<index_t, Rank> result{};
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        result[dimension] = view.coordinates()[position * Rank + dimension];
      }
      return result;
    };
    return internal_sparse_print::Print(view, "coo", coordinate, sink, options,
                                        scratch, report);
  }
}

/**
 * @brief Prints CSR/CSC stored entries using offset searches, without
 * conversion.
 * @tparam Element Exact supported integer, real or complex scalar; may be
 * const.
 * @tparam Format CSR or CSC; traversal follows its outer and inner dimensions.
 * @param[in] view Finalized host view; values and immutable structure stay
 * alive.
 * @param[in,out] sink Synchronous sink; failures may leave an accepted prefix.
 * @param[in] options Display limits; only element/byte caps apply to stored
 * entries.
 * @param[in,out] scratch Disjoint bounded bytes fitting metadata and one entry.
 * @param[out] report Initialized before checks; accepted byte/value progress.
 * @return OK including visible truncation, or placement/alias/capacity/I/O
 * status. No input mutation, allocation, densification, transfer or
 * synchronization occurs.
 * @par Complexity
 * O(displayed entries * log(outer extent + 1)); no linear empty-row scan.
 * Separate reports/scratch allow concurrent reads; sink concurrency is
 * caller-owned.
 * @ingroup asc_sparse
 */
template <typename Element, SparseCompressedFormat Format>
Status PrintArray(const CompressedSparseView<Element, Format>& view,
                  ByteSink& sink, const ArrayPrintOptions& options,
                  std::span<std::byte> scratch, ArrayPrintReport& report) {
  report = {};
  if constexpr (!internal_array_format::kWireScalar<
                    std::remove_const_t<Element>>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    Status status = internal_array_format::ValidateOptions(options);
    if (!status.ok()) {
      return status;
    }
    status = internal_sparse_print::Validate(view, scratch);
    if (!status.ok()) {
      return status;
    }
    constexpr std::size_t kOuterDimension =
        Format == SparseCompressedFormat::kCsr ? 0 : 1;
    auto offsets = CheckedAdd(view.extents()[kOuterDimension], extent_t{1});
    if (!offsets.ok()) {
      return offsets.status();
    }
    auto offset_bytes = CheckedByteCount(*offsets, sizeof(nnz_t));
    if (!offset_bytes.ok()) {
      return offset_bytes.status();
    }
    auto index_bytes = CheckedByteCount(view.nnz(), sizeof(index_t));
    if (!index_bytes.ok()) {
      return index_bytes.status();
    }
    if (internal_array_format::Overlaps(view.outer_offsets(), *offset_bytes,
                                        scratch.data(), scratch.size()) ||
        internal_array_format::Overlaps(view.inner_indices(), *index_bytes,
                                        scratch.data(), scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    auto coordinate = [&view, offsets](std::size_t position) {
      const auto* end = view.outer_offsets() + *offsets;
      const auto* found = std::upper_bound(view.outer_offsets(), end,
                                           static_cast<nnz_t>(position));
      std::array<index_t, 2> result{};
      result[kOuterDimension] = found - view.outer_offsets() - 1;
      result[1 - kOuterDimension] = view.inner_indices()[position];
      return result;
    };
    return internal_sparse_print::Print(
        view, Format == SparseCompressedFormat::kCsr ? "csr" : "csc",
        coordinate, sink, options, scratch, report);
  }
}

/**
 * @brief Borrows a COO owner's const view for a bounded host value preview.
 * @tparam Element Supported native wire scalar.
 * @tparam ExtentsType Owner's static/dynamic shape type.
 * @param[in] array Owner retained unmodified until the synchronous return.
 * @param[in,out] sink Explicit sink; partial output can remain on failure.
 * @param[in] options Bounded display policy.
 * @param[in,out] scratch Disjoint caller buffer; no allocation occurs.
 * @param[out] report Actual output progress, initialized before validation.
 * @return Same placement, capacity, alias and I/O statuses as the COO-view
 * overload. Ownership, complexity and concurrency follow that overload; no view
 * escapes.
 * @ingroup asc_sparse
 */
template <SparseElement Element, typename ExtentsType>
Status PrintArray(const CoordinateArray<Element, ExtentsType>& array,
                  ByteSink& sink, const ArrayPrintOptions& options,
                  std::span<std::byte> scratch, ArrayPrintReport& report) {
  report = {};
  if (internal_array_format::Overlaps(&array, sizeof(array), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = array.view();
  if (!view.ok()) {
    return view.status();
  }
  return PrintArray(*view, sink, options, scratch, report);
}

/**
 * @brief Borrows a CSR/CSC owner's const view for a bounded host preview.
 * @tparam Element Supported native wire scalar.
 * @tparam Format CSR or CSC owner kind.
 * @param[in] array Owner retained unmodified throughout the synchronous call.
 * @param[in,out] sink Explicit synchronous sink; no rollback after partial
 * output.
 * @param[in] options Bounded display policy.
 * @param[in,out] scratch Disjoint caller bytes fitting each output line.
 * @param[out] report Initialized before validation; actual output progress.
 * @return View creation or printer status; inputs remain unchanged, without
 * allocation, densification or transfer. Complexity/concurrency follow the view
 * overload and no borrowed view escapes this call.
 * @ingroup asc_sparse
 */
template <SparseElement Element, SparseCompressedFormat Format>
Status PrintArray(const CompressedSparseArray<Element, Format>& array,
                  ByteSink& sink, const ArrayPrintOptions& options,
                  std::span<std::byte> scratch, ArrayPrintReport& report) {
  report = {};
  if (internal_array_format::Overlaps(&array, sizeof(array), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = array.view();
  if (!view.ok()) {
    return view.status();
  }
  return PrintArray(*view, sink, options, scratch, report);
}

}  // namespace asc

#endif  // ASC_SPARSE_PRINT_H_
