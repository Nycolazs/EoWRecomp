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

## Marco atual: M4 alcançado (Linux / Vulkan); M5 em andamento

Comprovado por screenshot (`EOWRECOMP_SHOT_AT`, `build/frames/`): o jogo inicializa, carrega, toca a abertura
(masmorra do prólogo: Link, moblins, tochas, piso texturizado) e renderiza corretamente no Vulkan (Intel Iris Xe,
Mesa ANV). Backend testado: **Vulkan no Linux**. Metal não foi tocado nem testado.

### Funciona
- M1: geração dos 4 módulos sem `Unsupported`; lifter com LDXP/STXP, CAS/CASP, LSE (testado contra Unicorn).
- M2/M3: boot pelo `nn::init::Start` do SDK 16.2, serviços (sm via TIPC, fs, acc, hid, am, time, audren REV11),
  primeiro `nvnQueuePresentTexture`, threads, áudio (renderer abre; mixes/efeitos aceitos pelo SDK).
- M4: imagem correta da cena de abertura.

### Problemas abertos (por prioridade)
1. **Desempenho de GPU**: ~7–12 fps na abertura numa Iris Xe; um único material (fs `72ee66f300`, com laço)
   custa ~87 ms de 126 ms por frame. Causa: a máquina de estados `for { switch (pc) }` dos shaders com laço
   (registradores vivos na aresta de volta → spill). Próximo passo: estruturar laços (PBK/BRK/BRA para trás →
   `do { } while (true)`) no `maxwell/translate.c`. Shaders só com desvios para frente já saem sem o laço.
2. **Estabilidade**: houve travamentos (~frame 1600–1900) e um `vkWaitSemaphores: device lost` numa execução
   com um shader pulado. A investigar depois do item 1 (frames muito longos podem disparar o watchdog da GPU).
3. Dois compute shaders ainda não traduzem (RED com min/max assinado; `SUST` `eb20…`): 128 dispatches pulados.
4. Input e áudio ainda não verificados a ouvido/jogando; save ainda não testado (diretório monta).

### Como testar (Linux)
- `tools/run.sh` (SECS=60; janela oculta; `SHOW=1` mostra) — log em `build/run.log`, pilha do jogo resolvida.
- Screenshots: `EOWRECOMP_SHOT_AT=300,600,900` (imagem do jogo) → `build/frames/`.
- Desempenho: `EOWRECOMP_VK_PROF=1` (tempo de CPU/GPU na linha de fps), `EOWRECOMP_GPUPROF_AT=<frame>` (GPU por passe).

### Windows
- `tools/windows/build.sh all` gera `build/win/dist/EoWRecomp/` (exe + `eowgame.dll` + DLLs). Ainda não executado
  no Windows: é o próximo teste do usuário (ver instruções no relatório da sessão).

### Como compilar (Linux, sem sudo)
1. `tools/venv` (Python: capstone, unicorn, lz4, cryptography, pillow).
2. `tools/linux/dev_deps.sh`: zig 0.17.0 (clang 22 + lld, hash conferido) e os pacotes -dev (SDL3, Vulkan, shaderc,
   FFmpeg, VMA) via `apt-get download`, sem instalar nada no sistema.
3. `make gen` (84 s) e `PATH=$PWD/build/bin:$PATH make -j12 DEPS=$PWD/build/deps-linux GENARCH=-mcpu=x86_64_v3`
   (código do jogo: ~1 h na primeira vez; depois só o que muda).

### Pendências conhecidas
- `docs/linux.md` / `docs/BUILD-windows.md` ainda são do LAS.
- Widescreen, ícones de botão, menu sem abas específicas do EoW: M7.
