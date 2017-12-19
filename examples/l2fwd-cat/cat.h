/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef _CAT_H
#define _CAT_H

/**
 * @file
 * PQoS CAT
 */

#include <stdint.h>
#include <string.h>

#include <rte_lcore.h>

#ifdef __cplusplus
extern "C" {
#endif

/* L3 cache allocation class of service data structure */
struct cat_config {
	rte_cpuset_t cpumask;		/* CPUs bitmask */
	int cdp;			/* data & code masks used if true */
	union {
		uint64_t mask;		/* capacity bitmask (CBM) */
		struct {
			uint64_t data_mask; /* data capacity bitmask (CBM) */
			uint64_t code_mask; /* code capacity bitmask (CBM) */
		};
	};
};

int cat_init(int argc, char **argv);

void cat_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _CAT_H */
