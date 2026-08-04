/*
 * PeerHayes — INI configuration loader ([modem], [network], [dial])
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/config.h"
#include "vmodem/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vmod_config_defaults(vmod_config_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  snprintf(cfg->serial, sizeof(cfg->serial), "stdio");
  cfg->baudrate = 57600;
  cfg->modem_speed = 14400;
  cfg->auto_answer = 0;
  snprintf(cfg->listen, sizeof(cfg->listen), "0.0.0.0:5000");
  snprintf(cfg->name, sizeof(cfg->name), "vmodem");
  cfg->directory.entries = cfg->dial_entries;
  cfg->directory.count = 0;
}

static char *trim(char *s) {
  char *e;
  while (*s && isspace((unsigned char)*s))
    s++;
  if (!*s)
    return s;
  e = s + strlen(s) - 1;
  while (e > s && isspace((unsigned char)*e))
    *e-- = '\0';
  return s;
}

static int add_dial(vmod_config_t *cfg, const char *num, const char *ep) {
  size_t i;
  if (cfg->dial_count >= VMOD_CFG_DIAL_MAX)
    return -1;
  i = cfg->dial_count++;
  snprintf(cfg->dial_num_storage[i], sizeof(cfg->dial_num_storage[i]), "%s",
           num);
  snprintf(cfg->dial_ep_storage[i], sizeof(cfg->dial_ep_storage[i]), "%s", ep);
  cfg->dial_entries[i].number = cfg->dial_num_storage[i];
  cfg->dial_entries[i].endpoint = cfg->dial_ep_storage[i];
  cfg->directory.entries = cfg->dial_entries;
  cfg->directory.count = cfg->dial_count;
  return 0;
}

vmod_status_t vmod_config_load(vmod_config_t *cfg, const char *path) {
  FILE *fp;
  char line[512];
  char section[64] = "";

  if (!cfg || !path)
    return VMOD_ERR_INVAL;
  vmod_config_defaults(cfg);

  fp = fopen(path, "r");
  if (!fp) {
    VMOD_LOGE("cannot open config: %s", path);
    return VMOD_ERR_IO;
  }

  while (fgets(line, sizeof(line), fp)) {
    char *p = trim(line);
    char *eq;
    if (!*p || *p == '#' || *p == ';')
      continue;
    if (*p == '[') {
      char *end = strchr(p, ']');
      if (!end)
        continue;
      *end = '\0';
      snprintf(section, sizeof(section), "%s", p + 1);
      continue;
    }
    eq = strchr(p, '=');
    if (!eq)
      continue;
    *eq = '\0';
    {
      char *key = trim(p);
      char *val = trim(eq + 1);
      if (strcmp(section, "modem") == 0) {
        if (strcmp(key, "serial") == 0)
          snprintf(cfg->serial, sizeof(cfg->serial), "%s", val);
        else if (strcmp(key, "baudrate") == 0)
          cfg->baudrate = atoi(val);
        else if (strcmp(key, "modem_speed") == 0)
          cfg->modem_speed = atoi(val);
        else if (strcmp(key, "auto_answer") == 0 || strcmp(key, "s0") == 0)
          cfg->auto_answer = atoi(val);
        else if (strcmp(key, "name") == 0)
          snprintf(cfg->name, sizeof(cfg->name), "%s", val);
      } else if (strcmp(section, "network") == 0) {
        if (strcmp(key, "listen") == 0)
          snprintf(cfg->listen, sizeof(cfg->listen), "%s", val);
      } else if (strcmp(section, "dial") == 0) {
        add_dial(cfg, key, val);
      }
    }
  }
  fclose(fp);
  VMOD_LOGI("config loaded: %s (listen=%s serial=%s dials=%zu)", path,
            cfg->listen, cfg->serial, cfg->dial_count);
  return VMOD_OK;
}
