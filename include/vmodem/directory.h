/*
 * PeerHayes — static phone-number → endpoint directory
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_DIRECTORY_H
#define VMODEM_DIRECTORY_H

#include "vmodem/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *number;
  const char *endpoint;
} vmod_dir_entry_t;

typedef struct {
  const vmod_dir_entry_t *entries;
  size_t count;
} vmod_directory_t;

/* Resolve dial string to endpoint. Returns VMOD_OK and writes into out_ep. */
vmod_status_t vmod_directory_resolve(const vmod_directory_t *dir,
                                     const char *number, char *out_ep,
                                     size_t out_ep_sz);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_DIRECTORY_H */
