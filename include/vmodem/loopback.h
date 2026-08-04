/*
 * PeerHayes — in-process paired transport for unit tests / demos
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_LOOPBACK_H
#define VMODEM_LOOPBACK_H

#include "vmodem/modem.h"
#include "vmodem/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  vmod_transport_t a;
  vmod_transport_t b;
  vmod_modem_t *modem_a;
  vmod_modem_t *modem_b;
  /* Simple byte queues between peers */
  uint8_t q_ab[8192];
  size_t q_ab_len;
  uint8_t q_ba[8192];
  size_t q_ba_len;
} vmod_loopback_pair_t;

void vmod_loopback_init(vmod_loopback_pair_t *pair, vmod_modem_t *a,
                        vmod_modem_t *b);
void vmod_loopback_poll(vmod_loopback_pair_t *pair);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_LOOPBACK_H */
