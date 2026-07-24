// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/error.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_ERROR_H_
#define ASC_ERROR_H_

#include <iomanip>
#include <iostream>
#include <sstream>

#include "asc/core/globals.h"

#ifdef ASC_USE_EXCEPTION
#include <stdexcept>
#include <string>
#endif

namespace asc {

/// Action to take when ASC encounters an error.
enum ErrorAction {
  kErrorAbort = 0,  ///< Abort execution using abort().
                    ///< The default error action when the build option
                    ///< ASC_USE_EXCEPTION = NO.
  kErrorThrow       ///< Throw an ErrorException.
                    ///< The default error action when the build option
                    ///< ASC_USE_EXCEPTION = YES.
};

/// Set the action ASC takes when an error is encountered.
void SetErrorAction(ErrorAction action);

/// Get the action ASC takes when an error is encountered.
ErrorAction GetErrorAction();

#ifdef ASC_USE_EXCEPTION
/// @brief Exception class thrown when ASC encounters an error and the
/// current ErrorAction is set to kErrorThrow.
class ErrorException : public std::exception {
 private:
  std::string msg;

 public:
  explicit ErrorException(const std::string& in_msg) : msg(in_msg) {}
  virtual ~ErrorException() throw() {}
  virtual const char* what() const throw();
};
#endif

/// Function called when an error is encountered.
/// Used by the macros ASC_ABORT, ASC_ASSERT, ASC_VERIFY.
void Error(const char* msg = NULL);

/// Function called by the macro ASC_WARNING.
void Warning(const char* msg = NULL);

}  // namespace asc

#ifndef _ASC_FUNC_NAME
#ifndef _ASC_VER
// This is nice because it shows the class and method name
#define _ASC_FUNC_NAME __PRETTY_FUNCTION__
// This one is C99 standard.
// #define _ASC_FUNC_NAME __func__
#else
// for Visual Studio C++
#define _ASC_FUNC_NAME __FUNCSIG__
#endif
#endif

#define ASC_LOCATION                                                       \
  "\n ... in function: " << _ASC_FUNC_NAME                                 \
                         << "\n ... in file: " << __FILE__ << ':' << __LINE__ \
                         << '\n'

// Common error message and abort macro
#define _ASC_MESSAGE(msg, fn)                               \
  {                                                            \
    std::ostringstream msg_stream;                             \
    msg_stream << std::setprecision(16);                       \
    msg_stream << std::setiosflags(std::ios_base::scientific); \
    msg_stream << msg << ASC_LOCATION;                      \
    asc::fn(msg_stream.str().c_str());                      \
  }

// Outputs lots of useful information and aborts.
// For all of these functions, "msg" is pushed to an ostream, so you can
// write useful (if complicated) error messages instead of writing
// out to the screen first, then calling abort.  For example:
// ASC_ABORT( "Unknown geometry type: " << type );
#define ASC_ABORT(msg) _ASC_MESSAGE("ASC abort: " << msg, Error)

// Does a check, and then outputs lots of useful information if the test fails
#define ASC_VERIFY(x, msg)                                                  \
  if (!(x)) {                                                                  \
    _ASC_MESSAGE(                                                           \
        "Verification failed: (" << (x) << ") is false\n --> " << msg, Error); \
  }

// Use this if the only place your variable is used is in ASSERTs
// For example, this code snippet:
//   int info = remove(file);
//   ASC_CONTRACT_VAR(info);
//   ASC_ASSERT( info != 0, "Fail to remove file " << file );
#define ASC_CONTRACT_VAR(x) \
  if (false && (&x) + 1) {     \
  }

// Now set up some optional error, but only if the right flags are on
#ifdef ASC_DEBUG
#define ASC_ASSERT(x, msg)                                               \
  if (!(x)) {                                                               \
    _ASC_MESSAGE(                                                        \
        "Assertion failed: (" << (x) << ") is false\n --> " << msg, Error); \
  }
#else
#define ASC_ASSERT(x, msg)
#endif

// Generate a warning message - always generated, regardless of ASC_DEBUG.
#define ASC_WARNING(msg) _ASC_MESSAGE("ASC Warning: " << msg, Warning)

// Abort inside a device kernel
#if defined(__CUDA_ARCH__)
#define ASC_ABORT_KERNEL(...) \
  {                              \
    printf(__VA_ARGS__);         \
    asm("trap;");                \
  }
#else
#define ASC_ABORT_KERNEL(...) \
  {                              \
    printf(__VA_ARGS__);         \
    ASC_ABORT("");            \
  }
#endif

// Verify inside a device kernel
#define ASC_VERIFY_KERNEL(x, ...) \
  if (!(x)) {                        \
    ASC_ABORT_KERNEL(__VA_ARGS__) \
  }

// Assert inside a device kernel
#ifdef ASC_DEBUG
#define ASC_ASSERT_KERNEL(x, ...) \
  if (!(x)) {                        \
    ASC_ABORT_KERNEL(__VA_ARGS__) \
  }
#else
#define ASC_ASSERT_KERNEL(x, ...)
#endif

// Static asserts
#if (__cplusplus >= 201103L)
#define ASC_STATIC_ASSERT(x, msg) static_assert((x), msg)
#else
#define ASC_STATIC_ASSERT(x, msg)
#endif

// Check x >= xmin
#define ASC_STATIC_ASSERT_LOWER_BOUND(x, xmin) \
  ASC_STATIC_ASSERT((x >= xmin), #x " >= " #xmin " not satisfied")

// Check x > xmin
#define ASC_STATIC_ASSERT_STRICT_LOWER_BOUND(x, xmin) \
  ASC_STATIC_ASSERT((x > xmin), #x " > " #xmin " not satisfied")

// Check x <= xmax
#define ASC_STATIC_ASSERT_UPPER_BOUND(x, xmax) \
  ASC_STATIC_ASSERT((x <= xmax), #x " <= " #xmax " not satisfied")

// Check x < xmax
#define ASC_STATIC_ASSERT_STRICT_UPPER_BOUND(x, xmax) \
  ASC_STATIC_ASSERT((x < xmax), #x " < " #xmax " not satisfied")

// Check xmin <= x < xmax
#define ASC_STATIC_ASSERT_BOUND(x, xmin, xmax) \
  ASC_STATIC_ASSERT((x < xmax) && (x >= xmin), \
                       #xmin " <= " #x " < " #xmax " not satisfied")

// Check (x == a) or (x == b)
#define ASC_STATIC_ASSERT_BINARY_SWITCH(x, a, b) \
  ASC_STATIC_ASSERT((x == a) || (x == b), #x " must be " #a " or " #b);

#define ASC_STATIC_ASSERT_TRIVIAL_TYPE(T)         \
  ASC_STATIC_ASSERT(std::is_trivial<T>::value, #T \
                       " is not a trivial "          \
                       "type");

#endif  // ASC_ERROR_H_
