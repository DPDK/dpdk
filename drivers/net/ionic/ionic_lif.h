/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_LIF_H_
#define _IONIC_LIF_H_

#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_ether.h>

#include "ionic_osdep.h"
#include "ionic_dev.h"

#define IONIC_ADMINQ_LENGTH	16	/* must be a power of two */

#define IONIC_QCQ_F_INITED	BIT(0)
#define IONIC_QCQ_F_SG		BIT(1)
#define IONIC_QCQ_F_INTR	BIT(2)

/* Queue / Completion Queue */
struct ionic_qcq {
	struct ionic_queue q;        /**< Queue */
	struct ionic_cq cq;          /**< Completion Queue */
	struct ionic_lif *lif;       /**< LIF */
	struct rte_mempool *mb_pool; /**< mbuf pool to populate the RX ring */
	const struct rte_memzone *base_z;
	void *base;
	rte_iova_t base_pa;
	uint32_t total_size;
	uint32_t flags;
	struct ionic_intr_info intr;
};

#define IONIC_LIF_F_INITED		BIT(0)

#define IONIC_LIF_NAME_MAX_SZ		(32)

struct ionic_lif {
	struct ionic_adapter *adapter;
	struct rte_eth_dev *eth_dev;
	uint16_t port_id;  /**< Device port identifier */
	uint32_t index;
	uint32_t hw_index;
	uint32_t state;
	uint32_t kern_pid;
	rte_spinlock_t adminq_lock;
	rte_spinlock_t adminq_service_lock;
	struct ionic_qcq *adminqcq;
	struct ionic_doorbell __iomem *kern_dbpage;
	char name[IONIC_LIF_NAME_MAX_SZ];
	uint32_t info_sz;
	struct ionic_lif_info *info;
	rte_iova_t info_pa;
	const struct rte_memzone *info_z;
};

int ionic_lif_identify(struct ionic_adapter *adapter);
int ionic_lifs_size(struct ionic_adapter *ionic);

int ionic_lif_alloc(struct ionic_lif *lif);
void ionic_lif_free(struct ionic_lif *lif);

int ionic_lif_init(struct ionic_lif *lif);
void ionic_lif_deinit(struct ionic_lif *lif);

int ionic_lif_start(struct ionic_lif *lif);

int ionic_lif_configure(struct ionic_lif *lif);
void ionic_lif_reset(struct ionic_lif *lif);

int ionic_intr_alloc(struct ionic_lif *lif, struct ionic_intr_info *intr);
void ionic_intr_free(struct ionic_lif *lif, struct ionic_intr_info *intr);

bool ionic_adminq_service(struct ionic_cq *cq, uint32_t cq_desc_index,
	void *cb_arg);
int ionic_qcq_service(struct ionic_qcq *qcq, int budget, ionic_cq_cb cb,
	void *cb_arg);

void ionic_qcq_free(struct ionic_qcq *qcq);

int ionic_qcq_enable(struct ionic_qcq *qcq);
int ionic_qcq_disable(struct ionic_qcq *qcq);

#endif /* _IONIC_LIF_H_ */
