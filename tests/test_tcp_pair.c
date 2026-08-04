#include "vmodem/directory.h"
#include "vmodem/log.h"
#include "vmodem/modem.h"
#include "vmodem/platform.h"
#include "vmodem/tcp.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

static int g_fail;

static void expect(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    g_fail++;
  }
}

typedef struct {
  char buf[2048];
  size_t len;
} cap_t;

static void capture(void *user, const uint8_t *data, size_t len) {
  cap_t *c = (cap_t *)user;
  size_t i;
  for (i = 0; i < len && c->len + 1 < sizeof(c->buf); i++)
    c->buf[c->len++] = (char)data[i];
  c->buf[c->len] = '\0';
}

static int contains(const cap_t *c, const char *s) {
  return c->buf[0] && strstr(c->buf, s) != NULL;
}

int main(void) {
  vmod_modem_t a, b;
  vmod_transport_t *ta = NULL;
  vmod_transport_t *tb = NULL;
  cap_t ca, cb;
  static const vmod_dir_entry_t entries[] = {{"200", "127.0.0.1:18002"}};
  vmod_directory_t dir = {entries, 1};
  const uint8_t bin[] = {0x11, 0x22, 0x33, 0x7e, 0xff};
  uint64_t start;
  int i;

  g_fail = 0;
  memset(&ca, 0, sizeof(ca));
  memset(&cb, 0, sizeof(cb));
  vmod_log_set_level(VMOD_LOG_WARN);

  if (vmod_net_init() != VMOD_OK) {
    fprintf(stderr, "FAIL: net init\n");
    return 1;
  }

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

  expect(vmod_tcp_transport_create(&ta) == VMOD_OK, "create A tcp");
  expect(vmod_tcp_transport_create(&tb) == VMOD_OK, "create B tcp");
  vmod_modem_set_transport(&a, ta);
  vmod_modem_set_transport(&b, tb);

  expect(vmod_transport_listen(tb, "127.0.0.1:18002") == VMOD_OK, "B listen");
  expect(vmod_transport_listen(ta, "127.0.0.1:18001") == VMOD_OK, "A listen");

  vmod_modem_dte_input(&a, (const uint8_t *)"ATDT200\r", 8);

  start = vmod_now_ms();
  while (vmod_now_ms() - start < 2000) {
    vmod_transport_poll(ta);
    vmod_transport_poll(tb);
    vmod_modem_tick(&a, vmod_now_ms());
    vmod_modem_tick(&b, vmod_now_ms());
    if (contains(&cb, "RING"))
      break;
    vmod_sleep_ms(10);
  }
  expect(contains(&cb, "RING"), "B RING");

  ca.len = 0;
  ca.buf[0] = 0;
  cb.len = 0;
  cb.buf[0] = 0;
  vmod_modem_dte_input(&b, (const uint8_t *)"ATA\r", 4);

  start = vmod_now_ms();
  while (vmod_now_ms() - start < 2000) {
    vmod_transport_poll(ta);
    vmod_transport_poll(tb);
    if (a.state == VMOD_STATE_ONLINE && b.state == VMOD_STATE_ONLINE)
      break;
    vmod_sleep_ms(10);
  }
  expect(contains(&ca, "CONNECT"), "A CONNECT");
  expect(contains(&cb, "CONNECT"), "B CONNECT");
  expect(a.state == VMOD_STATE_ONLINE, "A ONLINE");
  expect(b.state == VMOD_STATE_ONLINE, "B ONLINE");

  ca.len = 0;
  ca.buf[0] = 0;
  cb.len = 0;
  cb.buf[0] = 0;
  vmod_modem_dte_input(&a, bin, sizeof(bin));
  for (i = 0; i < 50; i++) {
    vmod_transport_poll(ta);
    vmod_transport_poll(tb);
    if (cb.len >= sizeof(bin))
      break;
    vmod_sleep_ms(10);
  }
  expect(cb.len == sizeof(bin), "binary len");
  expect(memcmp(cb.buf, bin, sizeof(bin)) == 0, "binary match");

  vmod_tcp_transport_destroy(ta);
  vmod_tcp_transport_destroy(tb);
  vmod_net_shutdown();

  if (g_fail) {
    fprintf(stderr, "%d tcp pair tests failed\n", g_fail);
    return 1;
  }
  printf("test_tcp_pair: PASS\n");
  return 0;
}
