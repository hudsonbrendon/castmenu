<p align="center"><img src="assets/brand/logo_sticker.png" width="320" alt="Cast Menu"></p>

<p align="center"><b>Um menu aberto e bonito para GDEMU no Sega Dreamcast.</b></p>

<p align="center">
<a href="https://github.com/hudsonbrendon/castmenu/actions/workflows/build.yml"><img src="https://github.com/hudsonbrendon/castmenu/actions/workflows/build.yml/badge.svg" alt="build"></a>
<a href="https://github.com/hudsonbrendon/castmenu/releases"><img src="https://img.shields.io/github/v/release/hudsonbrendon/castmenu" alt="release"></a>
<a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="MIT"></a>
</p>

<p align="center">🇺🇸 <a href="README.md">Read in English</a></p>

O Cast Menu substitui o GDMENU no cartão SD de um Dreamcast com GDEMU. Ele sobe como a pasta `01`,
lê os jogos das outras pastas e mostra tudo num coverflow com capas, clipes de gameplay, sinopses,
tempo de jogo e uma animação de abertura no estilo do Dreamcast. Tudo é código aberto: o menu
(KallistiOS), o loader, a ferramenta de cartão e o servidor de chat opcional.

## Recursos

- **Coverflow, lista e grade**, fluidos e animados, renderizados no PVR em 640×480.
- **Capa para todo jogo**: tirada do disco (`0GDTEX.PVR`), de um `cover.png` ao lado da imagem,
  baixada do servidor de thumbnails do libretro, ou gerada como um cartão com o título.
- **Clipes de gameplay ao fundo** quando você para num jogo (estilo Batocera), codificados pela
  ferramenta a partir de `video.mp4` ou baixados com `yt-dlp`.
- **Tela de detalhes (Y)**: tempo de jogo, produtora, ano, jogadores, discos, gênero, uma janela com
  o clipe do jogo em loop, a sinopse completa com rolagem e ◀ ▶ para ir ao jogo anterior / próximo.
- **Sinopses** da Wikipedia no idioma do menu (pt, en, es, de, fr, it) ou dos seus próprios arquivos de texto.
- **Categorias inteligentes**: Todos, Favoritos, Últimos jogados, Atomiswave, CPS1 / CPS2, Neo Geo CD, Ports e Homebrew.
- **Animação de abertura**: um Dreamcast visto de cima abre a tampa, a capa do jogo pousa no eixo como
  um disco e gira enquanto uma barra de progresso real acompanha o boot (salvar → montar imagem → iniciar).
- **Perfil** com apelido e avatar, **últimos jogados** (B) com tempo de jogo, favoritos.
- **Loader aberto** com Region Free, Force VGA, In-Game Reset (A+B+X+Y+START), animação de boot e
  tela de licença SEGA; opcionalmente usa o `loader.bin` do próprio GDMENU.
- **Temas** (Dreamcast, Dreamcast Dark, Controller, Night, Ocean, SEGA Blue, Synthwave, Terminal) em
  pacotes editáveis `theme.ini` + `bg.png`; **6 idiomas**; relógio; protetor de tela.
- **Tela de sistema (START)**: firmware do GDEMU, BIOS, região, cabo e o que está ligado em cada
  porta de controle (nome do controle, VMU, rumble pack, microfone).
- **Cast Chat** e **Cast AI** pelo adaptador BBA/LAN ou modem via DreamPi: salas, amigos, mensagens
  privadas e de voz (microfone do Dreamcast), perguntas a um LLM por texto ou voz. Opcional, precisa
  do pequeno servidor Python em `server/`.
- Funciona com cartões preparados pelo **GDMENU Card Manager** (`LIST.INI`) e com a
  **ferramenta de cartão** incluída, que faz tudo pelo PC.

## Capturas de tela

| | |
|---|---|
| ![coverflow](docs/screenshots/coverflow.png) | ![detalhes](docs/screenshots/details.png) |
| ![animação de abertura](docs/screenshots/launch.png) | ![últimos jogados](docs/screenshots/sidebar.png) |
| ![lista](docs/screenshots/list.png) | ![grade](docs/screenshots/grid.png) |
| ![ajustes](docs/screenshots/settings.png) | ![sistema](docs/screenshots/sysinfo.png) |
| ![tema Controller](docs/screenshots/theme-controller.png) | ![tema Dreamcast Dark](docs/screenshots/theme-dreamcast-dark.png) |

Capturadas no emulador redream a partir de um cartão real. Mais em `docs/screenshots/`.

## Começo rápido

1. Baixe `castmenu-<versão>.zip` em [Releases](https://github.com/hudsonbrendon/castmenu/releases)
   e descompacte. Dentro vêm o binário do menu (`1ST_READ.BIN`), a ferramenta de cartão
   (`tools/cast_card`), os temas e a documentação. Não precisa de compilador.
2. Instale os requisitos no PC (abaixo): Python 3, Pillow, numpy, `mkisofs`; `ffmpeg` e `yt-dlp`
   só se quiser clipes.
3. Coloque os jogos no cartão SD, uma imagem por pasta (`02`, `03`, ...). Veja *Preparando o cartão*.
4. Na pasta descompactada, rode:

   ```bash
   python3 -m tools.cast_card fetch /Volumes/GDEMU --videos   # capas + sinopses + clipes (opcional, precisa de internet)
   python3 -m tools.cast_card build /Volumes/GDEMU            # grava o menu em 01/
   ```

   No Windows use a letra da unidade no lugar de `/Volumes/GDEMU` (por exemplo `E:\`).
5. Ejete o cartão, coloque no GDEMU e ligue o Dreamcast.

## Requisitos

| No PC | Para quê |
|---|---|
| Python 3.10+ com `pip install pillow numpy` | ferramenta de cartão, conversão de capas e clipes |
| `mkisofs` (macOS: `brew install cdrtools`; Debian/Ubuntu: `apt install genisoimage`; Windows: cdrtools ou `xorrisofs`) | monta as faixas ISO do menu |
| `ffmpeg` (opcional) | codifica `video.mp4` no formato de clipe do menu |
| `yt-dlp` (opcional) | baixa um clipe de gameplay por jogo com `fetch --videos` |

| No console | |
|---|---|
| GDEMU com firmware **5.13.0 ou mais novo** | o menu usa os comandos de troca de imagem do GDEMU |
| Cartão SD formatado em **FAT32** (qualquer tamanho que o GDEMU aceite) | |
| VMU no controle (opcional) | guarda ajustes, perfil, favoritos e tempo de jogo (`CASTMENU.SYS`, 1 bloco) |

## Preparando o cartão SD

O GDEMU lê uma imagem por pasta numerada. A pasta `01` é o menu, as pastas `02` a `999` são jogos.

```
/GDEMU.ini           criado pela ferramenta se não existir
/01/                 Cast Menu (gravado pelo `build`; não edite à mão)
/02/disc.gdi         um jogo por pasta, GDI (com seus arquivos de faixa) ou CDI
/02/cover.png        opcional: sua própria capa (quadrada, 256×256 ou maior)
/02/synopsis.pt.txt  opcional: sua própria sinopse naquele idioma
/02/video.mp4        opcional: um vídeo curto de gameplay (qualquer duração, a ferramenta corta)
/03/game.cdi
/avatars/*.png       opcional: fotos de perfil (64×64 bastam)
/server.txt          opcional: host:porta do seu servidor Cast Chat
/loader.bin          opcional: loader do GDMENU, selecionável em Ajustes → Loader
/themes/             opcional: temas extras (mesmo formato de `themes/` neste repositório)
```

As imagens podem ser GDI (o `.gdi` mais todos os `trackNN.bin/raw`), CDI ou ISO simples. O nome dos
arquivos não importa; o número da pasta importa. Arquivos compactados precisam ser extraídos antes.

Adicionando jogos pelo PC:

```bash
python3 -m tools.cast_card add  /Volumes/GDEMU "Sonic Adventure.gdi" "Crazy Taxi.cdi"   # copia para as próximas pastas livres
python3 -m tools.cast_card sort /Volumes/GDEMU            # renumera as pastas em ordem alfabética (opcional)
python3 -m tools.cast_card scan /Volumes/GDEMU            # lista o que a ferramenta vê: nome, região, GD/CD, disco n/m, VGA, produto, capa
```

Também dá para criar as pastas e copiar os arquivos à mão; o `build` só precisa da imagem dentro.

## Capas, sinopses e clipes

Tudo é opcional e em camadas; a ferramenta usa o primeiro que existir:

| Capa | Sinopse | Clipe de fundo |
|---|---|---|
| `cover.png` / `.jpg` na pasta do jogo | `synopsis.<idioma>.txt` na pasta do jogo (`pt`, `en`, `es`, `de`, `fr`, `it`) | `video.cvm` já codificado |
| `--covers PASTA` com arquivos `<PRODUTO>.png` (id do produto como no disco, ex. `MK51000.png`) | `synopsis.txt` (vale para qualquer idioma) | `video.mp4` / `.mkv` / `.webm` / `.mov` na pasta do jogo |
| `0GDTEX.PVR` dentro da imagem do disco | baixada pelo `fetch` da Wikipedia (idioma do menu primeiro, depois inglês) | baixado pelo `fetch --videos` (primeiro resultado do YouTube para "<título> dreamcast gameplay") |
| boxart do libretro baixada pelo `fetch` | | |
| um cartão gerado com o título | | |

```bash
python3 -m tools.cast_card fetch /Volumes/GDEMU                 # capas + sinopses que faltam
python3 -m tools.cast_card fetch /Volumes/GDEMU --videos        # também um clipe por jogo
python3 -m tools.cast_card fetch /Volumes/GDEMU --force         # refaz mesmo o que já existe
python3 -m tools.cast_card fetch /Volumes/GDEMU --no-synopses   # só capas
python3 -m tools.cast_card build /Volumes/GDEMU --videos-redo   # recodifica todos os clipes
```

O `fetch` grava `cover.png`, `synopsis.<idioma>.txt` e `video.mp4` ao lado de cada imagem, então
você pode conferir ou trocar antes de rodar o `build`. Os clipes são cortados em 12 s a 10 fps,
256×256, com barras pretas removidas automaticamente; ficam na área de alta densidade da imagem do
menu e são lidos direto do GD-ROM sem decodificação no console.

Outros arquivos de texto por jogo (uma linha cada): `name.txt` (título mostrado no menu),
`genre.txt`, `year.txt`, `players.txt`, `publisher.txt`, `category.txt` ou `platform.txt` (coloca o
jogo naquele carrossel; Atomiswave, CPS1 / CPS2, Neo Geo CD e Ports e Homebrew também são detectados
pelo título).

## Gravando o cartão

```bash
python3 -m tools.cast_card build /Volumes/GDEMU
```

O `build` varre todas as pastas, lê o `IP.BIN` de cada disco (nome, produto, região, VGA, disco n/m),
grava `name.txt` e `serial.txt` ao lado de cada imagem, gera `LIST.INI` (formato GDMENU) e
`CASTMENU.INI` (metadados extras), converte capas, sinopses, avatares, temas e clipes, e grava o menu
como um GDI em `01/`. Também cria um `GDEMU.ini` com valores padrão se não houver nenhum.

Opções: `--menu-bin ARQUIVO` (outro `1ST_READ.BIN`), `--covers PASTA`, `--themes PASTA`,
`--server host:porta`, `--gdmenu-loader ARQUIVO`, `--videos-redo`, `--out PASTA` (grava o menu em
outro lugar, por exemplo para testar num emulador).

Rode o `build` de novo sempre que adicionar, remover ou renomear jogos ou mudar qualquer extra. Ele
só reescreve `01/`; as pastas dos jogos não são tocadas, exceto por `name.txt` / `serial.txt` / `video.cvm`.

**macOS**: o Finder deixa arquivos `._*` em cartões FAT; rode `dot_clean -m /Volumes/GDEMU` antes de ejetar.
**Windows**: rode os comandos no PowerShell dentro da pasta do release com `py -3 -m tools.cast_card ...`.

## Usando o menu

| Tela | Botões |
|---|---|
| Biblioteca (coverflow / lista / grade) | ◀ ▶ (▲ ▼) navega · **A** iniciar · **Y** detalhes · **B** últimos jogados · **X** ajustes · **START** sistema · **L / R** categoria anterior / próxima · **L + R** pular para uma letra |
| Detalhes | ◀ ▶ jogo anterior / próximo · ▲ ▼ rola a sinopse · **L / R** página · **A** iniciar · **Y** favorito · **B** voltar |
| Últimos jogados (sidebar) | ▲ ▼ · **A** iniciar · **B** fechar |
| Ajustes | ▲ ▼ · **A** / ◀ ▶ muda · **START** salva no VMU · **B** voltar |
| Cast Chat | **X** escrever · **Y** segurar para gravar voz · **L / R** salas / amigos · **A** abrir · **B** voltar |
| Teclado na tela | **A** digita · **B** apaga · **X** espaço · **L / R** shift · **START** enviar · **Y** cancelar |
| Dentro do jogo | **A + B + X + Y + START** volta ao menu (In-Game Reset) |

Jogos com vários discos mostram só o disco 1 na biblioteca; ao iniciar, o menu pergunta qual disco.

### Ajustes (X)

- **Perfil**: apelido, avatar.
- **Loader**: Region Free, Force VGA, In-Game Reset, animação de boot, tela de licença SEGA, loader
  (Cast ou o `loader.bin` do GDMENU).
- **Visual**: tema, visualização padrão, idioma, vídeo de fundo, capas.
- **Biblioteca**: ordem, filtro de região, esconder discos extras.
- **Online**: Cast Chat, Cast AI, servidor, conectar ao ligar.
- **Sistema**: protetor de tela, atraso de scan do GDEMU, salvar no VMU, restaurar padrões,
  informações do sistema, sair para o menu do console.

Os ajustes são salvos no VMU (`CASTMENU.SYS`) com START; sem VMU o menu funciona com os padrões.

## Temas

Um tema é uma pasta com `theme.ini` (cores, badges, ajustes de layout), `bg.png` (fundo) e um
`logo.png` opcional. Edite os de `themes/`, ou adicione pastas em `/themes` na raiz do cartão, e rode o
`build`. Escolha em Ajustes → Tema. Formato em [docs/THEMES.md](docs/THEMES.md).

## Cast Chat e Cast AI (opcional)

`server/cast_server.py` é um servidor Python pequeno (só biblioteca padrão) para salas, amigos,
mensagens privadas e de voz, mais uma ponte com um LLM para o Cast AI:

```bash
python3 server/cast_server.py --ai anthropic      # ANTHROPIC_API_KEY no ambiente
```

Coloque `host:porta` em `/server.txt` no cartão (ou Ajustes → Servidor). O Dreamcast conecta pelo
adaptador BBA / LAN (DHCP) ou pelo modem via DreamPi. Protocolo e configuração em
[docs/CHAT.md](docs/CHAT.md) e [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Compatibilidade

- Cartões feitos com o **GDMENU Card Manager** funcionam como estão: o Cast Menu lê o `LIST.INI`.
  Rode o `build` uma vez para ganhar capas, sinopses e clipes.
- Cartões do **openMenu**: as pastas dos jogos são as mesmas; rode o `build` para trocar a `01`.
- Firmware do GDEMU 5.13.0+; firmwares antigos mostram um aviso e não trocam de imagem.
- Testado num console real com GDEMU e no emulador redream (tudo menos o boot do jogo em si, que
  precisa da BIOS real e do GDEMU).

## Problemas comuns

| Sintoma | Solução |
|---|---|
| Cartão não reconhecido / LED do GDEMU piscando | só FAT32; confira que `01/disc.gdi` existe; ejete direito (`dot_clean -m` no macOS) |
| "Atualize o GDEMU para 5.13.0 ou superior" | grave um firmware mais novo no GDEMU |
| Jogo não aparece na lista | uma imagem por pasta; a pasta precisa ser um número; rode `scan` para ver o erro |
| Um jogo não inicia com o loader aberto | Ajustes → Loader → GDMENU (precisa do `loader.bin` na raiz do cartão) |
| Título ou capa errados | `name.txt` / `cover.png` na pasta do jogo, depois `build` |
| Jogo sem sinopse | adicione `synopsis.<idioma>.txt`; a Wikipedia não tem artigo de tudo |
| Textos do menu errados depois de compilar do fonte | `./build.sh clean && ./build.sh` |

## Compilando do fonte

Docker + a imagem `kazade/dreamcast-sdk` (KallistiOS 2.3):

```bash
./build.sh            # assets → build KOS → 1ST_READ.BIN
./build.sh clean
python3 tests/test_card_tool.py
```

Detalhes, teste no emulador e CI em [docs/BUILD.md](docs/BUILD.md). As notas de engenharia reversa
que tornaram o loader possível estão em [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md).

Pastas: `src/` menu (KOS, C), `src/net/` protocolo do chat, `ldr/` loader aberto, `server/` servidor
de referência, `tools/cast_card/` ferramenta de cartão, `themes/` temas, `assets/lang/strings.tsv`
traduções, `docs/`.

## Roadmap

Ideias na mesa, em ordem aproximada de interesse. Abra uma issue para votar ou sugerir outras.

- Cheats por jogo (base do CodeBreaker) aplicados pelo loader.
- Gerenciador de saves do VMU: backup / restauração dos saves para o cartão SD pelo menu.
- Game ID para troca automática de VMU virtual no VM2 / VMUPro, e acerto do relógio pelo VMU.
- Pastas virtuais / coleções e busca com o teclado na tela.
- Música de fundo (WAV/OGG no cartão) e sons por tema.
- Screenshots e contracapa na tela de detalhes; estatísticas estilo RetroAchievements para tempo de jogo.
- Online: presença no Dreamcast Now, acerto de hora, backup do `CASTMENU.SYS` na nuvem.
- Navegação por mouse e teclado.

## Créditos e licença

MIT. Obrigado a neuroacid (GDMENU), Deunan (GDEMU), KallistiOS, megavolt85 (gdemu_sdk), mrneo240
(openMenu), sonik-br (GDMENU Card Manager), ao projeto de thumbnails do libretro e à Wikipedia.
Dreamcast e o espiral são marcas da SEGA; este é um projeto de fãs sem afiliação. A arte do logo do
Dreamcast vem do Wikimedia Commons (domínio público como logo simples); a fonte do menu é a fonte de
fãs NiseSegaDreamcast.
