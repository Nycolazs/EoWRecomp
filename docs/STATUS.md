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

## Marco atual: M0 — base montada (antes do M1)

Feito:
- LASRecomp (`d23597d`) copiado como base, licença MIT e crédito mantidos; partes do LA removidas (randomizer,
  `third_party/LAS-Randomizer`, `tools/rando`, HLE de gameplay, abas de randomizer/warp/save editor, texpatch,
  imagens e ícone do LAS); renomeado (`eowrecomp`, `eowgame`, `eowgen`, prefixo `EOWRECOMP_`, `eowrecomp.ini`);
  `tools/patches.py` zerado (só o mecanismo com `expect()`); verificador de updates desativado; `GetInfo` com o
  title ID do EoW.
- Extração: XCI → NCAs com hactool (compilado do fonte); ExeFS/RomFS com `tools/nca_extract.py` (novo — o
  hactool não lê o RomFS comprimido). Arquivos em `extracted/{exefs,romfs,nca,meta,control}` (ignorados pelo git).
- `tools/venv` com capstone 5.0.9, unicorn 2.1.4, lz4 4.4.5, cryptography.

Não funciona / pendente:
- **O runtime não compila ainda**: ainda há referências em C ao que foi removido (rando, warp, texpatch, abas do
  menu) e a máquina Linux não tem clang/lld/SDL3/Vulkan/shaderc instalados. Corte dessas referências: no M2, com
  compilador para validar.
- `docs/linux.md` e `docs/BUILD-windows.md` são do LAS (só renomeados); revisar quando o build existir.
- Nada foi commitado (aguardando aprovação).

## Próximo passo: M1
1. Inventário medido → `docs/GAP.md` (módulos, símbolos, imports por origem, NVN, serviços IPC, SVCs).
2. `gen.py` para os 4 módulos (bases novas: `main` não cabe antes de `0x7004000000`), lifter com `LDXP/STXP`
   e LSE; meta: zero `Unsupported` ou lista justificada.
