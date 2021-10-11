/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EV_H_
#define _SFC_EV_H_

#include <ethdev_driver.h>

#include "efx.h"

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_adapter;
struct sfc_dp_rxq;
struct sfc_dp_txq;

enum sfc_evq_state {
	SFC_EVQ_UNINITIALIZED = 0,
	SFC_EVQ_INITIALIZED,
	SFC_EVQ_STARTING,
	SFC_EVQ_STARTED,

	SFC_EVQ_NSTATES
};

enum sfc_evq_type {
	SFC_EVQ_TYPE_MGMT = 0,
	SFC_EVQ_TYPE_RX,
	SFC_EVQ_TYPE_TX,

	SFC_EVQ_NTYPES
};

struct sfc_evq {
	/* Used on datapath */
	efx_evq_t			*common;
	const efx_ev_callbacks_t	*callbacks;
	unsigned int			read_ptr;
	unsigned int			read_ptr_primed;
	boolean_t			exception;
	efsys_mem_t			mem;
	struct sfc_dp_rxq		*dp_rxq;
	struct sfc_dp_txq		*dp_txq;

	/* Not used on datapath */
	struct sfc_adapter		*sa;
	unsigned int			evq_index;
	enum sfc_evq_state		init_state;
	enum sfc_evq_type		type;
	unsigned int			entries;
};

static inline sfc_sw_index_t
sfc_mgmt_evq_sw_index(__rte_unused const struct sfc_adapter_shared *sas)
{
	return 0;
}

/* Return the number of Rx queues reserved for driver's internal use */
static inline unsigned int
sfc_nb_reserved_rxq(const struct sfc_adapter_shared *sas)
{
	return sfc_nb_counter_rxq(sas) + sfc_repr_nb_rxq(sas);
}

/* Return the number of Tx queues reserved for driver's internal use */
static inline unsigned int
sfc_nb_txq_reserved(const struct sfc_adapter_shared *sas)
{
	return sfc_repr_nb_txq(sas);
}

static inline unsigned int
sfc_nb_reserved_evq(const struct sfc_adapter_shared *sas)
{
	/* An EvQ is required for each reserved Rx/Tx queue */
	return 1 + sfc_nb_reserved_rxq(sas) + sfc_nb_txq_reserved(sas);
}

/*
 * The mapping functions that return SW index of a specific reserved
 * queue rely on the relative order of reserved queues. Some reserved
 * queues are optional, and if they are disabled or not supported, then
 * the function for that specific reserved queue will return previous
 * valid index of a reserved queue in the dependency chain or
 * SFC_SW_INDEX_INVALID if it is the first reserved queue in the chain.
 * If at least one of the reserved queues in the chain is enabled, then
 * the corresponding function will give valid SW index, even if previous
 * functions in the chain returned SFC_SW_INDEX_INVALID, since this value
 * is one less than the first valid SW index.
 *
 * The dependency mechanism is utilized to avoid regid defines for SW indices
 * for reserved queues and to allow these indices to shrink and make space
 * for ethdev queue indices when some of the reserved queues are disabled.
 */

static inline sfc_sw_index_t
sfc_counters_rxq_sw_index(const struct sfc_adapter_shared *sas)
{
	return sas->counters_rxq_allocated ? 0 : SFC_SW_INDEX_INVALID;
}

static inline sfc_sw_index_t
sfc_repr_rxq_sw_index(const struct sfc_adapter_shared *sas,
		      unsigned int repr_queue_id)
{
	return sfc_counters_rxq_sw_index(sas) + sfc_repr_nb_rxq(sas) +
		repr_queue_id;
}

static inline sfc_sw_index_t
sfc_repr_txq_sw_index(const struct sfc_adapter_shared *sas,
		      unsigned int repr_queue_id)
{
	/* Reserved TxQ for representors is the first reserved TxQ */
	return sfc_repr_available(sas) ? repr_queue_id : SFC_SW_INDEX_INVALID;
}

/*
 * Functions below define event queue to transmit/receive queue and vice
 * versa mapping.
 * SFC_ETHDEV_QID_INVALID is returned when sw_index is converted to
 * ethdev_qid, but sw_index represents a reserved queue for driver's
 * internal use.
 * Own event queue is allocated for management, each Rx and each Tx queue.
 * Zero event queue is used for management events.
 * When counters are supported, one Rx event queue is reserved.
 * When representors are supported, Rx and Tx event queues are reserved.
 * Rx event queues follow reserved event queues.
 * Tx event queues follow Rx event queues.
 */

static inline sfc_ethdev_qid_t
sfc_ethdev_rx_qid_by_rxq_sw_index(struct sfc_adapter_shared *sas,
				  sfc_sw_index_t rxq_sw_index)
{
	if (rxq_sw_index < sfc_nb_reserved_rxq(sas))
		return SFC_ETHDEV_QID_INVALID;

	return rxq_sw_index - sfc_nb_reserved_rxq(sas);
}

static inline sfc_sw_index_t
sfc_rxq_sw_index_by_ethdev_rx_qid(struct sfc_adapter_shared *sas,
				  sfc_ethdev_qid_t ethdev_qid)
{
	return sfc_nb_reserved_rxq(sas) + ethdev_qid;
}

static inline sfc_sw_index_t
sfc_evq_sw_index_by_rxq_sw_index(struct sfc_adapter *sa,
				 sfc_sw_index_t rxq_sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;

	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, rxq_sw_index);
	if (ethdev_qid == SFC_ETHDEV_QID_INVALID) {
		/* One EvQ is reserved for management */
		return 1 + rxq_sw_index;
	}

	return sfc_nb_reserved_evq(sas) + ethdev_qid;
}

static inline sfc_ethdev_qid_t
sfc_ethdev_tx_qid_by_txq_sw_index(struct sfc_adapter_shared *sas,
				  sfc_sw_index_t txq_sw_index)
{
	if (txq_sw_index < sfc_nb_txq_reserved(sas))
		return SFC_ETHDEV_QID_INVALID;

	return txq_sw_index - sfc_nb_txq_reserved(sas);
}

static inline sfc_sw_index_t
sfc_txq_sw_index_by_ethdev_tx_qid(struct sfc_adapter_shared *sas,
				  sfc_ethdev_qid_t ethdev_qid)
{
	return sfc_nb_txq_reserved(sas) + ethdev_qid;
}

static inline sfc_sw_index_t
sfc_evq_sw_index_by_txq_sw_index(struct sfc_adapter *sa,
				 sfc_sw_index_t txq_sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;

	ethdev_qid = sfc_ethdev_tx_qid_by_txq_sw_index(sas, txq_sw_index);
	if (ethdev_qid == SFC_ETHDEV_QID_INVALID) {
		return sfc_nb_reserved_evq(sas) - sfc_nb_txq_reserved(sas) +
			txq_sw_index;
	}

	return sfc_nb_reserved_evq(sas) + sa->eth_dev->data->nb_rx_queues +
		ethdev_qid;
}

int sfc_ev_attach(struct sfc_adapter *sa);
void sfc_ev_detach(struct sfc_adapter *sa);
int sfc_ev_start(struct sfc_adapter *sa);
void sfc_ev_stop(struct sfc_adapter *sa);

int sfc_ev_qinit(struct sfc_adapter *sa,
		 enum sfc_evq_type type, unsigned int type_index,
		 unsigned int entries, int socket_id, struct sfc_evq **evqp);
void sfc_ev_qfini(struct sfc_evq *evq);
int sfc_ev_qstart(struct sfc_evq *evq, unsigned int hw_index);
void sfc_ev_qstop(struct sfc_evq *evq);

int sfc_ev_qprime(struct sfc_evq *evq);
void sfc_ev_qpoll(struct sfc_evq *evq);

void sfc_ev_mgmt_qpoll(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_EV_H_ */
