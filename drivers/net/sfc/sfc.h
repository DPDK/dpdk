/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_H
#define _SFC_H

#include <stdbool.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_kvargs.h>
#include <rte_spinlock.h>
#include <rte_atomic.h>

#include "efx.h"

#include "sfc_filter.h"

#ifdef __cplusplus
extern "C" {
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
	uint32_t			logtype;
	uint32_t			proxy_handle;
	efx_rc_t			proxy_result;
};

struct sfc_intr {
	efx_intr_type_t			type;
	rte_intr_callback_fn		handler;
	boolean_t			lsc_intr;
};

struct sfc_rxq;
struct sfc_txq;

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

	/*
	 * Flow API isolated mode overrides promisc and allmulti settings;
	 * they won't be applied if isolated mode is active
	 */
	boolean_t			promisc;
	boolean_t			allmulti;

	struct ether_addr		default_mac_addr;

	unsigned int			max_mcast_addrs;
	unsigned int			nb_mcast_addrs;
	uint8_t				*mcast_addrs;

	rte_spinlock_t			mac_stats_lock;
	uint64_t			*mac_stats_buf;
	unsigned int			mac_stats_nb_supported;
	efsys_mem_t			mac_stats_dma_mem;
	boolean_t			mac_stats_reset_pending;
	uint16_t			mac_stats_update_period_ms;
	uint32_t			mac_stats_update_generation;
	boolean_t			mac_stats_periodic_dma_supported;
	uint64_t			mac_stats_last_request_timestamp;

	uint32_t		mac_stats_mask[EFX_MAC_STATS_MASK_NPAGES];

	uint64_t			ipackets;
};

struct sfc_rss_hf_rte_to_efx {
	uint64_t			rte;
	efx_rx_hash_type_t		efx;
};

struct sfc_rss {
	unsigned int			channels;
	efx_rx_scale_context_type_t	context_type;
	efx_rx_hash_support_t		hash_support;
	efx_rx_hash_alg_t		hash_alg;
	unsigned int			hf_map_nb_entries;
	struct sfc_rss_hf_rte_to_efx	*hf_map;

	efx_rx_hash_type_t		hash_types;
	unsigned int			tbl[EFX_RSS_TBL_SIZE];
	uint8_t				key[EFX_RSS_KEY_SIZE];
};

/* Adapter private data shared by primary and secondary processes */
struct sfc_adapter_shared {
	unsigned int			rxq_count;
	struct sfc_rxq_info		*rxq_info;

	unsigned int			txq_count;
	struct sfc_txq_info		*txq_info;

	struct sfc_rss			rss;

	boolean_t			isolated;
	uint32_t			tunnel_encaps;

	struct rte_pci_addr		pci_addr;
	uint16_t			port_id;

	char				*dp_rx_name;
	char				*dp_tx_name;
};

/* Adapter process private data */
struct sfc_adapter_priv {
	struct sfc_adapter_shared	*shared;
	const struct sfc_dp_rx		*dp_rx;
	const struct sfc_dp_tx		*dp_tx;
	uint32_t			logtype_main;
};

static inline struct sfc_adapter_priv *
sfc_adapter_priv_by_eth_dev(struct rte_eth_dev *eth_dev)
{
	struct sfc_adapter_priv *sap = eth_dev->process_private;

	SFC_ASSERT(sap != NULL);
	return sap;
}

/* Adapter private data */
struct sfc_adapter {
	/*
	 * It must be the first field of the sfc_adapter structure since
	 * sfc_adapter is the primary process private data (i.e.  process
	 * private data plus additional primary process specific data).
	 */
	struct sfc_adapter_priv		priv;

	/*
	 * PMD setup and configuration is not thread safe. Since it is not
	 * performance sensitive, it is better to guarantee thread-safety
	 * and add device level lock. Adapter control operations which
	 * change its state should acquire the lock.
	 */
	rte_spinlock_t			lock;
	enum sfc_adapter_state		state;
	struct rte_eth_dev		*eth_dev;
	struct rte_kvargs		*kvargs;
	int				socket_id;
	efsys_bar_t			mem_bar;
	efx_family_t			family;
	efx_nic_t			*nic;
	rte_spinlock_t			nic_lock;
	rte_atomic32_t			restart_required;

	struct sfc_mcdi			mcdi;
	struct sfc_intr			intr;
	struct sfc_port			port;
	struct sfc_filter		filter;

	unsigned int			rxq_max;
	unsigned int			txq_max;

	unsigned int			rxq_max_entries;
	unsigned int			rxq_min_entries;

	unsigned int			txq_max_entries;
	unsigned int			txq_min_entries;

	unsigned int			evq_max_entries;
	unsigned int			evq_min_entries;

	uint32_t			evq_flags;
	unsigned int			evq_count;

	unsigned int			mgmt_evq_index;
	/*
	 * The lock is used to serialise management event queue polling
	 * which can be done from different context. Also the lock
	 * guarantees that mgmt_evq_running is preserved while the lock
	 * is held. It is used to serialise polling and start/stop
	 * operations.
	 *
	 * Locks which may be held when the lock is acquired:
	 *  - adapter lock, when:
	 *    - device start/stop to change mgmt_evq_running
	 *    - any control operations in client side MCDI proxy handling to
	 *	poll management event queue waiting for proxy response
	 *  - MCDI lock, when:
	 *    - any control operations in client side MCDI proxy handling to
	 *	poll management event queue waiting for proxy response
	 *
	 * Locks which are acquired with the lock held:
	 *  - nic_lock, when:
	 *    - MC event processing on management event queue polling
	 *	(e.g. MC REBOOT or BADASSERT events)
	 */
	rte_spinlock_t			mgmt_evq_lock;
	bool				mgmt_evq_running;
	struct sfc_evq			*mgmt_evq;

	struct sfc_rxq			*rxq_ctrl;
	struct sfc_txq			*txq_ctrl;

	boolean_t			tso;
	boolean_t			tso_encap;

	uint32_t			rxd_wait_timeout_ns;
};

static inline struct sfc_adapter_shared *
sfc_adapter_shared_by_eth_dev(struct rte_eth_dev *eth_dev)
{
	struct sfc_adapter_shared *sas = eth_dev->data->dev_private;

	return sas;
}

static inline struct sfc_adapter *
sfc_adapter_by_eth_dev(struct rte_eth_dev *eth_dev)
{
	struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(eth_dev);

	SFC_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);

	return container_of(sap, struct sfc_adapter, priv);
}

static inline struct sfc_adapter_shared *
sfc_sa2shared(struct sfc_adapter *sa)
{
	return sa->priv.shared;
}

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

uint32_t sfc_register_logtype(const struct rte_pci_addr *pci_addr,
			      const char *lt_prefix_str,
			      uint32_t ll_default);

int sfc_probe(struct sfc_adapter *sa);
void sfc_unprobe(struct sfc_adapter *sa);
int sfc_attach(struct sfc_adapter *sa);
void sfc_detach(struct sfc_adapter *sa);
int sfc_start(struct sfc_adapter *sa);
void sfc_stop(struct sfc_adapter *sa);

void sfc_schedule_restart(struct sfc_adapter *sa);

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
