/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_DEV_H_
#define _IONIC_DEV_H_

#include "ionic_osdep.h"
#include "ionic_if.h"
#include "ionic_regs.h"

#define IONIC_LIFS_MAX			1024

#define IONIC_DEVCMD_TIMEOUT	30 /* devcmd_timeout */
#define	IONIC_ALIGN             4096

struct ionic_adapter;

struct ionic_dev_bar {
	void __iomem *vaddr;
	rte_iova_t bus_addr;
	unsigned long len;
};

static inline void ionic_struct_size_checks(void)
{
	RTE_BUILD_BUG_ON(sizeof(struct ionic_doorbell) != 8);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_intr) != 32);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_intr_status) != 8);

	RTE_BUILD_BUG_ON(sizeof(union ionic_dev_regs) != 4096);
	RTE_BUILD_BUG_ON(sizeof(union ionic_dev_info_regs) != 2048);
	RTE_BUILD_BUG_ON(sizeof(union ionic_dev_cmd_regs) != 2048);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_stats) != 1024);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_admin_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_admin_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_nop_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_nop_comp) != 16);

	/* Device commands */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_identify_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_identify_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_reset_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_getattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_getattr_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_setattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_dev_setattr_comp) != 16);

	/* Port commands */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_identify_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_identify_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_reset_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_getattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_getattr_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_setattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_port_setattr_comp) != 16);

	/* LIF commands */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_getattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_getattr_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_setattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_lif_setattr_comp) != 16);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_q_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_q_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_q_control_cmd) != 64);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_rx_mode_set_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rx_filter_add_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rx_filter_add_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rx_filter_del_cmd) != 64);

	/* RDMA commands */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rdma_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rdma_queue_cmd) != 64);

	/* Events */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_notifyq_cmd) != 4);
	RTE_BUILD_BUG_ON(sizeof(union ionic_notifyq_comp) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_notifyq_event) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_link_change_event) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_reset_event) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_heartbeat_event) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_log_event) != 64);

	/* I/O */
	RTE_BUILD_BUG_ON(sizeof(struct ionic_txq_desc) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_txq_sg_desc) != 128);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_txq_comp) != 16);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_desc) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_sg_desc) != 128);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_comp) != 16);
}

struct ionic_dev {
	union ionic_dev_info_regs __iomem *dev_info;
	union ionic_dev_cmd_regs __iomem *dev_cmd;

	struct ionic_doorbell __iomem *db_pages;
	rte_iova_t phy_db_pages;

	struct ionic_intr __iomem *intr_ctrl;

	struct ionic_intr_status __iomem *intr_status;

	struct ionic_port_info *port_info;
	const struct rte_memzone *port_info_z;
	rte_iova_t port_info_pa;
	uint32_t port_info_sz;
};

int ionic_dev_setup(struct ionic_adapter *adapter);

void ionic_dev_cmd_go(struct ionic_dev *idev, union ionic_dev_cmd *cmd);
uint8_t ionic_dev_cmd_status(struct ionic_dev *idev);
bool ionic_dev_cmd_done(struct ionic_dev *idev);
void ionic_dev_cmd_comp(struct ionic_dev *idev, void *mem);

void ionic_dev_cmd_identify(struct ionic_dev *idev, uint8_t ver);
void ionic_dev_cmd_init(struct ionic_dev *idev);
void ionic_dev_cmd_reset(struct ionic_dev *idev);

void ionic_dev_cmd_port_identify(struct ionic_dev *idev);
void ionic_dev_cmd_port_init(struct ionic_dev *idev);
void ionic_dev_cmd_port_reset(struct ionic_dev *idev);
void ionic_dev_cmd_port_state(struct ionic_dev *idev, uint8_t state);
void ionic_dev_cmd_port_speed(struct ionic_dev *idev, uint32_t speed);
void ionic_dev_cmd_port_mtu(struct ionic_dev *idev, uint32_t mtu);
void ionic_dev_cmd_port_autoneg(struct ionic_dev *idev, uint8_t an_enable);
void ionic_dev_cmd_port_fec(struct ionic_dev *idev, uint8_t fec_type);
void ionic_dev_cmd_port_pause(struct ionic_dev *idev, uint8_t pause_type);
void ionic_dev_cmd_port_loopback(struct ionic_dev *idev,
	uint8_t loopback_mode);

void ionic_dev_cmd_lif_identify(struct ionic_dev *idev, uint8_t type,
	uint8_t ver);
void ionic_dev_cmd_lif_init(struct ionic_dev *idev, uint16_t lif_index,
	rte_iova_t addr);
void ionic_dev_cmd_lif_reset(struct ionic_dev *idev, uint16_t lif_index);

#endif /* _IONIC_DEV_H_ */
