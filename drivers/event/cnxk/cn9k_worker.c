/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"

#include "cn9k_worker.h"

uint16_t __rte_hot
cn9k_sso_hws_enq(void *port, const struct rte_event *ev)
{
	struct cn9k_sso_hws *ws = port;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		return cn9k_sso_hws_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		cn9k_sso_hws_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		cnxk_sso_hws_swtag_flush(ws->tag_op, ws->swtag_flush_op);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __rte_hot
cn9k_sso_hws_enq_burst(void *port, const struct rte_event ev[],
		       uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return cn9k_sso_hws_enq(port, ev);
}

uint16_t __rte_hot
cn9k_sso_hws_enq_new_burst(void *port, const struct rte_event ev[],
			   uint16_t nb_events)
{
	struct cn9k_sso_hws *ws = port;
	uint16_t i, rc = 1;

	for (i = 0; i < nb_events && rc; i++)
		rc = cn9k_sso_hws_new_event(ws, &ev[i]);

	return nb_events;
}

uint16_t __rte_hot
cn9k_sso_hws_enq_fwd_burst(void *port, const struct rte_event ev[],
			   uint16_t nb_events)
{
	struct cn9k_sso_hws *ws = port;

	RTE_SET_USED(nb_events);
	cn9k_sso_hws_forward_event(ws, ev);

	return 1;
}

uint16_t __rte_hot
cn9k_sso_hws_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks)
{
	struct cn9k_sso_hws *ws = port;

	RTE_SET_USED(timeout_ticks);

	if (ws->swtag_req) {
		ws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(ws->tag_op);
		return 1;
	}

	return cn9k_sso_hws_get_work(ws, ev);
}

uint16_t __rte_hot
cn9k_sso_hws_deq_burst(void *port, struct rte_event ev[], uint16_t nb_events,
		       uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return cn9k_sso_hws_deq(port, ev, timeout_ticks);
}

uint16_t __rte_hot
cn9k_sso_hws_tmo_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks)
{
	struct cn9k_sso_hws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	if (ws->swtag_req) {
		ws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(ws->tag_op);
		return ret;
	}

	ret = cn9k_sso_hws_get_work(ws, ev);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = cn9k_sso_hws_get_work(ws, ev);

	return ret;
}

uint16_t __rte_hot
cn9k_sso_hws_tmo_deq_burst(void *port, struct rte_event ev[],
			   uint16_t nb_events, uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return cn9k_sso_hws_tmo_deq(port, ev, timeout_ticks);
}

/* Dual ws ops. */

uint16_t __rte_hot
cn9k_sso_hws_dual_enq(void *port, const struct rte_event *ev)
{
	struct cn9k_sso_hws_dual *dws = port;
	struct cn9k_sso_hws_state *vws;

	vws = &dws->ws_state[!dws->vws];
	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		return cn9k_sso_hws_dual_new_event(dws, ev);
	case RTE_EVENT_OP_FORWARD:
		cn9k_sso_hws_dual_forward_event(dws, vws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		cnxk_sso_hws_swtag_flush(vws->tag_op, vws->swtag_flush_op);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __rte_hot
cn9k_sso_hws_dual_enq_burst(void *port, const struct rte_event ev[],
			    uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return cn9k_sso_hws_dual_enq(port, ev);
}

uint16_t __rte_hot
cn9k_sso_hws_dual_enq_new_burst(void *port, const struct rte_event ev[],
				uint16_t nb_events)
{
	struct cn9k_sso_hws_dual *dws = port;
	uint16_t i, rc = 1;

	for (i = 0; i < nb_events && rc; i++)
		rc = cn9k_sso_hws_dual_new_event(dws, &ev[i]);

	return nb_events;
}

uint16_t __rte_hot
cn9k_sso_hws_dual_enq_fwd_burst(void *port, const struct rte_event ev[],
				uint16_t nb_events)
{
	struct cn9k_sso_hws_dual *dws = port;

	RTE_SET_USED(nb_events);
	cn9k_sso_hws_dual_forward_event(dws, &dws->ws_state[!dws->vws], ev);

	return 1;
}

uint16_t __rte_hot
cn9k_sso_hws_dual_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks)
{
	struct cn9k_sso_hws_dual *dws = port;
	uint16_t gw;

	RTE_SET_USED(timeout_ticks);
	if (dws->swtag_req) {
		dws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(dws->ws_state[!dws->vws].tag_op);
		return 1;
	}

	gw = cn9k_sso_hws_dual_get_work(&dws->ws_state[dws->vws],
					&dws->ws_state[!dws->vws], ev);
	dws->vws = !dws->vws;
	return gw;
}

uint16_t __rte_hot
cn9k_sso_hws_dual_deq_burst(void *port, struct rte_event ev[],
			    uint16_t nb_events, uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return cn9k_sso_hws_dual_deq(port, ev, timeout_ticks);
}

uint16_t __rte_hot
cn9k_sso_hws_dual_tmo_deq(void *port, struct rte_event *ev,
			  uint64_t timeout_ticks)
{
	struct cn9k_sso_hws_dual *dws = port;
	uint16_t ret = 1;
	uint64_t iter;

	if (dws->swtag_req) {
		dws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(dws->ws_state[!dws->vws].tag_op);
		return ret;
	}

	ret = cn9k_sso_hws_dual_get_work(&dws->ws_state[dws->vws],
					 &dws->ws_state[!dws->vws], ev);
	dws->vws = !dws->vws;
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++) {
		ret = cn9k_sso_hws_dual_get_work(&dws->ws_state[dws->vws],
						 &dws->ws_state[!dws->vws], ev);
		dws->vws = !dws->vws;
	}

	return ret;
}

uint16_t __rte_hot
cn9k_sso_hws_dual_tmo_deq_burst(void *port, struct rte_event ev[],
				uint16_t nb_events, uint64_t timeout_ticks)
{
	RTE_SET_USED(nb_events);

	return cn9k_sso_hws_dual_tmo_deq(port, ev, timeout_ticks);
}
