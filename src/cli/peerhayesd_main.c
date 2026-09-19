/*
 * PeerHayes daemon (peerhayesd) — main event loop
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Owns one virtual modem instance: DTE (serial/pipe/stdio) + TCP peer
 * transport. Raises DTR only while ONLINE so com0com peers see DCD follow
 * carrier (AT&C1 behaviour).
 */

#include "peerhayes/version.h"
#include "vmodem/config.h"
#include "vmodem/log.h"
#include "vmodem/modem.h"
#include "vmodem/platform.h"
#include "vmodem/serial.h"
#include "vmodem/tcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static volatile int g_run = 1;
static BOOL WINAPI console_ctrl(DWORD type) {
  (void)type;
  g_run = 0;
  return TRUE;
}
#else
#include <signal.h>
static volatile int g_run = 1;
static void on_sig(int sig) {
  (void)sig;
  g_run = 0;
}
#endif

typedef struct {
  vmod_serial_t *serial;
  vmod_modem_t *modem;
} dte_ctx_t;

static void dte_write_cb(void *user, const uint8_t *data, size_t len) {
  dte_ctx_t *ctx = (dte_ctx_t *)user;
  if (ctx && ctx->serial)
    vmod_serial_write(ctx->serial, data, len);
}

/*
 * Map modem call state to RS-232 carrier indication.
 * On a com0com null-modem pair, our DTR appears as DCD/DSR on the mailer side.
 */
static void on_modem_state(void *user, vmod_call_state_t old_st,
                           vmod_call_state_t new_st) {
  dte_ctx_t *ctx = (dte_ctx_t *)user;
  int carrier;
  (void)old_st;
  if (!ctx || !ctx->serial || !ctx->modem)
    return;
  carrier = (new_st == VMOD_STATE_ONLINE || new_st == VMOD_STATE_ONLINE_CMD);
  if (ctx->modem->cfg.dcd_always_on)
    carrier = 1;
  vmod_serial_set_dtr(ctx->serial, carrier);
}

static void print_version(void) {
  /* Keep --version compact: name/version, one-line description, copyright. */
  printf("%s %s (%s)\n", PEERHAYES_NAME, PEERHAYES_VERSION_STRING,
         PEERHAYES_DAEMON_NAME);
  printf("%s\n\n", PEERHAYES_DESCRIPTION);
  printf("%s\n", PEERHAYES_COPYRIGHT);
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "%s %s — %s\n"
          "Usage: %s --config <file> [--debug|--trace] [--name NAME]\n"
          "       %s --version\n"
          "       %s -c <file>\n",
          PEERHAYES_NAME, PEERHAYES_VERSION_STRING, PEERHAYES_DAEMON_NAME,
          argv0, argv0, argv0);
}

int main(int argc, char **argv) {
  const char *config_path = NULL;
  const char *name_override = NULL;
  vmod_log_level_t log_level = VMOD_LOG_INFO;
  vmod_config_t cfg;
  vmod_modem_t modem;
  vmod_transport_t *tcp = NULL;
  vmod_serial_t *serial = NULL;
  dte_ctx_t dte;
  int i;

  for (i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "--config") == 0 || strcmp(argv[i], "-c") == 0) &&
        i + 1 < argc) {
      config_path = argv[++i];
    } else if (strcmp(argv[i], "--debug") == 0) {
      log_level = VMOD_LOG_DEBUG;
    } else if (strcmp(argv[i], "--trace") == 0) {
      log_level = VMOD_LOG_TRACE;
    } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
      name_override = argv[++i];
    } else if (strcmp(argv[i], "--version") == 0 ||
               strcmp(argv[i], "-V") == 0) {
      print_version();
      return 0;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown arg: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  if (!config_path) {
    usage(argv[0]);
    return 1;
  }

  vmod_log_set_level(log_level);
  VMOD_LOGI("%s %s starting", PEERHAYES_NAME, PEERHAYES_VERSION_STRING);

  if (vmod_config_load(&cfg, config_path) != VMOD_OK)
    return 1;
  if (name_override)
    snprintf(cfg.name, sizeof(cfg.name), "%s", name_override);

#ifdef _WIN32
  SetConsoleCtrlHandler(console_ctrl, TRUE);
#else
  signal(SIGINT, on_sig);
  signal(SIGTERM, on_sig);
#endif

  if (vmod_net_init() != VMOD_OK) {
    VMOD_LOGE("network init failed");
    return 1;
  }

  vmod_modem_init(&modem, cfg.name);
  modem.cfg.modem_speed = cfg.modem_speed;
  modem.cfg.echo = 1;
  if (cfg.auto_answer > 0 && cfg.auto_answer < 255)
    modem.cfg.s[0] = (uint8_t)cfg.auto_answer;
  vmod_modem_set_directory(&modem, &cfg.directory);

  if (vmod_tcp_transport_create(&tcp) != VMOD_OK) {
    VMOD_LOGE("TCP transport create failed");
    return 1;
  }
  vmod_modem_set_transport(&modem, tcp);

  if (vmod_transport_listen(tcp, cfg.listen) != VMOD_OK) {
    VMOD_LOGE("listen failed: %s", cfg.listen);
    vmod_tcp_transport_destroy(tcp);
    return 1;
  }

  if (vmod_serial_open(&serial, cfg.serial, cfg.baudrate) != VMOD_OK) {
    vmod_tcp_transport_destroy(tcp);
    return 1;
  }
  dte.serial = serial;
  dte.modem = &modem;
  vmod_modem_set_dte_writer(&modem, dte_write_cb, &dte);
  vmod_modem_set_state_cb(&modem, on_modem_state, &dte);
  /* No carrier until CONNECT (&C1) */
  vmod_serial_set_dtr(serial, modem.cfg.dcd_always_on ? 1 : 0);

  VMOD_LOGI("%s ready (DTE=%s listen=%s)", cfg.name, cfg.serial, cfg.listen);
  if (vmod_serial_kind(serial) == VMOD_SERIAL_STDIO) {
    fprintf(stderr,
            "stdio DTE: type AT commands ending with Enter. Example: ATZ\n");
  }

  while (g_run) {
    uint8_t buf[512];
    int n = vmod_serial_read(serial, buf, sizeof(buf));
    if (n < 0) {
      VMOD_LOGW("DTE read error / pipe closed");
      break;
    }
    if (n > 0)
      vmod_modem_dte_input(&modem, buf, (size_t)n);

    vmod_transport_poll(tcp);
    vmod_modem_tick(&modem, vmod_now_ms());
    vmod_sleep_ms(10);
  }

  VMOD_LOGI("shutting down");
  vmod_modem_hangup(&modem);
  vmod_serial_close(serial);
  vmod_tcp_transport_destroy(tcp);
  vmod_net_shutdown();
  return 0;
}
