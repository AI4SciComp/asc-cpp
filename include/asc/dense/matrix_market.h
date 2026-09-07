#ifndef ASC_DENSE_MATRIX_MARKET_H_
#define ASC_DENSE_MATRIX_MARKET_H_

/** @file
 * @brief Dense-owned, bounded Matrix Market array interchange.
 *
 * Only matrix array input is accepted; coordinate conversion is an optional
 * separate operation and is not silently performed. Wire order is column-first
 * regardless of memory layout. CPU-only codecs allocate no hidden storage or
 * transfer data. Borrowed stream error strings follow asc/core/array_io.h;
 * resource-owner and File error boundaries remain separate.
 * @ingroup asc_dense
 */

#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/io.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/export.h"
#include "asc/dense/io.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"

namespace asc {
namespace internal_dense_matrix_market {
class ReaderAccess;
ASC_DENSE_EXPORT Result<std::size_t> ValidateHeader(
    std::span<const extent_t, 2> shape, MatrixMarketField field,
    MatrixMarketSymmetry symmetry, const ArrayIoLimits& limits);
ASC_DENSE_EXPORT Status WriteHeader(std::span<const extent_t, 2> shape,
                                    MatrixMarketField field,
                                    MatrixMarketSymmetry symmetry,
                                    const ArrayIoLimits& limits,
                                    std::span<std::byte> scratch,
                                    internal_array_io::Output& output);
}  // namespace internal_dense_matrix_market

/** @brief Move-only prepared Matrix Market array cursor with copied rank-two
 * metadata.
 *
 * The source, report and caller scratch must outlive the cursor and stay
 * exclusively available. Limits and two extents are copied; no metadata or
 * payload allocation occurs during preparation. Coordinate input is rejected
 * without conversion. An attempted payload, successful or not, consumes the
 * cursor; argument/allocation rejection before payload leaves it prepared.
 * Failed payloads cannot resynchronize or roll back source progress. Every
 * payload includes trailing comment/whitespace and EOF validation.
 * @ingroup asc_dense
 */
class ASC_DENSE_EXPORT DenseMatrixMarketReader {
 public:
  /** @brief Reads and validates one array banner and dimension record.
   * @param[in,out] source Explicit synchronous source, borrowed until
   * consumption.
   * @param[in,out] scratch Caller token bytes, disjoint from source/report
   * objects.
   * @param[in] limits Copied byte, shape, stored-count and resource budgets.
   * @param[out] report Reset before parsing; retains actual source progress.
   * @return Prepared cursor or bounded format/shape/resource/I/O failure.
   * No value allocation, read-ahead beyond the dimension record, transfer or
   * provider selection occurs. Work is linear in bounded header bytes.
   * @ingroup asc_dense
   */
  static Result<DenseMatrixMarketReader> Prepare(ByteSource& source,
                                                 std::span<std::byte> scratch,
                                                 const ArrayIoLimits& limits,
                                                 ArrayIoReport& report);

  /** @brief A source cursor cannot be copied. @ingroup asc_dense */
  DenseMatrixMarketReader(const DenseMatrixMarketReader&) = delete;
  /** @brief Copy assignment cannot duplicate source progress.
   * @ingroup asc_dense
   */
  DenseMatrixMarketReader& operator=(const DenseMatrixMarketReader&) = delete;
  /** @brief Transfers the cursor, invalidating the source cursor.
   * @param[in,out] other Cursor whose borrowed lifetimes remain unchanged.
   * @ingroup asc_dense
   */
  DenseMatrixMarketReader(DenseMatrixMarketReader&& other) noexcept;
  /** @brief Assignment cannot discard an unfinished source cursor.
   * @ingroup asc_dense
   */
  DenseMatrixMarketReader& operator=(DenseMatrixMarketReader&&) = delete;
  /** @brief Releases no caller storage and performs no reads.
   * @ingroup asc_dense
   */
  ~DenseMatrixMarketReader() = default;

  /** @brief Returns the validated field, which does not encode scalar
   * precision.
   * @return Integer, real or complex field; array pattern is invalid.
   * @ingroup asc_dense
   */
  [[nodiscard]] MatrixMarketField field() const noexcept { return field_; }
  /** @brief Returns the validated lower-half interpretation.
   * @return General, symmetric, skew-symmetric or complex Hermitian semantics.
   * @ingroup asc_dense
   */
  [[nodiscard]] MatrixMarketSymmetry symmetry() const noexcept {
    return symmetry_;
  }
  /** @brief Borrows the cursor's copied row/column extents.
   * @return Two extents, valid until this cursor is moved or destroyed.
   * @ingroup asc_dense
   */
  [[nodiscard]] std::span<const extent_t, 2> shape() const noexcept {
    return shape_;
  }
  /** @brief Returns the full expanded Dense element count.
   * @return Checked rows times columns, including zero-sized matrices.
   * @ingroup asc_dense
   */
  [[nodiscard]] std::size_t count() const noexcept { return count_; }
  /** @brief Returns the exact number of scalar records on wire.
   * @return Full, lower-triangular or strict-lower count as declared by
   * symmetry.
   * @ingroup asc_dense
   */
  [[nodiscard]] std::size_t stored_count() const noexcept {
    return stored_count_;
  }
  /** @brief Reports whether a payload attempt remains available.
   * @return False after a payload attempt or move.
   * @ingroup asc_dense
   */
  [[nodiscard]] bool ready() const noexcept { return ready_; }

 private:
  /// @cond ASC_INTERNAL
  friend class internal_dense_matrix_market::ReaderAccess;
  /// @endcond
  DenseMatrixMarketReader(ByteSource& source, std::span<std::byte> scratch,
                          ArrayIoLimits limits, ArrayIoReport& report)
      : input_(source, limits, report), scratch_(scratch) {}
  Status ParseHeader();

  template <typename T, typename Store>
  Status Payload(Store store) {
    ready_ = false;
    input_.report().section = ArrayIoSection::kPayload;
    const auto rows = static_cast<std::size_t>(shape_[0]);
    const auto columns = static_cast<std::size_t>(shape_[1]);
    if (symmetry_ == MatrixMarketSymmetry::kSkewSymmetric) {
      for (std::size_t diagonal = 0; diagonal < rows; ++diagonal) {
        store(diagonal, diagonal, T{0});
      }
    }
    for (std::size_t column = 0; rows != 0 && column < columns; ++column) {
      const std::size_t first =
          symmetry_ == MatrixMarketSymmetry::kGeneral
              ? 0
              : column + (symmetry_ == MatrixMarketSymmetry::kSkewSymmetric);
      for (std::size_t row = first; row < rows; ++row) {
        auto record = input_.NextRecord(scratch_);
        if (!record.ok()) {
          return record.status();
        }
        if (record->end_of_file) {
          return input_.Fail(ErrorCode::kEncoding);
        }
        auto value = internal_matrix_market::ParseValue<T>(
            field_, record->tokens(), MatrixMarketPatternPolicy::kUnspecified,
            input_.limits().max_token_bytes);
        if (!value.ok()) {
          return input_.Fail(value.status().code());
        }
        if constexpr (internal_array_format::kComplex<T>) {
          if (symmetry_ == MatrixMarketSymmetry::kHermitian && row == column &&
              value->imag() != 0) {
            return input_.Fail(ErrorCode::kEncoding);
          }
        }
        store(row, column, *value);
        if (row != column && symmetry_ != MatrixMarketSymmetry::kGeneral) {
          T mirror = *value;
          if (symmetry_ == MatrixMarketSymmetry::kSkewSymmetric) {
            auto negated = internal_matrix_market::CheckedNegate(*value);
            if (!negated.ok()) {
              return input_.Fail(negated.status().code());
            }
            mirror = *negated;
          } else if (symmetry_ == MatrixMarketSymmetry::kHermitian) {
            mirror = internal_matrix_market::Conjugate(*value);
          }
          const std::size_t reflected_row = column;
          const std::size_t reflected_column = row;
          store(reflected_row, reflected_column, mirror);
        }
        ++input_.report().values_processed;
      }
    }
    input_.report().section = ArrayIoSection::kTrailer;
    auto status = input_.Finish(scratch_);
    if (status.ok()) {
      input_.report().section = ArrayIoSection::kComplete;
    }
    return status;
  }

  internal_matrix_market::Input input_;
  std::span<std::byte> scratch_;
  std::array<extent_t, 2> shape_{};
  MatrixMarketField field_ = MatrixMarketField::kReal;
  MatrixMarketSymmetry symmetry_ = MatrixMarketSymmetry::kGeneral;
  std::size_t count_ = 0;
  std::size_t stored_count_ = 0;
  bool ready_ = false;
};

namespace internal_dense_matrix_market {

class ReaderAccess {
 public:
  static const ArrayIoLimits& Limits(const DenseMatrixMarketReader& reader) {
    return reader.input_.limits();
  }
  static std::span<std::byte> Scratch(const DenseMatrixMarketReader& reader) {
    return reader.scratch_;
  }
  static bool Aliases(DenseMatrixMarketReader& reader, const void* data,
                      std::size_t bytes) {
    return internal_array_format::Overlaps(data, bytes, &reader,
                                           sizeof(reader)) ||
           internal_array_format::Overlaps(data, bytes, &reader.input_.report(),
                                           sizeof(ArrayIoReport)) ||
           internal_array_format::Overlaps(data, bytes, reader.scratch_.data(),
                                           reader.scratch_.size());
  }
  static void Commit(DenseMatrixMarketReader& reader) {
    reader.input_.report().committed = true;
  }
  template <typename T, typename Store>
  static Status Payload(DenseMatrixMarketReader& reader, Store store) {
    return reader.Payload<T>(store);
  }
};

template <typename T>
Status ValidateRead(const DenseMatrixMarketReader& reader) {
  if (!reader.ready()) {
    return Status(ErrorCode::kInvalidState);
  }
  return internal_matrix_market::ValidateField<T>(
      reader.field(), MatrixMarketPatternPolicy::kUnspecified);
}

template <typename T, std::size_t Rank>
std::size_t Offset(const DenseView<T, Rank>& view, std::size_t row,
                   std::size_t column) {
  return row * static_cast<std::size_t>(view.strides()[0]) +
         column * static_cast<std::size_t>(view.strides()[1]);
}

template <typename T>
Result<std::size_t> ValueBytes(T value, std::span<std::byte> scratch,
                               const ArrayIoLimits& limits) {
  if constexpr (internal_array_format::kComplex<T>) {
    auto real = ValueBytes(value.real(), scratch, limits);
    auto imag = ValueBytes(value.imag(), scratch, limits);
    if (!real.ok()) {
      return real.status();
    }
    if (!imag.ok()) {
      return imag.status();
    }
    auto sum = internal_array_io::AddSize(*real, *imag);
    if (!sum.ok()) {
      return sum.status();
    }
    return internal_array_io::AddSize(*sum, std::size_t{1});
  } else {
    auto size = internal_array_format::FormatScalar(
        value,
        std::span(reinterpret_cast<char*>(scratch.data()), scratch.size()),
        ArrayFloatFormat::kGeneral, std::numeric_limits<T>::max_digits10);
    if (size.ok() && *size > limits.max_token_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    return size;
  }
}

template <typename T, std::size_t Rank>
Status ValidateWriterAliases(const DenseView<T, Rank>& view,
                             std::span<std::byte> scratch,
                             ArrayIoReport& report) {
  auto bytes = internal_array_io::MultiplySize(
      view.mapping().required_span_size(), sizeof(T));
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (internal_array_format::Overlaps(&report, sizeof(report), scratch.data(),
                                      scratch.size()) ||
      internal_array_format::Overlaps(&report, sizeof(report), view.data(),
                                      *bytes) ||
      internal_array_format::Overlaps(&report, sizeof(report), &view,
                                      sizeof(view))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

template <typename T, std::size_t Rank>
Status ValidateWrite(const DenseView<T, Rank>& view,
                     MatrixMarketSymmetry symmetry, const ArrayIoLimits& limits,
                     std::span<std::byte> scratch) {
  using Value = std::remove_const_t<T>;
  if constexpr (Rank != 2) {
    return Status(ErrorCode::kShape);
  } else if constexpr (!internal_array_format::kWireScalar<Value>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    auto status = internal_array_io::ValidateLimits(limits);
    if (status.ok()) {
      status = internal_dense_io::ValidateView(view, scratch, limits);
    }
    if (!status.ok()) {
      return status;
    }
    auto count = ValidateHeader(view.extents(),
                                internal_matrix_market::ScalarField<Value>(),
                                symmetry, limits);
    if (!count.ok()) {
      return count.status();
    }
    const auto rows = static_cast<std::size_t>(view.extents()[0]);
    const auto columns = static_cast<std::size_t>(view.extents()[1]);
    for (std::size_t column = 0; rows != 0 && column < columns; ++column) {
      for (std::size_t row = 0; row < rows; ++row) {
        const Value value = view.data()[Offset(view, row, column)];
        if (!internal_matrix_market::Finite(value)) {
          return Status(ErrorCode::kOverflow);
        }
        if (symmetry == MatrixMarketSymmetry::kSkewSymmetric && row == column) {
          if (!internal_matrix_market::IsZero(value)) {
            return Status(ErrorCode::kEncoding);
          }
        } else if (symmetry != MatrixMarketSymmetry::kGeneral) {
          Value expected = value;
          if (symmetry == MatrixMarketSymmetry::kHermitian) {
            expected = internal_matrix_market::Conjugate(value);
          } else if (symmetry == MatrixMarketSymmetry::kSkewSymmetric) {
            auto negated = internal_matrix_market::CheckedNegate(value);
            if (!negated.ok()) {
              return negated.status();
            }
            expected = *negated;
          }
          const std::size_t reflected_row = column;
          const std::size_t reflected_column = row;
          if (view.data()[Offset(view, reflected_row, reflected_column)] !=
              expected) {
            return Status(ErrorCode::kEncoding);
          }
        }
      }
    }
    return Status::Ok();
  }
}

}  // namespace internal_dense_matrix_market

/** @brief Loads a prepared Matrix Market array into a new explicit-layout
 * owner.
 * @tparam Element Caller-selected supported integer/IEEE real/complex scalar.
 * @tparam ExtentsType Rank-two static/dynamic shape, checked before allocation.
 * @tparam Layout Explicit LayoutLeft or LayoutRight physical arrangement.
 * @param[in,out] reader Prepared one-use source cursor with retained
 * scratch/report.
 * @param[in] resource Host resource outliving the result; one budgeted request.
 * @param[in] layout Destination layout, independent of column-oriented wire
 * order.
 * @return Complete owner after conversion, mirroring and EOF, or failure
 * releasing candidate storage. Integer conversion must be exact; imaginary
 * parts are never dropped. No hidden allocation/transfer occurs. Work is
 * O(input bytes + rows*cols). Preflight/allocation errors do not consume a
 * prepared payload; malformed payloads invalidate it. Caller serializes
 * source/resource access and retains lifetimes.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> ReadDenseMatrixMarket(
    DenseMatrixMarketReader& reader, MemoryResource& resource, Layout layout) {
  using Access = internal_dense_matrix_market::ReaderAccess;
  if constexpr (ExtentsType::kRank != 2) {
    return Status(ErrorCode::kShape);
  } else if constexpr (!internal_array_format::kWireScalar<Element>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    auto status = internal_dense_matrix_market::ValidateRead<Element>(reader);
    if (!status.ok()) {
      return status;
    }
    auto extents = internal_dense_io::MatchExtents<ExtentsType>(
        reader.shape(), std::make_index_sequence<2>{});
    if (!extents.ok()) {
      return extents.status();
    }
    if (resource.space() != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    auto bytes =
        internal_array_io::MultiplySize(reader.count(), sizeof(Element));
    if (!bytes.ok()) {
      return bytes.status();
    }
    const auto& limits = Access::Limits(reader);
    if (*bytes > limits.max_decoded_bytes ||
        *bytes > limits.max_staging_bytes ||
        *bytes > limits.max_allocation_bytes || limits.max_allocations == 0) {
      return Status(ErrorCode::kAllocation);
    }
    status = internal_dense_io::ValidateOwnerLayout(reader.shape(), layout);
    if (!status.ok()) {
      return status;
    }
    auto candidate =
        DenseArray<Element, ExtentsType>::Create(resource, *extents, layout);
    if (!candidate.ok()) {
      return internal_core_result::StatusAccess::TakeFailure(
          std::move(candidate));
    }
    auto view = candidate->view();
    if (!view.ok()) {
      return internal_core_result::StatusAccess::TakeFailure(std::move(view));
    }
    if (Access::Aliases(reader, view->data(), *bytes)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    status = Access::Payload<Element>(reader, [&view](std::size_t row,
                                                      std::size_t column,
                                                      Element value) {
      view->data()[internal_dense_matrix_market::Offset(*view, row, column)] =
          value;
    });
    if (!status.ok()) {
      return status;
    }
    Access::Commit(reader);
    return std::move(*candidate);
  }
}

/** @brief Transactionally loads a prepared array into existing Dense storage.
 * @tparam Element Supported writable destination scalar, explicitly chosen.
 * @tparam Rank Destination rank; only two is valid.
 * @tparam StagingExtent Caller span capacity, static or dynamic.
 * @param[in,out] reader Prepared source/scratch/report, retained through EOF.
 * @param[in,out] destination Valid unique host/pinned mapping; padding is
 * untouched.
 * @param[in,out] staging Live typed capacity at least rows*cols; disjoint from
 * destination backing/descriptor, cursor, report and parser scratch. Staging
 * may change on failure. Complete supplied capacity counts against staging
 * limits.
 * @return Status; every destination byte is unchanged on failure. Success uses
 * prevalidated nonfailing scatter after all parse/conversion/symmetry/EOF
 * checks. O(input bytes + rows*cols), no allocation, transfer or source
 * rollback. Argument rejection leaves a prepared cursor reusable; a payload
 * attempt consumes it. All borrowed objects require exclusive access and
 * caller-managed lifetimes.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank, std::size_t StagingExtent>
  requires(!std::is_const_v<Element>)
Status ReadDenseMatrixMarketInto(DenseMatrixMarketReader& reader,
                                 const DenseView<Element, Rank>& destination,
                                 std::span<Element, StagingExtent> staging) {
  using Access = internal_dense_matrix_market::ReaderAccess;
  if constexpr (Rank != 2) {
    return Status(ErrorCode::kShape);
  } else if constexpr (!internal_array_format::kWireScalar<Element>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    auto status = internal_dense_matrix_market::ValidateRead<Element>(reader);
    if (!status.ok()) {
      return status;
    }
    if (destination.extents()[0] != reader.shape()[0] ||
        destination.extents()[1] != reader.shape()[1]) {
      return Status(ErrorCode::kShape);
    }
    const auto& limits = Access::Limits(reader);
    status = internal_dense_io::ValidateView(destination,
                                             Access::Scratch(reader), limits);
    if (!status.ok()) {
      return status;
    }
    auto staging_bytes =
        internal_array_io::MultiplySize(staging.size(), sizeof(Element));
    auto target_bytes = internal_array_io::MultiplySize(
        destination.mapping().required_span_size(), sizeof(Element));
    if (!staging_bytes.ok() || !target_bytes.ok()) {
      return Status(ErrorCode::kOverflow);
    }
    if (staging.size() < reader.count() ||
        *staging_bytes > limits.max_staging_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    if (Access::Aliases(reader, staging.data(), *staging_bytes) ||
        Access::Aliases(reader, destination.data(), *target_bytes) ||
        Access::Aliases(reader, &destination, sizeof(destination)) ||
        internal_array_format::Overlaps(staging.data(), *staging_bytes,
                                        destination.data(), *target_bytes) ||
        internal_array_format::Overlaps(staging.data(), *staging_bytes,
                                        &destination, sizeof(destination))) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto rows = static_cast<std::size_t>(reader.shape()[0]);
    status = Access::Payload<Element>(
        reader,
        [staging, rows](std::size_t row, std::size_t column, Element value) {
          staging[row + column * rows] = value;
        });
    if (!status.ok()) {
      return status;
    }
    for (std::size_t column = 0;
         rows != 0 && column < static_cast<std::size_t>(reader.shape()[1]);
         ++column) {
      for (std::size_t row = 0; row < rows; ++row) {
        destination.data()[internal_dense_matrix_market::Offset(
            destination, row, column)] = staging[row + column * rows];
      }
    }
    Access::Commit(reader);
    return Status::Ok();
  }
}

/** @brief Writes a complete column-oriented Matrix Market array.
 * @tparam Element Supported scalar, optionally const; determines the wire
 * field.
 * @tparam Rank Compile-time rank; only two is accepted.
 * @param[in] view Valid host/pinned mapping, retained unmodified; padding
 * ignored.
 * @param[in,out] sink Explicit synchronous sink; partial output may remain on
 * error.
 * @param[in] symmetry Explicit compression; the whole matrix is validated
 * first.
 * @param[in] limits Independent shape, token, line, output and scratch budgets.
 * @param[in,out] scratch Disjoint caller numeric/header encoding bytes.
 * @param[out] report Counts emitted wire records; reset before ordinary
 * validation. Aliased report storage is rejected without touching it.
 * @return Format/value/resource/I/O status. Real/integer Hermitian and array
 * pattern are invalid. Nonfinite values are rejected, never encoded. No
 * allocation, densification, implicit transfer or provider dispatch occurs.
 * O(rows*cols + output bytes); exact full-matrix symmetry checks precede the
 * first sink write. Caller serializes sink access; report/scratch/source
 * storage must be mutually disjoint.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status WriteDenseMatrixMarket(const DenseView<Element, Rank>& view,
                              ByteSink& sink, MatrixMarketSymmetry symmetry,
                              const ArrayIoLimits& limits,
                              std::span<std::byte> scratch,
                              ArrayIoReport& report) {
  auto aliases = internal_dense_matrix_market::ValidateWriterAliases(
      view, scratch, report);
  if (!aliases.ok()) {
    return aliases;
  }
  if (internal_array_format::Overlaps(scratch.data(), scratch.size(), &sink,
                                      sizeof(sink))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report = {};
  using Value = std::remove_const_t<Element>;
  if constexpr (Rank != 2) {
    return Status(ErrorCode::kShape);
  } else if constexpr (!internal_array_format::kWireScalar<Value>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    auto status = internal_dense_matrix_market::ValidateWrite(view, symmetry,
                                                              limits, scratch);
    if (!status.ok()) {
      return status;
    }
    const auto field = internal_matrix_market::ScalarField<Value>();
    internal_array_io::Output output(sink, limits.max_output_bytes, report);
    status = internal_dense_matrix_market::WriteHeader(
        view.extents(), field, symmetry, limits, scratch, output);
    if (!status.ok()) {
      return status;
    }
    report.section = ArrayIoSection::kPayload;
    for (std::size_t column = 0;
         view.extents()[0] != 0 &&
         column < static_cast<std::size_t>(view.extents()[1]);
         ++column) {
      const std::size_t first =
          symmetry == MatrixMarketSymmetry::kGeneral
              ? 0
              : column + (symmetry == MatrixMarketSymmetry::kSkewSymmetric);
      for (std::size_t row = first;
           row < static_cast<std::size_t>(view.extents()[0]); ++row) {
        const Value value = view.data()[internal_dense_matrix_market::Offset(
            view, row, column)];
        auto bytes =
            internal_dense_matrix_market::ValueBytes(value, scratch, limits);
        if (!bytes.ok()) {
          return bytes.status();
        }
        if (*bytes >= limits.max_header_bytes) {
          return Status(ErrorCode::kAllocation);
        }
        status = internal_matrix_market::WriteValue(
            value, field, scratch, limits.max_token_bytes, output);
        if (status.ok()) {
          status = internal_matrix_market::Text(output, "\n");
        }
        if (!status.ok()) {
          return status;
        }
        ++report.values_processed;
      }
    }
    report.section = ArrayIoSection::kComplete;
    return Status::Ok();
  }
}

/** @brief Writes an owner's values using the same checked array view contract.
 * @tparam Element Supported owner scalar; no field conversion on writing.
 * @tparam ExtentsType Owner's exact static/dynamic rank-two shape.
 * @param[in] array Owner retained unmodified; no view escapes.
 * @param[in,out] sink Explicit synchronous sink.
 * @param[in] symmetry Full-matrix-validated compression choice.
 * @param[in] limits Independent resource and byte limits.
 * @param[in,out] scratch Caller bytes disjoint from the owner and its storage.
 * @param[out] report Actual accepted-byte and complete-record progress.
 * @return Owner view or writer status; failure may leave a sink prefix. Same
 * O(rows*cols + output bytes), no-allocation and CPU boundary as the view
 * overload. Caller manages borrowed lifetimes and synchronization.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType>
Status WriteDenseMatrixMarket(const DenseArray<Element, ExtentsType>& array,
                              ByteSink& sink, MatrixMarketSymmetry symmetry,
                              const ArrayIoLimits& limits,
                              std::span<std::byte> scratch,
                              ArrayIoReport& report) {
  if (internal_array_format::Overlaps(&array, sizeof(array), scratch.data(),
                                      scratch.size()) ||
      internal_array_format::Overlaps(&array, sizeof(array), &report,
                                      sizeof(report))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = array.view();
  if (!view.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(view));
  }
  return WriteDenseMatrixMarket(*view, sink, symmetry, limits, scratch, report);
}

/** @brief Prepares and transactionally loads one whole Matrix Market array
 * stream.
 * @tparam Element Explicit destination scalar and exact-conversion policy.
 * @tparam ExtentsType Rank-two static/dynamic destination extents.
 * @tparam Layout Explicit left/right physical owner layout.
 * @param[in,out] source Synchronous borrowed source; trailing comments and EOF
 * required.
 * @param[in] resource Host resource outliving a successful owner.
 * @param[in] layout Physical layout independent of file order.
 * @param[in,out] scratch Bounded disjoint caller token bytes, not allocated.
 * @param[in] limits Copied resource caps; EOF probes need one spare input byte.
 * @param[out] report Progress, record count and publication state.
 * @return Complete owner or failure with candidate resources released. At most
 * one explicit candidate allocation, no hidden transfer; O(input bytes +
 * rows*cols). Caller retains/exclusively uses the borrowed objects through the
 * call.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> ReadDenseMatrixMarket(
    ByteSource& source, MemoryResource& resource, Layout layout,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report) {
  auto reader =
      DenseMatrixMarketReader::Prepare(source, scratch, limits, report);
  if (!reader.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(reader));
  }
  return ReadDenseMatrixMarket<Element, ExtentsType>(*reader, resource, layout);
}

/** @brief Loads one whole Matrix Market array file and checks explicit close.
 * @tparam Element Caller-selected supported destination scalar.
 * @tparam ExtentsType Rank-two static/dynamic shape.
 * @tparam Layout Explicit owner layout, independent of wire order.
 * @param[in] path File to open through the existing Core File boundary.
 * @param[in] resource Host resource outliving the returned owner.
 * @param[in] layout Left/right physical arrangement.
 * @param[in,out] scratch Bounded caller parser bytes retained throughout the
 * call.
 * @param[in] limits Independent resource caps, including an available EOF-probe
 * byte.
 * @param[out] report Progress/publication plus any secondary close failure.
 * @return Complete owner only after payload, trailing comments, EOF and close;
 * otherwise no owner escapes and candidate storage is released. One explicit
 * candidate allocation and O(file bytes + rows*cols) codec work; Core File may
 * separately allocate handle/path diagnostics. No transfer or source rollback.
 * Caller manages shared path/resource synchronization and all borrowed
 * lifetimes.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> LoadDenseMatrixMarket(
    const std::filesystem::path& path, MemoryResource& resource, Layout layout,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report) {
  if (internal_array_format::Overlaps(scratch.data(), scratch.size(), &report,
                                      sizeof(report))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report = {};
  auto file = File::OpenRead(path);
  if (!file.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(file));
  }
  auto owner = ReadDenseMatrixMarket<Element, ExtentsType>(
      *file, resource, layout, scratch, limits, report);
  auto close = file->Close();
  if (!close.ok()) {
    report.cleanup_error = close.code();
    report.committed = false;
    report.section = ArrayIoSection::kTrailer;
    if (owner.ok()) {
      return close;
    }
  }
  return owner;
}

/** @brief Creates/truncates a path and writes one checked Matrix Market array.
 * @tparam Element Supported scalar, optionally const.
 * @tparam Rank Compile-time rank, required to be two.
 * @param[in] path Explicit Core File path; no implicit overwrite permission.
 * @param[in] view Valid host/pinned values retained unmodified.
 * @param[in] symmetry Compression choice, fully validated before opening the
 * file.
 * @param[in] overwrite Required explicit create/truncate intent.
 * @param[in] limits Independent output/shape/scratch budgets.
 * @param[in,out] scratch Disjoint caller encoding bytes.
 * @param[out] report Accepted bytes and secondary cleanup failure, if any.
 * @return First write/flush/close or validation failure. Opening may truncate
 * an existing path; failed writes can leave a prefix, without atomic
 * replacement or durability promises. Codecs allocate nothing and use
 * O(rows*cols + output bytes); Core File's handle/path allocation contract is
 * separate. CPU-only, with caller-managed path and borrowed-object
 * synchronization.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status SaveDenseMatrixMarket(const std::filesystem::path& path,
                             const DenseView<Element, Rank>& view,
                             MatrixMarketSymmetry symmetry,
                             ArrayFileOverwrite overwrite,
                             const ArrayIoLimits& limits,
                             std::span<std::byte> scratch,
                             ArrayIoReport& report) {
  auto aliases = internal_dense_matrix_market::ValidateWriterAliases(
      view, scratch, report);
  if (!aliases.ok()) {
    return aliases;
  }
  report = {};
  if (overwrite != ArrayFileOverwrite::kTruncate) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto status = internal_dense_matrix_market::ValidateWrite(view, symmetry,
                                                            limits, scratch);
  if (!status.ok()) {
    return status;
  }
  auto file = File::OpenWrite(path);
  if (!file.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(file));
  }
  status =
      WriteDenseMatrixMarket(view, *file, symmetry, limits, scratch, report);
  if (status.ok()) {
    report.section = ArrayIoSection::kTrailer;
    status = file->Flush();
  }
  auto close = file->Close();
  if (!close.ok()) {
    report.cleanup_error = close.code();
    if (status.ok()) {
      status = std::move(close);
    }
  }
  if (status.ok()) {
    report.section = ArrayIoSection::kComplete;
  }
  return status;
}

}  // namespace asc

#endif  // ASC_DENSE_MATRIX_MARKET_H_
