// The game's ExeFS modules: one list for the generator (Makefile gen, runtime/gamebuild.c), the game code library's
// tables (tools/gamelib_entry.c), the loader and the dispatcher. Guest addresses are host addresses.
//
// Order is the order Horizon's rtld loads them in (main, subsdk*, sdk), which is also the symbol lookup order.
// Bases: main's image is 0x4004000 bytes (Echoes of Wisdom 1.0.0), so it gets 128 MB; the others 32 MB each.
// rtld isn't loaded: the runtime does its job (relocation, module init) itself.
#pragma once

#define GAME_MODULES(X)            \
    X(main, 0x7000000000ull)       \
    X(subsdk0, 0x7008000000ull)    \
    X(subsdk1, 0x700A000000ull)    \
    X(sdk, 0x700C000000ull)
#define GAME_NMODULES 4
#define GAME_MODULE_SPAN 0x2000000ull  // room after every base but main's (checked by the loader)
