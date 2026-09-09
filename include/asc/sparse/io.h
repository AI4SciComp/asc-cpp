#ifndef ASC_SPARSE_IO_H_
#define ASC_SPARSE_IO_H_

/** @file
 * @brief Sparse-owned ASC text/binary archives with exact-structure rollback.
 * @ingroup asc_sparse
 */

#include <algorithm>
#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <new>
#include <optional>
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
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/export.h"

namespace asc {

/** @brief Exact Sparse archive structure; reads never implicitly convert kind.
 * @ingroup asc_sparse
 */
enum class SparseArrayKind : std::uint8_t {
  kCoo =
      2,  ///< Dimension-zero-first lexicographic coordinates, arbitrary rank.
  kCsr = 3,  ///< Rank-two row offsets and strictly ordered column indices.
  kCsc = 4,  ///< Rank-two column offsets and strictly ordered row indices.
};

namespace internal_sparse_io {
class ReaderAccess;

ASC_SPARSE_EXPORT Result<std::size_t> StructureCount(
    SparseArrayKind kind, std::span<const extent_t> shape, std::uint64_t count);
ASC_SPARSE_EXPORT Status ValidateShape(SparseArrayKind kind,
                                       std::span<const extent_t> shape,
                                       std::uint64_t count,
                                       ArrayScalarCode scalar,
                                       const ArrayIoLimits& limits);
ASC_SPARSE_EXPORT Status ValidateStructure(SparseArrayKind kind,
                                           std::span<const extent_t> shape,
                                           std::size_t count,
                                           std::span<const index_t> structure);
ASC_SPARSE_EXPORT Status WriteHeader(SparseArrayKind kind,
                                     std::span<const extent_t> shape,
                                     std::size_t count, ArrayScalarCode scalar,
                                     bool binary, const ArrayIoLimits& limits,
                                     std::span<std::byte> scratch,
                                     internal_array_io::Output& output,
                                     ArrayIoReport& report);
ASC_SPARSE_EXPORT Status WriteIndex(std::uint64_t value, bool binary,
                                    std::string_view separator,
                                    const ArrayIoLimits& limits,
                                    std::span<std::byte> scratch,
                                    internal_array_io::Output& output);
}  // namespace internal_sparse_io

/** @brief Move-only prepared Sparse header and one-use payload cursor.
 *
 * Borrows source, metadata, scratch and report; each must outlive this cursor
 * and be exclusively available during its synchronous use. No source read-ahead
 * or allocation occurs. Header metadata may change on preparation failure but
 * no owner is published. After any failure, successful consumption or move,
 * the old cursor cannot read again; restart only at a known frame boundary.
 * The caller must keep metadata/scratch disjoint from future target storage.
 * Source-based Into wrappers perform that check before reading the header.
 * Only host Sparse values/structure are supported; no transfer, densification,
 * Dense dependency or provider selection is implied. @ingroup asc_sparse
 */
class SparseArrayReader {
 public:
  /** @brief Reads an ASC text-v1 Sparse header without allocating values.
   * @param source Borrowed synchronous source; short reads are normal.
   * @param metadata Caller extent capacity, checked before rank-sized writes.
   * @param scratch Disjoint caller token/metadata scratch bytes.
   * @param limits Copied independent and aggregate resource budgets.
   * @param report Reset before parsing; retains exact consumed-byte progress.
   * @param require_eof Include trailing-whitespace/EOF checking in eventual
   * payload transaction; EOF probing needs one spare input-budget byte.
   * @return Prepared cursor or bounded header/type/kind/shape/I/O failure.
   * O(header bytes) work; no allocation, source rollback or synchronization.
   * @ingroup asc_sparse
   */
  static ASC_SPARSE_EXPORT Result<SparseArrayReader> PrepareText(
      ByteSource& source, std::span<extent_t> metadata,
      std::span<std::byte> scratch, const ArrayIoLimits& limits,
      ArrayIoReport& report, bool require_eof = false);
  /** @brief Reads an ASC binary-v1 Sparse envelope without values allocation.
   * @param source Borrowed source retained through checksum validation.
   * @param metadata Caller extent capacity checked before rank writes.
   * @param scratch Disjoint caller bytes, at least 56 for the fixed envelope.
   * @param limits Copied bounded bytes, shape, count and allocation budgets.
   * @param report Reset before parsing; retains exact consumed-byte progress.
   * @param require_eof Include exact file EOF after CRC in the transaction;
   * EOF probing requires one spare input-budget byte.
   * @return Prepared cursor or envelope/size/type/kind/I/O failure. Same
   * lifetime, concurrency and no-allocation contract as PrepareText applies.
   * @ingroup asc_sparse
   */
  static ASC_SPARSE_EXPORT Result<SparseArrayReader> PrepareBinary(
      ByteSource& source, std::span<extent_t> metadata,
      std::span<std::byte> scratch, const ArrayIoLimits& limits,
      ArrayIoReport& report, bool require_eof = false);
  /** @brief A consumable source cursor cannot be copied. @ingroup asc_sparse */
  SparseArrayReader(const SparseArrayReader&) = delete;
  /** @brief Copy assignment cannot duplicate a cursor. @ingroup asc_sparse */
  SparseArrayReader& operator=(const SparseArrayReader&) = delete;
  /** @brief Transfers the cursor and invalidates the source reader.
   * @param other Cursor whose borrowed lifetimes are retained unchanged.
   * @ingroup asc_sparse
   */
  ASC_SPARSE_EXPORT SparseArrayReader(SparseArrayReader&& other) noexcept;
  /** @brief Assignment cannot discard an unfinished cursor. @ingroup asc_sparse
   */
  SparseArrayReader& operator=(SparseArrayReader&&) = delete;
  /** @brief Performs no reads and releases no borrowed objects.
   * @ingroup asc_sparse
   */
  ~SparseArrayReader() = default;
  /** @brief Returns the exact declared COO/CSR/CSC kind.
   * @return Validated wire representation, without implicit conversion.
   * @ingroup asc_sparse
   */
  [[nodiscard]] SparseArrayKind kind() const noexcept { return kind_; }
  /** @brief Returns exact wire scalar identity, without conversion.
   * @return Scalar code required by typed payload readers.
   * @ingroup asc_sparse
   */
  [[nodiscard]] ArrayScalarCode scalar() const noexcept { return scalar_; }
  /** @brief Borrows validated extents; caller must keep them immutable.
   * @return Rank-sized span borrowing the supplied metadata storage.
   * @ingroup asc_sparse
   */
  [[nodiscard]] std::span<const extent_t> shape() const noexcept {
    return shape_;
  }
  /** @brief Returns stored entries, including explicit zeros.
   * @return Checked stored-value count, independent of capacity.
   * @ingroup asc_sparse
   */
  [[nodiscard]] std::size_t count() const noexcept { return count_; }
  /** @brief Returns true only before payload consumption/failure/move.
   * @return Whether this cursor can begin one payload transaction.
   * @ingroup asc_sparse
   */
  [[nodiscard]] bool ready() const noexcept { return ready_; }

 private:
  /// @cond ASC_INTERNAL
  friend class internal_sparse_io::ReaderAccess;
  /// @endcond
  SparseArrayReader(ByteSource& source, std::span<extent_t> metadata,
                    std::span<std::byte> scratch, ArrayIoLimits limits,
                    ArrayIoReport& report, bool binary, bool require_eof)
      : input_(source, limits, report),
        shape_(metadata),
        scratch_(scratch),
        binary_(binary),
        require_eof_(require_eof) {}
  ASC_SPARSE_EXPORT Status ParseTextHeader();
  ASC_SPARSE_EXPORT Status ParseTextPrefix();
  ASC_SPARSE_EXPORT Status ParseBinaryHeader();
  ASC_SPARSE_EXPORT Status ValidateBinaryPayload(std::uint64_t count,
                                                 std::uint64_t structure_count,
                                                 std::uint64_t payload_bytes,
                                                 std::uint64_t header_bytes);
  ASC_SPARSE_EXPORT Status Trailer();
  ASC_SPARSE_EXPORT Result<index_t> ReadIndex(char separator);
  ASC_SPARSE_EXPORT Status Line(std::string_view text);
  Status Fail(ErrorCode code) {
    ready_ = false;
    structure_read_ = false;
    return input_.Fail(code);
  }
  template <typename Store>
  Status Structure(Store store) {
    if (!ready_) {
      return Fail(ErrorCode::kInvalidState);
    }
    ready_ = false;
    input_.report().section = ArrayIoSection::kPayload;
    Status status =
        kind_ == SparseArrayKind::kCoo ? Coordinates(store) : Compressed(store);
    if (!status.ok()) {
      return status;
    }
    status = binary_ ? Status::Ok() : Line("values");
    if (!status.ok()) {
      return status;
    }
    structure_read_ = true;
    return Status::Ok();
  }
  template <typename Store>
  Status Coordinates(Store& store) {
    std::size_t ordinal = 0;
    Status status;
    status = binary_ ? Status::Ok() : Line("coordinates");
    if (!status.ok()) {
      return status;
    }
    for (std::size_t entry = 0; entry < count_; ++entry) {
      status = binary_ ? Status::Ok() : input_.Expect("(");
      if (!status.ok()) {
        return status;
      }
      for (std::size_t dimension = 0; dimension < shape_.size(); ++dimension) {
        auto value = ReadIndex(dimension + 1 == shape_.size() ? ')' : ',');
        if (!value.ok()) {
          return value.status();
        }
        status = store(ordinal++, *value);
        if (!status.ok()) {
          return Fail(status.code());
        }
      }
      if (!binary_) {
        status = shape_.empty() ? input_.Expect(")") : Status::Ok();
        if (!status.ok()) {
          return status;
        }
        status = input_.EndOfLine();
        if (!status.ok()) {
          return status;
        }
      }
    }
    return Status::Ok();
  }
  template <typename Store>
  Status Compressed(Store& store) {
    std::size_t ordinal = 0;
    Status status;
    const auto outer = static_cast<std::size_t>(
        shape_[kind_ == SparseArrayKind::kCsr ? 0 : 1]);
    status = binary_ ? Status::Ok() : Line("offsets");
    if (!status.ok()) {
      return status;
    }
    for (std::size_t offset = 0; offset <= outer; ++offset) {
      auto value = ReadIndex('\n');
      if (!value.ok()) {
        return value.status();
      }
      status = store(ordinal++, *value);
      if (!status.ok()) {
        return Fail(status.code());
      }
    }
    status = binary_ ? Status::Ok() : Line("indices");
    if (!status.ok()) {
      return status;
    }
    for (std::size_t entry = 0; entry < count_; ++entry) {
      auto value = ReadIndex('\n');
      if (!value.ok()) {
        return value.status();
      }
      status = store(ordinal++, *value);
      if (!status.ok()) {
        return Fail(status.code());
      }
    }
    return Status::Ok();
  }
  template <typename Element>
  Status Values(std::span<Element> values) {
    if (!structure_read_ || values.size() != count_) {
      return Fail(ErrorCode::kInvalidState);
    }
    structure_read_ = false;
    const auto text =
        std::span(reinterpret_cast<char*>(scratch_.data()), scratch_.size());
    for (Element& destination : values) {
      if (binary_) {
        const auto width = internal_array_io::ScalarWidth(scalar_);
        if (width > scratch_.size()) {
          return Fail(ErrorCode::kAllocation);
        }
        auto status = input_.Read(scratch_.first(width));
        if (!status.ok()) {
          return status;
        }
        auto value =
            internal_array_io::DecodeScalar<Element>(scratch_.first(width));
        if (!value.ok()) {
          return Fail(value.status().code());
        }
        destination = *value;
      } else {
        auto token = input_.Line(text, input_.limits().max_token_bytes);
        if (!token.ok()) {
          return token.status();
        }
        auto value = internal_array_io::ParseScalar<Element>(
            *token, input_.limits().max_token_bytes);
        if (!value.ok()) {
          return Fail(value.status().code());
        }
        destination = *value;
      }
      ++input_.report().values_processed;
    }
    return Trailer();
  }
  internal_array_io::Input input_;
  std::span<extent_t> shape_;
  std::span<std::byte> scratch_;
  SparseArrayKind kind_ = SparseArrayKind::kCoo;
  ArrayScalarCode scalar_ = ArrayScalarCode::kU8;
  std::size_t count_ = 0;
  bool binary_ = false;
  bool require_eof_ = false;
  bool ready_ = false;
  bool structure_read_ = false;
};

namespace internal_sparse_io {

class ReaderAccess {
 public:
  static const ArrayIoLimits& Limits(const SparseArrayReader& reader) {
    return reader.input_.limits();
  }
  static std::span<std::byte> Scratch(const SparseArrayReader& reader) {
    return reader.scratch_;
  }
  static Status Fail(SparseArrayReader& reader, ErrorCode code) {
    return reader.Fail(code);
  }
  static void Commit(SparseArrayReader& reader) {
    reader.input_.report().committed = true;
  }
  template <typename Store>
  static Status Structure(SparseArrayReader& reader, Store store) {
    return reader.Structure(store);
  }
  template <typename Element>
  static Status Values(SparseArrayReader& reader, std::span<Element> values) {
    return reader.Values(values);
  }
};

template <typename View>
struct ViewTraits;
template <typename Element, std::size_t Rank>
struct ViewTraits<CoordinateView<Element, Rank>> {
  using Value = std::remove_const_t<Element>;
  static constexpr SparseArrayKind kKind = SparseArrayKind::kCoo;
  static std::array<std::span<const index_t>, 2> Structure(
      const CoordinateView<Element, Rank>& view) {
    return {std::span(view.coordinates(),
                      static_cast<std::size_t>(view.nnz()) * Rank),
            std::span<const index_t>{}};
  }
};
template <typename Element, SparseCompressedFormat Format>
struct ViewTraits<CompressedSparseView<Element, Format>> {
  using Value = std::remove_const_t<Element>;
  static constexpr SparseArrayKind kKind =
      Format == SparseCompressedFormat::kCsr ? SparseArrayKind::kCsr
                                             : SparseArrayKind::kCsc;
  static std::array<std::span<const index_t>, 2> Structure(
      const CompressedSparseView<Element, Format>& view) {
    const auto outer = static_cast<std::size_t>(
        Format == SparseCompressedFormat::kCsr ? view.rows() : view.columns());
    return {
        std::span(view.outer_offsets(), outer + 1),
        std::span(view.inner_indices(), static_cast<std::size_t>(view.nnz()))};
  }
};

template <typename View>
concept SparseIoView = requires { typename ViewTraits<View>::Value; };

template <SparseIoView View>
Status ValidateView(const View& view, std::span<std::byte> scratch,
                    const ArrayIoLimits& limits) {
  using T = typename ViewTraits<View>::Value;
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    if (view.memory_space() != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    if (!view.canonical_structure_trusted()) {
      return Status(ErrorCode::kInvalidState);
    }
    Status status = ValidateShape(ViewTraits<View>::kKind, view.extents(),
                                  static_cast<std::uint64_t>(view.nnz()),
                                  internal_array_io::ScalarCode<T>(), limits);
    if (!status.ok()) {
      return status;
    }
    if (scratch.size() > limits.max_scratch_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    const auto count = static_cast<std::size_t>(view.nnz());
    if (internal_array_format::Overlaps(view.values(), count * sizeof(T),
                                        scratch.data(), scratch.size()) ||
        internal_array_format::Overlaps(&view, sizeof(view), scratch.data(),
                                        scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    for (auto storage : ViewTraits<View>::Structure(view)) {
      if (internal_array_format::Overlaps(storage.data(), storage.size_bytes(),
                                          scratch.data(), scratch.size())) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    return Status::Ok();
  }
}

}  // namespace internal_sparse_io

namespace internal_sparse_io {

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

template <typename Element>
struct Staging {
  std::optional<Buffer> structure_owner;
  std::optional<Buffer> values_owner;
  std::span<index_t> structure;
  std::span<Element> values;
};

template <typename Element>
Result<Staging<Element>> AllocateStaging(SparseArrayReader& reader,
                                         MemoryResource& resource) {
  const auto& limits = ReaderAccess::Limits(reader);
  if (resource.space() != MemorySpace::kHost) {
    return ReaderAccess::Fail(reader, ErrorCode::kMemoryAccess);
  }
  auto structure_count =
      StructureCount(reader.kind(), reader.shape(), reader.count());
  if (!structure_count.ok()) {
    return ReaderAccess::Fail(reader, structure_count.status().code());
  }
  auto structure_bytes =
      internal_array_io::MultiplySize(*structure_count, sizeof(index_t));
  auto value_bytes =
      internal_array_io::MultiplySize(reader.count(), sizeof(Element));
  if (!structure_bytes.ok() || !value_bytes.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  auto decoded = internal_array_io::AddSize(*structure_bytes, *value_bytes);
  if (!decoded.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  auto peak = internal_array_io::MultiplySize(*decoded, std::size_t{2});
  if (!peak.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  const std::size_t staging_requests =
      static_cast<std::size_t>(*structure_bytes != 0) +
      static_cast<std::size_t>(*value_bytes != 0);
  // Existing owners make two COO or three compressed resource requests,
  // including zero-byte requests. No temporary resource wrapper may escape.
  const std::size_t owner_requests =
      reader.kind() == SparseArrayKind::kCoo ? 2 : 3;
  if (*decoded > limits.max_decoded_bytes || *peak > limits.max_staging_bytes ||
      *peak > limits.max_allocation_bytes ||
      staging_requests + owner_requests > limits.max_allocations) {
    return ReaderAccess::Fail(reader, ErrorCode::kAllocation);
  }
  Staging<Element> staging;
  if (*structure_bytes != 0) {
    auto storage =
        Buffer::Allocate(resource, *structure_bytes, alignof(index_t));
    if (!storage.ok()) {
      return ReaderAccess::Fail(reader, storage.status().code());
    }
    auto* data = ::new (storage->data()) index_t[*structure_count];
    staging.structure = std::span(data, *structure_count);
    staging.structure_owner.emplace(std::move(*storage));
  }
  if (*value_bytes != 0) {
    auto storage = Buffer::Allocate(resource, *value_bytes, alignof(Element));
    if (!storage.ok()) {
      return ReaderAccess::Fail(reader, storage.status().code());
    }
    auto* data = ::new (storage->data()) Element[reader.count()];
    staging.values = std::span(data, reader.count());
    staging.values_owner.emplace(std::move(*storage));
  }
  return staging;
}

template <typename Element>
Status ReadStaging(SparseArrayReader& reader, Staging<Element>& staging) {
  Status status = ReaderAccess::Structure(
      reader, [&staging](std::size_t ordinal, index_t value) {
        staging.structure[ordinal] = value;
        return Status::Ok();
      });
  if (!status.ok()) {
    return status;
  }
  status = ValidateStructure(reader.kind(), reader.shape(), reader.count(),
                             staging.structure);
  if (!status.ok()) {
    return ReaderAccess::Fail(reader, status.code());
  }
  return ReaderAccess::Values(reader, staging.values);
}

template <typename Element>
Status ValidateScalar(SparseArrayReader& reader, SparseArrayKind kind) {
  if (!reader.ready()) {
    return ReaderAccess::Fail(reader, ErrorCode::kInvalidState);
  }
  if (reader.kind() != kind) {
    return ReaderAccess::Fail(reader, ErrorCode::kEncoding);
  }
  if constexpr (!internal_array_format::kWireScalar<Element>) {
    return ReaderAccess::Fail(reader, ErrorCode::kUnsupported);
  } else {
    if (reader.scalar() != internal_array_io::ScalarCode<Element>()) {
      return ReaderAccess::Fail(reader, ErrorCode::kEncoding);
    }
  }
  return Status::Ok();
}

template <SparseIoView View>
Status WriteStructure(const View& view, bool binary,
                      const ArrayIoLimits& limits, std::span<std::byte> scratch,
                      internal_array_io::Output& output) {
  Status status;
  const auto structure = ViewTraits<View>::Structure(view);
  if constexpr (ViewTraits<View>::kKind == SparseArrayKind::kCoo) {
    status = binary ? Status::Ok() : output.Text("coordinates\n");
    if (!status.ok()) {
      return status;
    }
    for (std::size_t entry = 0; entry < static_cast<std::size_t>(view.nnz());
         ++entry) {
      status = binary ? Status::Ok() : output.Text("(");
      if (!status.ok()) {
        return status;
      }
      for (std::size_t dimension = 0; dimension < View::kRank; ++dimension) {
        status = WriteIndex(static_cast<std::uint64_t>(
                                structure[0][entry * View::kRank + dimension]),
                            binary, dimension + 1 == View::kRank ? "" : ",",
                            limits, scratch, output);
        if (!status.ok()) {
          return status;
        }
      }
      status = binary ? Status::Ok() : output.Text(")\n");
      if (!status.ok()) {
        return status;
      }
    }
  } else {
    status = binary ? Status::Ok() : output.Text("offsets\n");
    if (!status.ok()) {
      return status;
    }
    for (index_t offset : structure[0]) {
      status = WriteIndex(static_cast<std::uint64_t>(offset), binary, "\n",
                          limits, scratch, output);
      if (!status.ok()) {
        return status;
      }
    }
    status = binary ? Status::Ok() : output.Text("indices\n");
    if (!status.ok()) {
      return status;
    }
    for (index_t index : structure[1]) {
      status = WriteIndex(static_cast<std::uint64_t>(index), binary, "\n",
                          limits, scratch, output);
      if (!status.ok()) {
        return status;
      }
    }
  }
  return binary ? Status::Ok() : output.Text("values\n");
}

template <typename Element>
Status WriteValues(std::span<const Element> values, bool binary,
                   const ArrayIoLimits& limits, std::span<std::byte> scratch,
                   internal_array_io::Output& output, ArrayIoReport& report) {
  Status status;
  for (Element value : values) {
    if (binary) {
      const auto width = internal_array_io::ScalarWidth(
          internal_array_io::ScalarCode<Element>());
      if (scratch.size() < width) {
        return Status(ErrorCode::kAllocation);
      }
      status = internal_array_io::EncodeScalar(value, scratch.first(width));
      if (status.ok()) {
        status = output.Write(scratch.first(width));
      }
    } else {
      const auto text =
          std::span(reinterpret_cast<char*>(scratch.data()), scratch.size());
      constexpr int kPrecision =
          std::same_as<Element, float> ||
                  std::same_as<Element, std::complex<float>>
              ? 9
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
    status = binary ? Status::Ok() : output.Text("\n");
    if (!status.ok()) {
      return status;
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

template <SparseIoView View>
Status Write(const View& view, ByteSink& sink, const ArrayIoLimits& limits,
             std::span<std::byte> scratch, ArrayIoReport& report, bool binary) {
  report = {};
  Status status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  status = ValidateView(view, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  using T = typename ViewTraits<View>::Value;
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  } else {
    internal_array_io::Output output(sink, limits.max_output_bytes, report);
    status = WriteHeader(ViewTraits<View>::kKind, view.extents(),
                         static_cast<std::size_t>(view.nnz()),
                         internal_array_io::ScalarCode<T>(), binary, limits,
                         scratch, output, report);
    if (!status.ok()) {
      return status;
    }
    report.section = ArrayIoSection::kPayload;
    status = WriteStructure(view, binary, limits, scratch, output);
    if (!status.ok()) {
      return status;
    }
    return WriteValues(
        std::span<const T>(view.values(), static_cast<std::size_t>(view.nnz())),
        binary, limits, scratch, output, report);
  }
}

template <SparseIoView View>
Status ValidateInto(const View& view, std::span<const extent_t> metadata,
                    std::span<std::byte> scratch,
                    std::span<typename ViewTraits<View>::Value> staging,
                    const ArrayIoLimits& limits) {
  Status status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  status = ValidateView(view, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  auto staging_bytes = internal_array_io::MultiplySize(
      staging.size(), sizeof(typename ViewTraits<View>::Value));
  auto metadata_bytes =
      internal_array_io::MultiplySize(metadata.size(), sizeof(extent_t));
  if (!staging_bytes.ok() || !metadata_bytes.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  if (staging.size() < static_cast<std::size_t>(view.nnz()) ||
      *staging_bytes > limits.max_staging_bytes ||
      *metadata_bytes > limits.max_scratch_bytes ||
      scratch.size() > limits.max_scratch_bytes - *metadata_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  const std::array<ConstMemoryView, 3> work{
      ConstMemoryView(staging.data(), *staging_bytes, MemorySpace::kHost),
      ConstMemoryView(metadata.data(), *metadata_bytes, MemorySpace::kHost),
      ConstMemoryView(scratch.data(), scratch.size(), MemorySpace::kHost)};
  for (std::size_t i = 0; i < work.size(); ++i) {
    for (std::size_t j = i + 1; j < work.size(); ++j) {
      if (internal_array_format::Overlaps(work[i].data(), work[i].size(),
                                          work[j].data(), work[j].size())) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    if (internal_array_format::Overlaps(
            work[i].data(), work[i].size(), view.values(),
            static_cast<std::size_t>(view.nnz()) *
                sizeof(typename ViewTraits<View>::Value)) ||
        internal_array_format::Overlaps(work[i].data(), work[i].size(), &view,
                                        sizeof(view))) {
      return Status(ErrorCode::kInvalidArgument);
    }
    for (auto storage : ViewTraits<View>::Structure(view)) {
      if (internal_array_format::Overlaps(work[i].data(), work[i].size(),
                                          storage.data(),
                                          storage.size_bytes())) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

}  // namespace internal_sparse_io

/** @brief Loads a prepared COO archive into a new explicit-resource owner.
 * @tparam Element Exact supported wire scalar, without conversion.
 * @tparam ExtentsType Compile-time rank/static extents checked before
 * allocation.
 * @param reader Borrowed one-use prepared COO cursor, with retained metadata.
 * @param resource Host resource outliving the returned owner; all temporary
 * and final buffers use it. Peak staging and cumulative bytes are twice decoded
 * storage; every actual request is preflight-budgeted before allocation,
 * including the owner's two zero-byte requests for an empty COO. Empty staging
 * buffers make no requests. Zero-byte requests still count in max_allocations.
 * @return Complete owner after canonical structure, full values and trailer/
 * checksum/optional EOF validation, or failure releasing all candidate storage.
 * Explicit zeros survive; no sorting, summing, densification or transfer
 * occurs. O(rank*count) work. Shared source/resource concurrency remains
 * caller-owned.
 * @ingroup asc_sparse
 */
template <SparseElement Element, SparseExtents ExtentsType>
Result<CoordinateArray<Element, ExtentsType>> ReadSparseArray(
    SparseArrayReader& reader, MemoryResource& resource) {
  using internal_sparse_io::AllocateStaging;
  using internal_sparse_io::MatchExtents;
  using internal_sparse_io::ReaderAccess;
  using internal_sparse_io::ReadStaging;
  using internal_sparse_io::ValidateScalar;
  Status status = ValidateScalar<Element>(reader, SparseArrayKind::kCoo);
  if (!status.ok()) {
    return status;
  }
  auto extents = MatchExtents<ExtentsType>(
      reader.shape(), std::make_index_sequence<ExtentsType::kRank>{});
  if (!extents.ok()) {
    return ReaderAccess::Fail(reader, extents.status().code());
  }
  auto staging = AllocateStaging<Element>(reader, resource);
  if (!staging.ok()) {
    return staging.status();
  }
  status = ReadStaging(reader, *staging);
  if (!status.ok()) {
    return status;
  }
  auto owner = CoordinateArray<Element, ExtentsType>::Create(
      resource, *extents, staging->structure, staging->values);
  if (!owner.ok()) {
    return ReaderAccess::Fail(reader, owner.status().code());
  }
  ReaderAccess::Commit(reader);
  return std::move(*owner);
}

/** @brief Loads a prepared exact-kind CSR/CSC frame into a new host owner.
 * @tparam Element Exact supported scalar; no implicit conversion.
 * @tparam Format Declared CSR/CSC kind, checked with rank two before
 * allocation.
 * @param reader One-use prepared cursor; report retains consumed-byte progress.
 * @param resource Caller host allocator outliving final storage. All staging
 * and final buffer requests are budgeted; peak/cumulative bytes are twice
 * decoded. All three final-owner requests count in max_allocations, even when
 * indices or values have zero bytes; only nonempty temporary buffers make
 * requests.
 * @return Complete immutable-structure owner or failure with all temporary
 * allocations released. Exact offset/index order and stored zeros survive.
 * O(outer extent + count) work, no allocation outside the resource, Dense edge,
 * synchronization or transfer. Independent resources/cursors may run
 * concurrently.
 * @ingroup asc_sparse
 */
template <SparseElement Element, SparseCompressedFormat Format>
Result<CompressedSparseArray<Element, Format>> ReadSparseArray(
    SparseArrayReader& reader, MemoryResource& resource) {
  using internal_sparse_io::AllocateStaging;
  using internal_sparse_io::ReaderAccess;
  using internal_sparse_io::ReadStaging;
  using internal_sparse_io::ValidateScalar;
  constexpr auto kKind = Format == SparseCompressedFormat::kCsr
                             ? SparseArrayKind::kCsr
                             : SparseArrayKind::kCsc;
  Status status = ValidateScalar<Element>(reader, kKind);
  if (!status.ok()) {
    return status;
  }
  if (reader.shape().size() != 2) {
    return ReaderAccess::Fail(reader, ErrorCode::kShape);
  }
  auto staging = AllocateStaging<Element>(reader, resource);
  if (!staging.ok()) {
    return staging.status();
  }
  status = ReadStaging(reader, *staging);
  if (!status.ok()) {
    return status;
  }
  const auto offsets =
      static_cast<std::size_t>(
          reader.shape()[Format == SparseCompressedFormat::kCsr ? 0 : 1]) +
      1;
  const std::span<const extent_t, 2> shape(reader.shape().data(), 2);
  auto owner = CompressedSparseArray<Element, Format>::Create(
      resource, shape, staging->structure.first(offsets),
      staging->structure.subspan(offsets), staging->values);
  if (!owner.ok()) {
    return ReaderAccess::Fail(reader, owner.status().code());
  }
  ReaderAccess::Commit(reader);
  return std::move(*owner);
}

/** @brief Writes a complete canonical COO/CSR/CSC ASC text-v1 view.
 * @tparam View Finalized CoordinateView or CompressedSparseView, optionally
 * const.
 * @param view Host structure and values, live and immutable throughout the
 * call.
 * @param sink Explicit synchronous sink; partial accepted bytes remain on
 * error.
 * @param limits Independent shape/structure/token/header/output/scratch
 * budgets.
 * @param scratch Disjoint caller bytes; individual tokens, not entire high-rank
 * records, must fit. No hidden allocation, transfer or densification occurs.
 * @param report Reset before checks; actual accepted bytes and complete values.
 * @return Validation/size/encoding/I/O Status, or OK after the complete
 * terminator. O(structure integers + stored values) work; no stored zero is
 * dropped. Callers serialize shared sinks and retain all borrowed lifetimes.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View>
Status WriteSparseArrayText(const View& view, ByteSink& sink,
                            const ArrayIoLimits& limits,
                            std::span<std::byte> scratch,
                            ArrayIoReport& report) {
  return internal_sparse_io::Write(view, sink, limits, scratch, report, false);
}

/** @brief Writes exact Sparse structure and scalar components with binary CRC.
 * @tparam View Finalized COO/CSR/CSC view with exact supported scalar.
 * @param view Host values/structure kept live and immutable; padding not
 * serialized.
 * @param sink Explicit synchronous sink; short writes retry, failed prefixes
 * remain.
 * @param limits Checked shape/count/structure/decoded/output budgets.
 * @param scratch Disjoint caller bytes, at least 56; no allocation occurs.
 * @param report Reset before validation; accepted-byte/value progress on
 * failure.
 * @return OK after CRC or validation/size/I/O failure. Finite bits, signed zero
 * and quiet-NaN components survive on supported IEEE paths; signaling NaNs
 * excluded. O(structure integers + count) work; no Dense dependency, packing,
 * transfer or synchronization. Shared sink/resource concurrency is
 * caller-managed.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View>
Status WriteSparseArrayBinary(const View& view, ByteSink& sink,
                              const ArrayIoLimits& limits,
                              std::span<std::byte> scratch,
                              ArrayIoReport& report) {
  return internal_sparse_io::Write(view, sink, limits, scratch, report, true);
}

/** @brief Transactionally replaces values only, requiring identical structure.
 * @tparam View Mutable finalized COO/CSR/CSC view; structural buffers stay
 * immutable.
 * @tparam Capacity Static/dynamic live typed staging capacity.
 * @param reader Prepared exact-kind/scalar/rank/shape/count cursor, consumed
 * once.
 * @param destination Host finalized target; every parsed
 * coordinate/offset/index must equal its corresponding immutable integer, not
 * merely a checksum/hash.
 * @param staging At least stored count values, disjoint from target, metadata
 * and scratch. All bytes count toward max_staging_bytes. Caller owns live
 * elements.
 * @return OK after full frame/checksum/optional EOF and a prevalidated
 * nonfailing value copy. All target values remain unchanged on every failure.
 * No structural replacement, sorting, allocation, transfer, densification or
 * source rollback. O(structure + count) work. Borrowed buffers/source require
 * exclusive use.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View, std::size_t Capacity>
  requires(!std::is_const_v<typename View::element_type>)
Status ReadSparseArrayInto(
    SparseArrayReader& reader, const View& destination,
    std::span<typename internal_sparse_io::ViewTraits<View>::Value, Capacity>
        staging) {
  using internal_sparse_io::ReaderAccess;
  using internal_sparse_io::ValidateInto;
  using internal_sparse_io::ValidateScalar;
  using internal_sparse_io::ViewTraits;
  using T = typename ViewTraits<View>::Value;
  Status status = ValidateScalar<T>(reader, ViewTraits<View>::kKind);
  if (!status.ok()) {
    return status;
  }
  if (reader.shape().size() != View::kRank ||
      reader.count() != static_cast<std::size_t>(destination.nnz()) ||
      !std::equal(reader.shape().begin(), reader.shape().end(),
                  destination.extents().begin())) {
    return ReaderAccess::Fail(reader, ErrorCode::kShape);
  }
  status =
      ValidateInto(destination, reader.shape(), ReaderAccess::Scratch(reader),
                   std::span<T>(staging), ReaderAccess::Limits(reader));
  if (!status.ok()) {
    return ReaderAccess::Fail(reader, status.code());
  }
  const auto structure = ViewTraits<View>::Structure(destination);
  status = ReaderAccess::Structure(
      reader, [structure](std::size_t ordinal, index_t value) {
        const auto expected = ordinal < structure[0].size()
                                  ? structure[0][ordinal]
                                  : structure[1][ordinal - structure[0].size()];
        return value == expected ? Status::Ok() : Status(ErrorCode::kEncoding);
      });
  if (!status.ok()) {
    return status;
  }
  status =
      ReaderAccess::Values(reader, std::span<T>(staging).first(reader.count()));
  if (!status.ok()) {
    return status;
  }
  if (reader.count() != 0) {
    std::copy_n(staging.data(), reader.count(), destination.values());
  }
  ReaderAccess::Commit(reader);
  return Status::Ok();
}

namespace internal_sparse_io {

template <typename Owner>
struct OwnerTraits;
template <SparseElement Element, SparseExtents ExtentsType>
struct OwnerTraits<CoordinateArray<Element, ExtentsType>> {};
template <SparseElement Element, SparseCompressedFormat Format>
struct OwnerTraits<CompressedSparseArray<Element, Format>> {};
template <typename Owner>
concept SparseIoOwner = requires { sizeof(OwnerTraits<Owner>); };

template <SparseIoOwner Owner>
Result<Owner> ReadOwner(SparseArrayReader& reader, MemoryResource& resource) {
  if constexpr (requires { Owner::kFormat; }) {
    return ReadSparseArray<typename Owner::element_type, Owner::kFormat>(
        reader, resource);
  } else {
    return ReadSparseArray<typename Owner::element_type,
                           typename Owner::extents_type>(reader, resource);
  }
}

template <SparseIoOwner Owner>
Result<Owner> ReadSource(ByteSource& source, MemoryResource& resource,
                         std::span<extent_t> metadata,
                         std::span<std::byte> scratch,
                         const ArrayIoLimits& limits, ArrayIoReport& report,
                         bool binary, bool require_eof) {
  auto reader =
      binary ? SparseArrayReader::PrepareBinary(source, metadata, scratch,
                                                limits, report, require_eof)
             : SparseArrayReader::PrepareText(source, metadata, scratch, limits,
                                              report, require_eof);
  if (!reader.ok()) {
    return reader.status();
  }
  return ReadOwner<Owner>(*reader, resource);
}

template <SparseIoOwner Owner>
Status WriteOwner(const Owner& owner, ByteSink& sink,
                  const ArrayIoLimits& limits, std::span<std::byte> scratch,
                  ArrayIoReport& report, bool binary) {
  report = {};
  if (internal_array_format::Overlaps(&owner, sizeof(owner), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto view = owner.view();
  if (!view.ok()) {
    return view.status();
  }
  return Write(*view, sink, limits, scratch, report, binary);
}

template <SparseIoView View>
Status ReadIntoSource(ByteSource& source, const View& destination,
                      std::span<typename ViewTraits<View>::Value> staging,
                      std::span<extent_t> metadata,
                      std::span<std::byte> scratch, const ArrayIoLimits& limits,
                      ArrayIoReport& report, bool binary, bool require_eof) {
  report = {};
  auto status = ValidateInto(destination, metadata, scratch, staging, limits);
  if (!status.ok()) {
    return status;
  }
  auto reader =
      binary ? SparseArrayReader::PrepareBinary(source, metadata, scratch,
                                                limits, report, require_eof)
             : SparseArrayReader::PrepareText(source, metadata, scratch, limits,
                                              report, require_eof);
  if (!reader.ok()) {
    return reader.status();
  }
  return ReadSparseArrayInto(*reader, destination, staging);
}

}  // namespace internal_sparse_io

/** @brief Prepares and loads one canonical text frame into an explicit owner.
 * @tparam Owner CoordinateArray or CompressedSparseArray specifying exact
 * scalar, rank/static extents and kind; no implicit representation conversion
 * occurs.
 * @param source Borrowed synchronous source, consuming exactly one frame by
 * default.
 * @param resource Host allocator outliving the returned owner.
 * @param metadata Caller extent capacity, disjoint from scratch.
 * @param scratch Bounded caller token storage; no hidden parser allocation.
 * @param limits Copied limits, including all staging/final requests and bytes.
 * @param report Reset progress and publication outcome, retained on failure.
 * @param require_eof Include trailing whitespace and EOF before publication;
 * the EOF probe requires one spare input-budget byte.
 * @return Owner only after canonical structure and complete frame validation;
 * failure releases all resource allocations. Same zero-byte request accounting,
 * no transfer/densification, complexity and lifetime contract as
 * ReadSparseArray. Caller serializes shared sources and resources.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Result<Owner> ReadSparseArrayText(ByteSource& source, MemoryResource& resource,
                                  std::span<extent_t> metadata,
                                  std::span<std::byte> scratch,
                                  const ArrayIoLimits& limits,
                                  ArrayIoReport& report,
                                  bool require_eof = false) {
  return internal_sparse_io::ReadSource<Owner>(
      source, resource, metadata, scratch, limits, report, false, require_eof);
}

/** @brief Prepares and loads one binary frame after exact structure and CRC
 * checks.
 * @tparam Owner Exact scalar/rank/kind CoordinateArray or
 * CompressedSparseArray.
 * @param source Borrowed source, without read-ahead past the current CRC.
 * @param resource Host allocator retained by the resulting owner.
 * @param metadata Caller extent capacity checked before metadata writes.
 * @param scratch Disjoint bounded bytes, at least 56 for the envelope.
 * @param limits Copied limits counting all temporary/final storage requests.
 * @param report Initialized consumed-byte/value progress and commit outcome.
 * @param require_eof Include exact EOF in the transaction; needs a spare byte
 * budget for its probe. False permits adjacent independent frames.
 * @return Fully validated owner, or error releasing all candidates. Bit/zero
 * preservation, zero-byte request counting and O(structure + count) work follow
 * ReadSparseArray; no hidden allocation, transfer or densification. Borrowed
 * objects require exclusive synchronous use. @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Result<Owner> ReadSparseArrayBinary(
    ByteSource& source, MemoryResource& resource, std::span<extent_t> metadata,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    ArrayIoReport& report, bool require_eof = false) {
  return internal_sparse_io::ReadSource<Owner>(
      source, resource, metadata, scratch, limits, report, true, require_eof);
}

/** @brief Writes a finalized Sparse owner's complete ASC text archive.
 * @tparam Owner CoordinateArray or CompressedSparseArray with supported scalar.
 * @param owner Live unmodified host owner; only its const view is traversed.
 * @param sink Caller sink, with checked short writes and nontransactional
 * prefixes.
 * @param limits Independent shape/count/header/output/scratch budgets.
 * @param scratch Caller bytes disjoint from owner object and all its storage.
 * @param report Initialized progress, including accepted bytes on failure.
 * @return Owner/view validation or writer Status; no allocation/transfer
 * occurs. Same canonical ordering, zero preservation, complexity and
 * concurrency as the view overload; no temporary view escapes.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Status WriteSparseArrayText(const Owner& owner, ByteSink& sink,
                            const ArrayIoLimits& limits,
                            std::span<std::byte> scratch,
                            ArrayIoReport& report) {
  return internal_sparse_io::WriteOwner(owner, sink, limits, scratch, report,
                                        false);
}

/** @brief Writes a Sparse owner's field-encoded ASC binary archive and CRC.
 * @tparam Owner Exact-scalar CoordinateArray or CompressedSparseArray.
 * @param owner Retained immutable host owner; capacity/padding are not
 * serialized.
 * @param sink Borrowed synchronous sink; failed writes may retain a prefix.
 * @param limits Checked independent and aggregate size/output bounds.
 * @param scratch Disjoint caller bytes, at least 56; no allocation occurs.
 * @param report Initialized accepted-byte/value progress on success or failure.
 * @return Owner/view or binary-writer Status; quiet-NaN/signed-zero and
 * explicit structure rules follow the view overload, without transfer or
 * densification. O(structure + count) work and caller-managed shared sink
 * access.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Status WriteSparseArrayBinary(const Owner& owner, ByteSink& sink,
                              const ArrayIoLimits& limits,
                              std::span<std::byte> scratch,
                              ArrayIoReport& report) {
  return internal_sparse_io::WriteOwner(owner, sink, limits, scratch, report,
                                        true);
}

/** @brief Reads one text frame into values-only staging, then commits
 * atomically.
 * @tparam View Mutable finalized COO/CSR/CSC view of the exact wire scalar.
 * @tparam Capacity Caller typed staging capacity, static or dynamic.
 * @param source Borrowed synchronous source, without frame read-ahead.
 * @param destination Host values replaced only after exact kind/shape/structure
 * and complete frame validation; finalized offsets/indices are never changed.
 * @param staging Live typed values, at least nnz, disjoint from all other
 * storage.
 * @param metadata Caller extent capacity; aliases are rejected before header
 * writes.
 * @param scratch Disjoint bounded caller token bytes, with no hidden
 * allocation.
 * @param limits Independent/aggregate input, staging and parser budgets.
 * @param report Reset before preflight, preserving progress and commit outcome.
 * @param require_eof Include trailing-whitespace/EOF in the transaction; EOF
 * needs a spare byte of input budget. False consumes exactly one frame.
 * @return OK after a nonfailing value copy, or failure leaving every target
 * byte unchanged, even on header failure. No source rollback, transfer or
 * densification. O(structure + count) work; shared sources/targets require
 * exclusive use.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View, std::size_t Capacity>
  requires(!std::is_const_v<typename View::element_type>)
Status ReadSparseArrayTextInto(
    ByteSource& source, const View& destination,
    std::span<typename internal_sparse_io::ViewTraits<View>::Value, Capacity>
        staging,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report,
    bool require_eof = false) {
  return internal_sparse_io::ReadIntoSource(
      source, destination,
      std::span<typename internal_sparse_io::ViewTraits<View>::Value>(staging),
      metadata, scratch, limits, report, false, require_eof);
}

/** @brief Reads binary Sparse values transactionally with exact structure and
 * CRC.
 * @tparam View Mutable finalized COO/CSR/CSC view with exact wire scalar.
 * @tparam Capacity Static/dynamic typed staging capacity.
 * @param source Borrowed synchronous source; no read-ahead past this CRC.
 * @param destination Host target; every structure integer is compared exactly.
 * @param staging At least nnz live values, disjoint from target and parser
 * buffers.
 * @param metadata Caller rank capacity; target aliases fail before parsing.
 * @param scratch Disjoint bounded caller bytes, at least 56.
 * @param limits Checked shape/structure/input/decoded/staging/scratch budgets.
 * @param report Initialized before validation, retaining consumed bytes on
 * error.
 * @param require_eof Include exact EOF after CRC before commit; needs a spare
 * input-budget byte for the probe. False permits adjacent framed objects.
 * @return OK after full validation and nonfailing value copy, or failure
 * leaving every destination byte unchanged. No allocation, transfer,
 * densification or source rollback. O(structure + count) work; caller
 * serializes shared access.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View, std::size_t Capacity>
  requires(!std::is_const_v<typename View::element_type>)
Status ReadSparseArrayBinaryInto(
    ByteSource& source, const View& destination,
    std::span<typename internal_sparse_io::ViewTraits<View>::Value, Capacity>
        staging,
    std::span<extent_t> metadata, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, ArrayIoReport& report,
    bool require_eof = false) {
  return internal_sparse_io::ReadIntoSource(
      source, destination,
      std::span<typename internal_sparse_io::ViewTraits<View>::Value>(staging),
      metadata, scratch, limits, report, true, require_eof);
}

namespace internal_sparse_io {

template <SparseIoView View>
Status Save(const std::filesystem::path& path, const View& view,
            ArrayFileOverwrite overwrite, const ArrayIoLimits& limits,
            std::span<std::byte> scratch, ArrayIoReport& report, bool binary) {
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

template <SparseIoOwner Owner>
Result<Owner> Load(const std::filesystem::path& path, MemoryResource& resource,
                   std::span<extent_t> metadata, std::span<std::byte> scratch,
                   const ArrayIoLimits& limits, ArrayIoReport& report,
                   bool binary) {
  report = {};
  auto file = File::OpenRead(path);
  if (!file.ok()) {
    return file.status();
  }
  auto owner = ReadSource<Owner>(*file, resource, metadata, scratch, limits,
                                 report, binary, true);
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

}  // namespace internal_sparse_io

/** @brief Creates/truncates a path and saves one complete canonical text file.
 * @tparam View Finalized host COO/CSR/CSC view of an exact supported scalar.
 * @param path Explicit Core File path; caller serializes concurrent path
 * access.
 * @param view Borrowed immutable values and structure, retained during the
 * call.
 * @param overwrite Required explicit destructive create/truncate intent.
 * @param limits Independent output and parser scratch budgets.
 * @param scratch Disjoint caller encoder storage.
 * @param report Accepted-byte progress and any secondary checked-close failure.
 * @return First encoding/write/flush/close failure, or OK. Failure may leave a
 * partial/truncated file; no atomic replacement, durability or race guarantee.
 * O(structure + count), no codec allocation/transfer/densification; Core File's
 * separate handle/path allocation boundary applies. @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View>
Status SaveSparseArrayText(const std::filesystem::path& path, const View& view,
                           ArrayFileOverwrite overwrite,
                           const ArrayIoLimits& limits,
                           std::span<std::byte> scratch,
                           ArrayIoReport& report) {
  return internal_sparse_io::Save(path, view, overwrite, limits, scratch,
                                  report, false);
}

/** @brief Creates/truncates a path and saves a field-encoded binary file and
 * CRC.
 * @tparam View Finalized exact-scalar host COO/CSR/CSC view.
 * @param path Explicit Core File path, requiring caller-owned synchronization.
 * @param view Live immutable structure/values; no capacity or padding is saved.
 * @param overwrite Required create/truncate intent with no default.
 * @param limits Checked independent output/header/shape/scratch budgets.
 * @param scratch Disjoint caller bytes, at least 56 for the envelope.
 * @param report Initialized progress and secondary close failure if any.
 * @return First writer/flush/close failure or success. Files may retain a
 * failed prefix; no atomic replace or durability guarantee. Same bit
 * preservation and O(structure + count) codec work as WriteSparseArrayBinary,
 * without codec allocation/transfer/densification. Core File handle/path
 * allocations are a separately documented boundary. @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoView View>
Status SaveSparseArrayBinary(const std::filesystem::path& path,
                             const View& view, ArrayFileOverwrite overwrite,
                             const ArrayIoLimits& limits,
                             std::span<std::byte> scratch,
                             ArrayIoReport& report) {
  return internal_sparse_io::Save(path, view, overwrite, limits, scratch,
                                  report, true);
}

/** @brief Loads a whole canonical Sparse text file with trailing
 * whitespace/EOF.
 * @tparam Owner Exact scalar/kind/rank CoordinateArray or
 * CompressedSparseArray.
 * @param path Explicit Core File path; caller controls shared-path access.
 * @param resource Host resource outliving the returned owner.
 * @param metadata Caller extent capacity disjoint from scratch.
 * @param scratch Bounded caller token bytes retained through parsing.
 * @param limits Copied resource budgets; EOF requires one spare input-budget
 * byte.
 * @param report Consumed progress, publication and secondary checked-close
 * error.
 * @return Owner only after canonical payload, terminator, trailing whitespace,
 * EOF and checked close succeed; failure releases candidates. Codec resource
 * accounting, zero-byte requests, no transfer/densification and complexity
 * follow ReadSparseArrayText. Core File handle/path allocations are separate;
 * caller retains/synchronizes borrowed resources and buffers.
 * @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Result<Owner> LoadSparseArrayText(const std::filesystem::path& path,
                                  MemoryResource& resource,
                                  std::span<extent_t> metadata,
                                  std::span<std::byte> scratch,
                                  const ArrayIoLimits& limits,
                                  ArrayIoReport& report) {
  return internal_sparse_io::Load<Owner>(path, resource, metadata, scratch,
                                         limits, report, false);
}

/** @brief Loads a whole Sparse binary file, including CRC, exact EOF and close.
 * @tparam Owner Exact scalar/kind/rank CoordinateArray or
 * CompressedSparseArray.
 * @param path Explicit Core File path; no format inference from file extension.
 * @param resource Host allocator retained by the returned owner.
 * @param metadata Caller rank capacity disjoint from scratch.
 * @param scratch Bounded caller bytes, at least 56 for the envelope.
 * @param limits Copied budgets, including one spare input byte for EOF probing.
 * @param report Initialized progress/publication and secondary close
 * diagnostics.
 * @return Complete owner only after payload/CRC/EOF/close; failures release all
 * candidates. Zero-byte request accounting and O(structure + count) work follow
 * ReadSparseArrayBinary. No hidden codec allocation, transfer or densification;
 * Core File's handle/path allocation boundary remains separate. Caller manages
 * shared path/resource concurrency and borrowed lifetimes. @ingroup asc_sparse
 */
template <internal_sparse_io::SparseIoOwner Owner>
Result<Owner> LoadSparseArrayBinary(const std::filesystem::path& path,
                                    MemoryResource& resource,
                                    std::span<extent_t> metadata,
                                    std::span<std::byte> scratch,
                                    const ArrayIoLimits& limits,
                                    ArrayIoReport& report) {
  return internal_sparse_io::Load<Owner>(path, resource, metadata, scratch,
                                         limits, report, true);
}

}  // namespace asc

#endif  // ASC_SPARSE_IO_H_
