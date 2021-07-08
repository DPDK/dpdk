/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_ETHDEV_H_
#define _NGBE_ETHDEV_H_

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct ngbe_adapter {
	struct ngbe_hw             hw;
};

static inline struct ngbe_adapter *
ngbe_dev_adapter(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = dev->data->dev_private;

	return ad;
}

static inline struct ngbe_hw *
ngbe_dev_hw(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = &ad->hw;

	return hw;
}

#define NGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */

#endif /* _NGBE_ETHDEV_H_ */
