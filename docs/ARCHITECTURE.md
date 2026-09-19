# PeerHayes architecture

**PeerHayes** — peer-to-peer Hayes modem emulator over IP  
Version 0.1.1-dev · Alexey Matrosov · 2:203/910

## Goal

A pair of software Hayes modems that negotiate a real call (RING / ATA / CONNECT) over TCP, then pass a transparent binary byte stream. Legacy applications on each side see a normal modem on a COM/tty port.

```text
App ── RS-232 ── Hayes UI ── Call SM ── IP transport ── Call SM ── Hayes UI ── RS-232 ── App
```

Not an analog DSP / V.xx audio modem.  
Not a one-sided serial↔TCP bridge.

## Difference from tcpser and similar tools

| Tool | Model |
|---|---|
| tcpser / modemu2k | Serial port ↔ TCP; dialer often gets CONNECT when TCP connects |
| **PeerHayes** | Two Hayes peers; CONNECT only after remote answer (VMCP/1) |

## Layers

```text
cli/          peerhayesd — config, logging, main loop
hayes/        AT parser, S-registers, result codes
modem/        call state machine, escape (+++)
session/      VMCP/1 peer call control
directory/    phone number → host:port
transport/    TCP (+ loopback for tests)
serial/       Win32 COM / pipe / stdio (+ POSIX later)
```

**Rule:** Hayes logic never parses TCP frames; TCP never parses AT commands.

## Call states

`IDLE` → `DIALING` → `CALL_SETUP` → `ONLINE` / `ONLINE_CMD`  
Inbound: `IDLE` → `RINGING` → `ANSWERING` → `ONLINE`

## VMCP/1 (wire protocol)

ASCII CR-terminated control lines until CONNECT, then raw octets:

```text
VMCP/1 CALL <dialstring>
VMCP/1 RING
VMCP/1 ANSWER
VMCP/1 CONNECT <dce_speed>
VMCP/1 BUSY | NOANSWER | HANGUP | ERROR …
```

## DTE / control lines

With com0com-style null modem, **local DTR is seen by the application as DCD**. PeerHayes raises DTR only while ONLINE (unless `&C0`).

## Platforms (0.1.1-dev)

- Windows 10/11 host and guest (COM, com0com, TCP) — tested with KittenMail
- Later: FreeDOS (host pipe / Watt-32), Linux PTY, OS/2

## Internal code prefix

C identifiers use the historical `vmod_` / `vmodem/` prefix for the library modules. The product name and daemon are **PeerHayes** / **peerhayesd**.
