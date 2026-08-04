/*
 * PeerHayes — DTE serial / pipe / stdio backend
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Opens the port that faces the legacy application. On Win32 COM ports,
 * DTR is driven to emulate carrier (com0com maps DTR → peer DCD).
 */

#ifndef VMODEM_SERIAL_H
#define VMODEM_SERIAL_H

#include "vmodem/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmod_serial vmod_serial_t;

typedef enum {
  VMOD_SERIAL_COM = 0,
  VMOD_SERIAL_PIPE,
  VMOD_SERIAL_STDIO
} vmod_serial_kind_t;

vmod_status_t vmod_serial_open(vmod_serial_t **out, const char *path,
                               int baudrate);
void vmod_serial_close(vmod_serial_t *s);

/* Non-blocking-ish read: returns bytes read (0 = none). */
int vmod_serial_read(vmod_serial_t *s, uint8_t *buf, size_t buflen);

/* Blocking-enough write of all bytes; returns 0 on success. */
int vmod_serial_write(vmod_serial_t *s, const uint8_t *buf, size_t len);

vmod_serial_kind_t vmod_serial_kind(const vmod_serial_t *s);

/* Drive DTR on our port. With com0com null-modem, peer sees this as DCD/DSR. */
void vmod_serial_set_dtr(vmod_serial_t *s, int on);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_SERIAL_H */
