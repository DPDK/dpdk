/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#ifndef _IGC_FILTER_H_
#define _IGC_FILTER_H_

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>
#include <rte_eth_ctrl.h>

#include "igc_ethdev.h"

#ifdef __cplusplus
extern "C" {
#endif

int igc_add_ethertype_filter(struct rte_eth_dev *dev,
		const struct igc_ethertype_filter *filter);
int igc_del_ethertype_filter(struct rte_eth_dev *dev,
		const struct igc_ethertype_filter *filter);
int igc_add_ntuple_filter(struct rte_eth_dev *dev,
		const struct igc_ntuple_filter *tuple);
int igc_del_ntuple_filter(struct rte_eth_dev *dev,
		const struct igc_ntuple_filter *tuple);
int igc_set_syn_filter(struct rte_eth_dev *dev,
		const struct igc_syn_filter *filter);
void igc_clear_syn_filter(struct rte_eth_dev *dev);
void igc_clear_all_filter(struct rte_eth_dev *dev);
int
eth_igc_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type filter_type,
		enum rte_filter_op filter_op, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* IGC_FILTER_H_ */
