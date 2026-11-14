// LDC.64 whose address register is also its destination (ef95006002870606 = LDC.64 R6, c[6][R6 + 0x28], the 4-influence
// skin path of vs 72ee694000): the hardware reads R6 before writing it, so the second word must use the OLD R6.
// Before the fix r7 was loaded with the new r6 -> garbage cbuf address -> hair/hood/cloth stretched into beams
// (RX 580 log + screenshot, 2026-11-11).
// build + run: tools/test_ldc.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

int main(void) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)(blob + 0x80);
    code[1] = 0xef95006002870606ull;                 // aliasing LDC.64
    code[2] = 0xef95006002870a08ull;                 // LDC.64 R8, c[6][R10 + 0x28]: no aliasing, must stay plain
    code[3] = 0xe30000000007000full;                 // EXIT
    ShaderKey key = {0};
    key.glsl = 1;
    ShaderInfo *info = maxwell_translate(blob, 0x400, STAGE_VERTEX, &key);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    const char *m = info->msl;
    int ok = strstr(m, "uint ldc_a = r6;") && strstr(m, "r7 = cbr(cb6, (ldc_a) + 44u);") && !strstr(m, "r7 = cbr(cb6, (r6) + 44u)");
    ok = ok && strstr(m, "r9 = cbr(cb6, (r10) + 44u);");
    printf("%s: aliased LDC.64 reads the old address register\n", ok ? "PASS" : "FAIL");
    if (!ok) fputs(m, stdout);
    return !ok;
}
