#ifndef ASC_DENSE_IO_H_
#define ASC_DENSE_IO_H_

/**
 * @file
 * @brief Dense-owned ASC text/binary archives and staged transactional reads.
 *
 * Borrowed ByteSource/ByteSink errors retain ErrorCode and native_code while
 * dropping owning message/provider strings, as specified in
 * asc/core/array_io.h. File and owner-resource failure contracts are separate.
 * @ingroup asc_dense
 */

#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/array_format.h"
#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/io.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/export.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"

namespace asc {
namespace internal_dense_io {
class ReaderAccess;
ASC_DENSE_EXPORT Status ValidateShape(std::span<const extent_t> shape,
                                      std::uint64_t count,
                                      ArrayScalarCode scalar,
                                      const ArrayIoLimits& limits);
ASC_DENSE_EXPORT Status WriteHeader(std::span<const extent_t> shape,
                                    std::uint64_t count, ArrayScalarCode scalar,
                                    bool binary, const ArrayIoLimits& limits,
                                    std::span<std::byte> scratch,
                                    internal_array_io::Output& output,
                                    ArrayIoReport& report);
}  // namespace internal_dense_io

/**
 * @brief Move-only prepared Dense archive cursor with caller-owned metadata.
 *
 * Preparation consumes exactly one header and retains the source position;
 * payload operations never expect the header again. No source byte is read
 * ahead of its exact grammar boundary. The source, metadata, scratch and
 * report must outlive this reader and remain exclusively available/unmodified
 * during its use. Limits are copied. Metadata/scratch are borrowed, never
 * allocated. Successful consumption, any failure, or move invalidates further
 * reads through the old cursor. Reconstruct only at a known source boundary;
 * there is no automatic resynchronization or source rollback. CPU-only.
 * @ingroup asc_dense
 */
class ASC_DENSE_EXPORT DenseArrayReader {
 public:
  /**
   * @brief Prepares an ASC text-v1 Dense header without allocating values.
   * @param[in,out] source Synchronous source retained until payload completion.
   * @param[in,out] metadata Caller extent capacity, checked before rank writes.
   * @param[in,out] scratch Caller parser/token bytes, disjoint from metadata.
   * @param[in] limits Copied independent/aggregate resource limits.
   * @param[out] report Reset before parsing; retains consumed bytes on failure.
   * @param[in] require_eof Whether trailing whitespace and EOF are part of the
   * eventual payload transaction. Its unseekable EOF probe needs spare input
   * byte budget. False consumes exactly through the frame terminator.
   * @return Prepared cursor, or type/kind/version/shape/size/I/O failure.
   * Metadata/scratch may change on failure; no numerical owner is published.
   * Work is linear in bounded header bytes; no allocation/transfer occurs.
   * @ingroup asc_dense
   */
  static Result<DenseArrayReader> PrepareText(ByteSource& source,
                                              std::span<extent_t> metadata,
                                              std::span<std::byte> scratch,
                                              const ArrayIoLimits& limits,
                                              ArrayIoReport& report,
                                              bool require_eof = false);

  /**
   * @brief Prepares an ASC binary-v1 Dense envelope without allocating values.
   * @param[in,out] source Borrowed source retained through payload/checksum.
   * @param[in,out] metadata Caller extent capacity; rank is checked first.
   * @param[in,out] scratch Disjoint caller bytes, at least 56 for the envelope.
   * @param[in] limits Copied byte/rank/extent/decoded resource limits.
   * @param[out] report Initialized before parsing, with actual source progress.
   * @param[in] require_eof If true, exact file EOF joins checksum validation in
   * the transaction; its EOF probe requires spare input byte budget.
   * @return Prepared cursor or bounded envelope/shape/I/O error. No numerical
   * allocation, provider selection, transfer or source rollback occurs.
   * Same lifetime, failure and concurrency contract as PrepareText applies.
   * @ingroup asc_dense
   */
  static Result<DenseArrayReader> PrepareBinary(ByteSource& source,
                                                std::span<extent_t> metadata,
                                                std::span<std::byte> scratch,
                                                const ArrayIoLimits& limits,
                                                ArrayIoReport& report,
                                                bool require_eof = false);

  /** @brief Readers cannot duplicate a consumable source cursor.
   * @ingroup asc_dense
   */
  DenseArrayReader(const DenseArrayReader&) = delete;
  /** @brief Copy assignment cannot duplicate a cursor. @ingroup asc_dense */
  DenseArrayReader& operator=(const DenseArrayReader&) = delete;
  /** @brief Transfers the cursor and invalidates the source object.
   * @param[in,out] other Reader whose borrowed lifetimes remain unchanged.
   * @ingroup asc_dense
   */
  DenseArrayReader(DenseArrayReader&& other) noexcept;
  /** @brief Assignment cannot discard an unfinished cursor. @ingroup asc_dense
   */
  DenseArrayReader& operator=(DenseArrayReader&&) = delete;
  /** @brief Releases no borrowed resources and performs no further reads.
   * @ingroup asc_dense
   */
  ~DenseArrayReader() = default;

  /** @brief Returns exact wire scalar identity; no conversion is implied.
   * @return Scalar declared in the validated envelope. @ingroup asc_dense
   */
  [[nodiscard]] ArrayScalarCode scalar() const noexcept { return scalar_; }
  /** @brief Views validated extents in dimension order without allocating.
   * @return Borrowed metadata, valid until its caller changes/releases it.
   * Caller must not mutate metadata while the reader is live.
   * @ingroup asc_dense
   */
  [[nodiscard]] std::span<const extent_t> shape() const noexcept {
    return shape_;
  }
  /** @brief Reports Dense logical count; scalar rank zero has one value.
   * @return Validated checked product of extents. @ingroup asc_dense
   */
  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
  /** @brief Reports whether a payload can still be consumed exactly once.
   * @return False after consumption, failure or move. @ingroup asc_dense
   */
  [[nodiscard]] bool ready() const noexcept { return ready_; }

 private:
  /// @cond ASC_INTERNAL
  friend class internal_dense_io::ReaderAccess;
  /// @endcond
  DenseArrayReader(ByteSource& source, std::span<extent_t> metadata,
                   std::span<std::byte> scratch, ArrayIoLimits limits,
                   ArrayIoReport& report, bool binary, bool require_eof)
      : input_(source, limits, report),
        shape_(metadata),
        scratch_(scratch),
        binary_(binary),
        require_eof_(require_eof) {}
  Status ParseTextHeader();
  Status ParseBinaryHeader();
  Status Trailer();
  Status Fail(ErrorCode code) {
    ready_ = false;
    return input_.Fail(code);
  }

  template <typename T, typename Store>
  Status Payload(std::size_t count, Store store) {
    if (!ready_) {
      return Fail(ErrorCode::kInvalidState);
    }
    ready_ = false;
    if (count != count_) {
      return Fail(ErrorCode::kShape);
    }
    input_.report().section = ArrayIoSection::kPayload;
    const std::span<char> text(reinterpret_cast<char*>(scratch_.data()),
                               scratch_.size());
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
      if (binary_) {
        const std::size_t width = internal_array_io::ScalarWidth(scalar_);
        if (width > scratch_.size()) {
          return Fail(ErrorCode::kAllocation);
        }
        Status status = input_.Read(scratch_.first(width));
        if (!status.ok()) {
          return status;
        }
        auto value = internal_array_io::DecodeScalar<T>(scratch_.first(width));
        if (!value.ok()) {
          return Fail(value.status().code());
        }
        store(ordinal, *value);
      } else {
        auto token = input_.Line(text, input_.limits().max_token_bytes);
        if (!token.ok()) {
          return token.status();
        }
        auto value = internal_array_io::ParseScalar<T>(
            *token, input_.limits().max_token_bytes);
        if (!value.ok()) {
          return Fail(value.status().code());
        }
        store(ordinal, *value);
      }
      ++input_.report().values_processed;
    }
    return Trailer();
  }

  internal_array_io::Input input_;
  std::span<extent_t> shape_;
  std::span<std::byte> scratch_;
  ArrayScalarCode scalar_ = ArrayScalarCode::kU8;
  std::uint64_t count_ = 0;
  bool binary_ = false;
  bool require_eof_ = false;
  bool ready_ = false;
};

namespace internal_dense_io {

class ReaderAccess {
 public:
  static const ArrayIoLimits& Limits(const DenseArrayReader& reader) {
    return reader.input_.limits();
  }
  static std::span<std::byte> Scratch(const DenseArrayReader& reader) {
    return reader.scratch_;
  }
  static Status Fail(DenseArrayReader& reader, ErrorCode code) {
    return reader.Fail(code);
  }
  static void Commit(DenseArrayReader& reader) {
    reader.input_.report().committed = true;
  }
  template <typename T>
  static Status Payload(DenseArrayReader& reader, std::span<T> values) {
    return reader.Payload<T>(
        values.size(),
        [values](std::size_t ordinal, T value) { values[ordinal] = value; });
  }
  template <typename T, typename Store>
  static Status PayloadMapped(DenseArrayReader& reader, std::size_t count,
                              Store store) {
    return reader.Payload<T>(count, store);
  }
};

template <typename ExtentsType, std::size_t... Dimensions>
Result<ExtentsType> MatchExtents(
    std::span<const extent_t> shape,
    std::index_sequence<Dimensions...> /*dimensions*/) {
  constexpr std::array<extent_t, ExtentsType::kRank> kStatic{
      ExtentsType::template static_extent<Dimensions>()...};
  if (shape.size() != ExtentsType::kRank) {
    return Status(ErrorCode::kShape);
  }
  std::array<extent_t, ExtentsType::kDynamicRank> dynamic{};
  std::size_t next = 0;
  for (std::size_t dimension = 0; dimension < shape.size(); ++dimension) {
    if (kStatic[dimension] == kDynamicExtent) {
      dynamic[next++] = shape[dimension];
    } else if (kStatic[dimension] != shape[dimension]) {
      return Status(ErrorCode::kShape);
    }
  }
  return ExtentsType::Create(std::span<const extent_t>(dynamic));
}

// Preserve DenseLayout's representability checks without constructing its
// owning diagnostics for hostile archive shapes. Its logical-size validation
// checks forward prefixes even for right layout and before a later zero.
template <typename Layout>
Status ValidateOwnerLayout(std::span<const extent_t> shape, Layout /*layout*/) {
  extent_t product = 1;
  for (extent_t extent : shape) {
    auto next = internal_array_io::MultiplySize(product, extent);
    if (!next.ok()) {
      return next.status();
    }
    product = *next;
  }
  if constexpr (std::same_as<Layout, LayoutRight>) {
    product = 1;
    for (std::size_t reverse = shape.size(); reverse > 0; --reverse) {
      auto next = internal_array_io::MultiplySize(product, shape[reverse - 1]);
      if (!next.ok()) {
        return next.status();
      }
      product = *next;
    }
  }
  return Status::Ok();
}

template <typename T, std::size_t Rank>
Status ValidateView(const DenseView<T, Rank>& view,
                    std::span<std::byte> scratch, const ArrayIoLimits& limits) {
  if constexpr (!internal_array_format::kWireScalar<std::remove_const_t<T>>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (view.memory_space() != MemorySpace::kHost &&
        view.memory_space() != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    auto bytes = internal_array_io::MultiplySize(
        view.mapping().required_span_size(), sizeof(T));
    if (!bytes.ok()) {
      return bytes.status();
    }
    if (scratch.size() > limits.max_scratch_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    if (internal_array_format::Overlaps(view.data(), *bytes, scratch.data(),
                                        scratch.size()) ||
        internal_array_format::Overlaps(&view, sizeof(view), scratch.data(),
                                        scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    return ValidateShape(
        view.extents(), static_cast<std::uint64_t>(view.logical_size()),
        internal_array_io::ScalarCode<std::remove_const_t<T>>(), limits);
  }
}

template <std::size_t Rank>
std::size_t Offset(std::size_t ordinal, std::span<const extent_t, Rank> shape,
                   std::span<const stride_t, Rank> strides) {
  std::size_t offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    const auto extent = static_cast<std::size_t>(shape[dimension]);
    offset += (ordinal % extent) * static_cast<std::size_t>(strides[dimension]);
    ordinal /= extent;
  }
  return offset;
}

template <typename Element, std::size_t Rank>
Status Write(const DenseView<Element, Rank>& view, ByteSink& sink,
             const ArrayIoLimits& limits, std::span<std::byte> scratch,
             ArrayIoReport& report, bool binary) {
  report = {};
  Status status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  status = ValidateView(view, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  using T = std::remove_const_t<Element>;
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    const auto scalar = internal_array_io::ScalarCode<T>();
    internal_array_io::Output output(sink, limits.max_output_bytes, report);
    status = WriteHeader(view.extents(),
                         static_cast<std::uint64_t>(view.logical_size()),
                         scalar, binary, limits, scratch, output, report);
    if (!status.ok()) {
      return status;
    }
    report.section = ArrayIoSection::kPayload;
    const auto count = static_cast<std::size_t>(view.logical_size());
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
      const T value =
          view.data()[Offset(ordinal, view.extents(), view.strides())];
      if (binary) {
        const auto width = internal_array_io::ScalarWidth(scalar);
        if (scratch.size() < width) {
          return Status(ErrorCode::kAllocation);
        }
        status = internal_array_io::EncodeScalar(value, scratch.first(width));
        if (status.ok()) {
          status = output.Write(scratch.first(width));
        }
      } else {
        const std::span<char> text(reinterpret_cast<char*>(scratch.data()),
                                   scratch.size());
        constexpr int kPrecision =
            std::same_as<T, float> || std::same_as<T, std::complex<float>> ? 9
                                                                           : 17;
        auto size = internal_array_format::FormatScalar(
            value, text, ArrayFloatFormat::kGeneral, kPrecision);
        if (!size.ok()) {
          return size.status();
        }
        if (*size > limits.max_token_bytes) {
          return Status(ErrorCode::kAllocation);
        }
        status = output.Text(std::string_view(text.data(), *size));
      }
      if (!status.ok()) {
        return status;
      }
      ++report.values_processed;
      if (!binary) {
        status = output.Text("\n");
        if (!status.ok()) {
          return status;
        }
      }
    }
    report.section = ArrayIoSection::kTrailer;
    if (binary) {
      if (scratch.size() < 4) {
        return Status(ErrorCode::kAllocation);
      }
      status = EncodeLittleEndian(output.checksum(), scratch.first(4));
      if (status.ok()) {
        status = output.Write(scratch.first(4), false);
      }
    } else {
      status = output.Text("end\n");
    }
    if (status.ok()) {
      report.section = ArrayIoSection::kComplete;
    }
    return status;
  }
}

}  // namespace internal_dense_io

/**
 * @brief Loads a prepared Dense frame into a new caller-resource owner.
 * @tparam Element Exact wire scalar type; no conversion is performed.
 * @tparam ExtentsType Static/dynamic shape type checked before allocation.
 * @tparam Layout Explicit LayoutLeft or LayoutRight destination arrangement.
 * @param[in,out] reader Prepared one-use cursor with retained header metadata.
 * @param[in] resource Host resource that outlives the result. Exactly one
 * allocation request occurs, including for empty storage, within all budgets.
 * @param[in] layout Destination physical layout, independent of wire order.
 * @return Fully validated owner after trailer/checksum and optional EOF, or
 * failure with candidate storage released and no partial owner. Source progress
 * remains in reader's report; no source rollback/transfer/provider dispatch.
 * CPU synchronous work is O(rank + count*rank); no hidden packing allocation.
 * Borrowed objects must remain unmodified and nonconcurrently accessed.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> ReadDenseArray(
    DenseArrayReader& reader, MemoryResource& resource, Layout layout) {
  using Access = internal_dense_io::ReaderAccess;
  if constexpr (!internal_array_format::kWireScalar<Element>) {
    return Access::Fail(reader, ErrorCode::kUnsupported);
  } else {
    if (!reader.ready()) {
      return Access::Fail(reader, ErrorCode::kInvalidState);
    }
    if (reader.scalar() != internal_array_io::ScalarCode<Element>()) {
      return Access::Fail(reader, ErrorCode::kEncoding);
    }
    auto extents = internal_dense_io::MatchExtents<ExtentsType>(
        reader.shape(), std::make_index_sequence<ExtentsType::kRank>{});
    if (!extents.ok()) {
      return Access::Fail(reader, extents.status().code());
    }
    if (resource.space() != MemorySpace::kHost) {
      return Access::Fail(reader, ErrorCode::kMemoryAccess);
    }
    const auto& limits = Access::Limits(reader);
    auto bytes = internal_array_io::MultiplySize(
        static_cast<std::size_t>(reader.count()), sizeof(Element));
    if (!bytes.ok()) {
      return Access::Fail(reader, bytes.status().code());
    }
    if (*bytes > limits.max_staging_bytes ||
        *bytes > limits.max_decoded_bytes ||
        *bytes > limits.max_allocation_bytes || limits.max_allocations == 0) {
      return Access::Fail(reader, ErrorCode::kAllocation);
    }
    Status status =
        internal_dense_io::ValidateOwnerLayout(reader.shape(), layout);
    if (!status.ok()) {
      return Access::Fail(reader, status.code());
    }
    auto candidate =
        DenseArray<Element, ExtentsType>::Create(resource, *extents, layout);
    if (!candidate.ok()) {
      return Access::Fail(reader, candidate.status().code());
    }
    auto view = candidate->view();
    if (!view.ok()) {
      return Access::Fail(reader, view.status().code());
    }
    status = Access::PayloadMapped<Element>(
        reader, static_cast<std::size_t>(reader.count()),
        [&view](std::size_t ordinal, Element value) {
          view->data()[internal_dense_io::Offset(ordinal, view->extents(),
                                                 view->strides())] = value;
        });
    if (!status.ok()) {
      return status;
    }
    Access::Commit(reader);
    return std::move(*candidate);
  }
}

/**
 * @brief Transactionally reads a prepared frame into an existing Dense view.
 * @tparam Element Exact supported writable wire scalar.
 * @tparam Rank Destination compile-time rank.
 * @tparam StagingExtent Static or dynamic caller staging capacity.
 * @param[in,out] reader One-use prepared cursor; consumes full trailer/checksum
 * and configured EOF before mutation. A failed cursor cannot be replayed.
 * @param[in,out] destination Unique host/pinned-host mapping, all backing bytes
 * alive. Every value and padding byte is unchanged on any returned failure.
 * @param[in,out] staging Caller live typed values, at least count entries,
 * disjoint from target, metadata and parse scratch. No allocation occurs.
 * @return Shape/type/placement/capacity/alias/I/O status or OK after a
 * prevalidated nonfailing scatter. No fallible At calls occur during commit.
 * Work O(rank + count*rank); no transfer, synchronization or densification.
 * Caller serializes shared sources/targets and retains all borrowed lifetimes.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank, std::size_t StagingExtent>
  requires(!std::is_const_v<Element>)
Status ReadDenseArrayInto(DenseArrayReader& reader,
                          const DenseView<Element, Rank>& destination,
                          std::span<Element, StagingExtent> staging) {
  using Access = internal_dense_io::ReaderAccess;
  if constexpr (!internal_array_format::kWireScalar<Element>) {
    return Access::Fail(reader, ErrorCode::kUnsupported);
  } else {
    if (!reader.ready()) {
      return Access::Fail(reader, ErrorCode::kInvalidState);
    }
    if (reader.scalar() != internal_array_io::ScalarCode<Element>()) {
      return Access::Fail(reader, ErrorCode::kEncoding);
    }
    if (reader.shape().size() != Rank) {
      return Access::Fail(reader, ErrorCode::kShape);
    }
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (reader.shape()[dimension] != destination.extents()[dimension]) {
        return Access::Fail(reader, ErrorCode::kShape);
      }
    }
    const auto scratch = Access::Scratch(reader);
    const auto& limits = Access::Limits(reader);
    Status status =
        internal_dense_io::ValidateView(destination, scratch, limits);
    if (!status.ok()) {
      return Access::Fail(reader, status.code());
    }
    const auto count = static_cast<std::size_t>(reader.count());
    auto staging_bytes =
        internal_array_io::MultiplySize(staging.size(), sizeof(Element));
    auto target_bytes = internal_array_io::MultiplySize(
        destination.mapping().required_span_size(), sizeof(Element));
    if (!staging_bytes.ok() || !target_bytes.ok()) {
      return Access::Fail(reader, ErrorCode::kOverflow);
    }
    if (staging.size() < count || *staging_bytes > limits.max_staging_bytes) {
      return Access::Fail(reader, ErrorCode::kAllocation);
    }
    if (count != 0 && staging.data() == nullptr) {
      return Access::Fail(reader, ErrorCode::kInvalidArgument);
    }
    if (internal_array_format::Overlaps(staging.data(), *staging_bytes,
                                        destination.data(), *target_bytes) ||
        internal_array_format::Overlaps(staging.data(), *staging_bytes,
                                        scratch.data(), scratch.size()) ||
        internal_array_format::Overlaps(staging.data(), *staging_bytes,
                                        reader.shape().data(),
                                        reader.shape().size_bytes()) ||
        internal_array_format::Overlaps(destination.data(), *target_bytes,
                                        reader.shape().data(),
                                        reader.shape().size_bytes())) {
      return Access::Fail(reader, ErrorCode::kInvalidArgument);
    }
    status = Access::Payload(reader, staging.first(count));
    if (!status.ok()) {
      return status;
    }
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
      destination.data()[internal_dense_io::Offset(
          ordinal, destination.extents(), destination.strides())] =
          staging[ordinal];
    }
    Access::Commit(reader);
    return Status::Ok();
  }
}

/**
 * @brief Writes a complete ASC text-v1 frame in dimension-zero-fastest order.
 * @tparam Element Supported exact scalar, optionally const.
 * @tparam Rank Compile-time rank, including zero.
 * @param[in] view Valid host/pinned-host storage retained without mutation.
 * @param[in,out] sink Explicit synchronous sink; partial bytes can remain on
 * failure.
 * @param[in] limits Explicit token/header/decoded/output/scratch limits.
 * @param[in,out] scratch Disjoint bounded caller bytes fitting header and
 * scalar.
 * @param[out] report Initialized before checks; actual accepted progress.
 * @return Validation/placement/limit/I/O status, or OK after full terminator.
 * No allocation, hidden packing, transfer or provider selection. O(rank*count)
 * logical indexing ignores padding. Caller serializes shared sink access.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status WriteDenseArrayText(const DenseView<Element, Rank>& view, ByteSink& sink,
                           const ArrayIoLimits& limits,
                           std::span<std::byte> scratch,
                           ArrayIoReport& report) {
  return internal_dense_io::Write(view, sink, limits, scratch, report, false);
}

/**
 * @brief Writes field-encoded ASC binary-v1 with CRC-32 and exact scalar bits.
 * @tparam Element Exact supported scalar, optionally const; quiet NaN payloads
 * and signed zero are retained on supported IEEE paths, signaling NaNs
 * excluded.
 * @tparam Rank Compile-time rank.
 * @param[in] view Borrowed host/pinned-host values; physical padding is
 * ignored.
 * @param[in,out] sink Synchronous sink; accepted prefixes cannot be rolled
 * back.
 * @param[in] limits Checked independent and output resource budgets.
 * @param[in,out] scratch Disjoint caller bytes, at least 56 for the header.
 * @param[out] report Initialized before validation; accepted-byte/value
 * progress.
 * @return OK after checksum, or validation, encoding, placement, size or I/O
 * status. No allocation/transfer/synchronization occurs. O(rank*count) work.
 * All borrowed objects outlive the call; shared sink concurrency is
 * caller-owned.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status WriteDenseArrayBinary(const DenseView<Element, Rank>& view,
                             ByteSink& sink, const ArrayIoLimits& limits,
                             std::span<std::byte> scratch,
                             ArrayIoReport& report) {
  return internal_dense_io::Write(view, sink, limits, scratch, report, true);
}

/**
 * @brief Writes an owner's complete ASC text-v1 values through its const view.
 * @tparam Element Supported exact scalar.
 * @tparam ExtentsType Owner's compile-time-rank static/dynamic extents.
 * @param[in] array Owner retained alive and unmodified during the call.
 * @param[in,out] sink Explicit synchronous sink; errors may leave a prefix.
 * @param[in] limits Independent input-shape/output/scratch budgets.
 * @param[in,out] scratch Disjoint caller bytes; no allocation occurs.
 * @param[out] report Initialized before owner validation; accepted progress.
 * @return View creation or text writer status. Complexity, failure, host
 * placement and concurrency match WriteDenseArrayText(view,...); no view
 * escapes.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType>
Status WriteDenseArrayText(const DenseArray<Element, ExtentsType>& array,
                           ByteSink& sink, const ArrayIoLimits& limits,
                           std::span<std::byte> scratch,
                           ArrayIoReport& report) {
  report = {};
  if (internal_array_format::Overlaps(&array, sizeof(array), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = array.view();
  if (!view.ok()) {
    return view.status();
  }
  return WriteDenseArrayText(*view, sink, limits, scratch, report);
}

/**
 * @brief Writes an owner's complete ASC binary-v1 values through its const
 * view.
 * @tparam Element Supported exact scalar.
 * @tparam ExtentsType Owner shape type.
 * @param[in] array Owner kept alive and unmodified for the synchronous call.
 * @param[in,out] sink Explicit sink; partial accepted bytes remain on failure.
 * @param[in] limits Bounded metadata/value/output capacities.
 * @param[in,out] scratch Disjoint caller bytes, at least 56; no allocation.
 * @param[out] report Reset before validation; actual output progress.
 * @return Owner-view or binary-writer status. Bit identity, placement,
 * complexity and concurrency follow the view overload; no hidden transfer.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType>
Status WriteDenseArrayBinary(const DenseArray<Element, ExtentsType>& array,
                             ByteSink& sink, const ArrayIoLimits& limits,
                             std::span<std::byte> scratch,
                             ArrayIoReport& report) {
  report = {};
  if (internal_array_format::Overlaps(&array, sizeof(array), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = array.view();
  if (!view.ok()) {
    return view.status();
  }
  return WriteDenseArrayBinary(*view, sink, limits, scratch, report);
}

/**
 * @brief Prepares and loads one explicit text frame into a new Dense owner.
 * @tparam Element Exact scalar checked before allocation.
 * @tparam ExtentsType Rank/static extents checked before allocation.
 * @tparam Layout Explicit destination LayoutLeft or LayoutRight.
 * @param[in,out] source Synchronous borrowed source; exactly one frame
 * consumed.
 * @param[in] resource Host resource outliving the result, with one budgeted
 * request.
 * @param[in] layout Physical destination layout independent of wire order.
 * @param[in,out] metadata Caller extent capacity, disjoint from parser scratch.
 * @param[in,out] scratch Bounded caller token bytes retained through this call.
 * @param[in] limits Copied size/rank/byte/allocation budgets.
 * @param[out] report Reset before parsing, with byte/value progress and
 * publication.
 * @return Fully validated owner or error with candidate allocations released.
 * No source rollback, hidden transfer or provider dependency; O(rank*count).
 * Caller serializes source/resource access as their contracts require.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> ReadDenseArrayText(
    ByteSource& source, MemoryResource& resource, Layout layout,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report) {
  auto reader =
      DenseArrayReader::PrepareText(source, metadata, scratch, limits, report);
  if (!reader.ok()) {
    return reader.status();
  }
  return ReadDenseArray<Element, ExtentsType>(*reader, resource, layout);
}

/**
 * @brief Prepares and loads one explicit binary frame into a new Dense owner.
 * @tparam Element Exact scalar identity checked before values allocation.
 * @tparam ExtentsType Compile-time rank/static extents checked before
 * allocation.
 * @tparam Layout Explicit left/right destination layout.
 * @param[in,out] source Borrowed synchronous source; no read-ahead beyond CRC.
 * @param[in] resource Host allocator outliving the result; one budgeted
 * request.
 * @param[in] layout Physical result arrangement, independent of wire order.
 * @param[in,out] metadata Caller extent capacity disjoint from scratch.
 * @param[in,out] scratch Caller parser bytes, at least 56, no hidden
 * allocation.
 * @param[in] limits Copied independent and aggregate resource budgets.
 * @param[out] report Initialized before work; actual source/staging/publication
 * progress.
 * @return Complete checksum-validated owner or error releasing all candidate
 * storage. Source rollback is not promised. No device transfer or provider;
 * O(rank*count) work, with caller-managed shared source/resource concurrency.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> ReadDenseArrayBinary(
    ByteSource& source, MemoryResource& resource, Layout layout,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report) {
  auto reader = DenseArrayReader::PrepareBinary(source, metadata, scratch,
                                                limits, report);
  if (!reader.ok()) {
    return reader.status();
  }
  return ReadDenseArray<Element, ExtentsType>(*reader, resource, layout);
}

namespace internal_dense_io {

template <typename Element, std::size_t Rank>
Status Save(const std::filesystem::path& path,
            const DenseView<Element, Rank>& view, ArrayFileOverwrite overwrite,
            const ArrayIoLimits& limits, std::span<std::byte> scratch,
            ArrayIoReport& report, bool binary) {
  report = {};
  if (overwrite != ArrayFileOverwrite::kTruncate) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto status = internal_array_io::ValidateLimits(limits);
  if (status.ok()) {
    status = ValidateView(view, scratch, limits);
  }
  if (!status.ok()) {
    return status;
  }
  auto file = File::OpenWrite(path);
  if (!file.ok()) {
    return file.status();
  }
  status = Write(view, *file, limits, scratch, report, binary);
  if (status.ok()) {
    report.section = ArrayIoSection::kTrailer;
    status = file->Flush();
  }
  const auto close = file->Close();
  if (!close.ok()) {
    report.cleanup_error = close.code();
    if (status.ok()) {
      status = close;
    }
  }
  if (status.ok()) {
    report.section = ArrayIoSection::kComplete;
  }
  return status;
}

template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> Load(
    const std::filesystem::path& path, MemoryResource& resource, Layout layout,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report, bool binary) {
  report = {};
  auto file = File::OpenRead(path);
  if (!file.ok()) {
    return file.status();
  }
  auto reader = binary ? DenseArrayReader::PrepareBinary(
                             *file, metadata, scratch, limits, report, true)
                       : DenseArrayReader::PrepareText(*file, metadata, scratch,
                                                       limits, report, true);
  if (!reader.ok()) {
    const auto close = file->Close();
    if (!close.ok()) {
      report.cleanup_error = close.code();
    }
    return reader.status();
  }
  auto owner = ReadDenseArray<Element, ExtentsType>(*reader, resource, layout);
  const auto close = file->Close();
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

}  // namespace internal_dense_io

/**
 * @brief Creates/truncates a path and saves one complete Dense ASC text file.
 * @tparam Element Supported exact scalar, optionally const.
 * @tparam Rank Compile-time rank.
 * @param[in] path Explicit file path, subject to Core File path semantics.
 * @param[in] view Borrowed host/pinned-host values, unmodified during the call.
 * @param[in] overwrite Required explicit create/truncate intent; no default.
 * @param[in] limits Independent output and resource budgets.
 * @param[in,out] scratch Disjoint caller encoder storage.
 * @param[out] report Accepted-byte progress and any secondary close error.
 * @return Writer, flush or close status, preserving the first failure. Failure
 * may leave a truncated/partial file; no atomic replace, durability or path
 * race guarantee is made. O(rank*count) codec work without codec allocation;
 * Core File's separately documented handle/path allocation boundary applies.
 * Caller serializes access to the path and borrowed objects.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status SaveDenseArrayText(const std::filesystem::path& path,
                          const DenseView<Element, Rank>& view,
                          ArrayFileOverwrite overwrite,
                          const ArrayIoLimits& limits,
                          std::span<std::byte> scratch, ArrayIoReport& report) {
  return internal_dense_io::Save(path, view, overwrite, limits, scratch, report,
                                 false);
}

/**
 * @brief Creates/truncates a path and saves one Dense ASC binary-v1 file.
 * @tparam Element Exact wire scalar, optionally const; quiet NaN bits retained.
 * @tparam Rank Compile-time rank.
 * @param[in] path Explicit path with Core File semantics.
 * @param[in] view Borrowed host/pinned-host values; padding is not written.
 * @param[in] overwrite Required explicit destructive create/truncate intent.
 * @param[in] limits Bounded output/header/shape/value/scratch resources.
 * @param[in,out] scratch Disjoint caller bytes, at least 56 for the header.
 * @param[out] report Accepted-byte progress and secondary checked-close error.
 * @return Encoding, write, flush or checked-close status. Failure can leave a
 * partial file. No atomic replacement or filesystem durability is promised.
 * No codec allocation/transfer occurs; Core File may allocate handles/path
 * diagnostics. O(rank*count); caller owns shared-path/object synchronization.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
Status SaveDenseArrayBinary(const std::filesystem::path& path,
                            const DenseView<Element, Rank>& view,
                            ArrayFileOverwrite overwrite,
                            const ArrayIoLimits& limits,
                            std::span<std::byte> scratch,
                            ArrayIoReport& report) {
  return internal_dense_io::Save(path, view, overwrite, limits, scratch, report,
                                 true);
}

/**
 * @brief Loads a whole Dense ASC text file, accepting only trailing whitespace.
 * @tparam Element Exact scalar identity, checked before values allocation.
 * @tparam ExtentsType Exact rank/static extents, checked before allocation.
 * @tparam Layout Explicit left/right destination layout.
 * @param[in] path Explicit path opened by Core File.
 * @param[in] resource Host resource outliving the returned owner.
 * @param[in] layout Result layout independent of dimension-zero-fast wire
 * order.
 * @param[in,out] metadata Caller extent capacity disjoint from scratch.
 * @param[in,out] scratch Bounded caller parser/token bytes.
 * @param[in] limits Copied limits; whole-file EOF probe needs spare byte
 * budget.
 * @param[out] report Consumed bytes, publication and secondary close error.
 * @return Owner only after payload, terminator, trailing whitespace, EOF and
 * checked close succeed; otherwise candidate storage is released. Exactly one
 * budgeted value allocation, no transfer; Core File's handle/path allocation
 * boundary applies. O(rank*count+file bytes). Borrowed resource/object lifetime
 * and shared-path synchronization remain caller responsibilities.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> LoadDenseArrayText(
    const std::filesystem::path& path, MemoryResource& resource, Layout layout,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report) {
  return internal_dense_io::Load<Element, ExtentsType>(
      path, resource, layout, metadata, scratch, limits, report, false);
}

/**
 * @brief Loads one whole Dense binary-v1 file with checksum and exact EOF.
 * @tparam Element Exact supported scalar checked before allocation.
 * @tparam ExtentsType Exact rank/static extents checked before allocation.
 * @tparam Layout Explicit destination LayoutLeft or LayoutRight.
 * @param[in] path Explicit Core File path.
 * @param[in] resource Host allocator outliving the successful owner.
 * @param[in] layout Physical result order, independent of wire order.
 * @param[in,out] metadata Caller extent capacity retained for parsing.
 * @param[in,out] scratch Disjoint bounded caller bytes, at least 56.
 * @param[in] limits Copied limits; the EOF probe needs spare input-byte budget.
 * @param[out] report Source progress, publication and secondary close failure.
 * @return Owner after payload, CRC, EOF and checked close; any error releases
 * candidate storage. One budgeted value request, no hidden device work; Core
 * File's handle/path allocation boundary applies. O(rank*count+file bytes);
 * caller manages shared resource/path concurrency and borrowed lifetimes.
 * @ingroup asc_dense
 */
template <DenseElement Element, typename ExtentsType, typename Layout>
Result<DenseArray<Element, ExtentsType>> LoadDenseArrayBinary(
    const std::filesystem::path& path, MemoryResource& resource, Layout layout,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report) {
  return internal_dense_io::Load<Element, ExtentsType>(
      path, resource, layout, metadata, scratch, limits, report, true);
}

}  // namespace asc

#endif  // ASC_DENSE_IO_H_
