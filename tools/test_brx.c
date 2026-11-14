// BRX jump-table labelling in runtime/nvn/maxwell/translate.c (brx_table_labels, GLSL only).
// A vertex shader that loads a jump table from cb1 (LDC) and goes through BRX must get a `case` only for the table's
// targets (plus the entry), not for every instruction: on AMD's Windows driver the all-instructions dispatcher never
// finished building (Radeon RX 580 log, 2026-11-08; LASRecomp #237).
// build + run: tools/test_brx.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

#define EXIT_W 0xe30000000007000full
#define CONST_OFF 0x400

static int count(const char *s, const char *needle) {
    int n = 0;
    for (const char *p = s; (p = strstr(p, needle)); p += strlen(needle)) n++;
    return n;
}

int main(void) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)(blob + 0x80);
    // qword 0 / 4 / 8 / 12: scheduling words. 1: LDC R0, c[1][R0 + 0] (as in the game's ef94001000070000);
    // 2: BRX R0 + 0 (as e2500ffff087000f but imm 0); 3, 5..14: EXIT (the table's targets are 5, 6 and 7)
    code[1] = 0xef94001000070000ull;
    code[2] = 0xe25000000007000full;  // ra = R0 (bits 8..15), imm 0 (bits 20..43), predicate PT (bits 16..19)
    for (int i = 3; i < 15; i++) if (i % 4) code[i] = EXIT_W;
    int32_t *tab = (int32_t *)(blob + CONST_OFF);
    // BRX at qword 2 (pc 16): target = pc + 8 + table[k] + imm
    tab[0] = 5 * 8 - 24; tab[1] = 6 * 8 - 24; tab[2] = 7 * 8 - 24;
    tab[3] = 0x7fffffff;  // not an instruction of this shader: ends the table
    ShaderKey key = {0};
    key.glsl = 1;
    ShaderInfo *info = maxwell_translate(blob, CONST_OFF, STAGE_VERTEX, &key);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    int ncase = count(info->msl, "  case ");
    int ok = 1;
    ok &= count(info->msl, "case 40u:") == 1 && count(info->msl, "case 48u:") == 1 && count(info->msl, "case 56u:") == 1;
    ok &= count(info->msl, "case 64u:") == 0 && count(info->msl, "case 24u:") == 0;
    ok &= ncase == 4;  // the entry (pc 8) and the three targets
    printf("%s: %d case labels (want 4: the entry and the table's three targets)\n", ok ? "PASS" : "FAIL", ncase);
    if (!ok) fputs(info->msl, stdout);
    return !ok;
}
