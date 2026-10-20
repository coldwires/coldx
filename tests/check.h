#pragma once

// Minimal assertions for coldx's tests. A failed check prints and is counted; the test's
// main returns CheckResult(), nonzero if anything failed, which is what CTest reads.

#include <cstdio>
#include <cstdlib>

namespace coldx_test {

inline int& Failures() {
    static int failures = 0;
    return failures;
}

inline int CheckResult() {
    if (Failures() == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", Failures());
    return 1;
}

} // namespace coldx_test

#define CHECK(cond)                                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);       \
            ++coldx_test::Failures();                                                   \
        }                                                                               \
    } while (0)

#define CHECK_NEAR(a, b, tolerance)                                                     \
    do {                                                                                \
        const long long a_ = static_cast<long long>(a);                                 \
        const long long b_ = static_cast<long long>(b);                                 \
        if (std::llabs(a_ - b_) > (tolerance)) {                                        \
            std::printf("%s:%d: CHECK_NEAR failed: %s = %lld, %s = %lld\n", __FILE__,   \
                        __LINE__, #a, a_, #b, b_);                                      \
            ++coldx_test::Failures();                                                   \
        }                                                                               \
    } while (0)
