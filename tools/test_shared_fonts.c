// Drive the actual font loader concurrently. A second request must not observe a half-published font.
#include <pthread.h>
#include "../runtime/hle.h"
#undef HLE_FUNC
#undef RT_ON_RESET
#define HLE_FUNC(name, fn) static void fn(Cpu *c)
#define RT_ON_RESET(fn) static void fn(void)
#include "../runtime/hle/pl.c"

static const char *standard_path, *chinese_path;
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate = PTHREAD_COND_INITIALIZER;
static int loading;

void *hle_alloc(uint64_t size) {
    if (size > (1u << 20)) {
        pthread_mutex_lock(&gate_lock);
        loading = 1; pthread_cond_broadcast(&gate);
        pthread_mutex_unlock(&gate_lock);
        struct timespec delay = {0, 100000000}; nanosleep(&delay, NULL);
    }
    return calloc(1, (size_t)size);
}
bool rt_resource(const char *name, const char *dev, char *out, size_t cap) {
    (void)dev;
    snprintf(out, cap, "%s", !strcmp(name, "EowSansSC-Regular.ttf") ? chinese_path : standard_path);
    return true;
}
const char *settings_language(void) { return "en-US"; }
void rt_fatal_trace(void) { abort(); }

typedef struct { uint64_t addr, size; } Seen;
static void *request(void *arg) {
    Seen *seen = arg;
    const Font *font = font_for(1);
    seen->addr = font->addr; seen->size = font->size;
    return NULL;
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    standard_path = chinese_path = argv[1];
    setenv("EOWRECOMP_FONT", standard_path, 1);
    Seen first = {0}, second = {0};
    pthread_t a, b;
    pthread_create(&a, NULL, request, &first);
    pthread_mutex_lock(&gate_lock);
    while (!loading) pthread_cond_wait(&gate, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    pthread_create(&b, NULL, request, &second);
    pthread_join(a, NULL); pthread_join(b, NULL);
    bool ok = first.addr && first.addr == second.addr && first.size == second.size && first.size > (1u << 20);
    printf("%s concurrent CJK requests: sizes %llu / %llu; same address %d\n", ok ? "PASS" : "FAIL",
           (unsigned long long)first.size, (unsigned long long)second.size, first.addr == second.addr);
    return ok ? 0 : 1;
}
