/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_RX_H
#define _SFC_RX_H

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev_driver.h>

#include "efx.h"

#include "sfc_dp_rx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_adapter;
struct sfc_evq;

/**
 * Software Rx descriptor information associated with hardware Rx
 * descriptor.
 */
struct sfc_efx_rx_sw_desc {
	struct rte_mbuf		*mbuf;
	unsigned int		flags;
	unsigned int		size;
};

/** Receive queue state bits */
enum sfc_rxq_state_bit {
	SFC_RXQ_INITIALIZED_BIT = 0,
#define SFC_RXQ_INITIALIZED	(1 << SFC_RXQ_INITIALIZED_BIT)
	SFC_RXQ_STARTED_BIT,
#define SFC_RXQ_STARTED		(1 << SFC_RXQ_STARTED_BIT)
	SFC_RXQ_FLUSHING_BIT,
#define SFC_RXQ_FLUSHING	(1 << SFC_RXQ_FLUSHING_BIT)
	SFC_RXQ_FLUSHED_BIT,
#define SFC_RXQ_FLUSHED		(1 << SFC_RXQ_FLUSHED_BIT)
	SFC_RXQ_FLUSH_FAILED_BIT,
#define SFC_RXQ_FLUSH_FAILED	(1 << SFC_RXQ_FLUSH_FAILED_BIT)
};

/**
 * Receive queue control primary process-only information.
 */
struct sfc_rxq {
	struct sfc_evq		*evq;
	efx_rxq_t		*common;
	efsys_mem_t		mem;
	unsigned int		hw_index;
	uint16_t		buf_size;
};

struct sfc_rxq *sfc_rxq_by_dp_rxq(const struct sfc_dp_rxq *dp_rxq);

/**
 * Receive queue information used on libefx-based data path.
 * Allocated on the socket specified on the queue setup.
 */
struct sfc_efx_rxq {
	/* Used on data path */
	struct sfc_evq			*evq;
	unsigned int			flags;
#define SFC_EFX_RXQ_FLAG_STARTED	0x1
#define SFC_EFX_RXQ_FLAG_RUNNING	0x2
#define SFC_EFX_RXQ_FLAG_RSS_HASH	0x4
#define SFC_EFX_RXQ_FLAG_INTR_EN	0x8
	unsigned int			ptr_mask;
	unsigned int			pending;
	unsigned int			completed;
	uint16_t			batch_max;
	uint16_t			prefix_size;
	struct sfc_efx_rx_sw_desc	*sw_desc;

	/* Used on refill */
	unsigned int			added;
	unsigned int			pushed;
	unsigned int			max_fill_level;
	unsigned int			refill_threshold;
	uint16_t			buf_size;
	struct rte_mempool		*refill_mb_pool;
	efx_rxq_t			*common;

	/* Datapath receive queue anchor */
	struct sfc_dp_rxq		dp;
};

static inline struct sfc_efx_rxq *
sfc_efx_rxq_by_dp_rxq(struct sfc_dp_rxq *dp_rxq)
{
	return container_of(dp_rxq, struct sfc_efx_rxq, dp);
}

/**
 * Receive queue information used during setup/release only.
 * Allocated on the same socket as adapter data.
 */
struct sfc_rxq_info {
	unsigned int		state;
	unsigned int		max_entries;
	unsigned int		entries;
	efx_rxq_type_t		type;
	unsigned int		type_flags;
	struct sfc_dp_rxq	*dp;
	boolean_t		deferred_start;
	boolean_t		deferred_started;
	unsigned int		refill_threshold;
	struct rte_mempool	*refill_mb_pool;
	unsigned int		rxq_flags;
};

struct sfc_rxq_info *sfc_rxq_info_by_dp_rxq(const struct sfc_dp_rxq *dp_rxq);

int sfc_rx_configure(struct sfc_adapter *sa);
void sfc_rx_close(struct sfc_adapter *sa);
int sfc_rx_start(struct sfc_adapter *sa);
void sfc_rx_stop(struct sfc_adapter *sa);

int sfc_rx_qinit(struct sfc_adapter *sa, unsigned int rx_queue_id,
		 uint16_t nb_rx_desc, unsigned int socket_id,
		 const struct rte_eth_rxconf *rx_conf,
		 struct rte_mempool *mb_pool);
void sfc_rx_qfini(struct sfc_adapter *sa, unsigned int sw_index);
int sfc_rx_qstart(struct sfc_adapter *sa, unsigned int sw_index);
void sfc_rx_qstop(struct sfc_adapter *sa, unsigned int sw_index);

uint64_t sfc_rx_get_dev_offload_caps(struct sfc_adapter *sa);
uint64_t sfc_rx_get_queue_offload_caps(struct sfc_adapter *sa);

void sfc_rx_qflush_done(struct sfc_rxq_info *rxq_info);
void sfc_rx_qflush_failed(struct sfc_rxq_info *rxq_info);

int sfc_rx_hash_init(struct sfc_adapter *sa);
void sfc_rx_hash_fini(struct sfc_adapter *sa);
int sfc_rx_hf_rte_to_efx(struct sfc_adapter *sa, uint64_t rte,
			 efx_rx_hash_type_t *efx);
uint64_t sfc_rx_hf_efx_to_rte(struct sfc_rss *rss, efx_rx_hash_type_t efx);
boolean_t sfc_rx_check_scatter(size_t pdu, size_t rx_buf_size,
			       uint32_t rx_prefix_size,
			       boolean_t rx_scatter_enabled,
			       uint32_t rx_scatter_max,
			       const char **error);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_RX_H */
