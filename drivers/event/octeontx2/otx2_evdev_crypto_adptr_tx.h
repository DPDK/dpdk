/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#ifndef _OTX2_EVDEV_CRYPTO_ADPTR_TX_H_
#define _OTX2_EVDEV_CRYPTO_ADPTR_TX_H_

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_event_crypto_adapter.h>
#include <rte_eventdev.h>

#include <otx2_cryptodev_qp.h>
#include <otx2_worker.h>

static inline uint16_t
otx2_ca_enq(uintptr_t tag_op, const struct rte_event *ev)
{
	union rte_event_crypto_metadata *m_data;
	struct rte_crypto_op *crypto_op;
	struct rte_cryptodev *cdev;
	struct otx2_cpt_qp *qp;
	uint8_t cdev_id;
	uint16_t qp_id;

	crypto_op = ev->event_ptr;
	if (crypto_op == NULL)
		return 0;

	if (crypto_op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		m_data = rte_cryptodev_sym_session_get_user_data(
						crypto_op->sym->session);
		if (m_data == NULL)
			goto free_op;

		cdev_id = m_data->request_info.cdev_id;
		qp_id = m_data->request_info.queue_pair_id;
	} else if (crypto_op->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
		   crypto_op->private_data_offset) {
		m_data = (union rte_event_crypto_metadata *)
			 ((uint8_t *)crypto_op +
			  crypto_op->private_data_offset);
		cdev_id = m_data->request_info.cdev_id;
		qp_id = m_data->request_info.queue_pair_id;
	} else {
		goto free_op;
	}

	cdev = &rte_cryptodevs[cdev_id];
	qp = cdev->data->queue_pairs[qp_id];

	if (!ev->sched_type)
		otx2_ssogws_head_wait(tag_op);
	if (qp->ca_enable)
		return cdev->enqueue_burst(qp, &crypto_op, 1);

free_op:
	rte_pktmbuf_free(crypto_op->sym->m_src);
	rte_crypto_op_free(crypto_op);
	rte_errno = EINVAL;
	return 0;
}

static uint16_t __rte_hot
otx2_ssogws_ca_enq(void *port, struct rte_event ev[], uint16_t nb_events)
{
	struct otx2_ssogws *ws = port;

	RTE_SET_USED(nb_events);

	return otx2_ca_enq(ws->tag_op, ev);
}

static uint16_t __rte_hot
otx2_ssogws_dual_ca_enq(void *port, struct rte_event ev[], uint16_t nb_events)
{
	struct otx2_ssogws_dual *ws = port;

	RTE_SET_USED(nb_events);

	return otx2_ca_enq(ws->ws_state[!ws->vws].tag_op, ev);
}
#endif /* _OTX2_EVDEV_CRYPTO_ADPTR_TX_H_ */
