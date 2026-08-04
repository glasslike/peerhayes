/*
 * PeerHayes — Hayes AT command parser and result-code formatting
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Implements the practical AT subset used by classic FTN mailers and
 * terminal programs (ATZ, dial/answer, S-registers, &C/&D/&K, …).
 * Unknown single-letter commands are ignored for init-string compatibility.
 */

#ifndef VMODEM_HAYES_H
#define VMODEM_HAYES_H

#include "vmodem/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VMOD_SREG_COUNT 64
#define VMOD_AT_LINE_MAX 256
#define VMOD_DIAL_MAX 64

struct vmod_modem;

typedef struct {
  int echo;              /* ATE */
  int quiet;             /* ATQ */
  int verbose;           /* ATV */
  int result_level;      /* ATX 0..4 */
  int dcd_always_on;     /* AT&C0 => 1, &C1 => 0 */
  int dtr_mode;          /* AT&D 0..2 */
  int flow_rts;          /* AT&K3 */
  uint8_t s[VMOD_SREG_COUNT];
  char last_dial[VMOD_DIAL_MAX];
  char last_cmd[VMOD_AT_LINE_MAX];
  int modem_speed;       /* reported CONNECT speed */
} vmod_hayes_cfg_t;

void vmod_hayes_cfg_defaults(vmod_hayes_cfg_t *cfg);

/* Feed DTE bytes while in command mode. Emits result codes via modem callbacks. */
void vmod_hayes_feed(struct vmod_modem *m, const uint8_t *data, size_t len);

/* Format a result code into dst (CRLF wrapped if verbose). Returns length. */
size_t vmod_hayes_format_result(const vmod_hayes_cfg_t *cfg, vmod_result_t code,
                                char *dst, size_t dst_sz);

/* CONNECT with optional speed based on X and modem_speed. */
size_t vmod_hayes_format_connect(const vmod_hayes_cfg_t *cfg, char *dst,
                                 size_t dst_sz);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_HAYES_H */
