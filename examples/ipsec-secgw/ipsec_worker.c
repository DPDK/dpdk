/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright (C) 2020 Marvell International Ltd.
 */
#include <rte_event_eth_tx_adapter.h>

#include "event_helper.h"
#include "ipsec.h"
#include "ipsec-secgw.h"

static inline void
ipsec_event_pre_forward(struct rte_mbuf *m, unsigned int port_id)
{
	/* Save the destination port in the mbuf */
	m->port = port_id;

	/* Save eth queue for Tx */
	rte_event_eth_tx_adapter_txq_set(m, 0);
}

static inline void
prepare_out_sessions_tbl(struct sa_ctx *sa_out,
		struct rte_security_session **sess_tbl, uint16_t size)
{
	struct rte_ipsec_session *pri_sess;
	struct ipsec_sa *sa;
	uint32_t i;

	if (!sa_out)
		return;

	for (i = 0; i < sa_out->nb_sa; i++) {

		sa = &sa_out->sa[i];
		if (!sa)
			continue;

		pri_sess = ipsec_get_primary_session(sa);
		if (!pri_sess)
			continue;

		if (pri_sess->type !=
			RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {

			RTE_LOG(ERR, IPSEC, "Invalid session type %d\n",
				pri_sess->type);
			continue;
		}

		if (sa->portid >= size) {
			RTE_LOG(ERR, IPSEC,
				"Port id >= than table size %d, %d\n",
				sa->portid, size);
			continue;
		}

		/* Use only first inline session found for a given port */
		if (sess_tbl[sa->portid])
			continue;
		sess_tbl[sa->portid] = pri_sess->security.ses;
	}
}

/*
 * Event mode exposes various operating modes depending on the
 * capabilities of the event device and the operating mode
 * selected.
 */

/* Workers registered */
#define IPSEC_EVENTMODE_WORKERS		1

/*
 * Event mode worker
 * Operating parameters : non-burst - Tx internal port - driver mode
 */
static void
ipsec_wrkr_non_burst_int_port_drv_mode(struct eh_event_link_info *links,
		uint8_t nb_links)
{
	struct rte_security_session *sess_tbl[RTE_MAX_ETHPORTS] = { NULL };
	unsigned int nb_rx = 0;
	struct rte_mbuf *pkt;
	struct rte_event ev;
	uint32_t lcore_id;
	int32_t socket_id;
	int16_t port_id;

	/* Check if we have links registered for this lcore */
	if (nb_links == 0) {
		/* No links registered - exit */
		return;
	}

	/* Get core ID */
	lcore_id = rte_lcore_id();

	/* Get socket ID */
	socket_id = rte_lcore_to_socket_id(lcore_id);

	/*
	 * Prepare security sessions table. In outbound driver mode
	 * we always use first session configured for a given port
	 */
	prepare_out_sessions_tbl(socket_ctx[socket_id].sa_out, sess_tbl,
			RTE_MAX_ETHPORTS);

	RTE_LOG(INFO, IPSEC,
		"Launching event mode worker (non-burst - Tx internal port - "
		"driver mode) on lcore %d\n", lcore_id);

	/* We have valid links */

	/* Check if it's single link */
	if (nb_links != 1) {
		RTE_LOG(INFO, IPSEC,
			"Multiple links not supported. Using first link\n");
	}

	RTE_LOG(INFO, IPSEC, " -- lcoreid=%u event_port_id=%u\n", lcore_id,
			links[0].event_port_id);
	while (!force_quit) {
		/* Read packet from event queues */
		nb_rx = rte_event_dequeue_burst(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,	/* events */
				1,	/* nb_events */
				0	/* timeout_ticks */);

		if (nb_rx == 0)
			continue;

		pkt = ev.mbuf;
		port_id = pkt->port;

		rte_prefetch0(rte_pktmbuf_mtod(pkt, void *));

		/* Process packet */
		ipsec_event_pre_forward(pkt, port_id);

		if (!is_unprotected_port(port_id)) {

			if (unlikely(!sess_tbl[port_id])) {
				rte_pktmbuf_free(pkt);
				continue;
			}

			/* Save security session */
			pkt->udata64 = (uint64_t) sess_tbl[port_id];

			/* Mark the packet for Tx security offload */
			pkt->ol_flags |= PKT_TX_SEC_OFFLOAD;
		}

		/*
		 * Since tx internal port is available, events can be
		 * directly enqueued to the adapter and it would be
		 * internally submitted to the eth device.
		 */
		rte_event_eth_tx_adapter_enqueue(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,	/* events */
				1,	/* nb_events */
				0	/* flags */);
	}
}

static uint8_t
ipsec_eventmode_populate_wrkr_params(struct eh_app_worker_params *wrkrs)
{
	struct eh_app_worker_params *wrkr;
	uint8_t nb_wrkr_param = 0;

	/* Save workers */
	wrkr = wrkrs;

	/* Non-burst - Tx internal port - driver mode */
	wrkr->cap.burst = EH_RX_TYPE_NON_BURST;
	wrkr->cap.tx_internal_port = EH_TX_TYPE_INTERNAL_PORT;
	wrkr->cap.ipsec_mode = EH_IPSEC_MODE_TYPE_DRIVER;
	wrkr->worker_thread = ipsec_wrkr_non_burst_int_port_drv_mode;
	wrkr++;

	return nb_wrkr_param;
}

static void
ipsec_eventmode_worker(struct eh_conf *conf)
{
	struct eh_app_worker_params ipsec_wrkr[IPSEC_EVENTMODE_WORKERS] = {
					{{{0} }, NULL } };
	uint8_t nb_wrkr_param;

	/* Populate l2fwd_wrkr params */
	nb_wrkr_param = ipsec_eventmode_populate_wrkr_params(ipsec_wrkr);

	/*
	 * Launch correct worker after checking
	 * the event device's capabilities.
	 */
	eh_launch_worker(conf, ipsec_wrkr, nb_wrkr_param);
}

int ipsec_launch_one_lcore(void *args)
{
	struct eh_conf *conf;

	conf = (struct eh_conf *)args;

	if (conf->mode == EH_PKT_TRANSFER_MODE_POLL) {
		/* Run in poll mode */
		ipsec_poll_mode_worker();
	} else if (conf->mode == EH_PKT_TRANSFER_MODE_EVENT) {
		/* Run in event mode */
		ipsec_eventmode_worker(conf);
	}
	return 0;
}
