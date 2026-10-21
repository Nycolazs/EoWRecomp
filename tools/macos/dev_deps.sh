#!/bin/bash
# tools/macos/dev_deps.sh: the macOS development setup (Apple Silicon). Homebrew packages for the Vulkan backend
# (MoltenVK, the Vulkan loader and headers, shaderc), SDL3 and FFmpeg; Vulkan Memory Allocator (no Homebrew formula)
# from its pinned release into build/deps-macos/include/vma; the Python tools in tools/venv.
set -euo pipefail
cd "$(dirname "$0")/../.."
HOMEBREW_NO_AUTO_UPDATE=1 brew install sdl3 ffmpeg molten-vk vulkan-loader vulkan-headers shaderc python@3.11
D=build/deps-macos
mkdir -p "$D/src" "$D/include/vma"
if [ ! -f "$D/include/vma/vk_mem_alloc.h" ]; then
    T=$D/src/VulkanMemoryAllocator-3.3.0.tar.gz
    [ -f "$T" ] || curl -fsSL -o "$T" https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz
    echo "c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e  $T" | shasum -a 256 -c --quiet
    tar -xf "$T" -C "$D/src"
    cp "$D/src/VulkanMemoryAllocator-3.3.0/include/vk_mem_alloc.h" "$D/include/vma/"
fi
[ -x tools/venv/bin/python ] || /opt/homebrew/bin/python3.11 -m venv tools/venv
tools/venv/bin/pip install -q capstone unicorn lz4 cryptography pillow
echo "ok: make gen && make -j\$(sysctl -n hw.ncpu)"
