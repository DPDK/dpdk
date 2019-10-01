/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_errno.h>

#include "nitrox_sym_reqmgr.h"
#include "nitrox_logs.h"

struct nitrox_softreq {
	rte_iova_t iova;
};

static void
softreq_init(struct nitrox_softreq *sr, rte_iova_t iova)
{
	memset(sr, 0, sizeof(*sr));
	sr->iova = iova;
}

static void
req_pool_obj_init(__rte_unused struct rte_mempool *mp,
		  __rte_unused void *opaque, void *obj,
		  __rte_unused unsigned int obj_idx)
{
	softreq_init(obj, rte_mempool_virt2iova(obj));
}

struct rte_mempool *
nitrox_sym_req_pool_create(struct rte_cryptodev *cdev, uint32_t nobjs,
			   uint16_t qp_id, int socket_id)
{
	char softreq_pool_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(softreq_pool_name, RTE_RING_NAMESIZE, "%s_sr_%d",
		 cdev->data->name, qp_id);
	mp = rte_mempool_create(softreq_pool_name,
				RTE_ALIGN_MUL_CEIL(nobjs, 64),
				sizeof(struct nitrox_softreq),
				64, 0, NULL, NULL, req_pool_obj_init, NULL,
				socket_id, 0);
	if (unlikely(!mp))
		NITROX_LOG(ERR, "Failed to create req pool, qid %d, err %d\n",
			   qp_id, rte_errno);

	return mp;
}

void
nitrox_sym_req_pool_free(struct rte_mempool *mp)
{
	rte_mempool_free(mp);
}
