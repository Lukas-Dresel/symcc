# This file is part of SymCC.
#
# SymCC is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# SymCC is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# SymCC. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.5)
project(SymbolicCompiler)

option(QSYM_BACKEND "Use the Qsym backend instead of our own" OFF)
option(RUST_BACKEND "Build the support code required for a Rust backend as a static archive." OFF)
option(TARGET_32BIT "Make the compiler work correctly with -m32" OFF)

# take the path to the symcc_runtime_dir library from the environment if it is set
if (NOT SYM_RUNTIME_DIR)
  set(SYM_RUNTIME_DIR "$ENV{SYMCC_RUNTIME_DIR}")
endif()
if (NOT SYM_RUNTIME_DIR)
  set(SYM_RUNTIME_DIR "$ENV{SYM_RUNTIME_DIR}")
endif()

# We need to build the runtime as an external project because CMake otherwise
# doesn't allow us to build it twice with different options (one 32-bit version
# and one 64-bit variant).
include(ExternalProject)

set(SYM_RUNTIME_BUILD_ARGS
  -DCMAKE_AR=${CMAKE_AR}
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
  -DCMAKE_C_FLAGS_INIT=${CMAKE_C_FLAGS_INIT}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
  -DCMAKE_CXX_FLAGS_INIT=${CMAKE_CXX_FLAGS_INIT}
  -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
  -DCMAKE_EXE_LINKER_FLAGS_INIT=${CMAKE_EXE_LINKER_FLAGS_INIT}
  -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
  -DCMAKE_MODULE_LINKER_FLAGS=${CMAKE_MODULE_LINKER_FLAGS}
  -DCMAKE_MODULE_LINKER_FLAGS_INIT=${CMAKE_MODULE_LINKER_FLAGS_INIT}
  -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
  -DRUST_BACKEND=${RUST_BACKEND}
  -DCMAKE_SHARED_LINKER_FLAGS_INIT=${CMAKE_SHARED_LINKER_FLAGS_INIT}
  -DCMAKE_SYSROOT=${CMAKE_SYSROOT}
  -DQSYM_BACKEND=${QSYM_BACKEND}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DZ3_TRUST_SYSTEM_VERSION=${Z3_TRUST_SYSTEM_VERSION})




set(SYM_RUNTIME_BUILD_ARGS ${SYM_RUNTIME_BUILD_ARGS} -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/runtime)

ExternalProject_Add(SymRuntime
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/runtime
  CMAKE_ARGS
  ${SYM_RUNTIME_BUILD_ARGS}
  -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}
  -DZ3_DIR=${Z3_DIR}
  -DLLVM_DIR=${LLVM_DIR}
  INSTALL_COMMAND ""
  BUILD_ALWAYS TRUE)

if (NOT SYM_RUNTIME_DIR)
  message(STATUS "SYM_RUNTIME_DIR not set; building runtime as an external project.")

  ExternalProject_Get_Property(SymRuntime BINARY_DIR)
  set(SYM_RUNTIME_DIR ${BINARY_DIR})
endif()


if (${TARGET_32BIT})
  ExternalProject_Add(SymRuntime32
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/runtime
    CMAKE_ARGS
    ${SYM_RUNTIME_BUILD_ARGS}
    -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS} -m32"
    -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -m32"
    -DZ3_DIR=${Z3_32BIT_DIR}
    -DLLVM_DIR=${LLVM_32BIT_DIR}
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE)

  ExternalProject_Get_Property(SymRuntime32 BINARY_DIR)

  if (NOT SYM_RUNTIME_32BIT_DIR)
    set(SYM_RUNTIME_32BIT_DIR ${BINARY_DIR})
  endif()
endif()


find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake from ${LLVM_DIR}")

if (${LLVM_VERSION_MAJOR} LESS 8 OR ${LLVM_VERSION_MAJOR} GREATER 15)
  message(WARNING "The software has been developed for LLVM 8 through 15; \
it is unlikely to work with other versions!")
endif()

add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 \
-Wredundant-decls -Wcast-align -Wmissing-include-dirs -Wswitch-default \
-Wextra -Wall -Winvalid-pch -Wredundant-decls -Wformat=2 \
-Wmissing-format-attribute -Wformat-nonliteral -Werror -Wno-error=deprecated-declarations")

# Mark nodelete to work around unload bug in upstream LLVM 5.0+
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,nodelete")

# This is the compiler pass that we later load into clang or opt. If LLVM is
# built without RTTI we have to disable it for our library too, otherwise we'll
# get linker errors.
add_library(Symbolize MODULE
  compiler/Symbolizer.cpp
  compiler/Pass.cpp
  compiler/Runtime.cpp
  compiler/Main.cpp)
if (NOT LLVM_ENABLE_RTTI)
  set_target_properties(Symbolize PROPERTIES COMPILE_FLAGS "-fno-rtti")
endif()

find_program(CLANG_BINARY "clang"
  HINTS ${LLVM_TOOLS_BINARY_DIR}
  DOC "The clang binary to use in the symcc wrapper script.")
find_program(CLANGPP_BINARY "clang++"
  HINTS ${LLVM_TOOLS_BINARY_DIR}
  DOC "The clang binary to use in the sym++ wrapper script.")
if (NOT CLANG_BINARY)
  message(FATAL_ERROR "Clang not found; please make sure that the version corresponding to your LLVM installation is available.")
endif()

if (${LLVM_VERSION_MAJOR} LESS 13)
  set(CLANG_LOAD_PASS "-Xclang -load -Xclang ")
else()
  set(CLANG_LOAD_PASS "-fpass-plugin=")
endif()

configure_file("compiler/symcc.in" "symcc" @ONLY)
configure_file("compiler/sym++.in" "sym++" @ONLY)

# now that the compilers are built, use them to build the libc_harness using its Makefile
# to do so, we have to set the environment variables CC and CXX to the symcc wrappers and BUILD_DIR to our build
# directory
add_custom_target(libc_harness ALL
  COMMAND ${CMAKE_COMMAND} -E env SYMCC_RUNTIME_DIR=${SYM_RUNTIME_DIR} CC=${CMAKE_BINARY_DIR}/symcc CXX=${CMAKE_BINARY_DIR}/sym++ BUILD_DIR=${CMAKE_BINARY_DIR} SYMCC_NO_LIBC_HARNESS=1 SYMCC_NO_SYMBOLIC_INPUT=1 make -C ${CMAKE_SOURCE_DIR}/libc_harness
)
add_dependencies(libc_harness Symbolize)


# set the AFL_DIR if not already set from environment to `realpath ${CMAKE_SOURCE_DIR}/../AFLPlusplus`
if (NOT AFL_DIR)
  get_filename_component(AFL_DIR "${CMAKE_SOURCE_DIR}/../AFLPlusplus" REALPATH)
endif()
configure_file("compiler/symcc-afl-clang.in" "symcc-afl-clang" @ONLY)
configure_file("compiler/symcc-afl-clang++.in" "symcc-afl-clang++" @ONLY)
configure_file("compiler/afl-clang-fast.in" "afl-clang-fast" @ONLY)
configure_file("compiler/afl-clang-fast++.in" "afl-clang-fast++" @ONLY)


add_subdirectory(test)
