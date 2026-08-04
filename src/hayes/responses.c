/*
 * PeerHayes — Hayes result-code formatting (OK, RING, CONNECT n, …)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/hayes.h"
#include "vmodem/modem.h"

#include <stdio.h>
#include <string.h>

void vmod_hayes_cfg_defaults(vmod_hayes_cfg_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->echo = 1;
  cfg->quiet = 0;
  cfg->verbose = 1;
  cfg->result_level = 4;
  cfg->dcd_always_on = 0;
  cfg->dtr_mode = 2;
  cfg->flow_rts = 1;
  cfg->modem_speed = 14400;

  cfg->s[0] = 0;   /* auto-answer rings */
  cfg->s[2] = 43;  /* '+' */
  cfg->s[3] = 13;  /* CR */
  cfg->s[4] = 10;  /* LF */
  cfg->s[5] = 8;   /* BS */
  cfg->s[7] = 50;  /* wait for carrier, seconds */
  cfg->s[10] = 14; /* lost carrier delay */
  cfg->s[12] = 50; /* escape guard, 1/50 s units */
}

static size_t emit_crlf(const vmod_hayes_cfg_t *cfg, char *dst, size_t dst_sz,
                        size_t pos) {
  if (pos + 2 < dst_sz) {
    dst[pos++] = (char)cfg->s[3];
    dst[pos++] = (char)cfg->s[4];
  }
  return pos;
}

size_t vmod_hayes_format_result(const vmod_hayes_cfg_t *cfg, vmod_result_t code,
                                char *dst, size_t dst_sz) {
  static const char *verbose[] = {
      "OK",          "CONNECT", "RING",   "NO CARRIER", "ERROR", "",
      "NO DIALTONE", "BUSY",    "NO ANSWER"};
  size_t pos = 0;
  char num[16];

  if (!cfg || !dst || dst_sz == 0)
    return 0;
  if (cfg->quiet)
    return 0;

  pos = emit_crlf(cfg, dst, dst_sz, pos);
  if (cfg->verbose) {
    const char *msg = "ERROR";
    if ((int)code >= 0 && (int)code <= 8 && verbose[code][0])
      msg = verbose[code];
    while (*msg && pos + 1 < dst_sz)
      dst[pos++] = *msg++;
  } else {
    snprintf(num, sizeof(num), "%d", (int)code);
    {
      const char *p = num;
      while (*p && pos + 1 < dst_sz)
        dst[pos++] = *p++;
    }
  }
  pos = emit_crlf(cfg, dst, dst_sz, pos);
  if (pos < dst_sz)
    dst[pos] = '\0';
  return pos;
}

size_t vmod_hayes_format_connect(const vmod_hayes_cfg_t *cfg, char *dst,
                                 size_t dst_sz) {
  size_t pos = 0;
  char body[64];

  if (!cfg || !dst || dst_sz == 0)
    return 0;
  if (cfg->quiet)
    return 0;

  if (cfg->result_level == 0 || cfg->modem_speed <= 0)
    snprintf(body, sizeof(body), "CONNECT");
  else
    snprintf(body, sizeof(body), "CONNECT %d", cfg->modem_speed);

  pos = emit_crlf(cfg, dst, dst_sz, pos);
  if (cfg->verbose) {
    const char *p = body;
    while (*p && pos + 1 < dst_sz)
      dst[pos++] = *p++;
  } else {
    /* numeric CONNECT is 1 */
    dst[pos++] = '1';
  }
  pos = emit_crlf(cfg, dst, dst_sz, pos);
  if (pos < dst_sz)
    dst[pos] = '\0';
  return pos;
}
