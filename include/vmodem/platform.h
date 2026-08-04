/*
 * PeerHayes — portable time helpers
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#ifndef VMODEM_PLATFORM_H
#define VMODEM_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t vmod_now_ms(void);
void vmod_sleep_ms(unsigned ms);

#ifdef __cplusplus
}
#endif

#endif /* VMODEM_PLATFORM_H */
