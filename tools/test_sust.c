// SUST (surface store) in runtime/nvn/maxwell/translate.c (emit_sust, GLSL compute only): the instruction of the game's
// compute shader 72a4723000 at 0x358 (`eb20138600f70018`, SUST.P.2D.RGBA [R0], R24, R39), whose handle register R39
// comes from an LDC c[0][0x120]. It used to fail the whole shader ("unknown instruction"): ~7 dispatches a frame skipped
// (Link, Ganon's cape and the void portals rendering stretched / missing is the suspect).
// build + run: tools/test_sust.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

#define EXIT_W 0xe30000000007000full
#define SUST_W 0xeb20138600f70018ull

static ShaderInfo *translate(uint64_t ldc) {
    uint8_t *blob = calloc(1, 0x800);
    *(uint32_t *)blob = 0x12345679u;  // compute: a 0x100-byte header, code after it
    uint64_t *code = (uint64_t *)(blob + 0x100);
    code[1] = ldc;
    code[2] = SUST_W;
    code[3] = EXIT_W;
    ShaderKey key = {0};
    key.glsl = 1;
    return maxwell_translate(blob, 0x400, STAGE_COMPUTE, &key);
}

int main(void) {
    int fail = 0;
    // LDC R39, c[0][0x120] (32-bit, no index register)
    ShaderInfo *info = translate(0xef94000000000000ull | (0x120ull << 20) | (7ull << 16) | (0xffull << 8) | 0x27);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    int ok = info->ntex == 1 && info->tex[0].is_image && info->tex[0].bindless && info->tex[0].bindless_cbuf == 0 &&
             info->tex[0].bindless_offset == 0x120;
    printf("%s: one image, c[0][0x120]\n", ok ? "PASS" : "FAIL");
    fail |= !ok;
    ok = strstr(info->msl, "uniform writeonly image2D t0;") != NULL;
    printf("%s: image2D declaration\n", ok ? "PASS" : "FAIL");
    fail |= !ok;
    ok = strstr(info->msl, "imageStore(t0, ivec2(int(r0), int(r1)), vec4(as_f(r24), as_f(r25), as_f(r26), as_f(r27)));") != NULL;
    printf("%s: imageStore with R24..R27 at (R0, R1)\n", ok ? "PASS" : "FAIL");
    fail |= !ok;
    if (fail) fputs(info->msl, stdout);
    // a handle register set some other way (here: MOV32I R39) is refused, not guessed
    ShaderInfo *bad = translate(0x0100000000000000ull | (0x7ull << 16) | 0x27);
    ok = !bad->msl;
    printf("%s: handle register not from a constant buffer is refused (%s)\n", ok ? "PASS" : "FAIL", ok ? bad->error : "translated");
    fail |= !ok;
    return fail;
}
