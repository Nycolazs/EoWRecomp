# Arquitetura do EoWRecomp (herdada do LASRecomp)

Base: LASRecomp commit `d23597d` (2026-10-18), estudado arquivo por arquivo. Este documento confirma/corrige a
descrição inicial do projeto e lista o que é reaproveitado, adaptado ou novo. Números marcados "medido" vêm de
ferramentas rodadas sobre o dump (comandos na seção final), não de estimativa.

## 1. Pipeline

```
XCI ──hactool──> NCAs ──tools/nca_extract.py──> extracted/exefs (NSOs) + extracted/romfs
extracted/exefs/<módulo> ──tools/gen.py──> generated/<módulo>/*.c ──clang──> eowgame.{so,dll,dylib}
runtime (eowrecomp[.exe]) ── carrega NSOs na memória do guest, reloca, dlopen(eowgame) ── nn::init::Start
```

### Recompilador (Python, `tools/`)
- **`nso.py`** — lê o NSO (header, 3 segmentos LZ4, MOD0, `.dynamic`, dynsym via `DT_HASH`, relocs RELA/JMPREL).
  Confirmado.
- **`analyze.py`** — descoberta de funções. *Correção:* não é só `.eh_frame`: une `.eh_frame_hdr` (tabela de busca
  binária), símbolos `STT_FUNC`, alvos de relocs `R_AARCH64_RELATIVE` que caem em `.text` (vtables/callbacks) e alvos
  de `BL`. Também faz o censo de instruções (`--census`).
- **`gen.py`** — além dessas fontes, adiciona alvos de `ADR` e `ADRP+ADD`, e itera até um ponto fixo: todo
  branch/call para fora da função corrente vira nova fronteira (divide a função). Jump tables (`br xN`) são resolvidas
  emulando o trecho com **Unicorn** para cada índice (formas "fallthrough" com `b.hi/b.hs` e "taken" com `b.ls/b.lo`);
  se não resolver, a função ganha um `switch` sobre *todas* as instruções (`L_dispatch`) como fallback.
  *Correção:* não gera um arquivo por função — gera **chunks** (`<módulo>_NNN.c`, até 1500 funções / ~1,5 MB de C por
  arquivo; funções grandes ganham arquivo próprio) mais `<módulo>_table.c` (endereços ordenados + ponteiros). Cada
  função é `void f_<endereço>(Cpu *c)`; chamadas diretas são chamadas C, saltos para fora são `musttail`, indiretos
  passam por `rt_dispatch`. Só regrava arquivos que mudaram (rebuild incremental).
- **`lifter.py`** — AArch64 → C. Inteiros, load/store, flags NZCV, exclusivos (LDXR/STXR emulados com CAS no host)
  viram C; FP/SIMD/CRC32 vira inline asm em host ARM64 (`--simd asm`) ou C portátil (`--simd c`: `simd_c.py` +
  `runtime/simd.h` + `simd_fp.h`, exige AVX2/FMA: `-march=x86-64-v3`). Endereço do guest == endereço do host
  (`LD64(a)` é um ponteiro direto). Não suporta: pares exclusivos (`LDXP/STXP`), LSE (`CAS/SWP/LDADD…`).
- **`patches.py`** — substituição por instrução (endereço → C), aplicada na geração, sempre conferindo a palavra
  original (`expect()`). No EoWRecomp está vazio (só o mecanismo); os do LA foram removidos.
- **`passes.py`**, `simd_c.py`, testes `test_lift.py`, `test_simd.py`, `test_translate.c`, `test_shaders.*`,
  `test_astc.*`, `test_vk_pipelines.*`.

### Código do jogo como biblioteca separada (não estava na descrição)
O código gerado compila para `eowgame.so/.dll/.dylib`, carregado em runtime (`gamelib.c`, entrada
`tools/gamelib_entry.c`, id = hash do gerador + headers + flags). O executável não contém código do jogo; a tela
inicial pode gerar e compilar a biblioteca na máquina do usuário (`gamebuild.c`, gerador empacotado com PyInstaller,
`zig cc` como compilador).

### Runtime (`runtime/`)
- **Espaço de endereços** (`rt.h`): janela fixa `0x7000000000–0x7320000000` reservada no início do processo
  (re-exec se ocupada). `main` em `0x7000000000`, `sdk` em `0x7004000000`, HLE em `0x7080000000` (endereços falsos
  de funções do host), TLS, heap de HLE, alias, heap do jogo em `0x7240000000` (máx. 3,25 GB).
- **`loader.c`** — carrega `main` e `sdk` (lista fixa), descomprime, aplica relocs; imports são resolvidos primeiro
  contra implementações do host (`HLE_FUNC` por nome de símbolo), depois contra símbolos do guest; o que sobra vira
  stub "UNIMPLEMENTED" que retorna 0 e loga.
- **`main.c`** — não usa o `rtld`: chama `nn::init::Start` do `sdk` diretamente e passa callbacks falsos que rodam
  `DT_INIT` / `init_array` de cada módulo, como o rtld faria. Prioridade/stack da thread principal vêm do `main.npdm`.
- **`dispatch.c`** — endereço do guest → função recompilada (busca binária em 2 tabelas + cache direto de 4096
  entradas por thread), registro de HLE por nome (`HLE_FUNC` numa seção do linker), overrides por endereço,
  `guest_call`, backtraces e watchpoints de depuração. `hle_symbols.txt` permite substituir funções *internas* do
  `sdk` por símbolo na geração (vazio no LAS).
- **`kernel.c`** — 43 SVCs: threads = pthreads (prioridade/núcleo só registrados), mutex/condvar do Horizon
  (`ArbitrateLock`, `WaitProcessWideKey`), `WaitForAddress/SignalToAddress`, eventos, `WaitSynchronization`,
  memória (heap, map/unmap por cópia, shared/transfer memory, physical memory), `GetInfo`, `QueryMemory`, ticks de
  19,2 MHz. Suporta "reboot" no próprio processo (unwind das threads em pontos seguros).
- **`ipc.c` + `services/`** — HIPC/CMIF com domains e controle (`ConvertCurrentObjectToDomain`, clone,
  `QueryPointerBufferSize`); `sm:` via `ConnectToNamedPort`. Serviços: `acc:u0`, `fsp-srv`, `appletOE`, `apm`, `lm`,
  `audren:u`, `audout:u`, `time:*`, `hid`, `hid:sys`, `set`, `set:sys`, `pctl*`, `aoc:u`; os demais caem num stub que
  loga e responde OK com zeros. *Não há TIPC.*
- **HLE de SDK** (`hle/`): `nn::vi` / `nv::` (display), `nn::hid` (estado dos controles a partir do SDL), `nn::pl`
  (fontes do sistema), `nn::swkbd`, `movie::` (vídeo com FFmpeg), `nvnBootstrapLoader` / `nvnDeviceGetProcAddress`,
  heaps do motor (`heaps.c`), frame pacing.
- **Gráficos** (`nvn/`): a **API NVN é reimplementada em alto nível** (não se emula o push buffer da GPU). 516
  funções `nvn*` (`objects.c`: device, memory pools, texturas, samplers, programas, filas, sync, janela;
  `cmdbuf.c`: comandos gravados como registros `CmdRec` dentro da própria memória de comandos do guest). As listas
  são executadas pelo backend: **Metal é o primário** (`renderer.m`, `metal.m`); `vulkan/exec.c` é um port do mesmo
  executor (VMA, swapchain, descriptor sets, cache de pipelines). Texturas: layout block-linear da Tegra
  (`tegra.c`), ASTC decodificado em software (`astc.c`). **Shaders**: binário Maxwell (header NVN + SPH + código)
  → MSL, e GLSL por reescrita do MSL (`maxwell/translate.c`); GLSL → SPIR-V em runtime com **shaderc**, com cache
  em disco (`cache/spirv/`). Upscalers (bilinear, FSR1, MetalFX), SMAA.
- **Host**: SDL3 (janela, input, áudio), FFmpeg (vídeos), Dear ImGui (menu F1 e tela inicial), plataforma
  (`platform/{linux,windows,macos}.c`: reserva de memória, crash handler, backtrace).
- **Testes roteirizados**: variáveis `EOWRECOMP_*` (input gravado, screenshot no frame N, profilers), `tools/shot.sh`,
  `tools/frames.sh`.

## 2. O que o EoW tem de diferente (medido)

| | Link's Awakening (LAS) | Echoes of Wisdom (dump) |
|---|---|---|
| Módulos ExeFS | main, sdk | **main, sdk, subsdk0 (glslc), subsdk1 (multimedia)**, rtld |
| SDK | (mais antigo) | **NintendoSDK 16.2.0** (nnSdk, NVN, gfx 16_2_0) |
| `main` | cabe em 64 MB | imagem **0x4004000** (64 MB + 16 KB): passa de `SDK_BASE` |
| `.text` main / sdk | — | 39,8 MB / 6,2 MB (+ 6,8 MB glslc, 3,4 MB multimedia) |
| RomFS | — | 7,48 GiB, 10 963 arquivos, **comprimido** no NCA |

Imports do `main` (711): 671 do `sdk`, 28 do `multimedia` (exatamente as 28 funções `movie::` que `hle/movie.c` já
implementa), 8 do `glslc` (`glslcInitialize`, `glslcCompile`, `glslcCompileSpecialized`, …). Nomes `nvn*` no
`main`: 276, dos quais **263 já implementados** no LAS; faltam 11 (+2 de bootstrap, já tratados):
`CommandBufferBind{SeparateSamplers,SeparateTextures,Textures,UniformBuffers}Deferred`, `InsertCheckpointStatic`,
`SetProvokingVertexMode`, `SetZCullZF32CompressionEnable`, `DeviceSetAutomaticCheckpoints{ByDebugGroup,Enable,Mask}`,
`DeviceSetCheckpointMaxCallStacksDepth`. SVCs com wrapper no `sdk`: 60, 41 implementadas (as 19 restantes podem nunca
ser chamadas; o M2 mostra). Pares exclusivos/LSE no `sdk`: 187 `ldxp/stxp/ldaxp/stlxp`, 16 `cas*/swp*`; nenhum no
`main`.

## 3. Reaproveitar, adaptar, novo

**Reaproveitar sem mudança (esperado)**
- `nso.py`, `analyze.py`, núcleo do `lifter.py` (inteiros, memória, flags, branches), `simd_c.py` + `simd.h` +
  `simd_fp.h`, resolução de jump tables do `gen.py`, mecanismo de `patches.py`.
- `cpu.h`, despacho e registro HLE (`dispatch.c`, `hle.h`), primitivas de sincronização e threads do `kernel.c`,
  codec CMIF/domains do `ipc.c`.
- NVN: `objects.c`, `cmdbuf.c` (263/276 nomes cobertos), `tegra.c`, `astc.c`, `formats.c`, `maxwell/translate.c`,
  backend Vulkan (`vulkan/*`), `output_scale.c`, SMAA/FSR.
- `hle/movie.c` (mesmos 28 símbolos), `hle/vi.c`, `hle/swkbd.c`, `hle/pl.c`, `hle/nvn_boot.c`.
- Camada de plataforma, gamelib/gamebuild, ImGui (menu, controles, OSK), scripts de teste.

**Adaptar**
- Layout de memória (`rt.h`): `main` não cabe antes de `SDK_BASE`; quatro módulos; revisar tamanho do heap.
- `loader.c` / `main.c`: lista de módulos lida do ExeFS, ordem de init como o rtld (todos os módulos), resolução de
  símbolos entre 4 módulos.
- `gen.py` / `Makefile gen` / `gamelib.h` / `gamelib_entry.c` / `dispatch.c`: N módulos em vez de `main` + `sdk`.
- `lifter.py`: `LDXP/STXP/LDAXP/STLXP` e LSE (`CAS`, `SWP`) usados pelo `sdk` 16.2.
- `ipc.c`: provável **TIPC** (o `sm:` usa TIPC desde o SDK 12) — confirmar no M2.
- `kernel.c`: SVCs que o SDK 16.2 chamar e o LAS não tem; `GetInfo` (title ID já trocado).
- `services/`: comandos novos do SDK 16 (hid, am/applet, audren revisão nova, fs, set, time) — guiado por log.
- NVN: 11 funções faltando; conferir offsets das structs/builders gravadas na memória do guest para a versão de NVN
  do SDK 16.2.
- `hle/heaps.c`, `hle/input.c`, `hle/framepace.c`, `hle/bindings.c`: valores/endereços do motor do LA.
- UI: `launcher.mm` (configuração dos arquivos do jogo, sem randomizer), `menu.mm` (abas do LA removidas).

**Novo**
- `tools/nca_extract.py` (feito): o hactool não lê o RomFS deste NCA (compressão BucketTree/LZ4 sobre a camada de
  integridade). Depois: extração dentro do app a partir do XCI + chaves do usuário.
- `glslc` (`subsdk0`): o jogo importa o compilador GLSL da NVN. Se for chamado em runtime, HLE que compila o GLSL
  direto (shaderc) em vez de recompilar os 6,8 MB do glslc — decidir por medição.
- Recompilação e despacho de 4 módulos.
- HLE/patches específicos do EoW (heaps do motor, save, M7: resolução, aspect ratio, 60 fps, ícones de botões).
- Testes roteirizados próprios (input gravado do EoW, frames de referência).

## 4. Comandos das medições

```
tools/venv/bin/python -I tools/nso.py extracted/exefs/{main,sdk,subsdk0,subsdk1,rtld}
tools/venv/bin/python -I tools/analyze.py extracted/exefs/main --census
```
(Imports por módulo de origem, nomes NVN, SVCs e atômicos foram contados com scripts ad hoc sobre `nso.py`/Capstone;
viram ferramenta própria no inventário do `docs/GAP.md`.)
