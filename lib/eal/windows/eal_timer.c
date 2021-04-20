/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <inttypes.h>

#include <rte_windows.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include "eal_private.h"

#define US_PER_SEC 1E6
#define CYC_PER_10MHZ 1E7

void
rte_delay_us_sleep(unsigned int us)
{
	HANDLE timer;
	LARGE_INTEGER due_time;

	/* create waitable timer */
	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (!timer) {
		RTE_LOG_WIN32_ERR("CreateWaitableTimer()");
		rte_errno = ENOMEM;
		return;
	}

	/*
	 * due_time's uom is 100 ns, multiply by 10 to convert to microseconds
	 * set us microseconds time for timer
	 */
	due_time.QuadPart = -((int64_t)us * 10);
	if (!SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE)) {
		RTE_LOG_WIN32_ERR("SetWaitableTimer()");
		rte_errno = EINVAL;
		goto end;
	}
	/* start wait for timer for us microseconds */
	if (WaitForSingleObject(timer, INFINITE) == WAIT_FAILED) {
		RTE_LOG_WIN32_ERR("WaitForSingleObject()");
		rte_errno = EINVAL;
	}

end:
	CloseHandle(timer);
}

uint64_t
get_tsc_freq(void)
{
	LARGE_INTEGER t_start, t_end, elapsed_us;
	LARGE_INTEGER frequency;
	uint64_t tsc_hz;
	uint64_t end, start;

	QueryPerformanceFrequency(&frequency);

	QueryPerformanceCounter(&t_start);
	start = rte_get_tsc_cycles();

	rte_delay_us_sleep(US_PER_SEC / 10); /* 1/10 second */

	if (rte_errno != 0)
		return 0;

	QueryPerformanceCounter(&t_end);
	end = rte_get_tsc_cycles();

	elapsed_us.QuadPart = t_end.QuadPart - t_start.QuadPart;

	/*
	 * To guard against loss-of-precision, convert to microseconds
	 * *before* dividing by ticks-per-second.
	 */
	elapsed_us.QuadPart *= US_PER_SEC;
	elapsed_us.QuadPart /= frequency.QuadPart;

	double secs = ((double)elapsed_us.QuadPart)/US_PER_SEC;
	tsc_hz = (uint64_t)((end - start)/secs);

	/* Round up to 10Mhz. 1E7 ~ 10Mhz */
	return RTE_ALIGN_MUL_NEAR(tsc_hz, CYC_PER_10MHZ);
}


int
rte_eal_timer_init(void)
{
	set_tsc_freq();
	return 0;
}
