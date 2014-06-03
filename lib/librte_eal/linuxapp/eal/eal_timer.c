/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2012-2013 6WIND S.A.
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <pthread.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_tailq.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

enum timer_source eal_timer_source = EAL_TIMER_HPET;

/* The frequency of the RDTSC timer resolution */
static uint64_t eal_tsc_resolution_hz = 0;

#ifdef RTE_LIBEAL_USE_HPET

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

/* Period at which the HPET counter increments in
 * femtoseconds (10^-15 seconds). */
static uint32_t eal_hpet_resolution_fs = 0;

/* Frequency of the HPET counter in Hz */
static uint64_t eal_hpet_resolution_hz = 0;

/* Incremented 4 times during one 32bits hpet full count */
static uint32_t eal_hpet_msb;

static pthread_t msb_inc_thread_id;

/*
 * This function runs on a specific thread to update a global variable
 * containing used to process MSB of the HPET (unfortunatelly, we need
 * this because hpet is 32 bits by default under linux).
 */
static void
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

uint64_t
rte_get_hpet_hz(void)
{
	if(internal_config.no_hpet)
		rte_panic("Error, HPET called, but no HPET present\n");

	return eal_hpet_resolution_hz;
}

uint64_t
rte_get_hpet_cycles(void)
{
	uint32_t t, msb;
	uint64_t ret;

	if(internal_config.no_hpet)
		rte_panic("Error, HPET called, but no HPET present\n");

	t = eal_hpet->counter_l;
	msb = eal_hpet_msb;
	ret = (msb + 2 - (t >> 30)) / 4;
	ret <<= 32;
	ret += t;
	return ret;
}

#endif


void
rte_delay_us(unsigned us)
{
	const uint64_t start = rte_get_timer_cycles();
	const uint64_t ticks = (uint64_t)us * rte_get_timer_hz() / 1E6;
	while ((rte_get_timer_cycles() - start) < ticks)
		rte_pause();
}

uint64_t
rte_get_tsc_hz(void)
{
	return eal_tsc_resolution_hz;
}


#ifdef RTE_LIBEAL_USE_HPET
/*
 * Open and mmap /dev/hpet (high precision event timer) that will
 * provide our time reference.
 */
int
rte_eal_hpet_init(int make_default)
{
	int fd, ret;

	if (internal_config.no_hpet) {
		RTE_LOG(INFO, EAL, "HPET is disabled\n");
		return -1;
	}

	fd = open(DEV_HPET, O_RDONLY);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "ERROR: Cannot open "DEV_HPET": %s!\n",
			strerror(errno));
		internal_config.no_hpet = 1;
		return -1;
	}
	eal_hpet = mmap(NULL, 1024, PROT_READ, MAP_SHARED, fd, 0);
	if (eal_hpet == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "ERROR: Cannot mmap "DEV_HPET"!\n"
				"Please enable CONFIG_HPET_MMAP in your kernel configuration "
				"to allow HPET support.\n"
				"To run without using HPET, set CONFIG_RTE_LIBEAL_USE_HPET=n "
				"in your build configuration or use '--no-hpet' EAL flag.\n");
		close(fd);
		internal_config.no_hpet = 1;
		return -1;
	}
	close(fd);

	eal_hpet_resolution_fs = (uint32_t)((eal_hpet->capabilities &
					CLK_PERIOD_MASK) >>
					CLK_PERIOD_SHIFT);

	eal_hpet_resolution_hz = (1000ULL*1000ULL*1000ULL*1000ULL*1000ULL) /
		(uint64_t)eal_hpet_resolution_fs;

	RTE_LOG(INFO, EAL, "HPET frequency is ~%"PRIu64" kHz\n",
			eal_hpet_resolution_hz/1000);

	eal_hpet_msb = (eal_hpet->counter_l >> 30);

	/* create a thread that will increment a global variable for
	 * msb (hpet is 32 bits by default under linux) */
	ret = pthread_create(&msb_inc_thread_id, NULL,
			(void *(*)(void *))hpet_msb_inc, NULL);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "ERROR: Cannot create HPET timer thread!\n");
		internal_config.no_hpet = 1;
		return -1;
	}

	if (make_default)
		eal_timer_source = EAL_TIMER_HPET;
	return 0;
}
#endif

static void
check_tsc_flags(void)
{
	char line[512];
	FILE *stream;

	stream = fopen("/proc/cpuinfo", "r");
	if (!stream) {
		RTE_LOG(WARNING, EAL, "WARNING: Unable to open /proc/cpuinfo\n");
		return;
	}

	while (fgets(line, sizeof line, stream)) {
		char *constant_tsc;
		char *nonstop_tsc;

		if (strncmp(line, "flags", 5) != 0)
			continue;

		constant_tsc = strstr(line, "constant_tsc");
		nonstop_tsc = strstr(line, "nonstop_tsc");
		if (!constant_tsc || !nonstop_tsc)
			RTE_LOG(WARNING, EAL,
				"WARNING: cpu flags "
				"constant_tsc=%s "
				"nonstop_tsc=%s "
				"-> using unreliable clock cycles !\n",
				constant_tsc ? "yes":"no",
				nonstop_tsc ? "yes":"no");
		break;
	}

	fclose(stream);
}

static int
set_tsc_freq_from_clock(void)
{
#ifdef CLOCK_MONOTONIC_RAW
#define NS_PER_SEC 1E9

	struct timespec sleeptime = {.tv_nsec = 5E8 }; /* 1/2 second */

	struct timespec t_start, t_end;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
		uint64_t ns, end, start = rte_rdtsc();
		nanosleep(&sleeptime,NULL);
		clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
		end = rte_rdtsc();
		ns = ((t_end.tv_sec - t_start.tv_sec) * NS_PER_SEC);
		ns += (t_end.tv_nsec - t_start.tv_nsec);

		double secs = (double)ns/NS_PER_SEC;
		eal_tsc_resolution_hz = (uint64_t)((end - start)/secs);
		return 0;
	}
#endif
	return -1;
}

static void
set_tsc_freq_fallback(void)
{
	RTE_LOG(WARNING, EAL, "WARNING: clock_gettime cannot use "
			"CLOCK_MONOTONIC_RAW and HPET is not available"
			" - clock timings may be less accurate.\n");
	/* assume that the sleep(1) will sleep for 1 second */
	uint64_t start = rte_rdtsc();
	sleep(1);
	eal_tsc_resolution_hz = rte_rdtsc() - start;
}
/*
 * This function measures the TSC frequency. It uses a variety of approaches.
 *
 * 1. If kernel provides CLOCK_MONOTONIC_RAW we use that to tune the TSC value
 * 2. If kernel does not provide that, and we have HPET support, tune using HPET
 * 3. Lastly, if neither of the above can be used, just sleep for 1 second and
 * tune off that, printing a warning about inaccuracy of timing
 */
static void
set_tsc_freq(void)
{
	if (set_tsc_freq_from_clock() < 0)
		set_tsc_freq_fallback();

	RTE_LOG(INFO, EAL, "TSC frequency is ~%"PRIu64" KHz\n",
			eal_tsc_resolution_hz/1000);
}

int
rte_eal_timer_init(void)
{

	eal_timer_source = EAL_TIMER_TSC;

	set_tsc_freq();
	check_tsc_flags();
	return 0;
}
