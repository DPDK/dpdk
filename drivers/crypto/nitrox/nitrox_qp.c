/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "nitrox_qp.h"
#include "nitrox_hal.h"
#include "nitrox_logs.h"

#define MAX_CMD_QLEN 16384

static int
nitrox_setup_ridq(struct nitrox_qp *qp, int socket_id)
{
	size_t ridq_size = qp->count * sizeof(*qp->ridq);

	qp->ridq = rte_zmalloc_socket("nitrox ridq", ridq_size,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!qp->ridq) {
		NITROX_LOG(ERR, "Failed to create rid queue\n");
		return -ENOMEM;
	}

	return 0;
}

int
nitrox_qp_setup(struct nitrox_qp *qp, uint8_t *bar_addr, const char *dev_name,
		uint32_t nb_descriptors, uint8_t instr_size, int socket_id)
{
	int err;
	uint32_t count;

	RTE_SET_USED(bar_addr);
	RTE_SET_USED(instr_size);
	count = rte_align32pow2(nb_descriptors);
	if (count > MAX_CMD_QLEN) {
		NITROX_LOG(ERR, "%s: Number of descriptors too big %d,"
			   " greater than max queue length %d\n",
			   dev_name, count,
			   MAX_CMD_QLEN);
		return -EINVAL;
	}

	qp->count = count;
	qp->head = qp->tail = 0;
	rte_atomic16_init(&qp->pending_count);
	err = nitrox_setup_ridq(qp, socket_id);
	if (err)
		goto ridq_err;

	return 0;

ridq_err:
	return err;

}

static void
nitrox_release_ridq(struct nitrox_qp *qp)
{
	rte_free(qp->ridq);
}

int
nitrox_qp_release(struct nitrox_qp *qp, uint8_t *bar_addr)
{
	RTE_SET_USED(bar_addr);
	nitrox_release_ridq(qp);
	return 0;
}
