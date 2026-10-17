#ifndef ASC_DENSE_LAPACK_TYPES_H_
#define ASC_DENSE_LAPACK_TYPES_H_

/** @file
 * @brief Provider-neutral identities for explicitly selected Dense LAPACK
 * calls.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/export.h"

namespace asc {

/** @brief Actual primary and mixed numerical signatures, independent of
 * storage. */
enum class LapackScalarKind : std::uint8_t {
  kF32,   ///< Single real precision.
  kF64,   ///< Double real precision.
  kC64,   ///< Single complex precision with real float auxiliary buffers.
  kC128,  ///< Double complex precision with real double auxiliary buffers.
  kMixedF64F32,  ///< Explicit double/single real arguments in one operation.
  kMixedC128C64  ///< Explicit double/single complex arguments in one operation.
};

/** @brief Distinguishes native execution from a separately selected provider.
 */
enum class LapackProviderKind : std::uint8_t {
  kNative,    ///< Provider-free ASC implementation.
  kReference  ///< Explicit Reference-LAPACK provider facet.
};

/** @brief Integer interfaces whose matching foreign libraries must be verified.
 */
enum class LapackIntegerAbi : std::uint8_t {
  kNative64,   ///< Native ASC indices, without a foreign integer interface.
  kLp64,       ///< Ordinary provider interface with signed 32-bit integers.
  kIlp64,      ///< Entire provider stack uses signed 64-bit integers.
  kSuffixed64  ///< Separate suffixed 64-bit interface; not an ILP64 build
               ///< claim.
};

/** @brief Copied immutable provider provenance; contains no owned foreign
 * handle.
 *
 * Reference contexts supply exact source/build digests and runtime ABI widths.
 * All-zero digests identify an unspecified native build, never a verified
 * reference provider. This record makes no provider availability claim.
 */
struct LapackProviderIdentity {
  LapackProviderKind kind = LapackProviderKind::kNative;  ///< Dispatch route.
  LapackIntegerAbi integer_abi =
      LapackIntegerAbi::kNative64;  ///< Integer route.
  std::uint8_t logical_bytes = 1;   ///< Native logical payload width in bytes.
  std::array<std::byte, 32> source_sha256{};  ///< Exact provider source digest.
  std::array<std::byte, 32>
      build_sha256{};  ///< Compiler/options/library digest.
  std::array<std::uint32_t, 3>
      version{};  ///< Recorded major/minor/patch metadata.
  /** @brief Compares complete copied provenance without allocation.
   * @return True exactly when every compared identity field is equal.
   */
  bool operator==(const LapackProviderIdentity&) const = default;
};

/** @brief Owns a complete bounded query key; changing any field invalidates
 * plans.
 *
 * CPU-only metadata construction allocates nothing. The binding must supply
 * every dimension and option influencing a query, including leading dimensions
 * and layout. Capacity overflow is rejected, never truncated. This key does not
 * replace each operation's scalar, flag, shape or provider validation.
 */
class LapackPlanIdentity {
 public:
  /** @brief Copies a key with at most 31 name characters and 16 fields per
   * list.
   * @param routine Nonempty upstream operation name; copied into owned storage.
   * @param scalar Actual numerical signature from the reviewed operation.
   * @param dimensions All nonnegative dimensions affecting this query.
   * @param options All integer-encoded flags and layouts affecting this query.
   * @param provider Complete selected provider/build/integer/logical identity.
   * @return An owned key, or invalid argument/overflow without allocation.
   */
  static ASC_DENSE_EXPORT Result<LapackPlanIdentity> Create(
      std::string_view routine, LapackScalarKind scalar,
      std::span<const extent_t> dimensions,
      std::span<const std::int64_t> options, LapackProviderIdentity provider);

  /** @brief Returns a borrowed name valid until this key is destroyed.
   * @return A borrowed name valid until this key is destroyed.
   */
  [[nodiscard]] std::string_view routine() const noexcept {
    return {routine_.data(), routine_size_};
  }
  /** @brief Returns the primary/mixed numerical signature.
   * @return The primary/mixed numerical signature.
   */
  [[nodiscard]] LapackScalarKind scalar() const noexcept { return scalar_; }
  /** @brief Returns copied provider provenance; no foreign lifetime is
   * extended.
   * @return Copied provider provenance; no foreign lifetime is extended.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }
  /** @brief Compares all owned fields; independent of caller buffer lifetimes.
   * @return True exactly when every compared identity field is equal.
   */
  bool operator==(const LapackPlanIdentity&) const = default;

 private:
  LapackPlanIdentity() = default;
  std::array<char, 32> routine_{};
  std::size_t routine_size_ = 0;
  LapackScalarKind scalar_ = LapackScalarKind::kF32;
  std::array<extent_t, 16> dimensions_{};
  std::array<std::int64_t, 16> options_{};
  std::size_t dimension_count_ = 0;
  std::size_t option_count_ = 0;
  LapackProviderIdentity provider_{};
};

/** @brief Foreign pivot/factor encodings must retain their originating family.
 */
enum class LapackFactorFamily : std::uint8_t {
  kLuPartialPivot,  ///< GETRF one-based sequential row swaps.
  kCholesky,        ///< POTRF positive-definite triangular factor.
  kHouseholderQr,   ///< GEQRF packed reflectors and tau.
  kBunchKaufman,    ///< Signed paired block pivots, not an LU swap sequence.
  kRook,            ///< Rook-specific signed block-pivot conventions.
  kAasen,           ///< Aasen-specific factor/pivot metadata.
  kColumnPivotedQr  ///< GEQP3/GELSY one-based final column permutation.
};

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_TYPES_H_
