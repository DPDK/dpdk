/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ntlog.h"
#include "nt_util.h"

/* uses usleep which schedules out the calling thread */
void nt_os_wait_usec(int val)
{
	rte_delay_us_sleep(val);
}

uint64_t nt_os_get_time_monotonic_counter(void)
{
	return rte_get_timer_cycles();
}

/* Allocation size matching minimum alignment of specified size */
uint64_t nt_util_align_size(uint64_t size)
{
	return 1 << rte_log2_u64(size);
}
