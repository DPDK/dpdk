/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#ifndef _RTE_DEV_INFO_H_
#define _RTE_DEV_INFO_H_

#include <stdint.h>

/*
 * Placeholder for accessing device registers
 */
struct rte_dev_reg_info {
	void *data; /**< Buffer for return registers */
	uint32_t offset; /**< Start register table location for access */
	uint32_t length; /**< Number of registers to fetch */
	uint32_t width; /**< Size of device register */
	uint32_t version; /**< Device version */
};

/*
 * Placeholder for accessing device eeprom
 */
struct rte_dev_eeprom_info {
	void *data; /**< Buffer for return eeprom */
	uint32_t offset; /**< Start eeprom address for access*/
	uint32_t length; /**< Length of eeprom region to access */
	uint32_t magic; /**< Device-specific key, such as device-id */
};

/**
 * Placeholder for accessing plugin module eeprom
 */
struct rte_eth_dev_module_info {
	uint32_t type; /**< Type of plugin module eeprom */
	uint32_t eeprom_len; /**< Length of plugin module eeprom */
};

/* EEPROM Standards for plug in modules */
#define RTE_ETH_MODULE_SFF_8079             0x1
#define RTE_ETH_MODULE_SFF_8079_LEN         256
#define RTE_ETH_MODULE_SFF_8472             0x2
#define RTE_ETH_MODULE_SFF_8472_LEN         512
#define RTE_ETH_MODULE_SFF_8636             0x3
#define RTE_ETH_MODULE_SFF_8636_LEN         256
#define RTE_ETH_MODULE_SFF_8636_MAX_LEN     640
#define RTE_ETH_MODULE_SFF_8436             0x4
#define RTE_ETH_MODULE_SFF_8436_LEN         256
#define RTE_ETH_MODULE_SFF_8436_MAX_LEN     640

#endif /* _RTE_DEV_INFO_H_ */
