/*
 * PeerHayes — simple INI-style configuration loader
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Sections: [modem], [network], [dial] (number = host:port).
 */

#ifndef VMODEM_CONFIG_H
#define VMODEM_CONFIG_H

#include "vmodem/directory.h"
#include "vmodem/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VMOD_CFG_PATH_MAX 256
#define VMOD_CFG_DIAL_MAX 32

typedef struct {
  char serial[VMOD_CFG_PATH_MAX];
  int baudrate;
  int modem_speed;
  int auto_answer; /* S0 rings; 0 = manual ATA */
  char listen[128];
  char name[32];

  vmod_dir_entry_t dial_entries[VMOD_CFG_DIAL_MAX];
  char dial_num_storage[VMOD_CFG_DIAL_MAX][64];
  char dial_ep_storage[VMOD_CFG_DIAL_MAX][128];
  size_t dial_count;
  vmod_directory_t directory;
} vmod_config_t;

void vmod_config_defaults(vmod_config_t *cfg);
vmod_status_t vmod_config_load(vmod_config_t *cfg, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_CONFIG_H */
