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

#include <stdint.h>

#define IFPGA_MAX_PORT_NUM   4

/**
 * UUID data structure.
 */
typedef struct {
	uint8_t b[16];
} rte_pmd_ifpga_uuid;

/**
 * FME property data structure.
 */
typedef struct {
	uint32_t num_ports;
	uint32_t boot_page;
	uint64_t bitstream_id;
	uint64_t bitstream_metadata;
	rte_pmd_ifpga_uuid pr_id;
	uint32_t bmc_version;
	uint32_t bmc_nios_version;
} rte_pmd_ifpga_common_prop;

/**
 * port property data structure.
 */
typedef struct {
	rte_pmd_ifpga_uuid afu_id;
	uint32_t type;   /* AFU memory access control type */
} rte_pmd_ifpga_port_prop;

/**
 * FPGA property data structure.
 */
typedef struct {
	rte_pmd_ifpga_common_prop  common;
	rte_pmd_ifpga_port_prop    port[IFPGA_MAX_PORT_NUM];
} rte_pmd_ifpga_prop;

/**
 * PHY information data structure.
 */
typedef struct {
	uint32_t num_retimers;
	uint32_t link_speed;
	uint32_t link_status;
} rte_pmd_ifpga_phy_info;

/**
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
int
rte_pmd_ifpga_get_dev_id(const char *pci_addr, uint16_t *dev_id);

/**
 * Get current RSU status of the specified Intel FPGA device
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @param stat
 *    The buffer to output RSU status.
 * @param prog
 *    The buffer to output RSU progress.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-ENOMEM) if share data is not initialized.
 */
int
rte_pmd_ifpga_get_rsu_status(uint16_t dev_id, uint32_t *stat, uint32_t *prog);

/**
 * Set current RSU status of the specified Intel FPGA device
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @param stat
 *    The RSU status value to set.
 * @param prog
 *    The RSU progress value to set.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-ENOMEM) if share data is not initialized.
 */
int
rte_pmd_ifpga_set_rsu_status(uint16_t dev_id, uint32_t stat, uint32_t prog);

/**
 * Get FPGA property of specified Intel FPGA device
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @param prop
 *    The data pointer of FPGA property buffer.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-EBUSY) if FPGA is rebooting.
 *   - (-EIO) if failed to access hardware.
 */
int
rte_pmd_ifpga_get_property(uint16_t dev_id, rte_pmd_ifpga_prop *prop);

/**
 * Get PHY information of specified Intel FPGA device
 *
 * @param dev_id
 *    The raw device ID of specified Intel FPGA device.
 * @param info
 *    The data pointer of PHY information buffer.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if dev_id is invalid.
 *   - (-EBUSY) if FPGA is rebooting.
 *   - (-EIO) if failed to access hardware.
 */
int
rte_pmd_ifpga_get_phy_info(uint16_t dev_id, rte_pmd_ifpga_phy_info *info);

/**
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
int
rte_pmd_ifpga_update_flash(uint16_t dev_id, const char *image,
	uint64_t *status);

/**
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
int
rte_pmd_ifpga_stop_update(uint16_t dev_id, int force);

/**
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
int
rte_pmd_ifpga_reboot_try(uint16_t dev_id);

/**
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
int
rte_pmd_ifpga_reload(uint16_t dev_id, int type, int page);

/**
 * Perform PR (partial reconfiguration) on specified Intel FPGA device
 *
 * @param dev_id
 *   The raw device ID of specified Intel FPGA device.
 * @param port
 *   The port index of the partial reconfiguration area.
 * @param file
 *   The GBS (Green BitStream) image file name string.
 * @return
 *   - (0) if successful.
 *   - (-EINVAL) if bad parameter or operation failed.
 *   - (-ENOMEM) if failed to allocate memory.
 */
int
rte_pmd_ifpga_partial_reconfigure(uint16_t dev_id, int port, const char *file);

/**
 * Free software resources allocated by Intel FPGA PMD
 */
void
rte_pmd_ifpga_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_IFPGA_H_ */
