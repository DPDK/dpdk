/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <eal_export.h>
#include <rte_spinlock.h>
#include <rte_debug.h>

#include "power_common.h"
#include "power_cpufreq.h"

static enum power_management_env global_default_env = PM_ENV_NOT_SET;
static struct rte_power_cpufreq_ops *global_cpufreq_ops;

static rte_spinlock_t global_env_cfg_lock = RTE_SPINLOCK_INITIALIZER;
static RTE_TAILQ_HEAD(, rte_power_cpufreq_ops) cpufreq_ops_list =
			TAILQ_HEAD_INITIALIZER(cpufreq_ops_list);

const char *power_env_str[] = {
	"not set",
	"acpi",
	"kvm-vm",
	"intel-pstate",
	"cppc",
	"amd-pstate"
};

/* register the ops struct in rte_power_cpufreq_ops, return 0 on success. */
RTE_EXPORT_INTERNAL_SYMBOL(rte_power_register_cpufreq_ops)
int
rte_power_register_cpufreq_ops(struct rte_power_cpufreq_ops *driver_ops)
{
	if (!driver_ops->init || !driver_ops->exit ||
		!driver_ops->check_env_support || !driver_ops->get_avail_freqs ||
		!driver_ops->get_freq || !driver_ops->set_freq ||
		!driver_ops->freq_up || !driver_ops->freq_down ||
		!driver_ops->freq_max || !driver_ops->freq_min ||
		!driver_ops->turbo_status || !driver_ops->enable_turbo ||
		!driver_ops->disable_turbo || !driver_ops->get_caps) {
		POWER_LOG(ERR, "Missing callbacks while registering cpufreq ops");
		return -1;
	}

	TAILQ_INSERT_TAIL(&cpufreq_ops_list, driver_ops, next);

	return 0;
}

RTE_EXPORT_SYMBOL(rte_power_check_env_supported)
int
rte_power_check_env_supported(enum power_management_env env)
{
	struct rte_power_cpufreq_ops *ops;

	if (env >= RTE_DIM(power_env_str))
		return 0;

	RTE_TAILQ_FOREACH(ops, &cpufreq_ops_list, next)
		if (strncmp(ops->name, power_env_str[env],
				RTE_POWER_DRIVER_NAMESZ) == 0)
			return ops->check_env_support();

	return 0;
}

RTE_EXPORT_SYMBOL(rte_power_set_env)
int
rte_power_set_env(enum power_management_env env)
{
	struct rte_power_cpufreq_ops *ops;
	int ret = -1;

	rte_spinlock_lock(&global_env_cfg_lock);

	if (global_default_env != PM_ENV_NOT_SET) {
		POWER_LOG(ERR, "Power Management Environment already set.");
		goto out;
	}

	RTE_TAILQ_FOREACH(ops, &cpufreq_ops_list, next)
		if (strncmp(ops->name, power_env_str[env],
				RTE_POWER_DRIVER_NAMESZ) == 0) {
			global_cpufreq_ops = ops;
			global_default_env = env;
			ret = 0;
			goto out;
		}

	POWER_LOG(ERR, "Invalid Power Management Environment(%d) set",
			env);
out:
	rte_spinlock_unlock(&global_env_cfg_lock);
	return ret;
}

RTE_EXPORT_SYMBOL(rte_power_unset_env)
void
rte_power_unset_env(void)
{
	rte_spinlock_lock(&global_env_cfg_lock);
	global_default_env = PM_ENV_NOT_SET;
	global_cpufreq_ops = NULL;
	rte_spinlock_unlock(&global_env_cfg_lock);
}

RTE_EXPORT_SYMBOL(rte_power_get_env)
enum power_management_env
rte_power_get_env(void) {
	return global_default_env;
}

RTE_EXPORT_SYMBOL(rte_power_init)
int
rte_power_init(unsigned int lcore_id)
{
	struct rte_power_cpufreq_ops *ops;
	uint8_t env;

	if (global_default_env != PM_ENV_NOT_SET)
		return global_cpufreq_ops->init(lcore_id);

	POWER_LOG(INFO, "Env isn't set yet!");

	/* Auto detect Environment */
	RTE_TAILQ_FOREACH(ops, &cpufreq_ops_list, next) {
		POWER_LOG(INFO,
			"Attempting to initialise %s cpufreq power management...",
			ops->name);
		for (env = 0; env < RTE_DIM(power_env_str); env++) {
			if ((strncmp(ops->name, power_env_str[env],
				RTE_POWER_DRIVER_NAMESZ) == 0) &&
				(ops->init(lcore_id) == 0)) {
				rte_power_set_env(env);
				return 0;
			}
		}
	}

	POWER_LOG(ERR,
		"Unable to set Power Management Environment for lcore %u",
		lcore_id);

	return -1;
}

RTE_EXPORT_SYMBOL(rte_power_exit)
int
rte_power_exit(unsigned int lcore_id)
{
	if (global_default_env != PM_ENV_NOT_SET)
		return global_cpufreq_ops->exit(lcore_id);

	POWER_LOG(ERR,
		"Environment has not been set, unable to exit gracefully");

	return -1;
}

RTE_EXPORT_SYMBOL(rte_power_freqs)
uint32_t
rte_power_freqs(unsigned int lcore_id, uint32_t *freqs, uint32_t n)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->get_avail_freqs(lcore_id, freqs, n);
}

RTE_EXPORT_SYMBOL(rte_power_get_freq)
uint32_t
rte_power_get_freq(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->get_freq(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_set_freq)
uint32_t
rte_power_set_freq(unsigned int lcore_id, uint32_t index)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->set_freq(lcore_id, index);
}

RTE_EXPORT_SYMBOL(rte_power_freq_up)
int
rte_power_freq_up(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->freq_up(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_freq_down)
int
rte_power_freq_down(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->freq_down(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_freq_max)
int
rte_power_freq_max(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->freq_max(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_freq_min)
int
rte_power_freq_min(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->freq_min(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_turbo_status)
int
rte_power_turbo_status(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->turbo_status(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_freq_enable_turbo)
int
rte_power_freq_enable_turbo(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->enable_turbo(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_freq_disable_turbo)
int
rte_power_freq_disable_turbo(unsigned int lcore_id)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->disable_turbo(lcore_id);
}

RTE_EXPORT_SYMBOL(rte_power_get_capabilities)
int
rte_power_get_capabilities(unsigned int lcore_id,
		struct rte_power_core_capabilities *caps)
{
	RTE_ASSERT(global_cpufreq_ops != NULL);
	return global_cpufreq_ops->get_caps(lcore_id, caps);
}
