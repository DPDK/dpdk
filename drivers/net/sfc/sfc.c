/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

/* sysconf() */
#include <unistd.h>

#include <rte_errno.h>
#include <rte_alarm.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_kvargs.h"
#include "sfc_tweak.h"


int
sfc_dma_alloc(const struct sfc_adapter *sa, const char *name, uint16_t id,
	      size_t len, int socket_id, efsys_mem_t *esmp)
{
	const struct rte_memzone *mz;

	sfc_log_init(sa, "name=%s id=%u len=%zu socket_id=%d",
		     name, id, len, socket_id);

	mz = rte_eth_dma_zone_reserve(sa->eth_dev, name, id, len,
				      sysconf(_SC_PAGESIZE), socket_id);
	if (mz == NULL) {
		sfc_err(sa, "cannot reserve DMA zone for %s:%u %#x@%d: %s",
			name, (unsigned int)id, (unsigned int)len, socket_id,
			rte_strerror(rte_errno));
		return ENOMEM;
	}

	esmp->esm_addr = mz->iova;
	if (esmp->esm_addr == RTE_BAD_IOVA) {
		(void)rte_memzone_free(mz);
		return EFAULT;
	}

	esmp->esm_mz = mz;
	esmp->esm_base = mz->addr;

	return 0;
}

void
sfc_dma_free(const struct sfc_adapter *sa, efsys_mem_t *esmp)
{
	int rc;

	sfc_log_init(sa, "name=%s", esmp->esm_mz->name);

	rc = rte_memzone_free(esmp->esm_mz);
	if (rc != 0)
		sfc_err(sa, "rte_memzone_free(() failed: %d", rc);

	memset(esmp, 0, sizeof(*esmp));
}

static uint32_t
sfc_phy_cap_from_link_speeds(uint32_t speeds)
{
	uint32_t phy_caps = 0;

	if (~speeds & ETH_LINK_SPEED_FIXED) {
		phy_caps |= (1 << EFX_PHY_CAP_AN);
		/*
		 * If no speeds are specified in the mask, any supported
		 * may be negotiated
		 */
		if (speeds == ETH_LINK_SPEED_AUTONEG)
			phy_caps |=
				(1 << EFX_PHY_CAP_1000FDX) |
				(1 << EFX_PHY_CAP_10000FDX) |
				(1 << EFX_PHY_CAP_25000FDX) |
				(1 << EFX_PHY_CAP_40000FDX) |
				(1 << EFX_PHY_CAP_50000FDX) |
				(1 << EFX_PHY_CAP_100000FDX);
	}
	if (speeds & ETH_LINK_SPEED_1G)
		phy_caps |= (1 << EFX_PHY_CAP_1000FDX);
	if (speeds & ETH_LINK_SPEED_10G)
		phy_caps |= (1 << EFX_PHY_CAP_10000FDX);
	if (speeds & ETH_LINK_SPEED_25G)
		phy_caps |= (1 << EFX_PHY_CAP_25000FDX);
	if (speeds & ETH_LINK_SPEED_40G)
		phy_caps |= (1 << EFX_PHY_CAP_40000FDX);
	if (speeds & ETH_LINK_SPEED_50G)
		phy_caps |= (1 << EFX_PHY_CAP_50000FDX);
	if (speeds & ETH_LINK_SPEED_100G)
		phy_caps |= (1 << EFX_PHY_CAP_100000FDX);

	return phy_caps;
}

/*
 * Check requested device level configuration.
 * Receive and transmit configuration is checked in corresponding
 * modules.
 */
static int
sfc_check_conf(struct sfc_adapter *sa)
{
	const struct rte_eth_conf *conf = &sa->eth_dev->data->dev_conf;
	int rc = 0;

	sa->port.phy_adv_cap =
		sfc_phy_cap_from_link_speeds(conf->link_speeds) &
		sa->port.phy_adv_cap_mask;
	if ((sa->port.phy_adv_cap & ~(1 << EFX_PHY_CAP_AN)) == 0) {
		sfc_err(sa, "No link speeds from mask %#x are supported",
			conf->link_speeds);
		rc = EINVAL;
	}

#if !EFSYS_OPT_LOOPBACK
	if (conf->lpbk_mode != 0) {
		sfc_err(sa, "Loopback not supported");
		rc = EINVAL;
	}
#endif

	if (conf->dcb_capability_en != 0) {
		sfc_err(sa, "Priority-based flow control not supported");
		rc = EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		sfc_err(sa, "Flow Director not supported");
		rc = EINVAL;
	}

	if ((conf->intr_conf.lsc != 0) &&
	    (sa->intr.type != EFX_INTR_LINE) &&
	    (sa->intr.type != EFX_INTR_MESSAGE)) {
		sfc_err(sa, "Link status change interrupt not supported");
		rc = EINVAL;
	}

	if (conf->intr_conf.rxq != 0 &&
	    (sa->priv.dp_rx->features & SFC_DP_RX_FEAT_INTR) == 0) {
		sfc_err(sa, "Receive queue interrupt not supported");
		rc = EINVAL;
	}

	return rc;
}

/*
 * Find out maximum number of receive and transmit queues which could be
 * advertised.
 *
 * NIC is kept initialized on success to allow other modules acquire
 * defaults and capabilities.
 */
static int
sfc_estimate_resource_limits(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	efx_drv_limits_t limits;
	int rc;
	uint32_t evq_allocated;
	uint32_t rxq_allocated;
	uint32_t txq_allocated;

	memset(&limits, 0, sizeof(limits));

	/* Request at least one Rx and Tx queue */
	limits.edl_min_rxq_count = 1;
	limits.edl_min_txq_count = 1;
	/* Management event queue plus event queue for each Tx and Rx queue */
	limits.edl_min_evq_count =
		1 + limits.edl_min_rxq_count + limits.edl_min_txq_count;

	/* Divide by number of functions to guarantee that all functions
	 * will get promised resources
	 */
	/* FIXME Divide by number of functions (not 2) below */
	limits.edl_max_evq_count = encp->enc_evq_limit / 2;
	SFC_ASSERT(limits.edl_max_evq_count >= limits.edl_min_rxq_count);

	/* Split equally between receive and transmit */
	limits.edl_max_rxq_count =
		MIN(encp->enc_rxq_limit, (limits.edl_max_evq_count - 1) / 2);
	SFC_ASSERT(limits.edl_max_rxq_count >= limits.edl_min_rxq_count);

	limits.edl_max_txq_count =
		MIN(encp->enc_txq_limit,
		    limits.edl_max_evq_count - 1 - limits.edl_max_rxq_count);

	if (sa->tso)
		limits.edl_max_txq_count =
			MIN(limits.edl_max_txq_count,
			    encp->enc_fw_assisted_tso_v2_n_contexts /
			    encp->enc_hw_pf_count);

	SFC_ASSERT(limits.edl_max_txq_count >= limits.edl_min_rxq_count);

	/* Configure the minimum required resources needed for the
	 * driver to operate, and the maximum desired resources that the
	 * driver is capable of using.
	 */
	efx_nic_set_drv_limits(sa->nic, &limits);

	sfc_log_init(sa, "init nic");
	rc = efx_nic_init(sa->nic);
	if (rc != 0)
		goto fail_nic_init;

	/* Find resource dimensions assigned by firmware to this function */
	rc = efx_nic_get_vi_pool(sa->nic, &evq_allocated, &rxq_allocated,
				 &txq_allocated);
	if (rc != 0)
		goto fail_get_vi_pool;

	/* It still may allocate more than maximum, ensure limit */
	evq_allocated = MIN(evq_allocated, limits.edl_max_evq_count);
	rxq_allocated = MIN(rxq_allocated, limits.edl_max_rxq_count);
	txq_allocated = MIN(txq_allocated, limits.edl_max_txq_count);

	/* Subtract management EVQ not used for traffic */
	SFC_ASSERT(evq_allocated > 0);
	evq_allocated--;

	/* Right now we use separate EVQ for Rx and Tx */
	sa->rxq_max = MIN(rxq_allocated, evq_allocated / 2);
	sa->txq_max = MIN(txq_allocated, evq_allocated - sa->rxq_max);

	/* Keep NIC initialized */
	return 0;

fail_get_vi_pool:
fail_nic_init:
	efx_nic_fini(sa->nic);
	return rc;
}

static int
sfc_set_drv_limits(struct sfc_adapter *sa)
{
	const struct rte_eth_dev_data *data = sa->eth_dev->data;
	efx_drv_limits_t lim;

	memset(&lim, 0, sizeof(lim));

	/* Limits are strict since take into account initial estimation */
	lim.edl_min_evq_count = lim.edl_max_evq_count =
		1 + data->nb_rx_queues + data->nb_tx_queues;
	lim.edl_min_rxq_count = lim.edl_max_rxq_count = data->nb_rx_queues;
	lim.edl_min_txq_count = lim.edl_max_txq_count = data->nb_tx_queues;

	return efx_nic_set_drv_limits(sa->nic, &lim);
}

static int
sfc_set_fw_subvariant(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	uint64_t tx_offloads = sa->eth_dev->data->dev_conf.txmode.offloads;
	unsigned int txq_index;
	efx_nic_fw_subvariant_t req_fw_subvariant;
	efx_nic_fw_subvariant_t cur_fw_subvariant;
	int rc;

	if (!encp->enc_fw_subvariant_no_tx_csum_supported) {
		sfc_info(sa, "no-Tx-checksum subvariant not supported");
		return 0;
	}

	for (txq_index = 0; txq_index < sas->txq_count; ++txq_index) {
		struct sfc_txq_info *txq_info = &sas->txq_info[txq_index];

		if (txq_info->state & SFC_TXQ_INITIALIZED)
			tx_offloads |= txq_info->offloads;
	}

	if (tx_offloads & (DEV_TX_OFFLOAD_IPV4_CKSUM |
			   DEV_TX_OFFLOAD_TCP_CKSUM |
			   DEV_TX_OFFLOAD_UDP_CKSUM |
			   DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM))
		req_fw_subvariant = EFX_NIC_FW_SUBVARIANT_DEFAULT;
	else
		req_fw_subvariant = EFX_NIC_FW_SUBVARIANT_NO_TX_CSUM;

	rc = efx_nic_get_fw_subvariant(sa->nic, &cur_fw_subvariant);
	if (rc != 0) {
		sfc_err(sa, "failed to get FW subvariant: %d", rc);
		return rc;
	}
	sfc_info(sa, "FW subvariant is %u vs required %u",
		 cur_fw_subvariant, req_fw_subvariant);

	if (cur_fw_subvariant == req_fw_subvariant)
		return 0;

	rc = efx_nic_set_fw_subvariant(sa->nic, req_fw_subvariant);
	if (rc != 0) {
		sfc_err(sa, "failed to set FW subvariant %u: %d",
			req_fw_subvariant, rc);
		return rc;
	}
	sfc_info(sa, "FW subvariant set to %u", req_fw_subvariant);

	return 0;
}

static int
sfc_try_start(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(sa->state == SFC_ADAPTER_STARTING);

	sfc_log_init(sa, "set FW subvariant");
	rc = sfc_set_fw_subvariant(sa);
	if (rc != 0)
		goto fail_set_fw_subvariant;

	sfc_log_init(sa, "set resource limits");
	rc = sfc_set_drv_limits(sa);
	if (rc != 0)
		goto fail_set_drv_limits;

	sfc_log_init(sa, "init nic");
	rc = efx_nic_init(sa->nic);
	if (rc != 0)
		goto fail_nic_init;

	encp = efx_nic_cfg_get(sa->nic);

	/*
	 * Refresh (since it may change on NIC reset/restart) a copy of
	 * supported tunnel encapsulations in shared memory to be used
	 * on supported Rx packet type classes get.
	 */
	sa->priv.shared->tunnel_encaps =
		encp->enc_tunnel_encapsulations_supported;

	if (encp->enc_tunnel_encapsulations_supported != 0) {
		sfc_log_init(sa, "apply tunnel config");
		rc = efx_tunnel_reconfigure(sa->nic);
		if (rc != 0)
			goto fail_tunnel_reconfigure;
	}

	rc = sfc_intr_start(sa);
	if (rc != 0)
		goto fail_intr_start;

	rc = sfc_ev_start(sa);
	if (rc != 0)
		goto fail_ev_start;

	rc = sfc_port_start(sa);
	if (rc != 0)
		goto fail_port_start;

	rc = sfc_rx_start(sa);
	if (rc != 0)
		goto fail_rx_start;

	rc = sfc_tx_start(sa);
	if (rc != 0)
		goto fail_tx_start;

	rc = sfc_flow_start(sa);
	if (rc != 0)
		goto fail_flows_insert;

	sfc_log_init(sa, "done");
	return 0;

fail_flows_insert:
	sfc_tx_stop(sa);

fail_tx_start:
	sfc_rx_stop(sa);

fail_rx_start:
	sfc_port_stop(sa);

fail_port_start:
	sfc_ev_stop(sa);

fail_ev_start:
	sfc_intr_stop(sa);

fail_intr_start:
fail_tunnel_reconfigure:
	efx_nic_fini(sa->nic);

fail_nic_init:
fail_set_drv_limits:
fail_set_fw_subvariant:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

int
sfc_start(struct sfc_adapter *sa)
{
	unsigned int start_tries = 3;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	switch (sa->state) {
	case SFC_ADAPTER_CONFIGURED:
		break;
	case SFC_ADAPTER_STARTED:
		sfc_notice(sa, "already started");
		return 0;
	default:
		rc = EINVAL;
		goto fail_bad_state;
	}

	sa->state = SFC_ADAPTER_STARTING;

	do {
		rc = sfc_try_start(sa);
	} while ((--start_tries > 0) &&
		 (rc == EIO || rc == EAGAIN || rc == ENOENT || rc == EINVAL));

	if (rc != 0)
		goto fail_try_start;

	sa->state = SFC_ADAPTER_STARTED;
	sfc_log_init(sa, "done");
	return 0;

fail_try_start:
	sa->state = SFC_ADAPTER_CONFIGURED;
fail_bad_state:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_stop(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	switch (sa->state) {
	case SFC_ADAPTER_STARTED:
		break;
	case SFC_ADAPTER_CONFIGURED:
		sfc_notice(sa, "already stopped");
		return;
	default:
		sfc_err(sa, "stop in unexpected state %u", sa->state);
		SFC_ASSERT(B_FALSE);
		return;
	}

	sa->state = SFC_ADAPTER_STOPPING;

	sfc_flow_stop(sa);
	sfc_tx_stop(sa);
	sfc_rx_stop(sa);
	sfc_port_stop(sa);
	sfc_ev_stop(sa);
	sfc_intr_stop(sa);
	efx_nic_fini(sa->nic);

	sa->state = SFC_ADAPTER_CONFIGURED;
	sfc_log_init(sa, "done");
}

static int
sfc_restart(struct sfc_adapter *sa)
{
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (sa->state != SFC_ADAPTER_STARTED)
		return EINVAL;

	sfc_stop(sa);

	rc = sfc_start(sa);
	if (rc != 0)
		sfc_err(sa, "restart failed");

	return rc;
}

static void
sfc_restart_if_required(void *arg)
{
	struct sfc_adapter *sa = arg;

	/* If restart is scheduled, clear the flag and do it */
	if (rte_atomic32_cmpset((volatile uint32_t *)&sa->restart_required,
				1, 0)) {
		sfc_adapter_lock(sa);
		if (sa->state == SFC_ADAPTER_STARTED)
			(void)sfc_restart(sa);
		sfc_adapter_unlock(sa);
	}
}

void
sfc_schedule_restart(struct sfc_adapter *sa)
{
	int rc;

	/* Schedule restart alarm if it is not scheduled yet */
	if (!rte_atomic32_test_and_set(&sa->restart_required))
		return;

	rc = rte_eal_alarm_set(1, sfc_restart_if_required, sa);
	if (rc == -ENOTSUP)
		sfc_warn(sa, "alarms are not supported, restart is pending");
	else if (rc != 0)
		sfc_err(sa, "cannot arm restart alarm (rc=%d)", rc);
	else
		sfc_notice(sa, "restart scheduled");
}

int
sfc_configure(struct sfc_adapter *sa)
{
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	SFC_ASSERT(sa->state == SFC_ADAPTER_INITIALIZED ||
		   sa->state == SFC_ADAPTER_CONFIGURED);
	sa->state = SFC_ADAPTER_CONFIGURING;

	rc = sfc_check_conf(sa);
	if (rc != 0)
		goto fail_check_conf;

	rc = sfc_intr_configure(sa);
	if (rc != 0)
		goto fail_intr_configure;

	rc = sfc_port_configure(sa);
	if (rc != 0)
		goto fail_port_configure;

	rc = sfc_rx_configure(sa);
	if (rc != 0)
		goto fail_rx_configure;

	rc = sfc_tx_configure(sa);
	if (rc != 0)
		goto fail_tx_configure;

	sa->state = SFC_ADAPTER_CONFIGURED;
	sfc_log_init(sa, "done");
	return 0;

fail_tx_configure:
	sfc_rx_close(sa);

fail_rx_configure:
	sfc_port_close(sa);

fail_port_configure:
	sfc_intr_close(sa);

fail_intr_configure:
fail_check_conf:
	sa->state = SFC_ADAPTER_INITIALIZED;
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_close(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	SFC_ASSERT(sa->state == SFC_ADAPTER_CONFIGURED);
	sa->state = SFC_ADAPTER_CLOSING;

	sfc_tx_close(sa);
	sfc_rx_close(sa);
	sfc_port_close(sa);
	sfc_intr_close(sa);

	sa->state = SFC_ADAPTER_INITIALIZED;
	sfc_log_init(sa, "done");
}

static int
sfc_mem_bar_init(struct sfc_adapter *sa, unsigned int membar)
{
	struct rte_eth_dev *eth_dev = sa->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	efsys_bar_t *ebp = &sa->mem_bar;
	struct rte_mem_resource *res = &pci_dev->mem_resource[membar];

	SFC_BAR_LOCK_INIT(ebp, eth_dev->data->name);
	ebp->esb_rid = membar;
	ebp->esb_dev = pci_dev;
	ebp->esb_base = res->addr;
	return 0;
}

static void
sfc_mem_bar_fini(struct sfc_adapter *sa)
{
	efsys_bar_t *ebp = &sa->mem_bar;

	SFC_BAR_LOCK_DESTROY(ebp);
	memset(ebp, 0, sizeof(*ebp));
}

/*
 * A fixed RSS key which has a property of being symmetric
 * (symmetrical flows are distributed to the same CPU)
 * and also known to give a uniform distribution
 * (a good distribution of traffic between different CPUs)
 */
static const uint8_t default_rss_key[EFX_RSS_KEY_SIZE] = {
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
};

static int
sfc_rss_attach(struct sfc_adapter *sa)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	int rc;

	rc = efx_intr_init(sa->nic, sa->intr.type, NULL);
	if (rc != 0)
		goto fail_intr_init;

	rc = efx_ev_init(sa->nic);
	if (rc != 0)
		goto fail_ev_init;

	rc = efx_rx_init(sa->nic);
	if (rc != 0)
		goto fail_rx_init;

	rc = efx_rx_scale_default_support_get(sa->nic, &rss->context_type);
	if (rc != 0)
		goto fail_scale_support_get;

	rc = efx_rx_hash_default_support_get(sa->nic, &rss->hash_support);
	if (rc != 0)
		goto fail_hash_support_get;

	rc = sfc_rx_hash_init(sa);
	if (rc != 0)
		goto fail_rx_hash_init;

	efx_rx_fini(sa->nic);
	efx_ev_fini(sa->nic);
	efx_intr_fini(sa->nic);

	rte_memcpy(rss->key, default_rss_key, sizeof(rss->key));

	return 0;

fail_rx_hash_init:
fail_hash_support_get:
fail_scale_support_get:
	efx_rx_fini(sa->nic);

fail_rx_init:
	efx_ev_fini(sa->nic);

fail_ev_init:
	efx_intr_fini(sa->nic);

fail_intr_init:
	return rc;
}

static void
sfc_rss_detach(struct sfc_adapter *sa)
{
	sfc_rx_hash_fini(sa);
}

int
sfc_attach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp;
	efx_nic_t *enp = sa->nic;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	efx_mcdi_new_epoch(enp);

	sfc_log_init(sa, "reset nic");
	rc = efx_nic_reset(enp);
	if (rc != 0)
		goto fail_nic_reset;

	/*
	 * Probed NIC is sufficient for tunnel init.
	 * Initialize tunnel support to be able to use libefx
	 * efx_tunnel_config_udp_{add,remove}() in any state and
	 * efx_tunnel_reconfigure() on start up.
	 */
	rc = efx_tunnel_init(enp);
	if (rc != 0)
		goto fail_tunnel_init;

	encp = efx_nic_cfg_get(sa->nic);

	/*
	 * Make a copy of supported tunnel encapsulations in shared
	 * memory to be used on supported Rx packet type classes get.
	 */
	sa->priv.shared->tunnel_encaps =
		encp->enc_tunnel_encapsulations_supported;

	if (sfc_dp_tx_offload_capa(sa->priv.dp_tx) & DEV_TX_OFFLOAD_TCP_TSO) {
		sa->tso = encp->enc_fw_assisted_tso_v2_enabled;
		if (!sa->tso)
			sfc_info(sa, "TSO support isn't available on this adapter");
	}

	if (sa->tso &&
	    (sfc_dp_tx_offload_capa(sa->priv.dp_tx) &
	     (DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
	      DEV_TX_OFFLOAD_GENEVE_TNL_TSO)) != 0) {
		sa->tso_encap = encp->enc_fw_assisted_tso_v2_encap_enabled;
		if (!sa->tso_encap)
			sfc_info(sa, "Encapsulated TSO support isn't available on this adapter");
	}

	sfc_log_init(sa, "estimate resource limits");
	rc = sfc_estimate_resource_limits(sa);
	if (rc != 0)
		goto fail_estimate_rsrc_limits;

	sa->evq_max_entries = encp->enc_evq_max_nevs;
	SFC_ASSERT(rte_is_power_of_2(sa->evq_max_entries));

	sa->evq_min_entries = encp->enc_evq_min_nevs;
	SFC_ASSERT(rte_is_power_of_2(sa->evq_min_entries));

	sa->rxq_max_entries = encp->enc_rxq_max_ndescs;
	SFC_ASSERT(rte_is_power_of_2(sa->rxq_max_entries));

	sa->rxq_min_entries = encp->enc_rxq_min_ndescs;
	SFC_ASSERT(rte_is_power_of_2(sa->rxq_min_entries));

	sa->txq_max_entries = encp->enc_txq_max_ndescs;
	SFC_ASSERT(rte_is_power_of_2(sa->txq_max_entries));

	sa->txq_min_entries = encp->enc_txq_min_ndescs;
	SFC_ASSERT(rte_is_power_of_2(sa->txq_min_entries));

	rc = sfc_intr_attach(sa);
	if (rc != 0)
		goto fail_intr_attach;

	rc = sfc_ev_attach(sa);
	if (rc != 0)
		goto fail_ev_attach;

	rc = sfc_port_attach(sa);
	if (rc != 0)
		goto fail_port_attach;

	rc = sfc_rss_attach(sa);
	if (rc != 0)
		goto fail_rss_attach;

	rc = sfc_filter_attach(sa);
	if (rc != 0)
		goto fail_filter_attach;

	sfc_log_init(sa, "fini nic");
	efx_nic_fini(enp);

	sfc_flow_init(sa);

	sa->state = SFC_ADAPTER_INITIALIZED;

	sfc_log_init(sa, "done");
	return 0;

fail_filter_attach:
	sfc_rss_detach(sa);

fail_rss_attach:
	sfc_port_detach(sa);

fail_port_attach:
	sfc_ev_detach(sa);

fail_ev_attach:
	sfc_intr_detach(sa);

fail_intr_attach:
	efx_nic_fini(sa->nic);

fail_estimate_rsrc_limits:
fail_tunnel_init:
	efx_tunnel_fini(sa->nic);

fail_nic_reset:

	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_detach(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sfc_flow_fini(sa);

	sfc_filter_detach(sa);
	sfc_rss_detach(sa);
	sfc_port_detach(sa);
	sfc_ev_detach(sa);
	sfc_intr_detach(sa);
	efx_tunnel_fini(sa->nic);

	sa->state = SFC_ADAPTER_UNINITIALIZED;
}

static int
sfc_kvarg_fv_variant_handler(__rte_unused const char *key,
			     const char *value_str, void *opaque)
{
	uint32_t *value = opaque;

	if (strcasecmp(value_str, SFC_KVARG_FW_VARIANT_DONT_CARE) == 0)
		*value = EFX_FW_VARIANT_DONT_CARE;
	else if (strcasecmp(value_str, SFC_KVARG_FW_VARIANT_FULL_FEATURED) == 0)
		*value = EFX_FW_VARIANT_FULL_FEATURED;
	else if (strcasecmp(value_str, SFC_KVARG_FW_VARIANT_LOW_LATENCY) == 0)
		*value = EFX_FW_VARIANT_LOW_LATENCY;
	else if (strcasecmp(value_str, SFC_KVARG_FW_VARIANT_PACKED_STREAM) == 0)
		*value = EFX_FW_VARIANT_PACKED_STREAM;
	else if (strcasecmp(value_str, SFC_KVARG_FW_VARIANT_DPDK) == 0)
		*value = EFX_FW_VARIANT_DPDK;
	else
		return -EINVAL;

	return 0;
}

static int
sfc_get_fw_variant(struct sfc_adapter *sa, efx_fw_variant_t *efv)
{
	efx_nic_fw_info_t enfi;
	int rc;

	rc = efx_nic_get_fw_version(sa->nic, &enfi);
	if (rc != 0)
		return rc;
	else if (!enfi.enfi_dpcpu_fw_ids_valid)
		return ENOTSUP;

	/*
	 * Firmware variant can be uniquely identified by the RxDPCPU
	 * firmware id
	 */
	switch (enfi.enfi_rx_dpcpu_fw_id) {
	case EFX_RXDP_FULL_FEATURED_FW_ID:
		*efv = EFX_FW_VARIANT_FULL_FEATURED;
		break;

	case EFX_RXDP_LOW_LATENCY_FW_ID:
		*efv = EFX_FW_VARIANT_LOW_LATENCY;
		break;

	case EFX_RXDP_PACKED_STREAM_FW_ID:
		*efv = EFX_FW_VARIANT_PACKED_STREAM;
		break;

	case EFX_RXDP_DPDK_FW_ID:
		*efv = EFX_FW_VARIANT_DPDK;
		break;

	default:
		/*
		 * Other firmware variants are not considered, since they are
		 * not supported in the device parameters
		 */
		*efv = EFX_FW_VARIANT_DONT_CARE;
		break;
	}

	return 0;
}

static const char *
sfc_fw_variant2str(efx_fw_variant_t efv)
{
	switch (efv) {
	case EFX_RXDP_FULL_FEATURED_FW_ID:
		return SFC_KVARG_FW_VARIANT_FULL_FEATURED;
	case EFX_RXDP_LOW_LATENCY_FW_ID:
		return SFC_KVARG_FW_VARIANT_LOW_LATENCY;
	case EFX_RXDP_PACKED_STREAM_FW_ID:
		return SFC_KVARG_FW_VARIANT_PACKED_STREAM;
	case EFX_RXDP_DPDK_FW_ID:
		return SFC_KVARG_FW_VARIANT_DPDK;
	default:
		return "unknown";
	}
}

static int
sfc_kvarg_rxd_wait_timeout_ns(struct sfc_adapter *sa)
{
	int rc;
	long value;

	value = SFC_RXD_WAIT_TIMEOUT_NS_DEF;

	rc = sfc_kvargs_process(sa, SFC_KVARG_RXD_WAIT_TIMEOUT_NS,
				sfc_kvarg_long_handler, &value);
	if (rc != 0)
		return rc;

	if (value < 0 ||
	    (unsigned long)value > EFX_RXQ_ES_SUPER_BUFFER_HOL_BLOCK_MAX) {
		sfc_err(sa, "wrong '" SFC_KVARG_RXD_WAIT_TIMEOUT_NS "' "
			    "was set (%ld);", value);
		sfc_err(sa, "it must not be less than 0 or greater than %u",
			    EFX_RXQ_ES_SUPER_BUFFER_HOL_BLOCK_MAX);
		return EINVAL;
	}

	sa->rxd_wait_timeout_ns = value;
	return 0;
}

static int
sfc_nic_probe(struct sfc_adapter *sa)
{
	efx_nic_t *enp = sa->nic;
	efx_fw_variant_t preferred_efv;
	efx_fw_variant_t efv;
	int rc;

	preferred_efv = EFX_FW_VARIANT_DONT_CARE;
	rc = sfc_kvargs_process(sa, SFC_KVARG_FW_VARIANT,
				sfc_kvarg_fv_variant_handler,
				&preferred_efv);
	if (rc != 0) {
		sfc_err(sa, "invalid %s parameter value", SFC_KVARG_FW_VARIANT);
		return rc;
	}

	rc = sfc_kvarg_rxd_wait_timeout_ns(sa);
	if (rc != 0)
		return rc;

	rc = efx_nic_probe(enp, preferred_efv);
	if (rc == EACCES) {
		/* Unprivileged functions cannot set FW variant */
		rc = efx_nic_probe(enp, EFX_FW_VARIANT_DONT_CARE);
	}
	if (rc != 0)
		return rc;

	rc = sfc_get_fw_variant(sa, &efv);
	if (rc == ENOTSUP) {
		sfc_warn(sa, "FW variant can not be obtained");
		return 0;
	}
	if (rc != 0)
		return rc;

	/* Check that firmware variant was changed to the requested one */
	if (preferred_efv != EFX_FW_VARIANT_DONT_CARE && preferred_efv != efv) {
		sfc_warn(sa, "FW variant has not changed to the requested %s",
			 sfc_fw_variant2str(preferred_efv));
	}

	sfc_notice(sa, "running FW variant is %s", sfc_fw_variant2str(efv));

	return 0;
}

int
sfc_probe(struct sfc_adapter *sa)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);
	unsigned int membar;
	efx_nic_t *enp;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sa->socket_id = rte_socket_id();
	rte_atomic32_init(&sa->restart_required);

	sfc_log_init(sa, "get family");
	rc = efx_family(pci_dev->id.vendor_id, pci_dev->id.device_id,
			&sa->family, &membar);
	if (rc != 0)
		goto fail_family;
	sfc_log_init(sa, "family is %u, membar is %u", sa->family, membar);

	sfc_log_init(sa, "init mem bar");
	rc = sfc_mem_bar_init(sa, membar);
	if (rc != 0)
		goto fail_mem_bar_init;

	sfc_log_init(sa, "create nic");
	rte_spinlock_init(&sa->nic_lock);
	rc = efx_nic_create(sa->family, (efsys_identifier_t *)sa,
			    &sa->mem_bar, &sa->nic_lock, &enp);
	if (rc != 0)
		goto fail_nic_create;
	sa->nic = enp;

	rc = sfc_mcdi_init(sa);
	if (rc != 0)
		goto fail_mcdi_init;

	sfc_log_init(sa, "probe nic");
	rc = sfc_nic_probe(sa);
	if (rc != 0)
		goto fail_nic_probe;

	sfc_log_init(sa, "done");
	return 0;

fail_nic_probe:
	sfc_mcdi_fini(sa);

fail_mcdi_init:
	sfc_log_init(sa, "destroy nic");
	sa->nic = NULL;
	efx_nic_destroy(enp);

fail_nic_create:
	sfc_mem_bar_fini(sa);

fail_mem_bar_init:
fail_family:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_unprobe(struct sfc_adapter *sa)
{
	efx_nic_t *enp = sa->nic;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sfc_log_init(sa, "unprobe nic");
	efx_nic_unprobe(enp);

	sfc_mcdi_fini(sa);

	/*
	 * Make sure there is no pending alarm to restart since we are
	 * going to free device private which is passed as the callback
	 * opaque data. A new alarm cannot be scheduled since MCDI is
	 * shut down.
	 */
	rte_eal_alarm_cancel(sfc_restart_if_required, sa);

	sfc_log_init(sa, "destroy nic");
	sa->nic = NULL;
	efx_nic_destroy(enp);

	sfc_mem_bar_fini(sa);

	sfc_flow_fini(sa);
	sa->state = SFC_ADAPTER_UNINITIALIZED;
}

uint32_t
sfc_register_logtype(const struct rte_pci_addr *pci_addr,
		     const char *lt_prefix_str, uint32_t ll_default)
{
	size_t lt_prefix_str_size = strlen(lt_prefix_str);
	size_t lt_str_size_max;
	char *lt_str = NULL;
	int ret;

	if (SIZE_MAX - PCI_PRI_STR_SIZE - 1 > lt_prefix_str_size) {
		++lt_prefix_str_size; /* Reserve space for prefix separator */
		lt_str_size_max = lt_prefix_str_size + PCI_PRI_STR_SIZE + 1;
	} else {
		return sfc_logtype_driver;
	}

	lt_str = rte_zmalloc("logtype_str", lt_str_size_max, 0);
	if (lt_str == NULL)
		return sfc_logtype_driver;

	strncpy(lt_str, lt_prefix_str, lt_prefix_str_size);
	lt_str[lt_prefix_str_size - 1] = '.';
	rte_pci_device_name(pci_addr, lt_str + lt_prefix_str_size,
			    lt_str_size_max - lt_prefix_str_size);
	lt_str[lt_str_size_max - 1] = '\0';

	ret = rte_log_register_type_and_pick_level(lt_str, ll_default);
	rte_free(lt_str);

	if (ret < 0)
		return sfc_logtype_driver;

	return ret;
}
