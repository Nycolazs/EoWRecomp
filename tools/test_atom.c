// ATOM (ed03000000500005 = ATOM.E.ADD.S32 R5, [R0], R5, from the vegetation computes 72a4711000 / 72a4715000): returns the old value.
// Untranslated, those dispatches were skipped and trees / grass never appeared (log of 2026-11-12).
// build + run: tools/test_atom.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

int main(void) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)(blob + 0x80);
    code[1] = 0xed03000000500005ull;  // ATOM.E.ADD.S32 R5, [R0], R5
    code[2] = 0xed23000000500105ull;  // same with op 2 (max), S32: needs the compare-and-swap loop
    code[3] = 0xe30000000007000full;  // EXIT
    ShaderKey key = {0};
    key.glsl = 1;
    ShaderInfo *info = maxwell_translate(blob, 0x400, STAGE_VERTEX, &key);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    const char *m = info->msl;
    int ok = strstr(m, "o_ = atomicAdd(gmem_.w[uint(ga >> 2)], v_);") && strstr(m, "r5 = o_;") &&
             strstr(m, "uint(max(int(o_), int(v_)))");
    printf("%s: ATOM returns the old value\n", ok ? "PASS" : "FAIL");
    if (!ok) fputs(m, stdout);
    return !ok;
}
