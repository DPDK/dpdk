/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _FPGA_5GNR_FEC_H_
#define _FPGA_5GNR_FEC_H_

#include <stdint.h>
#include <stdbool.h>

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, fpga_5gnr_fec_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "fpga_5gnr_fec: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

/* FPGA 5GNR FEC driver names */
#define FPGA_5GNR_FEC_PF_DRIVER_NAME intel_fpga_5gnr_fec_pf
#define FPGA_5GNR_FEC_VF_DRIVER_NAME intel_fpga_5gnr_fec_vf

/* FPGA 5GNR FEC PCI vendor & device IDs */
#define FPGA_5GNR_FEC_VENDOR_ID (0x8086)
#define FPGA_5GNR_FEC_PF_DEVICE_ID (0x0D8F)
#define FPGA_5GNR_FEC_VF_DEVICE_ID (0x0D90)

/* Private data structure for each FPGA FEC device */
struct fpga_5gnr_fec_device {
	/** Base address of MMIO registers (BAR0) */
	void *mmio_base;
	/** True if this is a PF FPGA FEC device */
	bool pf_device;
};

#endif /* _FPGA_5GNR_FEC_H_ */
