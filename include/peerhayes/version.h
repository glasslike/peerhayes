/*
 * PeerHayes — peer-to-peer Hayes modem emulator over IP
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Unlike serial-to-TCP bridges (e.g. tcpser), both endpoints are full
 * virtual Hayes modems that negotiate a call (RING / ATA / CONNECT)
 * before switching to a transparent binary byte stream.
 */

#ifndef PEERHAYES_VERSION_H
#define PEERHAYES_VERSION_H

#define PEERHAYES_NAME "PeerHayes"
#define PEERHAYES_DAEMON_NAME "peerhayesd"
#define PEERHAYES_VERSION_MAJOR 0
#define PEERHAYES_VERSION_MINOR 1
#define PEERHAYES_VERSION_PATCH 1
/* Development builds on the `dev` branch use a -dev suffix. */
#define PEERHAYES_VERSION_STRING "0.1.1-dev"

#define PEERHAYES_AUTHOR "Alexey Matrosov"
#define PEERHAYES_FTN_ADDR "2:203/910"
/* One-line banner for --version (avoid repeating author/copyright). */
#define PEERHAYES_COPYRIGHT \
  "(c) 2026 Alexey Matrosov, 2:203/910 Fidonet."

#define PEERHAYES_DESCRIPTION \
  "Peer-to-peer Hayes modem emulator with call setup over TCP/IP"

#endif /* PEERHAYES_VERSION_H */
