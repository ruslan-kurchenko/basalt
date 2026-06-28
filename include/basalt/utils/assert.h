/**
BSD 3-Clause License

This file is part of the Basalt project.
https://gitlab.com/VladyslavUsenko/basalt-headers.git

Copyright (c) 2019, Vladyslav Usenko and Nikolaus Demmel.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

@file
@brief Assertions used in the project

================================ DOR FORK PATCH (T2b crash-guard) =============

This file SHADOWS basalt-headers' `include/basalt/utils/assert.h`. The Basalt
fork's own `include/` directory is added to the `basalt` target via
`target_include_directories(basalt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)`,
which CMake orders BEFORE the `basalt::basalt-headers` linked-target interface
include directories. `#include <basalt/utils/assert.h>` therefore resolves to
THIS file when compiling libbasalt, not the upstream basalt-headers copy.

The ONLY behavioral change vs. upstream is in the four helper functions
(`assertionFailed`, `assertionFailedMsg`, `logFatal`, `logFatalMsg`): upstream
prints to std::cerr and calls std::abort() — which SIGABRTs the whole flight
binary on a numerical failure inside the VIO estimator. This shadow instead
THROWS a recoverable `basalt::AssertionError` (a std::runtime_error subclass).
The DOR Basalt driver catches it inside the estimator's processing thread,
flags an error state, and triggers a clean VIO reset + re-bootstrap instead of
a process abort.

The macro set, signatures, include guard (`#pragma once`), and the
`BASALT_DISABLE_ASSERTS` / release-unconditional semantics are byte-identical to
upstream — only the abort is replaced by a throw, so any code that relied on a
failed BASALT_ASSERT not returning (the `noreturn` contract) is unaffected: a
throw also never returns to the caller.
*/
#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace basalt {

/// Recoverable Basalt assertion failure (DOR T2b crash-guard). Thrown by the
/// shadowed BASALT_ASSERT* / BASALT_LOG_FATAL* helpers below in place of
/// std::abort(). Derives from std::runtime_error so the estimator processing
/// thread can catch it as `const std::exception&`.
class AssertionError : public std::runtime_error {
 public:
  explicit AssertionError(const std::string& what_arg)
      : std::runtime_error(what_arg) {}
};

#define UNUSED(x) (void)(x)
#define BASALT_ATTRIBUTE_NORETURN __attribute__((noreturn))

inline BASALT_ATTRIBUTE_NORETURN void assertionFailed(char const* expr,
                                                      char const* function,
                                                      char const* file,
                                                      long line) {
  std::ostringstream oss;
  oss << "Assertion (" << expr << ") failed in " << function << ":\n"
      << file << ':' << line << ":";
  throw ::basalt::AssertionError(oss.str());
}

inline BASALT_ATTRIBUTE_NORETURN void assertionFailedMsg(char const* expr,
                                                         char const* msg,
                                                         char const* function,
                                                         char const* file,
                                                         long line) {
  std::ostringstream oss;
  oss << "Assertion (" << expr << ") failed in " << function << ":\n"
      << file << ':' << line << ": " << msg;
  throw ::basalt::AssertionError(oss.str());
}

inline BASALT_ATTRIBUTE_NORETURN void logFatal(char const* function,
                                               char const* file, long line) {
  std::ostringstream oss;
  oss << "Fatal error in " << function << ":\n" << file << ':' << line << ":";
  throw ::basalt::AssertionError(oss.str());
}

inline BASALT_ATTRIBUTE_NORETURN void logFatalMsg(char const* msg,
                                                  char const* function,
                                                  char const* file, long line) {
  std::ostringstream oss;
  oss << "Fatal error in " << function << ":\n"
      << file << ':' << line << ": " << msg;
  throw ::basalt::AssertionError(oss.str());
}

}  // namespace basalt

#define BASALT_LIKELY(x) __builtin_expect(x, 1)

#if defined(BASALT_DISABLE_ASSERTS)

#define BASALT_ASSERT(expr) ((void)0)

#define BASALT_ASSERT_MSG(expr, msg) ((void)0)

#define BASALT_ASSERT_STREAM(expr, msg) ((void)0)

#else

#define BASALT_ASSERT(expr)                                              \
  (BASALT_LIKELY(!!(expr))                                               \
       ? ((void)0)                                                       \
       : ::basalt::assertionFailed(#expr, __PRETTY_FUNCTION__, __FILE__, \
                                   __LINE__))

#define BASALT_ASSERT_MSG(expr, msg)                                   \
  (BASALT_LIKELY(!!(expr))                                             \
       ? ((void)0)                                                     \
       : ::basalt::assertionFailedMsg(#expr, msg, __PRETTY_FUNCTION__, \
                                      __FILE__, __LINE__))

#define BASALT_ASSERT_STREAM(expr, msg)                                   \
  (BASALT_LIKELY(!!(expr))                                                \
       ? ((void)0)                                                        \
       : (std::cerr << msg << std::endl,                                  \
          ::basalt::assertionFailed(#expr, __PRETTY_FUNCTION__, __FILE__, \
                                    __LINE__)))

#endif

#define BASALT_LOG_FATAL(msg) \
  ::basalt::logFatalMsg(msg, __PRETTY_FUNCTION__, __FILE__, __LINE__)

#define BASALT_LOG_FATAL_STREAM(msg) \
  (std::cerr << msg << std::endl,    \
   ::basalt::logFatal(__PRETTY_FUNCTION__, __FILE__, __LINE__))
