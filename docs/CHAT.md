# Cast Chat, Cast AI and voice messages

## Network on the console

`src/net/netif.c` brings the link up in a thread (status in the top bar: signal bars):

1. **Broadband / LAN adapter**: KOS `INIT_NET` probes the BBA (HIT-0400) and the LAN adapter
   (HIT-0300) at boot; `net_dhcp_request()` then gets an address.
2. **Modem via DreamPi ("Wi-Fi")**: with no Ethernet device the modem is initialised and PPP is
   dialled with any number and the login `dream`/`dreamcast`, which is what a DreamPi (Raspberry Pi
   + USB modem bridging to the Pi's Wi-Fi) accepts. Any other PPP bridge that answers the call works.

"Connect at boot" (Settings → Online) starts this automatically; otherwise use *Connect*.

## Server

`server/cast_server.py` is a stdlib-only asyncio TCP server (SQLite for friends, private and
room history):

```bash
python3 server/cast_server.py --port 7350                      # chat + echo AI
ANTHROPIC_API_KEY=sk-... python3 server/cast_server.py --ai anthropic
python3 server/cast_server.py --stt-cmd 'whisper {wav} --model tiny --output_format txt -o /tmp >/dev/null; cat /tmp/$(basename {wav} .wav).txt'
```

The console learns the address from `server.txt` on the SD root (written into `CASTMENU.INI`
as `server=host:port`) or from Settings → Online → Server (on-screen keyboard). The value is saved
on the VMU.

## Protocol

Line based, one command per line (`src/net/proto.h` has the full table):

```
HELLO <nick> <lang> 1          WELCOME <nick> <motd>
ROOMS / JOIN r / SAY r text    ROOMS a:3 b:0 / JOINED r / MSG r nick text
FRIENDS / ADD n / DEL n        FRIENDS n:1 m:0 ... / PRESENCE n 1|0
PM n text                      PM from text
VOICE n <base64 mu-law 8 kHz>  VOICE from <base64>
HISTORY n|r                    (replay) ... HISTORY_END
AI text / AIVOICE <base64>     AI text
PING t                         PONG t
```

Voice is exactly what the Dreamcast microphone produces (`SIP_SAMPLE_8BIT_ULAW`, 8 kHz), up to
12 s (96 KB, ~128 KB base64). The client is `src/net/proto.c`, portable C; `tests/test_proto.py`
compiles it on the host and runs it against the server (rooms, friends, PM, voice relay, AI, errors,
presence).

## In the menu

* **Cast Chat** (Settings → Online): L/R switch Rooms/Friends, A opens a room or a friend,
  X writes with the on-screen keyboard (or a Dreamcast keyboard), hold **Y** to record a voice
  message and release to send; incoming voice plays on the AICA. Unread private messages show a
  toast and a counter next to your nickname in the coverflow.
* **Cast AI**: X types a question, A sends, hold **Y** to ask by voice (the server transcribes with
  `--stt-cmd`). Answers come in the menu language.

## Not verified without hardware

Networking (BBA/DreamPi), the microphone and AICA playback cannot run in redream; the UI paths
were exercised offline and the protocol on the host.
