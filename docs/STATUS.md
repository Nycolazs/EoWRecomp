# Status

Atualizado: 2026-11-07 (sessão 4: Windows pelo GitHub Actions).

## Sessão 4 (2026-11-07): pacote do Windows pelo GitHub Actions

### Feito
- `.github/workflows/build-windows.yml` (push em `runtime/`, `tools/`, etc., ou "Run workflow"): três jobs.
  `exe` (cross-build com zig, `tools/windows/build.sh RELEASE=1`, sem console, log em `eowrecomp.log`), `helper`
  (`eowgen.exe` por PyInstaller no windows-latest) e `package` (zig para Windows de `tools/fetch_zig.sh`,
  `gamelib-src/`, `eowrecomp.dll.a`). Artefato `EoWRecomp-windows-x64`: ~164 MB (528 MB descompactado), sem arquivos
  do jogo. O zig é o completo (sem `tools/zig_slim.sh`).
- `runtime/nxpkg.c` inclui `platform.h` (pread/mkdir no Windows). `Makefile`: `all` só inclui o `eowgame.dll`
  quando há código gerado (`$(strip $(GEN_SRC))`). Manifest `activeCodePage` UTF-8
  (`runtime/platform/eowrecomp.manifest`): caminhos com acento (`C:\Users\José\...`) falhavam no `stat()` e a tela
  inicial dizia "No game files found". A busca do pacote agora registra no log o que achou e descartou.

### Primeiro teste no Windows (usuário: Windows 10, Xeon E5-2650 v4, RX 580, Vulkan 1.3.260)
- Funciona: XCI + `prod.keys` achados; código do jogo gerado e compilado na máquina (1917 s, uma vez; 4 módulos,
  296 677 funções, `extracted/eowgame.dll`); os 4 módulos carregam; boot do SDK; RomFS lido direto do XCI
  (8 033 071 180 bytes, 10 963 arquivos); swapchain Vulkan 1280x720; áudio (audren) abre; XInput detectado.
- **Problema**: tela preta com os botões Y / X / A e o contador de fps por cima (screenshot do usuário). Execução de
  ~95 s até fechar a janela. No log, milhares de desenhos pulados por "program compiling" / "pipeline compiling" a
  cada 2 s até o fim (ex.: 4 797 desenhos x 11 700 pulados). Ainda não se sabe se é só a compilação assíncrona
  atrasada (primeira execução, 95 s) ou um defeito neste driver; no Linux (Iris Xe) a abertura aparecia.
- Mesmo `SUST eb20138600f70018` sem tradução do problema aberto 3 (um compute shader).
- Próximo teste (usuário): deixar rodar vários minutos; e `set EOWRECOMP_SYNC_SHADERS=1` antes de abrir o `.exe`
  (compila cada shader na primeira vez, bloqueando: sem desenhos pulados) e olhar se a cena aparece.


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

### Sessão 4, continuação (2026-11-08): travamento de pipelines no Windows / Radeon RX 580
- Log com `[vk] jobs`: tradutor ocioso, 16 workers presos por minutos, sempre no mesmo job: `p fs code 72a5c12e00, glsl 50007 bytes`
  (criação de pipeline, `build_pipeline`). >186 s sem terminar, memória privada até 38 GB; draws pulados (pipeline compiling).
  Causa ainda não provada (driver AMD Windows vs. forma do GLSL).
- `EOWRECOMP_MAX_FS_GLSL=<bytes>` falha os pipelines de fragment shaders maiores que isso (experimento). `EOWRECOMP_SAVE_MSL=1`
  grava `build/glsl/<code>_fs.glsl` para inspeção.
- Pista do LASRecomp (branch `beta_6`, não a `main`): modo de shaders estruturados (`LARECOMP_VK_STRUCTURE`, `static_stacks_resolve`,
  `struct_blocks`, port do StructuredIr do Ryujinx, MIT): if/else/do-while em vez do laço `switch (pc)`; padrão no Mali/Adreno.
  Candidato a port se o `.glsl` travado for dominado pelo laço `switch (pc)`.
- Próximo passo: o usuário roda com `EOWRECOMP_MAX_FS_GLSL=30000` e `EOWRECOMP_SAVE_MSL=1`, manda `eowrecomp.log` e o `.glsl`.

- 2026-11-08 (later): causa provável do travamento: os shaders pesados (vs 72a5c0e100 990 cases, vs 72a5c2b000 392, fs 72eb9d4700 1254)
  têm todos um `BRX`, que fazia o tradutor rotular TODAS as instruções como destino (um `case` por instrução no laço `switch (pc)`).
  O LASRecomp já corrigiu isso (d92cc2a, issue #237, "AMD Windows nunca termina o build"): só os destinos da tabela de saltos (cb1) ganham
  `case`. Portado para `translate.c` (`brx_table_labels`; `EOWRECOMP_BRX_ALL_LABELS=1` volta ao antigo) + `tools/test_brx.sh`
  (4 labels com a tabela, 11 sem). MSL inalterado (só GLSL). Ainda NÃO verificado no PC do usuário (RX 580, Windows).
- Também: `EOWRECOMP_MAX_FS_GLSL` / `EOWRECOMP_MAX_VS_GLSL` (descartam pipelines grandes, só experimento), `EOWRECOMP_NO_PIPELINE_CACHE`,
  linha `[vk] jobs` com o job mais longo (tipo, fs/vs, tamanho). `EOWRECOMP_VK_OPT=1` não ajudou.
- Usuário deve testar: duplo clique, sem variáveis; olhar se a cena 3D aparece e se `pipeline compiling` zera, mandar `eowrecomp.log`.

- 2026-11-08 (verificado): com o port do BRX a cena 3D renderiza na RX 580 / Windows (Vulkan), 60 fps, workers ociosos, memória estável.
- Abertura/título com save existente: o vídeo agora é lido do RomFS (XCI) quando não existe no disco (`fs_rom_read`, `movie.c`);
  CI ok, ainda NÃO verificado pelo usuário com save.
- Crash após ~6 min: `FATAL: no function at 7001a65870 (lr=7001a65160)`, x10 = alvo. É um `br` indireto não resolvido pelo lifter
  (alvo ~0x710 depois do lr: provavelmente dentro da mesma função). Hipótese: tabela de endereços absolutos
  (`ldr xt,[xb,xi,lsl #3]; br xt`), que `computed_target` não reconhecia e ia direto para `rt_dispatch`. Correção em `tools/gen.py`
  (esse padrão ganha o fallback de todas as instruções) + `tools/test_gen_br.py`. NÃO verificada: o binário do jogo não está
  neste ambiente; precisa regenerar no PC do usuário. Se repetir, o alvo/lr novos dizem se a hipótese estava errada.
- Abertos: corações da HUD e item largado pretos; malha/textura do Link esticada; compute shader com SUST não traduzido (840 dispatches/2 s);
  efeito de áudio tipo 3; `[tex] type mismatch: want 3 have 1`.
- Usuário deve testar: novo pacote (regenera o código do jogo, build demorado), jogar >10 min com o save, mandar o log se cair.

- 2026-11-09: crash ao sair da sala persistiu com o pacote do b9850dc (a hipótese `ldr x,[b,i,lsl #3]` não bastou; o fallback ficou, é inofensivo).
  `dispatch.c` agora imprime x0-x30 e o código da função chamada pelo `bl` antes do lr (até o primeiro `br`) no FATAL; precisa do log.
- Suspeita nova para corações/item pretos e Link esticado: o port do BRX (labels só dos alvos da tabela). Esses materiais/skinning têm
  BRX; se a tabela de cb1 lida a partir de `blob+const_offset` estiver incompleta, o dispatcher cai no `default`. No Linux/Intel (antes
  do port, com todos os labels) a HUD saía certa mesmo com o compute `SUST` pulado, então `SUST` não parece a causa.
  `EOWRECOMP_BRX_LOG=1` imprime por BRX: hash do shader, nº de entradas e as instruções-alvo. A/B: `EOWRECOMP_BRX_ALL_LABELS=1`
  (volta ao antigo, mas na RX 580 trava os builds de pipeline: usar só numa cena curta com `EOWRECOMP_SHADER_THREADS` alto).
  `[tex] type mismatch: want 3 (CUBE) have 1 (2D) 1408x792 fmt 0x35`: um sampler de cubo recebe uma textura 2D de render target
  (slot não usado/obsoleto); vira textura preta, provavelmente inofensivo.

- 2026-11-09 (crash resolvido por leitura do log, aguarda teste): o diagnóstico novo mostrou `f_7001a657e0`:
  `adr x10,+0x20; ldrh w11,[x9,x8,lsl#1]; add x10,x10,x11,lsl#2; 4x mov; br x10` — tabela compacta LLVM com o `br` 7 palavras depois do ADR.
  `adr_feeds_br` olhava só 6 → o ADR virou início de função, a função foi dividida, o alvo (7001a65870) ficou em nenhum pedaço.
  Janela passou a 16 (`tools/gen.py`); `tools/test_gen_br.py` usa a sequência real (janela 6 falha, 16 passa). Exige regenerar o código do jogo.
  (A mudança `ldr xt,[xb,xi,lsl #3]` do commit b9850dc não era a causa; o fallback ficou, é inofensivo.)
- `[brx]` do log do usuário: 148 linhas, 66 shaders, tabelas de 3/5/6/8/13 entradas, ascendentes e sensatas (mesmo BRX na inst 30 dá 5 ou 8 entradas conforme
  o shader) — o port do BRX parece correto; a suspeita sobre ele enfraqueceu.
- Link, capa do Ganon esticados e portal do void ausente: todos deformação/partículas → suspeita volta ao compute shader `SUST` (7 dispatches/frame pulados).

- 2026-11-09 (2º crash, após a cutscene do logo): `no function at 7001810220`, lr=700180f698 (lr-4 é `blr`, sem dump do callee), x9=tabela em rodata,
  x8=0xa, x11=0x2e, x10=base+0xb8 → mesmo padrão de tabela compacta, mas o `br` não foi achado. O 1º crash (janela do ADR) foi corrigido e passou.
  Correção geral em `tools/gen.py`: `same_function()` — endereço de código materializado (ADR / ADRP+ADD) dentro da própria função do .eh_frame
  nunca é callback, então não vira início de função. Teste em `tools/test_gen_br.py`. NÃO verificado: precisa do pacote novo.
  `dispatch.c` agora também despeja 0x150 bytes antes do alvo (o `br` fica logo antes dos casos).

- 2026-11-09 SUST (compute `72a4723000`, `eb20138600f70018` = SUST.P.2D.RGBA [R0], R24, R39): traduzido (`emit_sust` em translate.c: imageStore em
  `image2D` writeonly sem formato; handle = LDC/MOV c[b][o] achado voltando no código; se não achar, o erro mostra a instrução que escreveu o registrador).
  Runtime (Vulkan): `vk_reflect` reconhece imagem de escrita; texturas com a flag NVN IMAGE (0x4) ganham `STORAGE`; `image_info()` resolve o handle
  (banco 0: slots de textura 0x20.., imagens 0x120.. — PALPITE; outros bancos: lê o handle da memória do buffer); features
  `shaderStorageImageWriteWithoutFormat/ExtendedFormats`; barreira total depois do dispatch. Teste: `tools/test_sust.sh`.
  NÃO verificado em execução (sem Vulkan/GPU aqui; só tradutor testado). O log do usuário mostra `[cs-img] ...` com handle/textura/flags: se disser
  "NOT writable" ou "(none)", o palpite do offset ou o valor da flag IMAGE está errado. Hipótese a confirmar: era isso que esticava Link/capa/portal.

- 2026-11-09 (log do usuário com o pacote da SUST): o crash dos `br` parou (cutscenes, água, teleporte). O compute `72a4723000` (SUST) traduz e roda, mas
  `[cs-img] c[2][0x120]: handle 0 -> none`: o handle está no banco 2 (não 0) → `image_info` agora trata bancos 0 e 2 como a tabela do driver (imagens em 0x120+8*i = st.img).
  Outros 2 computes (`72a4711000`, `72a4715000`, mesma instrução em 0xd28: `3b7707e061172a0f`) seguem sem tradução; ainda não identifiquei a instrução
  (não é SHF/BFI/DFMA; Rd=R15, Ra=R42, pred PT). A mensagem de erro agora traz as 3 instruções antes e depois. Gráficos ainda estranhos: corações com fundo vermelho
  opaco (alfa ignorado), Link/capa/espadas esticados, portal do void.

## 2026-11-09 (noite) — SUST por view, DEBUG_FRAME no Windows
- `vk_storage_view(tex, view)` agora usa o nível base / camada do view de cada handle (setmask 1/2): o log mostrava 6 handles da
  mesma textura (saídas de uma cadeia de mips) e todos escreviam no nível 0. **Não verificado em execução** (precisa de log novo:
  `[cs-img]` + imagem).
- `vk_save_png` cria o diretório de destino: no Windows não existe `build/` ao lado do exe e DEBUG_FRAME/SHOT_AT falhavam.
- **Aberto:** instrução `3b7707e061172a0f` (computes 72a4711000/72a4715000), reg-form 0x5d77. Campos: Rd bits 0-7, Ra 8-15,
  Rc = bits 39-46 (nos dois exemplos Rc == Rd/R31, vindo de um IADD32I logo antes), imm16 em 20-35 (0x611 / 0x610). Hipótese:
  VMNMX (vmin/vmax do PTX: d = op2(min|max(a,b), c)); sem a tabela de bits de min/max, sinal e seletores NÃO traduzido de propósito
  (chutar produziria índices de ossos errados). Pista de uso: IMNMX seguinte é max(R15,R16).
- Próximo teste do usuário: `$env:EOWRECOMP_DEBUG_FRAME="300"; .\eowrecomp.exe`; enviar `build/pass_*.png` + log (corações vermelhos).

## 2026-11-10 — diagnóstico de skinning / corações (sem correção ainda)
- Testes do usuário (RX 580) com a build 2b7ae2a: 60 fps estáveis, 0 compute pulado (SUST OK). Link/Zelda continuam esticados **só controlando**
  (algumas cutscenes normais); corpo da Zelda ok, uma peça vira um feixe vertical. `EOWRECOMP_VK_VCACHE=0` e `EOWRECOMP_VK_NO_GMEM=1`
  não mudam o esticamento (cache de vértices e gmem descartados; NO_GMEM só faz o texto sumir).
- VS 72a5c2b000 (personagem): índice de osso em a7 (uvec4, 32 bits) * 0x30 → matriz 3x4 lida de `cb6` (UBO do jogo, `ubuf[0][3]`).
- Corações: FS 7269fa9900 = (tex*cb3[0] + cb3[1]) * cor do vértice; textura 250x100 ASTC 4x4 sRGB (0x87). Ícones ASTC (0x8a/0x8b) saem certos.
  Decodificador ASTC revisado (modos de cor, pesos): sem erro achado. Blend factor: sem "unknown blend" nos logs.
- `EOWRECOMP_DEBUG_DATA=1` (com F9) agora também imprime todos os vertex streams (floats + words) e os cbufs do VS (`vcbN`, tamanho do
  range vs. declarado). **Pedir ao usuário:** `$env:EOWRECOMP_DEBUG_DATA="1"; $env:EOWRECOMP_DEBUG_FRAME="300"; .\eowrecomp.exe`, F9 com a Zelda
  esticada na tela, e enviar o log (e `build/pass_*.png` finais) — e o mesmo na cena dos corações para `fcb3`.
- Aberto: VMNMX (`3b77…`, tabelas do envytools recebidas; falta o bit min/max da 1ª operação).
- 2026-11-10 (log DEBUG_DATA, frame 2445): paletas de ossos (vcb6) OK — 1 osso = 48 B, 2 = 96 B, Link/Zelda 58 = 2784 B, matrizes com rotação/translação
  sãs; hipótese "matriz fora da faixa/lixo" não sustentada. Personagens compartilham o mesmo FS (hash 9f4e87dd94e644eb): VS 72a5c2b000,
  72ee694000, 72a6c0c000, 72a64cc000. Novo `EOWRECOMP_SKIP_VSHASH=h1,h2` (+ "vs hash" no log de draw) para achar qual VS desenha o feixe.
- 2026-11-10 (noite): formatos de atributo de esqueleto conferidos no log (a7 = R8UI/RG8UI/RGBA8UI, a8 = RG8/RGBA8/RGBA16 UNORM, a0 F32x3/F16x4,
  a6 snorm10): todos mapeados certo em `vertex_format`; 0x3E já tratado. Hipótese testável: paleta de ossos (cb6, dinâmico) copiada por
  `guest_copy` antes de o compute do mesmo submit escrever os ossos secundários (capuz/capa/espada). `cbuf_info` agora lê UBOs dinâmicos
  (não-compute) direto do heap (`guest_live`, gmem com usage UNIFORM). `EOWRECOMP_VK_LIVE_UBO=0` volta à cópia. **Não verificado em jogo**:
  se piorar (tremor/flicker de personagens), testar com LIVE_UBO=0 e reportar.
- 2026-11-10: UBO in-place (10ce7aa) NÃO resolveu o esticamento (usuário testou). Próximo: `EOWRECOMP_DEBUG_DATA` imprime por draw skinned
  "skin: N vertices, palette cbK P matrices, max bone index, indices out of range, unsane matrices". (Nota: as linhas "vbuf16" dos logs
  anteriores eram na verdade ubuf[0][0] — overrun de vbuf[16], corrigido.)
- 2026-11-10 (tarde): log DEBUG_DATA com a build 0ff6ddb+: a7 = R8UI (stride 32, 1 osso) ou RGBA8UI (stride 40, 4 ossos, pesos em a8 RGBA16 UNORM), a0 = F32x3,
  nenhum atributo usa 0x3E; 36 verificações `skin:` limpas (índices dentro da paleta de 58 matrizes, matrizes sãs). Ainda esticado (usuário testou).
  Índices, paleta e formatos de atributo estão descartados como causa. Novo em `EOWRECOMP_DEBUG_DATA` (**não verificado em jogo**):
  - `skin-cpu:` por draw skinned — faz o skinning na CPU com os mesmos dados que a GPU lê e imprime bbox bind-pose vs. skinned, distância média,
    nº de vértices a mais de 8x a média e o pior vértice (ossos e pesos). Se já aparece um feixe aqui, o problema está nos dados (pose/pesos);
    se a malha sai compacta aqui mas esticada na tela, o problema está no lado da GPU (VS traduzido, fetch do atributo).
  - `blend rtN on:` por draw — fatores/operações de blend crus + máscara de escrita + formato do RT (corações com fundo vermelho opaco).
  - `[astc] nvn fmt ... alpha min/max ...` para cada textura ASTC decodificada na CPU, e PNGs `build/astc_87_WxH_N.png` + `_alpha.png` das primeiras
    texturas 0x87 (4x4 sRGB, a dos corações é 250x100): mostra se o alfa já está errado no decodificador.
  Pedir ao usuário: `$env:EOWRECOMP_DEBUG_DATA="1"; $env:EOWRECOMP_DEBUG_FRAME="300"; .\eowrecomp.exe`, F9 com Link/Zelda esticados na tela (e
  de novo na cena dos corações), e enviar o log + os `build/astc_87_*.png`.
- 2026-11-10 (log eb740ce4, build fe3a172): 36 draws skinned, todos com índices de osso dentro da paleta (58 matrizes, max 57) e 0 matrizes
  NaN/enormes; 12 primeiras matrizes = rotação de escala 1 + translação ~(160,-45,170). Novo diagnóstico: "skin: bones used by vertices that look
  uncomputed (zero / near origin)" — hipótese: ossos secundários (capuz/capa/espada) com matriz nunca calculada → vértices colapsam à origem.
