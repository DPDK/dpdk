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

/* sysconf() */
#include <unistd.h>

#include <rte_errno.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tx.h"


int
sfc_dma_alloc(const struct sfc_adapter *sa, const char *name, uint16_t id,
	      size_t len, int socket_id, efsys_mem_t *esmp)
{
	const struct rte_memzone *mz;

	sfc_log_init(sa, "name=%s id=%u len=%lu socket_id=%d",
		     name, id, len, socket_id);

	mz = rte_eth_dma_zone_reserve(sa->eth_dev, name, id, len,
				      sysconf(_SC_PAGESIZE), socket_id);
	if (mz == NULL) {
		sfc_err(sa, "cannot reserve DMA zone for %s:%u %#x@%d: %s",
			name, (unsigned int)id, (unsigned int)len, socket_id,
			rte_strerror(rte_errno));
		return ENOMEM;
	}

	esmp->esm_addr = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);
	if (esmp->esm_addr == RTE_BAD_PHYS_ADDR) {
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
				(1 << EFX_PHY_CAP_40000FDX);
	}
	if (speeds & ETH_LINK_SPEED_1G)
		phy_caps |= (1 << EFX_PHY_CAP_1000FDX);
	if (speeds & ETH_LINK_SPEED_10G)
		phy_caps |= (1 << EFX_PHY_CAP_10000FDX);
	if (speeds & ETH_LINK_SPEED_40G)
		phy_caps |= (1 << EFX_PHY_CAP_40000FDX);

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

	if (conf->lpbk_mode != 0) {
		sfc_err(sa, "Loopback not supported");
		rc = EINVAL;
	}

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

	if (conf->intr_conf.rxq != 0) {
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

int
sfc_start(struct sfc_adapter *sa)
{
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	switch (sa->state) {
	case SFC_ADAPTER_CONFIGURED:
		break;
	case SFC_ADAPTER_STARTED:
		sfc_info(sa, "already started");
		return 0;
	default:
		rc = EINVAL;
		goto fail_bad_state;
	}

	sa->state = SFC_ADAPTER_STARTING;

	sfc_log_init(sa, "set resource limits");
	rc = sfc_set_drv_limits(sa);
	if (rc != 0)
		goto fail_set_drv_limits;

	sfc_log_init(sa, "init nic");
	rc = efx_nic_init(sa->nic);
	if (rc != 0)
		goto fail_nic_init;

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

	sa->state = SFC_ADAPTER_STARTED;
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
	efx_nic_fini(sa->nic);

fail_nic_init:
fail_set_drv_limits:
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
		sfc_info(sa, "already stopped");
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
sfc_mem_bar_init(struct sfc_adapter *sa)
{
	struct rte_eth_dev *eth_dev = sa->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	efsys_bar_t *ebp = &sa->mem_bar;
	unsigned int i;
	struct rte_mem_resource *res;

	for (i = 0; i < RTE_DIM(pci_dev->mem_resource); i++) {
		res = &pci_dev->mem_resource[i];
		if ((res->len != 0) && (res->phys_addr != 0)) {
			/* Found first memory BAR */
			SFC_BAR_LOCK_INIT(ebp, eth_dev->data->name);
			ebp->esb_rid = i;
			ebp->esb_dev = pci_dev;
			ebp->esb_base = res->addr;
			return 0;
		}
	}

	return EFAULT;
}

static void
sfc_mem_bar_fini(struct sfc_adapter *sa)
{
	efsys_bar_t *ebp = &sa->mem_bar;

	SFC_BAR_LOCK_DESTROY(ebp);
	memset(ebp, 0, sizeof(*ebp));
}

#if EFSYS_OPT_RX_SCALE
/*
 * A fixed RSS key which has a property of being symmetric
 * (symmetrical flows are distributed to the same CPU)
 * and also known to give a uniform distribution
 * (a good distribution of traffic between different CPUs)
 */
static const uint8_t default_rss_key[SFC_RSS_KEY_SIZE] = {
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
	0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
};
#endif

static int
sfc_set_rss_defaults(struct sfc_adapter *sa)
{
#if EFSYS_OPT_RX_SCALE
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

	rc = efx_rx_scale_support_get(sa->nic, &sa->rss_support);
	if (rc != 0)
		goto fail_scale_support_get;

	rc = efx_rx_hash_support_get(sa->nic, &sa->hash_support);
	if (rc != 0)
		goto fail_hash_support_get;

	efx_rx_fini(sa->nic);
	efx_ev_fini(sa->nic);
	efx_intr_fini(sa->nic);

	sa->rss_hash_types = sfc_rte_to_efx_hash_type(SFC_RSS_OFFLOADS);

	rte_memcpy(sa->rss_key, default_rss_key, sizeof(sa->rss_key));

	return 0;

fail_hash_support_get:
fail_scale_support_get:
fail_rx_init:
	efx_ev_fini(sa->nic);

fail_ev_init:
	efx_intr_fini(sa->nic);

fail_intr_init:
	return rc;
#else
	return 0;
#endif
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

	encp = efx_nic_cfg_get(sa->nic);

	if (sa->dp_tx->features & SFC_DP_TX_FEAT_TSO) {
		sa->tso = encp->enc_fw_assisted_tso_v2_enabled;
		if (!sa->tso)
			sfc_warn(sa,
				 "TSO support isn't available on this adapter");
	}

	sfc_log_init(sa, "estimate resource limits");
	rc = sfc_estimate_resource_limits(sa);
	if (rc != 0)
		goto fail_estimate_rsrc_limits;

	sa->txq_max_entries = encp->enc_txq_max_ndescs;
	SFC_ASSERT(rte_is_power_of_2(sa->txq_max_entries));

	rc = sfc_intr_attach(sa);
	if (rc != 0)
		goto fail_intr_attach;

	rc = sfc_ev_attach(sa);
	if (rc != 0)
		goto fail_ev_attach;

	rc = sfc_port_attach(sa);
	if (rc != 0)
		goto fail_port_attach;

	rc = sfc_set_rss_defaults(sa);
	if (rc != 0)
		goto fail_set_rss_defaults;

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
fail_set_rss_defaults:
	sfc_port_detach(sa);

fail_port_attach:
	sfc_ev_detach(sa);

fail_ev_attach:
	sfc_intr_detach(sa);

fail_intr_attach:
	efx_nic_fini(sa->nic);

fail_estimate_rsrc_limits:
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
	sfc_port_detach(sa);
	sfc_ev_detach(sa);
	sfc_intr_detach(sa);

	sa->state = SFC_ADAPTER_UNINITIALIZED;
}

int
sfc_probe(struct sfc_adapter *sa)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);
	efx_nic_t *enp;
	int rc;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sa->socket_id = rte_socket_id();

	sfc_log_init(sa, "init mem bar");
	rc = sfc_mem_bar_init(sa);
	if (rc != 0)
		goto fail_mem_bar_init;

	sfc_log_init(sa, "get family");
	rc = efx_family(pci_dev->id.vendor_id, pci_dev->id.device_id,
			&sa->family);
	if (rc != 0)
		goto fail_family;
	sfc_log_init(sa, "family is %u", sa->family);

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
	rc = efx_nic_probe(enp);
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
fail_family:
	sfc_mem_bar_fini(sa);

fail_mem_bar_init:
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

	sfc_log_init(sa, "destroy nic");
	sa->nic = NULL;
	efx_nic_destroy(enp);

	sfc_mem_bar_fini(sa);

	sfc_flow_fini(sa);
	sa->state = SFC_ADAPTER_UNINITIALIZED;
}
