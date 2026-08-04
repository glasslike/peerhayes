/*
 * PeerHayes — fixed-capacity byte ring buffer
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_RINGBUF_H
#define VMODEM_RINGBUF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t *data;
  size_t capacity;
  size_t head;
  size_t tail;
  size_t count;
} vmod_ringbuf_t;

int vmod_ringbuf_init(vmod_ringbuf_t *rb, uint8_t *storage, size_t capacity);
void vmod_ringbuf_reset(vmod_ringbuf_t *rb);
size_t vmod_ringbuf_count(const vmod_ringbuf_t *rb);
size_t vmod_ringbuf_space(const vmod_ringbuf_t *rb);
size_t vmod_ringbuf_write(vmod_ringbuf_t *rb, const uint8_t *src, size_t len);
size_t vmod_ringbuf_read(vmod_ringbuf_t *rb, uint8_t *dst, size_t len);
size_t vmod_ringbuf_peek(const vmod_ringbuf_t *rb, uint8_t *dst, size_t len);
int vmod_ringbuf_putc(vmod_ringbuf_t *rb, uint8_t c);
int vmod_ringbuf_getc(vmod_ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_RINGBUF_H */
