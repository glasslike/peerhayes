#include "vmodem/directory.h"
#include "vmodem/loopback.h"
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

typedef struct {
  char buf[1024];
} cap_t;

static void capture(void *user, const uint8_t *data, size_t len) {
  cap_t *c = (cap_t *)user;
  size_t n = strlen(c->buf);
  size_t i;
  for (i = 0; i < len && n + 1 < sizeof(c->buf); i++)
    c->buf[n++] = (char)data[i];
  c->buf[n] = '\0';
}

int main(void) {
  vmod_modem_t a, b;
  vmod_loopback_pair_t pair;
  cap_t ca, cb;
  static const vmod_dir_entry_t entries[] = {{"200", "loopback:b"}};
  vmod_directory_t dir = {entries, 1};
  const uint8_t bin[] = {1, 2, 3, 4, 5, 0x7e, 0x7f, 0xff};

  g_fail = 0;
  memset(&ca, 0, sizeof(ca));
  memset(&cb, 0, sizeof(cb));

  vmod_modem_init(&a, "A");
  vmod_modem_init(&b, "B");
  a.cfg.echo = 0;
  b.cfg.echo = 0;
  a.cfg.modem_speed = 9600;
  b.cfg.modem_speed = 9600;
  vmod_modem_set_dte_writer(&a, capture, &ca);
  vmod_modem_set_dte_writer(&b, capture, &cb);
  vmod_modem_set_directory(&a, &dir);
  vmod_modem_set_directory(&b, &dir);
  vmod_loopback_init(&pair, &a, &b);

  vmod_modem_dte_input(&a, (const uint8_t *)"ATDT200\r", 8);
  vmod_loopback_poll(&pair);
  expect(strstr(cb.buf, "RING") != NULL, "B RING");
  expect(a.state == VMOD_STATE_CALL_SETUP, "A CALL_SETUP");

  ca.buf[0] = 0;
  cb.buf[0] = 0;
  vmod_modem_dte_input(&b, (const uint8_t *)"ATA\r", 4);
  vmod_loopback_poll(&pair);

  expect(strstr(ca.buf, "CONNECT") != NULL, "A CONNECT");
  expect(strstr(cb.buf, "CONNECT") != NULL, "B CONNECT");
  expect(a.state == VMOD_STATE_ONLINE, "A ONLINE");
  expect(b.state == VMOD_STATE_ONLINE, "B ONLINE");

  ca.buf[0] = 0;
  cb.buf[0] = 0;
  vmod_modem_dte_input(&a, bin, sizeof(bin));
  vmod_loopback_poll(&pair);
  expect(strlen(cb.buf) == sizeof(bin), "binary length");
  expect(memcmp(cb.buf, bin, sizeof(bin)) == 0, "binary match");

  if (g_fail) {
    fprintf(stderr, "%d loopback tests failed\n", g_fail);
    return 1;
  }
  printf("test_loopback: PASS\n");
  return 0;
}
