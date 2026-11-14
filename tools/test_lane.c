// S2R SR_LANEID / SR_LANEMASK_LT and VOTE/SHFL (from the vegetation culling compute 72a4711000: the lane id and the lane mask
// were lowered to 0, so the ballot leader never matched and the instance counter stayed 0 - trees vanished, log of 2026-11-13).
// Also checks that ballots/shuffles stay in the 32-lane half (wave64 GPUs). build + run: tools/test_lane.sh
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/nvn/maxwell/shader.h"

int main(void) {
    uint8_t *blob = calloc(1, 0x800);
    uint64_t *code = (uint64_t *)blob;
    code[1] = 0xf0c8000000000005ull;  // S2R R5, SR_LANEID
    code[2] = 0xf0c8000003900006ull;  // S2R R6, SR_LANEMASK_LT (0x39)
    code[3] = 0x50d9e3800007000eull;  // VOTE.ANY R14, PT, PT
    code[4] = 0xe30000000007000full;  // EXIT
    ShaderKey key = {0};
    key.glsl = 1;
    ShaderInfo *info = maxwell_translate(blob, 0x400, STAGE_COMPUTE, &key);
    if (!info->msl) { printf("FAIL: translate failed: %s\n", info->error); return 1; }
    const char *m = info->msl;
    int ok = strstr(m, "r5 = mw_lane();") && strstr(m, "r6 = ((1u << mw_lane()) - 1u);") && strstr(m, "r14 = mw_ballot(vp_);") &&
             strstr(m, "uint mw_ballot(bool b)") && strstr(m, "gl_SubgroupInvocationID >= 32u ? v.y : v.x");
    printf("%s: lane id / lane mask / 32-lane ballot\n", ok ? "PASS" : "FAIL");
    if (!ok) fputs(m, stdout);
    return !ok;
}
