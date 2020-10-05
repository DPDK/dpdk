/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_ACC100_PMD_H_
#define _RTE_ACC100_PMD_H_

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, acc100_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "acc100_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* ACC100 PF and VF driver names */
#define ACC100PF_DRIVER_NAME           intel_acc100_pf
#define ACC100VF_DRIVER_NAME           intel_acc100_vf

/* ACC100 PCI vendor & device IDs */
#define RTE_ACC100_VENDOR_ID           (0x8086)
#define RTE_ACC100_PF_DEVICE_ID        (0x0d5c)
#define RTE_ACC100_VF_DEVICE_ID        (0x0d5d)

/* Private data structure for each ACC100 device */
struct acc100_device {
	void *mmio_base;  /**< Base address of MMIO registers (BAR0) */
	bool pf_device; /**< True if this is a PF ACC100 device */
	bool configured; /**< True if this ACC100 device is configured */
};

#endif /* _RTE_ACC100_PMD_H_ */
