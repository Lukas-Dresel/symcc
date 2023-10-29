#ifndef SYMBOLIZE_H
#define SYMBOLIZE_H

#ifdef NOT_SYMBOLIZED
#define SYM(x) x
#else
#define SYM(x) x##_symbolized
#endif

#endif // SYMBOLIZE_H