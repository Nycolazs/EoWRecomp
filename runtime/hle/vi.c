// nn::vi (display/layer) and nv:: (graphics driver init), implemented on the host.
#include <stdlib.h>
#include <time.h>

#include "../hle.h"
#include "../kernel.h"

static uint32_t vsync_r, vsync_w;
static KEvent *vsync_event;
uint32_t vi_vsync_handle(void) { return vsync_r; }
volatile uint64_t g_vsync_count;  // vsyncs signalled so far (runtime/hle/framepace.c)
uint64_t g_native_window;
static pthread_mutex_t vsync_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t vsync_cond = PTHREAD_COND_INITIALIZER;

uint64_t vi_vsync_count(void) { return __atomic_load_n(&g_vsync_count, __ATOMIC_ACQUIRE); }

// Block until vsync n has been signalled (nvn/objects.c: the window's display queue). Counted as blocked time, as a
// kernel wait is (the frame pacer's busy time, nvn/vulkan/exec.c); gives up when guest threads are being stopped.
void vi_wait_vsync(uint64_t n) {
    if (vi_vsync_count() >= n) return;
    extern __thread uint64_t g_blocked_ns;
    uint64_t t0 = plat_now_ns();
    pthread_mutex_lock(&vsync_lock);
    while (vi_vsync_count() < n && !g_rt_unwind) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 50000000;  // (re-check the unwind flag now and then)
        if (ts.tv_nsec >= 1000000000) ts.tv_sec++, ts.tv_nsec -= 1000000000;
        pthread_cond_timedwait(&vsync_cond, &vsync_lock, &ts);
    }
    pthread_mutex_unlock(&vsync_lock);
    g_blocked_ns += plat_now_ns() - t0;
}

static void *vsync_thread(void *arg) {
    (void)arg;
    const uint64_t period = 16666667;
    plat_thread_realtime(period);
    uint64_t next = plat_now_ns();
    for (;;) {
        next += period;
        plat_sleep_until_ns(next);
        uint64_t now = plat_now_ns();
        if (now > next + period * 3) {
            static int warned;
            if (warned++ < 20) LOG("[vi] vsync timer late by %llums (host throttling?)\n", (unsigned long long)((now - next) / 1000000));
            next = now;  // don't burst-signal to catch up
        }
        pthread_mutex_lock(&vsync_lock);
        __atomic_add_fetch(&g_vsync_count, 1, __ATOMIC_RELEASE);
        pthread_cond_broadcast(&vsync_cond);
        pthread_mutex_unlock(&vsync_lock);
        pthread_mutex_lock(&g_klock);
        kernel_signal(vsync_event);
        pthread_mutex_unlock(&g_klock);
    }
    return NULL;
}

HLE_FUNC("_ZN2nn2vi10InitializeEv", vi_initialize) { (void)c; }
HLE_FUNC("_ZN2nn2vi8FinalizeEv", vi_finalize) { (void)c; }

HLE_FUNC("_ZN2nn2vi18OpenDefaultDisplayEPPNS0_7DisplayE", vi_open_default_display) {
    *(uint64_t *)(uintptr_t)c->x[0] = (uint64_t)hle_alloc(0x100);
    c->x[0] = 0;
}

HLE_FUNC("_ZN2nn2vi12CloseDisplayEPNS0_7DisplayE", vi_close_display) { (void)c; }

HLE_FUNC("_ZN2nn2vi11CreateLayerEPPNS0_5LayerEPNS0_7DisplayE", vi_create_layer) {
    *(uint64_t *)(uintptr_t)c->x[0] = (uint64_t)hle_alloc(0x100);
    c->x[0] = 0;
}

HLE_FUNC("_ZN2nn2vi12DestroyLayerEPNS0_5LayerE", vi_destroy_layer) { (void)c; }
HLE_FUNC("_ZN2nn2vi12SetLayerCropEPNS0_5LayerEiiii", vi_set_layer_crop) { c->x[0] = 0; }

HLE_FUNC("_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE", vi_get_native_window) {
    if (!g_native_window) g_native_window = (uint64_t)hle_alloc(0x100);
    *(uint64_t *)(uintptr_t)c->x[0] = g_native_window;
    c->x[0] = 0;
}

// Reboot: the vsync timer and its event live on (the kernel's handle table doesn't): new handles at the next boot.
RT_ON_RESET(vi_reset) {
    vsync_r = vsync_w = 0;
    g_native_window = 0;  // (HLE heap)
}

HLE_FUNC("_ZN2nn2vi20GetDisplayVsyncEventEPNS_2os15SystemEventTypeEPNS0_7DisplayE", vi_get_vsync_event) {
    if (!vsync_event) {
        vsync_event = kernel_create_event_pair(&vsync_w, &vsync_r);
        pthread_t t;
        pthread_create(&t, NULL, vsync_thread, NULL);
    } else if (!vsync_r) {
        vsync_w = kernel_new_handle(vsync_event);
        vsync_r = kernel_new_handle(vsync_event);
    }
    // nn::os::AttachReadableHandleToSystemEvent(event, handle, managed=false, clear_mode=AutoClear)
    c->x[1] = vsync_r;
    c->x[2] = 0;
    c->x[3] = 1;
    guest_call(c, guest_sym("_ZN2nn2os33AttachReadableHandleToSystemEventEPNS0_15SystemEventTypeEjbNS0_14EventClearModeE"));
    c->x[0] = 0;
}

HLE_FUNC("_ZN2nv18InitializeGraphicsEPvm", nv_initialize_graphics) { (void)c; }
HLE_FUNC("_ZN2nv16FinalizeGraphicsEv", nv_finalize_graphics) { (void)c; }
HLE_FUNC("_ZN2nv20SetGraphicsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", nv_set_allocator) { (void)c; }
HLE_FUNC("_ZN2nv28SetGraphicsDevtoolsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", nv_set_dt_allocator) { (void)c; }
