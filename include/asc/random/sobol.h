// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/sobol.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_SOBOL_H_
#define ASC_SOBOL_H_

#include <cstdint>
#include <type_traits>

#include "asc/array/marray.h"
#include "asc/core/error.h"
#include "asc/core/math.h"
#include "asc/array/mlayout.h"

namespace asc {

// Constants for Sobol sequence data
inline constexpr int kSobolDimMax2 = 1111;
inline constexpr int kSobolLogMax = 62;

/// @brief Initialize Sobol direction numbers
/// @param v 2D array [kSobolDimMax2][kSobolLogMax] to fill with direction
/// numbers
void InitializeSobolDirectionNumbers(int64_t (&v)[kSobolDimMax2][kSobolLogMax]);

/// @brief Clamp a value to [0, 1) range for quasi-random sequences
/// @tparam T Floating-point type
/// @param x Input value
/// @return Value clamped to [0, 1 - epsilon]
template <FloatingPoint T>
T Variate(T x);

/// @brief Sobol sequence (deterministic)
///
/// Sobol sequences are low-discrepancy sequences widely used in
/// quasi-Monte Carlo methods for high-dimensional integration and sampling.
/// They provide better uniform coverage of the sample space compared to
/// pseudo-random sampling.
///
/// @tparam T The floating-point type for samples
template <FloatingPoint T>
class SobolSampler {
 protected:
  int PosHighBitOne(int64_t n) {
    int bit = 0;
    while (0 < n) {
      ++bit;
      n /= 2;
    }
    return bit;
  }

  int PosLowBitZero(int64_t n) {
    int bit = 0;
    int64_t n2;
    while (true) {
      bit = bit + 1;
      n2 = n / 2;
      if (n == 2 * n2) break;
      n = n2;
    }
    return bit;
  }

  void SampleSobol(int n_dims, T quasi[]) {
    ASC_VERIFY(n_dims >= 1 && n_dims <= kSobolDimMax2,
                  "Sobol dimension must be in [1, " << kSobolDimMax2
                                                     << "], got " << n_dims);
    //
    //  Here, we have commented out the definition of ATMOST, because
    //  in some cases, a compiler was complaining that the value of ATMOST
    //  could not seem to be properly stored.  We only need ATMOST in order to
    //  specify MAXCOL, so as long as we set MAXCOL (below) to what we expect
    //  it should be, we may be able to get around this difficulty. JVB, 24
    //  January 2006.
    //
    // static int64_t atmost = 4611686018427387903;
    //
    int64_t i;
    bool includ[kSobolLogMax];
    int64_t j;
    int64_t j2;
    int64_t k;
    int64_t l;
    int64_t m;
    int64_t newv;
    static constexpr int64_t poly[kSobolDimMax2] = {
        1,     3,     7,     11,    13,    19,    25,    37,    59,    47,
        61,    55,    41,    67,    97,    91,    109,   103,   115,   131,
        193,   137,   145,   143,   241,   157,   185,   167,   229,   171,
        213,   191,   253,   203,   211,   239,   247,   285,   369,   299,
        301,   333,   351,   355,   357,   361,   391,   397,   425,   451,
        463,   487,   501,   529,   539,   545,   557,   563,   601,   607,
        617,   623,   631,   637,   647,   661,   675,   677,   687,   695,
        701,   719,   721,   731,   757,   761,   787,   789,   799,   803,
        817,   827,   847,   859,   865,   875,   877,   883,   895,   901,
        911,   949,   953,   967,   971,   973,   981,   985,   995,   1001,
        1019,  1033,  1051,  1063,  1069,  1125,  1135,  1153,  1163,  1221,
        1239,  1255,  1267,  1279,  1293,  1305,  1315,  1329,  1341,  1347,
        1367,  1387,  1413,  1423,  1431,  1441,  1479,  1509,  1527,  1531,
        1555,  1557,  1573,  1591,  1603,  1615,  1627,  1657,  1663,  1673,
        1717,  1729,  1747,  1759,  1789,  1815,  1821,  1825,  1849,  1863,
        1869,  1877,  1881,  1891,  1917,  1933,  1939,  1969,  2011,  2035,
        2041,  2053,  2071,  2091,  2093,  2119,  2147,  2149,  2161,  2171,
        2189,  2197,  2207,  2217,  2225,  2255,  2257,  2273,  2279,  2283,
        2293,  2317,  2323,  2341,  2345,  2363,  2365,  2373,  2377,  2385,
        2395,  2419,  2421,  2431,  2435,  2447,  2475,  2477,  2489,  2503,
        2521,  2533,  2551,  2561,  2567,  2579,  2581,  2601,  2633,  2657,
        2669,  2681,  2687,  2693,  2705,  2717,  2727,  2731,  2739,  2741,
        2773,  2783,  2793,  2799,  2801,  2811,  2819,  2825,  2833,  2867,
        2879,  2881,  2891,  2905,  2911,  2917,  2927,  2941,  2951,  2955,
        2963,  2965,  2991,  2999,  3005,  3017,  3035,  3037,  3047,  3053,
        3083,  3085,  3097,  3103,  3159,  3169,  3179,  3187,  3205,  3209,
        3223,  3227,  3229,  3251,  3263,  3271,  3277,  3283,  3285,  3299,
        3305,  3319,  3331,  3343,  3357,  3367,  3373,  3393,  3399,  3413,
        3417,  3427,  3439,  3441,  3475,  3487,  3497,  3515,  3517,  3529,
        3543,  3547,  3553,  3559,  3573,  3589,  3613,  3617,  3623,  3627,
        3635,  3641,  3655,  3659,  3669,  3679,  3697,  3707,  3709,  3713,
        3731,  3743,  3747,  3771,  3791,  3805,  3827,  3833,  3851,  3865,
        3889,  3895,  3933,  3947,  3949,  3957,  3971,  3985,  3991,  3995,
        4007,  4013,  4021,  4045,  4051,  4069,  4073,  4179,  4201,  4219,
        4221,  4249,  4305,  4331,  4359,  4383,  4387,  4411,  4431,  4439,
        4449,  4459,  4485,  4531,  4569,  4575,  4621,  4663,  4669,  4711,
        4723,  4735,  4793,  4801,  4811,  4879,  4893,  4897,  4921,  4927,
        4941,  4977,  5017,  5027,  5033,  5127,  5169,  5175,  5199,  5213,
        5223,  5237,  5287,  5293,  5331,  5391,  5405,  5453,  5523,  5573,
        5591,  5597,  5611,  5641,  5703,  5717,  5721,  5797,  5821,  5909,
        5913,  5955,  5957,  6005,  6025,  6061,  6067,  6079,  6081,  6231,
        6237,  6289,  6295,  6329,  6383,  6427,  6453,  6465,  6501,  6523,
        6539,  6577,  6589,  6601,  6607,  6631,  6683,  6699,  6707,  6761,
        6795,  6865,  6881,  6901,  6923,  6931,  6943,  6999,  7057,  7079,
        7103,  7105,  7123,  7173,  7185,  7191,  7207,  7245,  7303,  7327,
        7333,  7355,  7365,  7369,  7375,  7411,  7431,  7459,  7491,  7505,
        7515,  7541,  7557,  7561,  7701,  7705,  7727,  7749,  7761,  7783,
        7795,  7823,  7907,  7953,  7963,  7975,  8049,  8089,  8123,  8125,
        8137,  8219,  8231,  8245,  8275,  8293,  8303,  8331,  8333,  8351,
        8357,  8367,  8379,  8381,  8387,  8393,  8417,  8435,  8461,  8469,
        8489,  8495,  8507,  8515,  8551,  8555,  8569,  8585,  8599,  8605,
        8639,  8641,  8647,  8653,  8671,  8675,  8689,  8699,  8729,  8741,
        8759,  8765,  8771,  8795,  8797,  8825,  8831,  8841,  8855,  8859,
        8883,  8895,  8909,  8943,  8951,  8955,  8965,  8999,  9003,  9031,
        9045,  9049,  9071,  9073,  9085,  9095,  9101,  9109,  9123,  9129,
        9137,  9143,  9147,  9185,  9197,  9209,  9227,  9235,  9247,  9253,
        9257,  9277,  9297,  9303,  9313,  9325,  9343,  9347,  9371,  9373,
        9397,  9407,  9409,  9415,  9419,  9443,  9481,  9495,  9501,  9505,
        9517,  9529,  9555,  9557,  9571,  9585,  9591,  9607,  9611,  9621,
        9625,  9631,  9647,  9661,  9669,  9679,  9687,  9707,  9731,  9733,
        9745,  9773,  9791,  9803,  9811,  9817,  9833,  9847,  9851,  9863,
        9875,  9881,  9905,  9911,  9917,  9923,  9963,  9973,  10003, 10025,
        10043, 10063, 10071, 10077, 10091, 10099, 10105, 10115, 10129, 10145,
        10169, 10183, 10187, 10207, 10223, 10225, 10247, 10265, 10271, 10275,
        10289, 10299, 10301, 10309, 10343, 10357, 10373, 10411, 10413, 10431,
        10445, 10453, 10463, 10467, 10473, 10491, 10505, 10511, 10513, 10523,
        10539, 10549, 10559, 10561, 10571, 10581, 10615, 10621, 10625, 10643,
        10655, 10671, 10679, 10685, 10691, 10711, 10739, 10741, 10755, 10767,
        10781, 10785, 10803, 10805, 10829, 10857, 10863, 10865, 10875, 10877,
        10917, 10921, 10929, 10949, 10967, 10971, 10987, 10995, 11009, 11029,
        11043, 11045, 11055, 11063, 11075, 11081, 11117, 11135, 11141, 11159,
        11163, 11181, 11187, 11225, 11237, 11261, 11279, 11297, 11307, 11309,
        11327, 11329, 11341, 11377, 11403, 11405, 11413, 11427, 11439, 11453,
        11461, 11473, 11479, 11489, 11495, 11499, 11533, 11545, 11561, 11567,
        11575, 11579, 11589, 11611, 11623, 11637, 11657, 11663, 11687, 11691,
        11701, 11747, 11761, 11773, 11783, 11795, 11797, 11817, 11849, 11855,
        11867, 11869, 11873, 11883, 11919, 11921, 11927, 11933, 11947, 11955,
        11961, 11999, 12027, 12029, 12037, 12041, 12049, 12055, 12095, 12097,
        12107, 12109, 12121, 12127, 12133, 12137, 12181, 12197, 12207, 12209,
        12239, 12253, 12263, 12269, 12277, 12287, 12295, 12309, 12313, 12335,
        12361, 12367, 12391, 12409, 12415, 12433, 12449, 12469, 12479, 12481,
        12499, 12505, 12517, 12527, 12549, 12559, 12597, 12615, 12621, 12639,
        12643, 12657, 12667, 12707, 12713, 12727, 12741, 12745, 12763, 12769,
        12779, 12781, 12787, 12799, 12809, 12815, 12829, 12839, 12857, 12875,
        12883, 12889, 12901, 12929, 12947, 12953, 12959, 12969, 12983, 12987,
        12995, 13015, 13019, 13031, 13063, 13077, 13103, 13137, 13149, 13173,
        13207, 13211, 13227, 13241, 13249, 13255, 13269, 13283, 13285, 13303,
        13307, 13321, 13339, 13351, 13377, 13389, 13407, 13417, 13431, 13435,
        13447, 13459, 13465, 13477, 13501, 13513, 13531, 13543, 13561, 13581,
        13599, 13605, 13617, 13623, 13637, 13647, 13661, 13677, 13683, 13695,
        13725, 13729, 13753, 13773, 13781, 13785, 13795, 13801, 13807, 13825,
        13835, 13855, 13861, 13871, 13883, 13897, 13905, 13915, 13939, 13941,
        13969, 13979, 13981, 13997, 14027, 14035, 14037, 14051, 14063, 14085,
        14095, 14107, 14113, 14125, 14137, 14145, 14151, 14163, 14193, 14199,
        14219, 14229, 14233, 14243, 14277, 14287, 14289, 14295, 14301, 14305,
        14323, 14339, 14341, 14359, 14365, 14375, 14387, 14411, 14425, 14441,
        14449, 14499, 14513, 14523, 14537, 14543, 14561, 14579, 14585, 14593,
        14599, 14603, 14611, 14641, 14671, 14695, 14701, 14723, 14725, 14743,
        14753, 14759, 14765, 14795, 14797, 14803, 14831, 14839, 14845, 14855,
        14889, 14895, 14909, 14929, 14941, 14945, 14951, 14963, 14965, 14985,
        15033, 15039, 15053, 15059, 15061, 15071, 15077, 15081, 15099, 15121,
        15147, 15149, 15157, 15167, 15187, 15193, 15203, 15205, 15215, 15217,
        15223, 15243, 15257, 15269, 15273, 15287, 15291, 15313, 15335, 15347,
        15359, 15373, 15379, 15381, 15391, 15395, 15397, 15419, 15439, 15453,
        15469, 15491, 15503, 15517, 15527, 15531, 15545, 15559, 15593, 15611,
        15613, 15619, 15639, 15643, 15649, 15661, 15667, 15669, 15681, 15693,
        15717, 15721, 15741, 15745, 15765, 15793, 15799, 15811, 15825, 15835,
        15847, 15851, 15865, 15877, 15881, 15887, 15899, 15915, 15935, 15937,
        15955, 15973, 15977, 16011, 16035, 16061, 16069, 16087, 16093, 16097,
        16121, 16141, 16153, 16159, 16165, 16183, 16189, 16195, 16197, 16201,
        16209, 16215, 16225, 16259, 16265, 16273, 16299, 16309, 16355, 16375,
        16381};
    int64_t seed_temp;

    if (!initialized_ || n_dims != dim_save_) {
      initialized_ = true;
      for (i = 0; i < kSobolDimMax2; i++) {
        for (j = 0; j < kSobolLogMax; j++) {
          v_[i][j] = 0;
        }
      }
      //
      //  Initialize (part of) V.
      //
      InitializeSobolDirectionNumbers(v_);
      dim_save_ = n_dims;
      seed_save_ = -1;
      for (i = 0; i < kSobolDimMax2; ++i) {
        lastq_[i] = 0;
      }
      //
      //  Find the number of bits in ATMOST.
      //
      //  Here, we have short-circuited the computation of MAXCOL from ATMOST,
      //  because in some cases, a compiler was complaining that the value of
      //  ATMOST could not seem to be properly stored.  We only need ATMOST in
      //  order to specify MAXCOL, so if we know what the answer should be we
      //  can try to get it this way! JVB, 24 January 2006.
      //
      //  maxcol_ = i8_bit_hi1 ( atmost );
      //
      maxcol_ = 62;
      //
      //  Initialize row 1 of V.
      //
      for (j = 0; j < maxcol_; j++) {
        v_[0][j] = 1;
      }
      //
      //  Initialize the remaining rows of V.
      //
      for (i = 1; i < n_dims; i++) {
        //
        //  The bit pattern of the integer POLY(I) gives the form
        //  of polynomial I.
        //
        //  Find the degree of polynomial I from binary encoding.
        //
        j = poly[i];
        m = 0;

        while (true) {
          j = j / 2;
          if (j <= 0) {
            break;
          }
          m = m + 1;
        }
        //
        //  We expand this bit pattern to separate components
        //  of the logical array INCLUD.
        //
        j = poly[i];
        for (k = m - 1; 0 <= k; k--) {
          j2 = j / 2;
          includ[k] = (j != (2 * j2));
          j = j2;
        }
        //
        //  Calculate the remaining elements of row I as explained
        //  in Bratley and Fox, section 2.
        //
        //  Some tricky indexing here.  Did I change it correctly?
        //
        for (j = m; j < maxcol_; j++) {
          newv = v_[i][j - m];
          l = 1;

          for (k = 0; k < m; k++) {
            l = 2 * l;

            if (includ[k]) {
              newv = (newv ^ (l * v_[i][j - k - 1]));
            }
          }
          v_[i][j] = newv;
        }
      }
      //
      //  Multiply columns of V by appropriate power of 2.
      //
      l = 1;
      for (j = maxcol_ - 2; 0 <= j; j--) {
        l = 2 * l;
        for (i = 0; i < n_dims; i++) {
          v_[i][j] = v_[i][j] * l;
        }
      }
      //
      //  RECIPD is 1/(common denominator of the elements in V).
      //
      recipd_ = 1.0E+00 / (static_cast<double>(2 * l));
    }

    if (seed_ < 0) {
      seed_ = 0;
    }
    if (seed_ == 0) {
      l = 1;
      for (i = 0; i < n_dims; i++) {
        lastq_[i] = 0;
      }
    } else if (seed_ == seed_save_ + 1) {
      l = PosLowBitZero(seed_);
    } else if (seed_ <= seed_save_) {
      seed_save_ = 0;
      l = 1;
      for (i = 0; i < n_dims; i++) {
        lastq_[i] = 0;
      }

      for (seed_temp = seed_save_; seed_temp <= (seed_)-1; seed_temp++) {
        l = PosLowBitZero(seed_temp);

        for (i = 0; i < n_dims; i++) {
          lastq_[i] = (lastq_[i] ^ v_[i][l - 1]);
        }
      }
      l = PosLowBitZero(seed_);
    } else if (seed_save_ + 1 < seed_) {
      for (seed_temp = seed_save_ + 1; seed_temp <= (seed_)-1; seed_temp++) {
        l = PosLowBitZero(seed_temp);

        for (i = 0; i < n_dims; i++) {
          lastq_[i] = (lastq_[i] ^ v_[i][l - 1]);
        }
      }
      l = PosLowBitZero(seed_);
    }
    ASC_VERIFY(maxcol_ >= l,
                  "Sobol seed is too large: seed=" << seed_
                                                   << ", maxcol=" << maxcol_
                                                   << ", l=" << l);
    //
    //  Calculate the new components of QUASI.
    //  The caret indicates the bitwise exclusive OR.
    //
    for (i = 0; i < n_dims; i++) {
      quasi[i] = (static_cast<T>(lastq_[i])) * recipd_;
      lastq_[i] = (lastq_[i] ^ v_[i][l - 1]);
    }

    seed_save_ = seed_++;

    return;
  }

 public:
  explicit SobolSampler(int64_t skip = 0) : seed_(skip) {}

  /// @brief Reset the sequence to the beginning
  void Reset() {
    seed_ = 0;
    seed_save_ = -1;
  }

  /// @brief Sample into a 1D buffer (vector)
  /// @tparam Buffer TensorLike type (must be 1D)
  /// @param buffer Output buffer for samples
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 1)
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    // Sample m points in 1D
    for (int i = 0; i < m; ++i) {
      SampleSobol(1, &buffer[i]);
    }
  }

  /// @brief Sample into a 2D buffer (matrix)
  /// @tparam Buffer TensorLike type (must be 2D)
  /// @param buffer Output buffer for samples (matrix)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 2)
  void Sample(Buffer& buffer) {
    if constexpr (std::is_same_v<DefaultLayout, LayoutLeft>) {
      const int m = buffer.GetExtent(0);  // Number of samples
      const int n = buffer.GetExtent(1);  // Dimension
      // Sample m points in n dimensions
      // Store in column-major: data[i + j * m] is sample i, dimension j
      DVector<T> temp(n);
      for (int i = 0; i < m; ++i) {
        SampleSobol(n, temp.GetData());
        for (int j = 0; j < n; ++j) {
          buffer(i, j) = temp[j];
        }
      }
    } else {
      const int m = buffer.GetExtent(1);  // Dimension
      const int n = buffer.GetExtent(0);  // Number of Samples
      // Sample n points in m dimensions
      // Store in row-major: data[i * n + j] is sample i, dimension j
      DVector<T> temp(m);
      for (int i = 0; i < n; ++i) {
        SampleSobol(m, temp.GetData());
        for (int j = 0; j < m; ++j) {
          buffer(i, j) = temp[j];
        }
      }
    }
  }

 private:
  int64_t seed_;
  bool initialized_ = false;
  int dim_save_ = 0;
  int64_t lastq_[kSobolDimMax2] = {};
  int64_t maxcol_ = 0;
  double recipd_ = 0.0;
  int64_t seed_save_ = -1;
  int64_t v_[kSobolDimMax2][kSobolLogMax] = {};
};

}  // namespace asc

#endif  // ASC_SOBOL_H_
