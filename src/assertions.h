#ifndef ASSERTIONS_H
#define ASSERTIONS_H

#include <cassert>

#ifdef NDEBUG
#define safely_assert(...) (void)(__VA_ARGS__)
#else
#define safely_assert(...) assert(__VA_ARGS__)
#endif

#define assert_not_reached() {                  \
        assert(false);                          \
        __builtin_unreachable();                \
    }

#endif
