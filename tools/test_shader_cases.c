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

// BRX through a c[1] jump table: only the table's targets (and the entry) become switch labels.
//   0x08 LDC R42, c[1][R42]   0x10 BRX R42   0x18 NOP   0x28 NOP (target)   0x30 NOP (target)   0x38 EXIT
static void check_brx(const char *name, bool from_ldc, bool with_cb1, const char *const *want, const char *const *reject) {
    static uint8_t code[0x80 + 0x100];
    memset(code, 0, sizeof code);
    uint64_t ctrl = 0x001f8000fc0007e0ull, nop = 0x50b0000000070f00ull, exit_ = 0xe30000000007000full;
    uint64_t ldc = from_ldc ? 0xef94001000072a2aull : 0x5c98078000070a2aull;  // (else MOV R42, R10)
    uint64_t brx = 0xe250000000000000ull | (0x7ull << 16) | (0x2aull << 8) | 0x0f;  // BRX R42 + 0
    uint64_t prog[8] = {ctrl, ldc, brx, nop, ctrl, nop, nop, exit_};
    memcpy(code + 0x80, prog, sizeof prog);
    // the table, after the code: pc 0x10 + 8 + v -> 0x28, 0x30; then a word no target can come from
    static uint32_t cb1[4] = {0x10, 0x18, 0xFFFFFFFFu, 0};
    maxwell_cb1 = with_cb1 ? (const uint8_t *)cb1 : NULL;
    ShaderKey key = {0};
    key.glsl = true;
    ShaderInfo *s = maxwell_translate(code, 0x80 + sizeof prog, STAGE_VERTEX, &key);
    maxwell_cb1 = NULL;
    bool ok = s->msl != NULL;
    if (!ok) printf("FAIL %s: translation failed: %s\n", name, s->error);
    for (; ok && want && *want; want++)
        if (!strstr(s->msl, *want)) printf("FAIL %s: missing \"%s\"\n", name, *want), ok = false;
    for (; ok && reject && *reject; reject++)
        if (strstr(s->msl, *reject)) printf("FAIL %s: has \"%s\"\n", name, *reject), ok = false;
    if (!ok) fails++;
    maxwell_free(s);
}

int main(void) {
    static const char *const brx_targets[] = {"case 8u:", "case 40u:", "case 48u:", NULL};
    static const char *const brx_only_targets[] = {"case 16u:", "case 24u:", "case 56u:", NULL};
    static const char *const brx_all[] = {"case 8u:", "case 16u:", "case 24u:", "case 40u:", "case 56u:", NULL};
    check_brx("brx table: its targets only", true, true, brx_targets, brx_only_targets);
    check_brx("brx without the constants: every instruction", true, false, brx_all, NULL);
    check_brx("brx register not from c[1]: every instruction", false, true, brx_all, NULL);
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
