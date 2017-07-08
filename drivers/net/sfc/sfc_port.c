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

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"
#include "sfc_kvargs.h"

/** Default MAC statistics update period is 1 second */
#define SFC_MAC_STATS_UPDATE_PERIOD_MS_DEF	MS_PER_S

/** The number of microseconds to sleep on attempt to get statistics update */
#define SFC_MAC_STATS_UPDATE_RETRY_INTERVAL_US	10

/** The number of attempts to await arrival of freshly generated statistics */
#define SFC_MAC_STATS_UPDATE_NB_ATTEMPTS	50

/**
 * Update MAC statistics in the buffer.
 *
 * @param	sa	Adapter
 *
 * @return Status code
 * @retval	0	Success
 * @retval	EAGAIN	Try again
 * @retval	ENOMEM	Memory allocation failure
 */
int
sfc_port_update_mac_stats(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;
	efsys_mem_t *esmp = &port->mac_stats_dma_mem;
	uint32_t *genp = NULL;
	uint32_t gen_old;
	unsigned int nb_attempts = 0;
	int rc;

	SFC_ASSERT(rte_spinlock_is_locked(&port->mac_stats_lock));

	if (sa->state != SFC_ADAPTER_STARTED)
		return EINVAL;

	/*
	 * If periodic statistics DMA'ing is off or if not supported,
	 * make a manual request and keep an eye on timer if need be
	 */
	if (!port->mac_stats_periodic_dma_supported ||
	    (port->mac_stats_update_period_ms == 0)) {
		if (port->mac_stats_update_period_ms != 0) {
			uint64_t timestamp = sfc_get_system_msecs();

			if ((timestamp -
			     port->mac_stats_last_request_timestamp) <
			    port->mac_stats_update_period_ms)
				return 0;

			port->mac_stats_last_request_timestamp = timestamp;
		}

		rc = efx_mac_stats_upload(sa->nic, esmp);
		if (rc != 0)
			return rc;

		genp = &port->mac_stats_update_generation;
		gen_old = *genp;
	}

	do {
		if (nb_attempts > 0)
			rte_delay_us(SFC_MAC_STATS_UPDATE_RETRY_INTERVAL_US);

		rc = efx_mac_stats_update(sa->nic, esmp,
					  port->mac_stats_buf, genp);
		if (rc != 0)
			return rc;

	} while ((genp != NULL) && (*genp == gen_old) &&
		 (++nb_attempts < SFC_MAC_STATS_UPDATE_NB_ATTEMPTS));

	return 0;
}

int
sfc_port_reset_mac_stats(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;
	int rc;

	rte_spinlock_lock(&port->mac_stats_lock);
	rc = efx_mac_stats_clear(sa->nic);
	rte_spinlock_unlock(&port->mac_stats_lock);

	return rc;
}

static int
sfc_port_init_dev_link(struct sfc_adapter *sa)
{
	struct rte_eth_link *dev_link = &sa->eth_dev->data->dev_link;
	int rc;
	efx_link_mode_t link_mode;
	struct rte_eth_link current_link;

	rc = efx_port_poll(sa->nic, &link_mode);
	if (rc != 0)
		return rc;

	sfc_port_link_mode_to_info(link_mode, &current_link);

	EFX_STATIC_ASSERT(sizeof(*dev_link) == sizeof(rte_atomic64_t));
	rte_atomic64_set((rte_atomic64_t *)dev_link,
			 *(uint64_t *)&current_link);

	return 0;
}

int
sfc_port_start(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;
	int rc;
	uint32_t phy_adv_cap;
	const uint32_t phy_pause_caps =
		((1u << EFX_PHY_CAP_PAUSE) | (1u << EFX_PHY_CAP_ASYM));
	unsigned int i;

	sfc_log_init(sa, "entry");

	sfc_log_init(sa, "init filters");
	rc = efx_filter_init(sa->nic);
	if (rc != 0)
		goto fail_filter_init;

	sfc_log_init(sa, "init port");
	rc = efx_port_init(sa->nic);
	if (rc != 0)
		goto fail_port_init;

	sfc_log_init(sa, "set flow control to %#x autoneg=%u",
		     port->flow_ctrl, port->flow_ctrl_autoneg);
	rc = efx_mac_fcntl_set(sa->nic, port->flow_ctrl,
			       port->flow_ctrl_autoneg);
	if (rc != 0)
		goto fail_mac_fcntl_set;

	/* Preserve pause capabilities set by above efx_mac_fcntl_set()  */
	efx_phy_adv_cap_get(sa->nic, EFX_PHY_CAP_CURRENT, &phy_adv_cap);
	SFC_ASSERT((port->phy_adv_cap & phy_pause_caps) == 0);
	phy_adv_cap = port->phy_adv_cap | (phy_adv_cap & phy_pause_caps);

	sfc_log_init(sa, "set phy adv caps to %#x", phy_adv_cap);
	rc = efx_phy_adv_cap_set(sa->nic, phy_adv_cap);
	if (rc != 0)
		goto fail_phy_adv_cap_set;

	sfc_log_init(sa, "set MAC PDU %u", (unsigned int)port->pdu);
	rc = efx_mac_pdu_set(sa->nic, port->pdu);
	if (rc != 0)
		goto fail_mac_pdu_set;

	if (!port->isolated) {
		struct ether_addr *mac_addrs = sa->eth_dev->data->mac_addrs;

		sfc_log_init(sa, "set MAC address");
		rc = efx_mac_addr_set(sa->nic, mac_addrs[0].addr_bytes);
		if (rc != 0)
			goto fail_mac_addr_set;

		sfc_log_init(sa, "set MAC filters");
		port->promisc = (sa->eth_dev->data->promiscuous != 0) ?
				B_TRUE : B_FALSE;
		port->allmulti = (sa->eth_dev->data->all_multicast != 0) ?
				 B_TRUE : B_FALSE;
		rc = sfc_set_rx_mode(sa);
		if (rc != 0)
			goto fail_mac_filter_set;

		sfc_log_init(sa, "set multicast address list");
		rc = efx_mac_multicast_list_set(sa->nic, port->mcast_addrs,
						port->nb_mcast_addrs);
		if (rc != 0)
			goto fail_mcast_address_list_set;
	}

	if (port->mac_stats_reset_pending) {
		rc = sfc_port_reset_mac_stats(sa);
		if (rc != 0)
			sfc_err(sa, "statistics reset failed (requested "
				    "before the port was started)");

		port->mac_stats_reset_pending = B_FALSE;
	}

	efx_mac_stats_get_mask(sa->nic, port->mac_stats_mask,
			       sizeof(port->mac_stats_mask));

	for (i = 0, port->mac_stats_nb_supported = 0; i < EFX_MAC_NSTATS; ++i)
		if (EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i))
			port->mac_stats_nb_supported++;

	port->mac_stats_update_generation = 0;

	if (port->mac_stats_update_period_ms != 0) {
		/*
		 * Update MAC stats using periodic DMA;
		 * any positive update interval different from
		 * 1000 ms can be set only on SFN8xxx provided
		 * that FW version is 6.2.1.1033 or higher
		 */
		sfc_log_init(sa, "request MAC stats DMA'ing");
		rc = efx_mac_stats_periodic(sa->nic, &port->mac_stats_dma_mem,
					    port->mac_stats_update_period_ms,
					    B_FALSE);
		if (rc == 0) {
			port->mac_stats_periodic_dma_supported = B_TRUE;
		} else if (rc == EOPNOTSUPP) {
			port->mac_stats_periodic_dma_supported = B_FALSE;
			port->mac_stats_last_request_timestamp = 0;
		} else {
			goto fail_mac_stats_periodic;
		}
	}

	if ((port->mac_stats_update_period_ms != 0) &&
	    port->mac_stats_periodic_dma_supported) {
		/*
		 * Request an explicit MAC stats upload immediately to
		 * preclude bogus figures readback if the user decides
		 * to read stats before periodic DMA is really started
		 */
		rc = efx_mac_stats_upload(sa->nic, &port->mac_stats_dma_mem);
		if (rc != 0)
			goto fail_mac_stats_upload;
	}

	sfc_log_init(sa, "disable MAC drain");
	rc = efx_mac_drain(sa->nic, B_FALSE);
	if (rc != 0)
		goto fail_mac_drain;

	/* Synchronize link status knowledge */
	rc = sfc_port_init_dev_link(sa);
	if (rc != 0)
		goto fail_port_init_dev_link;

	sfc_log_init(sa, "done");
	return 0;

fail_port_init_dev_link:
	(void)efx_mac_drain(sa->nic, B_TRUE);

fail_mac_drain:
	(void)efx_mac_stats_periodic(sa->nic, &port->mac_stats_dma_mem,
				     0, B_FALSE);

fail_mac_stats_upload:
fail_mac_stats_periodic:
fail_mcast_address_list_set:
fail_mac_filter_set:
fail_mac_addr_set:
fail_mac_pdu_set:
fail_phy_adv_cap_set:
fail_mac_fcntl_set:
	efx_port_fini(sa->nic);

fail_port_init:
	efx_filter_fini(sa->nic);

fail_filter_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_port_stop(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");

	efx_mac_drain(sa->nic, B_TRUE);

	(void)efx_mac_stats_periodic(sa->nic, &sa->port.mac_stats_dma_mem,
				     0, B_FALSE);

	efx_port_fini(sa->nic);
	efx_filter_fini(sa->nic);

	sfc_log_init(sa, "done");
}

int
sfc_port_configure(struct sfc_adapter *sa)
{
	const struct rte_eth_dev_data *dev_data = sa->eth_dev->data;
	struct sfc_port *port = &sa->port;

	sfc_log_init(sa, "entry");

	if (dev_data->dev_conf.rxmode.jumbo_frame)
		port->pdu = dev_data->dev_conf.rxmode.max_rx_pkt_len;
	else
		port->pdu = EFX_MAC_PDU(dev_data->mtu);

	return 0;
}

void
sfc_port_close(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");
}

int
sfc_port_attach(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;
	long kvarg_stats_update_period_ms;
	int rc;

	sfc_log_init(sa, "entry");

	efx_phy_adv_cap_get(sa->nic, EFX_PHY_CAP_PERM, &port->phy_adv_cap_mask);

	/* Enable flow control by default */
	port->flow_ctrl = EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE;
	port->flow_ctrl_autoneg = B_TRUE;

	port->max_mcast_addrs = EFX_MAC_MULTICAST_LIST_MAX;
	port->nb_mcast_addrs = 0;
	port->mcast_addrs = rte_calloc_socket("mcast_addr_list_buf",
					      port->max_mcast_addrs,
					      EFX_MAC_ADDR_LEN, 0,
					      sa->socket_id);
	if (port->mcast_addrs == NULL) {
		rc = ENOMEM;
		goto fail_mcast_addr_list_buf_alloc;
	}

	rte_spinlock_init(&port->mac_stats_lock);

	rc = ENOMEM;
	port->mac_stats_buf = rte_calloc_socket("mac_stats_buf", EFX_MAC_NSTATS,
						sizeof(uint64_t), 0,
						sa->socket_id);
	if (port->mac_stats_buf == NULL)
		goto fail_mac_stats_buf_alloc;

	rc = sfc_dma_alloc(sa, "mac_stats", 0, EFX_MAC_STATS_SIZE,
			   sa->socket_id, &port->mac_stats_dma_mem);
	if (rc != 0)
		goto fail_mac_stats_dma_alloc;

	port->mac_stats_reset_pending = B_FALSE;

	kvarg_stats_update_period_ms = SFC_MAC_STATS_UPDATE_PERIOD_MS_DEF;

	rc = sfc_kvargs_process(sa, SFC_KVARG_STATS_UPDATE_PERIOD_MS,
				sfc_kvarg_long_handler,
				&kvarg_stats_update_period_ms);
	if ((rc == 0) &&
	    ((kvarg_stats_update_period_ms < 0) ||
	     (kvarg_stats_update_period_ms > UINT16_MAX))) {
		sfc_err(sa, "wrong '" SFC_KVARG_STATS_UPDATE_PERIOD_MS "' "
			    "was set (%ld);", kvarg_stats_update_period_ms);
		sfc_err(sa, "it must not be less than 0 "
			    "or greater than %" PRIu16, UINT16_MAX);
		rc = EINVAL;
		goto fail_kvarg_stats_update_period_ms;
	} else if (rc != 0) {
		goto fail_kvarg_stats_update_period_ms;
	}

	port->mac_stats_update_period_ms = kvarg_stats_update_period_ms;

	sfc_log_init(sa, "done");
	return 0;

fail_kvarg_stats_update_period_ms:
fail_mac_stats_dma_alloc:
	rte_free(port->mac_stats_buf);
fail_mac_stats_buf_alloc:
fail_mcast_addr_list_buf_alloc:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_port_detach(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;

	sfc_log_init(sa, "entry");

	sfc_dma_free(sa, &port->mac_stats_dma_mem);
	rte_free(port->mac_stats_buf);

	sfc_log_init(sa, "done");
}

int
sfc_set_rx_mode(struct sfc_adapter *sa)
{
	struct sfc_port *port = &sa->port;
	int rc;

	rc = efx_mac_filter_set(sa->nic, port->promisc, B_TRUE,
				port->promisc || port->allmulti, B_TRUE);

	return rc;
}

void
sfc_port_link_mode_to_info(efx_link_mode_t link_mode,
			   struct rte_eth_link *link_info)
{
	SFC_ASSERT(link_mode < EFX_LINK_NMODES);

	memset(link_info, 0, sizeof(*link_info));
	if ((link_mode == EFX_LINK_DOWN) || (link_mode == EFX_LINK_UNKNOWN))
		link_info->link_status = ETH_LINK_DOWN;
	else
		link_info->link_status = ETH_LINK_UP;

	switch (link_mode) {
	case EFX_LINK_10HDX:
		link_info->link_speed  = ETH_SPEED_NUM_10M;
		link_info->link_duplex = ETH_LINK_HALF_DUPLEX;
		break;
	case EFX_LINK_10FDX:
		link_info->link_speed  = ETH_SPEED_NUM_10M;
		link_info->link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case EFX_LINK_100HDX:
		link_info->link_speed  = ETH_SPEED_NUM_100M;
		link_info->link_duplex = ETH_LINK_HALF_DUPLEX;
		break;
	case EFX_LINK_100FDX:
		link_info->link_speed  = ETH_SPEED_NUM_100M;
		link_info->link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case EFX_LINK_1000HDX:
		link_info->link_speed  = ETH_SPEED_NUM_1G;
		link_info->link_duplex = ETH_LINK_HALF_DUPLEX;
		break;
	case EFX_LINK_1000FDX:
		link_info->link_speed  = ETH_SPEED_NUM_1G;
		link_info->link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case EFX_LINK_10000FDX:
		link_info->link_speed  = ETH_SPEED_NUM_10G;
		link_info->link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case EFX_LINK_40000FDX:
		link_info->link_speed  = ETH_SPEED_NUM_40G;
		link_info->link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	default:
		SFC_ASSERT(B_FALSE);
		/* FALLTHROUGH */
	case EFX_LINK_UNKNOWN:
	case EFX_LINK_DOWN:
		link_info->link_speed  = ETH_SPEED_NUM_NONE;
		link_info->link_duplex = 0;
		break;
	}

	link_info->link_autoneg = ETH_LINK_AUTONEG;
}
