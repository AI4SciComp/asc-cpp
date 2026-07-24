// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/permutation.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include "asc/random/permutation.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace asc {

Prime& Prime::Get() {
  static Prime instance;
  return instance;
}

int Prime::Find(int n, bool enable_cache) {
  ASC_VERIFY(n >= 1, "prime index must be positive");

  if (enable_cache && n <= size_) {
    return data_[static_cast<std::size_t>(n)];
  }

  int limit = EstimateUpperBound(n);
  while (true) {
    std::vector<bool> sieve(static_cast<std::size_t>(limit + 1), true);
    sieve[0] = false;
    sieve[1] = false;

    for (int i = 2; i * i <= limit; ++i) {
      if (!sieve[i]) {
        continue;
      }
      for (int j = i * i; j <= limit; j += i) {
        sieve[j] = false;
      }
    }

    std::vector<int> primes;
    primes.reserve(static_cast<std::size_t>(n));
    for (int i = 2; i <= limit && static_cast<int>(primes.size()) < n; ++i) {
      if (sieve[i]) {
        primes.push_back(i);
      }
    }

    if (static_cast<int>(primes.size()) < n) {
      limit *= 2;
      continue;
    }

    const int result = primes[static_cast<std::size_t>(n - 1)];
    if (enable_cache && static_cast<int>(primes.size()) > size_) {
      const int prime_count = static_cast<int>(primes.size());
      data_.reserve(static_cast<std::size_t>(prime_count + 1));
      cumsum_.reserve(static_cast<std::size_t>(prime_count + 1));
      for (int i = size_; i < prime_count; ++i) {
        ++size_;
        const int prime = primes[static_cast<std::size_t>(i)];
        data_.push_back(prime);
        cumsum_.push_back(cumsum_.back() + prime);
      }
    }
    return result;
  }
}

Prime::Prime() : data_(1, 0), cumsum_(1, 0), size_(0) {
  data_.reserve(32);
  cumsum_.reserve(32);
}

int Prime::EstimateUpperBound(int n) {
  if (n < 6) {
    return 15;
  }
  return static_cast<int>(n * (std::log(n) + std::log(std::log(n))));
}

int* LowDiscrepancyPermutation::GetData() {
  EnsureSize(std::max(Prime::Get().GetSize(), 2));
  return data_.data();
}

void LowDiscrepancyPermutation::EnsureSize(int prime_count) {
  ASC_VERIFY(prime_count >= 0, "prime count must be non-negative");
  if (prime_count <= prime_count_) {
    return;
  }

  Prime& p = Prime::Get();
  p.Find(prime_count, true);
  const int* primes = p.GetData();
  const int* prime_sums = p.GetCumsum();

  data_.assign(static_cast<std::size_t>(prime_sums[prime_count]), 0);

  int* offset = data_.data();
  for (int i = 1; i <= prime_count; ++i) {
    const int pi = primes[i];
    for (int j = 0; j < pi; ++j) {
      offset[j] = j;
    }

    if (i > 2) {
      std::mt19937 rng(static_cast<std::mt19937::result_type>(
          5489u + static_cast<unsigned int>(i)));
      std::shuffle(offset + 1, offset + pi, rng);
    }
    offset += pi;
  }

  prime_count_ = prime_count;
}

LowDiscrepancyPermutation& LowDiscrepancyPermutation::Get() {
  static LowDiscrepancyPermutation instance;
  return instance;
}

}  // namespace asc
