/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef _ENA_COMMON_H_
#define _ENA_COMMON_H_

#define ENA_COMMON_SPEC_VERSION_MAJOR        2
#define ENA_COMMON_SPEC_VERSION_MINOR        0

struct ena_common_mem_addr {
	uint32_t mem_addr_low;

	uint32_t mem_addr_high;
};

#endif /* _ENA_COMMON_H_ */
