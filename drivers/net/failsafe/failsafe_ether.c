/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>

#include "failsafe_private.h"

static int
fs_eth_dev_conf_apply(struct rte_eth_dev *dev,
		struct sub_device *sdev)
{
	struct rte_eth_dev *edev;
	struct rte_vlan_filter_conf *vfc1;
	struct rte_vlan_filter_conf *vfc2;
	uint32_t i;
	int ret;

	edev = ETH(sdev);
	/* RX queue setup */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct rxq *rxq;

		rxq = dev->data->rx_queues[i];
		ret = rte_eth_rx_queue_setup(PORT_ID(sdev), i,
				rxq->info.nb_desc, rxq->socket_id,
				&rxq->info.conf, rxq->info.mp);
		if (ret) {
			ERROR("rx_queue_setup failed");
			return ret;
		}
	}
	/* TX queue setup */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct txq *txq;

		txq = dev->data->tx_queues[i];
		ret = rte_eth_tx_queue_setup(PORT_ID(sdev), i,
				txq->info.nb_desc, txq->socket_id,
				&txq->info.conf);
		if (ret) {
			ERROR("tx_queue_setup failed");
			return ret;
		}
	}
	/* dev_link.link_status */
	if (dev->data->dev_link.link_status !=
	    edev->data->dev_link.link_status) {
		DEBUG("Configuring link_status");
		if (dev->data->dev_link.link_status)
			ret = rte_eth_dev_set_link_up(PORT_ID(sdev));
		else
			ret = rte_eth_dev_set_link_down(PORT_ID(sdev));
		if (ret) {
			ERROR("Failed to apply link_status");
			return ret;
		}
	} else {
		DEBUG("link_status already set");
	}
	/* promiscuous */
	if (dev->data->promiscuous != edev->data->promiscuous) {
		DEBUG("Configuring promiscuous");
		if (dev->data->promiscuous)
			rte_eth_promiscuous_enable(PORT_ID(sdev));
		else
			rte_eth_promiscuous_disable(PORT_ID(sdev));
	} else {
		DEBUG("promiscuous already set");
	}
	/* all_multicast */
	if (dev->data->all_multicast != edev->data->all_multicast) {
		DEBUG("Configuring all_multicast");
		if (dev->data->all_multicast)
			rte_eth_allmulticast_enable(PORT_ID(sdev));
		else
			rte_eth_allmulticast_disable(PORT_ID(sdev));
	} else {
		DEBUG("all_multicast already set");
	}
	/* MTU */
	if (dev->data->mtu != edev->data->mtu) {
		DEBUG("Configuring MTU");
		ret = rte_eth_dev_set_mtu(PORT_ID(sdev), dev->data->mtu);
		if (ret) {
			ERROR("Failed to apply MTU");
			return ret;
		}
	} else {
		DEBUG("MTU already set");
	}
	/* default MAC */
	DEBUG("Configuring default MAC address");
	ret = rte_eth_dev_default_mac_addr_set(PORT_ID(sdev),
			&dev->data->mac_addrs[0]);
	if (ret) {
		ERROR("Setting default MAC address failed");
		return ret;
	}
	/* additional MAC */
	if (PRIV(dev)->nb_mac_addr > 1)
		DEBUG("Configure additional MAC address%s",
			(PRIV(dev)->nb_mac_addr > 2 ? "es" : ""));
	for (i = 1; i < PRIV(dev)->nb_mac_addr; i++) {
		struct ether_addr *ea;

		ea = &dev->data->mac_addrs[i];
		ret = rte_eth_dev_mac_addr_add(PORT_ID(sdev), ea,
				PRIV(dev)->mac_addr_pool[i]);
		if (ret) {
			char ea_fmt[ETHER_ADDR_FMT_SIZE];

			ether_format_addr(ea_fmt, ETHER_ADDR_FMT_SIZE, ea);
			ERROR("Adding MAC address %s failed", ea_fmt);
		}
	}
	/* VLAN filter */
	vfc1 = &dev->data->vlan_filter_conf;
	vfc2 = &edev->data->vlan_filter_conf;
	if (memcmp(vfc1, vfc2, sizeof(struct rte_vlan_filter_conf))) {
		uint64_t vbit;
		uint64_t ids;
		size_t i;
		uint16_t vlan_id;

		DEBUG("Configuring VLAN filter");
		for (i = 0; i < RTE_DIM(vfc1->ids); i++) {
			if (vfc1->ids[i] == 0)
				continue;
			ids = vfc1->ids[i];
			while (ids) {
				vlan_id = 64 * i;
				/* count trailing zeroes */
				vbit = ~ids & (ids - 1);
				/* clear least significant bit set */
				ids ^= (ids ^ (ids - 1)) ^ vbit;
				for (; vbit; vlan_id++)
					vbit >>= 1;
				ret = rte_eth_dev_vlan_filter(
					PORT_ID(sdev), vlan_id, 1);
				if (ret) {
					ERROR("Failed to apply VLAN filter %hu",
						vlan_id);
					return ret;
				}
			}
		}
	} else {
		DEBUG("VLAN filter already set");
	}
	return 0;
}

int
failsafe_eth_dev_state_sync(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	uint32_t inactive;
	int ret;
	uint8_t i;

	if (PRIV(dev)->state < DEV_PARSED)
		return 0;

	ret = failsafe_args_parse_subs(dev);
	if (ret)
		return ret;

	if (PRIV(dev)->state < DEV_PROBED)
		return 0;
	ret = failsafe_eal_init(dev);
	if (ret)
		return ret;
	if (PRIV(dev)->state < DEV_ACTIVE)
		return 0;
	inactive = 0;
	FOREACH_SUBDEV(sdev, i, dev)
		if (sdev->state == DEV_PROBED)
			inactive |= UINT32_C(1) << i;
	ret = dev->dev_ops->dev_configure(dev);
	if (ret)
		return ret;
	FOREACH_SUBDEV(sdev, i, dev) {
		if (inactive & (UINT32_C(1) << i)) {
			ret = fs_eth_dev_conf_apply(dev, sdev);
			if (ret) {
				ERROR("Could not apply configuration to sub_device %d",
				      i);
				/* TODO: disable device */
				return ret;
			}
		}
	}
	/*
	 * If new devices have been configured, check if
	 * the link state has changed.
	 */
	if (inactive)
		dev->dev_ops->link_update(dev, 1);
	if (PRIV(dev)->state < DEV_STARTED)
		return 0;
	ret = dev->dev_ops->dev_start(dev);
	if (ret)
		return ret;
	return 0;
}
