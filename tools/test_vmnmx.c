// VMNMX (3b7707e061172a0f, 3b770fe061071e10: the computes 72a4711000 / 72a4715000 of the forest scene): Rd = max(Ra, Rb, Rc)
// signed. Untranslated, those dispatches were skipped (432 per frame) and trees stayed invisible.
// build + run: tools/test_vmnmx.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

int main(void) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)(blob + 0x80);
    code[1] = 0x3b7707e061172a0full;  // R15 = max(R42, R17, R15)
    code[2] = 0x3b770fe061071e10ull;  // R16 = max(R30, R16, R31)
    code[3] = 0xe30000000007000full;  // EXIT
    ShaderKey key = {0};
    key.glsl = 1;
    ShaderInfo *info = maxwell_translate(blob, 0x400, STAGE_VERTEX, &key);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    const char *m = info->msl;
    int ok = strstr(m, "r15 = uint(max(int(max(int(r42), int(r17))), int(r15)));");
    ok = ok && strstr(m, "r16 = uint(max(int(max(int(r30), int(r16))), int(r31)));");
    printf("%s: VMNMX three-way max\n", ok ? "PASS" : "FAIL");
    if (!ok) fputs(m, stdout);
    return !ok;
}
