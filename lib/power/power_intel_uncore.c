/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>

#include <rte_memcpy.h>

#include "power_intel_uncore.h"
#include "power_common.h"

#define MAX_UNCORE_FREQS 32
#define MAX_NUMA_DIE 8
#define BUS_FREQ     100000
#define FILTER_LENGTH 18
#define PACKAGE_FILTER "package_%02u_die_*"
#define DIE_FILTER "package_%02u_die_%02u"
#define INTEL_UNCORE_FREQUENCY_DIR "/sys/devices/system/cpu/intel_uncore_frequency"
#define POWER_GOVERNOR_PERF "performance"
#define POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ \
		"/sys/devices/system/cpu/intel_uncore_frequency/package_%02u_die_%02u/max_freq_khz"
#define POWER_INTEL_UNCORE_SYSFILE_MIN_FREQ  \
		"/sys/devices/system/cpu/intel_uncore_frequency/package_%02u_die_%02u/min_freq_khz"
#define POWER_INTEL_UNCORE_SYSFILE_BASE_MAX_FREQ \
		"/sys/devices/system/cpu/intel_uncore_frequency/package_%02u_die_%02u/initial_max_freq_khz"
#define POWER_INTEL_UNCORE_SYSFILE_BASE_MIN_FREQ  \
		"/sys/devices/system/cpu/intel_uncore_frequency/package_%02u_die_%02u/initial_min_freq_khz"


struct uncore_power_info {
	unsigned int die;                  /* Core die id */
	unsigned int pkg;                  /* Package id */
	uint32_t freqs[MAX_UNCORE_FREQS];  /* Frequency array */
	uint32_t nb_freqs;                 /* Number of available freqs */
	FILE *f_cur_min;                   /* FD of scaling_min */
	FILE *f_cur_max;                   /* FD of scaling_max */
	uint32_t curr_idx;                 /* Freq index in freqs array */
	uint32_t org_min_freq;             /* Original min freq of uncore */
	uint32_t org_max_freq;             /* Original max freq of uncore */
	uint32_t init_max_freq;            /* System max uncore freq */
	uint32_t init_min_freq;            /* System min uncore freq */
} __rte_cache_aligned;

static struct uncore_power_info uncore_info[RTE_MAX_NUMA_NODES][MAX_NUMA_DIE];

static int
set_uncore_freq_internal(struct uncore_power_info *ui, uint32_t idx)
{
	uint32_t target_uncore_freq, curr_max_freq;
	int ret;

	if (idx >= MAX_UNCORE_FREQS || idx >= ui->nb_freqs) {
		POWER_LOG(DEBUG, "Invalid uncore frequency index %u, which "
				"should be less than %u", idx, ui->nb_freqs);
		return -1;
	}

	target_uncore_freq = ui->freqs[idx];

	/* check current max freq, so that the value to be flushed first
	 * can be accurately recorded
	 */
	open_core_sysfs_file(&ui->f_cur_max, "rw+", POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ,
			ui->pkg, ui->die);
	if (ui->f_cur_max == NULL) {
		POWER_LOG(DEBUG, "failed to open %s",
				POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ);
		return -1;
	}
	ret = read_core_sysfs_u32(ui->f_cur_max, &curr_max_freq);
	if (ret < 0) {
		POWER_LOG(DEBUG, "Failed to read %s",
				POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ);
		fclose(ui->f_cur_max);
		return -1;
	}

	/* check this value first before fprintf value to f_cur_max, so value isn't overwritten */
	if (fprintf(ui->f_cur_min, "%u", target_uncore_freq) < 0) {
		POWER_LOG(ERR, "Fail to write new uncore frequency for "
				"pkg %02u die %02u", ui->pkg, ui->die);
		return -1;
	}

	if (fprintf(ui->f_cur_max, "%u", target_uncore_freq) < 0) {
		POWER_LOG(ERR, "Fail to write new uncore frequency for "
				"pkg %02u die %02u", ui->pkg, ui->die);
		return -1;
	}

	POWER_DEBUG_LOG("Uncore frequency '%u' to be set for pkg %02u die %02u",
				target_uncore_freq, ui->pkg, ui->die);

	/* write the minimum value first if the target freq is less than current max */
	if (target_uncore_freq <= curr_max_freq) {
		fflush(ui->f_cur_min);
		fflush(ui->f_cur_max);
	} else {
		fflush(ui->f_cur_max);
		fflush(ui->f_cur_min);
	}
	ui->curr_idx = idx;

	return 0;
}

/*
 * Fopen the sys file for the future setting of the uncore die frequency.
 */
static int
power_init_for_setting_uncore_freq(struct uncore_power_info *ui)
{
	FILE *f_base_min = NULL, *f_base_max = NULL, *f_min = NULL, *f_max = NULL;
	uint32_t base_min_freq = 0, base_max_freq = 0, min_freq = 0, max_freq = 0;
	int ret;

	/* open and read all uncore sys files */
	/* Base max */
	open_core_sysfs_file(&f_base_max, "r", POWER_INTEL_UNCORE_SYSFILE_BASE_MAX_FREQ,
			ui->pkg, ui->die);
	if (f_base_max == NULL) {
		POWER_LOG(DEBUG, "failed to open %s",
				POWER_INTEL_UNCORE_SYSFILE_BASE_MAX_FREQ);
		goto err;
	}
	ret = read_core_sysfs_u32(f_base_max, &base_max_freq);
	if (ret < 0) {
		POWER_LOG(DEBUG, "Failed to read %s",
				POWER_INTEL_UNCORE_SYSFILE_BASE_MAX_FREQ);
		goto err;
	}

	/* Base min */
	open_core_sysfs_file(&f_base_min, "r", POWER_INTEL_UNCORE_SYSFILE_BASE_MIN_FREQ,
		ui->pkg, ui->die);
	if (f_base_min == NULL) {
		POWER_LOG(DEBUG, "failed to open %s",
				POWER_INTEL_UNCORE_SYSFILE_BASE_MIN_FREQ);
		goto err;
	}
	if (f_base_min != NULL) {
		ret = read_core_sysfs_u32(f_base_min, &base_min_freq);
		if (ret < 0) {
			POWER_LOG(DEBUG, "Failed to read %s",
					POWER_INTEL_UNCORE_SYSFILE_BASE_MIN_FREQ);
			goto err;
		}
	}

	/* Curr min */
	open_core_sysfs_file(&f_min, "rw+", POWER_INTEL_UNCORE_SYSFILE_MIN_FREQ,
			ui->pkg, ui->die);
	if (f_min == NULL) {
		POWER_LOG(DEBUG, "failed to open %s",
				POWER_INTEL_UNCORE_SYSFILE_MIN_FREQ);
		goto err;
	}
	if (f_min != NULL) {
		ret = read_core_sysfs_u32(f_min, &min_freq);
		if (ret < 0) {
			POWER_LOG(DEBUG, "Failed to read %s",
					POWER_INTEL_UNCORE_SYSFILE_MIN_FREQ);
			goto err;
		}
	}

	/* Curr max */
	open_core_sysfs_file(&f_max, "rw+", POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ,
			ui->pkg, ui->die);
	if (f_max == NULL) {
		POWER_LOG(DEBUG, "failed to open %s",
				POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ);
		goto err;
	}
	if (f_max != NULL) {
		ret = read_core_sysfs_u32(f_max, &max_freq);
		if (ret < 0) {
			POWER_LOG(DEBUG, "Failed to read %s",
					POWER_INTEL_UNCORE_SYSFILE_MAX_FREQ);
			goto err;
		}
	}

	/* assign file handles */
	ui->f_cur_min = f_min;
	ui->f_cur_max = f_max;
	/* save current min + max freq's so that they can be restored on exit */
	ui->org_min_freq = min_freq;
	ui->org_max_freq = max_freq;
	ui->init_max_freq = base_max_freq;
	ui->init_min_freq = base_min_freq;

	fclose(f_base_min);
	fclose(f_base_max);
	/* f_min and f_max are stored, no need to close */

	return 0;

err:
	if (f_base_min != NULL)
		fclose(f_base_min);
	if (f_base_max != NULL)
		fclose(f_base_max);
	if (f_min != NULL)
		fclose(f_min);
	if (f_max != NULL)
		fclose(f_max);
	return -1;
}

/*
 * Get the available uncore frequencies of the specific die by reading the
 * sys file.
 */
static int
power_get_available_uncore_freqs(struct uncore_power_info *ui)
{
	int ret = -1;
	uint32_t i, num_uncore_freqs = 0;

	num_uncore_freqs = (ui->init_max_freq - ui->init_min_freq) / BUS_FREQ + 1;
	if (num_uncore_freqs >= MAX_UNCORE_FREQS) {
		POWER_LOG(ERR, "Too many available uncore frequencies: %d",
				num_uncore_freqs);
		goto out;
	}

	/* Generate the uncore freq bucket array. */
	for (i = 0; i < num_uncore_freqs; i++)
		ui->freqs[i] = ui->init_max_freq - (i) * BUS_FREQ;

	ui->nb_freqs = num_uncore_freqs;

	ret = 0;

	POWER_DEBUG_LOG("%d frequency(s) of pkg %02u die %02u are available",
			num_uncore_freqs, ui->pkg, ui->die);

out:
	return ret;
}

static int
check_pkg_die_values(unsigned int pkg, unsigned int die)
{
	unsigned int max_pkgs, max_dies;
	max_pkgs = power_intel_uncore_get_num_pkgs();
	if (max_pkgs == 0)
		return -1;
	if (pkg >= max_pkgs) {
		POWER_LOG(DEBUG, "Package number %02u can not exceed %u",
				pkg, max_pkgs);
		return -1;
	}

	max_dies = power_intel_uncore_get_num_dies(pkg);
	if (max_dies == 0)
		return -1;
	if (die >= max_dies) {
		POWER_LOG(DEBUG, "Die number %02u can not exceed %u",
				die, max_dies);
		return -1;
	}

	return 0;
}

int
power_intel_uncore_init(unsigned int pkg, unsigned int die)
{
	struct uncore_power_info *ui;

	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	ui = &uncore_info[pkg][die];
	ui->die = die;
	ui->pkg = pkg;

	/* Init for setting uncore die frequency */
	if (power_init_for_setting_uncore_freq(ui) < 0) {
		POWER_LOG(DEBUG, "Cannot init for setting uncore frequency for "
				"pkg %02u die %02u", pkg, die);
		return -1;
	}

	/* Get the available frequencies */
	if (power_get_available_uncore_freqs(ui) < 0) {
		POWER_LOG(DEBUG, "Cannot get available uncore frequencies of "
				"pkg %02u die %02u", pkg, die);
		return -1;
	}

	return 0;
}

int
power_intel_uncore_exit(unsigned int pkg, unsigned int die)
{
	struct uncore_power_info *ui;

	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	ui = &uncore_info[pkg][die];

	if (fprintf(ui->f_cur_min, "%u", ui->org_min_freq) < 0) {
		POWER_LOG(ERR, "Fail to write original uncore frequency for "
				"pkg %02u die %02u", ui->pkg, ui->die);
		return -1;
	}

	if (fprintf(ui->f_cur_max, "%u", ui->org_max_freq) < 0) {
		POWER_LOG(ERR, "Fail to write original uncore frequency for "
				"pkg %02u die %02u", ui->pkg, ui->die);
		return -1;
	}

	fflush(ui->f_cur_min);
	fflush(ui->f_cur_max);

	/* Close FD of setting freq */
	fclose(ui->f_cur_min);
	fclose(ui->f_cur_max);
	ui->f_cur_min = NULL;
	ui->f_cur_max = NULL;

	return 0;
}

uint32_t
power_get_intel_uncore_freq(unsigned int pkg, unsigned int die)
{
	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	return uncore_info[pkg][die].curr_idx;
}

int
power_set_intel_uncore_freq(unsigned int pkg, unsigned int die, uint32_t index)
{
	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	return set_uncore_freq_internal(&(uncore_info[pkg][die]), index);
}

int
power_intel_uncore_freq_max(unsigned int pkg, unsigned int die)
{
	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	return set_uncore_freq_internal(&(uncore_info[pkg][die]), 0);
}


int
power_intel_uncore_freq_min(unsigned int pkg, unsigned int die)
{
	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	struct uncore_power_info *ui = &uncore_info[pkg][die];

	return set_uncore_freq_internal(&(uncore_info[pkg][die]), ui->nb_freqs - 1);
}

int
power_intel_uncore_freqs(unsigned int pkg, unsigned int die, uint32_t *freqs, uint32_t num)
{
	struct uncore_power_info *ui;

	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	if (freqs == NULL) {
		POWER_LOG(ERR, "NULL buffer supplied");
		return 0;
	}

	ui = &uncore_info[pkg][die];
	if (num < ui->nb_freqs) {
		POWER_LOG(ERR, "Buffer size is not enough");
		return 0;
	}
	rte_memcpy(freqs, ui->freqs, ui->nb_freqs * sizeof(uint32_t));

	return ui->nb_freqs;
}

int
power_intel_uncore_get_num_freqs(unsigned int pkg, unsigned int die)
{
	int ret = check_pkg_die_values(pkg, die);
	if (ret < 0)
		return -1;

	return uncore_info[pkg][die].nb_freqs;
}

unsigned int
power_intel_uncore_get_num_pkgs(void)
{
	DIR *d;
	struct dirent *dir;
	unsigned int count = 0;
	char filter[FILTER_LENGTH];

	d = opendir(INTEL_UNCORE_FREQUENCY_DIR);
	if (d == NULL) {
		POWER_LOG(ERR,
		"Uncore frequency management not supported/enabled on this kernel. "
		"Please enable CONFIG_INTEL_UNCORE_FREQ_CONTROL if on Intel x86 with linux kernel"
		" >= 5.6");
		return 0;
	}

	/* search by incrementing file name for max pkg file value */
	while ((dir = readdir(d)) != NULL) {
		snprintf(filter, FILTER_LENGTH, PACKAGE_FILTER, count);
		/* make sure filter string is in file name (don't include hidden files) */
		if (fnmatch(filter, dir->d_name, 0) == 0)
			count++;
	}

	closedir(d);

	return count;
}

unsigned int
power_intel_uncore_get_num_dies(unsigned int pkg)
{
	DIR *d;
	struct dirent *dir;
	unsigned int count = 0, max_pkgs;
	char filter[FILTER_LENGTH];

	max_pkgs = power_intel_uncore_get_num_pkgs();
	if (max_pkgs == 0)
		return 0;
	if (pkg >= max_pkgs) {
		POWER_LOG(DEBUG, "Invalid package number");
		return 0;
	}

	d = opendir(INTEL_UNCORE_FREQUENCY_DIR);
	if (d == NULL) {
		POWER_LOG(ERR,
		"Uncore frequency management not supported/enabled on this kernel. "
		"Please enable CONFIG_INTEL_UNCORE_FREQ_CONTROL if on Intel x86 with linux kernel"
		" >= 5.6");
		return 0;
	}

	/* search by incrementing file name for max die file value */
	while ((dir = readdir(d)) != NULL) {
		snprintf(filter, FILTER_LENGTH, DIE_FILTER, pkg, count);
		/* make sure filter string is in file name (don't include hidden files) */
		if (fnmatch(filter, dir->d_name, 0) == 0)
			count++;
	}

	closedir(d);

	return count;
}
