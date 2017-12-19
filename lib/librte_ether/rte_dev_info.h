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

#endif /* _RTE_DEV_INFO_H_ */
