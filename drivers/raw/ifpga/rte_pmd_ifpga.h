/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _RTE_PMD_IFPGA_H_
#define _RTE_PMD_IFPGA_H_

/**
 * @file rte_pmd_ifpga.h
 *
 * ifpga PMD specific functions.
 *
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Get raw device ID from PCI address string like 'Domain:Bus:Dev.Func'
 *
 * @param pci_addr
 *    The PCI address of specified Intel FPGA device.
 * @param dev_id
 *    The buffer to output device ID.
 * @return
 *   - (0) if successful.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENODEV) if FPGA is not probed by ifpga driver.
 */
__rte_experimental
int
rte_pmd_ifpga_get_dev_id(const char *pci_addr, uint16_t *dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Update image flash of specified Intel FPGA device
 *
 * @param dev_id
 *   The raw device ID of specified Intel FPGA device.
 * @param image
 *   The image file name string.
 * @param status
 *   The detailed update status for debug.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-EINVAL) if bad parameter or staging area is not initialized.
 *   - (-EBUSY) if FPGA is updating or rebooting.
 *   - (-EIO) if failed to open image file.
 */
__rte_experimental
int
rte_pmd_ifpga_update_flash(uint16_t dev_id, const char *image,
	uint64_t *status);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Stop flash update of specified Intel FPGA device
 *
 * @param dev_id
 *   The raw device ID of specified Intel FPGA device.
 * @param force
 *   Abort the update process by writing register if set non-zero.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-EAGAIN) if failed with force.
 */
__rte_experimental
int
rte_pmd_ifpga_stop_update(uint16_t dev_id, int force);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Check current Intel FPGA status and change it to reboot status if it is idle
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @return
 *   - (0) if FPGA is ready to reboot.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-ENOMEM) if share data is not initialized.
 *   - (-EBUSY) if FPGA is updating or rebooting.
 */
__rte_experimental
int
rte_pmd_ifpga_reboot_try(uint16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 * Trigger full reconfiguration of specified Intel FPGA device
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @param type
 *    Select reconfiguration type.
 *    0 - reconfigure FPGA only.
 *    1 - reboot the whole card including FPGA.
 * @param page
 *    Select image from which flash partition.
 *    0 - factory partition.
 *    1 - user partition.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-EBUSY) if failed to access BMC register.
 */
__rte_experimental
int
rte_pmd_ifpga_reload(uint16_t dev_id, int type, int page);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_IFPGA_H_ */
