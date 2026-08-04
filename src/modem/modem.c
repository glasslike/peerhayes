/*
 * PeerHayes — modem call state machine (dial, ring, answer, online, hangup)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * CONNECT is issued to the DTE only after VMCP ANSWER/CONNECT from the peer,
 * never on TCP connect alone.
 */

#include "vmodem/modem.h"
#include "vmodem/directory.h"
#include "vmodem/log.h"
#include "vmodem/transport.h"
#include "vmodem/vmcp.h"

#include <stdio.h>
#include <string.h>

/* from escape.c */
void vmod_escape_on_byte(vmod_modem_t *m, uint8_t bare, uint64_t now_ms);
void vmod_escape_on_idle(vmod_modem_t *m, uint64_t now_ms);

static const char *state_name(vmod_call_state_t s) {
  switch (s) {
  case VMOD_STATE_IDLE:
    return "IDLE";
  case VMOD_STATE_DIALING:
    return "DIALING";
  case VMOD_STATE_CALL_SETUP:
    return "CALL_SETUP";
  case VMOD_STATE_RINGING:
    return "RINGING";
  case VMOD_STATE_ANSWERING:
    return "ANSWERING";
  case VMOD_STATE_ONLINE:
    return "ONLINE";
  case VMOD_STATE_ONLINE_CMD:
    return "ONLINE_CMD";
  case VMOD_STATE_DISCONNECTING:
    return "DISCONNECTING";
  default:
    return "?";
  }
}

void vmod_modem_init(vmod_modem_t *m, const char *name) {
  memset(m, 0, sizeof(*m));
  if (name)
    snprintf(m->name, sizeof(m->name), "%s", name);
  else
    snprintf(m->name, sizeof(m->name), "modem");
  vmod_hayes_cfg_defaults(&m->cfg);
  m->state = VMOD_STATE_IDLE;
  m->cmd_mode = 1;
  vmod_ringbuf_init(&m->dte_tx, m->dte_tx_storage, sizeof(m->dte_tx_storage));
  vmod_ringbuf_init(&m->peer_rx, m->peer_rx_storage, sizeof(m->peer_rx_storage));
}

void vmod_modem_set_dte_writer(vmod_modem_t *m, vmod_dte_write_fn fn,
                               void *user) {
  m->dte_write = fn;
  m->dte_user = user;
}

void vmod_modem_set_state_cb(vmod_modem_t *m, vmod_state_fn fn, void *user) {
  m->on_state = fn;
  m->state_user = user;
}

void vmod_modem_set_transport(vmod_modem_t *m, vmod_transport_t *t) {
  m->transport = t;
  if (t)
    t->modem = m;
}

void vmod_modem_set_directory(vmod_modem_t *m, void *dir_ctx) {
  m->dir_ctx = dir_ctx;
}

void vmod_modem_set_state(vmod_modem_t *m, vmod_call_state_t st) {
  vmod_call_state_t old = m->state;
  if (old == st)
    return;
  m->state = st;
  VMOD_LOGI("%s state %s -> %s", m->name, state_name(old), state_name(st));
  if (m->on_state)
    m->on_state(m->state_user, old, st);
}

void vmod_modem_reset(vmod_modem_t *m) {
  int speed = m->cfg.modem_speed;
  char last_dial[VMOD_DIAL_MAX];
  strncpy(last_dial, m->cfg.last_dial, sizeof(last_dial));
  vmod_hayes_cfg_defaults(&m->cfg);
  m->cfg.modem_speed = speed;
  strncpy(m->cfg.last_dial, last_dial, sizeof(m->cfg.last_dial) - 1);
  m->cmd_mode = 1;
  m->at_len = 0;
  m->saw_a = 0;
  m->cmd_started = 0;
  m->esc_count = 0;
  m->ringing = 0;
  m->rings_sent = 0;
  if (m->state != VMOD_STATE_IDLE && m->state != VMOD_STATE_RINGING)
    vmod_modem_hangup(m);
}

void vmod_modem_emit_text(vmod_modem_t *m, const char *text) {
  char buf[128];
  size_t n;
  uint8_t cr = m->cfg.s[3];
  uint8_t lf = m->cfg.s[4];
  n = 0;
  buf[n++] = (char)cr;
  buf[n++] = (char)lf;
  while (*text && n + 3 < sizeof(buf))
    buf[n++] = *text++;
  buf[n++] = (char)cr;
  buf[n++] = (char)lf;
  vmod_ringbuf_write(&m->dte_tx, (const uint8_t *)buf, n);
  if (m->dte_write)
    m->dte_write(m->dte_user, (const uint8_t *)buf, n);
}

void vmod_modem_emit_result(vmod_modem_t *m, vmod_result_t code) {
  char buf[64];
  size_t n = vmod_hayes_format_result(&m->cfg, code, buf, sizeof(buf));
  if (n == 0)
    return;
  vmod_ringbuf_write(&m->dte_tx, (const uint8_t *)buf, n);
  if (m->dte_write)
    m->dte_write(m->dte_user, (const uint8_t *)buf, n);
  VMOD_LOGT("%s -> DTE result %d", m->name, (int)code);
}

void vmod_modem_emit_connect(vmod_modem_t *m) {
  char buf[64];
  size_t n = vmod_hayes_format_connect(&m->cfg, buf, sizeof(buf));
  if (n == 0)
    return;
  vmod_ringbuf_write(&m->dte_tx, (const uint8_t *)buf, n);
  if (m->dte_write)
    m->dte_write(m->dte_user, (const uint8_t *)buf, n);
}

static void send_vmcp(vmod_modem_t *m, vmcp_msg_type_t type, const char *arg,
                      int speed) {
  char line[VMCP_LINE_MAX];
  size_t n;
  if (!m->transport)
    return;
  n = vmcp_format(type, arg, speed, line, sizeof(line));
  if (n == 0)
    return;
  VMOD_LOGD("%s VMCP TX: %.*s", m->name, (int)(n > 0 && line[n - 1] == '\r' ? n - 1 : n),
            line);
  vmod_transport_send(m->transport, (const uint8_t *)line, n);
}

void vmod_modem_begin_dial(vmod_modem_t *m, const char *number) {
  char endpoint[128];
  vmod_directory_t *dir = (vmod_directory_t *)m->dir_ctx;

  strncpy(m->pending_dial, number, sizeof(m->pending_dial) - 1);
  vmod_modem_set_state(m, VMOD_STATE_DIALING);
  m->cmd_mode = 0;

  if (!dir || vmod_directory_resolve(dir, number, endpoint, sizeof(endpoint)) !=
                  VMOD_OK) {
    VMOD_LOGW("%s dial %s: unresolved", m->name, number);
    vmod_modem_emit_result(m, VMOD_RESULT_NO_DIALTONE);
    m->cmd_mode = 1;
    vmod_modem_set_state(m, VMOD_STATE_IDLE);
    return;
  }

  VMOD_LOGI("%s dialing %s -> %s", m->name, number, endpoint);
  if (!m->transport ||
      vmod_transport_dial(m->transport, endpoint) != VMOD_OK) {
    vmod_modem_emit_result(m, VMOD_RESULT_NO_CARRIER);
    m->cmd_mode = 1;
    vmod_modem_set_state(m, VMOD_STATE_IDLE);
    return;
  }

  m->transport->control_phase = 1;
  vmod_modem_set_state(m, VMOD_STATE_CALL_SETUP);
  send_vmcp(m, VMCP_MSG_CALL, number, 0);
  m->timer_active = 1;
  m->timer_deadline_ms = 0; /* set by tick with now + S7 */
}

void vmod_modem_answer(vmod_modem_t *m) {
  if (m->state != VMOD_STATE_RINGING && !m->ringing) {
    vmod_modem_emit_result(m, VMOD_RESULT_ERROR);
    return;
  }
  VMOD_LOGI("%s answering (S0=%u state=%d)", m->name, (unsigned)m->cfg.s[0],
            (int)m->state);
  m->next_ring_ms = 0;
  vmod_modem_set_state(m, VMOD_STATE_ANSWERING);
  send_vmcp(m, VMCP_MSG_ANSWER, NULL, 0);
  /* Peer will send CONNECT; we also emit CONNECT when peer confirms */
  send_vmcp(m, VMCP_MSG_CONNECT, NULL, m->cfg.modem_speed);
  m->ringing = 0;
  m->dcd = 1;
  m->cmd_mode = 0;
  if (m->transport)
    m->transport->control_phase = 0;
  vmod_modem_emit_connect(m);
  vmod_modem_set_state(m, VMOD_STATE_ONLINE);
}

void vmod_modem_hangup(vmod_modem_t *m) {
  int was_active = (m->state != VMOD_STATE_IDLE) ||
                   (m->transport && m->transport->connected);

  if (!was_active) {
    /* Already on-hook — mailers often spam ATH0; stay quiet. */
    VMOD_LOGD("%s hangup (already idle)", m->name);
    m->cmd_mode = 1;
    return;
  }

  VMOD_LOGI("%s hangup", m->name);
  if (m->transport && m->transport->connected) {
    if (m->transport->control_phase)
      send_vmcp(m, VMCP_MSG_HANGUP, NULL, 0);
    vmod_transport_close(m->transport);
  }
  m->dcd = m->cfg.dcd_always_on ? 1 : 0;
  m->ringing = 0;
  m->rings_sent = 0;
  m->next_ring_ms = 0;
  m->cmd_mode = 1;
  m->esc_count = 0;
  m->timer_active = 0;
  if (m->state != VMOD_STATE_IDLE) {
    vmod_modem_set_state(m, VMOD_STATE_DISCONNECTING);
    vmod_modem_emit_result(m, VMOD_RESULT_NO_CARRIER);
    vmod_modem_set_state(m, VMOD_STATE_IDLE);
  }
}

void vmod_modem_go_online(vmod_modem_t *m) {
  if (m->state == VMOD_STATE_ONLINE_CMD) {
    m->cmd_mode = 0;
    m->esc_count = 0;
    vmod_modem_set_state(m, VMOD_STATE_ONLINE);
    /* classic modems may not emit OK again after ATO */
  }
}

void vmod_modem_handle_vmcp_line(vmod_modem_t *m, const char *line, size_t len) {
  vmcp_msg_t msg;
  if (vmcp_parse(line, len, &msg) != VMOD_OK) {
    VMOD_LOGW("%s bad VMCP: %.*s", m->name, (int)len, line);
    return;
  }
  VMOD_LOGD("%s VMCP RX type=%d arg='%s' speed=%d", m->name, (int)msg.type,
            msg.arg, msg.speed);
  switch (msg.type) {
  case VMCP_MSG_CALL:
    vmod_modem_on_peer_call(m, msg.arg);
    break;
  case VMCP_MSG_RING:
    break;
  case VMCP_MSG_ANSWER:
    vmod_modem_on_peer_answer(m);
    break;
  case VMCP_MSG_CONNECT:
    vmod_modem_on_peer_connect(m, msg.speed);
    break;
  case VMCP_MSG_BUSY:
    vmod_modem_on_peer_busy(m);
    break;
  case VMCP_MSG_NOANSWER:
    vmod_modem_on_peer_no_answer(m);
    break;
  case VMCP_MSG_HANGUP:
    vmod_modem_on_peer_hangup(m);
    break;
  default:
    break;
  }
}

static void emit_ring(vmod_modem_t *m) {
  m->rings_sent++;
  VMOD_LOGI("%s RING #%d", m->name, m->rings_sent);
  vmod_modem_emit_result(m, VMOD_RESULT_RING);
  send_vmcp(m, VMCP_MSG_RING, NULL, 0);
  if (m->cfg.s[0] > 0 && m->rings_sent >= m->cfg.s[0])
    vmod_modem_answer(m);
}

void vmod_modem_on_peer_call(vmod_modem_t *m, const char *dialstring) {
  (void)dialstring;
  if (m->state != VMOD_STATE_IDLE) {
    send_vmcp(m, VMCP_MSG_BUSY, NULL, 0);
    return;
  }
  m->ringing = 1;
  m->rings_sent = 0;
  m->next_ring_ms = 0; /* tick schedules the following RINGs */
  vmod_modem_set_state(m, VMOD_STATE_RINGING);
  /* First RING immediately; more RINGs from tick() every ~3s. */
  emit_ring(m);
}

void vmod_modem_on_peer_answer(vmod_modem_t *m) {
  if (m->state != VMOD_STATE_CALL_SETUP && m->state != VMOD_STATE_DIALING)
    return;
  /* Wait for CONNECT message for speed; answer alone is progress */
  VMOD_LOGD("%s peer ANSWER", m->name);
}

void vmod_modem_on_peer_connect(vmod_modem_t *m, int speed) {
  if (speed > 0)
    m->cfg.modem_speed = speed;
  m->dcd = 1;
  m->cmd_mode = 0;
  m->timer_active = 0;
  if (m->transport)
    m->transport->control_phase = 0;
  if (m->state == VMOD_STATE_CALL_SETUP || m->state == VMOD_STATE_DIALING ||
      m->state == VMOD_STATE_ANSWERING) {
    vmod_modem_emit_connect(m);
    vmod_modem_set_state(m, VMOD_STATE_ONLINE);
  }
}

void vmod_modem_on_peer_busy(vmod_modem_t *m) {
  m->cmd_mode = 1;
  vmod_modem_emit_result(m, VMOD_RESULT_BUSY);
  if (m->transport)
    vmod_transport_close(m->transport);
  vmod_modem_set_state(m, VMOD_STATE_IDLE);
}

void vmod_modem_on_peer_no_answer(vmod_modem_t *m) {
  m->cmd_mode = 1;
  vmod_modem_emit_result(m, VMOD_RESULT_NO_ANSWER);
  if (m->transport)
    vmod_transport_close(m->transport);
  vmod_modem_set_state(m, VMOD_STATE_IDLE);
}

void vmod_modem_on_peer_hangup(vmod_modem_t *m) {
  vmod_modem_hangup(m);
}

void vmod_modem_on_peer_data(vmod_modem_t *m, const uint8_t *data, size_t len) {
  if (m->state != VMOD_STATE_ONLINE && m->state != VMOD_STATE_ONLINE_CMD)
    return;
  if (m->state == VMOD_STATE_ONLINE_CMD)
    return; /* held */
  vmod_ringbuf_write(&m->dte_tx, data, len);
  if (m->dte_write)
    m->dte_write(m->dte_user, data, len);
}

void vmod_modem_on_transport_closed(vmod_modem_t *m) {
  if (m->state == VMOD_STATE_ONLINE || m->state == VMOD_STATE_ONLINE_CMD ||
      m->state == VMOD_STATE_CALL_SETUP || m->state == VMOD_STATE_RINGING ||
      m->state == VMOD_STATE_ANSWERING) {
    m->dcd = m->cfg.dcd_always_on ? 1 : 0;
    m->cmd_mode = 1;
    vmod_modem_emit_result(m, VMOD_RESULT_NO_CARRIER);
    vmod_modem_set_state(m, VMOD_STATE_IDLE);
  }
}

void vmod_modem_dte_input(vmod_modem_t *m, const uint8_t *data, size_t len) {
  if (m->cmd_mode) {
    vmod_hayes_feed(m, data, len);
    return;
  }

  /* Online transparent path */
  if (m->state == VMOD_STATE_ONLINE) {
    size_t i;
    for (i = 0; i < len; i++) {
      uint8_t bare = (uint8_t)(data[i] & 0x7f);
      vmod_escape_on_byte(m, bare, 0);
    }
    if (m->esc_count >= 3) {
      vmod_escape_on_idle(m, 0);
      return; /* do not forward +++ */
    }
    if (m->transport && m->transport->connected)
      vmod_transport_send(m->transport, data, len);
  } else if (m->state == VMOD_STATE_ONLINE_CMD) {
    vmod_hayes_feed(m, data, len);
  }
}

size_t vmod_modem_poll_dte_out(vmod_modem_t *m, uint8_t *dst, size_t dst_sz) {
  return vmod_ringbuf_read(&m->dte_tx, dst, dst_sz);
}

void vmod_modem_tick(vmod_modem_t *m, uint64_t now_ms) {
  if (m->timer_active && m->timer_deadline_ms == 0) {
    m->timer_deadline_ms = now_ms + (uint64_t)m->cfg.s[7] * 1000ull;
  }
  if (m->timer_active && now_ms >= m->timer_deadline_ms) {
    m->timer_active = 0;
    if (m->state == VMOD_STATE_CALL_SETUP) {
      VMOD_LOGI("%s dial timeout (S7)", m->name);
      send_vmcp(m, VMCP_MSG_NOANSWER, NULL, 0);
      vmod_modem_on_peer_no_answer(m);
    }
  }

  if (m->state == VMOD_STATE_RINGING) {
    if (m->next_ring_ms == 0)
      m->next_ring_ms = now_ms + 3000;
    else if (now_ms >= m->next_ring_ms) {
      if (m->rings_sent >= 10) {
        VMOD_LOGI("%s ring timeout", m->name);
        send_vmcp(m, VMCP_MSG_NOANSWER, NULL, 0);
        vmod_modem_hangup(m);
      } else {
        emit_ring(m);
        if (m->state == VMOD_STATE_RINGING)
          m->next_ring_ms = now_ms + 3000;
      }
    }
  }
}
