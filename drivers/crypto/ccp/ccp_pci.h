/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _CCP_PCI_H_
#define _CCP_PCI_H_

#include <stdint.h>

#include <rte_bus_pci.h>

#define SYSFS_PCI_DEVICES "/sys/bus/pci/devices"
#define PROC_MODULES "/proc/modules"

int ccp_check_pci_uio_module(void);

int ccp_parse_pci_addr_format(const char *buf, int bufsize, uint16_t *domain,
			      uint8_t *bus, uint8_t *devid, uint8_t *function);

int ccp_pci_parse_sysfs_value(const char *filename, unsigned long *val);

int ccp_pci_parse_sysfs_resource(const char *filename,
				 struct rte_pci_device *dev);

int ccp_find_uio_devname(const char *dirname);

#endif /* _CCP_PCI_H_ */
