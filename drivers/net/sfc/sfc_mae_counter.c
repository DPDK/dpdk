/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <rte_common.h>

#include "efx.h"

#include "sfc_ev.h"
#include "sfc.h"
#include "sfc_rx.h"
#include "sfc_mae_counter.h"
#include "sfc_service.h"

static uint32_t
sfc_mae_counter_get_service_lcore(struct sfc_adapter *sa)
{
	uint32_t cid;

	cid = sfc_get_service_lcore(sa->socket_id);
	if (cid != RTE_MAX_LCORE)
		return cid;

	if (sa->socket_id != SOCKET_ID_ANY)
		cid = sfc_get_service_lcore(SOCKET_ID_ANY);

	if (cid == RTE_MAX_LCORE) {
		sfc_warn(sa, "failed to get service lcore for counter service");
	} else if (sa->socket_id != SOCKET_ID_ANY) {
		sfc_warn(sa,
			"failed to get service lcore for counter service at socket %d, but got at socket %u",
			sa->socket_id, rte_lcore_to_socket_id(cid));
	}
	return cid;
}

bool
sfc_mae_counter_rxq_required(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);

	if (encp->enc_mae_supported == B_FALSE)
		return false;

	if (sfc_mae_counter_get_service_lcore(sa) == RTE_MAX_LCORE)
		return false;

	return true;
}

int
sfc_mae_counter_rxq_attach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	char name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;
	unsigned int n_elements;
	unsigned int cache_size;
	/* The mempool is internal and private area is not required */
	const uint16_t priv_size = 0;
	const uint16_t data_room_size = RTE_PKTMBUF_HEADROOM +
		SFC_MAE_COUNTER_STREAM_PACKET_SIZE;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return 0;
	}

	/*
	 * At least one element in the ring is always unused to distinguish
	 * between empty and full ring cases.
	 */
	n_elements = SFC_COUNTER_RXQ_RX_DESC_COUNT - 1;

	/*
	 * The cache must have sufficient space to put received buckets
	 * before they're reused on refill.
	 */
	cache_size = rte_align32pow2(SFC_COUNTER_RXQ_REFILL_LEVEL +
				     SFC_MAE_COUNTER_RX_BURST - 1);

	if (snprintf(name, sizeof(name), "counter_rxq-pool-%u", sas->port_id) >=
	    (int)sizeof(name)) {
		sfc_err(sa, "failed: counter RxQ mempool name is too long");
		rc = ENAMETOOLONG;
		goto fail_long_name;
	}

	/*
	 * It could be single-producer single-consumer ring mempool which
	 * requires minimal barriers. However, cache size and refill/burst
	 * policy are aligned, therefore it does not matter which
	 * mempool backend is chosen since backend is unused.
	 */
	mp = rte_pktmbuf_pool_create(name, n_elements, cache_size,
				     priv_size, data_room_size, sa->socket_id);
	if (mp == NULL) {
		sfc_err(sa, "failed to create counter RxQ mempool");
		rc = rte_errno;
		goto fail_mp_create;
	}

	sa->counter_rxq.sw_index = sfc_counters_rxq_sw_index(sas);
	sa->counter_rxq.mp = mp;
	sa->counter_rxq.state |= SFC_COUNTER_RXQ_ATTACHED;

	sfc_log_init(sa, "done");

	return 0;

fail_mp_create:
fail_long_name:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

void
sfc_mae_counter_rxq_detach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_ATTACHED) == 0) {
		sfc_log_init(sa, "counter queue is not attached - skip");
		return;
	}

	rte_mempool_free(sa->counter_rxq.mp);
	sa->counter_rxq.mp = NULL;
	sa->counter_rxq.state &= ~SFC_COUNTER_RXQ_ATTACHED;

	sfc_log_init(sa, "done");
}

int
sfc_mae_counter_rxq_init(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	const struct rte_eth_rxconf rxconf = {
		.rx_free_thresh = SFC_COUNTER_RXQ_REFILL_LEVEL,
		.rx_drop_en = 1,
	};
	uint16_t nb_rx_desc = SFC_COUNTER_RXQ_RX_DESC_COUNT;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return 0;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_ATTACHED) == 0) {
		sfc_log_init(sa, "counter queue is not attached - skip");
		return 0;
	}

	nb_rx_desc = RTE_MIN(nb_rx_desc, sa->rxq_max_entries);
	nb_rx_desc = RTE_MAX(nb_rx_desc, sa->rxq_min_entries);

	rc = sfc_rx_qinit_info(sa, sa->counter_rxq.sw_index,
			       EFX_RXQ_FLAG_USER_MARK);
	if (rc != 0)
		goto fail_counter_rxq_init_info;

	rc = sfc_rx_qinit(sa, sa->counter_rxq.sw_index, nb_rx_desc,
			  sa->socket_id, &rxconf, sa->counter_rxq.mp);
	if (rc != 0) {
		sfc_err(sa, "failed to init counter RxQ");
		goto fail_counter_rxq_init;
	}

	sa->counter_rxq.state |= SFC_COUNTER_RXQ_INITIALIZED;

	sfc_log_init(sa, "done");

	return 0;

fail_counter_rxq_init:
fail_counter_rxq_init_info:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

void
sfc_mae_counter_rxq_fini(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_INITIALIZED) == 0) {
		sfc_log_init(sa, "counter queue is not initialized - skip");
		return;
	}

	sfc_rx_qfini(sa, sa->counter_rxq.sw_index);

	sfc_log_init(sa, "done");
}
