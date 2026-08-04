#include "vmodem/hayes.h"
#include "vmodem/modem.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    g_fail++;
  }
}

static void capture(void *user, const uint8_t *data, size_t len) {
  char *buf = (char *)user;
  size_t n = strlen(buf);
  size_t i;
  for (i = 0; i < len && n + 1 < 512; i++)
    buf[n++] = (char)data[i];
  buf[n] = '\0';
}

int main(void) {
  vmod_modem_t m;
  char out[512];
  char fmt[64];
  vmod_hayes_cfg_t cfg;

  g_fail = 0;
  vmod_hayes_cfg_defaults(&cfg);
  expect(cfg.echo == 1, "default echo on");
  expect(cfg.s[7] == 50, "default S7");

  expect(vmod_hayes_format_result(&cfg, VMOD_RESULT_OK, fmt, sizeof(fmt)) > 0,
         "format OK");
  expect(strstr(fmt, "OK") != NULL, "OK text");

  cfg.modem_speed = 14400;
  cfg.result_level = 4;
  vmod_hayes_format_connect(&cfg, fmt, sizeof(fmt));
  expect(strstr(fmt, "CONNECT 14400") != NULL, "CONNECT speed");

  vmod_modem_init(&m, "T");
  m.cfg.echo = 0;
  out[0] = '\0';
  vmod_modem_set_dte_writer(&m, capture, out);
  vmod_modem_dte_input(&m, (const uint8_t *)"AT\r", 3);
  expect(strstr(out, "OK") != NULL, "AT -> OK");

  out[0] = '\0';
  vmod_modem_dte_input(&m, (const uint8_t *)"ATE0\r", 5);
  expect(strstr(out, "OK") != NULL, "ATE0 -> OK");
  expect(m.cfg.echo == 0, "echo off");

  out[0] = '\0';
  vmod_modem_dte_input(&m, (const uint8_t *)"ATS0=1\r", 7);
  expect(m.cfg.s[0] == 1, "S0=1");
  expect(strstr(out, "OK") != NULL, "ATS0=1 OK");

  out[0] = '\0';
  vmod_modem_dte_input(&m, (const uint8_t *)"AT&C1&D2\r", 9);
  expect(m.cfg.dcd_always_on == 0, "&C1");
  expect(m.cfg.dtr_mode == 2, "&D2");

  if (g_fail) {
    fprintf(stderr, "%d hayes tests failed\n", g_fail);
    return 1;
  }
  printf("test_hayes: PASS\n");
  return 0;
}
