// On-screen touch controls (phones; touch.h): every Switch button the game uses, a floating left stick and a D-pad,
// drawn over the game by the overlay (runtime/ui/menu.mm menu_render) and read by runtime/hle/input.c. They show
// after a touch on the screen and hide while a controller or the keyboard is in use. Only direct touch screens count
// (a Mac trackpad's touches are not ours). Events: main thread; buttons / stick: atomics read by the game thread.
#include "touch.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <atomic>

#include "imgui.h"

namespace {

// nn::hid button bits (runtime/hle/input.c BTN_*)
enum { B_A = 0, B_B, B_X, B_Y, B_LS, B_RS, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_MINUS, B_LEFT, B_UP, B_RIGHT, B_DOWN };

enum Kind { ROUND, PILL, DPAD, MENU };
struct Control {
    const char *label;
    int bit;             // -1: no button (the menu)
    float x, y, r;       // centre (shares of the screen width / height), radius (share of the height)
    Kind kind;
};
// Landscape layout: face buttons in the Switch's diamond on the right, shoulders in the top corners, - / + and the
// overlay menu at the top centre, the D-pad left above the stick's area (the stick floats under the left thumb)
const Control kControls[] = {
    {"A", B_A, 0.925f, 0.640f, 0.075f, ROUND},  {"B", B_B, 0.855f, 0.800f, 0.075f, ROUND},
    {"X", B_X, 0.855f, 0.480f, 0.075f, ROUND},  {"Y", B_Y, 0.785f, 0.640f, 0.075f, ROUND},
    {"ZL", B_ZL, 0.055f, 0.090f, 0.060f, PILL}, {"L", B_L, 0.150f, 0.090f, 0.060f, PILL},
    {"R", B_R, 0.850f, 0.090f, 0.060f, PILL},   {"ZR", B_ZR, 0.945f, 0.090f, 0.060f, PILL},
    {"-", B_MINUS, 0.400f, 0.075f, 0.050f, ROUND}, {"+", B_PLUS, 0.600f, 0.075f, 0.050f, ROUND},
    {"", -1, 0.500f, 0.075f, 0.050f, MENU},
    {"", -1, 0.085f, 0.420f, 0.115f, DPAD},
};
const int N = sizeof kControls / sizeof *kControls;
const float STICK_R = 0.140f;           // the stick's travel (share of the height)
const float STICK_ZONE_X = 0.42f;       // a touch left of this and below STICK_ZONE_Y grabs the stick
const float STICK_ZONE_Y = 0.55f;
const float STICK_HOME_X = 0.160f, STICK_HOME_Y = 0.740f;

std::atomic<uint64_t> g_buttons{0};
std::atomic<int32_t> g_lx{0}, g_ly{0};
std::atomic<bool> g_visible{false};
std::atomic<float> g_base_x{STICK_HOME_X}, g_base_y{STICK_HOME_Y}, g_knob_x{STICK_HOME_X}, g_knob_y{STICK_HOME_Y};
std::atomic<bool> g_stick_held{false};
float g_aspect = 16.0f / 9;  // width / height of the window (main thread: from the events' window)

// fingers: what each one holds (a control's index, STICK, or nothing)
const int STICK = 1000, NONE = -1;
struct Finger { SDL_FingerID id; int what; uint64_t bits; bool used; };
Finger g_fingers[10];

Finger *finger(SDL_FingerID id, bool make) {
    for (Finger &f : g_fingers)
        if (f.used && f.id == id) return &f;
    if (!make) return nullptr;
    for (Finger &f : g_fingers)
        if (!f.used) { f = {id, NONE, 0, true}; return &f; }
    return nullptr;
}

// distance in height units (x scaled by the aspect)
float dist(float ax, float ay, float bx, float by) { return hypotf((ax - bx) * g_aspect, ay - by); }

// D-pad direction bits for a point relative to its centre
uint64_t dpad_bits(const Control &c, float x, float y) {
    float dx = (x - c.x) * g_aspect, dy = y - c.y;
    if (hypotf(dx, dy) < c.r * 0.25f) return 0;
    float a = atan2f(-dy, dx);  // (up positive)
    uint64_t b = 0;
    const float s = 3.14159265f / 8;  // 8 sectors: the diagonals press two
    if (a > -3 * s && a < 3 * s) b |= 1ull << B_RIGHT;
    if (a > s && a < 7 * s) b |= 1ull << B_UP;
    if (a > 5 * s || a < -5 * s) b |= 1ull << B_LEFT;
    if (a < -s && a > -7 * s) b |= 1ull << B_DOWN;
    return b;
}

int hit(float x, float y) {
    for (int i = 0; i < N; i++) {
        const Control &c = kControls[i];
        float slack = c.kind == DPAD ? 1.0f : 1.25f;  // (a little beyond the drawing: thumbs are wide)
        if (dist(x, y, c.x, c.y) < c.r * slack) return i;
    }
    return NONE;
}

void stick_to(float x, float y) {
    float bx = g_base_x.load(), by = g_base_y.load();
    float dx = (x - bx) * g_aspect, dy = y - by, d = hypotf(dx, dy);
    if (d > STICK_R) dx *= STICK_R / d, dy *= STICK_R / d;
    g_knob_x = bx + dx / g_aspect, g_knob_y = by + dy;
    g_lx = (int32_t)(dx / STICK_R * 32767), g_ly = (int32_t)(-dy / STICK_R * 32767);
}

void recompute() {
    uint64_t b = 0;
    for (const Finger &f : g_fingers)
        if (f.used) b |= f.bits;
    g_buttons = b;
}

void release_all() {
    for (Finger &f : g_fingers) f.used = false;
    g_buttons = 0, g_lx = 0, g_ly = 0;
    g_stick_held = false;
    g_base_x = g_knob_x = STICK_HOME_X, g_base_y = g_knob_y = STICK_HOME_Y;
}

}  // namespace

extern "C" void menu_set_open(bool open);  // runtime/ui/menu.mm

extern "C" bool touch_handle_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_KEY_DOWN:  // a controller / the keyboard: the controls go away
        if (g_visible.exchange(false)) release_all();
        return false;
    default: return false;
    }
    if (SDL_GetTouchDeviceType(e->tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) return false;
    if (SDL_Window *w = SDL_GetWindowFromID(e->tfinger.windowID)) {
        int ww, wh;
        SDL_GetWindowSize(w, &ww, &wh);
        if (ww > 0 && wh > 0) g_aspect = (float)ww / (float)wh;
    }
    float x = e->tfinger.x, y = e->tfinger.y;
    if (e->type == SDL_EVENT_FINGER_DOWN) {
        if (!g_visible.exchange(true)) release_all();  // (shown by this touch: nothing held from before)
        Finger *f = finger(e->tfinger.fingerID, true);
        if (!f) return true;
        int i = hit(x, y);
        if (i != NONE) {
            const Control &c = kControls[i];
            f->what = i;
            if (c.kind == MENU) { f->used = false; menu_set_open(true); release_all(); return true; }
            f->bits = c.kind == DPAD ? dpad_bits(c, x, y) : 1ull << c.bit;
        } else if (x < STICK_ZONE_X && y > STICK_ZONE_Y && !g_stick_held.load()) {
            f->what = STICK;  // the stick's base where the thumb landed
            g_stick_held = true;
            g_base_x = x, g_base_y = y;
            stick_to(x, y);
        } else {
            f->used = false;  // (nothing there: the touch only showed the controls)
        }
        recompute();
        return true;
    }
    Finger *f = finger(e->tfinger.fingerID, false);
    if (!f) return true;
    if (e->type == SDL_EVENT_FINGER_MOTION) {
        if (f->what == STICK) stick_to(x, y);
        else if (f->what >= 0 && kControls[f->what].kind == DPAD) f->bits = dpad_bits(kControls[f->what], x, y);
        else if (f->what >= 0 && dist(x, y, kControls[f->what].x, kControls[f->what].y) > kControls[f->what].r * 2.2f) {
            f->bits = 0;  // (slid off the button: let go, so a thumb rolling to the next one doesn't hold both)
            int j = hit(x, y);
            if (j != NONE && kControls[j].kind == ROUND && kControls[j].bit >= 0) f->what = j, f->bits = 1ull << kControls[j].bit;
        }
    } else {  // up / canceled
        if (f->what == STICK) {
            g_stick_held = false;
            g_lx = 0, g_ly = 0;
            g_base_x = g_knob_x = STICK_HOME_X, g_base_y = g_knob_y = STICK_HOME_Y;
        }
        f->used = false;
    }
    recompute();
    return true;
}

extern "C" bool touch_visible(void) { return g_visible.load(); }
extern "C" void touch_preview(void) { g_visible = true; }
extern "C" void touch_hide(void) {
    if (g_visible.exchange(false)) release_all();
}
extern "C" uint64_t touch_buttons(void) { return g_visible.load() ? g_buttons.load() : 0; }
extern "C" void touch_stick(int32_t *lx, int32_t *ly) {
    bool v = g_visible.load();
    *lx = v ? g_lx.load() : 0, *ly = v ? g_ly.load() : 0;
}

// ---- drawing (render thread, inside the overlay's ImGui frame)
static ImU32 col(int r, int g, int b, float a) { return IM_COL32(r, g, b, (int)fminf(fmaxf(a * 255, 0), 255)); }

extern "C" void touch_draw(void) {
    if (!g_visible.load()) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float W = ds.x, H = ds.y;
    uint64_t held = g_buttons.load();
    ImFont *font = ImGui::GetFont();
    auto text_at = [&](ImVec2 c, float size, const char *t, ImU32 color) {
        ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0, t);
        dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), color, t);
    };
    for (int i = 0; i < N; i++) {
        const Control &c = kControls[i];
        ImVec2 p(c.x * W, c.y * H);
        float r = c.r * H;
        bool on = c.bit >= 0 && (held >> c.bit & 1);
        ImU32 fill = col(20, 24, 30, on ? 0.55f : 0.30f), edge = col(235, 235, 240, on ? 0.95f : 0.55f);
        float th = fmaxf(1.5f, H * 0.004f);
        switch (c.kind) {
        case ROUND:
            dl->AddCircleFilled(p, r, on ? col(110, 200, 190, 0.45f) : fill, 48);
            dl->AddCircle(p, r, edge, 48, th);
            text_at(p, r * 0.95f, c.label, col(255, 255, 255, on ? 1 : 0.85f));
            break;
        case PILL: {
            ImVec2 a(p.x - r * 1.35f, p.y - r * 0.62f), b(p.x + r * 1.35f, p.y + r * 0.62f);
            dl->AddRectFilled(a, b, on ? col(110, 200, 190, 0.45f) : fill, r * 0.62f);
            dl->AddRect(a, b, edge, r * 0.62f, 0, th);
            text_at(p, r * 0.8f, c.label, col(255, 255, 255, on ? 1 : 0.85f));
            break;
        }
        case MENU: {  // three bars: the overlay menu
            dl->AddCircleFilled(p, r, fill, 48);
            dl->AddCircle(p, r, edge, 48, th);
            for (int k = -1; k <= 1; k++)
                dl->AddLine(ImVec2(p.x - r * 0.45f, p.y + k * r * 0.32f), ImVec2(p.x + r * 0.45f, p.y + k * r * 0.32f),
                            col(255, 255, 255, 0.85f), th * 1.3f);
            break;
        }
        case DPAD: {  // a cross; each arm lit while its direction is held
            float a = r * 0.36f;
            static const struct { int bit; float dx, dy; } arms[] = {{B_UP, 0, -1}, {B_DOWN, 0, 1}, {B_LEFT, -1, 0}, {B_RIGHT, 1, 0}};
            for (const auto &arm : arms) {
                ImVec2 q(p.x + arm.dx * r * 0.6f, p.y + arm.dy * r * 0.6f);
                bool lit = held >> arm.bit & 1;
                dl->AddRectFilled(ImVec2(q.x - a, q.y - a), ImVec2(q.x + a, q.y + a), lit ? col(110, 200, 190, 0.5f) : fill, a * 0.35f);
                dl->AddRect(ImVec2(q.x - a, q.y - a), ImVec2(q.x + a, q.y + a), edge, a * 0.35f, 0, th);
                // the arm's arrow
                ImVec2 t1(q.x + arm.dx * a * 0.5f, q.y + arm.dy * a * 0.5f);
                ImVec2 t2(q.x - arm.dx * a * 0.3f + arm.dy * a * 0.45f, q.y - arm.dy * a * 0.3f + arm.dx * a * 0.45f);
                ImVec2 t3(q.x - arm.dx * a * 0.3f - arm.dy * a * 0.45f, q.y - arm.dy * a * 0.3f - arm.dx * a * 0.45f);
                dl->AddTriangleFilled(t1, t2, t3, col(255, 255, 255, lit ? 1 : 0.75f));
            }
            break;
        }
        }
    }
    // the stick: its base, and the knob where the thumb is
    ImVec2 base(g_base_x.load() * W, g_base_y.load() * H), knob(g_knob_x.load() * W, g_knob_y.load() * H);
    bool held_stick = g_stick_held.load();
    float sr = STICK_R * H;
    dl->AddCircleFilled(base, sr, col(20, 24, 30, held_stick ? 0.35f : 0.22f), 64);
    dl->AddCircle(base, sr, col(235, 235, 240, held_stick ? 0.7f : 0.4f), 64, fmaxf(1.5f, H * 0.004f));
    dl->AddCircleFilled(knob, sr * 0.45f, col(235, 235, 240, held_stick ? 0.75f : 0.45f), 48);
}
