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

#include <Runtime.h>

#include <iostream>
#include <array>
#include <cassert>
#include <numeric>

#include "GarbageCollection.h"
#include "RuntimeCommon.h"
#include "Shadow.h"

namespace {

constexpr int kMaxFunctionArguments = 256;

/// Global storage for function parameters and the return value.
SymExpr g_return_value;
std::array<SymExpr, kMaxFunctionArguments> g_function_arguments;
// TODO make thread-local

} // namespace

void _sym_set_return_expression(SymExpr expr) {
  // print out the expression
  if (expr) {
    printf("return expression: %p\n", expr);
    _sym_notify_ret_expr(expr);
  }
  g_return_value = expr;
}

SymExpr _sym_get_return_expression(void) {
  auto *result = g_return_value;
  // TODO this is a safeguard that can eventually be removed
  g_return_value = nullptr;
  return result;
}

void _sym_set_parameter_expression(uint8_t index, SymExpr expr) {
  _sym_notify_param_expr(index, expr);
  g_function_arguments[index] = expr;
}

SymExpr _sym_get_parameter_expression(uint8_t index) {
  return g_function_arguments[index];
}

void _sym_memcpy(
  SymExpr sym_dst, SymExpr sym_src, SymExpr sym_size,
  uint8_t *dest, const uint8_t *src, size_t length)
{
  bool symbolic_args = sym_dst || sym_src || sym_size;
  bool symbolic_data = !isConcrete(src, length) || !isConcrete(dest, length);
  if (symbolic_args || symbolic_data) {
    _sym_backend_memcpy(sym_dst, sym_src, sym_size, dest, src, length);
  }
  if (!symbolic_data)
    return;

  ReadOnlyShadow srcShadow(src, length);
  ReadWriteShadow destShadow(dest, length);
  std::copy(srcShadow.begin(), srcShadow.end(), destShadow.begin());
}

void _sym_memset(
  SymExpr sym_dst, SymExpr sym_value, SymExpr sym_size,
  uint8_t *memory, int value, size_t length
) {
  bool symbolic_args = sym_dst || sym_value || sym_size;
  bool symbolic_data = !isConcrete(memory, length);
  if (symbolic_args || symbolic_data) {
    _sym_backend_memset(sym_dst, sym_value, sym_size, memory, value, length);
  }
  if ((sym_value == nullptr) && !symbolic_data) {
    return;
  }

  ReadWriteShadow shadow(memory, length);
  std::fill(shadow.begin(), shadow.end(), sym_value);
}

void _sym_memmove(SymExpr sym_dst, SymExpr sym_src, SymExpr sym_size, uint8_t *dest, const uint8_t *src, size_t length)
{
  bool symbolic_args = sym_dst || sym_src || sym_size;
  bool symbolic_data = !isConcrete(src, length) || !isConcrete(dest, length);
  if (symbolic_args || symbolic_data) {
    _sym_backend_memmove(sym_dst, sym_src, sym_size, dest, src, length);
  }
  if (!symbolic_data)
    return;

  ReadOnlyShadow srcShadow(src, length);
  ReadWriteShadow destShadow(dest, length);
  if (dest > src)
    std::copy_backward(srcShadow.begin(), srcShadow.end(), destShadow.end());
  else
    std::copy(srcShadow.begin(), srcShadow.end(), destShadow.begin());
}

SymExpr _sym_read_memory(
  SymExpr symbolic_addr,
  uint8_t *addr, size_t length, bool little_endian) {
  assert(length && "Invalid query for zero-length memory region");

#ifdef DEBUG_RUNTIME
  std::cerr << "Reading " << length << " bytes from address " << P(addr)
            << std::endl;
  dump_known_regions();
#endif

  // If the entire memory region is concrete, don't create a symbolic expression
  // at all.
  bool symbolic_args = symbolic_addr != nullptr;
  bool symbolic_data = !isConcrete(addr, length);
  SymExpr read_value = nullptr;
  if (symbolic_data)
  {
    ReadOnlyShadow shadow(addr, length);
    read_value = std::accumulate(shadow.begin_non_null(), shadow.end_non_null(),
                          static_cast<SymExpr>(nullptr),
                          [&](SymExpr result, SymExpr byteExpr) {
                            if (result == nullptr)
                              return byteExpr;

                            return little_endian
                                        ? _sym_concat_helper(byteExpr, result)
                                        : _sym_concat_helper(result, byteExpr);
                          });
  }
  else {
    if (!symbolic_args) // the entire memory region is concrete and the address is concrete, exit without logging
      return nullptr;
  }
  assert(symbolic_addr != nullptr || read_value != nullptr);
  return _sym_backend_read_memory(symbolic_addr, read_value, addr, length, little_endian);
}

void _sym_write_memory( SymExpr symbolic_addr_expr, SymExpr written_expr,
                        uint8_t *concrete_addr, size_t concrete_length, bool little_endian) {
  assert(concrete_length && "Invalid query for zero-length memory region");

#ifdef DEBUG_RUNTIME
  std::cerr << "Writing " << length << " bytes to address " << P(addr)
            << std::endl;

  dump_known_regions();
#endif

  bool symbolic_args = symbolic_addr_expr != nullptr || written_expr != nullptr;
  bool symbolic_data = !isConcrete(concrete_addr, concrete_length);
  if (symbolic_data || symbolic_args) {
    _sym_backend_write_memory(symbolic_addr_expr, written_expr, concrete_addr, concrete_length, little_endian);
  }

  if (written_expr == nullptr && !symbolic_data) {
    return;
  }

  ReadWriteShadow shadow(concrete_addr, concrete_length);
  if (written_expr == nullptr) {
    std::fill(shadow.begin(), shadow.end(), nullptr);
  } else {
    size_t i = 0;
    for (SymExpr &byteShadow : shadow) {
      byteShadow = little_endian
                       ? _sym_extract_helper(written_expr, 8 * (i + 1) - 1, 8 * i)
                       : _sym_extract_helper(written_expr, (concrete_length - i) * 8 - 1,
                                             (concrete_length - i - 1) * 8);
      i++;
    }
  }
}

SymExpr _sym_build_extract(SymExpr expr, uint64_t offset, uint64_t length,
                           bool little_endian) {
  size_t totalBits = _sym_bits_helper(expr);
  assert((totalBits % 8 == 0) && "Aggregate type contains partial bytes");

  SymExpr result;
  if (little_endian) {
    result = _sym_extract_helper(expr, totalBits - offset * 8 - 1,
                                 totalBits - offset * 8 - 8);
    for (size_t i = 1; i < length; i++) {
      result = _sym_concat_helper(
          _sym_extract_helper(expr, totalBits - (offset + i) * 8 - 1,
                              totalBits - (offset + i + 1) * 8),
          result);
    }
  } else {
    result = _sym_extract_helper(expr, totalBits - offset * 8 - 1,
                                 totalBits - (offset + length) * 8);
  }

  return result;
}

SymExpr _sym_build_bswap(SymExpr expr) {
  size_t bits = _sym_bits_helper(expr);
  assert((bits % 16 == 0) && "bswap is not applicable");
  return _sym_build_extract(expr, 0, bits / 8, true);
}

SymExpr _sym_build_insert(SymExpr target, SymExpr to_insert, uint64_t offset,
                          bool little_endian) {
  size_t bitsToInsert = _sym_bits_helper(to_insert);
  assert((bitsToInsert % 8 == 0) &&
         "Expression to insert contains partial bytes");

  SymExpr beforeInsert =
      (offset == 0) ? nullptr : _sym_build_extract(target, 0, offset, false);
  SymExpr newPiece = little_endian ? _sym_build_bswap(to_insert) : to_insert;
  uint64_t afterLen =
      (_sym_bits_helper(target) / 8) - offset - (bitsToInsert / 8);
  SymExpr afterInsert =
      (afterLen == 0) ? nullptr
                      : _sym_build_extract(target, offset + (bitsToInsert / 8),
                                           afterLen, false);

  SymExpr result = beforeInsert;
  if (result == nullptr) {
    result = newPiece;
  } else {
    result = _sym_concat_helper(result, newPiece);
  }

  if (afterInsert != nullptr) {
    result = _sym_concat_helper(result, afterInsert);
  }

  return result;
}

void _sym_register_expression_region(SymExpr *start, size_t length) {
  registerExpressionRegion({start, length});
}
