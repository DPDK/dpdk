/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_QP_H_
#define _ZSDA_QP_H_

#include "zsda_qp_common.h"
#include "zsda_device.h"

#define ZSDA_ADMIN_Q_START		0x100
#define ZSDA_ADMIN_Q_STOP		0x100
#define ZSDA_ADMIN_Q_STOP_RESP	0x104
#define ZSDA_ADMIN_Q_CLR		0x108
#define ZSDA_ADMIN_Q_CLR_RESP	0x10C

#define ZSDA_IO_Q_START			0x200
#define ZSDA_IO_Q_STOP			0x200
#define ZSDA_IO_Q_STOP_RESP		0x400
#define ZSDA_IO_Q_CLR			0x600
#define ZSDA_IO_Q_CLR_RESP		0x800

#define ZSDA_ADMIN_WQ			0x40
#define ZSDA_ADMIN_WQ_BASE7		0x5C
#define ZSDA_ADMIN_WQ_CRC		0x5C
#define ZSDA_ADMIN_WQ_VERSION	0x5D
#define ZSDA_ADMIN_WQ_FLAG		0x5E
#define ZSDA_ADMIN_CQ			0x60
#define ZSDA_ADMIN_CQ_BASE7		0x7C
#define ZSDA_ADMIN_CQ_CRC		0x7C
#define ZSDA_ADMIN_CQ_VERSION	0x7D
#define ZSDA_ADMIN_CQ_FLAG		0x7E
#define ZSDA_ADMIN_WQ_TAIL		0x80
#define ZSDA_ADMIN_CQ_HEAD		0x84

#define ZSDA_Q_START		0x1
#define ZSDA_Q_STOP			0x0
#define ZSDA_CLEAR_VALID	0x1
#define ZSDA_CLEAR_INVALID	0x0
#define ZSDA_RESP_VALID		0x1
#define ZSDA_RESP_INVALID	0x0

#define ADMIN_BUF_DATA_LEN		0x1C
#define ADMIN_BUF_TOTAL_LEN		0x20

#define IO_DB_INITIAL_CONFIG	0x1C00
#define SET_CYCLE			0xff
#define SET_HEAD_INTI		0x0


#define ZSDA_TIME_SLEEP_US	100
#define ZSDA_TIME_NUM		500

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

extern struct zsda_num_qps zsda_nb_qps;

int zsda_queue_init(struct zsda_pci_device *zsda_pci_dev);

#endif /* _ZSDA_QP_H_ */
