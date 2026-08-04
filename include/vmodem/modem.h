/*
 * PeerHayes — virtual modem instance (call state machine + DTE hooks)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * One vmod_modem_t is one Hayes endpoint. It talks to:
 *   - a DTE (mailer/terminal) via serial callbacks
 *   - a peer modem via vmod_transport_t (TCP or loopback) using VMCP/1
 *
 * CONNECT is emitted to the DTE only after peer answer negotiation — never
 * merely because a TCP socket became writable.
 */

#ifndef VMODEM_MODEM_H
#define VMODEM_MODEM_H

#include "vmodem/hayes.h"
#include "vmodem/ringbuf.h"
#include "vmodem/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vmod_transport;

typedef void (*vmod_dte_write_fn)(void *user, const uint8_t *data, size_t len);
typedef void (*vmod_state_fn)(void *user, vmod_call_state_t old_st,
                              vmod_call_state_t new_st);

typedef struct vmod_modem {
  char name[32];
  vmod_hayes_cfg_t cfg;
  vmod_call_state_t state;
  int cmd_mode; /* 1 = accept AT; 0 = online data */
  int dcd;
  int ringing;

  char at_line[VMOD_AT_LINE_MAX];
  size_t at_len;
  int saw_a;
  int cmd_started;

  /* Escape sequence +++ (guard timing refined in escape.c / tick) */
  int esc_count;
  int esc_armed;

  struct vmod_transport *transport;
  void *dir_ctx; /* vmod_directory_t* */

  vmod_dte_write_fn dte_write;
  void *dte_user;
  vmod_state_fn on_state;
  void *state_user;

  uint8_t dte_tx_storage[4096];
  vmod_ringbuf_t dte_tx;

  uint8_t peer_rx_storage[4096];
  vmod_ringbuf_t peer_rx;

  char pending_dial[VMOD_DIAL_MAX];
  int rings_sent;
  uint64_t timer_deadline_ms;
  int timer_active;
  uint64_t next_ring_ms; /* RING cadence while RINGING (~3s) */
} vmod_modem_t;

void vmod_modem_init(vmod_modem_t *m, const char *name);
void vmod_modem_set_dte_writer(vmod_modem_t *m, vmod_dte_write_fn fn, void *user);
void vmod_modem_set_state_cb(vmod_modem_t *m, vmod_state_fn fn, void *user);
void vmod_modem_set_transport(vmod_modem_t *m, struct vmod_transport *t);
void vmod_modem_set_directory(vmod_modem_t *m, void *dir_ctx);

void vmod_modem_reset(vmod_modem_t *m);
void vmod_modem_set_state(vmod_modem_t *m, vmod_call_state_t st);

void vmod_modem_dte_input(vmod_modem_t *m, const uint8_t *data, size_t len);
size_t vmod_modem_poll_dte_out(vmod_modem_t *m, uint8_t *dst, size_t dst_sz);

void vmod_modem_on_peer_call(vmod_modem_t *m, const char *dialstring);
void vmod_modem_on_peer_answer(vmod_modem_t *m);
void vmod_modem_on_peer_connect(vmod_modem_t *m, int speed);
void vmod_modem_on_peer_busy(vmod_modem_t *m);
void vmod_modem_on_peer_no_answer(vmod_modem_t *m);
void vmod_modem_on_peer_hangup(vmod_modem_t *m);
void vmod_modem_on_peer_data(vmod_modem_t *m, const uint8_t *data, size_t len);
void vmod_modem_on_transport_closed(vmod_modem_t *m);

/* Dispatch one VMCP control line (CR not required in len). */
void vmod_modem_handle_vmcp_line(vmod_modem_t *m, const char *line, size_t len);

/* Advance timers / RING cadence; now_ms is monotonic milliseconds. */
void vmod_modem_tick(vmod_modem_t *m, uint64_t now_ms);

void vmod_modem_emit_result(vmod_modem_t *m, vmod_result_t code);
void vmod_modem_emit_connect(vmod_modem_t *m);
void vmod_modem_emit_text(vmod_modem_t *m, const char *text);
void vmod_modem_begin_dial(vmod_modem_t *m, const char *number);
void vmod_modem_answer(vmod_modem_t *m);
void vmod_modem_hangup(vmod_modem_t *m);
void vmod_modem_go_online(vmod_modem_t *m);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_MODEM_H */
