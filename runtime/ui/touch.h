// On-screen touch controls for phones (runtime/ui/touch.mm).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union SDL_Event SDL_Event;

// Main thread: a touch on the screen (shows the controls; true = consumed), or a controller / key press (hides them).
bool touch_handle_event(const SDL_Event *e);
bool touch_visible(void);
void touch_hide(void);
void touch_preview(void);  // (test: shown without a touch)
// Game thread: the buttons (nn::hid bits) and left stick the controls hold now (0 while hidden).
uint64_t touch_buttons(void);
void touch_stick(int32_t *lx, int32_t *ly);
// Render thread, inside the overlay's ImGui frame (runtime/ui/menu.mm menu_render).
void touch_draw(void);

#ifdef __cplusplus
}
#endif
