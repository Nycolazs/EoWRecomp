# GAP: o que o EoW usa vs. o que o runtime implementa

Gerado por medição: `tools/venv/bin/python tools/inventory.py extracted/exefs [--lift]` (as tabelas abaixo são a
saída da ferramenta, de 2026-10-19). Rodar de novo depois de mudanças no runtime.

## Resumo e plano

| Área | Situação medida | O que falta | Marco |
|---|---|---|---|
| Lifter (AArch64 → C) | **0 instruções não suportadas** nos 4 módulos (13,5 M palavras); as 287 do `sdk` (LDXP/STXP, CAS/CASP, LSE) implementadas e testadas contra o Unicorn | — | M1 ✔ |
| Gerador | 4 módulos, 326 114 funções, ~900 MB de C; 0 `Unsupported` | jump tables: no `main` só 71 resolvidas, 1 832 no fallback "todas as instruções" (lento, C inflado) | otimização |
| Boot | `nn::init::Start` mudou para 3 callbacks no SDK 16.2 (tratado) | validar no M2 | M2 |
| IPC | `sm:` via TIPC (implementado); 21 serviços com HLE | os marcados **stub** abaixo, conforme o jogo os abrir (log `[ipc] no HLE implementation`) | M2–M5 |
| SVCs | `sdk` tem 60 wrappers, 41 implementadas | as 19 restantes só se forem chamadas (o kernel aborta com `unimplemented svc`) | M2 |
| NVN | 299 nomes, 263 implementados | 34 (lista abaixo; a maioria NVX/debug/checkpoints, sem efeito visual) | M3–M4 |
| glslc (`subsdk0`) | `main` importa `glslcInitialize/Compile/...` | ver se é chamado em runtime; se for, HLE com shaderc | M3–M4 |
| Vídeo (`subsdk1`) | os 28 imports `movie::` = as 28 funções de `hle/movie.c` | conferir formatos (os vídeos são `.webm` VP9) | M5 |

## Modules

| module | build id | .text | .rodata | .data+bss | image | syms | imports | exports | SDK strings |
|---|---|---|---|---|---|---|---|---|---|
| main | `a581406bc9858651` | 0x27c8920 | 0x13b48fc | 0x486000 | 0x4004000 | 758 | 711 | 46 | NVIDIA+PhysX-3_4_0-release, Nintendo+NintendoSDK_gfx-16_2_0-Release, Nintendo+NintendoWare_Atk-16_2_0-Release, Nintendo+NintendoWare_Font-16_2_0-Release, Nintendo+NintendoWare_G3d-16_2_0-Release, Nintendo+NintendoWare_Ui2d-16_2_0-Release, Nintendo+NintendoWare_Vfx-16_2_0-Release |
| sdk | `3053b22935e32ddf` | 0x5e75c0 | 0x6fbc38 | 0x120000 | 0xe04000 | 27633 | 13 | 27619 | Nintendo+NintendoSDK_NVN-16_2_0-Release, Nintendo+NintendoSDK_libz-16_2_0-Release, Nintendo+NintendoSdk_BuildTarget-64Bit, Nintendo+NintendoSdk_nnSdk-16_2_0-Release |
| subsdk0 | `e0d8adcc446cf14d` | 0x686a70 | 0x308000 | 0xfc000 | 0xa8b000 | 111 | 90 | 20 | - |
| subsdk1 | `ff0149d0b5e882b8` | 0x345b00 | 0x220064 | 0x14b000 | 0x6b2000 | 11625 | 667 | 10957 | Nintendo+NintendoSDK_movie-16_2_0-Release |

## Imports by exporting module

- **main**: {'unresolved': 4, 'sdk': 671, 'subsdk1': 28, 'subsdk0': 8}; bound to host HLE: 64; unresolved: __rel_plt_end, __rel_plt_start, __rel_dyn_end, __rel_dyn_start
- **sdk**: {'unresolved': 5, 'main': 8}; bound to host HLE: 0; unresolved: __rel_plt_end, __rel_plt_start, __rel_dyn_end, __rel_dyn_start, nnosGetDefaultUserExceptionStackForApplication
- **subsdk0**: {'unresolved': 4, 'sdk': 85, 'main': 1}; bound to host HLE: 0; unresolved: __rel_plt_end, __rel_plt_start, __rel_dyn_end, __rel_dyn_start
- **subsdk1**: {'unresolved': 4, 'sdk': 660, 'main': 3}; bound to host HLE: 2; unresolved: __rel_plt_end, __rel_plt_start, __rel_dyn_end, __rel_dyn_start

## NVN entry points named in the modules

299 names, 263 implemented, 34 missing:

- `nvnCUDAInteropFunction`
- `nvnCommandBufferBindSeparateSamplersDeferred`
- `nvnCommandBufferBindSeparateTexturesDeferred`
- `nvnCommandBufferBindTexturesDeferred`
- `nvnCommandBufferBindUniformBuffersDeferred`
- `nvnCommandBufferCopyTextureToTextureWithCopyEngineNVX`
- `nvnCommandBufferInsertCheckpointStatic`
- `nvnCommandBufferSetProvokingVertexMode`
- `nvnCommandBufferSetZCullZF32CompressionEnable`
- `nvnDeviceBuilderGetReservedAddressRangesNVX`
- `nvnDeviceBuilderSetReservedAddressRangesNVX`
- `nvnDeviceSetAutomaticCheckpointsByDebugGroup`
- `nvnDeviceSetAutomaticCheckpointsEnable`
- `nvnDeviceSetAutomaticCheckpointsMask`
- `nvnDeviceSetCheckpointMaxCallStacksDepth`
- `nvnDevtoolsBootstrap`
- `nvnInstallGlobalDebugCallback`
- `nvnInterceptionInitializeNVX`
- `nvnInterceptionSetDispatchResetCallbackNVX`
- `nvnInterceptionSetProcAddressNVX`
- `nvnInterceptionTryAcquireNVX`
- `nvnMemoryPoolBuilderGetNativeHandleNVX`
- `nvnMemoryPoolBuilderGetReservedAddressesNVX`
- `nvnMemoryPoolBuilderSetNativeHandleNVX`
- `nvnMemoryPoolBuilderSetPitchAddressNVX`
- `nvnMemoryPoolBuilderSetSpecialAddressNVX`
- `nvnMemoryPoolGetSpecialAddressNVX`
- `nvnQueueSetTimeoutNVX`
- `nvnSetExternalDebuggerEnabledNVX`
- `nvnSyncGetNvRmSyncNVX`
- `nvnSyncGetSyncpointsCountNVX`
- `nvnSyncInitializeFromNvRmSyncNVX`
- `nvnSyncSetSyncpointsNVX`
- `nvnTextureGetNvRmSurfaceNVX`

## HOS service names present as strings

| service | in modules | runtime |
|---|---|---|
| `acc:aa` | sdk | **stub** |
| `acc:u0` | sdk | implemented |
| `acc:u1` | sdk | **stub** |
| `aoc:u` | sdk | implemented |
| `apm` | sdk | implemented |
| `appletOE` | sdk | implemented |
| `auddev` | sdk | **stub** |
| `audin:u` | sdk | **stub** |
| `audout:u` | sdk | implemented |
| `audrec:u` | sdk | **stub** |
| `audren:u` | sdk | implemented |
| `bcat:s` | sdk | **stub** |
| `bsd:s` | sdk | **stub** |
| `bsd:u` | sdk | **stub** |
| `btdrv` | sdk | **stub** |
| `btm:u` | sdk | **stub** |
| `caps:su` | sdk | **stub** |
| `caps:u` | sdk | **stub** |
| `ectx:aw` | sdk | **stub** |
| `friend:a` | sdk | **stub** |
| `friend:u` | sdk | **stub** |
| `fsp-ldr` | sdk | **stub** |
| `fsp-pr` | sdk | **stub** |
| `fsp-srv` | sdk | implemented |
| `hid` | sdk | implemented |
| `hid:dbg` | sdk | **stub** |
| `hid:sys` | sdk | implemented |
| `hidbus` | sdk | **stub** |
| `ldn:u` | sdk | **stub** |
| `lm` | main, sdk, subsdk1 | implemented |
| `lr` | main, sdk | **stub** |
| `mii:e` | sdk | **stub** |
| `mii:u` | sdk | **stub** |
| `mm:u` | subsdk1 | **stub** |
| `nfc:user` | sdk | **stub** |
| `nfp:user` | sdk | **stub** |
| `ngc:u` | sdk | **stub** |
| `ngct:u` | sdk | **stub** |
| `nifm:a` | sdk | **stub** |
| `nsd:u` | sdk | **stub** |
| `nvdrv` | sdk | **stub** |
| `nvmemp` | sdk | **stub** |
| `pctl` | sdk | implemented |
| `pctl:a` | sdk | implemented |
| `pctl:r` | sdk | implemented |
| `pctl:s` | sdk | implemented |
| `pl:s` | sdk | **stub** |
| `pl:u` | sdk | **stub** |
| `prepo:u` | sdk | **stub** |
| `set` | sdk | implemented |
| `set:cal` | sdk | **stub** |
| `set:sys` | sdk | implemented |
| `sfdnsres` | sdk | **stub** |
| `sm:` | sdk | implemented |
| `ssl` | sdk | **stub** |
| `tc` | sdk | **stub** |
| `time:a` | sdk | implemented |
| `time:s` | sdk | implemented |
| `time:u` | sdk | implemented |
| `vi:m` | sdk | **stub** |
| `vi:s` | sdk | **stub** |
| `vi:u` | sdk | **stub** |

## SVCs (svc #imm sites)

- **sdk**: 60 distinct, 41 implemented; not implemented: 0x23 0x28 0x2A 0x2F 0x32 0x33 0x36 0x40 0x41 0x43 0x44 0x48 0x49 0x4B 0x50 0x51 0x52 0x70 0x72

## Lifter census (`--lift`, `--simd c`)

Antes deste marco: `sdk` 287 não suportadas (207 pares exclusivos / CAS, 80 atômicos LSE); demais módulos 0.
Depois: 0 em todos (`tools/test_lift.py` com `ONLY=` para as novas: 127 codificações, 0 falhas).
