#ifndef MINISHOT_TEST_H
#define MINISHOT_TEST_H

#include <stdio.h>
#include <math.h>

// Minimal assert-based test harness. Each tests/test_*.c defines a main()
// that runs CHECK / CHECK_EQ / CHECK_NEAR / CHECK_STR and returns t_fails.
static int t_fails = 0;
static int t_checks = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        t_checks++;                                                \
        if (!(cond)) {                                             \
            t_fails++;                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                \
    do {                                                              \
        t_checks++;                                                   \
        long long _a = (long long)(a), _b = (long long)(b);           \
        if (_a != _b) {                                               \
            t_fails++;                                                \
            printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, \
                   __LINE__, #a, #b, _a, _b);                         \
        }                                                             \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                               \
    do {                                                                    \
        t_checks++;                                                         \
        double _a = (a), _b = (b);                                          \
        if (fabs(_a - _b) > (eps)) {                                        \
            t_fails++;                                                      \
            printf("FAIL %s:%d: %s ~= %s (%g vs %g)\n", __FILE__, __LINE__, \
                   #a, #b, _a, _b);                                         \
        }                                                                   \
    } while (0)

#define CHECK_STR(a, b)                                                       \
    do {                                                                      \
        t_checks++;                                                           \
        if (strcmp((a), (b)) != 0) {                                          \
            t_fails++;                                                        \
            printf("FAIL %s:%d: \"%s\" == \"%s\"\n", __FILE__, __LINE__, (a), \
                   (b));                                                      \
        }                                                                     \
    } while (0)

#define TEST_REPORT()                                                        \
    do {                                                                     \
        printf("%s: %d checks, %d failures\n", __FILE__, t_checks, t_fails); \
        return t_fails;                                                      \
    } while (0)

#endif
