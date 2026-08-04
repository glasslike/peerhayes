/*
 * PeerHayes — VMCP/1 peer call-control protocol
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 *
 * Before CONNECT the TCP session carries CR-terminated ASCII lines:
 *   VMCP/1 CALL <dialstring>
 *   VMCP/1 RING
 *   VMCP/1 ANSWER
 *   VMCP/1 CONNECT <dce_speed>
 *   VMCP/1 BUSY | NOANSWER | HANGUP | ERROR …
 *
 * After CONNECT the same TCP socket becomes a raw octet stream (no framing).
 * This is what distinguishes PeerHayes from bridges that treat TCP connect
 * itself as carrier.
 */

#ifndef VMODEM_VMCP_H
#define VMODEM_VMCP_H

#include "vmodem/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VMCP_VERSION "VMCP/1"
#define VMCP_LINE_MAX 256

typedef enum {
  VMCP_MSG_CALL = 1,
  VMCP_MSG_RING,
  VMCP_MSG_ANSWER,
  VMCP_MSG_CONNECT,
  VMCP_MSG_BUSY,
  VMCP_MSG_NOANSWER,
  VMCP_MSG_HANGUP,
  VMCP_MSG_ERROR,
  VMCP_MSG_UNKNOWN = 0
} vmcp_msg_type_t;

typedef struct {
  vmcp_msg_type_t type;
  char arg[VMCP_LINE_MAX];
  int speed; /* CONNECT reported DCE speed */
} vmcp_msg_t;

/* Build a CR-terminated control line. Returns bytes written (excluding NUL). */
size_t vmcp_format(vmcp_msg_type_t type, const char *arg, int speed, char *dst,
                   size_t dst_sz);

/* Parse one CR or CRLF terminated line (terminator may be included in len). */
vmod_status_t vmcp_parse(const char *line, size_t len, vmcp_msg_t *out);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_VMCP_H */
