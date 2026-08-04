/*
 * PeerHayes — VMCP/1 encode/decode (call-control lines before CONNECT)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/vmcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t vmcp_format(vmcp_msg_type_t type, const char *arg, int speed, char *dst,
                   size_t dst_sz) {
  const char *name = "ERROR";
  int n;

  if (!dst || dst_sz < 8)
    return 0;

  switch (type) {
  case VMCP_MSG_CALL:
    name = "CALL";
    break;
  case VMCP_MSG_RING:
    name = "RING";
    break;
  case VMCP_MSG_ANSWER:
    name = "ANSWER";
    break;
  case VMCP_MSG_CONNECT:
    name = "CONNECT";
    break;
  case VMCP_MSG_BUSY:
    name = "BUSY";
    break;
  case VMCP_MSG_NOANSWER:
    name = "NOANSWER";
    break;
  case VMCP_MSG_HANGUP:
    name = "HANGUP";
    break;
  case VMCP_MSG_ERROR:
    name = "ERROR";
    break;
  default:
    break;
  }

  if (type == VMCP_MSG_CONNECT)
    n = snprintf(dst, dst_sz, "%s %s %d\r", VMCP_VERSION, name, speed);
  else if (arg && arg[0])
    n = snprintf(dst, dst_sz, "%s %s %s\r", VMCP_VERSION, name, arg);
  else
    n = snprintf(dst, dst_sz, "%s %s\r", VMCP_VERSION, name);

  if (n < 0 || (size_t)n >= dst_sz)
    return 0;
  return (size_t)n;
}

vmod_status_t vmcp_parse(const char *line, size_t len, vmcp_msg_t *out) {
  char buf[VMCP_LINE_MAX];
  char *p;
  char *tok;
  size_t n = len;

  if (!line || !out)
    return VMOD_ERR_INVAL;
  memset(out, 0, sizeof(*out));

  while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' ||
                   line[n - 1] == ' '))
    n--;
  if (n >= sizeof(buf))
    n = sizeof(buf) - 1;
  memcpy(buf, line, n);
  buf[n] = '\0';

  p = buf;
  tok = strtok(p, " ");
  if (!tok || strcmp(tok, VMCP_VERSION) != 0)
    return VMOD_ERR_PROTO;

  tok = strtok(NULL, " ");
  if (!tok)
    return VMOD_ERR_PROTO;

  if (strcmp(tok, "CALL") == 0)
    out->type = VMCP_MSG_CALL;
  else if (strcmp(tok, "RING") == 0)
    out->type = VMCP_MSG_RING;
  else if (strcmp(tok, "ANSWER") == 0)
    out->type = VMCP_MSG_ANSWER;
  else if (strcmp(tok, "CONNECT") == 0)
    out->type = VMCP_MSG_CONNECT;
  else if (strcmp(tok, "BUSY") == 0)
    out->type = VMCP_MSG_BUSY;
  else if (strcmp(tok, "NOANSWER") == 0)
    out->type = VMCP_MSG_NOANSWER;
  else if (strcmp(tok, "HANGUP") == 0)
    out->type = VMCP_MSG_HANGUP;
  else if (strcmp(tok, "ERROR") == 0)
    out->type = VMCP_MSG_ERROR;
  else {
    out->type = VMCP_MSG_UNKNOWN;
    return VMOD_ERR_PROTO;
  }

  tok = strtok(NULL, "");
  if (tok) {
    while (*tok == ' ')
      tok++;
    if (out->type == VMCP_MSG_CONNECT)
      out->speed = atoi(tok);
    else {
      strncpy(out->arg, tok, sizeof(out->arg) - 1);
    }
  }
  return VMOD_OK;
}
