// Widescreen: the renderer asks each frame whether the game image may fill a window that isn't 16:9, and how much of
// the rendered image to present. In LAS Recomp the camera code's 16:9 aspect loads were patched (tools/patches.py)
// so the game itself rendered wider views. Echoes of Wisdom has no such patches yet (milestone M7), so the game's
// image is 16:9 and is shown boxed in other window shapes: widening it here would only stretch it.
#include <stdbool.h>

#include "../hle.h"
#include "../settings.h"

static float g_wide = 1.0f;  // output aspect divided by 16:9 (1 = normal)

void widescreen_set_factor(float f) { g_wide = f; }
float widescreen_factor(void) { return g_wide; }

// The output may take the window's shape (unlocked aspect). False until the game's cameras follow it.
bool widescreen_allowed(void) { return false; }

void widescreen_frame_end(uint64_t frame) { (void)frame; }

// Presented part of the image (crop: uv origin and size) and its place in the window: all of it.
void widescreen_present_crop(uint64_t frame, double shape, float crop[4], double *vx, double *vy, double *vw, double *vh) {
    (void)frame, (void)shape, (void)vx, (void)vy, (void)vw, (void)vh;
    crop[0] = crop[1] = 0, crop[2] = crop[3] = 1;
}

// Current level name for logs: unknown for Echoes of Wisdom so far.
const char *game_level_name(void) { return "-"; }

// Test hook: the next presented frame is saved to this path (EOWRECOMP_HOST_INPUT "shot" entries, runtime/hle/input.c).
char g_window_shot_request[256];
