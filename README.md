# PeerHayes

**Peer-to-peer Hayes modem emulator over TCP/IP**

Version **0.1.0**  
Author: **Alexey Matrosov** · FidoNet **2:203/910**  
License: MIT

```text
Legacy app ── COM/tty ── peerhayesd ── TCP (call setup, then raw) ── peerhayesd ── COM/tty ── Legacy app
```

## Why PeerHayes (not just another modem emulator)

Most open-source tools in this space (**tcpser**, **modemu2k**, etc.) turn a serial port into a **TCP client/server bridge**: `ATDT host:port` opens a socket, and the remote side is usually a BBS, telnet service, or a single emulator waiting for a raw TCP connection.

**PeerHayes** emulates **two real Hayes modems** that share a virtual phone line:

1. Caller dials a number (`ATDT…`) mapped to `host:port`
2. Callee receives **RING** on its serial/COM port
3. Callee answers with **ATA** (or S0 auto-answer)
4. **Both** sides get **CONNECT** only after answer negotiation
5. The link becomes a **transparent binary stream** (ZMODEM, EMSI, Hydra, …)

That call state machine is the product. The IP network replaces the telephone line — not the modem.

## Daemon

| Item | Value |
|---|---|
| Program | `peerhayesd` |
| Project | PeerHayes |
| Version | 0.1.0 |

```bat
peerhayesd --config examples/peerhayes-com-a.conf --debug
peerhayesd --version
```

## Build (Windows / MinGW)

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Binary: `build/peerhayesd.exe`

## Quick test (two peers on one PC)

```bat
build\peerhayesd.exe --config examples\peerhayes-com-b.conf --debug
build\peerhayesd.exe --config examples\peerhayes-com-a.conf --debug
```

Point each mailer/terminal at the **other** end of a com0com pair (e.g. mailer on COM3, `peerhayesd` on COM10).

## Configuration sketch

```ini
[modem]
name = A
serial = COM10
baudrate = 57600
modem_speed = 9600

[network]
listen = 0.0.0.0:5001

[dial]
5283806 = 192.168.1.20:5002
```

Wire protocol during call setup: **VMCP/1** (PeerHayes call control). After CONNECT the socket carries raw octets only.

## Status (0.1.0)

- Hayes command subset used by classic FTN mailers
- Symmetric CALL / RING / ANSWER / CONNECT over TCP
- Win32 COM / named pipe / stdio DTE
- DCD via DTR for com0com null-modem pairs
- Tested: KittenMail on Windows 10 host ↔ KittenMail on Windows 11 VirtualBox VM

## Roadmap

- FreeDOS (host pipe / native)
- Debian / POSIX PTY peer
- Broader Hayes compatibility as needed by real software

## License

MIT — see [LICENSE](LICENSE).

Design notes: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
