/*
 * PeerHayes — transport abstraction (TCP today; loopback for tests)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * A transport moves bytes between peer modems. During control_phase==1 the
 * payload is VMCP/1 text lines; after CONNECT it is an unmodified byte stream.
 */

#ifndef VMODEM_TRANSPORT_H
#define VMODEM_TRANSPORT_H

#include "vmodem/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vmod_modem;
typedef struct vmod_transport vmod_transport_t;

typedef struct vmod_transport_ops {
  vmod_status_t (*listen)(vmod_transport_t *t, const char *bind_ep);
  vmod_status_t (*dial)(vmod_transport_t *t, const char *endpoint);
  vmod_status_t (*send)(vmod_transport_t *t, const uint8_t *data, size_t len);
  void (*close)(vmod_transport_t *t);
  void (*poll)(vmod_transport_t *t);
} vmod_transport_ops_t;

struct vmod_transport {
  const vmod_transport_ops_t *ops;
  struct vmod_modem *modem;
  void *impl;
  int connected;
  int control_phase; /* 1 while VMCP, 0 after CONNECT (raw) */
};

static inline vmod_status_t vmod_transport_listen(vmod_transport_t *t,
                                                  const char *bind_ep) {
  return t && t->ops && t->ops->listen ? t->ops->listen(t, bind_ep) : VMOD_ERR;
}

static inline vmod_status_t vmod_transport_dial(vmod_transport_t *t,
                                                const char *endpoint) {
  return t && t->ops && t->ops->dial ? t->ops->dial(t, endpoint) : VMOD_ERR;
}

static inline vmod_status_t vmod_transport_send(vmod_transport_t *t,
                                                const uint8_t *data,
                                                size_t len) {
  return t && t->ops && t->ops->send ? t->ops->send(t, data, len) : VMOD_ERR;
}

static inline void vmod_transport_close(vmod_transport_t *t) {
  if (t && t->ops && t->ops->close)
    t->ops->close(t);
}

static inline void vmod_transport_poll(vmod_transport_t *t) {
  if (t && t->ops && t->ops->poll)
    t->ops->poll(t);
}

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_TRANSPORT_H */
