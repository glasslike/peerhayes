/*
 * PeerHayes — interactive in-process loopback demo (two virtual modems)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "peerhayes/version.h"
#include "vmodem/directory.h"
#include "vmodem/log.h"
#include "vmodem/loopback.h"
#include "vmodem/modem.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  char buf[4096];
  size_t len;
} dte_cap_t;

static void capture_write(void *user, const uint8_t *data, size_t len) {
  dte_cap_t *c = (dte_cap_t *)user;
  size_t i;
  for (i = 0; i < len && c->len + 1 < sizeof(c->buf); i++)
    c->buf[c->len++] = (char)data[i];
  c->buf[c->len] = '\0';
}

static void dte_send(vmod_modem_t *m, const char *s) {
  vmod_modem_dte_input(m, (const uint8_t *)s, strlen(s));
}

static int contains(const char *hay, const char *needle) {
  return hay && needle && strstr(hay, needle) != NULL;
}

int main(void) {
  vmod_modem_t modem_a, modem_b;
  vmod_loopback_pair_t pair;
  dte_cap_t cap_a, cap_b;
  static const vmod_dir_entry_t entries[] = {
      {"200", "loopback:b"},
      {"100", "loopback:a"},
  };
  vmod_directory_t dir = {entries, 2};
  const uint8_t payload[] = {0x00, 0x01, 0xff, 'Z', 'M', 0x18};

  memset(&cap_a, 0, sizeof(cap_a));
  memset(&cap_b, 0, sizeof(cap_b));

  vmod_log_set_level(VMOD_LOG_DEBUG);

  vmod_modem_init(&modem_a, "A");
  vmod_modem_init(&modem_b, "B");
  modem_a.cfg.modem_speed = 9600;
  modem_b.cfg.modem_speed = 9600;
  modem_a.cfg.echo = 0;
  modem_b.cfg.echo = 0;

  vmod_modem_set_dte_writer(&modem_a, capture_write, &cap_a);
  vmod_modem_set_dte_writer(&modem_b, capture_write, &cap_b);
  vmod_modem_set_directory(&modem_a, &dir);
  vmod_modem_set_directory(&modem_b, &dir);

  vmod_loopback_init(&pair, &modem_a, &modem_b);

  printf("=== %s %s loopback demo ===\n", PEERHAYES_NAME,
         PEERHAYES_VERSION_STRING);

  dte_send(&modem_a, "ATZ\r");
  dte_send(&modem_b, "ATZ\r");
  printf("A after ATZ: %s\n", cap_a.buf);
  printf("B after ATZ: %s\n", cap_b.buf);

  cap_a.len = 0;
  cap_a.buf[0] = 0;
  cap_b.len = 0;
  cap_b.buf[0] = 0;

  printf("\nA: ATDT200\n");
  dte_send(&modem_a, "ATDT200\r");
  vmod_loopback_poll(&pair);

  printf("B DTE:\n%s\n", cap_b.buf);
  if (!contains(cap_b.buf, "RING")) {
    fprintf(stderr, "FAIL: B did not see RING\n");
    return 1;
  }

  printf("B: ATA\n");
  dte_send(&modem_b, "ATA\r");
  vmod_loopback_poll(&pair);

  printf("A DTE:\n%s\n", cap_a.buf);
  printf("B DTE:\n%s\n", cap_b.buf);

  if (!contains(cap_a.buf, "CONNECT")) {
    fprintf(stderr, "FAIL: A did not see CONNECT\n");
    return 1;
  }
  if (!contains(cap_b.buf, "CONNECT")) {
    fprintf(stderr, "FAIL: B did not see CONNECT\n");
    return 1;
  }

  cap_a.len = 0;
  cap_a.buf[0] = 0;
  cap_b.len = 0;
  cap_b.buf[0] = 0;

  printf("\nOnline binary transfer (%zu bytes)\n", sizeof(payload));
  vmod_modem_dte_input(&modem_a, payload, sizeof(payload));
  vmod_loopback_poll(&pair);

  if (cap_b.len != sizeof(payload) ||
      memcmp(cap_b.buf, payload, sizeof(payload)) != 0) {
    fprintf(stderr, "FAIL: binary payload mismatch (got %zu bytes)\n",
            cap_b.len);
    return 1;
  }
  printf("B received payload OK\n");

  cap_a.len = 0;
  cap_a.buf[0] = 0;
  cap_b.len = 0;
  cap_b.buf[0] = 0;

  printf("\nA: ATH\n");
  /* escape then hangup — Phase1 simplified: force cmd mode */
  modem_a.cmd_mode = 1;
  modem_a.state = VMOD_STATE_ONLINE_CMD;
  dte_send(&modem_a, "ATH\r");
  vmod_loopback_poll(&pair);
  printf("A DTE:\n%s\n", cap_a.buf);
  printf("B DTE:\n%s\n", cap_b.buf);

  printf("\nPASS: loopback call setup + binary OK\n");
  return 0;
}
