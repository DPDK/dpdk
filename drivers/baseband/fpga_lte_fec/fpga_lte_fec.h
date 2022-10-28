/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _FPGA_LTE_FEC_H_
#define _FPGA_LTE_FEC_H_

#include <stdint.h>
#include <stdbool.h>

#include <rte_compat.h>

/**
 * @file fpga_lte_fec.h
 *
 * Interface for Intel(R) FGPA LTE FEC device configuration at the host level,
 * directly accessible by the application.
 * Configuration related to LTE Turbo coding functionality is done through
 * librte_bbdev library.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

/**< Number of Virtual Functions FGPA 4G FEC supports */
#define FPGA_LTE_FEC_NUM_VFS 8

/**
 * Structure to pass FPGA 4G FEC configuration.
 */
struct rte_fpga_lte_fec_conf {
	/**< 1 if PF is used for dataplane, 0 for VFs */
	bool pf_mode_en;
	/**< Number of UL queues per VF */
	uint8_t vf_ul_queues_number[FPGA_LTE_FEC_NUM_VFS];
	/**< Number of DL queues per VF */
	uint8_t vf_dl_queues_number[FPGA_LTE_FEC_NUM_VFS];
	/**< UL bandwidth. Needed for schedule algorithm */
	uint8_t ul_bandwidth;
	/**< DL bandwidth. Needed for schedule algorithm */
	uint8_t dl_bandwidth;
	/**< UL Load Balance */
	uint8_t ul_load_balance;
	/**< DL Load Balance */
	uint8_t dl_load_balance;
	/**< FLR timeout value */
	uint16_t flr_time_out;
};

/**
 * Configure Intel(R) FPGA LTE FEC device
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
rte_fpga_lte_fec_configure(const char *dev_name,
		const struct rte_fpga_lte_fec_conf *conf);

#ifdef __cplusplus
}
#endif

#endif /* _FPGA_LTE_FEC_H_ */
