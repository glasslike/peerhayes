/*
 * PeerHayes — stderr logging with timestamps
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_LOG_H
#define VMODEM_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VMOD_LOG_ERROR = 0,
  VMOD_LOG_WARN,
  VMOD_LOG_INFO,
  VMOD_LOG_DEBUG,
  VMOD_LOG_TRACE
} vmod_log_level_t;

void vmod_log_set_level(vmod_log_level_t level);
vmod_log_level_t vmod_log_get_level(void);
void vmod_log(vmod_log_level_t level, const char *fmt, ...);

#define VMOD_LOGE(...) vmod_log(VMOD_LOG_ERROR, __VA_ARGS__)
#define VMOD_LOGW(...) vmod_log(VMOD_LOG_WARN, __VA_ARGS__)
#define VMOD_LOGI(...) vmod_log(VMOD_LOG_INFO, __VA_ARGS__)
#define VMOD_LOGD(...) vmod_log(VMOD_LOG_DEBUG, __VA_ARGS__)
#define VMOD_LOGT(...) vmod_log(VMOD_LOG_TRACE, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_LOG_H */
