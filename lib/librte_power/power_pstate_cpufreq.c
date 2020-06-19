/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>

#include <rte_atomic.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "power_pstate_cpufreq.h"
#include "power_common.h"


#ifdef RTE_LIBRTE_POWER_DEBUG
#define POWER_DEBUG_TRACE(fmt, args...) do { \
		RTE_LOG(ERR, POWER, "%s: " fmt, __func__, ## args); \
} while (0)
#else
#define POWER_DEBUG_TRACE(fmt, args...)
#endif

#define FOPEN_OR_ERR_RET(f, retval) do { \
		if ((f) == NULL) { \
			RTE_LOG(ERR, POWER, "File not opened\n"); \
			return retval; \
		} \
} while (0)

#define FOPS_OR_NULL_GOTO(ret, label) do { \
		if ((ret) == NULL) { \
			RTE_LOG(ERR, POWER, "fgets returns nothing\n"); \
			goto label; \
		} \
} while (0)

#define FOPS_OR_ERR_GOTO(ret, label) do { \
		if ((ret) < 0) { \
			RTE_LOG(ERR, POWER, "File operations failed\n"); \
			goto label; \
		} \
} while (0)


#define POWER_CONVERT_TO_DECIMAL 10
#define BUS_FREQ     100000

#define POWER_GOVERNOR_PERF "performance"
#define POWER_SYSFILE_GOVERNOR  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_governor"
#define POWER_SYSFILE_MAX_FREQ \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_max_freq"
#define POWER_SYSFILE_MIN_FREQ  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_min_freq"
#define POWER_SYSFILE_CUR_FREQ  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq"
#define POWER_SYSFILE_BASE_MAX_FREQ \
		"/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq"
#define POWER_SYSFILE_BASE_MIN_FREQ  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_min_freq"
#define POWER_SYSFILE_BASE_FREQ  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/base_frequency"
#define POWER_PSTATE_DRIVER "intel_pstate"
#define POWER_MSR_PATH  "/dev/cpu/%u/msr"

/*
 * MSR related
 */
#define PLATFORM_INFO     0x0CE
#define NON_TURBO_MASK    0xFF00
#define NON_TURBO_OFFSET  0x8


enum power_state {
	POWER_IDLE = 0,
	POWER_ONGOING,
	POWER_USED,
	POWER_UNKNOWN
};

struct pstate_power_info {
	unsigned int lcore_id;               /**< Logical core id */
	uint32_t freqs[RTE_MAX_LCORE_FREQS]; /**< Frequency array */
	uint32_t nb_freqs;                   /**< number of available freqs */
	FILE *f_cur_min;                     /**< FD of scaling_min */
	FILE *f_cur_max;                     /**< FD of scaling_max */
	char governor_ori[32];               /**< Original governor name */
	uint32_t curr_idx;                   /**< Freq index in freqs array */
	uint32_t non_turbo_max_ratio;        /**< Non Turbo Max ratio  */
	uint32_t sys_max_freq;               /**< system wide max freq  */
	uint32_t core_base_freq;             /**< core base freq  */
	volatile uint32_t state;             /**< Power in use state */
	uint16_t turbo_available;            /**< Turbo Boost available */
	uint16_t turbo_enable;               /**< Turbo Boost enable/disable */
	uint16_t priority_core;              /**< High Performance core */
} __rte_cache_aligned;


static struct pstate_power_info lcore_power_info[RTE_MAX_LCORE];

/**
 * It is to read the specific MSR.
 */

static int32_t
power_rdmsr(int msr, uint64_t *val, unsigned int lcore_id)
{
	int fd, ret;
	char fullpath[PATH_MAX];

	snprintf(fullpath, sizeof(fullpath), POWER_MSR_PATH, lcore_id);

	fd = open(fullpath, O_RDONLY);

	if (fd < 0) {
		RTE_LOG(ERR, POWER, "Error opening '%s': %s\n", fullpath,
				 strerror(errno));
		return fd;
	}

	ret = pread(fd, val, sizeof(uint64_t), msr);

	if (ret < 0) {
		RTE_LOG(ERR, POWER, "Error reading '%s': %s\n", fullpath,
				 strerror(errno));
		goto out;
	}

	POWER_DEBUG_TRACE("MSR Path %s, offset 0x%X for lcore %u\n",
			fullpath, msr, lcore_id);

	POWER_DEBUG_TRACE("Ret value %d, content is 0x%"PRIx64"\n", ret, *val);

out:	close(fd);
	return ret;
}

/**
 * It is to fopen the sys file for the future setting the lcore frequency.
 */
static int
power_init_for_setting_freq(struct pstate_power_info *pi)
{
	FILE *f_min, *f_max, *f_base;
	char fullpath_min[PATH_MAX];
	char fullpath_max[PATH_MAX];
	char fullpath_base[PATH_MAX];
	char buf_base[BUFSIZ];
	char *s_base;
	uint32_t base_ratio = 0;
	uint64_t max_non_turbo = 0;
	int  ret_val = 0;

	snprintf(fullpath_min, sizeof(fullpath_min), POWER_SYSFILE_MIN_FREQ,
			pi->lcore_id);

	f_min = fopen(fullpath_min, "rw+");
	FOPEN_OR_ERR_RET(f_min, -1);

	snprintf(fullpath_max, sizeof(fullpath_max), POWER_SYSFILE_MAX_FREQ,
			pi->lcore_id);

	f_max = fopen(fullpath_max, "rw+");
	if (f_max == NULL)
		fclose(f_min);

	FOPEN_OR_ERR_RET(f_max, -1);

	pi->f_cur_min = f_min;
	pi->f_cur_max = f_max;

	snprintf(fullpath_base, sizeof(fullpath_base), POWER_SYSFILE_BASE_FREQ,
			pi->lcore_id);

	f_base = fopen(fullpath_base, "r");
	if (f_base == NULL) {
		/* No sysfs base_frequency, that's OK, continue without */
		base_ratio = 0;
	} else {
		s_base = fgets(buf_base, sizeof(buf_base), f_base);
		FOPS_OR_NULL_GOTO(s_base, out);

		buf_base[BUFSIZ-1] = '\0';
		if (strlen(buf_base))
			/* Strip off terminating '\n' */
			strtok(buf_base, "\n");

		base_ratio = strtoul(buf_base, NULL, POWER_CONVERT_TO_DECIMAL)
				/ BUS_FREQ;
	}

	/* Add MSR read to detect turbo status */

	if (power_rdmsr(PLATFORM_INFO, &max_non_turbo, pi->lcore_id) < 0) {
		ret_val = -1;
		goto out;
	}

	max_non_turbo = (max_non_turbo&NON_TURBO_MASK)>>NON_TURBO_OFFSET;

	POWER_DEBUG_TRACE("no turbo perf %"PRIu64"\n", max_non_turbo);

	pi->non_turbo_max_ratio = max_non_turbo;

	/*
	 * If base_frequency is reported as greater than the maximum
	 * non-turbo frequency, then mark it as a high priority core.
	 */
	if (base_ratio > max_non_turbo)
		pi->priority_core = 1;
	else
		pi->priority_core = 0;
	pi->core_base_freq = base_ratio * BUS_FREQ;

out:
	if (f_base != NULL)
		fclose(f_base);
	return ret_val;
}

static int
set_freq_internal(struct pstate_power_info *pi, uint32_t idx)
{
	uint32_t target_freq = 0;

	if (idx >= RTE_MAX_LCORE_FREQS || idx >= pi->nb_freqs) {
		RTE_LOG(ERR, POWER, "Invalid frequency index %u, which "
				"should be less than %u\n", idx, pi->nb_freqs);
		return -1;
	}

	/* Check if it is the same as current */
	if (idx == pi->curr_idx)
		return 0;

	/* Because Intel Pstate Driver only allow user change min/max hint
	 * User need change the min/max as same value.
	 */
	if (fseek(pi->f_cur_min, 0, SEEK_SET) < 0) {
		RTE_LOG(ERR, POWER, "Fail to set file position indicator to 0 "
				"for setting frequency for lcore %u\n",
				pi->lcore_id);
		return -1;
	}

	if (fseek(pi->f_cur_max, 0, SEEK_SET) < 0) {
		RTE_LOG(ERR, POWER, "Fail to set file position indicator to 0 "
				"for setting frequency for lcore %u\n",
				pi->lcore_id);
		return -1;
	}

	/* Turbo is available and enabled, first freq bucket is sys max freq */
	if (pi->turbo_available && idx == 0) {
		if (pi->turbo_enable)
			target_freq = pi->sys_max_freq;
		else {
			RTE_LOG(ERR, POWER, "Turbo is off, frequency can't be scaled up more %u\n",
					pi->lcore_id);
			return -1;
		}
	} else
		target_freq = pi->freqs[idx];

	/* Decrease freq, the min freq should be updated first */
	if (idx  >  pi->curr_idx) {

		if (fprintf(pi->f_cur_min, "%u", target_freq) < 0) {
			RTE_LOG(ERR, POWER, "Fail to write new frequency for "
					"lcore %u\n", pi->lcore_id);
			return -1;
		}

		if (fprintf(pi->f_cur_max, "%u", target_freq) < 0) {
			RTE_LOG(ERR, POWER, "Fail to write new frequency for "
					"lcore %u\n", pi->lcore_id);
			return -1;
		}

		POWER_DEBUG_TRACE("Frequency '%u' to be set for lcore %u\n",
				  target_freq, pi->lcore_id);

		fflush(pi->f_cur_min);
		fflush(pi->f_cur_max);

	}

	/* Increase freq, the max freq should be updated first */
	if (idx  <  pi->curr_idx) {

		if (fprintf(pi->f_cur_max, "%u", target_freq) < 0) {
			RTE_LOG(ERR, POWER, "Fail to write new frequency for "
					"lcore %u\n", pi->lcore_id);
			return -1;
		}

		if (fprintf(pi->f_cur_min, "%u", target_freq) < 0) {
			RTE_LOG(ERR, POWER, "Fail to write new frequency for "
					"lcore %u\n", pi->lcore_id);
			return -1;
		}

		POWER_DEBUG_TRACE("Frequency '%u' to be set for lcore %u\n",
				  target_freq, pi->lcore_id);

		fflush(pi->f_cur_max);
		fflush(pi->f_cur_min);
	}

	pi->curr_idx = idx;

	return 1;
}

/**
 * It is to check the current scaling governor by reading sys file, and then
 * set it into 'performance' if it is not by writing the sys file. The original
 * governor will be saved for rolling back.
 */
static int
power_set_governor_performance(struct pstate_power_info *pi)
{
	FILE *f;
	int ret = -1;
	char buf[BUFSIZ];
	char fullpath[PATH_MAX];
	char *s;
	int val;

	snprintf(fullpath, sizeof(fullpath), POWER_SYSFILE_GOVERNOR,
			pi->lcore_id);
	f = fopen(fullpath, "rw+");
	FOPEN_OR_ERR_RET(f, ret);

	s = fgets(buf, sizeof(buf), f);
	FOPS_OR_NULL_GOTO(s, out);
	/* Strip off terminating '\n' */
	strtok(buf, "\n");

	/* Check if current governor is performance */
	if (strncmp(buf, POWER_GOVERNOR_PERF,
			sizeof(POWER_GOVERNOR_PERF)) == 0) {
		ret = 0;
		POWER_DEBUG_TRACE("Power management governor of lcore %u is "
				"already performance\n", pi->lcore_id);
		goto out;
	}
	/* Save the original governor */
	strlcpy(pi->governor_ori, buf, sizeof(pi->governor_ori));

	/* Write 'performance' to the governor */
	val = fseek(f, 0, SEEK_SET);
	FOPS_OR_ERR_GOTO(val, out);

	val = fputs(POWER_GOVERNOR_PERF, f);
	FOPS_OR_ERR_GOTO(val, out);

	/* We need to flush to see if the fputs succeeds */
	val = fflush(f);
	FOPS_OR_ERR_GOTO(val, out);

	ret = 0;
	RTE_LOG(INFO, POWER, "Power management governor of lcore %u has been "
			"set to performance successfully\n", pi->lcore_id);
out:
	fclose(f);

	return ret;
}

/**
 * It is to check the governor and then set the original governor back if
 * needed by writing the sys file.
 */
static int
power_set_governor_original(struct pstate_power_info *pi)
{
	FILE *f;
	int ret = -1;
	char buf[BUFSIZ];
	char fullpath[PATH_MAX];
	char *s;
	int val;

	snprintf(fullpath, sizeof(fullpath), POWER_SYSFILE_GOVERNOR,
			pi->lcore_id);
	f = fopen(fullpath, "rw+");
	FOPEN_OR_ERR_RET(f, ret);

	s = fgets(buf, sizeof(buf), f);
	FOPS_OR_NULL_GOTO(s, out);

	/* Check if the governor to be set is the same as current */
	if (strncmp(buf, pi->governor_ori, sizeof(pi->governor_ori)) == 0) {
		ret = 0;
		POWER_DEBUG_TRACE("Power management governor of lcore %u "
				"has already been set to %s\n",
				pi->lcore_id, pi->governor_ori);
		goto out;
	}

	/* Write back the original governor */
	val = fseek(f, 0, SEEK_SET);
	FOPS_OR_ERR_GOTO(val, out);

	val = fputs(pi->governor_ori, f);
	FOPS_OR_ERR_GOTO(val, out);

	ret = 0;
	RTE_LOG(INFO, POWER, "Power management governor of lcore %u "
			"has been set back to %s successfully\n",
			pi->lcore_id, pi->governor_ori);
out:
	fclose(f);

	return ret;
}

/**
 * It is to get the available frequencies of the specific lcore by reading the
 * sys file.
 */
static int
power_get_available_freqs(struct pstate_power_info *pi)
{
	FILE *f_min, *f_max;
	int ret = -1;
	char *p_min, *p_max;
	char buf_min[BUFSIZ];
	char buf_max[BUFSIZ];
	char fullpath_min[PATH_MAX];
	char fullpath_max[PATH_MAX];
	char *s_min, *s_max;
	uint32_t sys_min_freq = 0, sys_max_freq = 0, base_max_freq = 0;
	uint32_t i, num_freqs = 0;

	snprintf(fullpath_max, sizeof(fullpath_max),
			POWER_SYSFILE_BASE_MAX_FREQ,
			pi->lcore_id);
	snprintf(fullpath_min, sizeof(fullpath_min),
			POWER_SYSFILE_BASE_MIN_FREQ,
			pi->lcore_id);

	f_min = fopen(fullpath_min, "r");
	FOPEN_OR_ERR_RET(f_min, ret);

	f_max = fopen(fullpath_max, "r");
	if (f_max == NULL)
		fclose(f_min);

	FOPEN_OR_ERR_RET(f_max, ret);

	s_min = fgets(buf_min, sizeof(buf_min), f_min);
	FOPS_OR_NULL_GOTO(s_min, out);

	s_max = fgets(buf_max, sizeof(buf_max), f_max);
	FOPS_OR_NULL_GOTO(s_max, out);


	/* Strip the line break if there is */
	p_min = strchr(buf_min, '\n');
	if (p_min != NULL)
		*p_min = 0;

	p_max = strchr(buf_max, '\n');
	if (p_max != NULL)
		*p_max = 0;

	sys_min_freq = strtoul(buf_min, &p_min, POWER_CONVERT_TO_DECIMAL);
	sys_max_freq = strtoul(buf_max, &p_max, POWER_CONVERT_TO_DECIMAL);

	if (sys_max_freq < sys_min_freq)
		goto out;

	pi->sys_max_freq = sys_max_freq;

	if (pi->priority_core == 1)
		base_max_freq = pi->core_base_freq;
	else
		base_max_freq = pi->non_turbo_max_ratio * BUS_FREQ;

	POWER_DEBUG_TRACE("sys min %u, sys max %u, base_max %u\n",
			sys_min_freq,
			sys_max_freq,
			base_max_freq);

	if (base_max_freq < sys_max_freq)
		pi->turbo_available = 1;
	else
		pi->turbo_available = 0;

	/* If turbo is available then there is one extra freq bucket
	 * to store the sys max freq which value is base_max +1
	 */
	num_freqs = (base_max_freq - sys_min_freq) / BUS_FREQ + 1 +
		pi->turbo_available;

	/* Generate the freq bucket array.
	 * If turbo is available the freq bucket[0] value is base_max +1
	 * the bucket[1] is base_max, bucket[2] is base_max - BUS_FREQ
	 * and so on.
	 * If turbo is not available bucket[0] is base_max and so on
	 */
	for (i = 0, pi->nb_freqs = 0; i < num_freqs; i++) {
		if ((i == 0) && pi->turbo_available)
			pi->freqs[pi->nb_freqs++] = base_max_freq + 1;
		else
			pi->freqs[pi->nb_freqs++] =
			base_max_freq - (i - pi->turbo_available) * BUS_FREQ;
	}

	ret = 0;

	POWER_DEBUG_TRACE("%d frequency(s) of lcore %u are available\n",
			num_freqs, pi->lcore_id);

out:
	fclose(f_min);
	fclose(f_max);

	return ret;
}

int
power_pstate_cpufreq_check_supported(void)
{
	return cpufreq_check_scaling_driver(POWER_PSTATE_DRIVER);
}

int
power_pstate_cpufreq_init(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Lcore id %u can not exceed %u\n",
				lcore_id, RTE_MAX_LCORE - 1U);
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	if (rte_atomic32_cmpset(&(pi->state), POWER_IDLE, POWER_ONGOING)
			== 0) {
		RTE_LOG(INFO, POWER, "Power management of lcore %u is "
				"in use\n", lcore_id);
		return -1;
	}

	pi->lcore_id = lcore_id;
	/* Check and set the governor */
	if (power_set_governor_performance(pi) < 0) {
		RTE_LOG(ERR, POWER, "Cannot set governor of lcore %u to "
				"performance\n", lcore_id);
		goto fail;
	}
	/* Init for setting lcore frequency */
	if (power_init_for_setting_freq(pi) < 0) {
		RTE_LOG(ERR, POWER, "Cannot init for setting frequency for "
				"lcore %u\n", lcore_id);
		goto fail;
	}

	/* Get the available frequencies */
	if (power_get_available_freqs(pi) < 0) {
		RTE_LOG(ERR, POWER, "Cannot get available frequencies of "
				"lcore %u\n", lcore_id);
		goto fail;
	}


	/* Set freq to max by default */
	if (power_pstate_cpufreq_freq_max(lcore_id) < 0) {
		RTE_LOG(ERR, POWER, "Cannot set frequency of lcore %u "
				"to max\n", lcore_id);
		goto fail;
	}

	RTE_LOG(INFO, POWER, "Initialized successfully for lcore %u "
			"power management\n", lcore_id);
	rte_atomic32_cmpset(&(pi->state), POWER_ONGOING, POWER_USED);

	return 0;

fail:
	rte_atomic32_cmpset(&(pi->state), POWER_ONGOING, POWER_UNKNOWN);

	return -1;
}

int
power_pstate_cpufreq_exit(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Lcore id %u can not exceeds %u\n",
				lcore_id, RTE_MAX_LCORE - 1U);
		return -1;
	}
	pi = &lcore_power_info[lcore_id];

	if (rte_atomic32_cmpset(&(pi->state), POWER_USED, POWER_ONGOING)
			== 0) {
		RTE_LOG(INFO, POWER, "Power management of lcore %u is "
				"not used\n", lcore_id);
		return -1;
	}

	/* Close FD of setting freq */
	fclose(pi->f_cur_min);
	fclose(pi->f_cur_max);
	pi->f_cur_min = NULL;
	pi->f_cur_max = NULL;

	/* Set the governor back to the original */
	if (power_set_governor_original(pi) < 0) {
		RTE_LOG(ERR, POWER, "Cannot set the governor of %u back "
				"to the original\n", lcore_id);
		goto fail;
	}

	RTE_LOG(INFO, POWER, "Power management of lcore %u has exited from "
			"'performance' mode and been set back to the "
			"original\n", lcore_id);
	rte_atomic32_cmpset(&(pi->state), POWER_ONGOING, POWER_IDLE);

	return 0;

fail:
	rte_atomic32_cmpset(&(pi->state), POWER_ONGOING, POWER_UNKNOWN);

	return -1;
}


uint32_t
power_pstate_cpufreq_freqs(unsigned int lcore_id, uint32_t *freqs, uint32_t num)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return 0;
	}

	if (freqs == NULL) {
		RTE_LOG(ERR, POWER, "NULL buffer supplied\n");
		return 0;
	}

	pi = &lcore_power_info[lcore_id];
	if (num < pi->nb_freqs) {
		RTE_LOG(ERR, POWER, "Buffer size is not enough\n");
		return 0;
	}
	rte_memcpy(freqs, pi->freqs, pi->nb_freqs * sizeof(uint32_t));

	return pi->nb_freqs;
}

uint32_t
power_pstate_cpufreq_get_freq(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return RTE_POWER_INVALID_FREQ_INDEX;
	}

	return lcore_power_info[lcore_id].curr_idx;
}


int
power_pstate_cpufreq_set_freq(unsigned int lcore_id, uint32_t index)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	return set_freq_internal(&(lcore_power_info[lcore_id]), index);
}

int
power_pstate_cpufreq_freq_up(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
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
power_pstate_cpufreq_freq_down(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	if (pi->curr_idx + 1 == pi->nb_freqs)
		return 0;

	/* Frequencies in the array are from high to low. */
	return set_freq_internal(pi, pi->curr_idx + 1);
}

int
power_pstate_cpufreq_freq_max(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
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
power_pstate_cpufreq_freq_min(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	/* Frequencies in the array are from high to low. */
	return set_freq_internal(pi, pi->nb_freqs - 1);
}


int
power_pstate_turbo_status(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	return pi->turbo_enable;
}

int
power_pstate_enable_turbo(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	if (pi->turbo_available)
		pi->turbo_enable = 1;
	else {
		pi->turbo_enable = 0;
		RTE_LOG(ERR, POWER,
			"Failed to enable turbo on lcore %u\n",
			lcore_id);
			return -1;
	}

	return 0;
}


int
power_pstate_disable_turbo(unsigned int lcore_id)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];

	pi->turbo_enable = 0;

	if (pi->turbo_available && pi->curr_idx <= 1) {
		/* Try to set freq to max by default coming out of turbo */
		if (power_pstate_cpufreq_freq_max(lcore_id) < 0) {
			RTE_LOG(ERR, POWER,
				"Failed to set frequency of lcore %u to max\n",
				lcore_id);
			return -1;
		}
	}

	return 0;
}


int power_pstate_get_capabilities(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps)
{
	struct pstate_power_info *pi;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, POWER, "Invalid lcore ID\n");
		return -1;
	}
	if (caps == NULL) {
		RTE_LOG(ERR, POWER, "Invalid argument\n");
		return -1;
	}

	pi = &lcore_power_info[lcore_id];
	caps->capabilities = 0;
	caps->turbo = !!(pi->turbo_available);
	caps->priority = pi->priority_core;

	return 0;
}
