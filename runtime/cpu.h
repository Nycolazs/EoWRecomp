// Guest CPU state and helpers used by recompiled code.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct __attribute__((aligned(16))) Cpu {
    uint64_t x[32];          // x0-x30 (x[31] unused; sp is separate)
    uint64_t sp;             // 256
    uint64_t pc;             // 264: target for indirect dispatch
    uint64_t nzcv;           // 272: flags in bits 31..28
    uint64_t tpidr;          // 280
    uint64_t tpidrro;        // 288
    uint64_t excl_addr;      // 296
    uint64_t excl_val;       // 304
    void *thread;            // 312: runtime thread object
    uint64_t fpcr;           // 320: guest FPCR/FPSR (portable FP/SIMD lowering only; the asm path uses the host's)
    uint64_t fpsr;           // 328
    uint64_t excl_hi;        // 336: upper half of an exclusive pair (LDXP)
    uint8_t _pad[512 - 344];
    __uint128_t v[32];       // 512
} Cpu;

_Static_assert(offsetof(Cpu, sp) == 256, "sp");
_Static_assert(offsetof(Cpu, pc) == 264, "pc");
_Static_assert(offsetof(Cpu, nzcv) == 272, "nzcv");
_Static_assert(offsetof(Cpu, fpcr) == 320, "fpcr");
_Static_assert(offsetof(Cpu, fpsr) == 328, "fpsr");
_Static_assert(offsetof(Cpu, excl_hi) == 336, "excl_hi");
_Static_assert(offsetof(Cpu, v) == 512, "v");

typedef void (*GuestFn)(Cpu *);

// What the game code library calls in the app. On Windows the .exe exports them (EOW_RUNTIME: compiling the app)
// and the library imports them through tools/windows/eowrecomp.def; elsewhere the dynamic linker binds them.
#if defined(_WIN32) && defined(EOW_RUNTIME)
#define RT_API __declspec(dllexport)
#elif defined(_WIN32)
#define RT_API __declspec(dllimport)
#else
#define RT_API
#endif

// ---- runtime services (runtime/*.c)
RT_API void rt_dispatch(Cpu *c);                 // call function at c->pc
RT_API void rt_svc(Cpu *c, uint32_t imm, uint64_t pc);
RT_API void rt_brk(Cpu *c, uint32_t imm, uint64_t pc) __attribute__((noreturn));
RT_API void rt_udf(Cpu *c, uint64_t pc) __attribute__((noreturn));
RT_API uint64_t rt_ticks(void);

#if defined(__aarch64__)
static inline uint64_t rt_get_fpcr(void) { uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
static inline uint64_t rt_get_fpsr(void) { uint64_t v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
static inline void rt_set_fpcr(uint64_t v) { __asm__ volatile("msr fpcr, %0" :: "r"(v)); }
static inline void rt_set_fpsr(uint64_t v) { __asm__ volatile("msr fpsr, %0" :: "r"(v)); }
#else
// Host FP state is not the guest's here: code lifted with --simd=c keeps FPCR/FPSR in Cpu (c->fpcr, zeroed
// with the Cpu), so the host-side reset in thread start-up has nothing to do.
static inline uint64_t rt_get_fpcr(void) { return 0; }
static inline uint64_t rt_get_fpsr(void) { return 0; }
static inline void rt_set_fpcr(uint64_t v) { (void)v; }
static inline void rt_set_fpsr(uint64_t v) { (void)v; }
#endif

// ---- memory: guest addresses are host addresses
// Where the host can't give the guest the Switch-style window at 0x70_0000_0000 (Android: the zygote maps its
// libraries and heaps there on 39-bit kernels), the whole guest layout sits GUEST_SHIFT lower. Recompiled functions and
// labels keep their unshifted names (f_7000...), and so do patches and runtime/modules.h: only the values move.
// tools/gen.py --shift must match (as in LASRecomp-Android-Plus).
#if defined(__ANDROID__)
#define GUEST_SHIFT 0x6000000000ull
#else
#define GUEST_SHIFT 0ull
#endif
#define GUEST(a) ((uint64_t)(a) - GUEST_SHIFT)  // an address of the unshifted (Switch) layout
typedef uint16_t __attribute__((aligned(1), may_alias)) u16u;
typedef uint32_t __attribute__((aligned(1), may_alias)) u32u;
typedef uint64_t __attribute__((aligned(1), may_alias)) u64u;
typedef uint8_t __attribute__((may_alias)) u8a;

// Function-entry profiler (runtime/fnprof.c, EOWRECOMP_FNPROF at run time): only in game code built with
// -DEOWRECOMP_FNPROF (make FNPROF=1). Off, a call costs nothing: the check (a GOT load, a load and a branch on every
// guest call) was ~10% of the game code's instructions (measured in LASRecomp-Android-Plus).
RT_API extern volatile int g_fnprof;
RT_API void fnprof_hit(uint64_t addr, uint64_t lr, uint64_t x0);
#ifdef EOWRECOMP_FNPROF
#define FN_ENTER(a) do { if (__builtin_expect(g_fnprof, 0)) fnprof_hit(a, c->x[30], c->x[0]); } while (0)
#else
#define FN_ENTER(a) ((void)0)
#endif
// Generated functions take their Cpu as `Cpu *CPU_RESTRICT c`: guest memory never overlaps the host's Cpu, so stores
// to guest memory don't make the compiler reload guest registers.
#define CPU_RESTRICT __restrict

#define LD8(a)  (*(const u8a *)(uintptr_t)(a))
#define LD16(a) (*(const u16u *)(uintptr_t)(a))
#define LD32(a) (*(const u32u *)(uintptr_t)(a))
#define LD64(a) (*(const u64u *)(uintptr_t)(a))
#define ST8(a, v)  (*(u8a *)(uintptr_t)(a) = (uint8_t)(v))
#define ST16(a, v) (*(u16u *)(uintptr_t)(a) = (uint16_t)(v))
#define ST32(a, v) (*(u32u *)(uintptr_t)(a) = (uint32_t)(v))
#define ST64(a, v) (*(u64u *)(uintptr_t)(a) = (uint64_t)(v))

// ---- exclusive monitor (emulated with compare-and-swap)
#define EXCL(N, T)                                                                         \
    static inline uint64_t ldx##N(Cpu *c, uint64_t a) {                                    \
        T v = __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_ACQUIRE);                        \
        c->excl_addr = a;                                                                  \
        c->excl_val = v;                                                                   \
        return v;                                                                          \
    }                                                                                      \
    static inline uint32_t stx##N(Cpu *c, uint64_t a, uint64_t v) {                        \
        if (c->excl_addr != a) return 1;                                                   \
        c->excl_addr = ~0ull;                                                              \
        T expected = (T)c->excl_val;                                                       \
        return !__atomic_compare_exchange_n((T *)(uintptr_t)a, &expected, (T)v, 0,         \
                                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);           \
    }                                                                                      \
    static inline uint64_t lda##N(uint64_t a) {                                            \
        return __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_ACQUIRE);                       \
    }                                                                                      \
    static inline void stl##N(uint64_t a, uint64_t v) {                                    \
        __atomic_store_n((T *)(uintptr_t)a, (T)v, __ATOMIC_RELEASE);                       \
    }
EXCL(8, uint8_t)
EXCL(16, uint16_t)
EXCL(32, uint32_t)
EXCL(64, uint64_t)

// 128-bit exclusive pair (LDXP / STXP of two X registers): the monitor holds both halves, the store is one 16-byte
// compare-and-swap (cmpxchg16b on x86-64: x86-64-v2 and up; LDXP itself needs 16-byte alignment on the Switch).
typedef unsigned __int128 u128;
static inline void ldxp64(Cpu *c, uint64_t a, uint64_t *lo, uint64_t *hi) {
    u128 v = __atomic_load_n((u128 *)(uintptr_t)a, __ATOMIC_ACQUIRE);
    c->excl_addr = a;
    c->excl_val = *lo = (uint64_t)v;
    c->excl_hi = *hi = (uint64_t)(v >> 64);
}
static inline uint32_t stxp64(Cpu *c, uint64_t a, uint64_t lo, uint64_t hi) {
    if (c->excl_addr != a) return 1;
    c->excl_addr = ~0ull;
    u128 expected = ((u128)c->excl_hi << 64) | c->excl_val;
    return !__atomic_compare_exchange_n((u128 *)(uintptr_t)a, &expected, ((u128)hi << 64) | lo, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_SEQ_CST);
}

// ---- ARMv8.1 LSE (the SDK's outline atomics): CAS / CASP return the old value; LD<op> / SWP return the old value
#define LSE_IMPL(N, T, S)                                                                                         \
    static inline uint64_t cas##N(uint64_t a, uint64_t expect, uint64_t v) {                                     \
        T e = (T)expect;                                                                                          \
        __atomic_compare_exchange_n((T *)(uintptr_t)a, &e, (T)v, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);         \
        return e;                                                                                                 \
    }                                                                                                             \
    static inline uint64_t ldadd##N(uint64_t a, uint64_t v) { return __atomic_fetch_add((T *)(uintptr_t)a, (T)v, __ATOMIC_SEQ_CST); } \
    static inline uint64_t ldclr##N(uint64_t a, uint64_t v) { return __atomic_fetch_and((T *)(uintptr_t)a, (T)~v, __ATOMIC_SEQ_CST); } \
    static inline uint64_t ldeor##N(uint64_t a, uint64_t v) { return __atomic_fetch_xor((T *)(uintptr_t)a, (T)v, __ATOMIC_SEQ_CST); } \
    static inline uint64_t ldset##N(uint64_t a, uint64_t v) { return __atomic_fetch_or((T *)(uintptr_t)a, (T)v, __ATOMIC_SEQ_CST); }  \
    static inline uint64_t swp##N(uint64_t a, uint64_t v) { return __atomic_exchange_n((T *)(uintptr_t)a, (T)v, __ATOMIC_SEQ_CST); }  \
    static inline uint64_t ldsmax##N(uint64_t a, uint64_t v) {                                                    \
        T o = __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_RELAXED);                                               \
        while (!__atomic_compare_exchange_n((T *)(uintptr_t)a, &o, (S)o > (S)v ? o : (T)v, 1, __ATOMIC_SEQ_CST,  \
                                            __ATOMIC_RELAXED)) {}                                                 \
        return o;                                                                                                 \
    }                                                                                                             \
    static inline uint64_t ldsmin##N(uint64_t a, uint64_t v) {                                                    \
        T o = __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_RELAXED);                                               \
        while (!__atomic_compare_exchange_n((T *)(uintptr_t)a, &o, (S)o < (S)v ? o : (T)v, 1, __ATOMIC_SEQ_CST,  \
                                            __ATOMIC_RELAXED)) {}                                                 \
        return o;                                                                                                 \
    }                                                                                                             \
    static inline uint64_t ldumax##N(uint64_t a, uint64_t v) {                                                    \
        T o = __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_RELAXED);                                               \
        while (!__atomic_compare_exchange_n((T *)(uintptr_t)a, &o, o > (T)v ? o : (T)v, 1, __ATOMIC_SEQ_CST,     \
                                            __ATOMIC_RELAXED)) {}                                                 \
        return o;                                                                                                 \
    }                                                                                                             \
    static inline uint64_t ldumin##N(uint64_t a, uint64_t v) {                                                    \
        T o = __atomic_load_n((T *)(uintptr_t)a, __ATOMIC_RELAXED);                                               \
        while (!__atomic_compare_exchange_n((T *)(uintptr_t)a, &o, o < (T)v ? o : (T)v, 1, __ATOMIC_SEQ_CST,     \
                                            __ATOMIC_RELAXED)) {}                                                 \
        return o;                                                                                                 \
    }
LSE_IMPL(8, uint8_t, int8_t)
LSE_IMPL(16, uint16_t, int16_t)
LSE_IMPL(32, uint32_t, int32_t)
LSE_IMPL(64, uint64_t, int64_t)

static inline void casp64(uint64_t a, uint64_t elo, uint64_t ehi, uint64_t lo, uint64_t hi, uint64_t *olo, uint64_t *ohi) {
    u128 e = ((u128)ehi << 64) | elo;
    __atomic_compare_exchange_n((u128 *)(uintptr_t)a, &e, ((u128)hi << 64) | lo, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    *olo = (uint64_t)e;
    *ohi = (uint64_t)(e >> 64);
}

// ---- flags (packed NZCV in bits 31..28)
#define FLAG_N(c) (((c)->nzcv >> 31) & 1)
#define FLAG_Z(c) (((c)->nzcv >> 30) & 1)
#define FLAG_C(c) (((c)->nzcv >> 29) & 1)
#define FLAG_V(c) (((c)->nzcv >> 28) & 1)
#define COND_EQ(c) (FLAG_Z(c))
#define COND_NE(c) (!FLAG_Z(c))
#define COND_CS(c) (FLAG_C(c))
#define COND_CC(c) (!FLAG_C(c))
#define COND_MI(c) (FLAG_N(c))
#define COND_PL(c) (!FLAG_N(c))
#define COND_VS(c) (FLAG_V(c))
#define COND_VC(c) (!FLAG_V(c))
#define COND_HI(c) (FLAG_C(c) && !FLAG_Z(c))
#define COND_LS(c) (!(FLAG_C(c) && !FLAG_Z(c)))
#define COND_GE(c) (FLAG_N(c) == FLAG_V(c))
#define COND_LT(c) (FLAG_N(c) != FLAG_V(c))
#define COND_GT(c) (!FLAG_Z(c) && FLAG_N(c) == FLAG_V(c))
#define COND_LE(c) (!(!FLAG_Z(c) && FLAG_N(c) == FLAG_V(c)))

#define MKFLAGS(n, z, cy, v) \
    (((uint64_t)(n) << 31) | ((uint64_t)(z) << 30) | ((uint64_t)(cy) << 29) | ((uint64_t)(v) << 28))

static inline uint64_t adds64(Cpu *c, uint64_t a, uint64_t b) {
    uint64_t r = a + b;
    c->nzcv = MKFLAGS(r >> 63, r == 0, r < a, ((a ^ r) & (b ^ r)) >> 63);
    return r;
}
static inline uint64_t subs64(Cpu *c, uint64_t a, uint64_t b) {
    uint64_t r = a - b;
    c->nzcv = MKFLAGS(r >> 63, r == 0, a >= b, ((a ^ b) & (a ^ r)) >> 63);
    return r;
}
static inline uint32_t adds32(Cpu *c, uint32_t a, uint32_t b) {
    uint32_t r = a + b;
    c->nzcv = MKFLAGS(r >> 31, r == 0, r < a, ((a ^ r) & (b ^ r)) >> 31);
    return r;
}
static inline uint32_t subs32(Cpu *c, uint32_t a, uint32_t b) {
    uint32_t r = a - b;
    c->nzcv = MKFLAGS(r >> 31, r == 0, a >= b, ((a ^ b) & (a ^ r)) >> 31);
    return r;
}
static inline uint64_t logic64(Cpu *c, uint64_t r) { c->nzcv = MKFLAGS(r >> 63, r == 0, 0, 0); return r; }
static inline uint32_t logic32(Cpu *c, uint32_t r) { c->nzcv = MKFLAGS(r >> 31, r == 0, 0, 0); return r; }

#define ADC_IMPL(N, S)                                                                     \
    static inline uint##N##_t adc##N(Cpu *c, uint##N##_t a, uint##N##_t b) {               \
        return a + b + (uint##N##_t)FLAG_C(c);                                             \
    }                                                                                      \
    static inline uint##N##_t sbc##N(Cpu *c, uint##N##_t a, uint##N##_t b) {               \
        return a - b - 1 + (uint##N##_t)FLAG_C(c);                                         \
    }                                                                                      \
    static inline uint##N##_t adcs##N(Cpu *c, uint##N##_t a, uint##N##_t b) {              \
        uint##N##_t cin = FLAG_C(c), r = a + b + cin;                                      \
        int carry = cin ? r <= a : r < a;                                                  \
        c->nzcv = MKFLAGS(r >> (N - 1), r == 0, carry, ((a ^ r) & (b ^ r)) >> (N - 1));    \
        return r;                                                                          \
    }                                                                                      \
    static inline uint##N##_t sbcs##N(Cpu *c, uint##N##_t a, uint##N##_t b) {              \
        return adcs##N(c, a, (uint##N##_t)~b);                                             \
    }
ADC_IMPL(64, )
ADC_IMPL(32, )

// ---- bit ops
#define ROR64(v, r) ((uint64_t)(v) >> ((r) & 63) | (uint64_t)(v) << ((64 - ((r) & 63)) & 63))
#define ROR32(v, r) ((uint32_t)(v) >> ((r) & 31) | (uint32_t)(v) << ((32 - ((r) & 31)) & 31))
static inline uint64_t clz64(uint64_t v) { return v ? __builtin_clzll(v) : 64; }
static inline uint32_t clz32(uint32_t v) { return v ? __builtin_clz(v) : 32; }
static inline uint64_t cls64(uint64_t v) { return clz64(v ^ (uint64_t)((int64_t)v >> 1)) - 1; }
static inline uint32_t cls32(uint32_t v) { return clz32(v ^ (uint32_t)((int32_t)v >> 1)) - 1; }
static inline uint64_t rbit64(uint64_t v) { return __builtin_bitreverse64(v); }
static inline uint32_t rbit32(uint32_t v) { return __builtin_bitreverse32(v); }
static inline uint32_t rev16_32(uint32_t v) { return ((v & 0x00ff00ffu) << 8) | ((v >> 8) & 0x00ff00ffu); }
static inline uint64_t rev16_64(uint64_t v) {
    return ((v & 0x00ff00ff00ff00ffull) << 8) | ((v >> 8) & 0x00ff00ff00ff00ffull);
}
static inline uint64_t rev32_64(uint64_t v) {
    return ((uint64_t)__builtin_bswap32((uint32_t)v)) | ((uint64_t)__builtin_bswap32((uint32_t)(v >> 32)) << 32);
}
static inline uint64_t udiv64(uint64_t a, uint64_t b) { return b ? a / b : 0; }
static inline uint32_t udiv32(uint32_t a, uint32_t b) { return b ? a / b : 0; }
static inline uint64_t sdiv64(uint64_t a, uint64_t b) {
    if (!b) return 0;
    if ((int64_t)a == INT64_MIN && (int64_t)b == -1) return a;
    return (uint64_t)((int64_t)a / (int64_t)b);
}
static inline uint32_t sdiv32(uint32_t a, uint32_t b) {
    if (!b) return 0;
    if ((int32_t)a == INT32_MIN && (int32_t)b == -1) return a;
    return (uint32_t)((int32_t)a / (int32_t)b);
}

// Portable FP/SIMD helpers for code lifted with --simd=c (default off AArch64; gen.py includes simd.h itself
// in that mode, so AArch64 hosts can build it too).
#if !defined(__cplusplus) && !defined(__aarch64__)
#include "simd.h"
#endif
