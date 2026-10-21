cmake_minimum_required(VERSION 3.22)
# The Android app's native side: libmain.so (the runtime: kernel, services, HLE, NVN -> Vulkan, the start screen and
# menus) and libSDL3.so, which SDLActivity loads. The game code is not in here: it is libeowgame.so, built from the
# player's own game files (tools/android/build_game.sh) and loaded at startup (runtime/gamelib.c).
# Needs tools/android/build_deps.sh first (FFmpeg, shaderc, Vulkan headers, VMA, volk into build/android/deps, SDL3
# sources into build/android/SDL3). (After LASRecomp-Android-Plus, MIT.)
project(eowrecomp C CXX)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(DEPS ${ROOT}/build/android/deps)
if(NOT EXISTS ${DEPS}/lib/libshaderc_combined.a OR NOT EXISTS ${ROOT}/build/android/SDL3/CMakeLists.txt)
  message(FATAL_ERROR "run tools/android/build_deps.sh first")
endif()

# no paths of the build machine in the binaries
add_compile_options("-ffile-prefix-map=${ROOT}/=")

# ---- SDL3 (its own CMake project; SDLActivity's Java sources come from the same tree, android/app/build.gradle)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${ROOT}/build/android/SDL3 ${CMAKE_BINARY_DIR}/SDL3)

# ---- the game code library's id (runtime/gamelib.h): the same script makes the library's
execute_process(COMMAND ${ROOT}/tools/android/gamelib_id.sh OUTPUT_VARIABLE GAMELIB_ID OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${ROOT}/tools/android/gamelib_id.sh --flags OUTPUT_VARIABLE GAMELIB_FLAGS
                OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
foreach(f tools/gen.py tools/lifter.py tools/analyze.py tools/nso.py tools/passes.py tools/patches.py tools/simd_c.py
          tools/gamelib_entry.c runtime/hle/hle_symbols.txt runtime/cpu.h runtime/simd.h runtime/simd_fp.h
          runtime/gamelib.h runtime/modules.h tools/android/gamelib_id.sh)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ROOT}/${f})
endforeach()
message(STATUS "game code library id ${GAMELIB_ID}")

# ---- SMAA's reference shader as a C string (as the Makefile: non-ASCII -> '?', no CR)
set(SMAA_INC ${CMAKE_BINARY_DIR}/inc/smaa_hlsl.inc)
add_custom_command(OUTPUT ${SMAA_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/inc
  COMMAND sh -c "LC_ALL=C tr -d '\\r' < '${ROOT}/third_party/smaa/SMAA.hlsl' | LC_ALL=C tr '\\200-\\377' '?' | sed -e 's/\\\\/\\\\\\\\/g' -e 's/\"/\\\\\"/g' -e 's/^/\"/' -e 's/$/\\\\n\"/' > '${SMAA_INC}'"
  DEPENDS ${ROOT}/third_party/smaa/SMAA.hlsl VERBATIM)

# ---- the runtime
file(GLOB RT_SRC CONFIGURE_DEPENDS ${ROOT}/runtime/*.c ${ROOT}/runtime/hle/*.c ${ROOT}/runtime/services/*.c ${ROOT}/runtime/nvn/*.c
     ${ROOT}/runtime/nvn/maxwell/*.c ${ROOT}/runtime/nvn/vulkan/*.c)
file(GLOB RT_CXX CONFIGURE_DEPENDS ${ROOT}/runtime/*.cpp ${ROOT}/runtime/nvn/vulkan/*.cpp)
file(GLOB UI_SRC CONFIGURE_DEPENDS ${ROOT}/runtime/ui/*.mm)
set(IMGUI ${ROOT}/third_party/imgui)
set(IMGUI_SRC ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
              ${IMGUI}/backends/imgui_impl_sdl3.cpp ${IMGUI}/backends/imgui_impl_vulkan.cpp)

add_library(main SHARED ${RT_SRC} ${ROOT}/runtime/platform/android.c ${ROOT}/runtime/platform/android_jni.c ${RT_CXX} ${UI_SRC}
                        ${IMGUI_SRC} ${DEPS}/include/volk/volk.c ${SMAA_INC})
# the start screen / menu sources are plain C++ apart from their EOW_METAL blocks (as the Makefile's UIFLAGS)
set_source_files_properties(${UI_SRC} PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-x;c++")
target_include_directories(main PRIVATE ${ROOT}/runtime ${ROOT}/runtime/nvn ${IMGUI} ${IMGUI}/backends ${DEPS}/include
                                        ${DEPS}/include/volk ${CMAKE_BINARY_DIR}/inc ${ROOT}/third_party/smaa/Textures)
# Vulkan through volk: entry points loaded at run time (Android's libvulkan exports only the API level's core
# functions)
target_compile_definitions(main PRIVATE VK_NO_PROTOTYPES VK_USE_PLATFORM_ANDROID_KHR IMGUI_IMPL_VULKAN_USE_VOLK EOW_RUNTIME
                                        "GAMELIB_ID=\"${GAMELIB_ID}\"" "GAMELIB_CFLAGS=\"${GAMELIB_FLAGS}\""
                                        "GAMELIB_SHARED=\"-shared\"")
target_compile_options(main PRIVATE -fsigned-char -fno-strict-aliasing
                                    $<$<COMPILE_LANGUAGE:C>:-Wall -Wno-unused-function -Wno-format>
                                    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-format>)
# 16 KB page devices (Android 15+); -Bsymbolic: calls inside the library bind directly (FFmpeg's asm needs it in a
# shared library); the static libraries' own symbols aren't exported (the runtime's are: the game code library and
# the crash backtraces use them)
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,-Bsymbolic" "-Wl,--exclude-libs,ALL"
                                 "-Wl,-z,nostart-stop-gc")  # (the HLE / service / reset tables: sections kept whole)
target_link_libraries(main PRIVATE SDL3::SDL3-shared
                      ${DEPS}/lib/libavformat.a ${DEPS}/lib/libavcodec.a ${DEPS}/lib/libswscale.a ${DEPS}/lib/libavutil.a
                      ${DEPS}/lib/libshaderc_combined.a android log m dl)
