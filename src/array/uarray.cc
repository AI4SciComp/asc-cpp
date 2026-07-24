// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/uarray.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <fstream>
#include <type_traits>
#include "asc/array/uarray.h"
#include "asc/core/forall.h"

namespace asc {

template <typename T>
void UArray<T>::Print(std::ostream& os, int width) const {
  static_cast<void>(width);
  const T* data = HostRead();
  os << '[';
  for (int i = 0; i < size_; i++) {
    PPrint(os, data[i]);
    if (i != GetSize() - 1) os << ", ";
  }
  os << ']' << '\n';
}

template <typename T>
void UArray<T>::Save(std::ostream& os, int fmt) const {
  if (fmt == 0) {
    os << size_ << '\n';
  }
  const T* data = HostRead();
  for (int i = 0; i < size_; i++) {
    os << data[i] << '\n';
  }
}

template <typename T>
void UArray<T>::Load(std::istream& in, int fmt) {
  if (fmt == 0) {
    int new_size;
    in >> new_size;
    ASC_VERIFY(in.good(), "failed to read array size");
    SetSize(new_size);
  }
  T* data = HostWrite();
  for (int i = 0; i < size_; i++) {
    in >> data[i];
    ASC_VERIFY(in.good(), "failed to read array value");
  }
}

template <typename T>
T UArray<T>::Max() const {
  ASC_VERIFY(size_ > 0, "UArray is empty with size " << size_);

  const T* data = HostRead();
  T max = data[0];
  for (int i = 1; i < size_; i++) {
    if (max < data[i]) {
      max = data[i];
    }
  }

  return max;
}

template <typename T>
T UArray<T>::Min() const {
  ASC_VERIFY(size_ > 0, "UArray is empty with size " << size_);

  const T* data = HostRead();
  T min = data[0];
  for (int i = 1; i < size_; i++) {
    if (data[i] < min) {
      min = data[i];
    }
  }

  return min;
}

// Partial Sum
template <typename T>
void UArray<T>::PartialSum() {
  T sum = static_cast<T>(0);
  T* data = HostReadWrite();
  for (int i = 0; i < size_; i++) {
    sum += data[i];
    data[i] = sum;
  }
}

template <typename T>
void UArray<T>::Abs() {
  static_assert(std::is_arithmetic<T>::value, "Use with arithmetic types!");
  const bool use_dev = UseDevice();
  const int N = size_;
  auto y = ReadWrite(use_dev);
  asc::ForallSwitch(use_dev, N,
                    [=] ASC_HOST_DEVICE(int i) { y[i] = std::abs(y[i]); });
}

// Sum
template <typename T>
T UArray<T>::Sum() const {
  T sum = static_cast<T>(0);
  const T* data = HostRead();
  for (int i = 0; i < size_; i++) {
    sum += data[i];
  }

  return sum;
}

template <typename T>
int UArray<T>::IsSorted() const {
  if (size_ <= 1) {
    return 1;
  }
  const T* data = HostRead();
  T val_prev = data[0], val;
  for (int i = 1; i < size_; i++) {
    val = data[i];
    if (val < val_prev) {
      return 0;
    }
    val_prev = val;
  }

  return 1;
}

template class UArray<char>;
template class UArray<int>;
template class UArray<real_t>;

}  // namespace asc
