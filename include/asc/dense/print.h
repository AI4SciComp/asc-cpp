#ifndef ASC_DENSE_PRINT_H_
#define ASC_DENSE_PRINT_H_

/**
 * @file
 * @brief Bounded logical-value previews of host Dense owners and views.
 *
 * ByteSink failures preserve ErrorCode/native_code, not owning diagnostic
 * strings; see the bounded stream policy in asc/core/array_format.h.
 * @ingroup asc_dense
 */

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/array_format.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/view.h"

namespace asc {
namespace internal_dense_print {

inline std::string_view ValuePrefix(bool gap, bool emitted) {
  if (gap) {
    return emitted ? ", ..., " : "..., ";
  }
  return emitted ? ", " : "";
}

template <typename Element, std::size_t Rank>
class Printer {
 public:
  Printer(const DenseView<Element, Rank>& view, ByteSink& sink,
          const ArrayPrintOptions& options, std::span<char> scratch,
          ArrayPrintReport& report)
      : view_(view),
        options_(options),
        scratch_(scratch),
        report_(report),
        out_(&sink, options.max_output_bytes, report) {}

  Status Run() {
    const std::size_t minimum_body = MinimumBody();
    if (options_.show_metadata) {
      internal_array_format::TextBuffer header(scratch_);
      header.Write("dense scalar=");
      header.Write(
          internal_array_format::ScalarName<std::remove_const_t<Element>>());
      header.Write(" shape=");
      Shape(header);
      header.Write(" count=");
      header.Integer(view_.logical_size());
      header.Write("\n");
      if (!header.ok()) {
        return Status(ErrorCode::kAllocation);
      }
      if (!out_.Fits(header.size(), minimum_body)) {
        return Status(ErrorCode::kAllocation);
      }
      if (!out_.Write(header.text())) {
        return out_.status();
      }
    } else if (!out_.Fits(minimum_body)) {
      return Status(ErrorCode::kAllocation);
    }
    if (view_.logical_size() == 0) {
      internal_array_format::TextBuffer empty(scratch_);
      empty.Write("empty shape=");
      Shape(empty);
      empty.Write("\n");
      if (!empty.ok()) {
        return Status(ErrorCode::kAllocation);
      }
      out_.Write(empty.text());
      return out_.status();
    }
    if constexpr (Rank == 0) {
      if (options_.max_elements == 0) {
        report_.truncated = true;
      } else {
        EmitValue({}, 1);
        if (!out_.status().ok()) {
          return out_.status();
        }
        if (!report_.truncated) {
          out_.Write("\n");
        }
      }
    } else if constexpr (Rank == 1) {
      Vector();
    } else if constexpr (Rank == 2) {
      Matrix();
    } else {
      Slices();
    }
    if (report_.values_displayed <
        static_cast<std::size_t>(view_.logical_size())) {
      report_.truncated = true;
    }
    if (report_.truncated && out_.status().ok()) {
      out_.Write(internal_array_format::kTruncation);
    }
    return out_.status();
  }

 private:
  void Shape(internal_array_format::TextBuffer& text) const {
    text.Write("(");
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (dimension != 0) {
        text.Write(",");
      }
      text.Integer(view_.extents()[dimension]);
    }
    text.Write(")");
  }

  std::size_t MinimumBody() {
    if (view_.logical_size() == 0) {
      internal_array_format::TextBuffer empty(scratch_);
      empty.Write("empty shape=");
      Shape(empty);
      empty.Write("\n");
      return empty.ok() ? empty.size()
                        : std::numeric_limits<std::size_t>::max();
    }
    return internal_array_format::kTruncation.size() + (Rank == 0 ? 0 : 8);
  }

  bool EmitValue(std::string_view prefix, std::size_t closing) {
    if (report_.values_displayed == options_.max_elements) {
      report_.truncated = true;
      return false;
    }
    auto value = view_.At(std::span<const index_t, Rank>(indices_));
    if (!value.ok()) {
      out_.Fail(value.status().code());
      return false;
    }
    auto encoded = internal_array_format::FormatScalar(
        **value, scratch_, options_.float_format, options_.precision);
    if (!encoded.ok()) {
      out_.Fail(encoded.status().code());
      return false;
    }
    const bool last_value = report_.values_displayed + 1 ==
                            static_cast<std::size_t>(view_.logical_size());
    const std::size_t reserve =
        closing + (last_value && !report_.truncated
                       ? 0
                       : internal_array_format::kTruncation.size());
    if (!out_.Fits(*encoded, reserve + prefix.size())) {
      report_.truncated = true;
      byte_stop_ = true;
      return false;
    }
    if (!out_.Write(prefix) ||
        !out_.Write(std::string_view(scratch_.data(), *encoded))) {
      return false;
    }
    ++report_.values_displayed;
    return true;
  }

  bool Ellipsis(std::string_view prefix, std::size_t closing) {
    report_.truncated = true;
    if (!out_.Fits(prefix.size() + 3,
                   closing + internal_array_format::kTruncation.size())) {
      return false;
    }
    return out_.Write(prefix) && out_.Write("...");
  }

  void Vector()
    requires(Rank == 1)
  {
    out_.Write("[");
    const std::size_t count = static_cast<std::size_t>(view_.extents()[0]);
    const std::size_t selected =
        internal_array_format::SelectedCount(count, options_.max_elements);
    report_.truncated = selected < count;
    std::size_t previous = 0;
    bool emitted = false;
    bool omitted = false;
    for (std::size_t ordinal = 0; ordinal < selected && out_.status().ok();
         ++ordinal) {
      const std::size_t position = internal_array_format::SelectedIndex(
          ordinal, count, options_.max_elements, options_.edge_preview);
      const bool gap = ordinal == 0 ? position != 0 : position != previous + 1;
      indices_[0] = static_cast<index_t>(position);
      const std::string_view prefix = ValuePrefix(gap, emitted);
      if (!EmitValue(prefix, 2)) {
        omitted = true;
        break;
      }
      emitted = true;
      previous = position;
    }
    if (selected == 0 || omitted || (emitted && previous + 1 < count)) {
      Ellipsis(emitted ? ", " : "", 2);
    }
    out_.Write("]\n");
  }

  void Matrix()
    requires(Rank >= 2)
  {
    out_.Write("[");
    const std::size_t rows = static_cast<std::size_t>(view_.extents()[0]);
    const std::size_t columns = static_cast<std::size_t>(view_.extents()[1]);
    const std::size_t selected_rows =
        internal_array_format::SelectedCount(rows, options_.max_rows);
    const std::size_t selected_columns =
        internal_array_format::SelectedCount(columns, options_.max_columns);
    report_.truncated =
        report_.truncated || selected_rows < rows || selected_columns < columns;
    std::size_t previous_row = 0;
    bool emitted_row = false;
    bool stopped = false;
    for (std::size_t row_ordinal = 0;
         row_ordinal < selected_rows && out_.status().ok(); ++row_ordinal) {
      const std::size_t row = internal_array_format::SelectedIndex(
          row_ordinal, rows, options_.max_rows, options_.edge_preview);
      const bool row_gap =
          row_ordinal == 0 ? row != 0 : row != previous_row + 1;
      std::string_view prefix = emitted_row ? ",\n [" : "[";
      if (row_gap) {
        prefix = emitted_row ? ",\n ...,\n [" : "...,\n [";
      }
      if (!out_.Fits(prefix.size() + 4,
                     2 + internal_array_format::kTruncation.size())) {
        byte_stop_ = true;
        stopped = true;
        break;
      }
      out_.Write(prefix);
      indices_[0] = static_cast<index_t>(row);
      std::size_t previous_column = 0;
      bool emitted_column = false;
      bool column_stopped = false;
      for (std::size_t ordinal = 0;
           ordinal < selected_columns && out_.status().ok(); ++ordinal) {
        const std::size_t column = internal_array_format::SelectedIndex(
            ordinal, columns, options_.max_columns, options_.edge_preview);
        const bool gap =
            ordinal == 0 ? column != 0 : column != previous_column + 1;
        indices_[1] = static_cast<index_t>(column);
        const std::string_view separator = ValuePrefix(gap, emitted_column);
        if (!EmitValue(separator, 3)) {
          column_stopped = true;
          break;
        }
        emitted_column = true;
        previous_column = column;
      }
      if (selected_columns == 0 || column_stopped ||
          (emitted_column && previous_column + 1 < columns)) {
        Ellipsis(emitted_column ? ", " : "", 3);
      }
      out_.Write("]");
      emitted_row = true;
      previous_row = row;
      if (byte_stop_ || report_.values_displayed == options_.max_elements) {
        stopped = row_ordinal + 1 < selected_rows;
        break;
      }
    }
    if (selected_rows == 0 || stopped ||
        (emitted_row && previous_row + 1 < rows)) {
      Ellipsis(emitted_row ? ",\n " : "", 2);
    }
    out_.Write("]\n");
  }

  void Slices()
    requires(Rank >= 3)
  {
    std::size_t slices = 1;
    for (std::size_t dimension = 2; dimension < Rank; ++dimension) {
      slices *= static_cast<std::size_t>(view_.extents()[dimension]);
    }
    const std::size_t selected =
        internal_array_format::SelectedCount(slices, options_.max_slices);
    report_.truncated = selected < slices;
    std::size_t previous = 0;
    for (std::size_t ordinal = 0; ordinal < selected && out_.status().ok();
         ++ordinal) {
      const std::size_t position = internal_array_format::SelectedIndex(
          ordinal, slices, options_.max_slices, options_.edge_preview);
      std::size_t remainder = position;
      internal_array_format::TextBuffer label(scratch_);
      label.Write("slice axes=(0,1) fixed=(");
      for (std::size_t dimension = 2; dimension < Rank; ++dimension) {
        indices_[dimension] = static_cast<index_t>(
            remainder % static_cast<std::size_t>(view_.extents()[dimension]));
        remainder /= static_cast<std::size_t>(view_.extents()[dimension]);
        if (dimension != 2) {
          label.Write(",");
        }
        label.Integer(dimension);
        label.Write(":");
        label.Integer(indices_[dimension]);
      }
      label.Write(")\n");
      if (!label.ok()) {
        out_.Fail(ErrorCode::kAllocation);
        return;
      }
      const bool gap = ordinal == 0 ? position != 0 : position != previous + 1;
      if (!out_.Fits(
              label.size(),
              8 + (gap ? 4 : 0) + internal_array_format::kTruncation.size())) {
        report_.truncated = true;
        break;
      }
      if (gap) {
        out_.Write("...\n");
      }
      out_.Write(label.text());
      Matrix();
      if (byte_stop_ || report_.values_displayed == options_.max_elements) {
        break;
      }
      previous = position;
    }
  }

  const DenseView<Element, Rank>& view_;
  const ArrayPrintOptions& options_;
  std::span<char> scratch_;
  ArrayPrintReport& report_;
  internal_array_format::Output out_;
  std::array<index_t, Rank> indices_{};
  bool byte_stop_ = false;
};

}  // namespace internal_dense_print

/**
 * @brief Prints actual Dense values in conventional rows with bounded previews.
 * @tparam Element Supported integer, real or complex scalar, optionally const.
 * @tparam Rank Compile-time rank; scalar rank zero and empty shapes are
 * supported.
 * @param[in] view Valid borrowed host/pinned-host view, retained unmodified for
 * the call. Logical indexing honors layout/strides and never reads padding.
 * @param[in,out] sink Explicit synchronous output; a failure may leave a
 * prefix.
 * @param[in] options Bounded element/axis/slice/byte limits and scalar
 * spelling.
 * @param[in,out] scratch Disjoint caller bytes, sufficient for one scalar token
 * or metadata/slice label. No allocation, copying to host or evaluation occurs.
 * @param[out] report Reset before validation; actual accepted bytes, complete
 * scalar tokens, and whether values were omitted, including failure progress.
 * @return OK for complete or visibly truncated output; otherwise unsupported
 * scalar/placement, invalid options/alias, scratch/byte limit, or sink status.
 * @par Complexity
 * Work scales with rank metadata and selected values, independent of omitted
 * values. Width alignment is not performed. Concurrent calls need independent
 * scratch/reports and a sink whose writes the caller serializes as required.
 * Device/managed placement fails before any value access; inputs never mutate.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status PrintArray(const DenseView<Element, Rank>& view, ByteSink& sink,
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
    if (view.memory_space() != MemorySpace::kHost &&
        view.memory_space() != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    auto bytes =
        CheckedMultiply(view.mapping().required_span_size(),
                        sizeof(typename DenseView<Element, Rank>::value_type));
    if (!bytes.ok()) {
      return bytes.status();
    }
    if (internal_array_format::Overlaps(view.data(), *bytes, scratch.data(),
                                        scratch.size()) ||
        internal_array_format::Overlaps(&view, sizeof(view), scratch.data(),
                                        scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const std::span<char> characters(reinterpret_cast<char*>(scratch.data()),
                                     scratch.size());
    internal_dense_print::Printer printer(view, sink, options, characters,
                                          report);
    return printer.Run();
  }
}

/**
 * @brief Borrows a Dense owner's const view for a synchronous value preview.
 * @tparam Element Supported native wire scalar stored by the owner.
 * @tparam ExtentsType Owner's static/dynamic compile-time-rank extents.
 * @param[in] array Owner kept alive and unmodified throughout the call.
 * @param[in,out] sink Explicit synchronous sink; a prefix can remain on
 * failure.
 * @param[in] options Bounded display policy.
 * @param[in,out] scratch Caller bytes disjoint from the owner; no allocation.
 * @param[out] report Reset before view validation; actual output progress.
 * @return Owner view status or the Dense-view printer status. Placement,
 * complexity, lifetime, failure and concurrency rules match the view overload;
 * no view escapes and no transfer/synchronization or lazy evaluation occurs.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType>
Status PrintArray(const DenseArray<Element, ExtentsType>& array, ByteSink& sink,
                  const ArrayPrintOptions& options,
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

#endif  // ASC_DENSE_PRINT_H_
