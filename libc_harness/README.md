# symcc_libc_preload
Implementation of `qsort` and `bsearch` to compile with symcc and LD_PRELOAD over libc to avoid symcc crashing.

`symcc` does not (and cannot) support calling uninstrumented functions (e.g. in libc) with callbacks in instrumented code. If this happens,
the instrumented callback will assume it was called from instrumented code with the symbolic state setup correctly, but because the calling function
is uninstrumented, it will access stale symbolic arguments (and a myriad of things can happen).

Since `libc++` is expected to be compiled with symcc, the only really common offenders are in libc (which is usually dynamically linked in).
Since this issue affects only functions that take callback functions as arguments, the only two offenders i've seen so far are `bsearch` and `qsort`.

Separately, another big offender is `zlib` with its architecture of passing callbacks (e.g. via the `zalloc`, `zfree` fields in the state). However, that one is easy to just compile yourself and statically link in. To simplify vulnerability discovery, you can use [zlib-nop](https://github.com/Lukas-Dresel/zlib-nop) to get a drop-in replacement which does not do any actual compression (to keep constraints similar and focus exploration on actual logic).
