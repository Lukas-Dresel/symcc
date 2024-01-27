// Rust Run-time library interface                                  -*- C++ -*-
//
// This header is mostly a straight copy of RuntimeCommon.h with different
// function name prefixes, a separate SymExpr type and all functions that are
// implemented by this wrapper removed. This file defines the interface that the
// wrapped runtime should implement. Consult the README for a high-level
// overview.
//
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

#ifndef RUSTRUNTIME_H
#define RUSTRUNTIME_H

#include <stddef.h>

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

typedef uintptr_t RSymExpr;

/*
 * Construction of simple values
 */
RSymExpr _rsym_build_integer(uint64_t value, uint8_t bits);
RSymExpr _rsym_build_integer128(uint64_t high, uint64_t low);
RSymExpr _rsym_build_float(double value, bool is_double);
RSymExpr _rsym_build_null_pointer(void);
RSymExpr _rsym_build_true(void);
RSymExpr _rsym_build_false(void);
RSymExpr _rsym_build_bool(bool value);

/*
 * Arithmetic and shifts
 */
RSymExpr _rsym_build_neg(RSymExpr expr);
RSymExpr _rsym_build_add(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_sub(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_mul(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_div(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_signed_div(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_rem(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_signed_rem(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_shift_left(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_logical_shift_right(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_arithmetic_shift_right(RSymExpr a, RSymExpr b);

RSymExpr _rsym_build_fp_add(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_fp_sub(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_fp_mul(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_fp_div(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_fp_rem(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_fp_abs(RSymExpr a);

/*
 * Boolean operations
 */
RSymExpr _rsym_build_not(RSymExpr expr);
RSymExpr _rsym_build_signed_less_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_signed_less_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_signed_greater_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_signed_greater_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_less_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_less_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_greater_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_unsigned_greater_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_not_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_bool_and(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_and(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_bool_or(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_or(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_bool_xor(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_xor(RSymExpr a, RSymExpr b);

RSymExpr _rsym_build_float_ordered_greater_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered_greater_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered_less_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered_less_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered_not_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_ordered(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_greater_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_greater_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_less_than(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_less_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_equal(RSymExpr a, RSymExpr b);
RSymExpr _rsym_build_float_unordered_not_equal(RSymExpr a, RSymExpr b);

/*
 * Casts
 */
RSymExpr _rsym_build_sext(RSymExpr expr, uint8_t bits);
RSymExpr _rsym_build_zext(RSymExpr expr, uint8_t bits);
RSymExpr _rsym_build_trunc(RSymExpr expr, uint8_t bits);
RSymExpr _rsym_build_int_to_float(RSymExpr value, bool is_double,
                                  bool is_signed);
RSymExpr _rsym_build_float_to_float(RSymExpr expr, bool to_double);
RSymExpr _rsym_build_bits_to_float(RSymExpr expr, bool to_double);
RSymExpr _rsym_build_float_to_bits(RSymExpr expr);
RSymExpr _rsym_build_float_to_signed_integer(RSymExpr expr, uint8_t bits);
RSymExpr _rsym_build_float_to_unsigned_integer(RSymExpr expr, uint8_t bits);
RSymExpr _rsym_build_bool_to_bit(RSymExpr expr);

/*
 * Bit-array helpers
 */
RSymExpr _rsym_concat_helper(RSymExpr a, RSymExpr b);
RSymExpr _rsym_extract_helper(RSymExpr expr, size_t first_bit, size_t last_bit);

/*
 * Backend memory operations (if you want to implement a high-level symbolic memory on top of the concrete trace)
 */
/// @brief Causes a backend memory read, returning a new symbolic expression for the read value
/// @param addr_expr
/// @param concolic_read_value this is the value the concolic execution would return for this read, the default
///                            implementation, should simply return this, however, you can instead choose to return
///                            e.g. fresh values for reads
/// @param addr                the concrete address of this read
/// @param length              the length of this read (symbolic reads cannot occur here, they can only occur e.g. in memset)
/// @param little_endian       the endianness of this value (`concolic_read_value` already complies with this)
/// @return a SymExpr representing the result of the read. If you don't want to implement a more complex memory model,
///         just return `concolic_read_value` here
RSymExpr _rsym_backend_read_memory(
    RSymExpr addr_expr, RSymExpr concolic_read_value,
    uint8_t* addr, size_t length, bool little_endian
);
/// @brief Registers a symbolic write in the backend, again, symbolic sizes here are not supported, see, e.g., the
///        `memset` special case below for cases where symbolic sizes are allowed
/// @param symbolic_addr_expr the symbolic address expression, or nullptr if concrete
/// @param written_expr       the value being written
/// @param concrete_addr      the concrete address of this write
/// @param concrete_length    the concrete length of this write
/// @param little_endian      the endianness of this write
// TODO: consider adding the concrete value being written here for the symbolic tracking if written_expr is NULL?
void _rsym_backend_write_memory(
    RSymExpr symbolic_addr_expr, RSymExpr written_expr,
    uint8_t *concrete_addr, size_t concrete_length, bool little_endian
);

void _rsym_backend_memcpy(
    RSymExpr sym_dest, RSymExpr sym_src, RSymExpr sym_len,
    uint8_t* dest, const uint8_t* src, size_t length
);
void _rsym_backend_memset(
    RSymExpr sym_dest, RSymExpr sym_val, RSymExpr sym_len,
    uint8_t *memory, int value, size_t length
);
void _rsym_backend_memmove(
    RSymExpr sym_dest, RSymExpr sym_src, RSymExpr sym_len,
    uint8_t *dest, const uint8_t *src, size_t length
);
/*
 * Concretization
 */
void _rsym_concretize_pointer(RSymExpr value, uintptr_t ptr, uintptr_t site_id);
void _rsym_concretize_size(RSymExpr value, size_t sz, uintptr_t site_id);

/*
 * Constraint handling
 */
void _rsym_push_path_constraint(RSymExpr constraint, bool taken,
                                uintptr_t site_id);
RSymExpr _rsym_get_input_byte(size_t offset, uint8_t value);

/*
 * Call-stack tracing
 */
void _rsym_notify_call(uintptr_t site_id);
void _rsym_notify_ret(uintptr_t site_id);
void _rsym_notify_basic_block(uintptr_t site_id);
void _rsym_notify_param_expr(uint8_t index, RSymExpr expr);
void _rsym_notify_ret_expr(RSymExpr expr);

/*
 * Garbage collection
 */
void _rsym_expression_unreachable(RSymExpr *expressions, size_t num_elements);

/*
 * Shadow memory access
 */
void _sym_get_memory_byte_expressions(SymExpr* out, void* addr, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
