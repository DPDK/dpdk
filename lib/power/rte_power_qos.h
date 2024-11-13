/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#ifndef RTE_POWER_QOS_H
#define RTE_POWER_QOS_H

#include <stdint.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file rte_power_qos.h
 *
 * PM QoS API.
 *
 * The CPU-wide resume latency limit has a positive impact on this CPU's idle
 * state selection in each cpuidle governor.
 * Please see the PM QoS on CPU wide in the following link:
 * https://www.kernel.org/doc/html/latest/admin-guide/abi-testing.html?highlight=pm_qos_resume_latency_us#abi-sys-devices-power-pm-qos-resume-latency-us
 *
 * The deeper the idle state, the lower the power consumption, but the
 * longer the resume time. Some services are latency sensitive and request
 * a low resume time, like interrupt packet receiving mode.
 *
 * In these case, per-CPU PM QoS API can be used to control this CPU's idle
 * state selection and limit just enter the shallowest idle state to low the
 * delay after sleep by setting strict resume latency (zero value).
 */

#define RTE_POWER_QOS_STRICT_LATENCY_VALUE		0
#define RTE_POWER_QOS_RESUME_LATENCY_NO_CONSTRAINT	INT32_MAX

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param lcore_id
 *   target logical core id
 *
 * @param latency
 *   The latency should be greater than and equal to zero in microseconds unit.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
__rte_experimental
int rte_power_qos_set_cpu_resume_latency(uint16_t lcore_id, int latency);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the current resume latency of this logical core.
 * The default value in kernel is @see RTE_POWER_QOS_RESUME_LATENCY_NO_CONSTRAINT
 * if don't set it.
 *
 * @return
 *   Negative value on failure.
 *   >= 0 means the actual resume latency limit on this core.
 */
__rte_experimental
int rte_power_qos_get_cpu_resume_latency(uint16_t lcore_id);

#ifdef __cplusplus
}
#endif

#endif /* RTE_POWER_QOS_H */
