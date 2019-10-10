/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_ETHDEV_H_
#define _HINIC_PMD_ETHDEV_H_

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_cfg.h"

#define HINIC_DEV_NAME_LEN	32
#define HINIC_MAX_RX_QUEUES	64

/* mbuf pool for copy invalid mbuf segs */
#define HINIC_COPY_MEMPOOL_DEPTH	128
#define HINIC_COPY_MBUF_SIZE		4096

#define SIZE_8BYTES(size)	(ALIGN((u32)(size), 8) >> 3)

#define HINIC_PKTLEN_TO_MTU(pktlen)	\
	((pktlen) - (ETH_HLEN + ETH_CRC_LEN))

#define HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev) \
	((struct hinic_nic_dev *)(dev)->data->dev_private)

#define HINIC_MAX_QUEUE_DEPTH		4096
#define HINIC_MIN_QUEUE_DEPTH		128
#define HINIC_TXD_ALIGN                 1
#define HINIC_RXD_ALIGN                 1

#define HINIC_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define HINIC_VFTA_SIZE            (4096 / HINIC_UINT32_BIT_SIZE)

enum hinic_dev_status {
	HINIC_DEV_INIT,
	HINIC_DEV_CLOSE,
	HINIC_DEV_START,
	HINIC_DEV_INTR_EN,
};

/* hinic nic_device */
struct hinic_nic_dev {
	/* hardware device */
	struct hinic_hwdev *hwdev;
	struct hinic_txq **txqs;
	struct hinic_rxq **rxqs;
	struct rte_mempool *cpy_mpool;
	u16 num_qps;
	u16 num_sq;
	u16 num_rq;
	u16 mtu_size;
	u8 rss_tmpl_idx;
	u8 rss_indir_flag;
	u8 num_rss;
	u8 rx_queue_list[HINIC_MAX_RX_QUEUES];

	u32 vfta[HINIC_VFTA_SIZE];	/* VLAN bitmap */

	/* info */
	unsigned int flags;
	struct nic_service_cap nic_cap;
	u32 rx_mode_status;	/* promisc or allmulticast */
	unsigned long dev_status;

	/* dpdk only */
	char proc_dev_name[HINIC_DEV_NAME_LEN];
	/* PF0->COS4, PF1->COS5, PF2->COS6, PF3->COS7,
	 * vf: the same with associate pf
	 */
	u32 default_cos;
};

#endif /* _HINIC_PMD_ETHDEV_H_ */
