// x86-64-v3 check (the game code is built with -march=x86-64-v3: AVX2, FMA, BMI1/2) straight from CPUID, so it
// needs no compiler runtime (__builtin_cpu_supports pulls in libgcc's __cpu_model, which zig cc doesn't have).
#pragma once
#include <stdbool.h>
#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>

static inline bool cpu_has_x86_64_v3(void) {
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return false;
    bool fma = c & (1u << 12), osxsave = c & (1u << 27), avx = c & (1u << 28);
    if (!fma || !osxsave || !avx) return false;
    unsigned lo, hi;
    __asm__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));  // the OS saves the YMM state
    if ((lo & 6) != 6) return false;
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d)) return false;
    return (b & (1u << 5)) && (b & (1u << 3)) && (b & (1u << 8));  // AVX2, BMI1, BMI2
}
#endif
