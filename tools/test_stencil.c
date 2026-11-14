// Stencil-aspect texture reads (ShaderKey.stencil_tex): the rift ribbon's fragment shader 72a71bb500 samples the depth-stencil
// buffer's stencil and tests bits 5/6 with R2P; sampling the depth aspect as a float sampler gave it garbage and the ribbon
// rendered as thin dashes (log of 2026-11-13). With the key bit the texture must be a usampler2D whose texel comes back as raw
// bits; without it (and for another handle slot) the sampler stays a plain sampler2D. build + run: tools/test_stencil.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

static ShaderInfo *build(uint32_t stencil_tex, int fetch) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)blob;
    // TEXS (info 1) / TLDS (info 2): R0, R1 <- 2D texture, handle index 0x0c (cbuf offset 0x30 = handle slot 2)
    uint64_t w = (fetch ? 0xd200000000000000ull : 0xd000000000000000ull) | ((fetch ? 2ull : 1ull) << 53) | (0x0cull << 36) | (1ull << 28) |
                 (9ull << 20) | (8ull << 8);
    code[1] = w;
    code[2] = code[3] = 0xe30000000007000full;  // EXIT (fills the 3-instruction group)
    ShaderKey key = {0};
    key.glsl = 1;
    key.stencil_tex = stencil_tex;
    return maxwell_translate(blob, 0x400, STAGE_COMPUTE, &key);
}

int main(void) {
    int bad = 0;
    for (int fetch = 0; fetch < 2; fetch++) {
        ShaderInfo *on = build(1u << 2, fetch), *off = build(0, fetch), *other = build(1u << 3, fetch);
        if (!on->msl || !off->msl || !other->msl) { printf("FAIL: translate failed: %s\n", on->msl ? off->msl ? other->error : off->error : on->error); return 1; }
        int ok = strstr(on->msl, "usampler2D t0;") && strstr(on->msl, "uintBitsToFloat(") &&
                 strstr(on->msl, fetch ? "uintBitsToFloat(texelFetch(t0," : "uintBitsToFloat(textureLod(t0,") &&
                 !strstr(off->msl, "usampler2D") && !strstr(off->msl, "uintBitsToFloat(texture") &&
                 !strstr(other->msl, "usampler2D") && strstr(off->msl, "sampler2D t0;");
        printf("%s: stencil texture read (%s)\n", ok ? "PASS" : "FAIL", fetch ? "TLDS" : "TEXS");
        if (!ok) { fputs(on->msl, stdout); bad = 1; }
    }
    return bad;
}
