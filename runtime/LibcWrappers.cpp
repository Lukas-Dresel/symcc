// This file is part of SymCC.
//
// SymCC is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// SymCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// SymCC. If not, see <https://www.gnu.org/licenses/>.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Config.h"
#include "Shadow.h"
#include <Runtime.h>

#define SYM(x) x##_symbolized

namespace {

/// The file descriptor referring to the symbolic input.
int inputFileDescriptor = -1;

/// The current position in the (symbolic) input.
uint64_t inputOffset = 0;

/// Tell the solver to try an alternative value than the given one.
template <typename V, typename F>
void tryAlternative(V value, SymExpr valueExpr, F caller) {
  if (valueExpr) {
    _sym_push_path_constraint(
        _sym_build_equal(valueExpr,
                         _sym_build_integer(value, sizeof(value) * 8)),
        true, reinterpret_cast<uintptr_t>(caller));
  }
}

// A partial specialization for pointer types for convenience.
template <typename E, typename F>
void tryAlternative(E *value, SymExpr valueExpr, F caller) {
  tryAlternative(reinterpret_cast<intptr_t>(value), valueExpr, caller);
}

void maybeSetInputFile(const char *path, int fd) {
  auto *fileInput = std::get_if<FileInput>(&g_config.input);
  if (fileInput == nullptr)
    return;

  if (strstr(path, fileInput->fileName.c_str()) == nullptr)
    return;

  if (inputFileDescriptor != -1)
    std::cerr << "Warning: input file opened multiple times; this is not yet "
                 "supported"
              << std::endl;

  inputFileDescriptor = fd;
  inputOffset = 0;
}

} // namespace

void initLibcWrappers() {
  if (std::holds_alternative<StdinInput>(g_config.input)) {
    // Symbolic data comes from standard input.
    inputFileDescriptor = 0;
  }
}

extern "C" {

void *SYM(malloc)(size_t size) {
  auto *result = malloc(size);

  _sym_concretize_size(_sym_get_parameter_expression(0), size, (uintptr_t)SYM(malloc));

  _sym_set_return_expression(nullptr);
  return result;
}

void *SYM(calloc)(size_t nmemb, size_t size) {
  auto *result = calloc(nmemb, size);

  _sym_concretize_size(_sym_get_parameter_expression(0), nmemb, (uintptr_t)SYM(calloc));
  _sym_concretize_size(_sym_get_parameter_expression(1), size, (uintptr_t)SYM(calloc));

  _sym_set_return_expression(nullptr);
  return result;
}

// See comment on lseek and lseek64 below; the same applies to the "off"
// parameter of mmap.

void *SYM(mmap64)(void *addr, size_t len, int prot, int flags, int fildes,
                  uint64_t off) {
  auto *result = mmap64(addr, len, prot, flags, fildes, off);

  _sym_concretize_size(_sym_get_parameter_expression(1), len, (uintptr_t)SYM(mmap64));

  _sym_set_return_expression(nullptr);
  return result;
}

void *SYM(mmap)(void *addr, size_t len, int prot, int flags, int fildes,
                uint32_t off) {
  return SYM(mmap64)(addr, len, prot, flags, fildes, off);
}

int SYM(open)(const char *path, int oflag, mode_t mode) {
  auto result = open(path, oflag, mode);
  _sym_set_return_expression(nullptr);

  if (result >= 0)
    maybeSetInputFile(path, result);

  return result;
}

ssize_t SYM(read)(int fildes, void *buf, size_t nbyte) {
  _sym_concretize_pointer(_sym_get_parameter_expression(1), buf, (uintptr_t)SYM(read));
  _sym_concretize_size(_sym_get_parameter_expression(2), nbyte, (uintptr_t)SYM(read));

  auto result = read(fildes, buf, nbyte);
  _sym_set_return_expression(nullptr);

  if (result < 0)
    return result;

  if (fildes == inputFileDescriptor) {
    // Reading symbolic input.
    _sym_make_symbolic(buf, result, inputOffset);
    inputOffset += result;
  } else if (!isConcrete(buf, result)) {
    ReadWriteShadow shadow(buf, result);
    std::fill(shadow.begin(), shadow.end(), nullptr);
  }

  return result;
}

// lseek is a bit tricky because, depending on preprocessor macros, glibc
// defines it to be a function operating on 32-bit values or aliases it to
// lseek64. Therefore, we cannot know in general whether calling lseek in our
// code takes a 32 or a 64-bit offset and whether it returns a 32 or a 64-bit
// result. In fact, since we compile this library against LLVM which requires us
// to compile with "-D_FILE_OFFSET_BITS=64", we happen to know that, for us,
// lseek is an alias to lseek64, but this may change any time. More importantly,
// client code may call one or the other, depending on its preprocessor
// definitions.
//
// Therefore, we define symbolic versions of both lseek and lseek64, but
// internally we only use lseek64 because it's the only one on whose
// availability we can rely.

uint64_t SYM(lseek64)(int fd, uint64_t offset, int whence) {
  auto result = lseek64(fd, offset, whence);
  _sym_set_return_expression(nullptr);
  if (result == (off_t)-1)
    return result;

  if (whence == SEEK_SET)
    _sym_set_return_expression(_sym_get_parameter_expression(1));

  if (fd == inputFileDescriptor)
    inputOffset = result;

  return result;
}

uint32_t SYM(lseek)(int fd, uint32_t offset, int whence) {
  uint64_t result = SYM(lseek64)(fd, offset, whence);

  // Perform the same overflow check as glibc in the 32-bit version of lseek.

  auto result32 = (uint32_t)result;
  if (result == result32)
    return result32;

  errno = EOVERFLOW;
  return (uint32_t)-1;
}

FILE *SYM(fopen)(const char *pathname, const char *mode) {
  auto *result = fopen(pathname, mode);
  _sym_set_return_expression(nullptr);

  if (result != nullptr)
    maybeSetInputFile(pathname, fileno(result));

  return result;
}

FILE *SYM(fopen64)(const char *pathname, const char *mode) {
  auto *result = fopen64(pathname, mode);
  _sym_set_return_expression(nullptr);

  if (result != nullptr)
    maybeSetInputFile(pathname, fileno(result));

  return result;
}

size_t SYM(fread)(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), ptr, (uintptr_t)SYM(fread));
  _sym_concretize_size(_sym_get_parameter_expression(1), size, (uintptr_t)SYM(fread));
  _sym_concretize_size(_sym_get_parameter_expression(2), nmemb, (uintptr_t)SYM(fread));

  auto result = fread(ptr, size, nmemb, stream);
  _sym_set_return_expression(nullptr);

  if (fileno(stream) == inputFileDescriptor) {
    // Reading symbolic input.
    _sym_make_symbolic(ptr, result * size, inputOffset);
    inputOffset += result * size;
  } else if (!isConcrete(ptr, result * size)) {
    ReadWriteShadow shadow(ptr, result * size);
    std::fill(shadow.begin(), shadow.end(), nullptr);
  }

  return result;
}

char *SYM(fgets)(char *str, int n, FILE *stream) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), str, (uintptr_t)SYM(fgets));
  _sym_concretize_size(_sym_get_parameter_expression(1), n, (uintptr_t)SYM(fgets));

  auto result = fgets(str, n, stream);
  _sym_set_return_expression(_sym_get_parameter_expression(0));

  if (fileno(stream) == inputFileDescriptor) {
    // Reading symbolic input.
    const auto length = sizeof(char) * strlen(str);
    _sym_make_symbolic(str, length, inputOffset);
    inputOffset += length;
  } else if (!isConcrete(str, sizeof(char) * strlen(str))) {
    ReadWriteShadow shadow(str, sizeof(char) * strlen(str));
    std::fill(shadow.begin(), shadow.end(), nullptr);
  }

  return result;
}

void SYM(rewind)(FILE *stream) {
  rewind(stream);
  _sym_set_return_expression(nullptr);

  if (fileno(stream) == inputFileDescriptor) {
    inputOffset = 0;
  }
}

int SYM(fseek)(FILE *stream, long offset, int whence) {
  _sym_concretize_size(_sym_get_parameter_expression(1), offset, (uintptr_t)SYM(fseek));

  auto result = fseek(stream, offset, whence);
  _sym_set_return_expression(nullptr);
  if (result == -1)
    return result;

  if (fileno(stream) == inputFileDescriptor) {
    auto pos = ftell(stream);
    if (pos == -1)
      return -1;
    inputOffset = pos;
  }

  return result;
}

int SYM(fseeko)(FILE *stream, off_t offset, int whence) {
  _sym_concretize_size(_sym_get_parameter_expression(1), offset, (uintptr_t)SYM(fseeko));

  auto result = fseeko(stream, offset, whence);
  _sym_set_return_expression(nullptr);
  if (result == -1)
    return result;

  if (fileno(stream) == inputFileDescriptor) {
    auto pos = ftello(stream);
    if (pos == -1)
      return -1;
    inputOffset = pos;
  }

  return result;
}

int SYM(fseeko64)(FILE *stream, uint64_t offset, int whence) {
  _sym_concretize_size(_sym_get_parameter_expression(1), offset, (uintptr_t)SYM(fseeko64));

  auto result = fseeko64(stream, offset, whence);
  _sym_set_return_expression(nullptr);
  if (result == -1)
    return result;

  if (fileno(stream) == inputFileDescriptor) {
    auto pos = ftello64(stream);
    if (pos == -1)
      return -1;
    inputOffset = pos;
  }

  return result;
}

int SYM(getc)(FILE *stream) {
  auto result = getc(stream);
  if (result == EOF) {
    _sym_set_return_expression(nullptr);
    return result;
  }

  if (fileno(stream) == inputFileDescriptor)
    _sym_set_return_expression(_sym_build_zext(
        _sym_get_input_byte(inputOffset++, result), sizeof(int) * 8 - 8));
  else
    _sym_set_return_expression(nullptr);

  return result;
}

int SYM(fgetc)(FILE *stream) {
  auto result = fgetc(stream);
  if (result == EOF) {
    _sym_set_return_expression(nullptr);
    return result;
  }

  if (fileno(stream) == inputFileDescriptor)
    _sym_set_return_expression(_sym_build_zext(
        _sym_get_input_byte(inputOffset++, result), sizeof(int) * 8 - 8));
  else
    _sym_set_return_expression(nullptr);

  return result;
}

int SYM(getchar)(void) { return SYM(getc)(stdin); }

int SYM(ungetc)(int c, FILE *stream) {
  auto result = ungetc(c, stream);
  _sym_set_return_expression(_sym_get_parameter_expression(0));

  if (fileno(stream) == inputFileDescriptor && result != EOF)
    inputOffset--;

  return result;
}

void *SYM(memcpy)(void *dest, const void *src, size_t n) {
  auto *result = memcpy(dest, src, n);

  auto sym_dest = _sym_get_parameter_expression(0);
  auto sym_src = _sym_get_parameter_expression(1);
  auto sym_n = _sym_get_parameter_expression(2);

  _sym_memcpy(
      sym_dest, sym_src, sym_n,
      static_cast<uint8_t *>(dest), static_cast<const uint8_t *>(src), n);
  _sym_set_return_expression(sym_dest);
  return result;
}

void *SYM(memset)(void *s, int c, size_t n) {
  auto *result = memset(s, c, n);

  auto sym_dest = _sym_get_parameter_expression(0);
  auto sym_c = _sym_get_parameter_expression(1);
  auto sym_n = _sym_get_parameter_expression(2);

  _sym_memset(sym_dest, sym_c, sym_n,
              static_cast<uint8_t *>(s), c, n);
  _sym_set_return_expression(sym_dest);
  return result;
}

void *SYM(memmove)(void *dest, const void *src, size_t n) {
  auto sym_dest = _sym_get_parameter_expression(0);
  auto sym_src = _sym_get_parameter_expression(1);
  auto sym_n = _sym_get_parameter_expression(2);

  auto *result = memmove(dest, src, n);
  _sym_memmove(sym_dest, sym_src, sym_n,
               static_cast<uint8_t *>(dest), static_cast<const uint8_t *>(src), n
  );

  _sym_set_return_expression(sym_dest);
  return result;
}

char *SYM(strncpy)(char *dest, const char *src, size_t n) {
  auto sym_dest = _sym_get_parameter_expression(0);
  auto sym_src = _sym_get_parameter_expression(1);
  auto sym_n = _sym_get_parameter_expression(2);

  _sym_concretize_pointer(sym_dest, dest, (uintptr_t)SYM(strncpy));
  _sym_concretize_pointer(sym_src, src, (uintptr_t)SYM(strncpy));
  _sym_concretize_size(sym_n, n, (uintptr_t)SYM(strncpy));

  auto *result = strncpy(dest, src, n);
  _sym_set_return_expression(nullptr);

  size_t srcLen = strnlen(src, n);
  size_t copied = std::min(n, srcLen);
  if (isConcrete(src, copied) && isConcrete(dest, n))
    return result;

  auto srcShadow = ReadOnlyShadow(src, copied);
  auto destShadow = ReadWriteShadow(dest, n);

  std::copy(srcShadow.begin(), srcShadow.end(), destShadow.begin());
  if (copied < n) {
    ReadWriteShadow destRestShadow(dest + copied, n - copied);
    std::fill(destRestShadow.begin(), destRestShadow.end(), nullptr);
  }

  return result;
}

const char *SYM(strchr)(const char *s, int c) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), s, (uintptr_t)SYM(strchr));
  _sym_concretize_size(_sym_get_parameter_expression(1), c, (uintptr_t)SYM(strchr));

  auto *result = strchr(s, c);
  _sym_set_return_expression(nullptr);

  auto *cExpr = _sym_get_parameter_expression(1);
  if (isConcrete(s, result != nullptr ? (result - s) : strlen(s)) &&
      cExpr == nullptr)
    return result;

  if (cExpr == nullptr)
    cExpr = _sym_build_integer(c, 8);
  else
    cExpr = _sym_build_trunc(cExpr, 8);

  size_t length = result != nullptr ? (result - s) : strlen(s);
  auto shadow = ReadOnlyShadow(s, length);
  auto shadowIt = shadow.begin();
  for (size_t i = 0; i < length; i++) {
    _sym_push_path_constraint(
        _sym_build_not_equal(
            (*shadowIt != nullptr) ? *shadowIt : _sym_build_integer(s[i], 8),
            cExpr),
        /*taken*/ 1, reinterpret_cast<uintptr_t>(SYM(strchr)));
    ++shadowIt;
  }

  return result;
}

int SYM(memcmp)(const void *a, const void *b, size_t n) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), a, (uintptr_t)SYM(memcmp));
  _sym_concretize_pointer(_sym_get_parameter_expression(1), b, (uintptr_t)SYM(memcmp));
  _sym_concretize_size(_sym_get_parameter_expression(2), n, (uintptr_t)SYM(memcmp));

  auto result = memcmp(a, b, n);
  _sym_set_return_expression(nullptr);

  if (isConcrete(a, n) && isConcrete(b, n))
    return result;

  auto aShadowIt = ReadOnlyShadow(a, n).begin_non_null();
  auto bShadowIt = ReadOnlyShadow(b, n).begin_non_null();
  auto *allEqual = _sym_build_equal(*aShadowIt, *bShadowIt);
  for (size_t i = 1; i < n; i++) {
    ++aShadowIt;
    ++bShadowIt;
    allEqual =
        _sym_build_bool_and(allEqual, _sym_build_equal(*aShadowIt, *bShadowIt));
  }

  _sym_push_path_constraint(allEqual, result == 0,
                            reinterpret_cast<uintptr_t>(SYM(memcmp)));
  return result;
}

int SYM(strncmp)(const char *a, const char *b, size_t n) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), a, (uintptr_t)SYM(strncmp));
  _sym_concretize_pointer(_sym_get_parameter_expression(1), b, (uintptr_t)SYM(strncmp));
  _sym_concretize_size(_sym_get_parameter_expression(2), n, (uintptr_t)SYM(strncmp));

  auto result = strncmp(a, b, n);
  _sym_set_return_expression(nullptr);

  auto real_len = std::min(n, std::min(strlen(a) + 1, strlen(b) + 1));

  if (isConcrete(a, real_len) && isConcrete(b, real_len))
    return result;

  auto aShadowIt = ReadOnlyShadow(a, n).begin_non_null();
  auto bShadowIt = ReadOnlyShadow(b, n).begin_non_null();
  auto *allEqual = _sym_build_equal(*aShadowIt, *bShadowIt);
  for (size_t i = 1; i < real_len; i++) {
    ++aShadowIt;
    ++bShadowIt;
    allEqual =
        _sym_build_bool_and(allEqual, _sym_build_equal(*aShadowIt, *bShadowIt));
  }

  _sym_push_path_constraint(allEqual, result == 0,
                            reinterpret_cast<uintptr_t>(SYM(strncmp)));
  return result;
}

int SYM(strncasecmp)(const char* a, const char* b, size_t n) {
  _sym_concretize_pointer(_sym_get_parameter_expression(0), a, (uintptr_t)SYM(strncasecmp));
  _sym_concretize_pointer(_sym_get_parameter_expression(1), b, (uintptr_t)SYM(strncasecmp));
  _sym_concretize_size(_sym_get_parameter_expression(2), n, (uintptr_t)SYM(strncasecmp));

  auto result = strncasecmp(a, b, n);
  _sym_set_return_expression(nullptr);

  auto real_len = std::min(n, std::min(strlen(a) + 1, strlen(b) + 1));

  if (isConcrete(a, real_len) && isConcrete(b, real_len))
    return result;

  auto aShadowIt = ReadOnlyShadow(a, n).begin_non_null();
  auto bShadowIt = ReadOnlyShadow(b, n).begin_non_null();
  auto *allEqual = _sym_build_equal(*aShadowIt, *bShadowIt);
  for (size_t i = 1; i < real_len; i++) {
    ++aShadowIt;
    ++bShadowIt;

    auto a_is_case_sensitive_ascii = _sym_build_or(
      _sym_build_bool_and(
        _sym_build_unsigned_greater_than(*aShadowIt, _sym_build_integer('A', 8)),
        _sym_build_unsigned_less_than(*aShadowIt, _sym_build_integer('Z', 8))
      ),
      _sym_build_bool_and(
        _sym_build_unsigned_greater_than(*aShadowIt, _sym_build_integer('a', 8)),
        _sym_build_unsigned_less_than(*aShadowIt, _sym_build_integer('z', 8))
      )
    );

    auto lowercase_a_expr = _sym_build_and(
      *aShadowIt,
      _sym_build_integer(~0x20, 8)
    );
    auto lowercase_b_expr = _sym_build_and(
      *bShadowIt,
      _sym_build_integer(~0x20, 8)
    );
    auto character_is_equivalent = _sym_build_bool_or(
      // either a is not in the ascii range, then it must be strictly equal
      _sym_build_equal(*aShadowIt, *bShadowIt),
      // or a is in the ascii range, then we need to check if its lowercase equivalent is equal to b's
      _sym_build_and(
        a_is_case_sensitive_ascii,
        _sym_build_equal(lowercase_a_expr, lowercase_b_expr)
      )
    );
    allEqual =
        _sym_build_bool_and(allEqual, character_is_equivalent);
  }

  _sym_push_path_constraint(allEqual, result == 0,
                            reinterpret_cast<uintptr_t>(SYM(strncasecmp)));
  return result;
}


int SYM(strcmp)(const char *a, const char *b) {
  auto len = strlen(b) + 1;
  return SYM(strncmp)(a, b, len);
}

uint32_t SYM(ntohl)(uint32_t netlong) {
  auto netlongExpr = _sym_get_parameter_expression(0);
  auto result = ntohl(netlong);

  if (netlongExpr == nullptr) {
    _sym_set_return_expression(nullptr);
    return result;
  }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  _sym_set_return_expression(_sym_build_bswap(netlongExpr));
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  _sym_set_return_expression(netlongExpr);
#else
#error Unsupported __BYTE_ORDER__
#endif

  return result;
}
}
