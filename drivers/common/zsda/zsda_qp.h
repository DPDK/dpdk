/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_QP_H_
#define _ZSDA_QP_H_

#include "zsda_qp_common.h"

#include "zsda_device.h"

#define ZSDA_ADMIN_Q_START	0x100
#define ZSDA_ADMIN_Q_STOP	0x100
#define ZSDA_ADMIN_Q_STOP_RESP	0x104
#define ZSDA_ADMIN_Q_CLR	0x108
#define ZSDA_ADMIN_Q_CLR_RESP	0x10C

#define ZSDA_IO_Q_START		0x200
#define ZSDA_IO_Q_STOP		0x200
#define ZSDA_IO_Q_STOP_RESP	0x400
#define ZSDA_IO_Q_CLR		0x600
#define ZSDA_IO_Q_CLR_RESP	0x800

#define ZSDA_ADMIN_WQ		0x40
#define ZSDA_ADMIN_WQ_BASE7	0x5C
#define ZSDA_ADMIN_WQ_CRC	0x5C
#define ZSDA_ADMIN_WQ_VERSION	0x5D
#define ZSDA_ADMIN_WQ_FLAG	0x5E
#define ZSDA_ADMIN_CQ		0x60
#define ZSDA_ADMIN_CQ_BASE7	0x7C
#define ZSDA_ADMIN_CQ_CRC	0x7C
#define ZSDA_ADMIN_CQ_VERSION	0x7D
#define ZSDA_ADMIN_CQ_FLAG	0x7E
#define ZSDA_ADMIN_WQ_TAIL	0x80
#define ZSDA_ADMIN_CQ_HEAD	0x84

#define ZSDA_Q_START		0x1
#define ZSDA_Q_STOP		0x0
#define ZSDA_CLEAR_VALID	0x1
#define ZSDA_CLEAR_INVALID	0x0
#define ZSDA_RESP_VALID		0x1
#define ZSDA_RESP_INVALID	0x0

#define ADMIN_BUF_DATA_LEN	0x1C
#define ADMIN_BUF_TOTAL_LEN	0x20

#define IO_DB_INITIAL_CONFIG	0x1C00
#define SET_CYCLE		0xff
#define SET_HEAD_INTI		0x0

#define ZSDA_TIME_SLEEP_US	100
#define ZSDA_TIME_NUM		500

#define WQ_CSR_LBASE	0x1000
#define WQ_CSR_UBASE	0x1004
#define CQ_CSR_LBASE	0x1400
#define CQ_CSR_UBASE	0x1404
#define WQ_TAIL		0x1800
#define CQ_HEAD		0x1804

/* CSR write macro */
#define ZSDA_CSR_WR(csrAddr, csrOffset, val)                                   \
	rte_write32(val, (((uint8_t *)csrAddr) + csrOffset))
#define ZSDA_CSR_WC_WR(csrAddr, csrOffset, val)                                \
	rte_write32_wc(val, (((uint8_t *)csrAddr) + csrOffset))

#define ZSDA_CSR_WQ_RING_BASE(csr_base_addr, ring, value)                      \
	do {                                                                   \
		uint32_t l_base = 0, u_base = 0;                               \
		l_base = (uint32_t)(value & 0xFFFFFFFF);                       \
		u_base = (uint32_t)((value & 0xFFFFFFFF00000000ULL) >> 32);    \
		ZSDA_CSR_WR(csr_base_addr, (ring << 3) + WQ_CSR_LBASE,         \
			    l_base);                                           \
		ZSDA_LOG(INFO, "l_basg - offset:0x%x, value:0x%x",             \
			 ((ring << 3) + WQ_CSR_LBASE), l_base);                \
		ZSDA_CSR_WR(csr_base_addr, (ring << 3) + WQ_CSR_UBASE,         \
			    u_base);                                           \
		ZSDA_LOG(INFO, "h_base - offset:0x%x, value:0x%x",             \
			 ((ring << 3) + WQ_CSR_UBASE), u_base);                \
	} while (0)

#define ZSDA_CSR_CQ_RING_BASE(csr_base_addr, ring, value)                      \
	do {                                                                   \
		uint32_t l_base = 0, u_base = 0;                               \
		l_base = (uint32_t)(value & 0xFFFFFFFF);                       \
		u_base = (uint32_t)((value & 0xFFFFFFFF00000000ULL) >> 32);    \
		ZSDA_CSR_WR(csr_base_addr, (ring << 3) + CQ_CSR_LBASE,         \
			    l_base);                                           \
		ZSDA_CSR_WR(csr_base_addr, (ring << 3) + CQ_CSR_UBASE,         \
			    u_base);                                           \
	} while (0)

#define WRITE_CSR_WQ_TAIL(csr_base_addr, ring, value)                          \
	ZSDA_CSR_WC_WR(csr_base_addr, WQ_TAIL + (ring << 3), value)
#define WRITE_CSR_CQ_HEAD(csr_base_addr, ring, value)                          \
	ZSDA_CSR_WC_WR(csr_base_addr, CQ_HEAD + (ring << 3), value)

extern struct zsda_num_qps zsda_nb_qps;

enum zsda_admin_msg_id {
	/* Version information */
	ZSDA_ADMIN_VERSION_REQ = 0,
	ZSDA_ADMIN_VERSION_RESP,
	/* algo type */
	ZSDA_ADMIN_QUEUE_CFG_REQ,
	ZSDA_ADMIN_QUEUE_CFG_RESP,
	/* get cycle */
	ZSDA_ADMIN_QUEUE_CYCLE_REQ,
	ZSDA_ADMIN_QUEUE_CYCLE_RESP,
	/* set cyclr */
	ZSDA_ADMIN_SET_CYCLE_REQ,
	ZSDA_ADMIN_SET_CYCLE_RESP,

	ZSDA_MIG_STATE_WARNING,
	ZSDA_ADMIN_RESERVE,
	/* set close flr register */
	ZSDA_FLR_SET_FUNCTION,
	ZSDA_ADMIN_MSG_VALID,
	ZSDA_ADMIN_INT_TEST
};

struct ring_size {
	uint16_t tx_msg_size;
	uint16_t rx_msg_size;
};

struct zsda_num_qps {
	uint16_t encomp;
	uint16_t decomp;
	uint16_t encrypt;
	uint16_t decrypt;
	uint16_t hash;
};

struct zsda_qp_config {
	enum zsda_service_type service_type;
	const struct zsda_qp_hw_data *hw;
	uint16_t nb_descriptors;
	uint32_t cookie_size;
	int socket_id;
	const char *service_str;
};

struct zsda_op_cookie {
	struct zsda_sgl sgl_src;
	struct zsda_sgl sgl_dst;
	phys_addr_t sgl_src_phys_addr;
	phys_addr_t sgl_dst_phys_addr;
	phys_addr_t comp_head_phys_addr;
	uint8_t comp_head[COMP_REMOVE_SPACE_LEN];
	uint16_t sid;
	bool used;
	bool decomp_no_tail;
	void *op;
};

struct task_queue_info {
	uint16_t qp_id;
	uint16_t nb_des;
	int socket_id;
	enum zsda_service_type type;
	const char *service_str;

	rx_callback rx_cb;
	tx_callback tx_cb;
	srv_match match;
};

extern struct zsda_num_qps zsda_nb_qps;

int zsda_queue_start(const struct rte_pci_device *pci_dev);
int zsda_queue_stop(const struct rte_pci_device *pci_dev);

int zsda_queue_init(struct zsda_pci_device *zsda_pci_dev);

struct zsda_qp_hw *
zsda_qps_hw_per_service(struct zsda_pci_device *zsda_pci_dev,
			const enum zsda_service_type service);

int zsda_task_queue_setup(struct zsda_pci_device *zsda_pci_dev,
			struct zsda_qp *qp, struct task_queue_info *task_q_info);

uint16_t zsda_enqueue_burst(struct zsda_qp *qp, void **ops, const uint16_t nb_ops);
uint16_t zsda_dequeue_burst(struct zsda_qp *qp, void **ops, const uint16_t nb_ops);

#endif /* _ZSDA_QP_H_ */
