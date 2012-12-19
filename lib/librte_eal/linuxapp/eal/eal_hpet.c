/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_tailq.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

#define DEV_HPET "/dev/hpet"

/* Maximum number of counters. */
#define HPET_TIMER_NUM 3

/* General capabilities register */
#define CLK_PERIOD_SHIFT     32 /* Clock period shift. */
#define CLK_PERIOD_MASK      0xffffffff00000000ULL /* Clock period mask. */
#define COUNT_SIZE_CAP_SHIFT 13 /* Count size capa. shift. */
#define COUNT_SIZE_CAP_MASK 0x0000000000002000ULL /* Count size capa. mask. */

/**
 * HPET timer registers. From the Intel IA-PC HPET (High Precision Event
 * Timers) Specification.
 */
struct eal_hpet_regs {
	/* Memory-mapped, software visible registers */
	uint64_t capabilities;      /**< RO General Capabilities Register. */
	uint64_t reserved0;         /**< Reserved for future use. */
	uint64_t config;            /**< RW General Configuration Register. */
	uint64_t reserved1;         /**< Reserved for future use. */
	uint64_t isr;               /**< RW Clear General Interrupt Status. */
	uint64_t reserved2[25];     /**< Reserved for future use. */
	union {
		uint64_t counter;   /**< RW Main Counter Value Register. */
		struct {
			uint32_t counter_l; /**< RW Main Counter Low. */
			uint32_t counter_h; /**< RW Main Counter High. */
		};
	};
	uint64_t reserved3;         /**< Reserved for future use. */
	struct {
		uint64_t config;    /**< RW Timer Config and Capability Reg. */
		uint64_t comp;      /**< RW Timer Comparator Value Register. */
		uint64_t fsb;       /**< RW FSB Interrupt Route Register. */
		uint64_t reserved4; /**< Reserved for future use. */
	} timers[HPET_TIMER_NUM]; /**< Set of HPET timers. */
};

/* Mmap'd hpet registers */
static volatile struct eal_hpet_regs *eal_hpet = NULL;

/* Period at which the counter increments in femtoseconds (10^-15 seconds). */
static uint32_t eal_hpet_resolution_fs = 0;

/* Frequency of the counter in Hz */
static uint64_t eal_hpet_resolution_hz = 0;

/* Incremented 4 times during one 32bits hpet full count */
static uint32_t eal_hpet_msb;

static pthread_t msb_inc_thread_id;

/*
 * This function runs on a specific thread to update a global variable
 * containing used to process MSB of the HPET (unfortunatelly, we need
 * this because hpet is 32 bits by default under linux).
 */
static __attribute__((noreturn)) void *
hpet_msb_inc(__attribute__((unused)) void *arg)
{
	uint32_t t;

	while (1) {
		t = (eal_hpet->counter_l >> 30);
		if (t != (eal_hpet_msb & 3))
			eal_hpet_msb ++;
		sleep(10);
	}
}

static inline void
set_rdtsc_freq(void)
{
	uint64_t start;

	start = rte_rdtsc();
	sleep(1);
	eal_hpet_resolution_hz = rte_rdtsc() - start;
	eal_hpet_resolution_fs = (uint32_t)
			((1.0 / eal_hpet_resolution_hz) / 1e-15);
}

/*
 * Open and mmap /dev/hpet (high precision event timer) that will
 * provide our time reference.
 */
int
rte_eal_hpet_init(void)
{
	int fd, ret;

	if (internal_config.no_hpet) {
		goto use_rdtsc;
	}

	fd = open(DEV_HPET, O_RDONLY);
	if (fd < 0) {
		RTE_LOG(WARNING, EAL, "WARNING: Cannot open "DEV_HPET": %s! "
				"The TSC will be used instead.\n",
			strerror(errno));
		goto use_rdtsc;
	}
	eal_hpet = mmap(NULL, 1024, PROT_READ, MAP_SHARED, fd, 0);
	if (eal_hpet == MAP_FAILED) {
		RTE_LOG(WARNING, EAL, "WARNING: Cannot mmap "DEV_HPET"! "
				"The TSC will be used instead.\n");
		close(fd);
		goto use_rdtsc;
	}
	close(fd);

	eal_hpet_resolution_fs = (uint32_t)((eal_hpet->capabilities &
					CLK_PERIOD_MASK) >>
					CLK_PERIOD_SHIFT);

	eal_hpet_resolution_hz = (1000ULL*1000ULL*1000ULL*1000ULL*1000ULL) /
		(uint64_t)eal_hpet_resolution_fs;

	eal_hpet_msb = (eal_hpet->counter_l >> 30);

	/* create a thread that will increment a global variable for
	 * msb (hpet is 32 bits by default under linux) */
	ret = pthread_create(&msb_inc_thread_id, NULL, hpet_msb_inc, NULL);
	if (ret < 0) {
		RTE_LOG(WARNING, EAL, "WARNING: Cannot create HPET timer thread! "
				"The TSC will be used instead.\n");
		goto use_rdtsc;
	}

	return 0;

use_rdtsc:
	internal_config.no_hpet = 1;
	set_rdtsc_freq();
	return 0;
}

uint64_t
rte_get_hpet_hz(void)
{
	return eal_hpet_resolution_hz;
}

uint64_t
rte_get_hpet_cycles(void)
{
	uint32_t t, msb;
	uint64_t ret;

	if(internal_config.no_hpet)
		/* fallback to rdtsc */
		return rte_rdtsc();

	t = eal_hpet->counter_l;
	msb = eal_hpet_msb;
	ret = (msb + 2 - (t >> 30)) / 4;
	ret <<= 32;
	ret += t;
	return ret;
}

void
rte_delay_us(unsigned us)
{
	uint64_t start;
	uint64_t ticks;
	ticks = (uint64_t)us * 1000ULL * 1000ULL * 1000ULL;
	ticks /= eal_hpet_resolution_fs;
	start = rte_get_hpet_cycles();
	while ((rte_get_hpet_cycles() - start) < ticks)
		rte_pause();
}
