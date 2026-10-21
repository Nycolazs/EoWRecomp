# Status

Atualizado: 2026-10-20 (sessão 3: macOS / Android).

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

## Sessão 3 (2026-10-20, MacBook Air M1 8 GB, macOS 27): macOS e Android

**Bloqueio**: o NSP entregue nesta sessão (`~/Downloads/data`, ver memória do agente) está **truncado**: 5 637 144 576
bytes de 5 806 819 063 (faltam 162 MiB, cópia interrompida em exatamente 5376 MiB). O ExeFS (código) e a tabela de
compressão do RomFS ficam justamente no fim do NCA: nada do jogo pôde ser extraído, gerado ou rodado no Mac. Um vigia
em segundo plano retoma sozinho quando um NSP íntegro aparecer na pasta. **O usuário precisa copiar o NSP de novo.**

### Feito e verificado (sem os arquivos do jogo)
- **macOS = Vulkan via MoltenVK** (padrão; `MACOS_BACKEND=metal` mantém o renderer do LAS, sem as correções do EoW).
  Portabilidade (`VK_KHR_portability_enumeration`/`_subset`). Tela inicial renderizada no M1 (MoltenVK 1.4.2).
  `tools/macos/dev_deps.sh`, `docs/BUILD-macos.md`.
- **Leitura do pacote no runtime** (`runtime/nxpkg.c`): NSP/XCI/NCA com as chaves do usuário, AES (ARMv8 AES no CTR),
  XTS, LZ4 e BucketTree em C. O setup extrai só o ExeFS (~40 MB) e lê o RomFS direto do pacote (`romfs.src`): sem
  `romfs.bin` de 8 GB. `tools/test_nxpkg.sh`: vetores FIPS-197 / SP 800-38A / IEEE 1619 e blocos LZ4; com o NCA real:
  cabeçalho e cabeçalho do RomFS corretos. `nca_extract.py` lê NSP e recusa pacote truncado.
- **Android** (`android/`, `docs/BUILD-android.md`): app Gradle/CMake (NDK 27.2, volk, SDL3, FFmpeg LGPL, shaderc),
  camada de plataforma e JNI (do LASRecomp-Android-Plus), deslocamento do layout do guest (`GUEST_SHIFT`,
  `gen.py --shift`), setup pelo seletor de pastas do sistema lendo o pacote por *file descriptor* (sem copiar o
  jogo), controles de toque (`runtime/ui/touch.mm`), ciclo de vida (superfície recriada ao voltar do segundo plano),
  swapchain pré-rotacionado, afinidade de núcleos. `tools/android/build_game.sh` gera o APK pessoal com o código do
  jogo. **Verificado no emulador Android 15 arm64** (AVD próprio `eow_test`): tela inicial em paisagem, pré-rotação
  90°, seletor → JNI → leitura do pacote (pacote de teste truncado: mensagem certa), fundo/frente.
- **Código do jogo**: `Cpu *__restrict` nas funções geradas e `FN_ENTER` compilado fora (`FNPROF=1` o traz de volta):
  ~10% menos instruções no LAS-Android. **Ainda não medido no EoW.**
- **Tradutor Maxwell**: RED S32 min/max e INC/DEC (laço CAS no GLSL); auxiliares do GLSL renomeados (`mw_*`) — no
  MoltenVK `popcount`/`clz`/`fmin` colidiam com built-ins do Metal (1015 de 3200 shaders do fuzz falhavam; agora 63
  de 5882, todos artefatos do método: textura 1D, atributo 31). `tools/test_moltenvk.sh`.
- **Release macOS**: `make app` → `build/EoWRecomp.app` (FFmpeg LGPL de `tools/build_deps_macos.sh`, MoltenVK + loader
  embutidos e forçados, gerador + zig para compilar o código do jogo na máquina do usuário). Tela inicial ok.
- gfast (cópias de buffers antes de compute) só em GPU dedicada por padrão (memória unificada: só custo). Não medido.

### Próximo passo (assim que houver um NSP íntegro)
1. `tools/venv/bin/python -I tools/nca_extract.py <prod.keys> <nsp> extracted --no-romfs --romfs-image` (ou setup pela
   tela inicial), `make gen`, `make -j8`, rodar no M1: abertura, primeira cena, áudio, save. Medir fps.
2. Comparar -O1 x -O2 no código do jogo no M1; medir o efeito do `restrict`/`FN_ENTER`.
3. SUST (imagens de armazenamento: tradutor + descritores) com o shader real `eb20…`.
4. Android: `build_game.sh --apk` e rodar no emulador (arm64 nativo no M1) e num celular. Em drivers sem
   `VK_EXT_external_memory_host` (comum no Android) a memória global dos compute shaders lê zero: precisa de um caminho
   por cópia.

## Marco atual (sessão 2): M4 concluído, M5 parcial (Linux / Vulkan)

Comprovado por screenshot (`EOWRECOMP_SHOT_AT`, capturas em `docs/screenshots/`): o jogo inicializa, toca a abertura
(masmorra do prólogo, Link e os moblins), entra na **primeira cena controlável** (HUD com corações e itens) e
**responde a input** (input roteirizado `EOWRECOMP_INPUT`: Link anda pelo corredor, entra na sala; prompts
"Swing sword", "Place / Toss" aparecem). Execução de 330 s / 2.819 frames sem travar. Backend testado: **Vulkan no
Linux** (Intel Iris Xe, Mesa ANV). Metal não foi tocado nem testado.

### Funciona
- M1: geração dos 4 módulos sem `Unsupported` (296 mil funções); lifter com LDXP/STXP, CAS/CASP, LSE (testado contra
  Unicorn); jump tables resolvidas também em funções divididas.
- M2/M3: boot pelo `nn::init::Start` do SDK 16.2 (3 callbacks), sm via TIPC, fs (RomFS original), acc, hid, am, time,
  audren REV11 (abre e é aceito pelo SDK), threads, mutex/condvar com a semântica do Horizon.
- M4: imagem correta da abertura. M5 (parcial): primeira cena jogável com input.

### Corrigido nesta sessão (cada um com a causa achada no código do jogo / SDK)
- Z-cull com tamanho 0; NVNsamplerBuilder maior que os 0x40 bytes que o jogo reserva (sobrescrevia a pilha);
  memória de comandos NVN (sombra 8x); revisão REV11 do áudio (status de efeito 0x90); `VFormatString` (jump table
  em função dividida); cabeçalho de 0x100 bytes dos compute shaders do NVN novo; tradutor Maxwell: BFE.BREV,
  HSETP2/HSET2 imediatos, VOTE, RED, LEA.HI, FLO.SH/.INV (o FLO errado fazia o laço de luzes rodar demais);
  laços estruturados nos shaders; timeout do `WaitProcessWideKeyAtomic` não readquire o mutex (travava a thread
  principal após ~1.200 frames).

### Problemas abertos (por prioridade)
1. **Desempenho de GPU**: ~9–12 fps na abertura numa Iris Xe. O material mais caro (fs `72ee66f300`, laço de
   luzes) ainda domina o passe principal; medir de novo sem outra carga na máquina (as últimas medidas tiveram a
   compilação do Windows em paralelo). Ferramentas: `EOWRECOMP_VK_PROF=1`, `EOWRECOMP_GPUPROF_AT=<frame>`,
   `EOWRECOMP_SKIP_FSHASH=<hash>`, `EOWRECOMP_SHADER_GUARD=<n>`, `EOWRECOMP_SAVE_MSL=1` (GLSL em `build/glsl/`).
2. **Artefato**: faixa escura vertical do topo da tela até o Link na abertura. Próximo passo: `EOWRECOMP_DEBUG_FRAME`
   (imagens por passe) para achar o draw.
3. Dois compute shaders sem tradução (RED com min/max assinado; `SUST` `eb20…`): 128 dispatches pulados.
4. Áudio não verificado a ouvido; save não testado (o diretório monta).
5. **Windows**: o `.exe` cruzado linka (7 exports para a DLL do jogo); a compilação da `eowgame.dll` foi
   interrompida a pedido do usuário em 44%. Rodar `tools/windows/build.sh all` de novo (só recompila o que falta) e
   testar no Windows.

### Como testar (Linux)
- `tools/run.sh` (SECS=60; janela oculta; `SHOW=1` mostra) — log em `build/run.log`, pilha do jogo resolvida
  (`tools/symgame.py`).
- Screenshots: `EOWRECOMP_SHOT_AT=300,600,900` → `build/frames/`. Input roteirizado:
  `EOWRECOMP_INPUT="1550:STICK_UP:150,1820:A:10"` (frame:botão:duração).
- Travamentos: rodar dentro do `gdb` e mandar `SIGINT` (o `ptrace_scope` impede anexar depois).

### Como compilar (Linux, sem sudo)
1. `tools/venv` (Python: capstone, unicorn, lz4, cryptography, pillow).
2. `tools/linux/dev_deps.sh`: zig 0.17.0 (clang 22 + lld, hash conferido) e os pacotes -dev (SDL3, Vulkan, shaderc,
   FFmpeg, VMA) via `apt-get download`, sem instalar nada no sistema.
3. `make gen` (84 s) e `PATH=$PWD/build/bin:$PATH make -j12 DEPS=$PWD/build/deps-linux GENARCH=-mcpu=x86_64_v3`
   (código do jogo: ~1 h na primeira vez; depois só o que muda).

### Pendências conhecidas
- `docs/linux.md` / `docs/BUILD-windows.md` ainda são do LAS.
- Widescreen, ícones de botão, menu sem abas específicas do EoW: M7.
