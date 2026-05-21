#ifndef TEST_H
#define TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int         g_test_pass;
extern int         g_test_fail;
extern int         g_current_failed;
extern const char* g_current_test;

#define ASSERT(cond)                                                                       \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            printf("    FAIL %s:%d in %s: %s\n", __FILE__, __LINE__, g_current_test, #cond); \
            g_current_failed = 1;                                                          \
            return;                                                                        \
        }                                                                                  \
    } while (0)

#define ASSERT_NEAR(a, b, eps)                                                                          \
    do {                                                                                                \
        double _ad = (double)(a), _bd = (double)(b), _ed = (double)(eps);                               \
        if (fabs(_ad - _bd) > _ed) {                                                                    \
            printf("    FAIL %s:%d in %s: |%g - %g| > %g\n", __FILE__, __LINE__, g_current_test, _ad,   \
                   _bd, _ed);                                                                           \
            g_current_failed = 1;                                                                       \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

#define RUN(fn)                                       \
    do {                                              \
        g_current_test   = #fn;                       \
        g_current_failed = 0;                         \
        fn();                                         \
        if (g_current_failed) {                       \
            g_test_fail++;                            \
        } else {                                      \
            printf("    pass %s\n", #fn);             \
            g_test_pass++;                            \
        }                                             \
    } while (0)

#endif
