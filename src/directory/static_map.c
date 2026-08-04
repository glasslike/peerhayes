/*
 * PeerHayes — resolve dial strings via a static number→endpoint table
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/directory.h"

#include <stdio.h>
#include <string.h>

vmod_status_t vmod_directory_resolve(const vmod_directory_t *dir,
                                     const char *number, char *out_ep,
                                     size_t out_ep_sz) {
  size_t i;
  if (!dir || !number || !out_ep || out_ep_sz == 0)
    return VMOD_ERR_INVAL;

  /* Direct endpoint: host:port or contains a letter / colon */
  if (strchr(number, ':') || strchr(number, '.')) {
    snprintf(out_ep, out_ep_sz, "%s", number);
    return VMOD_OK;
  }

  for (i = 0; i < dir->count; i++) {
    if (dir->entries[i].number &&
        strcmp(dir->entries[i].number, number) == 0) {
      snprintf(out_ep, out_ep_sz, "%s", dir->entries[i].endpoint);
      return VMOD_OK;
    }
  }
  return VMOD_ERR;
}
