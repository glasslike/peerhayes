/*
 * PeerHayes — TCP transport factory (Winsock / BSD sockets)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_TCP_H
#define VMODEM_TCP_H

#include "vmodem/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize Winsock/network if needed. Safe to call multiple times. */
vmod_status_t vmod_net_init(void);
void vmod_net_shutdown(void);

/* Create a TCP transport bound to modem later via vmod_modem_set_transport. */
vmod_status_t vmod_tcp_transport_create(vmod_transport_t **out);
void vmod_tcp_transport_destroy(vmod_transport_t *t);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_TCP_H */
