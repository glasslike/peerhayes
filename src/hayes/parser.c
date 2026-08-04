/*
 * PeerHayes — AT command line parser (command mode)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/hayes.h"
#include "vmodem/log.h"
#include "vmodem/modem.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char up(char c) {
  return (char)toupper((unsigned char)c);
}

/* Parse command body after "AT". Returns 0 on OK path, -1 on ERROR. */
static int hayes_exec_line(vmod_modem_t *m, const char *line) {
  size_t i = 0;
  size_t len = strlen(line);
  int any = 0;

  VMOD_LOGT("%s AT parse: '%s'", m->name, line);

  while (i < len) {
    char cmd = up(line[i++]);
    int num = 0;
    int has_num = 0;

    /* skip spaces */
    while (i < len && line[i] == ' ')
      i++;

    if (cmd == '&') {
      if (i >= len)
        return -1;
      cmd = (char)(up(line[i++]) | 0x80); /* mark extended */
      while (i < len && isdigit((unsigned char)line[i])) {
        has_num = 1;
        num = num * 10 + (line[i++] - '0');
      }
    } else if (cmd == 'S') {
      int sreg = 0;
      while (i < len && isdigit((unsigned char)line[i])) {
        sreg = sreg * 10 + (line[i++] - '0');
        has_num = 1;
      }
      if (i < len && line[i] == '=') {
        int val = 0;
        i++;
        while (i < len && isdigit((unsigned char)line[i]))
          val = val * 10 + (line[i++] - '0');
        if (sreg >= 0 && sreg < VMOD_SREG_COUNT)
          m->cfg.s[sreg] = (uint8_t)val;
        any = 1;
        continue;
      }
      if (i < len && line[i] == '?') {
        char buf[16];
        i++;
        snprintf(buf, sizeof(buf), "%03d",
                 sreg >= 0 && sreg < VMOD_SREG_COUNT ? m->cfg.s[sreg] : 0);
        vmod_modem_emit_text(m, buf);
        any = 1;
        continue;
      }
      /* bare S n treated as query-ish no-op */
      any = 1;
      (void)has_num;
      continue;
    } else if (cmd == 'D') {
      char dial[VMOD_DIAL_MAX];
      size_t dlen = 0;
      /* optional T/P */
      if (i < len && (up(line[i]) == 'T' || up(line[i]) == 'P'))
        i++;
      if (i < len && up(line[i]) == 'L') {
        i++;
        strncpy(dial, m->cfg.last_dial, sizeof(dial) - 1);
        dial[sizeof(dial) - 1] = '\0';
      } else {
        while (i < len && dlen + 1 < sizeof(dial)) {
          char c = line[i++];
          if (c == ';' )
            break;
          if (c == ' ' || c == '-' || c == '(' || c == ')')
            continue;
          dial[dlen++] = c;
        }
        dial[dlen] = '\0';
      }
      if (dial[0]) {
        strncpy(m->cfg.last_dial, dial, sizeof(m->cfg.last_dial) - 1);
        vmod_modem_begin_dial(m, dial);
        return 0; /* dial consumes rest; CONNECT/NO ANSWER later */
      }
      vmod_modem_emit_result(m, VMOD_RESULT_ERROR);
      return 0;
    } else {
      while (i < len && isdigit((unsigned char)line[i])) {
        has_num = 1;
        num = num * 10 + (line[i++] - '0');
      }
      if (!has_num)
        num = 0;
    }

    if ((cmd & 0x7f) == 'Z' || cmd == 'Z') {
      vmod_modem_reset(m);
      any = 1;
    } else if ((unsigned char)cmd == (unsigned char)('F' | 0x80)) {
      /* &F */
      vmod_hayes_cfg_defaults(&m->cfg);
      any = 1;
    } else if (cmd == 'E') {
      if (num > 1)
        return -1;
      m->cfg.echo = num;
      any = 1;
    } else if (cmd == 'Q') {
      if (num > 1)
        return -1;
      m->cfg.quiet = num;
      any = 1;
    } else if (cmd == 'V') {
      if (num > 1)
        return -1;
      m->cfg.verbose = num;
      any = 1;
    } else if (cmd == 'X') {
      if (num > 4)
        return -1;
      m->cfg.result_level = num;
      any = 1;
    } else if (cmd == 'H') {
      if (num == 0) {
        vmod_modem_hangup(m);
        vmod_modem_emit_result(m, VMOD_RESULT_OK);
      } else if (num == 1) {
        vmod_modem_answer(m);
      } else {
        return -1;
      }
      return 0;
    } else if (cmd == 'A') {
      vmod_modem_answer(m);
      return 0;
    } else if (cmd == 'O') {
      if (m->state == VMOD_STATE_ONLINE_CMD)
        vmod_modem_go_online(m);
      any = 1;
    } else if ((unsigned char)cmd == (unsigned char)('C' | 0x80)) {
      if (num > 1)
        return -1;
      m->cfg.dcd_always_on = (num == 0);
      any = 1;
    } else if ((unsigned char)cmd == (unsigned char)('D' | 0x80)) {
      if (num > 2)
        return -1;
      m->cfg.dtr_mode = num;
      any = 1;
    } else if ((unsigned char)cmd == (unsigned char)('K' | 0x80)) {
      if (num == 0)
        m->cfg.flow_rts = 0;
      else if (num == 3)
        m->cfg.flow_rts = 1;
      /* accept other values as no-ops for compatibility */
      any = 1;
    } else if (cmd == 'I' || cmd == 'L' || cmd == 'M' || cmd == 'N' ||
               cmd == 'P' || cmd == 'T' || cmd == 'W' || cmd == 'Y' ||
               cmd == 'B') {
      /* ignored for MVP compatibility */
      any = 1;
    } else if (cmd == '\0') {
      break;
    } else {
      VMOD_LOGD("%s unknown AT cmd '%c'", m->name, cmd & 0x7f);
      /* ignore unknown single letter for mailer init strings */
      any = 1;
    }
  }

  (void)any;
  vmod_modem_emit_result(m, VMOD_RESULT_OK);
  return 0;
}

static void hayes_on_line(vmod_modem_t *m) {
  m->at_line[m->at_len] = '\0';
  strncpy(m->cfg.last_cmd, m->at_line, sizeof(m->cfg.last_cmd) - 1);
  VMOD_LOGI("%s DTE AT%s", m->name, m->at_line);
  if (hayes_exec_line(m, m->at_line) < 0)
    vmod_modem_emit_result(m, VMOD_RESULT_ERROR);
  m->at_len = 0;
  m->saw_a = 0;
  m->cmd_started = 0;
}

void vmod_hayes_feed(vmod_modem_t *m, const uint8_t *data, size_t len) {
  size_t i;
  uint8_t cr = m->cfg.s[3];
  uint8_t bs = m->cfg.s[5];

  for (i = 0; i < len; i++) {
    uint8_t ch = data[i];
    uint8_t bare = (uint8_t)(ch & 0x7f);

    if (m->cfg.echo) {
      vmod_ringbuf_putc(&m->dte_tx, ch);
      if (m->dte_write)
        m->dte_write(m->dte_user, &ch, 1);
    }

    if (!m->cmd_started) {
      if (!m->saw_a) {
        if (bare == 'A' || bare == 'a')
          m->saw_a = 1;
        continue;
      }
      if (bare == 'T' || bare == 't') {
        m->cmd_started = 1;
        m->at_len = 0;
        continue;
      }
      if (bare == '/') {
        /* A/ repeat */
        m->saw_a = 0;
        strncpy(m->at_line, m->cfg.last_cmd, sizeof(m->at_line) - 1);
        m->at_len = strlen(m->at_line);
        hayes_on_line(m);
        continue;
      }
      if (bare != 'A' && bare != 'a')
        m->saw_a = 0;
      continue;
    }

    if (bare == bs) {
      if (m->at_len > 0)
        m->at_len--;
      continue;
    }
    if (bare == cr) {
      hayes_on_line(m);
      continue;
    }
    if (bare == m->cfg.s[4]) /* ignore LF */
      continue;
    if (m->at_len + 1 < sizeof(m->at_line))
      m->at_line[m->at_len++] = (char)bare;
  }
}
