/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#ifndef _ETH_PRIVATE_H_
#define _ETH_PRIVATE_H_

#include <rte_os_shim.h>

#include "rte_ethdev.h"

/*
 * Convert rte_eth_dev pointer to port ID.
 * NULL will be translated to RTE_MAX_ETHPORTS.
 */
uint16_t eth_dev_to_id(const struct rte_eth_dev *dev);

/* Generic rte_eth_dev comparison function. */
typedef int (*rte_eth_cmp_t)(const struct rte_eth_dev *, const void *);

/* Generic rte_eth_dev iterator. */
struct rte_eth_dev *
eth_find_device(const struct rte_eth_dev *_start, rte_eth_cmp_t cmp,
		const void *data);

/* Parse devargs value for representor parameter. */
int rte_eth_devargs_parse_representor_ports(char *str, void *data);

/* reset eth fast-path API to dummy values */
void eth_dev_fp_ops_reset(struct rte_eth_fp_ops *fpo);

/* setup eth fast-path API to ethdev values */
void eth_dev_fp_ops_setup(struct rte_eth_fp_ops *fpo,
		const struct rte_eth_dev *dev);

#endif /* _ETH_PRIVATE_H_ */
