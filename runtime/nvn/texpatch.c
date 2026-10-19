// Texture edits on upload (texpatch.h). No Echoes of Wisdom texture is known yet: every texture is left as it is.
#include "texpatch.h"

int texpatch_identify(const NvnTexture *t) {
    (void)t;
    return -1;
}

uint32_t texpatch_generation(void) { return 0; }

void texpatch_apply(int id, uint32_t lv, uint8_t *blocks, uint32_t w, uint32_t h, uint32_t row) {
    (void)id, (void)lv, (void)blocks, (void)w, (void)h, (void)row;
}
