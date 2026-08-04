/*
 * PeerHayes unit test: INI config + static dial directory
 * Copyright (c) 2026 Alexey Matrosov <2:203/910> — MIT
 */

#include "vmodem/config.h"
#include "vmodem/directory.h"
#include "vmodem/types.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  vmod_config_t cfg;
  char ep[128];
  if (vmod_config_load(&cfg, "examples/peerhayes-a.conf") != VMOD_OK &&
      vmod_config_load(&cfg, "../examples/peerhayes-a.conf") != VMOD_OK) {
    fprintf(stderr, "skip: examples/peerhayes-a.conf not found from cwd\n");
    return 0;
  }
  if (cfg.dial_count < 1) {
    fprintf(stderr, "FAIL: no dial entries\n");
    return 1;
  }
  if (strcmp(cfg.dial_entries[0].number, "200") != 0) {
    fprintf(stderr, "FAIL: dial number\n");
    return 1;
  }
  if (vmod_directory_resolve(&cfg.directory, "200", ep, sizeof(ep)) !=
      VMOD_OK) {
    fprintf(stderr, "FAIL: resolve\n");
    return 1;
  }
  printf("test_config: PASS (%s -> %s)\n", cfg.dial_entries[0].number, ep);
  return 0;
}
