/*
 * PeerHayes — logging (HH:MM:SS.mmm [LEVEL] message)
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/log.h"
#include "vmodem/platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static vmod_log_level_t g_level = VMOD_LOG_INFO;

void vmod_log_set_level(vmod_log_level_t level) { g_level = level; }

vmod_log_level_t vmod_log_get_level(void) { return g_level; }

static void log_timestamp(FILE *fp) {
#ifdef _WIN32
  SYSTEMTIME st;
  GetLocalTime(&st);
  fprintf(fp, "%02u:%02u:%02u.%03u ", (unsigned)st.wHour, (unsigned)st.wMinute,
          (unsigned)st.wSecond, (unsigned)st.wMilliseconds);
#else
  struct timespec ts;
  struct tm tm;
  clock_gettime(CLOCK_REALTIME, &ts);
  localtime_r(&ts.tv_sec, &tm);
  fprintf(fp, "%02d:%02d:%02d.%03ld ", tm.tm_hour, tm.tm_min, tm.tm_sec,
          ts.tv_nsec / 1000000L);
#endif
}

void vmod_log(vmod_log_level_t level, const char *fmt, ...) {
  static const char *tags[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
  va_list ap;
  if (level > g_level)
    return;
  log_timestamp(stderr);
  fprintf(stderr, "[%s] ", tags[level <= VMOD_LOG_TRACE ? level : VMOD_LOG_TRACE]);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}
