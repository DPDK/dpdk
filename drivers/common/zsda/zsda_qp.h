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

#define ZSDA_Q_START		0x1
#define ZSDA_Q_STOP			0x0
#define ZSDA_CLEAR_VALID	0x1
#define ZSDA_CLEAR_INVALID	0x0
#define ZSDA_RESP_VALID		0x1
#define ZSDA_RESP_INVALID	0x0

#define ZSDA_TIME_SLEEP_US	100
#define ZSDA_TIME_NUM		500

extern struct zsda_num_qps zsda_nb_qps;

int zsda_queue_init(struct zsda_pci_device *zsda_pci_dev);

#endif /* _ZSDA_QP_H_ */
