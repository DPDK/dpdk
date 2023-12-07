/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <rte_memcpy.h>
#include <rte_stdatomic.h>
#include <rte_string_fns.h>

#include "power_acpi_cpufreq.h"
#include "power_common.h"

#define STR_SIZE     1024
#define POWER_CONVERT_TO_DECIMAL 10

#define POWER_GOVERNOR_USERSPACE "userspace"
#define POWER_SYSFILE_AVAIL_FREQ \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_available_frequencies"
#define POWER_SYSFILE_SETSPEED   \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_setspeed"
#define POWER_ACPI_DRIVER "acpi-cpufreq"

/*
 * MSR related
 */
#define PLATFORM_INFO     0x0CE
#define TURBO_RATIO_LIMIT 0x1AD
#define IA32_PERF_CTL     0x199
#define CORE_TURBO_DISABLE_BIT ((uint64_t)1<<32)

enum power_state {
	POWER_IDLE = 0,
	POWER_ONGOING,
	POWER_USED,
	POWER_UNKNOWN
};

/**
 * Power info per lcore.
 */
struct acpi_power_info {
	unsigned int lcore_id;               /**< Logical core id */
	uint32_t freqs[RTE_MAX_LCORE_FREQS]; /**< Frequency array */
	uint32_t nb_freqs;                   /**< number of available freqs */
	FILE *f;                             /**< FD of scaling_setspeed */
	char governor_ori[32];               /**< Original governor name */
	uint32_t curr_idx;                   /**< Freq index in freqs array */
	RTE_ATOMIC(uint32_t) state;          /**< Power in use state */
	uint16_t turbo_available;            /**< Turbo Boost available */
	uint16_t turbo_enable;               /**< Turbo Boost enable/disable */
} __rte_cache_aligned;

static struct acpi_power_info lcore_power_info[RTE_MAX_LCORE];

/**
 * It is to set specific freq for specific logical core, according to the index
 * of supported frequencies.
 */
static int
set_freq_internal(struct acpi_power_info *pi, uint32_t idx)
{
	if (idx >= RTE_MAX_LCORE_FREQS || idx >= pi->nb_freqs) {
		POWER_LOG(ERR, "Invalid frequency index %u, which "
				"should be less than %u", idx, pi->nb_freqs);
		return -1;
	}

	/* Check if it is the same as current */
	if (idx == pi->curr_idx)
		return 0;

	POWER_DEBUG_LOG("Frequency[%u] %u to be set for lcore %u",
			idx, pi->freqs[idx], pi->lcore_id);
	if (fseek(pi->f, 0, SEEK_SET) < 0) {
		POWER_LOG(ERR, "Fail to set file position indicator to 0 "
				"for setting frequency for lcore %u", pi->lcore_id);
		return -1;
	}
	if (fprintf(pi->f, "%u", pi->freqs[idx]) < 0) {
		POWER_LOG(ERR, "Fail to write new frequency for "
				"lcore %u", pi->lcore_id);
		return -1;
	}
	fflush(pi->f);
	pi->curr_idx = idx;

	return 1;
}

/**
 * It is to check the current scaling governor by reading sys file, and then
 * set it into 'userspace' if it is not by writing the sys file. The original
 * governor will be saved for rolling back.
 */
static int
power_set_governor_userspace(struct acpi_power_info *pi)
{
	return power_set_governor(pi->lcore_id, POWER_GOVERNOR_USERSPACE,
			pi->governor_ori, sizeof(pi->governor_ori));
}

/**
 * It is to check the governor and then set the original governor back if
 * needed by writing the sys file.
 */
static int
power_set_governor_original(struct acpi_power_info *pi)
{
	return power_set_governor(pi->lcore_id, pi->governor_ori, NULL, 0);
}

/**
 * It is to get the available frequencies of the specific lcore by reading the
 * sys file.
 */
static int
power_get_available_freqs(struct acpi_power_info *pi)
{
	FILE *f;
	int ret = -1, i, count;
	char *p;
	char buf[BUFSIZ];
	char *freqs[RTE_MAX_LCORE_FREQS];

	open_core_sysfs_file(&f, "r", POWER_SYSFILE_AVAIL_FREQ, pi->lcore_id);
	if (f == NULL) {
		POWER_LOG(ERR, "failed to open %s",
				POWER_SYSFILE_AVAIL_FREQ);
		goto out;
	}

	ret = read_core_sysfs_s(f, buf, sizeof(buf));
	if ((ret) < 0) {
		POWER_LOG(ERR, "Failed to read %s",
				POWER_SYSFILE_AVAIL_FREQ);
		goto out;
	}

	/* Split string into at most RTE_MAX_LCORE_FREQS frequencies */
	count = rte_strsplit(buf, sizeof(buf), freqs,
			RTE_MAX_LCORE_FREQS, ' ');
	if (count <= 0) {
		POWER_LOG(ERR, "No available frequency in "
				POWER_SYSFILE_AVAIL_FREQ, pi->lcore_id);
		goto out;
	}
	if (count >= RTE_MAX_LCORE_FREQS) {
		POWER_LOG(ERR, "Too many available frequencies : %d",
				count);
		goto out;
	}

	/* Store the available frequencies into power context */
	for (i = 0, pi->nb_freqs = 0; i < count; i++) {
		POWER_DEBUG_LOG("Lcore %u frequency[%d]: %s", pi->lcore_id,
				i, freqs[i]);
		pi->freqs[pi->nb_freqs++] = strtoul(freqs[i], &p,
				POWER_CONVERT_TO_DECIMAL);
	}

	if ((pi->freqs[0]-1000) == pi->freqs[1]) {
		pi->turbo_available = 1;
		pi->turbo_enable = 1;
		POWER_DEBUG_LOG("Lcore %u Can do Turbo Boost",
				pi->lcore_id);
	} else {
		pi->turbo_available = 0;
		pi->turbo_enable = 0;
		POWER_DEBUG_LOG("Turbo Boost not available on Lcore %u",
				pi->lcore_id);
	}

	ret = 0;
	POWER_DEBUG_LOG("%d frequency(s) of lcore %u are available",
			count, pi->lcore_id);
out:
	if (f != NULL)
		fclose(f);

	return ret;
}

/**
 * It is to fopen the sys file for the future setting the lcore frequency.
 */
static int
power_init_for_setting_freq(struct acpi_power_info *pi)
{
	FILE *f;
	char buf[BUFSIZ];
	uint32_t i, freq;
	int ret;

	open_core_sysfs_file(&f, "rw+", POWER_SYSFILE_SETSPEED, pi->lcore_id);
	if (f == NULL) {
		POWER_LOG(ERR, "Failed to open %s",
				POWER_SYSFILE_SETSPEED);
		goto err;
	}

	ret = read_core_sysfs_s(f, buf, sizeof(buf));
	if ((ret) < 0) {
		POWER_LOG(ERR, "Failed to read %s",
				POWER_SYSFILE_SETSPEED);
		goto err;
	}

	freq = strtoul(buf, NULL, POWER_CONVERT_TO_DECIMAL);
	for (i = 0; i < pi->nb_freqs; i++) {
		if (freq == pi->freqs[i]) {
			pi->curr_idx = i;
			pi->f = f;
			return 0;
		}
	}

err:
	if (f != NULL)
		fclose(f);

	return -1;
}

int
power_acpi_cpufreq_check_supported(void)
{
	return cpufreq_check_scaling_driver(POWER_ACPI_DRIVER);
}

int
power_acpi_cpufreq_init(unsigned int lcore_id)
{
	struct acpi_power_info *pi;
	uint32_t exp_state;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Lcore id %u can not exceeds %u",
				lcore_id, RTE_MAX_LCORE - 1U);
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	exp_state = POWER_IDLE;
	/* The power in use state works as a guard variable between
	 * the CPU frequency control initialization and exit process.
	 * The ACQUIRE memory ordering here pairs with the RELEASE
	 * ordering below as lock to make sure the frequency operations
	 * in the critical section are done under the correct state.
	 */
	if (!rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state,
					POWER_ONGOING,
					rte_memory_order_acquire, rte_memory_order_relaxed)) {
		POWER_LOG(INFO, "Power management of lcore %u is "
				"in use", lcore_id);
		return -1;
	}

	pi->lcore_id = lcore_id;
	/* Check and set the governor */
	if (power_set_governor_userspace(pi) < 0) {
		POWER_LOG(ERR, "Cannot set governor of lcore %u to "
				"userspace", lcore_id);
		goto fail;
	}

	/* Get the available frequencies */
	if (power_get_available_freqs(pi) < 0) {
		POWER_LOG(ERR, "Cannot get available frequencies of "
				"lcore %u", lcore_id);
		goto fail;
	}

	/* Init for setting lcore frequency */
	if (power_init_for_setting_freq(pi) < 0) {
		POWER_LOG(ERR, "Cannot init for setting frequency for "
				"lcore %u", lcore_id);
		goto fail;
	}

	/* Set freq to max by default */
	if (power_acpi_cpufreq_freq_max(lcore_id) < 0) {
		POWER_LOG(ERR, "Cannot set frequency of lcore %u "
				"to max", lcore_id);
		goto fail;
	}

	POWER_LOG(INFO, "Initialized successfully for lcore %u "
			"power management", lcore_id);
	exp_state = POWER_ONGOING;
	rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state, POWER_USED,
				    rte_memory_order_release, rte_memory_order_relaxed);

	return 0;

fail:
	exp_state = POWER_ONGOING;
	rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state, POWER_UNKNOWN,
				    rte_memory_order_release, rte_memory_order_relaxed);

	return -1;
}

int
power_acpi_cpufreq_exit(unsigned int lcore_id)
{
	struct acpi_power_info *pi;
	uint32_t exp_state;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Lcore id %u can not exceeds %u",
				lcore_id, RTE_MAX_LCORE - 1U);
		return -1;
	}
	pi = &lcore_power_info[lcore_id];
	exp_state = POWER_USED;
	/* The power in use state works as a guard variable between
	 * the CPU frequency control initialization and exit process.
	 * The ACQUIRE memory ordering here pairs with the RELEASE
	 * ordering below as lock to make sure the frequency operations
	 * in the critical section are done under the correct state.
	 */
	if (!rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state,
					POWER_ONGOING,
					rte_memory_order_acquire, rte_memory_order_relaxed)) {
		POWER_LOG(INFO, "Power management of lcore %u is "
				"not used", lcore_id);
		return -1;
	}

	/* Close FD of setting freq */
	fclose(pi->f);
	pi->f = NULL;

	/* Set the governor back to the original */
	if (power_set_governor_original(pi) < 0) {
		POWER_LOG(ERR, "Cannot set the governor of %u back "
				"to the original", lcore_id);
		goto fail;
	}

	POWER_LOG(INFO, "Power management of lcore %u has exited from "
			"'userspace' mode and been set back to the "
			"original", lcore_id);
	exp_state = POWER_ONGOING;
	rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state, POWER_IDLE,
				    rte_memory_order_release, rte_memory_order_relaxed);

	return 0;

fail:
	exp_state = POWER_ONGOING;
	rte_atomic_compare_exchange_strong_explicit(&(pi->state), &exp_state, POWER_UNKNOWN,
				    rte_memory_order_release, rte_memory_order_relaxed);

	return -1;
}

uint32_t
power_acpi_cpufreq_freqs(unsigned int lcore_id, uint32_t *freqs, uint32_t num)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return 0;
	}

	if (freqs == NULL) {
		POWER_LOG(ERR, "NULL buffer supplied");
		return 0;
	}

	pi = &lcore_power_info[lcore_id];
	if (num < pi->nb_freqs) {
		POWER_LOG(ERR, "Buffer size is not enough");
		return 0;
	}
	rte_memcpy(freqs, pi->freqs, pi->nb_freqs * sizeof(uint32_t));

	return pi->nb_freqs;
}

uint32_t
power_acpi_cpufreq_get_freq(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return RTE_POWER_INVALID_FREQ_INDEX;
	}

	return lcore_power_info[lcore_id].curr_idx;
}

int
power_acpi_cpufreq_set_freq(unsigned int lcore_id, uint32_t index)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	return set_freq_internal(&(lcore_power_info[lcore_id]), index);
}

int
power_acpi_cpufreq_freq_down(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	if (pi->curr_idx + 1 == pi->nb_freqs)
		return 0;

	/* Frequencies in the array are from high to low. */
	return set_freq_internal(pi, pi->curr_idx + 1);
}

int
power_acpi_cpufreq_freq_up(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	if (pi->curr_idx == 0 ||
	    (pi->curr_idx == 1 && pi->turbo_available && !pi->turbo_enable))
		return 0;

	/* Frequencies in the array are from high to low. */
	return set_freq_internal(pi, pi->curr_idx - 1);
}

int
power_acpi_cpufreq_freq_max(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	/* Frequencies in the array are from high to low. */
	if (lcore_power_info[lcore_id].turbo_available) {
		if (lcore_power_info[lcore_id].turbo_enable)
			/* Set to Turbo */
			return set_freq_internal(
					&lcore_power_info[lcore_id], 0);
		else
			/* Set to max non-turbo */
			return set_freq_internal(
					&lcore_power_info[lcore_id], 1);
	} else
		return set_freq_internal(&lcore_power_info[lcore_id], 0);
}

int
power_acpi_cpufreq_freq_min(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	/* Frequencies in the array are from high to low. */
	return set_freq_internal(pi, pi->nb_freqs - 1);
}


int
power_acpi_turbo_status(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	return pi->turbo_enable;
}


int
power_acpi_enable_turbo(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	if (pi->turbo_available)
		pi->turbo_enable = 1;
	else {
		pi->turbo_enable = 0;
		POWER_LOG(ERR,
			"Failed to enable turbo on lcore %u",
			lcore_id);
			return -1;
	}

	/* Max may have changed, so call to max function */
	if (power_acpi_cpufreq_freq_max(lcore_id) < 0) {
		POWER_LOG(ERR,
			"Failed to set frequency of lcore %u to max",
			lcore_id);
			return -1;
	}

	return 0;
}

int
power_acpi_disable_turbo(unsigned int lcore_id)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	 pi->turbo_enable = 0;

	if ((pi->turbo_available) && (pi->curr_idx <= 1)) {
		/* Try to set freq to max by default coming out of turbo */
		if (power_acpi_cpufreq_freq_max(lcore_id) < 0) {
			POWER_LOG(ERR,
				"Failed to set frequency of lcore %u to max",
				lcore_id);
			return -1;
		}
	}

	return 0;
}

int power_acpi_get_capabilities(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps)
{
	struct acpi_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Invalid lcore ID");
		return -1;
	}
	if (caps == NULL) {
		POWER_LOG(ERR, "Invalid argument");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	caps->capabilities = 0;
	caps->turbo = !!(pi->turbo_available);

	return 0;
}
