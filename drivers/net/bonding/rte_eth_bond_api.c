/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <string.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_tcp.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_private.h"
#include "rte_eth_bond_8023ad_private.h"

int
check_for_bonded_ethdev(const struct rte_eth_dev *eth_dev)
{
	/* Check valid pointer */
	if (eth_dev->device->driver->name == NULL)
		return -1;

	/* return 0 if driver name matches */
	return eth_dev->device->driver->name != pmd_bond_drv.driver.name;
}

int
valid_bonded_port_id(uint16_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);
	return check_for_bonded_ethdev(&rte_eth_devices[port_id]);
}

int
check_for_master_bonded_ethdev(const struct rte_eth_dev *eth_dev)
{
	int i;
	struct bond_dev_private *internals;

	if (check_for_bonded_ethdev(eth_dev) != 0)
		return 0;

	internals = eth_dev->data->dev_private;

	/* Check if any of slave devices is a bonded device */
	for (i = 0; i < internals->slave_count; i++)
		if (valid_bonded_port_id(internals->slaves[i].port_id) == 0)
			return 1;

	return 0;
}

int
valid_slave_port_id(uint16_t port_id, uint8_t mode)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);

	/* Verify that port_id refers to a non bonded port */
	if (check_for_bonded_ethdev(&rte_eth_devices[port_id]) == 0 &&
			mode == BONDING_MODE_8023AD) {
		RTE_BOND_LOG(ERR, "Cannot add slave to bonded device in 802.3ad"
				" mode as slave is also a bonded device, only "
				"physical devices can be support in this mode.");
		return -1;
	}

	return 0;
}

void
activate_slave(struct rte_eth_dev *eth_dev, uint16_t port_id)
{
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint8_t active_count = internals->active_slave_count;

	if (internals->mode == BONDING_MODE_8023AD)
		bond_mode_8023ad_activate_slave(eth_dev, port_id);

	if (internals->mode == BONDING_MODE_TLB
			|| internals->mode == BONDING_MODE_ALB) {

		internals->tlb_slaves_order[active_count] = port_id;
	}

	RTE_ASSERT(internals->active_slave_count <
			(RTE_DIM(internals->active_slaves) - 1));

	internals->active_slaves[internals->active_slave_count] = port_id;
	internals->active_slave_count++;

	if (internals->mode == BONDING_MODE_TLB)
		bond_tlb_activate_slave(internals);
	if (internals->mode == BONDING_MODE_ALB)
		bond_mode_alb_client_list_upd(eth_dev);
}

void
deactivate_slave(struct rte_eth_dev *eth_dev, uint16_t port_id)
{
	uint16_t slave_pos;
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint16_t active_count = internals->active_slave_count;

	if (internals->mode == BONDING_MODE_8023AD) {
		bond_mode_8023ad_stop(eth_dev);
		bond_mode_8023ad_deactivate_slave(eth_dev, port_id);
	} else if (internals->mode == BONDING_MODE_TLB
			|| internals->mode == BONDING_MODE_ALB)
		bond_tlb_disable(internals);

	slave_pos = find_slave_by_id(internals->active_slaves, active_count,
			port_id);

	/* If slave was not at the end of the list
	 * shift active slaves up active array list */
	if (slave_pos < active_count) {
		active_count--;
		memmove(internals->active_slaves + slave_pos,
				internals->active_slaves + slave_pos + 1,
				(active_count - slave_pos) *
					sizeof(internals->active_slaves[0]));
	}

	RTE_ASSERT(active_count < RTE_DIM(internals->active_slaves));
	internals->active_slave_count = active_count;

	if (eth_dev->data->dev_started) {
		if (internals->mode == BONDING_MODE_8023AD) {
			bond_mode_8023ad_start(eth_dev);
		} else if (internals->mode == BONDING_MODE_TLB) {
			bond_tlb_enable(internals);
		} else if (internals->mode == BONDING_MODE_ALB) {
			bond_tlb_enable(internals);
			bond_mode_alb_client_list_upd(eth_dev);
		}
	}
}

int
rte_eth_bond_create(const char *name, uint8_t mode, uint8_t socket_id)
{
	struct bond_dev_private *internals;
	char devargs[52];
	uint16_t port_id;
	int ret;

	if (name == NULL) {
		RTE_BOND_LOG(ERR, "Invalid name specified");
		return -EINVAL;
	}

	ret = snprintf(devargs, sizeof(devargs),
		"driver=net_bonding,mode=%d,socket_id=%d", mode, socket_id);
	if (ret < 0 || ret >= (int)sizeof(devargs))
		return -ENOMEM;

	ret = rte_vdev_init(name, devargs);
	if (ret)
		return -ENOMEM;

	ret = rte_eth_dev_get_port_by_name(name, &port_id);
	RTE_ASSERT(!ret);

	/*
	 * To make bond_ethdev_configure() happy we need to free the
	 * internals->kvlist here.
	 *
	 * Also see comment in bond_ethdev_configure().
	 */
	internals = rte_eth_devices[port_id].data->dev_private;
	rte_kvargs_free(internals->kvlist);
	internals->kvlist = NULL;

	return port_id;
}

int
rte_eth_bond_free(const char *name)
{
	return rte_vdev_uninit(name);
}

static int
slave_vlan_filter_set(uint16_t bonded_port_id, uint16_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;
	int found;
	int res = 0;
	uint64_t slab = 0;
	uint32_t pos = 0;
	uint16_t first;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	if ((bonded_eth_dev->data->dev_conf.rxmode.offloads &
			DEV_RX_OFFLOAD_VLAN_FILTER) == 0)
		return 0;

	internals = bonded_eth_dev->data->dev_private;
	found = rte_bitmap_scan(internals->vlan_filter_bmp, &pos, &slab);
	first = pos;

	if (!found)
		return 0;

	do {
		uint32_t i;
		uint64_t mask;

		for (i = 0, mask = 1;
		     i < RTE_BITMAP_SLAB_BIT_SIZE;
		     i ++, mask <<= 1) {
			if (unlikely(slab & mask)) {
				uint16_t vlan_id = pos + i;

				res = rte_eth_dev_vlan_filter(slave_port_id,
							      vlan_id, 1);
			}
		}
		found = rte_bitmap_scan(internals->vlan_filter_bmp,
					&pos, &slab);
	} while (found && first != pos && res == 0);

	return res;
}

static int
slave_rte_flow_prepare(uint16_t slave_id, struct bond_dev_private *internals)
{
	struct rte_flow *flow;
	struct rte_flow_error ferror;
	uint16_t slave_port_id = internals->slaves[slave_id].port_id;

	if (internals->flow_isolated_valid != 0) {
		rte_eth_dev_stop(slave_port_id);
		if (rte_flow_isolate(slave_port_id, internals->flow_isolated,
		    &ferror)) {
			RTE_BOND_LOG(ERR, "rte_flow_isolate failed for slave"
				     " %d: %s", slave_id, ferror.message ?
				     ferror.message : "(no stated reason)");
			return -1;
		}
	}
	TAILQ_FOREACH(flow, &internals->flow_list, next) {
		flow->flows[slave_id] = rte_flow_create(slave_port_id,
							&flow->fd->attr,
							flow->fd->items,
							flow->fd->actions,
							&ferror);
		if (flow->flows[slave_id] == NULL) {
			RTE_BOND_LOG(ERR, "Cannot create flow for slave"
				     " %d: %s", slave_id,
				     ferror.message ? ferror.message :
				     "(no stated reason)");
			/* Destroy successful bond flows from the slave */
			TAILQ_FOREACH(flow, &internals->flow_list, next) {
				if (flow->flows[slave_id] != NULL) {
					rte_flow_destroy(slave_port_id,
							 flow->flows[slave_id],
							 &ferror);
					flow->flows[slave_id] = NULL;
				}
			}
			return -1;
		}
	}
	return 0;
}

static int
__eth_bond_slave_add_lock_free(uint16_t bonded_port_id, uint16_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev, *slave_eth_dev;
	struct bond_dev_private *internals;
	struct rte_eth_link link_props;
	struct rte_eth_dev_info dev_info;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	if (valid_slave_port_id(slave_port_id, internals->mode) != 0)
		return -1;

	slave_eth_dev = &rte_eth_devices[slave_port_id];
	if (slave_eth_dev->data->dev_flags & RTE_ETH_DEV_BONDED_SLAVE) {
		RTE_BOND_LOG(ERR, "Slave device is already a slave of a bonded device");
		return -1;
	}

	rte_eth_dev_info_get(slave_port_id, &dev_info);
	if (dev_info.max_rx_pktlen < internals->max_rx_pktlen) {
		RTE_BOND_LOG(ERR, "Slave (port %u) max_rx_pktlen too small",
			     slave_port_id);
		return -1;
	}

	slave_add(internals, slave_eth_dev);

	/* We need to store slaves reta_size to be able to synchronize RETA for all
	 * slave devices even if its sizes are different.
	 */
	internals->slaves[internals->slave_count].reta_size = dev_info.reta_size;

	if (internals->slave_count < 1) {
		/* if MAC is not user defined then use MAC of first slave add to
		 * bonded device */
		if (!internals->user_defined_mac) {
			if (mac_address_set(bonded_eth_dev,
					    slave_eth_dev->data->mac_addrs)) {
				RTE_BOND_LOG(ERR, "Failed to set MAC address");
				return -1;
			}
		}

		/* Inherit eth dev link properties from first slave */
		link_properties_set(bonded_eth_dev,
				&(slave_eth_dev->data->dev_link));

		/* Make primary slave */
		internals->primary_port = slave_port_id;
		internals->current_primary_port = slave_port_id;

		/* Inherit queues settings from first slave */
		internals->nb_rx_queues = slave_eth_dev->data->nb_rx_queues;
		internals->nb_tx_queues = slave_eth_dev->data->nb_tx_queues;

		internals->reta_size = dev_info.reta_size;

		/* Take the first dev's offload capabilities */
		internals->rx_offload_capa = dev_info.rx_offload_capa;
		internals->tx_offload_capa = dev_info.tx_offload_capa;
		internals->rx_queue_offload_capa = dev_info.rx_queue_offload_capa;
		internals->tx_queue_offload_capa = dev_info.tx_queue_offload_capa;
		internals->flow_type_rss_offloads = dev_info.flow_type_rss_offloads;

		/* Inherit first slave's max rx packet size */
		internals->candidate_max_rx_pktlen = dev_info.max_rx_pktlen;

	} else {
		internals->rx_offload_capa &= dev_info.rx_offload_capa;
		internals->tx_offload_capa &= dev_info.tx_offload_capa;
		internals->rx_queue_offload_capa &= dev_info.rx_queue_offload_capa;
		internals->tx_queue_offload_capa &= dev_info.tx_queue_offload_capa;
		internals->flow_type_rss_offloads &= dev_info.flow_type_rss_offloads;

		if (link_properties_valid(bonded_eth_dev,
				&slave_eth_dev->data->dev_link) != 0) {
			RTE_BOND_LOG(ERR, "Invalid link properties for slave %d"
					" in bonding mode %d", slave_port_id,
					internals->mode);
			return -1;
		}

		/* RETA size is GCD of all slaves RETA sizes, so, if all sizes will be
		 * the power of 2, the lower one is GCD
		 */
		if (internals->reta_size > dev_info.reta_size)
			internals->reta_size = dev_info.reta_size;

		if (!internals->max_rx_pktlen &&
		    dev_info.max_rx_pktlen < internals->candidate_max_rx_pktlen)
			internals->candidate_max_rx_pktlen = dev_info.max_rx_pktlen;
	}

	bonded_eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf &=
			internals->flow_type_rss_offloads;

	if (slave_rte_flow_prepare(internals->slave_count, internals) != 0) {
		RTE_BOND_LOG(ERR, "Failed to prepare new slave flows: port=%d",
			     slave_port_id);
		return -1;
	}

	internals->slave_count++;

	if (bonded_eth_dev->data->dev_started) {
		if (slave_configure(bonded_eth_dev, slave_eth_dev) != 0) {
			internals->slave_count--;
			RTE_BOND_LOG(ERR, "rte_bond_slaves_configure: port=%d",
					slave_port_id);
			return -1;
		}
	}

	/* Add slave details to bonded device */
	slave_eth_dev->data->dev_flags |= RTE_ETH_DEV_BONDED_SLAVE;

	/* Update all slave devices MACs*/
	mac_address_slaves_update(bonded_eth_dev);

	/* Register link status change callback with bonded device pointer as
	 * argument*/
	rte_eth_dev_callback_register(slave_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback, &bonded_eth_dev->data->port_id);

	/* If bonded device is started then we can add the slave to our active
	 * slave array */
	if (bonded_eth_dev->data->dev_started) {
		rte_eth_link_get_nowait(slave_port_id, &link_props);

		 if (link_props.link_status == ETH_LINK_UP) {
			if (internals->active_slave_count == 0 &&
			    !internals->user_defined_primary_port)
				bond_ethdev_primary_set(internals,
							slave_port_id);

			if (find_slave_by_id(internals->active_slaves,
					     internals->active_slave_count,
					     slave_port_id) == internals->active_slave_count)
				activate_slave(bonded_eth_dev, slave_port_id);
		}
	}

	slave_vlan_filter_set(bonded_port_id, slave_port_id);

	return 0;

}

int
rte_eth_bond_slave_add(uint16_t bonded_port_id, uint16_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;

	int retval;

	/* Verify that port id's are valid bonded and slave ports */
	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	rte_spinlock_lock(&internals->lock);

	retval = __eth_bond_slave_add_lock_free(bonded_port_id, slave_port_id);

	rte_spinlock_unlock(&internals->lock);

	return retval;
}

static int
__eth_bond_slave_remove_lock_free(uint16_t bonded_port_id,
				   uint16_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;
	struct rte_eth_dev *slave_eth_dev;
	struct rte_flow_error flow_error;
	struct rte_flow *flow;
	int i, slave_idx;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	if (valid_slave_port_id(slave_port_id, internals->mode) < 0)
		return -1;

	/* first remove from active slave list */
	slave_idx = find_slave_by_id(internals->active_slaves,
		internals->active_slave_count, slave_port_id);

	if (slave_idx < internals->active_slave_count)
		deactivate_slave(bonded_eth_dev, slave_port_id);

	slave_idx = -1;
	/* now find in slave list */
	for (i = 0; i < internals->slave_count; i++)
		if (internals->slaves[i].port_id == slave_port_id) {
			slave_idx = i;
			break;
		}

	if (slave_idx < 0) {
		RTE_BOND_LOG(ERR, "Couldn't find slave in port list, slave count %d",
				internals->slave_count);
		return -1;
	}

	/* Un-register link status change callback with bonded device pointer as
	 * argument*/
	rte_eth_dev_callback_unregister(slave_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback,
			&rte_eth_devices[bonded_port_id].data->port_id);

	/* Restore original MAC address of slave device */
	rte_eth_dev_default_mac_addr_set(slave_port_id,
			&(internals->slaves[slave_idx].persisted_mac_addr));

	/*
	 * Remove bond device flows from slave device.
	 * Note: don't restore flow isolate mode.
	 */
	TAILQ_FOREACH(flow, &internals->flow_list, next) {
		if (flow->flows[slave_idx] != NULL) {
			rte_flow_destroy(slave_port_id, flow->flows[slave_idx],
					 &flow_error);
			flow->flows[slave_idx] = NULL;
		}
	}

	slave_eth_dev = &rte_eth_devices[slave_port_id];
	slave_remove(internals, slave_eth_dev);
	slave_eth_dev->data->dev_flags &= (~RTE_ETH_DEV_BONDED_SLAVE);

	/*  first slave in the active list will be the primary by default,
	 *  otherwise use first device in list */
	if (internals->current_primary_port == slave_port_id) {
		if (internals->active_slave_count > 0)
			internals->current_primary_port = internals->active_slaves[0];
		else if (internals->slave_count > 0)
			internals->current_primary_port = internals->slaves[0].port_id;
		else
			internals->primary_port = 0;
	}

	if (internals->active_slave_count < 1) {
		/* if no slaves are any longer attached to bonded device and MAC is not
		 * user defined then clear MAC of bonded device as it will be reset
		 * when a new slave is added */
		if (internals->slave_count < 1 && !internals->user_defined_mac)
			memset(rte_eth_devices[bonded_port_id].data->mac_addrs, 0,
					sizeof(*(rte_eth_devices[bonded_port_id].data->mac_addrs)));
	}
	if (internals->slave_count == 0) {
		internals->rx_offload_capa = 0;
		internals->tx_offload_capa = 0;
		internals->rx_queue_offload_capa = 0;
		internals->tx_queue_offload_capa = 0;
		internals->flow_type_rss_offloads = ETH_RSS_PROTO_MASK;
		internals->reta_size = 0;
		internals->candidate_max_rx_pktlen = 0;
		internals->max_rx_pktlen = 0;
	}
	return 0;
}

int
rte_eth_bond_slave_remove(uint16_t bonded_port_id, uint16_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;
	int retval;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	rte_spinlock_lock(&internals->lock);

	retval = __eth_bond_slave_remove_lock_free(bonded_port_id, slave_port_id);

	rte_spinlock_unlock(&internals->lock);

	return retval;
}

int
rte_eth_bond_mode_set(uint16_t bonded_port_id, uint8_t mode)
{
	struct rte_eth_dev *bonded_eth_dev;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];

	if (check_for_master_bonded_ethdev(bonded_eth_dev) != 0 &&
			mode == BONDING_MODE_8023AD)
		return -1;

	return bond_ethdev_mode_set(bonded_eth_dev, mode);
}

int
rte_eth_bond_mode_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->mode;
}

int
rte_eth_bond_primary_set(uint16_t bonded_port_id, uint16_t slave_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (valid_slave_port_id(slave_port_id, internals->mode) != 0)
		return -1;

	internals->user_defined_primary_port = 1;
	internals->primary_port = slave_port_id;

	bond_ethdev_primary_set(internals, slave_port_id);

	return 0;
}

int
rte_eth_bond_primary_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (internals->slave_count < 1)
		return -1;

	return internals->current_primary_port;
}

int
rte_eth_bond_slaves_get(uint16_t bonded_port_id, uint16_t slaves[],
			uint16_t len)
{
	struct bond_dev_private *internals;
	uint8_t i;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	if (slaves == NULL)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (internals->slave_count > len)
		return -1;

	for (i = 0; i < internals->slave_count; i++)
		slaves[i] = internals->slaves[i].port_id;

	return internals->slave_count;
}

int
rte_eth_bond_active_slaves_get(uint16_t bonded_port_id, uint16_t slaves[],
		uint16_t len)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	if (slaves == NULL)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (internals->active_slave_count > len)
		return -1;

	memcpy(slaves, internals->active_slaves,
	internals->active_slave_count * sizeof(internals->active_slaves[0]));

	return internals->active_slave_count;
}

int
rte_eth_bond_mac_address_set(uint16_t bonded_port_id,
		struct ether_addr *mac_addr)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	/* Set MAC Address of Bonded Device */
	if (mac_address_set(bonded_eth_dev, mac_addr))
		return -1;

	internals->user_defined_mac = 1;

	/* Update all slave devices MACs*/
	if (internals->slave_count > 0)
		return mac_address_slaves_update(bonded_eth_dev);

	return 0;
}

int
rte_eth_bond_mac_address_reset(uint16_t bonded_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	internals->user_defined_mac = 0;

	if (internals->slave_count > 0) {
		/* Set MAC Address of Bonded Device */
		if (mac_address_set(bonded_eth_dev,
				&internals->slaves[internals->primary_port].persisted_mac_addr)
				!= 0) {
			RTE_BOND_LOG(ERR, "Failed to set MAC address on bonded device");
			return -1;
		}
		/* Update all slave devices MAC addresses */
		return mac_address_slaves_update(bonded_eth_dev);
	}
	/* No need to update anything as no slaves present */
	return 0;
}

int
rte_eth_bond_xmit_policy_set(uint16_t bonded_port_id, uint8_t policy)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	switch (policy) {
	case BALANCE_XMIT_POLICY_LAYER2:
		internals->balance_xmit_policy = policy;
		internals->burst_xmit_hash = burst_xmit_l2_hash;
		break;
	case BALANCE_XMIT_POLICY_LAYER23:
		internals->balance_xmit_policy = policy;
		internals->burst_xmit_hash = burst_xmit_l23_hash;
		break;
	case BALANCE_XMIT_POLICY_LAYER34:
		internals->balance_xmit_policy = policy;
		internals->burst_xmit_hash = burst_xmit_l34_hash;
		break;

	default:
		return -1;
	}
	return 0;
}

int
rte_eth_bond_xmit_policy_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->balance_xmit_policy;
}

int
rte_eth_bond_link_monitoring_set(uint16_t bonded_port_id, uint32_t internal_ms)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_status_polling_interval_ms = internal_ms;

	return 0;
}

int
rte_eth_bond_link_monitoring_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_status_polling_interval_ms;
}

int
rte_eth_bond_link_down_prop_delay_set(uint16_t bonded_port_id,
				       uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_down_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_down_prop_delay_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_down_delay_ms;
}

int
rte_eth_bond_link_up_prop_delay_set(uint16_t bonded_port_id, uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_up_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_up_prop_delay_get(uint16_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_up_delay_ms;
}
