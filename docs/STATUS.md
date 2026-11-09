# Status

Atualizado: 2026-11-08 (sessão 5: gráficos no Android - fendas, Hyrule Castle Town).

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
- **Brilho no chão** perto das luzes (Link, bombas): pixels 2x2 com as luzes erradas. O laço de luzes (fs 12ee66f300)
  junta as listas de luzes no warp (ballot + shuffles) e os subgroups do Adreno têm 64/128 lanes: agora cada 32 lanes
  do subgroup são um warp (lane, shuffle, ballot, vote).
- Compute 12a4723000 (filtra cube maps em quatro mapas 2D) não traduzia (`SUST.P.B`): agora vira `imageStore`
  (imagens ligadas com `nvnCommandBufferBindImage(s)`, `c[2][0x120 + 8 * slot]`; texturas com
  `NVN_TEXTURE_FLAGS_IMAGE` ganham uso storage). 0 dispatches pulados; no dungeon do prólogo a imagem não muda.
- **Thread de UI girando um core grande**: o `SDL_WaitEventTimeout` do Android nunca dorme (cada pump enfileira um
  sentinel que acorda o loop). Agora `SDL_PollEvent` + 4 ms: CPU do app 3,1 -> 1,2 G ciclos/s (simpleperf), mesmo fps.
- **Crash quando a Zelda é libertada** (fim do prólogo): `FATAL: no function at 1001a65870`. `f_7001a657e0` (switch
  0-8, tabela `ldrh`) tinha 4 `mov` entre o `add` e o `br`: o `adr` da base ficou fora da janela de 6 instruções de
  `adr_feeds_br`, foi tomado por callback e a função partida no primeiro caso (casos 1-8 sem função). Janela 16
  (para em b/bl/blr/ret): +46 tabelas nos quatro módulos. Visto no C gerado (os 9 casos no switch) e no aparelho: a
  parte da Zelda carrega e se joga.
- **Threads no core do render**: os threads criados a partir do thread de render herdavam a afinidade dele (cpu7
  sozinho): workers de pipeline, tradutor de shaders, áudio do SDL e mixer. Agora vão para os outros cores
  (`plat_children_elsewhere`); no cpu7 sobra só o render. Sem mudança de fps aqui (já 30 fixos); achado do
  PokeZ-Android no mesmo aparelho (lá +2 fps).
- `nvnCommandBufferClearBuffer` era gravado e descartado: agora executado em ordem. O EoW não o usa no prólogo.
- **Fuga do Rift lenta (24-27 fps)** com a GPU e o thread principal ambos ocupados menos da metade do tempo. Causa:
  no Turnip (kgsl) os timeline semaphores são emulados - `vkGetSemaphoreCounterValue` (e `vkWaitSemaphores` com
  timeout 0) bloqueava até a submissão recém-feita acabar (~11 ms), e todo `vkQueueSubmit` que sinalizava o timeline
  esperava na CPU pelos pontos anteriores (`vk_sync_timeline_gc_locked -> kgsl_syncobj_wait`, simpleperf
  `--trace-offcpu`): jogo e GPU andavam em turnos. Agora cada submissão termina gravando seu número num buffer
  coerente (`vkCmdUpdateBuffer`), lido/esperado por polling; no Android o timeline não é mais sinalizado
  (`EOWRECOMP_VK_TIMELINE=1` volta). Mesma fuga, mesmo percurso: 30 fps fixos; cutscenes chegam a 52-60.
  A linha de fps do log agora sempre traz `gpu X ms, main thread cpu Y ms` (por 2 s).
- `rt_dispatch`: o cache de chamadas indiretas saiu do TLS (TLSDESC no Android) para o `Cpu` do thread.
- **Sessão 5 (2026-11-08, Thor, Turnip; build 524c797..c15346d)**, a partir do save do usuário em Hyrule Castle
  Town / castelo:
  - **Telhados e paredes listrados, soleiras e caminhos pretos** nas casas de Hyrule Castle Town (mudavam com a
    câmera: as "texturas tremendo" e "sobreposições" do usuário). Causa: o modo depth-stencil STENCIL do NVN (view ou
    textura) era ignorado - a view era sempre a de profundidade. O draw que recorta a fenda (fs 12a46f5000) lê dois
    bits do stencil (R2P) e puxa a profundidade da cena para a frente onde eles estão zerados, e o pass com EQUAL
    depois deixa um buraco; lendo a profundidade, os bits do float decidiam. Agora a view é o aspecto stencil e o slot
    um sampler inteiro (ShaderKey.stencil_tex). Achado pulando draws num debug frame (`EOWRECOMP_DEBUG_FRAME` +
    `EOWRECOMP_SKIP_FSHASH`): sem esse draw a casa saía limpa. Visto no Thor (Vulkan): casa, soleira e pedras certas.
    O backend Metal (`renderer.m`) ainda lê a profundidade nesse caso.
  - Compute 12a4711000 / 12a4715000 não traduziam (`VMNMX`, depois `ATOM.E.ADD.S32`): todos os dispatches e
    ~120-850 draws a cada 2 s pulados ("compute program") na sala do trono, na cutscene da fenda e na cidade. Agora
    0 pulados. `S2R` LANEID e as máscaras de lane (LTMASK etc.) também (o append por warp do mesmo shader).
  - `fmin`/`fmax`/`saturate` como NMin/NMax/NClamp (do PokeZ-Android): laço de luzes 4,00 -> 3,78 ms, frame
    21,5 -> 21,3 ms na cidade.
  - Medido na cidade (GPU por frame, `EOWRECOMP_GPUPROF_AT`): ~21 ms a 30 fps; os mais caros: fs 12a5c12e00
    (estático) 4,2 ms, laço de luzes 12ee66f300 3,8 ms, pre-pass com skinning 12ee695200 3,6 ms.

### Ainda errado / medido no Thor
- A lista "Which data would you like to load?" vazia por alguns segundos e objetos que aparecem um pouco depois na
  primeira visita: são os draws pulados enquanto os pipelines compilam (assíncrono de propósito: sem travadas).
  Com o cache quente acontece quase só durante o fade do carregamento.
- Thread do jogo (simpleperf, prólogo): 42% código do jogo, 22% kernel (sincronização: ~2500 trocas de contexto/s
  no thread principal, 0 page faults), 21% runtime (`rt_dispatch` 9%: saltos indiretos), 4% Turnip. Onde os threads
  bloqueiam (simpleperf `sched:sched_switch`): `svc_wait_process_wide_key` (condvar) e `svc_arbitrate_lock` (mutex
  disputado), chamados de f_700c4f82b0 / f_700c4fa1f0 do SDK pelos workers do jogo (~17 000 trocas/s no processo).
- Testado nesta sessão sem defeitos novos: cutscene de abertura do novo jogo (profundidade de campo), combate,
  bombas e explosões, tochas, escadas, menus de pausa (Key Items / System), lista de saves.
- **Fenda sem o vazio preto - corrigido (97d8b3e)**: em Hyrule Castle Town há uma fenda sobre uma casa; ao vivo
  aparecia só o contorno roxo, a casa inteira dentro (o usuário confirmou no jogo: nas cutscenes as fendas são buracos
  pretos, jogando só contornos - também no dungeon depois do atalho). Os constant buffers são copiados na CPU quando o
  draw é gravado (`guest_copy`), e a compute 12a4705000 (1x1x1, sbuf 127e80a300+0x470) escreve parâmetros que draws
  da mesma submissão leem como constant buffer: a cópia saía velha. Achado com flush depois de cada pass (o vazio
  aparece) e bissecção até o pass logo depois das computes 12a46fe000 / 12a4705000 / 12a470d000, mais
  `EOWRECOMP_COPY_TRACE`. Agora esses constant buffers são lidos no lugar (o buffer do heap AHardwareBuffer como
  uniform buffer): a fenda vira vazio preto de bordas roxas, a Zelda na frente, 30 fps.
- **Grade escura no piso - corrigido (43967b4)**: com o constant buffer certo a iluminação em clusters do jogo
  passou a valer (a luz da Tri, os raios de sol) e o piso ficou com uma grade de linhas escuras serrilhadas (Suthorn
  com a Tri, entrada do castelo; o usuário confirmou que é erro). Não eram as sombras (`EOWRECOMP_NO_SHADOW=1` não
  mudava). Isolado com `EOWRECOMP_GLSL_OVERRIDE`: só o campo `cb4[70][2]` (escala vertical da grade de clusters,
  0 na cópia da CPU, ~0,116 da compute) e só no par vs `12a46e1000` / fs `12a46e1400`. Esse passe rasteriza os
  volumes das luzes numa grade 128x72 (e 64x36) e o fs faz `atomicOr` do bit da luz em cada célula coberta; sendo
  16:9 o alvo era escalado (2/3) como uma imagem: 85x48 fragmentos para 128x72 células, as do meio sem luz. Agora um
  alvo em que um fs com stores desenha volta ao tamanho do guest (`vk_texture_unscale`, log "kept at the guest
  size"). Metal tem a mesma escala e não foi tocado.
- **Travadas nas animações (17b48b5)**: no log da run do usuário (18 min) o pacing trocou 19 vezes entre os modos
  60 e 30 fps do jogo: em 30 parecia haver folga, voltava a 60, ficava 5-100 s a 52-57 fps com a GPU quase cheia e
  caía de novo. Uma queda menos de 60 s depois de voltar agora dobra a folga exigida (até 3840 frames); 60 s em 60
  zeram. Lógica testada fora do aparelho (a cena testada ficou estável em 30).
- Pre-pass e EQUAL: as listras não eram falta de precisão entre os dois vertex shaders (as contas de posição são as
  mesmas a menos de comutação; um bias de 256 unidades D24 escondia as listras, 128 não - era o recorte da fenda).

### Próximo passo (Android)
1. Feedback do usuário (9/11) ainda aberto: texturas tremendo às vezes (talvez as células sem luz da grade, que
   mudam com a câmera - confirmar), o menu de ecos (L segurado; no aparelho os ícones e o fundo parecem normais -
   saber o que ele estranha), os jardins do castelo à noite pretos até os shaders compilarem (primeira entrada
   depois de o GLSL mudar; na segunda deve vir do cache).
2. GPU (cidade ~21 ms por frame): fs `12a5c12e00` (2727 instruções, ~4,2 ms) e o laço de luzes `12ee66f300`
   (3,8 ms; ballot + shuffles em warps de 32 dentro dos subgroups de 64/128 do Adreno).
3. CPU do thread do jogo: a sincronização entre threads do guest (tempo no kernel).
4. Áudio e save em jogo mais longo.

**Para o usuário testar (build com 17b48b5):** pisos sem a grade escura (dungeon, entrada do castelo), a Tri com a
luz dela, as fendas como buracos pretos; e se as animações ainda dão trancos (o log mostra as trocas: `[pace]`).

Entrada pelo controle no aparelho: `sendevent /dev/input/event9` (o "Odin Controller"; o shell está no grupo
`input`): `BTN_TL` 0x136 = L, segurar para o menu de ecos. Os `input keyevent` do adb não chegam como controle.

Percurso até a fuga a partir do save do usuário (sala do trono): stick esquerda ×2, esquerda-baixo, esquerda-cima,
baixo, cima (começa a cutscene), A ×10, baixo-esquerda até a escada, A ×4, segurar baixo = fuga.

### Testes no aparelho (scripts)
`adb shell input swipe` usa um ponteiro só (pular andando não funciona). Multi-toque: `sendevent` em
`/dev/input/event6` (`fts_ts`, tela principal; protocolo B: slot 0 o stick, slot 1 o botão; com a tela em paisagem
raw_x = 1080 - y, raw_y = x).

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
