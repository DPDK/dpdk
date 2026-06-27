/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_FLOW_H__
#define __SXE2_FLOW_H__
#include <rte_flow_driver.h>
#include "sxe2_osal.h"
#include "sxe2_common.h"


int32_t sxe2_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);

int32_t sxe2_flow_init(struct rte_eth_dev *dev);

int32_t sxe2_flow_uninit(struct rte_eth_dev *dev);

int32_t sxe2_fnav_get_filter_cid(struct sxe2_adapter *adapter, struct sxe2_flow *flow);

int32_t sxe2_flow_free_mgr(struct sxe2_adapter *adapter,
		       struct sxe2_flow *flow,
		       struct sxe2_fnav_cid_mgr **mgr_ptr,
		       struct rte_flow_error *error);

int32_t sxe2_flow_query_mgr(struct sxe2_adapter *adapter,
			struct sxe2_flow *flow,
			struct sxe2_fnav_cid_mgr **mgr_ptr,
			struct rte_flow_error *error);
#endif /* __SXE2_FLOW_H__ */
