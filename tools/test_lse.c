// tools/test_lse.c (ARM64 hosts with LSE: Apple Silicon, ARMv8.1+ Linux): runtime/cpu.h's LSE helpers (what the
// lifter emits for the SDK's atomics) against the CPU's own instructions, every op and size, edge operands.
// Unicorn 2.1.4 gets LDSMAX / LDSMIN of 8 / 16 / 32 bits wrong (it compares them unsigned: ldsmaxb(0x80, 0x01) gives
// 0x80), so tools/test_lift.py leaves those to this test. Build and run:
//   clang -O1 -march=armv8.4-a -Iruntime tools/test_lse.c -o build/test_lse && build/test_lse
#include <stdio.h>
#include <stdint.h>
typedef struct Cpu Cpu;
#include "cpu.h"

#define HW(name, insn, T, R)                                                                                   \
    static T hw_##name(T *p, uint64_t v) {                                                                    \
        uint64_t old;                                                                                         \
        __asm__ volatile(insn " %" R "1, %" R "0, [%2]" : "=&r"(old) : "r"(v), "r"(p) : "memory");            \
        return (T)old;                                                                                        \
    }
#define OPS(N, T, SUF, R)                                                                                      \
    HW(ldadd##N, "ldadd" SUF, T, R) HW(ldclr##N, "ldclr" SUF, T, R) HW(ldeor##N, "ldeor" SUF, T, R)           \
    HW(ldset##N, "ldset" SUF, T, R) HW(ldsmax##N, "ldsmax" SUF, T, R) HW(ldsmin##N, "ldsmin" SUF, T, R)       \
    HW(ldumax##N, "ldumax" SUF, T, R) HW(ldumin##N, "ldumin" SUF, T, R) HW(swp##N, "swp" SUF, T, R)
OPS(8, uint8_t, "b", "w")
OPS(16, uint16_t, "h", "w")
OPS(32, uint32_t, "", "w")
OPS(64, uint64_t, "", "x")

static const uint64_t vals[] = {0, 1, 2, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff, 0x7fffffff, 0x80000000, 0xffffffff,
                                0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull, 0x0123456789abcdefull,
                                0xfedcba9876543210ull, 0x5555aaaa5555aaaaull};
#define NV (sizeof vals / sizeof *vals)
static int bad;

#define CHECK(N, T, op)                                                                                        \
    for (unsigned i = 0; i < NV; i++)                                                                         \
        for (unsigned j = 0; j < NV; j++) {                                                                   \
            T a = (T)vals[i], b = (T)vals[i];                                                                 \
            T o1 = hw_##op##N(&a, vals[j]);                                                                   \
            uint64_t o2 = op##N((uint64_t)(uintptr_t)&b, vals[j]);                                            \
            if (a != b || o1 != (T)o2) {                                                                      \
                if (bad++ < 20) printf("%s%d mem %llx v %llx: cpu %llx/%llx, ours %llx/%llx\n", #op, N,        \
                                       (unsigned long long)vals[i], (unsigned long long)vals[j],              \
                                       (unsigned long long)a, (unsigned long long)o1, (unsigned long long)b,  \
                                       (unsigned long long)o2);                                               \
            }                                                                                                 \
        }
#define SIZE(N, T) CHECK(N, T, ldadd) CHECK(N, T, ldclr) CHECK(N, T, ldeor) CHECK(N, T, ldset) CHECK(N, T, ldsmax) \
                   CHECK(N, T, ldsmin) CHECK(N, T, ldumax) CHECK(N, T, ldumin) CHECK(N, T, swp)

int main(void) {
    SIZE(8, uint8_t) SIZE(16, uint16_t) SIZE(32, uint32_t) SIZE(64, uint64_t)
    printf(bad ? "%d mismatches\n" : "all %d LSE ops x 4 sizes match the CPU\n", bad ? bad : 9);
    return bad != 0;
}
