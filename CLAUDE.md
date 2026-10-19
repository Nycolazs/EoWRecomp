# EoWRecomp — regras permanentes

Recompilação estática de *The Legend of Zelda: Echoes of Wisdom* (Switch, 2024) para PC. Derivado do
LASRecomp (MIT, https://github.com/peiikai/LASRecomp, commit base `d23597d`). Não é emulador: o ARM64 do jogo
vira C em tempo de build; o runtime substitui o Horizon e a NVN. Arquitetura e plano: `docs/ARCHITECTURE.md`;
estado atual e próximo passo: `docs/STATUS.md` (leia antes de começar).

## Arquivos da Nintendo
- **Nenhum arquivo da Nintendo entra no repositório**: nada de `extracted/`, `generated/`, chaves, XCI/NSP/NCA/NSO,
  fontes do sistema, texturas, modelos, música, texto ou qualquer asset — nem "um arquivo só para teste".
  O `.gitignore` cobre isso; confira com `git status --ignored` antes de cada commit.
- Os arquivos do usuário ficam em `/home/nycolas/Downloads/data` (sempre entre aspas: o nome tem espaços).
  **Só leitura.** Nunca copiar o XCI nem as chaves para dentro do repo. Não pedir, procurar nem sugerir fontes
  para esses arquivos.
- Extração: `tools/nca_extract.py` (o hactool não lê o RomFS comprimido deste NCA) para `extracted/`
  (`exefs/`, `romfs/`). Uma cópia filtrada das chaves, se precisar, vai para o scratchpad, nunca para o repo.
- Versão fixa: **title 01008CF01BAAC000, v0 (1.0.0)**, build IDs em `docs/STATUS.md`. Endereços de patches e HLE
  valem só para ela.

## Código
- Código gerado (`generated/`) nunca é editado à mão: mudanças vão em `tools/lifter.py`, `tools/gen.py` ou
  `tools/patches.py`.
- Todo patch confere a palavra de instrução original antes de aplicar (`expect()` em `tools/patches.py`).
- Seguir o estilo do código ao redor (nomes, estrutura, densidade de comentários). Comentários explicam o
  *porquê* (o que o jogo faz, o que é um endereço, que log/teste mostrou o problema).
- Para cada falha, achar a causa (Capstone, logs, Switchbrew, envytools) em vez de contornar. Recurso NVN ou
  instrução Maxwell faltando: implementar corretamente e adicionar teste em `tools/` (`test_translate`,
  `test_shaders`, `test_simd`, `test_lift`).
- Variáveis de ambiente de depuração usam o prefixo `EOWRECOMP_`.

## Verificação
- Uma correção só está verificada depois de vista funcionando (screenshot, log ou teste) — nunca declarar
  sucesso com base em "deve funcionar".
- Mudança no renderer compartilhado: dizer qual backend foi testado (Vulkan / Metal).
- Medir em vez de estimar (rodar as ferramentas: `tools/nso.py`, `tools/analyze.py --census`, logs).
- Avançar por marcos (M1…M7, `docs/STATUS.md`); só passar ao próximo com o atual comprovado.

## Git
- Commits pequenos e descritivos; um assunto por commit.
- README: projeto de fã, sem afiliação com a Nintendo, sem arquivos do jogo, exige cópia legal do próprio
  usuário; créditos ao LASRecomp e a todas as bibliotecas usadas, com suas licenças.

## Fim de sessão
- Atualizar `docs/STATUS.md`: marco atual, o que funciona, o que quebrou, próximo passo, e o que o usuário deve
  testar e observar.
