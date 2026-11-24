// Directed cases for the Maxwell shader translator (runtime/nvn/maxwell/translate.c): small programs whose
// translation must contain (or must not contain) given text, for bugs a fuzz-vs-reference diff can't see because
// the reference translator had them too. Run: tools/test_shader_cases.sh
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

static int fails;

// One vertex program: NVN header (0x30) and SPH (0x50), zero, then one bundle of the control word and three instructions.
static void check(const char *name, const uint64_t insts[3], bool glsl, const char *const *want, const char *const *reject) {
    static uint8_t code[0x80 + 32 * 8];
    memset(code, 0, sizeof code);
    uint64_t bundle[4] = {0x001f8000fc0007e0ull, insts[0], insts[1], insts[2]};
    memcpy(code + 0x80, bundle, sizeof bundle);
    ShaderKey key = {0};
    key.glsl = glsl;
    ShaderInfo *s = maxwell_translate(code, sizeof code, STAGE_VERTEX, &key);
    const char *src = s->msl;
    if (!src) {
        printf("FAIL %s (%s): translation failed: %s\n", name, glsl ? "glsl" : "msl", s->error);
        fails++;
        maxwell_free(s);
        return;
    }
    bool ok = true;
    for (; want && *want; want++)
        if (!strstr(src, *want)) printf("FAIL %s (%s): missing \"%s\"\n", name, glsl ? "glsl" : "msl", *want), ok = false;
    for (; reject && *reject; reject++)
        if (strstr(src, *reject)) printf("FAIL %s (%s): has \"%s\"\n", name, glsl ? "glsl" : "msl", *reject), ok = false;
    if (!ok) fails++;
    maxwell_free(s);
}

int main(void) {
    // LDC.64 R6, c[6][R6 + 0x28]: both words from the address R6 held before the load (Link's skinning, 4th bone).
    static const uint64_t ldc64_alias[3] = {0xef95006002870606ull, 0xe30000000007000full, 0x50b0000000070f00ull};
    static const char *const ldc64_want[] = {"ldc_a = r6;", "(ldc_a) + 40u", "(ldc_a) + 44u", NULL};
    static const char *const ldc64_reject[] = {"(r6) + 44u", NULL};
    check("ldc64 dest = address", ldc64_alias, false, ldc64_want, ldc64_reject);
    check("ldc64 dest = address", ldc64_alias, true, ldc64_want, ldc64_reject);
    // LDC.64 R6, c[6][R8 + 0x28]: no overlap, the address register is used as is.
    static const uint64_t ldc64[3] = {0xef95006002870806ull, 0xe30000000007000full, 0x50b0000000070f00ull};
    static const char *const ldc64_plain[] = {"(r8) + 40u", "(r8) + 44u", NULL};
    static const char *const ldc64_plain_reject[] = {"ldc_a", NULL};
    check("ldc64", ldc64, true, ldc64_plain, ldc64_plain_reject);
    printf(fails ? "%d case(s) failed\n" : "all cases pass\n", fails);
    return fails != 0;
}
