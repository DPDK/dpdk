/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SFC_H
#define _SFC_H

#include <stdbool.h>

#include <rte_ethdev.h>
#include <rte_kvargs.h>
#include <rte_spinlock.h>

#include "efx.h"

#include "sfc_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#if EFSYS_OPT_RX_SCALE
/** RSS key length (bytes) */
#define SFC_RSS_KEY_SIZE	40
/** RSS hash offloads mask */
#define SFC_RSS_OFFLOADS	(ETH_RSS_IP | ETH_RSS_TCP)
#endif

/*
 * +---------------+
 * | UNINITIALIZED |<-----------+
 * +---------------+		|
 *	|.eth_dev_init		|.eth_dev_uninit
 *	V			|
 * +---------------+------------+
 * |  INITIALIZED  |
 * +---------------+<-----------<---------------+
 *	|.dev_configure		|		|
 *	V			|failed		|
 * +---------------+------------+		|
 * |  CONFIGURING  |				|
 * +---------------+----+			|
 *	|success	|			|
 *	|		|		+---------------+
 *	|		|		|    CLOSING    |
 *	|		|		+---------------+
 *	|		|			^
 *	V		|.dev_configure		|
 * +---------------+----+			|.dev_close
 * |  CONFIGURED   |----------------------------+
 * +---------------+<-----------+
 *	|.dev_start		|
 *	V			|
 * +---------------+		|
 * |   STARTING    |------------^
 * +---------------+ failed	|
 *	|success		|
 *	|		+---------------+
 *	|		|   STOPPING    |
 *	|		+---------------+
 *	|			^
 *	V			|.dev_stop
 * +---------------+------------+
 * |    STARTED    |
 * +---------------+
 */
enum sfc_adapter_state {
	SFC_ADAPTER_UNINITIALIZED = 0,
	SFC_ADAPTER_INITIALIZED,
	SFC_ADAPTER_CONFIGURING,
	SFC_ADAPTER_CONFIGURED,
	SFC_ADAPTER_CLOSING,
	SFC_ADAPTER_STARTING,
	SFC_ADAPTER_STARTED,
	SFC_ADAPTER_STOPPING,

	SFC_ADAPTER_NSTATES
};

enum sfc_dev_filter_mode {
	SFC_DEV_FILTER_MODE_PROMISC = 0,
	SFC_DEV_FILTER_MODE_ALLMULTI,

	SFC_DEV_FILTER_NMODES
};

enum sfc_mcdi_state {
	SFC_MCDI_UNINITIALIZED = 0,
	SFC_MCDI_INITIALIZED,
	SFC_MCDI_BUSY,
	SFC_MCDI_COMPLETED,

	SFC_MCDI_NSTATES
};

struct sfc_mcdi {
	rte_spinlock_t			lock;
	efsys_mem_t			mem;
	enum sfc_mcdi_state		state;
	efx_mcdi_transport_t		transport;
	bool				logging;
	uint32_t			proxy_handle;
	efx_rc_t			proxy_result;
};

struct sfc_intr {
	efx_intr_type_t			type;
	rte_intr_callback_fn		handler;
	boolean_t			lsc_intr;
};

struct sfc_rxq_info;
struct sfc_txq_info;
struct sfc_dp_rx;

struct sfc_port {
	unsigned int			lsc_seq;

	uint32_t			phy_adv_cap_mask;
	uint32_t			phy_adv_cap;

	unsigned int			flow_ctrl;
	boolean_t			flow_ctrl_autoneg;
	size_t				pdu;

	boolean_t			promisc;
	boolean_t			allmulti;

	unsigned int			max_mcast_addrs;
	unsigned int			nb_mcast_addrs;
	uint8_t				*mcast_addrs;

	rte_spinlock_t			mac_stats_lock;
	uint64_t			*mac_stats_buf;
	efsys_mem_t			mac_stats_dma_mem;
	boolean_t			mac_stats_reset_pending;
	uint16_t			mac_stats_update_period_ms;
	uint32_t			mac_stats_update_generation;
	boolean_t			mac_stats_periodic_dma_supported;
	uint64_t			mac_stats_last_request_timestamp;

	uint32_t		mac_stats_mask[EFX_MAC_STATS_MASK_NPAGES];
};

/* Adapter private data */
struct sfc_adapter {
	/*
	 * PMD setup and configuration is not thread safe. Since it is not
	 * performance sensitive, it is better to guarantee thread-safety
	 * and add device level lock. Adapter control operations which
	 * change its state should acquire the lock.
	 */
	rte_spinlock_t			lock;
	enum sfc_adapter_state		state;
	struct rte_pci_addr		pci_addr;
	uint16_t			port_id;
	struct rte_eth_dev		*eth_dev;
	struct rte_kvargs		*kvargs;
	bool				debug_init;
	int				socket_id;
	efsys_bar_t			mem_bar;
	efx_family_t			family;
	efx_nic_t			*nic;
	rte_spinlock_t			nic_lock;

	struct sfc_mcdi			mcdi;
	struct sfc_intr			intr;
	struct sfc_port			port;
	struct sfc_filter		filter;

	unsigned int			rxq_max;
	unsigned int			txq_max;

	unsigned int			txq_max_entries;

	uint32_t			evq_flags;
	unsigned int			evq_count;

	unsigned int			mgmt_evq_index;
	rte_spinlock_t			mgmt_evq_lock;
	struct sfc_evq			*mgmt_evq;

	unsigned int			rxq_count;
	struct sfc_rxq_info		*rxq_info;

	unsigned int			txq_count;
	struct sfc_txq_info		*txq_info;

	boolean_t			tso;

	unsigned int			rss_channels;

#if EFSYS_OPT_RX_SCALE
	efx_rx_scale_support_t		rss_support;
	efx_rx_hash_support_t		hash_support;
	efx_rx_hash_type_t		rss_hash_types;
	unsigned int			rss_tbl[EFX_RSS_TBL_SIZE];
	uint8_t				rss_key[SFC_RSS_KEY_SIZE];
#endif

	/*
	 * Shared memory copy of the Rx datapath name to be used by
	 * the secondary process to find Rx datapath to be used.
	 */
	char				*dp_rx_name;
	const struct sfc_dp_rx		*dp_rx;

	/*
	 * Shared memory copy of the Tx datapath name to be used by
	 * the secondary process to find Rx datapath to be used.
	 */
	char				*dp_tx_name;
	const struct sfc_dp_tx		*dp_tx;
};

/*
 * Add wrapper functions to acquire/release lock to be able to remove or
 * change the lock in one place.
 */

static inline void
sfc_adapter_lock_init(struct sfc_adapter *sa)
{
	rte_spinlock_init(&sa->lock);
}

static inline int
sfc_adapter_is_locked(struct sfc_adapter *sa)
{
	return rte_spinlock_is_locked(&sa->lock);
}

static inline void
sfc_adapter_lock(struct sfc_adapter *sa)
{
	rte_spinlock_lock(&sa->lock);
}

static inline int
sfc_adapter_trylock(struct sfc_adapter *sa)
{
	return rte_spinlock_trylock(&sa->lock);
}

static inline void
sfc_adapter_unlock(struct sfc_adapter *sa)
{
	rte_spinlock_unlock(&sa->lock);
}

static inline void
sfc_adapter_lock_fini(__rte_unused struct sfc_adapter *sa)
{
	/* Just for symmetry of the API */
}

/** Get the number of milliseconds since boot from the default timer */
static inline uint64_t
sfc_get_system_msecs(void)
{
	return rte_get_timer_cycles() * MS_PER_S / rte_get_timer_hz();
}

int sfc_dma_alloc(const struct sfc_adapter *sa, const char *name, uint16_t id,
		  size_t len, int socket_id, efsys_mem_t *esmp);
void sfc_dma_free(const struct sfc_adapter *sa, efsys_mem_t *esmp);

int sfc_probe(struct sfc_adapter *sa);
void sfc_unprobe(struct sfc_adapter *sa);
int sfc_attach(struct sfc_adapter *sa);
void sfc_detach(struct sfc_adapter *sa);
int sfc_start(struct sfc_adapter *sa);
void sfc_stop(struct sfc_adapter *sa);

int sfc_mcdi_init(struct sfc_adapter *sa);
void sfc_mcdi_fini(struct sfc_adapter *sa);

int sfc_configure(struct sfc_adapter *sa);
void sfc_close(struct sfc_adapter *sa);

int sfc_intr_attach(struct sfc_adapter *sa);
void sfc_intr_detach(struct sfc_adapter *sa);
int sfc_intr_configure(struct sfc_adapter *sa);
void sfc_intr_close(struct sfc_adapter *sa);
int sfc_intr_start(struct sfc_adapter *sa);
void sfc_intr_stop(struct sfc_adapter *sa);

int sfc_port_attach(struct sfc_adapter *sa);
void sfc_port_detach(struct sfc_adapter *sa);
int sfc_port_configure(struct sfc_adapter *sa);
void sfc_port_close(struct sfc_adapter *sa);
int sfc_port_start(struct sfc_adapter *sa);
void sfc_port_stop(struct sfc_adapter *sa);
void sfc_port_link_mode_to_info(efx_link_mode_t link_mode,
				struct rte_eth_link *link_info);
int sfc_port_update_mac_stats(struct sfc_adapter *sa);
int sfc_port_reset_mac_stats(struct sfc_adapter *sa);
int sfc_set_rx_mode(struct sfc_adapter *sa);


#ifdef __cplusplus
}
#endif

#endif  /* _SFC_H */
