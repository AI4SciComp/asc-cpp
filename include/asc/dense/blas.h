#ifndef ASC_DENSE_BLAS_H_
#define ASC_DENSE_BLAS_H_

#include <algorithm>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/export.h"
#include "asc/dense/view.h"

namespace asc {

template <typename Element>
class DenseBlasVectorView {
 public:
  using value_type = Element;
  using element_type = std::remove_const_t<Element>;

  static_assert(
      std::same_as<element_type, float> || std::same_as<element_type, double> ||
          std::same_as<element_type, std::complex<float>> ||
          std::same_as<element_type, std::complex<double>> ||
          std::same_as<element_type, index_t>,
      "DenseBlasVectorView supports BLAS value and public index types");

  static Result<DenseBlasVectorView> Create(Element* logical_first,
                                            extent_t size, stride_t increment,
                                            ConstMemoryView backing_storage) {
    if (size < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A BLAS vector size cannot be negative");
    }
    if (increment == 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A BLAS vector increment cannot be zero");
    }
    if (!backing_storage.valid()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A BLAS vector backing span is invalid");
    }
    if (size == 0) {
      return DenseBlasVectorView(logical_first, size, increment,
                                 backing_storage, nullptr, 0);
    }
    if (logical_first == nullptr || backing_storage.data() == nullptr) {
      return Status(ErrorCode::kMemoryAccess,
                    "A nonempty BLAS vector requires backing storage");
    }

    const std::uintptr_t logical_address =
        reinterpret_cast<std::uintptr_t>(logical_first);
    if (logical_address % alignof(element_type) != 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A BLAS vector pointer is not properly aligned");
    }

    auto element_offset = CheckedMultiply<stride_t>(size - 1, increment);
    if (!element_offset.ok()) {
      return element_offset.status();
    }
    auto byte_offset = CheckedMultiply<stride_t>(
        *element_offset, static_cast<stride_t>(sizeof(element_type)));
    if (!byte_offset.ok()) {
      return byte_offset.status();
    }

    std::uintptr_t other_address = logical_address;
    if (*byte_offset >= 0) {
      const auto positive_offset = static_cast<std::uintptr_t>(*byte_offset);
      if (positive_offset >
          std::numeric_limits<std::uintptr_t>::max() - logical_address) {
        return Status(ErrorCode::kOverflow,
                      "A BLAS vector address calculation overflowed");
      }
      other_address += positive_offset;
    } else {
      if (*byte_offset == std::numeric_limits<stride_t>::min()) {
        return Status(ErrorCode::kOverflow,
                      "A BLAS vector address calculation overflowed");
      }
      const auto negative_offset = static_cast<std::uintptr_t>(-*byte_offset);
      if (logical_address < negative_offset) {
        return Status(ErrorCode::kOverflow,
                      "A BLAS vector address calculation underflowed");
      }
      other_address -= negative_offset;
    }

    const std::uintptr_t reachable_begin =
        std::min(logical_address, other_address);
    const std::uintptr_t reachable_last =
        std::max(logical_address, other_address);
    if (sizeof(element_type) >
        std::numeric_limits<std::uintptr_t>::max() - reachable_last) {
      return Status(ErrorCode::kOverflow,
                    "A BLAS vector reachable span overflowed");
    }
    const std::uintptr_t reachable_end = reachable_last + sizeof(element_type);
    const std::uintptr_t backing_begin =
        reinterpret_cast<std::uintptr_t>(backing_storage.data());
    if (backing_storage.size() >
        std::numeric_limits<std::uintptr_t>::max() - backing_begin) {
      return Status(ErrorCode::kOverflow,
                    "A BLAS vector backing span overflowed");
    }
    const std::uintptr_t backing_end = backing_begin + backing_storage.size();
    if (reachable_begin < backing_begin || reachable_end > backing_end) {
      return Status(ErrorCode::kMemoryAccess,
                    "A BLAS vector is outside its backing span");
    }
    return DenseBlasVectorView(
        logical_first, size, increment, backing_storage,
        reinterpret_cast<const void*>(reachable_begin),
        static_cast<std::size_t>(reachable_end - reachable_begin));
  }

  // Mutable-to-const view conversion is intentionally implicit.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator DenseBlasVectorView<const element_type>() const noexcept
    requires(!std::is_const_v<Element>)
  {
    return DenseBlasVectorView<const element_type>(
        data_, size_, increment_, backing_storage_, reachable_data_,
        reachable_size_);
  }

  [[nodiscard]] Element* data() const noexcept { return data_; }
  [[nodiscard]] extent_t size() const noexcept { return size_; }
  [[nodiscard]] stride_t increment() const noexcept { return increment_; }
  [[nodiscard]] MemorySpace memory_space() const noexcept {
    return backing_storage_.space();
  }
  [[nodiscard]] ConstMemoryView backing_storage() const noexcept {
    return backing_storage_;
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return ConstMemoryView(reachable_data_, reachable_size_,
                           backing_storage_.space());
  }

 private:
  template <typename>
  friend class DenseBlasVectorView;

  DenseBlasVectorView(Element* data, extent_t size, stride_t increment,
                      ConstMemoryView backing_storage,
                      const void* reachable_data,
                      std::size_t reachable_size) noexcept
      : data_(data),
        size_(size),
        increment_(increment),
        backing_storage_(backing_storage),
        reachable_data_(reachable_data),
        reachable_size_(reachable_size) {}

  Element* data_;
  extent_t size_;
  stride_t increment_;
  ConstMemoryView backing_storage_;
  const void* reachable_data_;
  std::size_t reachable_size_;
};

enum class DenseBlasTranspose : std::uint8_t {
  kNone,
  kTranspose,
};

enum class DenseBlasDotAccumulation : std::uint8_t {
  kDouble,
};

ASC_DENSE_EXPORT Status Rotg(const ExecutionContext& context,
                             DenseBlasVectorView<float> a,
                             DenseBlasVectorView<float> b,
                             DenseBlasVectorView<float> c,
                             DenseBlasVectorView<float> s);
ASC_DENSE_EXPORT Status Rotg(const ExecutionContext& context,
                             DenseBlasVectorView<double> a,
                             DenseBlasVectorView<double> b,
                             DenseBlasVectorView<double> c,
                             DenseBlasVectorView<double> s);
ASC_DENSE_EXPORT Status Rotg(const ExecutionContext& context,
                             DenseBlasVectorView<std::complex<float>> a,
                             DenseBlasVectorView<const std::complex<float>> b,
                             DenseBlasVectorView<float> c,
                             DenseBlasVectorView<std::complex<float>> s);
ASC_DENSE_EXPORT Status Rotg(const ExecutionContext& context,
                             DenseBlasVectorView<std::complex<double>> a,
                             DenseBlasVectorView<const std::complex<double>> b,
                             DenseBlasVectorView<double> c,
                             DenseBlasVectorView<std::complex<double>> s);

ASC_DENSE_EXPORT Status Rotmg(const ExecutionContext& context,
                              DenseBlasVectorView<float> d1,
                              DenseBlasVectorView<float> d2,
                              DenseBlasVectorView<float> x1,
                              DenseBlasVectorView<const float> y1,
                              DenseBlasVectorView<float> parameters);
ASC_DENSE_EXPORT Status Rotmg(const ExecutionContext& context,
                              DenseBlasVectorView<double> d1,
                              DenseBlasVectorView<double> d2,
                              DenseBlasVectorView<double> x1,
                              DenseBlasVectorView<const double> y1,
                              DenseBlasVectorView<double> parameters);

ASC_DENSE_EXPORT Status Rot(const ExecutionContext& context,
                            DenseBlasVectorView<float> x,
                            DenseBlasVectorView<float> y, float c, float s);
ASC_DENSE_EXPORT Status Rot(const ExecutionContext& context,
                            DenseBlasVectorView<double> x,
                            DenseBlasVectorView<double> y, double c, double s);
ASC_DENSE_EXPORT Status Rot(const ExecutionContext& context,
                            DenseBlasVectorView<std::complex<float>> x,
                            DenseBlasVectorView<std::complex<float>> y, float c,
                            float s);
ASC_DENSE_EXPORT Status Rot(const ExecutionContext& context,
                            DenseBlasVectorView<std::complex<double>> x,
                            DenseBlasVectorView<std::complex<double>> y,
                            double c, double s);

ASC_DENSE_EXPORT Status Rotm(const ExecutionContext& context,
                             DenseBlasVectorView<float> x,
                             DenseBlasVectorView<float> y,
                             DenseBlasVectorView<const float> parameters);
ASC_DENSE_EXPORT Status Rotm(const ExecutionContext& context,
                             DenseBlasVectorView<double> x,
                             DenseBlasVectorView<double> y,
                             DenseBlasVectorView<const double> parameters);

ASC_DENSE_EXPORT Status Swap(const ExecutionContext& context,
                             DenseBlasVectorView<float> x,
                             DenseBlasVectorView<float> y);
ASC_DENSE_EXPORT Status Swap(const ExecutionContext& context,
                             DenseBlasVectorView<double> x,
                             DenseBlasVectorView<double> y);
ASC_DENSE_EXPORT Status Swap(const ExecutionContext& context,
                             DenseBlasVectorView<std::complex<float>> x,
                             DenseBlasVectorView<std::complex<float>> y);
ASC_DENSE_EXPORT Status Swap(const ExecutionContext& context,
                             DenseBlasVectorView<std::complex<double>> x,
                             DenseBlasVectorView<std::complex<double>> y);

ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, float alpha,
                             DenseBlasVectorView<float> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, double alpha,
                             DenseBlasVectorView<double> destination);
ASC_DENSE_EXPORT Status
Scal(const ExecutionContext& context, std::complex<float> alpha,
     DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_EXPORT Status
Scal(const ExecutionContext& context, std::complex<double> alpha,
     DenseBlasVectorView<std::complex<double>> destination);
ASC_DENSE_EXPORT Status
Scal(const ExecutionContext& context, float alpha,
     DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_EXPORT Status
Scal(const ExecutionContext& context, double alpha,
     DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseBlasVectorView<const float> source,
                             DenseBlasVectorView<float> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseBlasVectorView<const double> source,
                             DenseBlasVectorView<double> destination);
ASC_DENSE_EXPORT Status
Copy(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<float>> source,
     DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_EXPORT Status
Copy(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<double>> source,
     DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, float alpha,
                             DenseBlasVectorView<const float> source,
                             DenseBlasVectorView<float> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, double alpha,
                             DenseBlasVectorView<const double> source,
                             DenseBlasVectorView<double> destination);
ASC_DENSE_EXPORT Status
Axpy(const ExecutionContext& context, std::complex<float> alpha,
     DenseBlasVectorView<const std::complex<float>> source,
     DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_EXPORT Status
Axpy(const ExecutionContext& context, std::complex<double> alpha,
     DenseBlasVectorView<const std::complex<double>> source,
     DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_EXPORT Status Dot(const ExecutionContext& context,
                            DenseBlasVectorView<const float> left,
                            DenseBlasVectorView<const float> right,
                            DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status Dot(const ExecutionContext& context,
                            DenseBlasVectorView<const double> left,
                            DenseBlasVectorView<const double> right,
                            DenseBlasVectorView<double> result);
ASC_DENSE_EXPORT Status Dot(const ExecutionContext& context, float bias,
                            DenseBlasVectorView<const float> left,
                            DenseBlasVectorView<const float> right,
                            DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status Dot(const ExecutionContext& context,
                            DenseBlasDotAccumulation accumulation,
                            DenseBlasVectorView<const float> left,
                            DenseBlasVectorView<const float> right,
                            DenseBlasVectorView<double> result);

ASC_DENSE_EXPORT Status
Dotu(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<float>> left,
     DenseBlasVectorView<const std::complex<float>> right,
     DenseBlasVectorView<std::complex<float>> result);
ASC_DENSE_EXPORT Status
Dotu(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<double>> left,
     DenseBlasVectorView<const std::complex<double>> right,
     DenseBlasVectorView<std::complex<double>> result);
ASC_DENSE_EXPORT Status
Dotc(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<float>> left,
     DenseBlasVectorView<const std::complex<float>> right,
     DenseBlasVectorView<std::complex<float>> result);
ASC_DENSE_EXPORT Status
Dotc(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<double>> left,
     DenseBlasVectorView<const std::complex<double>> right,
     DenseBlasVectorView<std::complex<double>> result);

ASC_DENSE_EXPORT Status Nrm2(const ExecutionContext& context,
                             DenseBlasVectorView<const float> operand,
                             DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status Nrm2(const ExecutionContext& context,
                             DenseBlasVectorView<const double> operand,
                             DenseBlasVectorView<double> result);
ASC_DENSE_EXPORT Status
Nrm2(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<float>> operand,
     DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status
Nrm2(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<double>> operand,
     DenseBlasVectorView<double> result);

ASC_DENSE_EXPORT Status Asum(const ExecutionContext& context,
                             DenseBlasVectorView<const float> operand,
                             DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status Asum(const ExecutionContext& context,
                             DenseBlasVectorView<const double> operand,
                             DenseBlasVectorView<double> result);
ASC_DENSE_EXPORT Status
Asum(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<float>> operand,
     DenseBlasVectorView<float> result);
ASC_DENSE_EXPORT Status
Asum(const ExecutionContext& context,
     DenseBlasVectorView<const std::complex<double>> operand,
     DenseBlasVectorView<double> result);

ASC_DENSE_EXPORT Status Iamax(const ExecutionContext& context,
                              DenseBlasVectorView<const float> operand,
                              DenseBlasVectorView<index_t> result);
ASC_DENSE_EXPORT Status Iamax(const ExecutionContext& context,
                              DenseBlasVectorView<const double> operand,
                              DenseBlasVectorView<index_t> result);
ASC_DENSE_EXPORT Status
Iamax(const ExecutionContext& context,
      DenseBlasVectorView<const std::complex<float>> operand,
      DenseBlasVectorView<index_t> result);
ASC_DENSE_EXPORT Status
Iamax(const ExecutionContext& context,
      DenseBlasVectorView<const std::complex<double>> operand,
      DenseBlasVectorView<index_t> result);

ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const float, 1> source,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const float, 2> source,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const double, 1> source,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const double, 2> source,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, float alpha,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, float alpha,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, double alpha,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, double alpha,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, float alpha,
                             DenseView<const float, 1> source,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, float alpha,
                             DenseView<const float, 2> source,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, double alpha,
                             DenseView<const double, 1> source,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, double alpha,
                             DenseView<const double, 2> source,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Result<float> Dot(const ExecutionContext& context,
                                   DenseView<const float, 1> left,
                                   DenseView<const float, 1> right);
ASC_DENSE_EXPORT Result<double> Dot(const ExecutionContext& context,
                                    DenseView<const double, 1> left,
                                    DenseView<const double, 1> right);

ASC_DENSE_EXPORT Result<float> Nrm2(const ExecutionContext& context,
                                    DenseView<const float, 1> operand);
ASC_DENSE_EXPORT Result<double> Nrm2(const ExecutionContext& context,
                                     DenseView<const double, 1> operand);

ASC_DENSE_EXPORT Status Gemv(const ExecutionContext& context,
                             DenseBlasTranspose transpose, float alpha,
                             DenseView<const float, 2> matrix,
                             DenseView<const float, 1> input, float beta,
                             DenseView<float, 1> output);
ASC_DENSE_EXPORT Status Gemv(const ExecutionContext& context,
                             DenseBlasTranspose transpose, double alpha,
                             DenseView<const double, 2> matrix,
                             DenseView<const double, 1> input, double beta,
                             DenseView<double, 1> output);

ASC_DENSE_EXPORT Status Gemm(const ExecutionContext& context,
                             DenseBlasTranspose left_transpose,
                             DenseBlasTranspose right_transpose, float alpha,
                             DenseView<const float, 2> left,
                             DenseView<const float, 2> right, float beta,
                             DenseView<float, 2> output);
ASC_DENSE_EXPORT Status Gemm(const ExecutionContext& context,
                             DenseBlasTranspose left_transpose,
                             DenseBlasTranspose right_transpose, double alpha,
                             DenseView<const double, 2> left,
                             DenseView<const double, 2> right, double beta,
                             DenseView<double, 2> output);

}  // namespace asc

#endif  // ASC_DENSE_BLAS_H_
