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

#ifndef RUSTINTERFACE_H
#define RUSTINTERFACE_H

#include <stddef.h>

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

typedef uintptr_t RSymExpr;

/*
 * Shadow memory access
 */
void _sym_get_symbolic_exprs_for_memory(RSymExpr* out, void* addr, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
