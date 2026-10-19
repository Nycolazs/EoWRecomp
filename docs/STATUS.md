# Status

Atualizado: 2026-10-19 (sessão 1).

## Jogo fixado (único build suportado)

| | |
|---|---|
| Título | The Legend of Zelda: Echoes of Wisdom |
| Title ID | **01008CF01BAAC000** |
| Versão | **v0 (0x0)**, display version **1.0.0**, sem update (partição update do XCI vazia) |
| Requisito de sistema (CNMT) | 16.0.0 (0x40000000) |
| SDK | NintendoSDK 16.2.0 (program NCA: SDK 16.2.0.0, key generation 0x11) |
| Program NCA | `0449fda9301caaad241256ec54b3c89f.nca` (5 804 949 504 bytes) |
| Nome interno do `main` | `Edward.nss` |

Build IDs dos módulos (endereços de patches/HLE valem só para estes):

| Módulo | Build ID | Arquivo |
|---|---|---|
| main | `a581406bc985865113d18cb19f11d5448ceea783` | 26 093 400 B |
| sdk (nnSdk) | `3053b22935e32ddf15ef81997603e5c267534a90` | 6 215 109 B |
| subsdk0 (glslc) | `e0d8adcc446cf14d6af1a51193f72e30ed3db9ac` | 5 392 097 B |
| subsdk1 (multimedia) | `ff0149d0b5e882b87c2f06a981302b130c1edb32` | 3 436 212 B |
| rtld | `2f1967113a281280a48ce3fc6dc23049327bf924` | 7 358 B |

RomFS: 10 963 arquivos, 8 032 371 686 bytes (todos com magic válido: FRES, BNTX, MsgS, FSTM, SARC, FSHA, webm…).

## Marco atual: M1 concluído, M2 em andamento

### M1 — comprovado
- `make gen` gera C para os 4 módulos (`main`, `subsdk0` glslc, `subsdk1` multimedia, `sdk`) **sem nenhum
  `Unsupported`**: 296 mil funções, 84 s. Medido com `tools/inventory.py --lift` e o log do gerador.
- Lifter: pares exclusivos (LDXP/STXP), CAS/CASP e atômicos LSE implementados; `tools/test_lift.py` contra o
  Unicorn: 127 codificações novas + 3000 aleatórias por módulo, 0 falhas.
- Jump tables: corrigida a divisão de funções nas bases de tabela (main: 963 resolvidas, eram 71).

### M2 — estado
- O runtime compila e linka no Linux (`build/eowrecomp`) e no Windows por cross-compilação
  (`tools/windows/build.sh` → `build/win/dist/EoWRecomp/eowrecomp.exe`). Nenhum dos dois foi executado ainda.
- Boot adaptado ao SDK 16.2: `nn::init::Start` com 3 callbacks; `sm:` via TIPC; `GetInfo` com os tipos novos.
- Em andamento: compilação da biblioteca do jogo (`build/eowgame.so`, ~900 MB de C, ~1 h em 12 núcleos).
- Próximo: rodar `tools/run.sh` (janela oculta) e seguir os logs de SVC/serviços até o primeiro
  `nvnQueuePresentTexture` (marcado no log como `[nvn] first nvnQueuePresentTexture`).

### Como compilar (Linux, sem sudo)
1. `tools/venv` (Python: capstone, unicorn, lz4, cryptography).
2. `tools/linux/dev_deps.sh`: zig 0.17.0 (clang 22 + lld, hash conferido) e os pacotes -dev (SDL3, Vulkan, shaderc,
   FFmpeg, VMA) via `apt-get download`, sem instalar nada no sistema.
3. `PATH=$PWD/build/bin:$PATH make -j12 DEPS=$PWD/build/deps-linux GENARCH=-mcpu=x86_64_v3`
   (`build/bin/clang` e `clang++` são wrappers de `zig cc` / `zig c++`).
4. `tools/run.sh` (SECS=30, SHOW=1 para ver a janela).

### Pendências conhecidas
- `docs/linux.md` / `docs/BUILD-windows.md` ainda são do LAS.
- Jump tables: funções com tabela ainda levam o fallback "todas as instruções" (correto, porém pesado);
  otimizar depois de haver profiling.
- Widescreen, ícones de botão, menu sem abas específicas do EoW: M7.
