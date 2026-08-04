/*
 * PeerHayes — online escape sequence (+++) handling
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/modem.h"
#include "vmodem/log.h"

#include <string.h>

/* Escape sequence handling while online (not ONLINE_CMD). */
void vmod_escape_on_byte(vmod_modem_t *m, uint8_t bare, uint64_t now_ms) {
  uint8_t esc = m->cfg.s[2];
  unsigned guard_ms = (unsigned)m->cfg.s[12] * 20u; /* S12 in 1/50s */

  (void)now_ms;
  (void)guard_ms;

  if (bare == esc) {
    if (m->esc_count < 3)
      m->esc_count++;
  } else {
    m->esc_count = 0;
    m->esc_armed = 0;
  }
}

void vmod_escape_on_idle(vmod_modem_t *m, uint64_t now_ms) {
  unsigned guard_ms = (unsigned)m->cfg.s[12] * 20u;

  (void)now_ms;
  if (m->state != VMOD_STATE_ONLINE)
    return;

  /* Simplified for Phase 1: after three '+' with esc_armed, enter ONLINE_CMD.
   * Full guard-time tracking arrives with the event loop. */
  if (m->esc_count >= 3) {
    VMOD_LOGI("%s escape +++ detected", m->name);
    m->esc_count = 0;
    m->cmd_mode = 1;
    vmod_modem_set_state(m, VMOD_STATE_ONLINE_CMD);
    vmod_modem_emit_result(m, VMOD_RESULT_OK);
  }
  (void)guard_ms;
}
