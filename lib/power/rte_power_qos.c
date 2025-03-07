/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eal_export.h>
#include <rte_lcore.h>
#include <rte_log.h>

#include "power_common.h"
#include "rte_power_qos.h"

#define PM_QOS_SYSFILE_RESUME_LATENCY_US	\
	"/sys/devices/system/cpu/cpu%u/power/pm_qos_resume_latency_us"

#define PM_QOS_CPU_RESUME_LATENCY_BUF_LEN	32

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_power_qos_set_cpu_resume_latency, 24.11)
int
rte_power_qos_set_cpu_resume_latency(uint16_t lcore_id, int latency)
{
	char buf[PM_QOS_CPU_RESUME_LATENCY_BUF_LEN];
	uint32_t cpu_id;
	FILE *f;
	int ret;

	if (!rte_lcore_is_enabled(lcore_id)) {
		POWER_LOG(ERR, "lcore id %u is not enabled", lcore_id);
		return -EINVAL;
	}
	ret = power_get_lcore_mapped_cpu_id(lcore_id, &cpu_id);
	if (ret != 0)
		return ret;

	if (latency < 0) {
		POWER_LOG(ERR, "latency should be greater than and equal to 0");
		return -EINVAL;
	}

	ret = open_core_sysfs_file(&f, "w", PM_QOS_SYSFILE_RESUME_LATENCY_US, cpu_id);
	if (ret != 0) {
		POWER_LOG(ERR, "Failed to open "PM_QOS_SYSFILE_RESUME_LATENCY_US" : %s",
			  cpu_id, strerror(errno));
		return ret;
	}

	/*
	 * Based on the sysfs interface pm_qos_resume_latency_us under
	 * @PM_QOS_SYSFILE_RESUME_LATENCY_US directory in kernel, their meaning
	 * is as follows for different input string.
	 * 1> the resume latency is 0 if the input is "n/a".
	 * 2> the resume latency is no constraint if the input is "0".
	 * 3> the resume latency is the actual value to be set.
	 */
	if (latency == RTE_POWER_QOS_STRICT_LATENCY_VALUE)
		snprintf(buf, sizeof(buf), "%s", "n/a");
	else if (latency == RTE_POWER_QOS_RESUME_LATENCY_NO_CONSTRAINT)
		snprintf(buf, sizeof(buf), "%u", 0);
	else
		snprintf(buf, sizeof(buf), "%u", latency);

	ret = write_core_sysfs_s(f, buf);
	if (ret != 0)
		POWER_LOG(ERR, "Failed to write "PM_QOS_SYSFILE_RESUME_LATENCY_US" : %s",
			  cpu_id, strerror(errno));

	fclose(f);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_power_qos_get_cpu_resume_latency, 24.11)
int
rte_power_qos_get_cpu_resume_latency(uint16_t lcore_id)
{
	char buf[PM_QOS_CPU_RESUME_LATENCY_BUF_LEN];
	int latency = -1;
	uint32_t cpu_id;
	FILE *f;
	int ret;

	if (!rte_lcore_is_enabled(lcore_id)) {
		POWER_LOG(ERR, "lcore id %u is not enabled", lcore_id);
		return -EINVAL;
	}
	ret = power_get_lcore_mapped_cpu_id(lcore_id, &cpu_id);
	if (ret != 0)
		return ret;

	ret = open_core_sysfs_file(&f, "r", PM_QOS_SYSFILE_RESUME_LATENCY_US, cpu_id);
	if (ret != 0) {
		POWER_LOG(ERR, "Failed to open "PM_QOS_SYSFILE_RESUME_LATENCY_US" : %s",
			  cpu_id, strerror(errno));
		return ret;
	}

	ret = read_core_sysfs_s(f, buf, sizeof(buf));
	if (ret != 0) {
		POWER_LOG(ERR, "Failed to read "PM_QOS_SYSFILE_RESUME_LATENCY_US" : %s",
			  cpu_id, strerror(errno));
		goto out;
	}

	/*
	 * Based on the sysfs interface pm_qos_resume_latency_us under
	 * @PM_QOS_SYSFILE_RESUME_LATENCY_US directory in kernel, their meaning
	 * is as follows for different output string.
	 * 1> the resume latency is 0 if the output is "n/a".
	 * 2> the resume latency is no constraint if the output is "0".
	 * 3> the resume latency is the actual value in used for other string.
	 */
	if (strcmp(buf, "n/a") == 0)
		latency = RTE_POWER_QOS_STRICT_LATENCY_VALUE;
	else {
		latency = strtoul(buf, NULL, 10);
		latency = latency == 0 ? RTE_POWER_QOS_RESUME_LATENCY_NO_CONSTRAINT : latency;
	}

out:
	fclose(f);

	return latency != -1 ? latency : ret;
}
