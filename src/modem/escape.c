/*
 * PeerHayes — Hayes online escape (+++) with guard time
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * A real Hayes modem leaves transparent mode only for this sequence:
 *   1. no DTE bytes for the guard time (S12, in units of 1/50 s)
 *   2. exactly three escape characters (S2, default '+')
 *   3. each of those characters less than one guard time after the previous
 *   4. no further DTE byte for another guard time
 * The three characters are not sent to the peer. Anything else, including
 * '+' bytes inside Hydra or ZMODEM frames, is data and is forwarded.
 * S12 = 0 disables recognition so every byte is data.
 */

#include "vmodem/modem.h"
#include "vmodem/log.h"
#include "vmodem/transport.h"

#include <string.h>

static unsigned guard_ms(const vmod_modem_t *m) {
  return (unsigned)m->cfg.s[12] * 20u;
}

/* The held escape bytes were not a completed +++. Deliver them as data. */
static void flush_held(vmod_modem_t *m) {
  uint8_t buf[3];
  int i;
  uint8_t esc;

  if (m->esc_count <= 0)
    return;
  esc = m->cfg.s[2];
  if (esc == 0)
    esc = (uint8_t)'+';
  for (i = 0; i < m->esc_count && i < 3; i++)
    buf[i] = esc;
  if (m->transport && m->transport->connected)
    vmod_transport_send(m->transport, buf, (size_t)m->esc_count);
  m->esc_count = 0;
}

static void forward_byte(vmod_modem_t *m, uint8_t byte) {
  if (m->transport && m->transport->connected)
    vmod_transport_send(m->transport, &byte, 1);
}

void vmod_escape_on_byte(vmod_modem_t *m, uint8_t byte, uint64_t now_ms) {
  unsigned guard;
  uint8_t esc;

  if (!m || m->state != VMOD_STATE_ONLINE)
    return;
  guard = guard_ms(m);
  esc = m->cfg.s[2];
  if (esc == 0)
    esc = (uint8_t)'+';

  /* Escape disabled, or this byte is ordinary payload. */
  if (guard == 0 || byte != esc) {
    flush_held(m);
    forward_byte(m, byte);
    m->esc_quiet_ms = now_ms;
    return;
  }

  if (m->esc_count == 0) {
    /* A '+' starts a candidate only after a full leading guard. */
    if (m->esc_quiet_ms != 0 && now_ms >= m->esc_quiet_ms &&
        (now_ms - m->esc_quiet_ms) >= guard) {
      m->esc_count = 1;
      m->esc_plus_ms = now_ms;
      return;
    }
    forward_byte(m, byte);
    m->esc_quiet_ms = now_ms;
    return;
  }

  /* A fourth '+', or a gap as long as the guard, means the hold is data. */
  if (m->esc_count >= 3 || now_ms < m->esc_plus_ms ||
      (now_ms - m->esc_plus_ms) >= guard) {
    flush_held(m);
    forward_byte(m, byte);
    m->esc_quiet_ms = now_ms;
    return;
  }

  m->esc_count++;
  m->esc_plus_ms = now_ms;
  /* Three characters are held until the trailing guard expires in tick. */
}

void vmod_escape_on_idle(vmod_modem_t *m, uint64_t now_ms) {
  unsigned guard = guard_ms(m);

  if (!m || m->state != VMOD_STATE_ONLINE || guard == 0 || m->esc_count <= 0)
    return;
  if (now_ms < m->esc_plus_ms || (now_ms - m->esc_plus_ms) < guard)
    return;

  if (m->esc_count == 3) {
    VMOD_LOGI("%s escape +++ detected", m->name);
    m->esc_count = 0;
    m->cmd_mode = 1;
    vmod_modem_set_state(m, VMOD_STATE_ONLINE_CMD);
    vmod_modem_emit_result(m, VMOD_RESULT_OK);
    return;
  }

  /* One or two '+' followed by silence: they were payload, sent late. */
  flush_held(m);
  m->esc_quiet_ms = now_ms;
}
