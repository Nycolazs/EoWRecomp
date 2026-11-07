# Status

Atualizado: 2026-11-07 (sessão 4: o jogo roda num celular Android).

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

## Sessão 4 (2026-11-07, Linux + AYN Thor): o jogo roda e se joga no Android

**Comprovado num aparelho real**: AYN Thor (Snapdragon 8 Gen 2 / QCS8550, **Adreno 740**, Android 13), APK pessoal
(`tools/android/build_game.sh extracted --apk`), setup pela tela inicial com um NSP v1.0.0. Vulkan pelo **Turnip
26.0.0 R8** (no APK, via libadrenotools; `EOWRECOMP_DRIVER=system` para o driver Qualcomm 1.3.128). Filme do título,
seleção de arquivo com texto, novo jogo, prólogo jogável a **30 fps em 720p** (GPU ~13 ms por frame).

### Corrigido (cada um visto no Thor)
- Crash ao abrir: features Vulkan pedidas sem consultar o driver (`uniformAndStorageBuffer8BitAccess`).
- `tools/nca_extract.py`: NSPs com title key (ticket / `title.keys`).
- Tela preta no título: filmes lidos do RomFS (pacote / `romfs.bin`) por um `AVIOContext`, não só de `extracted/romfs/`.
- 320 fps no título: o present respeita o intervalo do NVN (60 fps).
- **Sem texto** na seleção de arquivo: o heap do guest são `AHardwareBuffer`s (blocos de 512 MB) mapeados em
  `HEAP_BASE` e importados no Vulkan; os shaders leem a memória global por buffer address (`gmw()`), pois o Adreno não
  tem `VK_EXT_external_memory_host` e o storage buffer vai só até 128 MB. Com o driver Qualcomm o texto continua faltando
  (memória importada tipo 1, provavelmente sem coerência com o cache da CPU); com Turnip aparece.
- Turnip 24.3 desenhava tudo preto; 26.0.0 R8 desenha.
- **1 fps no prólogo** (Turnip): o vertex shader de skinning (12ee694000) tinha um BRX (tabela de salto pelo número de
  ossos) e caía no laço de dispatch: 48 registros e ~575 spills. A tabela agora é lida das constantes do compilador e o
  shader fica estruturado: 24 registros, sem spill, 3,3 ms -> ~0 por draw.
- `LDC.64` na própria base lia a segunda palavra errada (matrizes das ossos): a **faixa vertical escura até o Link**
  (problema aberto 2) sumiu junto.
- Opção de resolução "720p" (padrão no Android). Ferramentas: `EOWRECOMP_GPUPROF_DRAWS`, `EOWRECOMP_VK_STATS`,
  `EOWRECOMP_GLSL_OVERRIDE`.
- **Corações** (HUD / seleção) eram quadrados vermelhos, também no Linux: o RGB565 do NVN é `B5G6R5` no Vulkan.
- **Miniatura do save**: saves novos têm a imagem (a cópia textura -> buffer chega à memória do guest); os antigos
  foram gravados com a cena preta e ficam pretos até serem sobrescritos.
- Cache de pipelines gravado a cada 30 s enquanto surgem pipelines novos (o Android mata apps sem `exit()`).

### Ainda errado no Thor
- O chão perto do Link "respira" de leve: pixels esparsos numa faixa diagonal mudam com a animação dele (parece ruído
  de shadow map / luz; não é a precisão do depth: igual com D32). Prioridade baixa (usuário).
- A lista "Which data would you like to load?" mostra as entradas vazias por alguns segundos.
- Compute `eb20…` (SUST) sem tradução (~7 dispatches por frame pulados); primeira visita a uma cena trava alguns
  segundos enquanto o Turnip compila os pipelines.

### Próximo passo (Android)
1. 30 fps estáveis é a meta do MVP (usuário): o prólogo fica em 30,0 (CPU do thread do jogo ~28 ms por frame no modo
   30; GPU ~13 ms). Medir outras áreas (overworld) quando o jogo avançar.
2. SUST (imagens de armazenamento) e o ruído do chão.
3. Áudio e save em jogo mais longo.

### Como testar no Android
`tools/android/build_deps.sh`, `(cd android && ./gradlew assembleDebug)`, `tools/android/build_game.sh extracted --apk`,
`adb install -r android/app/build/outputs/apk/debug/app-debug.apk` (a release alpha_1 tem outra chave: desinstalar
antes). Log: `Android/data/com.eowrecomp.android/files/eowrecomp.log` ou `adb logcat -s EoWRecomp`. Gradle 8.14 não
roda no Java 25: usar um JDK 21.

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
- **Build do código do jogo no celular** (`tools/android/ondevice/`, `-PonDeviceBuild=true`): CPython 3.14 Android +
  capstone/unicorn/lz4, zig 0.17 (clang 22 + lld), kit do NDK; `gamebuild.c` com ramo Android (geradores um por vez,
  compiladores com nice e controle de memória, link com ld.lld contra `libmain.so`, instalação no armazenamento
  interno), `BuildService` (serviço em primeiro plano). **No emulador**: zig roda da pasta de bibliotecas nativas, o
  Python importa capstone/unicorn/lz4 e roda `gen.py`, `gamelib_entry.c` compila e linka contra `libmain.so`.
  `check_public_apk.sh`: APK de 79 MB sem código do jogo.
- **Lifter no ARM64** (`test_lift.py --synth`, sem NSO): 15 000 codificações sintetizadas contra o Unicorn; achou e
  corrigiu um crash (LDR literal SIMD opc 3) e C com UB em formas UNDEFINED de 32 bits. O Unicorn 2.1.4 erra
  LDSMAX/LDSMIN de 8/16/32 bits (compara sem sinal): `tools/test_lse.c` confere as 9 operações LSE x 4 tamanhos contra
  o próprio M1 — todas certas.
- Botão Voltar do Android abre o menu.
- **Release alpha_1** (2026-10-21, pré-release no GitHub, pedida pelo usuário mesmo sem o jogo rodado no Mac/Android):
  `.app` para **macOS 15+** — o loader do Vulkan, o shaderc e o Python do gerador (python-build-standalone) agora vêm
  de `build_deps_macos.sh` (os do Homebrew exigiam macOS 27); `bundle_macos.sh` confere todos os Mach-O. APK assinado
  com chave de release própria (fora do repo: `android/keystore.properties` → `~/.eowrecomp-signing/`, **guardar**:
  sem ela, as próximas versões não instalam por cima) e com o build no celular; `check_public_apk.sh` ok; tela inicial
  vista no M1 (Vulkan/MoltenVK) e no emulador Android 15. A tela inicial agora consulta as releases do repositório
  (`alpha_<n>` < `beta_<n>` < `v1.x`).

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
