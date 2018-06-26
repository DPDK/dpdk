/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef EAL_ALARM_PRIVATE_H
#define EAL_ALARM_PRIVATE_H

#include <inttypes.h>

/*
 * FreeBSD needs a back-channel communication mechanism between interrupt and
 * alarm thread, because on FreeBSD, timer period is set up inside the interrupt
 * API and not inside alarm API like on Linux.
 */

int
eal_alarm_get_timeout_ns(uint64_t *val);

#endif // EAL_ALARM_PRIVATE_H
