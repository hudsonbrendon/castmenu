#!/usr/bin/env python3
"""Cast Chat / Cast AI reference server (asyncio, stdlib only).

    python3 server/cast_server.py --port 7350 [--db cast.sqlite] [--ai anthropic|echo] [--stt-cmd "whisper {wav} ..."]

Protocol: see src/net/proto.h. Rooms, friends, private messages (stored so
offline friends get them on their next HISTORY), voice relay (base64 mu-law),
AI relay. AI backends: "echo" (default, no key needed) or "anthropic"
(ANTHROPIC_API_KEY env, Claude Messages API over urllib). Voice questions are
transcribed with --stt-cmd, a shell command receiving {wav} and printing text.
"""
import argparse, asyncio, base64, json, os, sqlite3, struct, subprocess, tempfile, time, urllib.request, wave

VERSION = "1"
ROOMS = ["geral", "dreamcast", "arcade", "ajuda"]

class DB:
    def __init__(self, path):
        self.c = sqlite3.connect(path); self.c.row_factory = sqlite3.Row
        self.c.executescript("""
        create table if not exists friends(owner text, friend text, primary key(owner, friend));
        create table if not exists pm(id integer primary key, ts real, src text, dst text, kind text, body text, delivered int default 0);
        create table if not exists room_msg(id integer primary key, ts real, room text, src text, body text);
        """)
    def friends(self, nick): return [r[0] for r in self.c.execute("select friend from friends where owner=? order by friend", (nick,))]
    def add(self, a, b): self.c.execute("insert or ignore into friends values(?,?)", (a, b)); self.c.commit()
    def rem(self, a, b): self.c.execute("delete from friends where owner=? and friend=?", (a, b)); self.c.commit()
    def pm(self, src, dst, kind, body): self.c.execute("insert into pm(ts,src,dst,kind,body) values(?,?,?,?,?)", (time.time(), src, dst, kind, body)); self.c.commit()
    def history(self, a, b, n=30):
        return list(self.c.execute("select * from (select * from pm where (src=? and dst=?) or (src=? and dst=?) order by id desc limit ?) order by id", (a, b, b, a, n)))
    def room_say(self, room, src, body): self.c.execute("insert into room_msg(ts,room,src,body) values(?,?,?,?)", (time.time(), room, src, body)); self.c.commit()
    def room_history(self, room, n=30): return list(self.c.execute("select * from (select * from room_msg where room=? order by id desc limit ?) order by id", (room, n)))

class Client:
    def __init__(self, w): self.w = w; self.nick = None; self.lang = "en"; self.room = None
    def send(self, line):
        try: self.w.write((line + "\n").encode("latin-1", "replace"))
        except Exception: pass

class Server:
    def __init__(self, db, ai, stt_cmd, motd):
        self.db, self.ai, self.stt_cmd, self.motd = db, ai, stt_cmd, motd
        self.clients = {}   # nick -> Client

    def online(self, nick): return nick in self.clients
    def room_users(self, room): return sum(1 for c in self.clients.values() if c.room == room)
    def presence(self, nick, on):
        for c in self.clients.values():
            if nick in self.db.friends(c.nick): c.send(f"PRESENCE {nick} {1 if on else 0}")
    def friends_line(self, nick): return "FRIENDS " + " ".join(f"{f}:{1 if self.online(f) else 0}" for f in self.db.friends(nick))

    async def handle(self, r, w):
        c = Client(w)
        try:
            while True:
                raw = await r.readline()
                if not raw: break
                line = raw.decode("latin-1").rstrip("\r\n")
                if not line: continue
                cmd, _, rest = line.partition(" ")
                if cmd == "HELLO":
                    parts = rest.split(" ")
                    nick = parts[0][:15].replace("\x01", "") or "Player"
                    base, i = nick, 2
                    while nick in self.clients: nick = f"{base}{i}"; i += 1
                    c.nick, c.lang = nick, (parts[1] if len(parts) > 1 else "en")
                    self.clients[nick] = c
                    c.send(f"WELCOME {nick} {self.motd}")
                    self.presence(nick, True)
                    continue
                if not c.nick: c.send("ERR hello first"); continue
                if cmd == "PING": c.send(f"PONG {rest}")
                elif cmd == "ROOMS": c.send("ROOMS " + " ".join(f"{rm}:{self.room_users(rm)}" for rm in ROOMS))
                elif cmd == "JOIN":
                    room = rest.strip().split(" ")[0][:30]
                    if room not in ROOMS: c.send("ERR no such room"); continue
                    c.room = room; c.send(f"JOINED {room}")
                elif cmd == "SAY":
                    room, _, text = rest.partition(" ")
                    text = text[:239]
                    if room not in ROOMS or not text: continue
                    self.db.room_say(room, c.nick, text)
                    for o in self.clients.values():
                        if o.room == room: o.send(f"MSG {room} {c.nick} {text}")
                elif cmd == "FRIENDS": c.send(self.friends_line(c.nick))
                elif cmd == "ADD":
                    f = rest.strip().split(" ")[0][:15]
                    if f and f != c.nick: self.db.add(c.nick, f)
                    c.send(self.friends_line(c.nick))
                elif cmd == "DEL":
                    self.db.rem(c.nick, rest.strip().split(" ")[0]); c.send(self.friends_line(c.nick))
                elif cmd in ("PM", "VOICE"):
                    dst, _, body = rest.partition(" ")
                    body = body[:239] if cmd == "PM" else body
                    if not dst or not body: continue
                    if cmd == "VOICE" and dst in ROOMS:
                        for o in self.clients.values():
                            if o.room == dst and o is not c: o.send(f"VOICE {c.nick} {body}")
                        continue
                    self.db.pm(c.nick, dst, cmd, body)
                    if dst in self.clients: self.clients[dst].send(f"{cmd} {c.nick} {body}")
                elif cmd == "HISTORY":
                    who = rest.strip().split(" ")[0]
                    if who in ROOMS:
                        for m in self.db.room_history(who): c.send(f"MSG {who} {m['src']} {m['body']}")
                    else:
                        for m in self.db.history(c.nick, who):
                            if m["kind"] == "PM": c.send(f"PM {m['src'] if m['src'] != c.nick else who} {m['body']}" if m["src"] != c.nick else f"PM {who} {m['body']}")
                    c.send("HISTORY_END")
                elif cmd == "WHO": c.send("WHO " + " ".join(self.clients))
                elif cmd == "AI":
                    q = rest[:239]
                    asyncio.create_task(self.ai_reply(c, q))
                elif cmd == "AIVOICE":
                    asyncio.create_task(self.ai_voice(c, rest))
                else: c.send("ERR unknown command")
        except (ConnectionResetError, asyncio.IncompleteReadError):
            pass
        finally:
            if c.nick and self.clients.get(c.nick) is c:
                del self.clients[c.nick]; self.presence(c.nick, False)
            try: w.close()
            except Exception: pass

    async def ai_reply(self, c, q):
        try: text = await asyncio.get_event_loop().run_in_executor(None, self.ai, q, c.lang, c.nick)
        except Exception as e: text = f"(AI error: {e})"
        for chunk in [text[i:i + 230] for i in range(0, max(1, len(text)), 230)]: c.send("AI " + chunk.replace("\n", " "))

    async def ai_voice(self, c, b64):
        try:
            ulaw = base64.b64decode(b64)
            text = await asyncio.get_event_loop().run_in_executor(None, self.transcribe, ulaw)
        except Exception as e: c.send(f"AI (voice error: {e})"); return
        if not text: c.send("AI (no transcription configured: run the server with --stt-cmd)"); return
        c.send(f"AI > {text[:200]}")
        await self.ai_reply(c, text)

    def transcribe(self, ulaw):
        if not self.stt_cmd: return ""
        pcm = ulaw_to_pcm16(ulaw)
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f: path = f.name
        with wave.open(path, "wb") as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(8000); w.writeframes(pcm)
        try: return subprocess.run(self.stt_cmd.format(wav=path), shell=True, capture_output=True, text=True, timeout=120).stdout.strip()
        finally: os.unlink(path)

def ulaw_to_pcm16(data):
    out = bytearray()
    for u in data:
        u = ~u & 0xFF
        t = (((u & 0x0F) << 3) + 0x84) << ((u & 0x70) >> 4)
        out += struct.pack("<h", (0x84 - t) if (u & 0x80) else (t - 0x84))
    return bytes(out)

# ---------- AI backends ----------
def ai_echo(q, lang, nick):
    return {"pt": f"{nick}, voce perguntou: '{q}'. Configure --ai anthropic no servidor para respostas de verdade.",
            "es": f"{nick}, preguntaste: '{q}'. Configura --ai anthropic en el servidor.",
            "de": f"{nick}, du hast gefragt: '{q}'. Starte den Server mit --ai anthropic.",
            "fr": f"{nick}, vous avez demande : '{q}'. Lancez le serveur avec --ai anthropic.",
            "it": f"{nick}, hai chiesto: '{q}'. Avvia il server con --ai anthropic."}.get(lang, f"{nick}, you asked: '{q}'. Run the server with --ai anthropic for real answers.")

LANG_NAMES = {"pt": "Brazilian Portuguese", "en": "English", "es": "Spanish", "de": "German", "fr": "French", "it": "Italian"}
def ai_anthropic(q, lang, nick):
    key = os.environ.get("ANTHROPIC_API_KEY")
    if not key: return "ANTHROPIC_API_KEY not set on the server."
    body = json.dumps({"model": os.environ.get("CAST_AI_MODEL", "claude-sonnet-5-5"), "max_tokens": 300,
        "system": f"You are Cast AI, a friendly Sega Dreamcast expert living inside the Cast Menu on the player's TV. Answer in {LANG_NAMES.get(lang, 'English')}, in at most 3 short sentences, plain text, ASCII/Latin-1 only (no emoji). The player's nickname is {nick}.",
        "messages": [{"role": "user", "content": q}]}).encode()
    req = urllib.request.Request("https://api.anthropic.com/v1/messages", data=body, headers={"x-api-key": key, "anthropic-version": "2023-06-01", "content-type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as r: data = json.load(r)
    return "".join(b.get("text", "") for b in data.get("content", []))

async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=7350); ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--db", default="cast.sqlite"); ap.add_argument("--ai", choices=["echo", "anthropic"], default="echo")
    ap.add_argument("--stt-cmd", default=os.environ.get("CAST_STT_CMD", "")); ap.add_argument("--motd", default="Bem-vindo ao Cast Chat!")
    a = ap.parse_args()
    srv = Server(DB(a.db), ai_anthropic if a.ai == "anthropic" else ai_echo, a.stt_cmd, a.motd)
    server = await asyncio.start_server(srv.handle, a.host, a.port, limit=1 << 20)
    print(f"cast server on {a.host}:{a.port} (ai={a.ai})", flush=True)
    async with server: await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
