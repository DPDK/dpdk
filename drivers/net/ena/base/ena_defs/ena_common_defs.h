/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef _ENA_COMMON_H_
#define _ENA_COMMON_H_

#define ENA_COMMON_SPEC_VERSION_MAJOR        2
#define ENA_COMMON_SPEC_VERSION_MINOR        0

/* ENA operates with 48-bit memory addresses. ena_mem_addr_t */
struct ena_common_mem_addr {
	uint32_t mem_addr_low;

	uint16_t mem_addr_high;

	/* MBZ */
	uint16_t reserved16;
};

#endif /* _ENA_COMMON_H_ */
