/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_PMD_FPGA_5GNR_FEC_H_
#define _RTE_PMD_FPGA_5GNR_FEC_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @file rte_pmd_fpga_5gnr_fec.h
 *
 * Interface for Intel(R) FGPA 5GNR FEC device configuration at the host level,
 * directly accessible by the application.
 * Configuration related to 5GNR functionality is done through
 * librte_bbdev library.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Number of Virtual Functions FGPA 4G FEC supports */
#define FPGA_5GNR_FEC_NUM_VFS 8

/**
 * Structure to pass FPGA 4G FEC configuration.
 */
struct fpga_5gnr_fec_conf {
	/** 1 if PF is used for dataplane, 0 for VFs */
	bool pf_mode_en;
	/** Number of UL queues per VF */
	uint8_t vf_ul_queues_number[FPGA_5GNR_FEC_NUM_VFS];
	/** Number of DL queues per VF */
	uint8_t vf_dl_queues_number[FPGA_5GNR_FEC_NUM_VFS];
	/** UL bandwidth. Needed for schedule algorithm */
	uint8_t ul_bandwidth;
	/** DL bandwidth. Needed for schedule algorithm */
	uint8_t dl_bandwidth;
	/** UL Load Balance */
	uint8_t ul_load_balance;
	/** DL Load Balance */
	uint8_t dl_load_balance;
	/** FLR timeout value */
	uint16_t flr_time_out;
};

/**
 * Configure Intel(R) FPGA 5GNR FEC device
 *
 * @param dev_name
 *   The name of the device. This is the short form of PCI BDF, e.g. 00:01.0.
 *   It can also be retrieved for a bbdev device from the dev_name field in the
 *   rte_bbdev_info structure returned by rte_bbdev_info_get().
 * @param conf
 *   Configuration to apply to FPGA 4G FEC.
 *
 * @return
 *   Zero on success, negative value on failure.
 */
__rte_experimental
int
fpga_5gnr_fec_configure(const char *dev_name,
		const struct fpga_5gnr_fec_conf *conf);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_FPGA_5GNR_FEC_H_ */
