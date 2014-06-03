/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_cycles.h>

#include "test.h"

#define N 10000

/*
 * Cycles test
 * ===========
 *
 * - Loop N times and check that the timer always increments and
 *   never decrements during this loop.
 *
 * - Wait one second using rte_usleep() and check that the increment
 *   of cycles is correct with regard to the frequency of the timer.
 */

int
test_cycles(void)
{
	unsigned i;
	uint64_t start_cycles, cycles, prev_cycles;
	uint64_t hz = rte_get_timer_hz();
	uint64_t max_inc = (hz / 100); /* 10 ms max between 2 reads */

	/* check that the timer is always incrementing */
	start_cycles = rte_get_timer_cycles();
	prev_cycles = start_cycles;
	for (i=0; i<N; i++) {
		cycles = rte_get_timer_cycles();
		if ((uint64_t)(cycles - prev_cycles) > max_inc) {
			printf("increment too high or going backwards\n");
			return -1;
		}
		prev_cycles = cycles;
	}

	/* check that waiting 1 second is precise */
	prev_cycles = rte_get_timer_cycles();
	rte_delay_us(1000000);
	cycles = rte_get_timer_cycles();

	if ((uint64_t)(cycles - prev_cycles) > (hz + max_inc)) {
		printf("delay_us is not accurate: too long\n");
		return -1;
	}
	if ((uint64_t)(cycles - prev_cycles) < (hz - max_inc)) {
		printf("delay_us is not accurate: too short\n");
		return -1;
	}

	return 0;
}
