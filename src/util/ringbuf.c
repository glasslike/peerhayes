/*
 * PeerHayes — ring buffer implementation
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/ringbuf.h"

#include <string.h>

int vmod_ringbuf_init(vmod_ringbuf_t *rb, uint8_t *storage, size_t capacity) {
  if (!rb || !storage || capacity == 0)
    return -1;
  rb->data = storage;
  rb->capacity = capacity;
  rb->head = 0;
  rb->tail = 0;
  rb->count = 0;
  return 0;
}

void vmod_ringbuf_reset(vmod_ringbuf_t *rb) {
  if (!rb)
    return;
  rb->head = rb->tail = rb->count = 0;
}

size_t vmod_ringbuf_count(const vmod_ringbuf_t *rb) {
  return rb ? rb->count : 0;
}

size_t vmod_ringbuf_space(const vmod_ringbuf_t *rb) {
  return rb ? (rb->capacity - rb->count) : 0;
}

size_t vmod_ringbuf_write(vmod_ringbuf_t *rb, const uint8_t *src, size_t len) {
  size_t i;
  if (!rb || !src || len == 0)
    return 0;
  for (i = 0; i < len; i++) {
    if (rb->count >= rb->capacity)
      break;
    rb->data[rb->head] = src[i];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
  }
  return i;
}

size_t vmod_ringbuf_read(vmod_ringbuf_t *rb, uint8_t *dst, size_t len) {
  size_t i;
  if (!rb || !dst || len == 0)
    return 0;
  for (i = 0; i < len; i++) {
    if (rb->count == 0)
      break;
    dst[i] = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
  }
  return i;
}

size_t vmod_ringbuf_peek(const vmod_ringbuf_t *rb, uint8_t *dst, size_t len) {
  size_t i, idx;
  if (!rb || !dst || len == 0)
    return 0;
  idx = rb->tail;
  for (i = 0; i < len && i < rb->count; i++) {
    dst[i] = rb->data[idx];
    idx = (idx + 1) % rb->capacity;
  }
  return i;
}

int vmod_ringbuf_putc(vmod_ringbuf_t *rb, uint8_t c) {
  return (int)vmod_ringbuf_write(rb, &c, 1);
}

int vmod_ringbuf_getc(vmod_ringbuf_t *rb) {
  uint8_t c;
  if (vmod_ringbuf_read(rb, &c, 1) != 1)
    return -1;
  return (int)c;
}
