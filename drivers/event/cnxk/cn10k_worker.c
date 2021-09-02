/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

uint16_t __rte_hot
cn10k_sso_hws_enq(void *port, const struct rte_event *ev)
{
	struct cn10k_sso_hws *ws = port;

	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		return cn10k_sso_hws_new_event(ws, ev);
	case RTE_EVENT_OP_FORWARD:
		cn10k_sso_hws_forward_event(ws, ev);
		break;
	case RTE_EVENT_OP_RELEASE:
		cnxk_sso_hws_swtag_flush(ws->base + SSOW_LF_GWS_WQE0,
					 ws->base + SSOW_LF_GWS_OP_SWTAG_FLUSH);
		break;
	default:
		return 0;
	}

	return 1;
}

uint16_t __rte_hot
cn10k_sso_hws_enq_burst(void *port, const struct rte_event ev[],
			uint16_t nb_events)
{
	RTE_SET_USED(nb_events);
	return cn10k_sso_hws_enq(port, ev);
}

uint16_t __rte_hot
cn10k_sso_hws_enq_new_burst(void *port, const struct rte_event ev[],
			    uint16_t nb_events)
{
	struct cn10k_sso_hws *ws = port;
	uint16_t i, rc = 1;

	for (i = 0; i < nb_events && rc; i++)
		rc = cn10k_sso_hws_new_event(ws, &ev[i]);

	return nb_events;
}

uint16_t __rte_hot
cn10k_sso_hws_enq_fwd_burst(void *port, const struct rte_event ev[],
			    uint16_t nb_events)
{
	struct cn10k_sso_hws *ws = port;

	RTE_SET_USED(nb_events);
	cn10k_sso_hws_forward_event(ws, ev);

	return 1;
}

uint16_t __rte_hot
cn10k_sso_hws_ca_enq(void *port, struct rte_event ev[], uint16_t nb_events)
{
	struct cn10k_sso_hws *ws = port;

	RTE_SET_USED(nb_events);

	return cn10k_cpt_crypto_adapter_enqueue(ws->base + SSOW_LF_GWS_TAG,
						ev->event_ptr);
}
