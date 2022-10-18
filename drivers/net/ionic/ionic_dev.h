/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_DEV_H_
#define _IONIC_DEV_H_

#include <stdbool.h>

#include "ionic_osdep.h"
#include "ionic_if.h"
#include "ionic_regs.h"

#define VLAN_TAG_SIZE			4

#define IONIC_MIN_MTU			RTE_ETHER_MIN_MTU
#define IONIC_MAX_MTU			9378
#define IONIC_ETH_OVERHEAD		(RTE_ETHER_HDR_LEN + VLAN_TAG_SIZE)

#define IONIC_MAX_RING_DESC		32768
#define IONIC_MIN_RING_DESC		16
#define IONIC_DEF_TXRX_DESC		4096
#define IONIC_DEF_TXRX_BURST		32

#define IONIC_DEVCMD_TIMEOUT		5	/* devcmd_timeout */
#define IONIC_DEVCMD_CHECK_PERIOD_US	10	/* devcmd status chk period */
#define IONIC_DEVCMD_RETRY_WAIT_US	20000

#define IONIC_Q_WDOG_MS			10	/* 10ms */
#define IONIC_Q_WDOG_MAX_MS		5000	/* 5s */
#define IONIC_ADMINQ_WDOG_MS		500	/* 500ms */

#define IONIC_ALIGN			4096

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
	RTE_BUILD_BUG_ON(sizeof(struct ionic_txq_sg_desc_v1) != 256);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_txq_comp) != 16);

	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_desc) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_sg_desc) != 128);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_rxq_comp) != 16);
}

struct ionic_dev {
	union ionic_dev_info_regs __iomem *dev_info;
	union ionic_dev_cmd_regs __iomem *dev_cmd;

	struct ionic_doorbell __iomem *db_pages;
	struct ionic_intr __iomem *intr_ctrl;
	struct ionic_intr_status __iomem *intr_status;

	struct ionic_port_info *port_info;
	const struct rte_memzone *port_info_z;
	rte_iova_t port_info_pa;
	uint32_t port_info_sz;
};

#define Q_NEXT_TO_POST(_q, _n)	(((_q)->head_idx + (_n)) & ((_q)->size_mask))
#define Q_NEXT_TO_SRVC(_q, _n)	(((_q)->tail_idx + (_n)) & ((_q)->size_mask))

#define IONIC_INFO_IDX(_q, _i)	((_i) * (_q)->num_segs)
#define IONIC_INFO_PTR(_q, _i)	(&(_q)->info[IONIC_INFO_IDX((_q), _i)])

struct ionic_queue {
	uint16_t num_descs;
	uint16_t num_segs;
	uint16_t head_idx;
	uint16_t tail_idx;
	uint16_t size_mask;
	uint8_t type;
	uint8_t hw_type;
	void *base;
	void *sg_base;
	struct ionic_doorbell __iomem *db;
	void **info;

	uint32_t index;
	uint32_t hw_index;
	rte_iova_t base_pa;
	rte_iova_t sg_base_pa;
};

#define IONIC_INTR_NONE		(-1)

struct ionic_intr_info {
	int index;
	uint32_t vector;
	struct ionic_intr __iomem *ctrl;
};

struct ionic_cq {
	uint16_t tail_idx;
	uint16_t num_descs;
	uint16_t size_mask;
	bool done_color;
	void *base;
	rte_iova_t base_pa;
};

struct ionic_lif;
struct ionic_adapter;
struct ionic_qcq;
struct rte_mempool;
struct rte_eth_dev;
struct rte_devargs;

struct ionic_dev_intf {
	int  (*setup)(struct ionic_adapter *adapter);
	int  (*devargs)(struct ionic_adapter *adapter,
			struct rte_devargs *devargs);
	void (*copy_bus_info)(struct ionic_adapter *adapter,
			struct rte_eth_dev *eth_dev);
	int  (*configure_intr)(struct ionic_adapter *adapter);
	void (*unconfigure_intr)(struct ionic_adapter *adapter);
	void (*unmap_bars)(struct ionic_adapter *adapter);
};

void ionic_intr_init(struct ionic_dev *idev, struct ionic_intr_info *intr,
	unsigned long index);

const char *ionic_opcode_to_str(enum ionic_cmd_opcode opcode);

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

void ionic_dev_cmd_queue_identify(struct ionic_dev *idev,
	uint16_t lif_type, uint8_t qtype, uint8_t qver);

void ionic_dev_cmd_lif_identify(struct ionic_dev *idev, uint8_t type,
	uint8_t ver);
void ionic_dev_cmd_lif_init(struct ionic_dev *idev, rte_iova_t addr);
void ionic_dev_cmd_lif_reset(struct ionic_dev *idev);

void ionic_dev_cmd_adminq_init(struct ionic_dev *idev, struct ionic_qcq *qcq);

struct ionic_doorbell __iomem *ionic_db_map(struct ionic_lif *lif,
	struct ionic_queue *q);

int ionic_cq_init(struct ionic_cq *cq, uint16_t num_descs);
void ionic_cq_reset(struct ionic_cq *cq);
void ionic_cq_map(struct ionic_cq *cq, void *base, rte_iova_t base_pa);
typedef bool (*ionic_cq_cb)(struct ionic_cq *cq, uint16_t cq_desc_index,
		void *cb_arg);
uint32_t ionic_cq_service(struct ionic_cq *cq, uint32_t work_to_do,
	ionic_cq_cb cb, void *cb_arg);

int ionic_q_init(struct ionic_queue *q, uint32_t index, uint16_t num_descs);
void ionic_q_reset(struct ionic_queue *q);
void ionic_q_map(struct ionic_queue *q, void *base, rte_iova_t base_pa);
void ionic_q_sg_map(struct ionic_queue *q, void *base, rte_iova_t base_pa);

static inline uint16_t
ionic_q_space_avail(struct ionic_queue *q)
{
	uint16_t avail = q->tail_idx;

	if (q->head_idx >= avail)
		avail += q->num_descs - q->head_idx - 1;
	else
		avail -= q->head_idx + 1;

	return avail;
}

static inline void
ionic_q_flush(struct ionic_queue *q)
{
	uint64_t val = IONIC_DBELL_QID(q->hw_index) | q->head_idx;

	rte_write64(rte_cpu_to_le_64(val), q->db);
}

#endif /* _IONIC_DEV_H_ */
