/*
 * PeerHayes — common status / call-state / Hayes result codes
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * These enums are shared by the AT layer, call state machine, and transports.
 * Numeric Hayes result codes follow the classic verbose/numeric mapping
 * (OK=0, CONNECT=1, RING=2, NO CARRIER=3, ERROR=4, …).
 */

#ifndef VMODEM_TYPES_H
#define VMODEM_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VMOD_OK = 0,
  VMOD_ERR = -1,
  VMOD_ERR_NOMEM = -2,
  VMOD_ERR_INVAL = -3,
  VMOD_ERR_BUSY = -4,
  VMOD_ERR_IO = -5,
  VMOD_ERR_TIMEOUT = -6,
  VMOD_ERR_PROTO = -7
} vmod_status_t;

/* Local modem call state (independent of TCP connected flag). */
typedef enum {
  VMOD_STATE_IDLE = 0,
  VMOD_STATE_DIALING,       /* ATDx accepted; resolving number / connecting */
  VMOD_STATE_CALL_SETUP,    /* TCP up; waiting for peer ANSWER/CONNECT */
  VMOD_STATE_RINGING,       /* inbound CALL; RING cadence to DTE */
  VMOD_STATE_ANSWERING,     /* ATA / S0 in progress */
  VMOD_STATE_ONLINE,        /* CONNECT issued; transparent data */
  VMOD_STATE_ONLINE_CMD,    /* +++ escape; carrier still up */
  VMOD_STATE_DISCONNECTING
} vmod_call_state_t;

/* Classic Hayes numeric result codes (ATV0). */
typedef enum {
  VMOD_RESULT_OK = 0,
  VMOD_RESULT_CONNECT = 1,
  VMOD_RESULT_RING = 2,
  VMOD_RESULT_NO_CARRIER = 3,
  VMOD_RESULT_ERROR = 4,
  VMOD_RESULT_NO_DIALTONE = 6,
  VMOD_RESULT_BUSY = 7,
  VMOD_RESULT_NO_ANSWER = 8
} vmod_result_t;

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_TYPES_H */
