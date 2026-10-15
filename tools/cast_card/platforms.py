"""Category detection for the carousels: Atomiswave, CPS1/CPS2, Neo Geo CD and
Ports & Homebrew ports are recognised by title keywords (the ports have no
reliable product id). A platform.txt / category.txt in the game folder wins,
then genre.txt."""
import re

ATOMISWAVE = ["metal slug 6", "guilty gear isuka", "guilty gear x ver", "dolphin blue", "demolish fist", "fist of the north star",
    "hokuto no ken", "king of fighters xi", "king of fighters neowave", "neogeo battle coliseum", "neo geo battle coliseum",
    "samurai shodown vi", "samurai spirits tenkaichi", "rumble fish", "maximum speed", "sports jam", "ranger mission",
    "faster than speed", "knights of valour", "bass fishing challenge", "clay challenge", "animal basket", "block pong",
    "extreme hunting", "sushi bar", "waiwai drive", "net select", "force five"]
CPS = ["street fighter zero", "street fighter alpha", "gem fighter", "pocket fighter", "darkstalkers", "vampire savior", "vampire chronicle",
    "vampire hunter", "night warriors", "marvel vs capcom", "marvel super heroes", "x-men vs", "x-men: children", "dungeons & dragons",
    "super puzzle fighter", "cyberbots", "giga wing", "gigawing", "19xx", "1944", "progear", "mars matrix", "dimahoo", "great mahou daisakusen",
    "super street fighter ii", "street fighter ii", "final fight", "captain commando", "knights of the round", "king of dragons",
    "three wonders", "armored warriors", "alien vs. predator", "battle circuit", "mega man: the power battle", "rockman: the power battle",
    "jojo's bizarre", "jojo no kimyou", "quiz nanairo", "capcom sports club", "eco fighters", "ring of destruction", "saturday night slam"]
NEOGEO_CD = ["metal slug", "king of fighters '9", "king of fighters 9", "kof '9", "samurai shodown", "samurai spirits", "fatal fury", "garou densetsu",
    "art of fighting", "ryuuko no ken", "world heroes", "last blade", "neo turf masters", "big tournament golf", "super baseball 2020",
    "3 count bout", "ninja master's", "aggressors of dark kombat", "savage reign", "kizuna encounter", "magician lord", "blue's journey",
    "nam-1975", "sengoku", "pulstar", "blazing star", "top hunter", "crossed swords", "mutation nation", "ninja commando", "ninja combat",
    "burning fight", "robo army", "football frenzy", "super sidekicks", "neo drift out", "over top", "galaxy fight", "breakers",
    "double dragon", "real bout", "shock troopers", "twinkle star", "puzzle bobble", "bust-a-move", "neo bomberman", "windjammers",
    "flying power disc", "ghost pilots", "league bowling", "baseball stars", "waku waku 7", "ninja masters", "karnov's revenge"]
HOMEBREW = ["homebrew", "port)", "(port", "quake", "doom", "wolfenstein", "duke nukem", "hexen", "heretic", "cave story", "sonic cd",
    "dreamshell", "retro", "emulator", "nesterdc", "snes", "genesis plus", "gens", "scummvm", "openlara", "rush rush rally", "ghost blade",
    "xenocider", "intrepid izzy", "shadow gangs", "tapeworm", "alice dreams", "sturmwind", "gunlord", "neo xyx", "fast striker",
    "flashback", "another world", "simulant", "frogger", "atari", "amiga", "c64", "4x4 jam", "giana", "jet set willy", "dcastle", "dreamroq"]

def detect(title, product=""):
    t = (title or "").lower()
    for kws, name in ((ATOMISWAVE, "Atomiswave"), (CPS, "CPS1 / CPS2"), (NEOGEO_CD, "Neo Geo CD"), (HOMEBREW, "Ports e Homebrew")):
        for k in kws:
            if k in t: return name
    return None

def category_for(folder_files, title, genre=None):
    """folder_files: dict name->content for platform.txt/category.txt/genre.txt."""
    for k in ("category.txt", "platform.txt"):
        v = (folder_files.get(k) or "").strip()
        if v: return v
    d = detect(title)
    if d: return d
    g = (folder_files.get("genre.txt") or genre or "").strip()
    return g or None

if __name__ == "__main__":
    assert detect("Metal Slug 6") == "Atomiswave"
    assert detect("Street Fighter Zero 2") == "CPS1 / CPS2"
    assert detect("Super Baseball 2020") == "Neo Geo CD"
    assert detect("Quake III Arena") == "Ports e Homebrew"
    assert detect("Sonic Adventure") is None
    assert category_for({"genre.txt": "Luta"}, "Sonic Adventure") == "Luta"
    assert category_for({"category.txt": "Infantil"}, "Metal Slug 6") == "Infantil"
    print("platforms ok")
