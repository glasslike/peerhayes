/*
 * PeerHayes — in-process loopback transport for tests
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/loopback.h"
#include "vmodem/log.h"

#include <string.h>

typedef struct {
  vmod_loopback_pair_t *pair;
  int side; /* 0=A, 1=B */
} lb_impl_t;

static lb_impl_t g_impl_a;
static lb_impl_t g_impl_b;

static uint8_t *out_q(vmod_loopback_pair_t *pair, int side, size_t **len) {
  if (side == 0) {
    *len = &pair->q_ab_len;
    return pair->q_ab;
  }
  *len = &pair->q_ba_len;
  return pair->q_ba;
}

static uint8_t *in_q(vmod_loopback_pair_t *pair, int side, size_t **len) {
  if (side == 0) {
    *len = &pair->q_ba_len;
    return pair->q_ba;
  }
  *len = &pair->q_ab_len;
  return pair->q_ab;
}

static vmod_status_t lb_listen(vmod_transport_t *t, const char *bind_ep) {
  (void)bind_ep;
  t->connected = 0;
  t->control_phase = 1;
  return VMOD_OK;
}

static vmod_status_t lb_dial(vmod_transport_t *t, const char *endpoint) {
  lb_impl_t *impl = (lb_impl_t *)t->impl;
  vmod_loopback_pair_t *pair = impl->pair;
  vmod_transport_t *peer = impl->side == 0 ? &pair->b : &pair->a;

  (void)endpoint;
  t->connected = 1;
  t->control_phase = 1;
  peer->connected = 1;
  peer->control_phase = 1;
  VMOD_LOGD("loopback dial side=%d connected", impl->side);
  return VMOD_OK;
}

static vmod_status_t lb_send(vmod_transport_t *t, const uint8_t *data,
                             size_t len) {
  lb_impl_t *impl = (lb_impl_t *)t->impl;
  size_t *qlen;
  uint8_t *q;
  size_t cap = 8192;

  if (!t->connected)
    return VMOD_ERR_IO;
  q = out_q(impl->pair, impl->side, &qlen);
  if (*qlen + len > cap)
    return VMOD_ERR_IO;
  memcpy(q + *qlen, data, len);
  *qlen += len;
  return VMOD_OK;
}

static void lb_close(vmod_transport_t *t) {
  lb_impl_t *impl = (lb_impl_t *)t->impl;
  vmod_loopback_pair_t *pair = impl->pair;
  vmod_transport_t *peer = impl->side == 0 ? &pair->b : &pair->a;
  int was = t->connected;

  t->connected = 0;
  t->control_phase = 1;
  if (peer->connected) {
    peer->connected = 0;
    peer->control_phase = 1;
    if (peer->modem)
      vmod_modem_on_transport_closed(peer->modem);
  }
  (void)was;
}

static void pump_side(vmod_transport_t *t) {
  lb_impl_t *impl = (lb_impl_t *)t->impl;
  size_t *qlen;
  uint8_t *q;
  size_t i;

  if (!t->modem)
    return;
  q = in_q(impl->pair, impl->side, &qlen);
  if (*qlen == 0)
    return;

  if (!t->control_phase) {
    vmod_modem_on_peer_data(t->modem, q, *qlen);
    *qlen = 0;
    return;
  }

  /* control phase: split on CR */
  i = 0;
  while (i < *qlen) {
    size_t start = i;
    while (i < *qlen && q[i] != '\r')
      i++;
    if (i >= *qlen)
      break; /* incomplete */
    vmod_modem_handle_vmcp_line(t->modem, (const char *)q + start, i - start);
    i++; /* skip CR */
    if (i < *qlen && q[i] == '\n')
      i++;
    /* if we left control phase mid-buffer, rest is data */
    if (!t->control_phase) {
      size_t rem = *qlen - i;
      if (rem)
        vmod_modem_on_peer_data(t->modem, q + i, rem);
      *qlen = 0;
      return;
    }
  }
  if (i > 0) {
    memmove(q, q + i, *qlen - i);
    *qlen -= i;
  }
}

static void lb_poll(vmod_transport_t *t) {
  (void)t;
  /* pair poll drives both sides */
}

static const vmod_transport_ops_t g_lb_ops = {
    lb_listen, lb_dial, lb_send, lb_close, lb_poll};

void vmod_loopback_init(vmod_loopback_pair_t *pair, vmod_modem_t *a,
                        vmod_modem_t *b) {
  memset(pair, 0, sizeof(*pair));
  pair->modem_a = a;
  pair->modem_b = b;

  g_impl_a.pair = pair;
  g_impl_a.side = 0;
  g_impl_b.pair = pair;
  g_impl_b.side = 1;

  pair->a.ops = &g_lb_ops;
  pair->a.impl = &g_impl_a;
  pair->a.modem = a;
  pair->a.control_phase = 1;

  pair->b.ops = &g_lb_ops;
  pair->b.impl = &g_impl_b;
  pair->b.modem = b;
  pair->b.control_phase = 1;

  vmod_modem_set_transport(a, &pair->a);
  vmod_modem_set_transport(b, &pair->b);
  vmod_transport_listen(&pair->a, "loopback");
  vmod_transport_listen(&pair->b, "loopback");
}

void vmod_loopback_poll(vmod_loopback_pair_t *pair) {
  /* Multiple passes so CALL -> RING -> ANSWER -> CONNECT settle. */
  int n;
  for (n = 0; n < 8; n++) {
    pump_side(&pair->a);
    pump_side(&pair->b);
  }
}
