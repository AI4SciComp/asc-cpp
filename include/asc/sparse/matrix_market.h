#ifndef ASC_SPARSE_MATRIX_MARKET_H_
#define ASC_SPARSE_MATRIX_MARKET_H_

/** @file
 * @brief Explicit Sparse coordinate Matrix Market interchange and assembly.
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
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/export.h"

namespace asc {

/** @brief Explicit Sparse Matrix Market assembly and work limits.
 *
 * Native ASC archives never use these repair/assembly policies. Repeated source
 * coordinates are grouped in original record order before a single symmetry
 * expansion. Sums use checked destination precision, zeros are dropped last.
 * In-place stable insertion sort uses caller/resource storage and has quadratic
 * worst-case comparisons, bounded by max_sort_comparisons. No hidden
 * allocation, sorting container, transfer, synchronization or Dense dependency
 * is implied.
 * @ingroup asc_sparse
 */
struct SparseMatrixMarketReadOptions {
  DuplicatePolicy duplicate_policy =
      DuplicatePolicy::kReject;  ///< Reject or checked stable sum.
  ExplicitZeroPolicy zero_policy =
      ExplicitZeroPolicy::kKeep;  ///< Keep or drop exact assembled zeros.
  MatrixMarketPatternPolicy pattern_policy = MatrixMarketPatternPolicy::
      kUnspecified;  ///< Explicit unit interpretation for pattern input.
  std::size_t max_sort_comparisons =
      16777216;  ///< Total sort comparisons; zero means zero.
};

/** @brief Explicit Sparse Matrix Market output interpretation.
 *
 * Structured output validates the complete represented matrix. Symmetry
 * compression preserves represented values, not an arbitrary asymmetric stored
 * zero pattern: Keep synthesizes a missing lower representative for an upper
 * stored zero, and skew zero diagonals must be omitted and reported. General
 * output preserves native stored entries subject to the explicit zero policy.
 * Unit pattern output requires every retained scalar to equal exact one.
 * @ingroup asc_sparse
 */
struct SparseMatrixMarketWriteOptions {
  MatrixMarketSymmetry symmetry =
      MatrixMarketSymmetry::kGeneral;  ///< Validated whole-matrix symmetry.
  ExplicitZeroPolicy zero_policy =
      ExplicitZeroPolicy::kKeep;  ///< Retained stored-zero policy.
  MatrixMarketPatternPolicy pattern_policy =
      MatrixMarketPatternPolicy::kUnspecified;  ///< Unit selects value-free
                                                ///< pattern output; unspecified
                                                ///< infers the scalar field.
};

/** @brief Bounded progress, assembly cost and preflight storage requirements.
 *
 * Reset before each operation except an invalid overlap with caller storage,
 * which is rejected without modifying the overlapping report. Requirements
 * are conservative before payload
 * parsing; counts and actual byte progress survive failure. Source/sink
 * rollback is never promised. Committed concerns a fully published/copied
 * destination. Callers retain the report exclusively throughout synchronous
 * reader use.
 * @ingroup asc_sparse
 */
struct SparseMatrixMarketReport {
  ArrayIoReport
      io;  ///< Exact source/sink progress, commit and cleanup outcome.
  std::size_t source_records = 0;  ///< Complete input records consumed.
  std::size_t assembled_entries =
      0;  ///< Final canonical entries after policies.
  std::size_t mirrored_entries = 0;   ///< Newly created off-diagonal partners.
  std::size_t duplicate_records = 0;  ///< Repeated source coordinates grouped.
  std::size_t dropped_zeros = 0;      ///< Assembled entries removed by Drop.
  std::size_t omitted_diagonal_zeros =
      0;  ///< Skew output diagonal zeros omitted by the format.
  std::size_t sort_comparisons = 0;  ///< Actual insertion-sort comparisons.
  std::size_t required_coordinates =
      0;  ///< Caller staging index slots, two per entry.
  std::size_t required_values = 0;  ///< Caller typed staging element capacity.
  std::size_t required_staging_bytes =
      0;  ///< Simultaneously live staging plus candidate bytes.
  std::size_t required_allocations =
      0;  ///< Preflight resource requests, including zero-byte owner requests.
};

/** @brief Caller-owned mutable coordinate/value staging for transactional
 * reads.
 *
 * Coordinates have two index slots per capacity entry; values are already-live
 * typed objects. Both full spans must be disjoint from scratch, each other and
 * all destination storage, the prepared reader and its report. Capacity
 * requirements are reported before payload
 * access. Failure may change staging but never the finalized destination.
 * @tparam Element Supported Sparse wire scalar, including complex components.
 * @ingroup asc_sparse
 */
template <SparseElement Element>
struct SparseMatrixMarketWorkspace {
  std::span<index_t>
      coordinates;  ///< Interleaved zero-based row/column staging.
  std::span<Element>
      values;  ///< Typed scalar staging and in-place sort storage.
};

namespace internal_sparse_matrix_market {
class ReaderAccess;
ASC_SPARSE_EXPORT Status
ValidateOptions(const SparseMatrixMarketReadOptions& options);
ASC_SPARSE_EXPORT Status
ValidateOptions(const SparseMatrixMarketWriteOptions& options);
ASC_SPARSE_EXPORT Status ValidateShape(std::span<const extent_t, 2> shape,
                                       const ArrayIoLimits& limits);
}  // namespace internal_sparse_matrix_market

/** @brief Move-only prepared native Sparse coordinate Matrix Market reader.
 *
 * Copies rank-two metadata; borrows source, scratch and report exclusively
 * until consumption. Header preparation allocates nothing. Array representation
 * and non-matrix objects are rejected; no implicit densification or sibling
 * storage conversion is performed. Host-only payload operations require
 * explicit resource or caller staging. Any failure, successful consumption or
 * move invalidates further payload use of the old cursor; source rollback is
 * absent.
 * @ingroup asc_sparse
 */
class SparseMatrixMarketReader {
 public:
  /** @brief Parses a complete coordinate banner and dimensions without
   * allocation.
   * @param source Borrowed synchronous source; short reads are normal.
   * @param scratch Caller token bytes; comments stream without a whole-line
   * buffer.
   * @param limits Copied byte/shape/resource budgets; zero limits remain zero.
   * @param options Explicit duplicate, zero, pattern and comparison policies.
   * @param report Reset progress and required staging capacities.
   * @param require_eof Validate trailing comments/whitespace and EOF as part of
   * the payload transaction. A false value explicitly selects a framed stream.
   * @return Prepared reader or bounded header/field/size/I/O failure. Input is
   * not rewound, no values are allocated and no destination is modified.
   * @ingroup asc_sparse
   */
  static ASC_SPARSE_EXPORT Result<SparseMatrixMarketReader> Prepare(
      ByteSource& source, std::span<std::byte> scratch,
      const ArrayIoLimits& limits, const SparseMatrixMarketReadOptions& options,
      SparseMatrixMarketReport& report, bool require_eof = true);
  /** @brief A source cursor cannot be copied. @ingroup asc_sparse */
  SparseMatrixMarketReader(const SparseMatrixMarketReader&) = delete;
  /** @brief Copy assignment cannot duplicate a transaction. @ingroup asc_sparse
   */
  SparseMatrixMarketReader& operator=(const SparseMatrixMarketReader&) = delete;
  /** @brief Transfers borrowed state and invalidates the source cursor.
   * @param other Reader whose source/scratch/report lifetimes remain required.
   * @ingroup asc_sparse
   */
  ASC_SPARSE_EXPORT SparseMatrixMarketReader(
      SparseMatrixMarketReader&& other) noexcept;
  /** @brief Assignment cannot discard a pending source transaction.
   * @ingroup asc_sparse
   */
  SparseMatrixMarketReader& operator=(SparseMatrixMarketReader&&) = delete;
  /** @brief No I/O or ownership release of borrowed objects occurs.
   * @ingroup asc_sparse
   */
  ~SparseMatrixMarketReader() = default;
  /** @brief Returns copied rank-two metadata.
   * @return Row and column extents, unchanged through payload processing.
   * @ingroup asc_sparse
   */
  [[nodiscard]] std::span<const extent_t, 2> shape() const { return shape_; }
  /** @brief Returns the declared scalar field.
   * @return Validated integer/real/complex/pattern identity, without coercion.
   * @ingroup asc_sparse
   */
  [[nodiscard]] MatrixMarketField field() const { return field_; }
  /** @brief Returns the declared structured interpretation.
   * @return Validated general/symmetric/skew/Hermitian mode.
   * @ingroup asc_sparse
   */
  [[nodiscard]] MatrixMarketSymmetry symmetry() const { return symmetry_; }
  /** @brief Returns declared source records before grouping and mirroring.
   * @return Exact number of required coordinate input records.
   * @ingroup asc_sparse
   */
  [[nodiscard]] std::size_t records() const { return records_; }
  /** @brief Returns whether payload consumption may begin once.
   * @return False after failure, consumption or move.
   * @ingroup asc_sparse
   */
  [[nodiscard]] bool ready() const { return ready_; }

 private:
  /// @cond ASC_INTERNAL
  friend class internal_sparse_matrix_market::ReaderAccess;
  /// @endcond
  SparseMatrixMarketReader(ByteSource& source, std::span<std::byte> scratch,
                           ArrayIoLimits limits,
                           SparseMatrixMarketReadOptions options,
                           SparseMatrixMarketReport& report, bool require_eof)
      : input_(source, limits, report.io),
        scratch_(scratch),
        options_(options),
        report_(&report),
        require_eof_(require_eof) {}
  ASC_SPARSE_EXPORT Status ParseHeader();
  ASC_SPARSE_EXPORT Status ParseDimensions();
  Status Fail(ErrorCode code) {
    ready_ = false;
    return input_.Fail(code);
  }
  internal_matrix_market::Input input_;
  std::span<std::byte> scratch_;
  SparseMatrixMarketReadOptions options_;
  SparseMatrixMarketReport* report_;
  std::array<extent_t, 2> shape_{};
  MatrixMarketField field_ = MatrixMarketField::kReal;
  MatrixMarketSymmetry symmetry_ = MatrixMarketSymmetry::kGeneral;
  std::size_t records_ = 0;
  std::size_t capacity_ = 0;
  std::size_t final_bound_ = 0;
  bool require_eof_;
  bool ready_ = false;
};

namespace internal_sparse_matrix_market {

class ReaderAccess {
 public:
  static internal_matrix_market::Input& Input(
      SparseMatrixMarketReader& reader) {
    return reader.input_;
  }
  static std::span<std::byte> Scratch(const SparseMatrixMarketReader& reader) {
    return reader.scratch_;
  }
  static const SparseMatrixMarketReadOptions& Options(
      const SparseMatrixMarketReader& reader) {
    return reader.options_;
  }
  static SparseMatrixMarketReport& Report(SparseMatrixMarketReader& reader) {
    return *reader.report_;
  }
  static std::size_t Capacity(const SparseMatrixMarketReader& reader) {
    return reader.capacity_;
  }
  static std::size_t FinalBound(const SparseMatrixMarketReader& reader) {
    return reader.final_bound_;
  }
  static Status Fail(SparseMatrixMarketReader& reader, ErrorCode code) {
    return reader.Fail(code);
  }
  static Status Finish(SparseMatrixMarketReader& reader) {
    reader.ready_ = false;
    reader.report_->io.section = ArrayIoSection::kTrailer;
    if (reader.require_eof_) {
      return reader.input_.Finish(reader.scratch_);
    }
    return Status::Ok();
  }
  static void Commit(SparseMatrixMarketReader& reader) {
    reader.report_->io.committed = true;
    reader.report_->io.section = ArrayIoSection::kComplete;
  }
};

template <typename View>
struct ViewTraits;
template <typename Element>
struct ViewTraits<CoordinateView<Element, 2>> {
  using Value = std::remove_const_t<Element>;
  static constexpr bool kColumn = false;
  static std::array<index_t, 2> Coordinate(
      const CoordinateView<Element, 2>& view, std::size_t entry) {
    return {view.coordinates()[2 * entry], view.coordinates()[2 * entry + 1]};
  }
  static std::array<std::span<const index_t>, 2> Structure(
      const CoordinateView<Element, 2>& view) {
    return {
        std::span(view.coordinates(), static_cast<std::size_t>(view.nnz()) * 2),
        std::span<const index_t>{}};
  }
};
template <typename Element, SparseCompressedFormat Format>
struct ViewTraits<CompressedSparseView<Element, Format>> {
  using Value = std::remove_const_t<Element>;
  static constexpr bool kColumn = Format == SparseCompressedFormat::kCsc;
  static std::array<std::span<const index_t>, 2> Structure(
      const CompressedSparseView<Element, Format>& view) {
    const auto outer =
        static_cast<std::size_t>(kColumn ? view.columns() : view.rows());
    return {
        std::span(view.outer_offsets(), outer + 1),
        std::span(view.inner_indices(), static_cast<std::size_t>(view.nnz()))};
  }
  static std::array<index_t, 2> Coordinate(
      const CompressedSparseView<Element, Format>& view, std::size_t entry) {
    const auto offsets = Structure(view)[0];
    const auto* next =
        std::upper_bound(offsets.data(), offsets.data() + offsets.size(),
                         static_cast<nnz_t>(entry));
    const auto outer = static_cast<index_t>(next - offsets.data() - 1);
    const auto inner = view.inner_indices()[entry];
    return kColumn ? std::array{inner, outer} : std::array{outer, inner};
  }
};

template <typename View>
concept MatrixView = requires { typename ViewTraits<View>::Value; };

template <MatrixView View>
Status ValidateView(const View& view, std::span<std::byte> scratch,
                    const ArrayIoLimits& limits) {
  using T = typename ViewTraits<View>::Value;
  if constexpr (!internal_array_format::kWireScalar<T>) {
    return Status(ErrorCode::kUnsupported);
  }
  auto status = internal_array_io::ValidateLimits(limits);
  if (!status.ok()) {
    return status;
  }
  if (view.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  if (!view.canonical_structure_trusted()) {
    return Status(ErrorCode::kInvalidState);
  }
  const std::span<const extent_t, 2> shape(view.extents().data(), 2);
  status = ValidateShape(shape, limits);
  if (!status.ok()) {
    return status;
  }
  if (static_cast<std::uint64_t>(view.nnz()) > limits.max_stored_elements ||
      scratch.size() > limits.max_scratch_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  const auto values = internal_array_io::MultiplySize(
      static_cast<std::size_t>(view.nnz()), sizeof(T));
  if (!values.ok()) {
    return values.status();
  }
  std::size_t structure_bytes = 0;
  for (const auto storage : ViewTraits<View>::Structure(view)) {
    const auto bytes =
        internal_array_io::MultiplySize(storage.size(), sizeof(index_t));
    if (!bytes.ok()) {
      return bytes.status();
    }
    const auto sum = internal_array_io::AddSize(structure_bytes, *bytes);
    if (!sum.ok()) {
      return sum.status();
    }
    structure_bytes = *sum;
    if (internal_array_format::Overlaps(storage.data(), *bytes, scratch.data(),
                                        scratch.size())) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  const auto decoded = internal_array_io::AddSize(structure_bytes, *values);
  if (!decoded.ok()) {
    return decoded.status();
  }
  if (structure_bytes > limits.max_structure_bytes ||
      *decoded > limits.max_decoded_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  if (internal_array_format::Overlaps(view.values(), *values, scratch.data(),
                                      scratch.size()) ||
      internal_array_format::Overlaps(&view, sizeof(view), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

inline bool Less(std::array<index_t, 2> left, std::array<index_t, 2> right,
                 bool column) {
  const std::size_t outer = column ? 1 : 0;
  return left[outer] < right[outer] ||
         (left[outer] == right[outer] && left[1 - outer] < right[1 - outer]);
}

template <typename T>
std::array<index_t, 2> Coordinate(SparseMatrixMarketWorkspace<T> work,
                                  std::size_t entry) {
  return {work.coordinates[2 * entry], work.coordinates[2 * entry + 1]};
}

template <typename T>
void Store(SparseMatrixMarketWorkspace<T> work, std::size_t entry,
           std::array<index_t, 2> coordinate, T value) {
  work.coordinates[2 * entry] = coordinate[0];
  work.coordinates[2 * entry + 1] = coordinate[1];
  work.values[entry] = value;
}

template <typename T>
Status Sort(SparseMatrixMarketReader& reader,
            SparseMatrixMarketWorkspace<T> work, std::size_t count,
            bool column) {
  auto& report = ReaderAccess::Report(reader);
  for (std::size_t i = 1; i < count; ++i) {
    const auto coordinate = Coordinate(work, i);
    const T value = work.values[i];
    std::size_t j = i;
    while (j > 0) {
      if (report.sort_comparisons ==
          ReaderAccess::Options(reader).max_sort_comparisons) {
        return ReaderAccess::Fail(reader, ErrorCode::kAllocation);
      }
      ++report.sort_comparisons;
      const auto previous = Coordinate(work, j - 1);
      if (!Less(coordinate, previous, column)) {
        break;
      }
      Store(work, j, previous, work.values[j - 1]);
      --j;
    }
    Store(work, j, coordinate, value);
  }
  return Status::Ok();
}

template <typename T>
Status ValidateScalar(SparseMatrixMarketReader& reader) {
  if (!reader.ready()) {
    return ReaderAccess::Fail(reader, ErrorCode::kInvalidState);
  }
  const auto status = internal_matrix_market::ValidateField<T>(
      reader.field(), ReaderAccess::Options(reader).pattern_policy);
  return status.ok() ? status : ReaderAccess::Fail(reader, status.code());
}

template <typename T>
Status ReadRecords(SparseMatrixMarketReader& reader,
                   SparseMatrixMarketWorkspace<T> work) {
  auto& input = ReaderAccess::Input(reader);
  auto& report = ReaderAccess::Report(reader);
  const auto& options = ReaderAccess::Options(reader);
  std::size_t components =
      reader.field() == MatrixMarketField::kComplex ? 2 : 1;
  if (reader.field() == MatrixMarketField::kPattern) {
    components = 0;
  }
  for (std::size_t i = 0; i < reader.records(); ++i) {
    const auto record = input.NextRecord(ReaderAccess::Scratch(reader));
    if (!record.ok()) {
      return ReaderAccess::Fail(reader, record.status().code());
    }
    if (record->count != 2 + components) {
      return ReaderAccess::Fail(reader, ErrorCode::kEncoding);
    }
    std::array<index_t, 2> coordinate{};
    for (std::size_t dimension = 0; dimension < 2; ++dimension) {
      const auto index = internal_matrix_market::ParseUnsigned(
          record->fields[dimension], input.limits().max_token_bytes);
      if (!index.ok()) {
        return ReaderAccess::Fail(reader, index.status().code());
      }
      if (*index == 0 ||
          *index > static_cast<std::uint64_t>(reader.shape()[dimension])) {
        return ReaderAccess::Fail(reader, ErrorCode::kIndex);
      }
      coordinate[dimension] = static_cast<index_t>(*index - 1);
    }
    if ((reader.symmetry() != MatrixMarketSymmetry::kGeneral &&
         coordinate[0] < coordinate[1]) ||
        (reader.symmetry() == MatrixMarketSymmetry::kSkewSymmetric &&
         coordinate[0] == coordinate[1])) {
      return ReaderAccess::Fail(reader, ErrorCode::kEncoding);
    }
    const auto value = internal_matrix_market::ParseValue<T>(
        reader.field(), record->tokens().subspan(2), options.pattern_policy,
        input.limits().max_token_bytes);
    if (!value.ok()) {
      return ReaderAccess::Fail(reader, value.status().code());
    }
    if constexpr (internal_array_format::kComplex<T>) {
      if (reader.symmetry() == MatrixMarketSymmetry::kHermitian &&
          coordinate[0] == coordinate[1] && value->imag() != 0) {
        return ReaderAccess::Fail(reader, ErrorCode::kEncoding);
      }
    }
    Store(work, i, coordinate, *value);
    ++report.source_records;
    ++report.io.values_processed;
  }
  return Status::Ok();
}

template <typename T>
Result<std::size_t> Assemble(SparseMatrixMarketReader& reader,
                             SparseMatrixMarketWorkspace<T> work, bool column) {
  auto& report = ReaderAccess::Report(reader);
  const auto& options = ReaderAccess::Options(reader);
  auto status = ReadRecords(reader, work);
  if (status.ok()) {
    status = Sort(reader, work, reader.records(), false);
  }
  if (!status.ok()) {
    return status;
  }
  std::size_t grouped = 0;
  for (std::size_t i = 0; i < reader.records(); ++i) {
    const auto coordinate = Coordinate(work, i);
    if (grouped != 0 && Coordinate(work, grouped - 1) == coordinate) {
      ++report.duplicate_records;
      if (options.duplicate_policy == DuplicatePolicy::kReject) {
        return ReaderAccess::Fail(reader, ErrorCode::kInvalidArgument);
      }
      const auto sum = internal_matrix_market::CheckedSum(
          work.values[grouped - 1], work.values[i]);
      if (!sum.ok()) {
        return ReaderAccess::Fail(reader, sum.status().code());
      }
      work.values[grouped - 1] = *sum;
    } else {
      Store(work, grouped++, coordinate, work.values[i]);
    }
  }
  std::size_t expanded = grouped;
  if (reader.symmetry() != MatrixMarketSymmetry::kGeneral) {
    for (std::size_t i = 0; i < grouped; ++i) {
      const auto coordinate = Coordinate(work, i);
      if (coordinate[0] == coordinate[1]) {
        continue;
      }
      T mirror = work.values[i];
      if (reader.symmetry() == MatrixMarketSymmetry::kSkewSymmetric) {
        const auto negated = internal_matrix_market::CheckedNegate(mirror);
        if (!negated.ok()) {
          return ReaderAccess::Fail(reader, negated.status().code());
        }
        mirror = *negated;
      } else if (reader.symmetry() == MatrixMarketSymmetry::kHermitian) {
        mirror = internal_matrix_market::Conjugate(mirror);
      }
      Store(work, expanded++, {coordinate[1], coordinate[0]}, mirror);
      ++report.mirrored_entries;
    }
  }
  std::size_t retained = 0;
  for (std::size_t i = 0; i < expanded; ++i) {
    if (options.zero_policy == ExplicitZeroPolicy::kDrop &&
        internal_matrix_market::IsZero(work.values[i])) {
      ++report.dropped_zeros;
      continue;
    }
    Store(work, retained++, Coordinate(work, i), work.values[i]);
  }
  if (column || report.mirrored_entries != 0) {
    status = Sort(reader, work, retained, column);
    if (!status.ok()) {
      return status;
    }
  }
  report.assembled_entries = retained;
  status = ReaderAccess::Finish(reader);
  if (!status.ok()) {
    return status;
  }
  return retained;
}

template <typename T>
struct Staging {
  std::optional<Buffer> coordinates_owner;
  std::optional<Buffer> values_owner;
  std::optional<Buffer> offsets_owner;
  SparseMatrixMarketWorkspace<T> work;
  std::span<nnz_t> offsets;
};

struct AllocationPlan {
  std::size_t coordinate_bytes;
  std::size_t value_bytes;
  std::size_t offset_bytes;
};

template <typename T>
Result<AllocationPlan> PlanAllocation(SparseMatrixMarketReader& reader,
                                      bool compressed, bool column) {
  using internal_array_io::AddSize;
  using internal_array_io::MultiplySize;
  const auto capacity = ReaderAccess::Capacity(reader);
  const auto bound = ReaderAccess::FinalBound(reader);
  const auto coordinate_bytes = MultiplySize(capacity, 2 * sizeof(index_t));
  const auto value_bytes = MultiplySize(capacity, sizeof(T));
  const auto offset_count =
      compressed
          ? AddSize(static_cast<std::size_t>(reader.shape()[column ? 1 : 0]),
                    std::size_t{1})
          : Result<std::size_t>(std::size_t{0});
  if (!coordinate_bytes.ok() || !value_bytes.ok() || !offset_count.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  const auto offset_bytes = MultiplySize(*offset_count, sizeof(nnz_t));
  const auto final_indices =
      MultiplySize(bound, (compressed ? 1 : 2) * sizeof(index_t));
  const auto final_values = MultiplySize(bound, sizeof(T));
  if (!offset_bytes.ok() || !final_indices.ok() || !final_values.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  const auto staging_structure = AddSize(*coordinate_bytes, *offset_bytes);
  const auto final_structure = AddSize(*final_indices, *offset_bytes);
  if (!staging_structure.ok() || !final_structure.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  const auto stage = AddSize(*staging_structure, *value_bytes);
  const auto final = AddSize(*final_structure, *final_values);
  if (!stage.ok() || !final.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  const auto peak = AddSize(*stage, *final);
  if (!peak.ok()) {
    return ReaderAccess::Fail(reader, ErrorCode::kOverflow);
  }
  auto& report = ReaderAccess::Report(reader);
  report.required_staging_bytes = *peak;
  report.required_allocations =
      (compressed ? 3 : 2) + static_cast<std::size_t>(*coordinate_bytes != 0) +
      static_cast<std::size_t>(*value_bytes != 0) +
      static_cast<std::size_t>(*offset_bytes != 0);
  const auto& limits = ReaderAccess::Input(reader).limits();
  if (*staging_structure > limits.max_structure_bytes ||
      *final_structure > limits.max_structure_bytes ||
      *final > limits.max_decoded_bytes || *peak > limits.max_staging_bytes ||
      *peak > limits.max_allocation_bytes ||
      report.required_allocations > limits.max_allocations) {
    return ReaderAccess::Fail(reader, ErrorCode::kAllocation);
  }
  return AllocationPlan{*coordinate_bytes, *value_bytes, *offset_bytes};
}

template <typename T>
Result<Staging<T>> Allocate(SparseMatrixMarketReader& reader,
                            MemoryResource& resource, bool compressed,
                            bool column) {
  if (resource.space() != MemorySpace::kHost) {
    return ReaderAccess::Fail(reader, ErrorCode::kMemoryAccess);
  }
  const auto plan = PlanAllocation<T>(reader, compressed, column);
  if (!plan.ok()) {
    return plan.status();
  }
  const auto capacity = ReaderAccess::Capacity(reader);
  Staging<T> staging;
  if (plan->coordinate_bytes != 0) {
    auto buffer =
        Buffer::Allocate(resource, plan->coordinate_bytes, alignof(index_t));
    if (!buffer.ok()) {
      return ReaderAccess::Fail(reader, buffer.status().code());
    }
    staging.coordinates_owner.emplace(std::move(*buffer));
    staging.work.coordinates = {
        static_cast<index_t*>(staging.coordinates_owner->data()), capacity * 2};
  }
  if (plan->value_bytes != 0) {
    auto buffer = Buffer::Allocate(resource, plan->value_bytes, alignof(T));
    if (!buffer.ok()) {
      return ReaderAccess::Fail(reader, buffer.status().code());
    }
    staging.values_owner.emplace(std::move(*buffer));
    auto* values = static_cast<T*>(staging.values_owner->data());
    if constexpr (internal_array_format::kComplex<T>) {
      values = ::new (static_cast<void*>(values)) T[capacity];
    }
    staging.work.values = {values, capacity};
  }
  if (plan->offset_bytes != 0) {
    auto buffer =
        Buffer::Allocate(resource, plan->offset_bytes, alignof(nnz_t));
    if (!buffer.ok()) {
      return ReaderAccess::Fail(reader, buffer.status().code());
    }
    staging.offsets_owner.emplace(std::move(*buffer));
    staging.offsets = {static_cast<nnz_t*>(staging.offsets_owner->data()),
                       plan->offset_bytes / sizeof(nnz_t)};
  }
  return staging;
}

template <typename ExtentsType>
Result<ExtentsType> MatchExtents(std::span<const extent_t, 2> shape) {
  constexpr std::array<extent_t, 2> kStatic{
      ExtentsType::template static_extent<0>(),
      ExtentsType::template static_extent<1>()};
  std::array<extent_t, ExtentsType::kDynamicRank> dynamic{};
  std::size_t next = 0;
  for (std::size_t i = 0; i < 2; ++i) {
    if (kStatic[i] == kDynamicExtent) {
      dynamic[next++] = shape[i];
    } else if (kStatic[i] != shape[i]) {
      return Status(ErrorCode::kShape);
    }
  }
  return ExtentsType::Create(dynamic);
}

}  // namespace internal_sparse_matrix_market

/** @brief Assembles a prepared coordinate Matrix Market object into host COO.
 * @tparam Element Wire scalar with explicit checked field conversion.
 * @tparam ExtentsType Rank-two static/dynamic extents, checked before
 * allocation.
 * @param reader One-use prepared transaction; retains exact failure progress.
 * @param resource Caller host resource, outliving the resulting owner.
 * @return Canonical owner after complete payload/selected EOF validation, or
 * bounded failure releasing every staging/candidate buffer. All requests,
 * including two zero-byte final-owner requests, count against the explicit
 * allocation budget. Simultaneous staging plus candidate bytes are preflighted.
 * Complex staging starts typed lifetimes by nonallocating default construction.
 * No allocation outside the resource, transfer or densification occurs.
 * Sorting is explicitly comparison-budgeted quadratic stable insertion sort;
 * coordinate grouping and symmetry expansion retain the selected policies.
 * @ingroup asc_sparse
 */
template <SparseElement Element, SparseExtents ExtentsType>
  requires(ExtentsType::kRank == 2)
Result<CoordinateArray<Element, ExtentsType>> ReadSparseMatrixMarket(
    SparseMatrixMarketReader& reader, MemoryResource& resource) {
  using internal_sparse_matrix_market::Allocate;
  using internal_sparse_matrix_market::Assemble;
  using internal_sparse_matrix_market::MatchExtents;
  using internal_sparse_matrix_market::ReaderAccess;
  using internal_sparse_matrix_market::ValidateScalar;
  auto status = ValidateScalar<Element>(reader);
  if (!status.ok()) {
    return status;
  }
  const auto extents = MatchExtents<ExtentsType>(reader.shape());
  if (!extents.ok()) {
    return ReaderAccess::Fail(reader, extents.status().code());
  }
  auto staging = Allocate<Element>(reader, resource, false, false);
  if (!staging.ok()) {
    return staging.status();
  }
  const auto count = Assemble(reader, staging->work, false);
  if (!count.ok()) {
    return count.status();
  }
  auto owner = CoordinateArray<Element, ExtentsType>::Create(
      resource, *extents, staging->work.coordinates.first(2 * *count),
      staging->work.values.first(*count));
  if (!owner.ok()) {
    return ReaderAccess::Fail(reader, owner.status().code());
  }
  ReaderAccess::Commit(reader);
  return std::move(*owner);
}

/** @brief Assembles a prepared coordinate Matrix Market object into host
 * CSR/CSC.
 * @tparam Element Checked destination wire scalar, including complex.
 * @tparam Format Explicit final compressed traversal; no automatic selection.
 * @param reader One-use prepared cursor retaining exact failure progress.
 * @param resource Explicit host resource outliving final ownership.
 * @return Canonical compressed owner only after payload/selected EOF
 * validation, or failure with all staging and candidate storage released. Three
 * final-owner resource requests count even for empty buffers. Explicit budgets
 * include coordinate/value/offset staging and simultaneous final buffers.
 * Assembly is bounded quadratic stable sorting plus O(outer extent) offset
 * construction, with no hidden allocation, conversion through Dense, transfer
 * or sync.
 * @ingroup asc_sparse
 */
template <SparseElement Element, SparseCompressedFormat Format>
Result<CompressedSparseArray<Element, Format>> ReadSparseMatrixMarket(
    SparseMatrixMarketReader& reader, MemoryResource& resource) {
  using internal_sparse_matrix_market::Allocate;
  using internal_sparse_matrix_market::Assemble;
  using internal_sparse_matrix_market::Coordinate;
  using internal_sparse_matrix_market::ReaderAccess;
  using internal_sparse_matrix_market::ValidateScalar;
  constexpr bool kColumn = Format == SparseCompressedFormat::kCsc;
  auto status = ValidateScalar<Element>(reader);
  if (!status.ok()) {
    return status;
  }
  auto staging = Allocate<Element>(reader, resource, true, kColumn);
  if (!staging.ok()) {
    return staging.status();
  }
  const auto count = Assemble(reader, staging->work, kColumn);
  if (!count.ok()) {
    return count.status();
  }
  std::size_t entry = 0;
  for (std::size_t outer = 0; outer < staging->offsets.size(); ++outer) {
    while (entry < *count && Coordinate(staging->work, entry)[kColumn ? 1 : 0] <
                                 static_cast<index_t>(outer)) {
      ++entry;
    }
    staging->offsets[outer] = static_cast<nnz_t>(entry);
  }
  for (std::size_t i = 0; i < *count; ++i) {
    // Compact after offset construction; destination i never overwrites a
    // future interleaved source at 2*i or 2*i+1.
    staging->work.coordinates[i] =
        Coordinate(staging->work, i)[kColumn ? 0 : 1];
  }
  auto owner = CompressedSparseArray<Element, Format>::Create(
      resource, reader.shape(), staging->offsets,
      staging->work.coordinates.first(*count),
      staging->work.values.first(*count));
  if (!owner.ok()) {
    return ReaderAccess::Fail(reader, owner.status().code());
  }
  ReaderAccess::Commit(reader);
  return std::move(*owner);
}

namespace internal_sparse_matrix_market {

template <typename T>
Status ValidateStateAliases(const void* state, std::size_t state_bytes,
                            std::span<std::byte> scratch,
                            SparseMatrixMarketWorkspace<T> work) {
  const auto coordinates =
      internal_array_io::MultiplySize(work.coordinates.size(), sizeof(index_t));
  const auto values =
      internal_array_io::MultiplySize(work.values.size(), sizeof(T));
  if (!coordinates.ok() || !values.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  if (internal_array_format::Overlaps(state, state_bytes, scratch.data(),
                                      scratch.size()) ||
      internal_array_format::Overlaps(state, state_bytes,
                                      work.coordinates.data(), *coordinates) ||
      internal_array_format::Overlaps(state, state_bytes, work.values.data(),
                                      *values)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return Status::Ok();
}

template <MatrixView View>
Status ValidateViewState(const View& view, const void* state,
                         std::size_t state_bytes) {
  using T = typename ViewTraits<View>::Value;
  const auto bytes = internal_array_io::MultiplySize(
      static_cast<std::size_t>(view.nnz()), sizeof(T));
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (internal_array_format::Overlaps(state, state_bytes, &view,
                                      sizeof(view)) ||
      internal_array_format::Overlaps(state, state_bytes, view.values(),
                                      *bytes)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (const auto storage : ViewTraits<View>::Structure(view)) {
    const auto size =
        internal_array_io::MultiplySize(storage.size(), sizeof(index_t));
    if (!size.ok()) {
      return size.status();
    }
    if (internal_array_format::Overlaps(state, state_bytes, storage.data(),
                                        *size)) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}

template <MatrixView View>
Status ResetReport(const View& view, std::span<std::byte> scratch,
                   SparseMatrixMarketReport& report) {
  auto status = ValidateStateAliases(
      &report, sizeof(report), scratch,
      SparseMatrixMarketWorkspace<typename ViewTraits<View>::Value>{});
  if (status.ok()) {
    status = ValidateViewState(view, &report, sizeof(report));
  }
  if (status.ok()) {
    report = {};
  }
  return status;
}

template <MatrixView View>
Status ValidateInto(
    const View& view, std::span<std::byte> scratch,
    SparseMatrixMarketWorkspace<typename ViewTraits<View>::Value> work,
    const ArrayIoLimits& limits) {
  auto status = ValidateView(view, scratch, limits);
  if (!status.ok()) {
    return status;
  }
  using T = typename ViewTraits<View>::Value;
  const auto coordinate_bytes =
      internal_array_io::MultiplySize(work.coordinates.size(), sizeof(index_t));
  const auto value_bytes =
      internal_array_io::MultiplySize(work.values.size(), sizeof(T));
  if (!coordinate_bytes.ok() || !value_bytes.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto total =
      internal_array_io::AddSize(*coordinate_bytes, *value_bytes);
  if (!total.ok()) {
    return total.status();
  }
  if (*total > limits.max_staging_bytes ||
      *coordinate_bytes > limits.max_structure_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  const std::array<ConstMemoryView, 3> buffers{
      ConstMemoryView(work.coordinates.data(), *coordinate_bytes,
                      MemorySpace::kHost),
      ConstMemoryView(work.values.data(), *value_bytes, MemorySpace::kHost),
      ConstMemoryView(scratch.data(), scratch.size(), MemorySpace::kHost)};
  for (std::size_t i = 0; i < buffers.size(); ++i) {
    for (std::size_t j = i + 1; j < buffers.size(); ++j) {
      if (internal_array_format::Overlaps(buffers[i].data(), buffers[i].size(),
                                          buffers[j].data(),
                                          buffers[j].size())) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
    if (internal_array_format::Overlaps(
            buffers[i].data(), buffers[i].size(), view.values(),
            static_cast<std::size_t>(view.nnz()) * sizeof(T)) ||
        internal_array_format::Overlaps(buffers[i].data(), buffers[i].size(),
                                        &view, sizeof(view))) {
      return Status(ErrorCode::kInvalidArgument);
    }
    for (const auto storage : ViewTraits<View>::Structure(view)) {
      if (internal_array_format::Overlaps(buffers[i].data(), buffers[i].size(),
                                          storage.data(),
                                          storage.size_bytes())) {
        return Status(ErrorCode::kInvalidArgument);
      }
    }
  }
  return Status::Ok();
}

template <typename Owner>
struct OwnerTraits;
template <typename T, typename Shape>
  requires(Shape::kRank == 2)
struct OwnerTraits<CoordinateArray<T, Shape>> {
  static Result<CoordinateArray<T, Shape>> Read(
      SparseMatrixMarketReader& reader, MemoryResource& resource) {
    return ReadSparseMatrixMarket<T, Shape>(reader, resource);
  }
};
template <typename T, SparseCompressedFormat Format>
struct OwnerTraits<CompressedSparseArray<T, Format>> {
  static Result<CompressedSparseArray<T, Format>> Read(
      SparseMatrixMarketReader& reader, MemoryResource& resource) {
    return ReadSparseMatrixMarket<T, Format>(reader, resource);
  }
};
template <typename Owner>
concept MatrixOwner =
    requires(SparseMatrixMarketReader& reader, MemoryResource& resource) {
      {
        OwnerTraits<Owner>::Read(reader, resource)
      } -> std::same_as<Result<Owner>>;
    };

}  // namespace internal_sparse_matrix_market

/** @brief Transactionally copies assembled values into an identical finalized
 * Sparse structure, without changing its coordinates, offsets or indices.
 * @tparam View Mutable finalized rank-two COO/CSR/CSC host view.
 * @param reader One-use prepared cursor; complete input validation precedes
 * copy.
 * @param destination Existing canonical structure and live scalar storage.
 * @param workspace Explicit disjoint typed values and paired-coordinate
 * staging.
 * @return OK only if shape, final count and every canonical coordinate exactly
 * match. Any header/payload/policy/EOF/structure failure leaves all destination
 * values unchanged; staging and source progress may change. Even a
 * value-equivalent stored-zero pattern difference is rejected. No allocation,
 * transfer, densification or synchronization is performed. Assembly comparison
 * limits and borrowed lifetime/exclusive-access rules of the reader apply.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixView View>
  requires(!std::is_const_v<typename View::element_type>)
Status ReadSparseMatrixMarketInto(
    SparseMatrixMarketReader& reader, const View& destination,
    SparseMatrixMarketWorkspace<
        typename internal_sparse_matrix_market::ViewTraits<View>::Value>
        workspace) {
  using internal_sparse_matrix_market::ReaderAccess;
  using Traits = internal_sparse_matrix_market::ViewTraits<View>;
  auto& report = ReaderAccess::Report(reader);
  auto status = internal_sparse_matrix_market::ValidateStateAliases(
      &reader, sizeof(reader), ReaderAccess::Scratch(reader), workspace);
  if (status.ok()) {
    status = internal_sparse_matrix_market::ValidateStateAliases(
        &report, sizeof(report), ReaderAccess::Scratch(reader), workspace);
  }
  if (status.ok()) {
    status = internal_sparse_matrix_market::ValidateViewState(
        destination, &reader, sizeof(reader));
  }
  if (status.ok()) {
    status = internal_sparse_matrix_market::ValidateViewState(
        destination, &report, sizeof(report));
  }
  if (!status.ok()) {
    return ReaderAccess::Fail(reader, status.code());
  }
  status =
      internal_sparse_matrix_market::ValidateScalar<typename Traits::Value>(
          reader);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_matrix_market::ValidateInto(
      destination, ReaderAccess::Scratch(reader), workspace,
      ReaderAccess::Input(reader).limits());
  if (!status.ok()) {
    return ReaderAccess::Fail(reader, status.code());
  }
  if (!std::equal(reader.shape().begin(), reader.shape().end(),
                  destination.extents().begin())) {
    return ReaderAccess::Fail(reader, ErrorCode::kShape);
  }
  if (workspace.coordinates.size() < report.required_coordinates ||
      workspace.values.size() < report.required_values) {
    return ReaderAccess::Fail(reader, ErrorCode::kAllocation);
  }
  report.required_staging_bytes =
      workspace.coordinates.size_bytes() + workspace.values.size_bytes();
  const auto count = internal_sparse_matrix_market::Assemble(reader, workspace,
                                                             Traits::kColumn);
  if (!count.ok()) {
    return count.status();
  }
  if (*count != static_cast<std::size_t>(destination.nnz())) {
    return ReaderAccess::Fail(reader, ErrorCode::kShape);
  }
  for (std::size_t i = 0; i < *count; ++i) {
    if (internal_sparse_matrix_market::Coordinate(workspace, i) !=
        Traits::Coordinate(destination, i)) {
      return ReaderAccess::Fail(reader, ErrorCode::kShape);
    }
  }
  std::copy_n(workspace.values.data(), *count, destination.values());
  ReaderAccess::Commit(reader);
  return Status::Ok();
}

/** @brief Prepares and assembles one Sparse coordinate Matrix Market source.
 * @tparam Owner Explicit rank-two COO/CSR/CSC owner and destination scalar.
 * @param source Borrowed synchronous byte source, not rewound on failure.
 * @param resource Explicit host allocator outliving final ownership.
 * @param scratch Disjoint caller token scratch, no whole-line allocation.
 * @param limits Shape/byte/resource limits including simultaneous candidate
 * peak.
 * @param options Explicit duplicate/zero/pattern and comparison policies.
 * @param report Reset progress, assembly counters and storage requirements.
 * @return Fully validated owner or bounded failure; complete trailing EOF is
 * required. No hidden allocation/transfer/densification occurs. To read a
 * framed stream, explicitly prepare a reader with require_eof=false instead.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixOwner Owner>
Result<Owner> ReadSparseMatrixMarket(
    ByteSource& source, MemoryResource& resource, std::span<std::byte> scratch,
    const ArrayIoLimits& limits, const SparseMatrixMarketReadOptions& options,
    SparseMatrixMarketReport& report) {
  auto reader = SparseMatrixMarketReader::Prepare(source, scratch, limits,
                                                  options, report);
  if (!reader.ok()) {
    return reader.status();
  }
  return internal_sparse_matrix_market::OwnerTraits<Owner>::Read(*reader,
                                                                 resource);
}

/** @brief Prevalidates caller staging/alias boundaries before consuming a
 * header, then performs an exact-structure transactional Sparse value read.
 * @tparam View Mutable rank-two canonical host COO/CSR/CSC view.
 * @param source Borrowed nonrewindable synchronous byte source.
 * @param destination Existing immutable structure and live mutable values.
 * @param scratch Caller bytes disjoint from all full staging/destination spans.
 * @param workspace Explicit paired coordinates and typed staged values.
 * @param limits Independent byte/shape/scratch/staging budgets.
 * @param options Explicit assembly and sort-work policies.
 * @param report Reset progress; committed only after whole-file
 * validation/copy.
 * @return OK or bounded error. Every failure preserves destination values and
 * structure; source/staging rollback is not implied. No hidden allocation,
 * transfer or densification. Selected policy results must match every existing
 * coordinate exactly, including stored zeros.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixView View>
  requires(!std::is_const_v<typename View::element_type>)
Status ReadSparseMatrixMarketInto(
    ByteSource& source, const View& destination, std::span<std::byte> scratch,
    SparseMatrixMarketWorkspace<
        typename internal_sparse_matrix_market::ViewTraits<View>::Value>
        workspace,
    const ArrayIoLimits& limits, const SparseMatrixMarketReadOptions& options,
    SparseMatrixMarketReport& report) {
  auto status = internal_sparse_matrix_market::ValidateStateAliases(
      &report, sizeof(report), scratch, workspace);
  if (status.ok()) {
    status = internal_sparse_matrix_market::ValidateViewState(
        destination, &report, sizeof(report));
  }
  if (!status.ok()) {
    return status;
  }
  report = {};
  status = internal_sparse_matrix_market::ValidateInto(destination, scratch,
                                                       workspace, limits);
  if (!status.ok()) {
    return status;
  }
  auto reader = SparseMatrixMarketReader::Prepare(source, scratch, limits,
                                                  options, report);
  if (!reader.ok()) {
    return reader.status();
  }
  return ReadSparseMatrixMarketInto(*reader, destination, workspace);
}

namespace internal_sparse_matrix_market {

template <MatrixView View>
std::size_t Find(const View& view, std::array<index_t, 2> coordinate) {
  using Traits = ViewTraits<View>;
  std::size_t low = 0;
  std::size_t high = static_cast<std::size_t>(view.nnz());
  while (low < high) {
    const auto middle = low + (high - low) / 2;
    if (Less(Traits::Coordinate(view, middle), coordinate, Traits::kColumn)) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  return low < static_cast<std::size_t>(view.nnz()) &&
                 Traits::Coordinate(view, low) == coordinate
             ? low
             : static_cast<std::size_t>(view.nnz());
}

template <typename T>
Result<T> Mirror(T value, MatrixMarketSymmetry symmetry) {
  if (symmetry == MatrixMarketSymmetry::kSkewSymmetric) {
    return internal_matrix_market::CheckedNegate(value);
  }
  return symmetry == MatrixMarketSymmetry::kHermitian
             ? internal_matrix_market::Conjugate(value)
             : value;
}

template <MatrixView View>
Result<std::size_t> ValidateOutput(
    const View& view, const SparseMatrixMarketWriteOptions& options,
    SparseMatrixMarketReport& report) {
  using Traits = ViewTraits<View>;
  using T = typename Traits::Value;
  const auto count = static_cast<std::size_t>(view.nnz());
  const bool structured = options.symmetry != MatrixMarketSymmetry::kGeneral;
  if (structured && view.extents()[0] != view.extents()[1]) {
    return Status(ErrorCode::kShape);
  }
  if constexpr (!internal_array_format::kComplex<T>) {
    if (options.symmetry == MatrixMarketSymmetry::kHermitian) {
      return Status(ErrorCode::kEncoding);
    }
  }
  std::size_t records = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const auto coordinate = Traits::Coordinate(view, i);
    const T value = view.values()[i];
    if (!internal_matrix_market::Finite(value)) {
      return Status(ErrorCode::kOverflow);
    }
    const bool zero = internal_matrix_market::IsZero(value);
    const bool diagonal = coordinate[0] == coordinate[1];
    const bool skew = options.symmetry == MatrixMarketSymmetry::kSkewSymmetric;
    if (structured) {
      if (skew && diagonal) {
        if (!zero) {
          return Status(ErrorCode::kEncoding);
        }
        ++report.omitted_diagonal_zeros;
        continue;
      }
      if constexpr (internal_array_format::kComplex<T>) {
        if (options.symmetry == MatrixMarketSymmetry::kHermitian && diagonal &&
            value.imag() != 0) {
          return Status(ErrorCode::kEncoding);
        }
      }
      if (!diagonal) {
        const auto mirror = Mirror(value, options.symmetry);
        if (!mirror.ok()) {
          return mirror.status();
        }
        const auto partner = Find(view, {coordinate[1], coordinate[0]});
        if ((partner == count && !zero) ||
            (partner != count && view.values()[partner] != *mirror)) {
          return Status(ErrorCode::kEncoding);
        }
      }
    }
    if (zero && options.zero_policy == ExplicitZeroPolicy::kDrop) {
      ++report.dropped_zeros;
      continue;
    }
    if (options.pattern_policy == MatrixMarketPatternPolicy::kUnit &&
        value != T{1}) {
      return Status(ErrorCode::kEncoding);
    }
    if (!structured || coordinate[0] >= coordinate[1] ||
        Find(view, {coordinate[1], coordinate[0]}) == count) {
      ++records;
    }
  }
  report.assembled_entries = records;
  return records;
}

class CountingSink final : public ByteSink {
 public:
  Result<std::size_t> WriteSome(std::span<const std::byte> bytes) override {
    return bytes.size();
  }
};

inline Status EncodeHeader(std::span<const extent_t, 2> shape,
                           MatrixMarketField field,
                           MatrixMarketSymmetry symmetry,
                           const ArrayIoLimits& limits,
                           std::span<std::byte> scratch, std::size_t count,
                           internal_array_io::Output& output,
                           ArrayIoReport& report) {
  using internal_matrix_market::Text;
  auto status = Text(output, "%%MatrixMarket matrix coordinate ");
  if (status.ok()) {
    status = Text(output, internal_matrix_market::FieldName(field));
  }
  if (status.ok()) {
    status = Text(output, " ");
  }
  if (status.ok()) {
    status = Text(output, internal_matrix_market::SymmetryName(symmetry));
  }
  if (status.ok()) {
    status = Text(output, "\n");
  }
  if (!status.ok()) {
    return status;
  }
  if (report.output_bytes > limits.max_header_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  const std::size_t line_start = report.output_bytes;
  const std::array<std::uint64_t, 3> dimensions{
      static_cast<std::uint64_t>(shape[0]),
      static_cast<std::uint64_t>(shape[1]), static_cast<std::uint64_t>(count)};
  for (std::size_t i = 0; i < dimensions.size(); ++i) {
    status = internal_matrix_market::WriteComponent(
        dimensions[i], scratch, limits.max_token_bytes, output);
    if (status.ok()) {
      status = Text(output, i == 2 ? "\n" : " ");
    }
    if (!status.ok()) {
      return status;
    }
  }
  if (report.output_bytes - line_start > limits.max_header_bytes) {
    return Status(ErrorCode::kAllocation);
  }
  report.section = ArrayIoSection::kPayload;
  return Status::Ok();
}

template <MatrixView View>
Status Encode(const View& view, ByteSink& sink, const ArrayIoLimits& limits,
              const SparseMatrixMarketWriteOptions& options,
              std::span<std::byte> scratch, std::size_t count,
              ArrayIoReport& report) {
  using Traits = ViewTraits<View>;
  using T = typename Traits::Value;
  using internal_matrix_market::Text;
  internal_array_io::Output output(sink, limits.max_output_bytes, report);
  const auto field = options.pattern_policy == MatrixMarketPatternPolicy::kUnit
                         ? MatrixMarketField::kPattern
                         : internal_matrix_market::ScalarField<T>();
  auto status = EncodeHeader(
      std::span<const extent_t, 2>(view.extents().data(), 2), field,
      options.symmetry, limits, scratch, count, output, report);
  if (!status.ok()) {
    return status;
  }
  const auto native_count = static_cast<std::size_t>(view.nnz());
  for (std::size_t i = 0; i < native_count; ++i) {
    auto coordinate = Traits::Coordinate(view, i);
    T value = view.values()[i];
    if ((options.zero_policy == ExplicitZeroPolicy::kDrop &&
         internal_matrix_market::IsZero(value)) ||
        (options.symmetry == MatrixMarketSymmetry::kSkewSymmetric &&
         coordinate[0] == coordinate[1])) {
      continue;
    }
    if (options.symmetry != MatrixMarketSymmetry::kGeneral &&
        coordinate[0] < coordinate[1]) {
      if (Find(view, {coordinate[1], coordinate[0]}) != native_count) {
        continue;
      }
      std::swap(coordinate[0], coordinate[1]);
      const auto mirror = Mirror(value, options.symmetry);
      if (!mirror.ok()) {
        return mirror.status();
      }
      value = *mirror;
    }
    const auto line_start = report.output_bytes;
    for (std::size_t dimension = 0; dimension < 2; ++dimension) {
      // Canonical indices are below signed extents, so conversion before +1
      // is representable even at the largest supported extent.
      status = internal_matrix_market::WriteComponent(
          static_cast<std::uint64_t>(coordinate[dimension]) + 1, scratch,
          limits.max_token_bytes, output);
      if (status.ok() &&
          (dimension == 0 || field != MatrixMarketField::kPattern)) {
        status = Text(output, " ");
      }
      if (!status.ok()) {
        return status;
      }
    }
    status = internal_matrix_market::WriteValue(value, field, scratch,
                                                limits.max_token_bytes, output);
    if (status.ok()) {
      status = Text(output, "\n");
    }
    if (!status.ok()) {
      return status;
    }
    if (report.output_bytes - line_start > limits.max_header_bytes) {
      return Status(ErrorCode::kAllocation);
    }
    ++report.values_processed;
  }
  report.section = ArrayIoSection::kComplete;
  return Status::Ok();
}

}  // namespace internal_sparse_matrix_market

/** @brief Writes canonical Sparse storage as explicit coordinate Matrix Market.
 * @tparam View Finalized rank-two COO/CSR/CSC host view, optionally const.
 * @param view Immutable live values/structure; no concurrent mutation is
 * allowed.
 * @param sink Caller synchronous sink retaining accepted bytes on failure.
 * @param limits Independent output/token/per-record/scratch and shape budgets.
 * @param options Explicit symmetry, zero and optional unit-pattern policies.
 * @param scratch Disjoint caller token bytes; no per-file allocation occurs.
 * @param report Reset output progress and zero-projection counters.
 * @return Validation/encoding/I/O failure or OK. Complete finite-value,
 * symmetry, unit-pattern and encoded-size validation precedes the first sink
 * write. General Keep preserves native stored entries. Structured compression
 * preserves represented values, not asymmetric stored-zero structure:
 * upper-only zero entries produce missing lower representatives; skew diagonal
 * zeros are omitted and counted. Nonzero missing partners and invalid diagonals
 * are rejected. Work is O(nnz log(nnz) log(outer extent)) for compressed
 * partner lookup; token emission is checked in a nonallocating dry pass then a
 * real pass. No hidden allocation, transfer, densification or sink-atomicity is
 * implied.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixView View>
Status WriteSparseMatrixMarket(const View& view, ByteSink& sink,
                               const ArrayIoLimits& limits,
                               const SparseMatrixMarketWriteOptions& options,
                               std::span<std::byte> scratch,
                               SparseMatrixMarketReport& report) {
  auto status =
      internal_sparse_matrix_market::ResetReport(view, scratch, report);
  if (!status.ok()) {
    return status;
  }
  status = internal_sparse_matrix_market::ValidateOptions(options);
  if (status.ok()) {
    status = internal_sparse_matrix_market::ValidateView(view, scratch, limits);
  }
  if (!status.ok()) {
    return status;
  }
  const auto count =
      internal_sparse_matrix_market::ValidateOutput(view, options, report);
  if (!count.ok()) {
    return count.status();
  }
  internal_sparse_matrix_market::CountingSink counter;
  ArrayIoReport preflight;
  status = internal_sparse_matrix_market::Encode(view, counter, limits, options,
                                                 scratch, *count, preflight);
  if (!status.ok()) {
    return status;
  }
  return internal_sparse_matrix_market::Encode(view, sink, limits, options,
                                               scratch, *count, report.io);
}

/** @brief Explicitly creates/truncates a path and writes coordinate Matrix
 * Market.
 * @tparam View Finalized rank-two canonical host Sparse view.
 * @param path Core File path with caller-serialized access.
 * @param view Live immutable scalar/structure storage.
 * @param overwrite Required destructive create/truncate intent, never
 * defaulted.
 * @param limits Explicit encoder/storage budgets.
 * @param options Symmetry/zero/unit-pattern interpretation.
 * @param scratch Disjoint caller token bytes.
 * @param report Accepted-byte progress and any checked-close cleanup error.
 * @return First validation/write/flush/close error or OK. File may be truncated
 * even on later validation failure; no atomic replace or durability promise.
 * Codec allocation/transfer/densification is absent; Core File path/handle
 * allocation is a separate explicit convenience boundary.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixView View>
Status SaveSparseMatrixMarket(const std::filesystem::path& path,
                              const View& view, ArrayFileOverwrite overwrite,
                              const ArrayIoLimits& limits,
                              const SparseMatrixMarketWriteOptions& options,
                              std::span<std::byte> scratch,
                              SparseMatrixMarketReport& report) {
  auto status =
      internal_sparse_matrix_market::ResetReport(view, scratch, report);
  if (!status.ok()) {
    return status;
  }
  if (overwrite != ArrayFileOverwrite::kTruncate) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto file = File::OpenWrite(path);
  if (!file.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(file));
  }
  status =
      WriteSparseMatrixMarket(view, *file, limits, options, scratch, report);
  if (status.ok()) {
    report.io.section = ArrayIoSection::kTrailer;
    status = file->Flush();
  }
  auto close = file->Close();
  if (!close.ok()) {
    report.io.cleanup_error = close.code();
    if (status.ok()) {
      status = std::move(close);
    }
  }
  if (status.ok()) {
    report.io.section = ArrayIoSection::kComplete;
  }
  return status;
}

/** @brief Loads one complete coordinate Matrix Market path into a new owner.
 * @tparam Owner Explicit native rank-two COO/CSR/CSC scalar/layout ownership.
 * @param path Core File path, opened read-only with checked close.
 * @param resource Explicit host allocator outliving final ownership.
 * @param scratch Disjoint bounded caller token storage.
 * @param limits Header/shape/resource/staging/token limits.
 * @param options Explicit duplicate/zero/pattern/comparison policies.
 * @param report Reset transaction progress and secondary close diagnostics.
 * @return Owner only after payload, trailing EOF and close success; otherwise
 * failure releases all staging/candidate storage. Codec uses no hidden
 * allocation/transfer/densification; Core File's handle/path allocation is an
 * explicit convenience boundary. Callers serialize shared resources and paths.
 * @ingroup asc_sparse
 */
template <internal_sparse_matrix_market::MatrixOwner Owner>
Result<Owner> LoadSparseMatrixMarket(
    const std::filesystem::path& path, MemoryResource& resource,
    std::span<std::byte> scratch, const ArrayIoLimits& limits,
    const SparseMatrixMarketReadOptions& options,
    SparseMatrixMarketReport& report) {
  if (internal_array_format::Overlaps(&report, sizeof(report), scratch.data(),
                                      scratch.size())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report = {};
  auto file = File::OpenRead(path);
  if (!file.ok()) {
    return internal_core_result::StatusAccess::TakeFailure(std::move(file));
  }
  auto owner = ReadSparseMatrixMarket<Owner>(*file, resource, scratch, limits,
                                             options, report);
  auto close = file->Close();
  if (!close.ok()) {
    report.io.cleanup_error = close.code();
    report.io.committed = false;
    report.io.section = ArrayIoSection::kTrailer;
    if (owner.ok()) {
      return close;
    }
  }
  return owner;
}

}  // namespace asc

#endif  // ASC_SPARSE_MATRIX_MARKET_H_
