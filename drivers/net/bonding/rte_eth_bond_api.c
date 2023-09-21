/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <string.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_tcp.h>
#include <bus_vdev_driver.h>
#include <rte_kvargs.h>

#include "rte_eth_bond.h"
#include "eth_bond_private.h"
#include "eth_bond_8023ad_private.h"

int
check_for_bonding_ethdev(const struct rte_eth_dev *eth_dev)
{
	/* Check valid pointer */
	if (eth_dev == NULL ||
		eth_dev->device == NULL ||
		eth_dev->device->driver == NULL ||
		eth_dev->device->driver->name == NULL)
		return -1;

	/* return 0 if driver name matches */
	return eth_dev->device->driver->name != pmd_bond_drv.driver.name;
}

int
valid_bonding_port_id(uint16_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);
	return check_for_bonding_ethdev(&rte_eth_devices[port_id]);
}

int
check_for_main_bonding_ethdev(const struct rte_eth_dev *eth_dev)
{
	int i;
	struct bond_dev_private *internals;

	if (check_for_bonding_ethdev(eth_dev) != 0)
		return 0;

	internals = eth_dev->data->dev_private;

	/* Check if any of member devices is a bonding device */
	for (i = 0; i < internals->member_count; i++)
		if (valid_bonding_port_id(internals->members[i].port_id) == 0)
			return 1;

	return 0;
}

int
valid_member_port_id(struct bond_dev_private *internals, uint16_t member_port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(member_port_id, -1);

	/* Verify that member_port_id refers to a non bonding port */
	if (check_for_bonding_ethdev(&rte_eth_devices[member_port_id]) == 0 &&
			internals->mode == BONDING_MODE_8023AD) {
		RTE_BOND_LOG(ERR, "Cannot add member to bonding device in 802.3ad"
				" mode as member is also a bonding device, only "
				"physical devices can be support in this mode.");
		return -1;
	}

	if (internals->port_id == member_port_id) {
		RTE_BOND_LOG(ERR,
			"Cannot add the bonding device itself as its member.");
		return -1;
	}

	return 0;
}

void
activate_member(struct rte_eth_dev *eth_dev, uint16_t port_id)
{
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint16_t active_count = internals->active_member_count;

	if (internals->mode == BONDING_MODE_8023AD)
		bond_mode_8023ad_activate_member(eth_dev, port_id);

	if (internals->mode == BONDING_MODE_TLB
			|| internals->mode == BONDING_MODE_ALB) {

		internals->tlb_members_order[active_count] = port_id;
	}

	RTE_ASSERT(internals->active_member_count <
			(RTE_DIM(internals->active_members) - 1));

	internals->active_members[internals->active_member_count] = port_id;
	internals->active_member_count++;

	if (internals->mode == BONDING_MODE_TLB)
		bond_tlb_activate_member(internals);
	if (internals->mode == BONDING_MODE_ALB)
		bond_mode_alb_client_list_upd(eth_dev);
}

void
deactivate_member(struct rte_eth_dev *eth_dev, uint16_t port_id)
{
	uint16_t member_pos;
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint16_t active_count = internals->active_member_count;

	if (internals->mode == BONDING_MODE_8023AD) {
		bond_mode_8023ad_stop(eth_dev);
		bond_mode_8023ad_deactivate_member(eth_dev, port_id);
	} else if (internals->mode == BONDING_MODE_TLB
			|| internals->mode == BONDING_MODE_ALB)
		bond_tlb_disable(internals);

	member_pos = find_member_by_id(internals->active_members, active_count,
			port_id);

	/*
	 * If member was not at the end of the list
	 * shift active members up active array list.
	 */
	if (member_pos < active_count) {
		active_count--;
		memmove(internals->active_members + member_pos,
				internals->active_members + member_pos + 1,
				(active_count - member_pos) *
					sizeof(internals->active_members[0]));
	}

	RTE_ASSERT(active_count < RTE_DIM(internals->active_members));
	internals->active_member_count = active_count;

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
	struct rte_eth_dev *bond_dev;
	char devargs[52];
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
		return ret;

	bond_dev = rte_eth_dev_get_by_name(name);
	RTE_ASSERT(bond_dev);

	/*
	 * To make bond_ethdev_configure() happy we need to free the
	 * internals->kvlist here.
	 *
	 * Also see comment in bond_ethdev_configure().
	 */
	internals = bond_dev->data->dev_private;
	rte_kvargs_free(internals->kvlist);
	internals->kvlist = NULL;

	return bond_dev->data->port_id;
}

int
rte_eth_bond_free(const char *name)
{
	return rte_vdev_uninit(name);
}

static int
member_vlan_filter_set(uint16_t bonding_port_id, uint16_t member_port_id)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;
	int found;
	int res = 0;
	uint64_t slab = 0;
	uint32_t pos = 0;
	uint16_t first;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	if ((bonding_eth_dev->data->dev_conf.rxmode.offloads &
			RTE_ETH_RX_OFFLOAD_VLAN_FILTER) == 0)
		return 0;

	internals = bonding_eth_dev->data->dev_private;
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

				res = rte_eth_dev_vlan_filter(member_port_id,
							      vlan_id, 1);
			}
		}
		found = rte_bitmap_scan(internals->vlan_filter_bmp,
					&pos, &slab);
	} while (found && first != pos && res == 0);

	return res;
}

static int
member_rte_flow_prepare(uint16_t member_id, struct bond_dev_private *internals)
{
	struct rte_flow *flow;
	struct rte_flow_error ferror;
	uint16_t member_port_id = internals->members[member_id].port_id;

	if (internals->flow_isolated_valid != 0) {
		if (rte_eth_dev_stop(member_port_id) != 0) {
			RTE_BOND_LOG(ERR, "Failed to stop device on port %u",
				     member_port_id);
			return -1;
		}

		if (rte_flow_isolate(member_port_id, internals->flow_isolated,
		    &ferror)) {
			RTE_BOND_LOG(ERR, "rte_flow_isolate failed for member"
				     " %d: %s", member_id, ferror.message ?
				     ferror.message : "(no stated reason)");
			return -1;
		}
	}
	TAILQ_FOREACH(flow, &internals->flow_list, next) {
		flow->flows[member_id] = rte_flow_create(member_port_id,
							flow->rule.attr,
							flow->rule.pattern,
							flow->rule.actions,
							&ferror);
		if (flow->flows[member_id] == NULL) {
			RTE_BOND_LOG(ERR, "Cannot create flow for member"
				     " %d: %s", member_id,
				     ferror.message ? ferror.message :
				     "(no stated reason)");
			/* Destroy successful bond flows from the member */
			TAILQ_FOREACH(flow, &internals->flow_list, next) {
				if (flow->flows[member_id] != NULL) {
					rte_flow_destroy(member_port_id,
							 flow->flows[member_id],
							 &ferror);
					flow->flows[member_id] = NULL;
				}
			}
			return -1;
		}
	}
	return 0;
}

static void
eth_bond_member_inherit_dev_info_rx_first(struct bond_dev_private *internals,
					 const struct rte_eth_dev_info *di)
{
	struct rte_eth_rxconf *rxconf_i = &internals->default_rxconf;

	internals->reta_size = di->reta_size;
	internals->rss_key_len = di->hash_key_size;

	/* Inherit Rx offload capabilities from the first member device */
	internals->rx_offload_capa = di->rx_offload_capa;
	internals->rx_queue_offload_capa = di->rx_queue_offload_capa;
	internals->flow_type_rss_offloads = di->flow_type_rss_offloads;

	/* Inherit maximum Rx packet size from the first member device */
	internals->candidate_max_rx_pktlen = di->max_rx_pktlen;

	/* Inherit default Rx queue settings from the first member device */
	memcpy(rxconf_i, &di->default_rxconf, sizeof(*rxconf_i));

	/*
	 * Turn off descriptor prefetch and writeback by default for all
	 * member devices. Applications may tweak this setting if need be.
	 */
	rxconf_i->rx_thresh.pthresh = 0;
	rxconf_i->rx_thresh.hthresh = 0;
	rxconf_i->rx_thresh.wthresh = 0;

	/* Setting this to zero should effectively enable default values */
	rxconf_i->rx_free_thresh = 0;

	/* Disable deferred start by default for all member devices */
	rxconf_i->rx_deferred_start = 0;
}

static void
eth_bond_member_inherit_dev_info_tx_first(struct bond_dev_private *internals,
					 const struct rte_eth_dev_info *di)
{
	struct rte_eth_txconf *txconf_i = &internals->default_txconf;

	/* Inherit Tx offload capabilities from the first member device */
	internals->tx_offload_capa = di->tx_offload_capa;
	internals->tx_queue_offload_capa = di->tx_queue_offload_capa;

	/* Inherit default Tx queue settings from the first member device */
	memcpy(txconf_i, &di->default_txconf, sizeof(*txconf_i));

	/*
	 * Turn off descriptor prefetch and writeback by default for all
	 * member devices. Applications may tweak this setting if need be.
	 */
	txconf_i->tx_thresh.pthresh = 0;
	txconf_i->tx_thresh.hthresh = 0;
	txconf_i->tx_thresh.wthresh = 0;

	/*
	 * Setting these parameters to zero assumes that default
	 * values will be configured implicitly by member devices.
	 */
	txconf_i->tx_free_thresh = 0;
	txconf_i->tx_rs_thresh = 0;

	/* Disable deferred start by default for all member devices */
	txconf_i->tx_deferred_start = 0;
}

static void
eth_bond_member_inherit_dev_info_rx_next(struct bond_dev_private *internals,
					const struct rte_eth_dev_info *di)
{
	struct rte_eth_rxconf *rxconf_i = &internals->default_rxconf;
	const struct rte_eth_rxconf *rxconf = &di->default_rxconf;

	internals->rx_offload_capa &= di->rx_offload_capa;
	internals->rx_queue_offload_capa &= di->rx_queue_offload_capa;
	internals->flow_type_rss_offloads &= di->flow_type_rss_offloads;

	/*
	 * If at least one member device suggests enabling this
	 * setting by default, enable it for all member devices
	 * since disabling it may not be necessarily supported.
	 */
	if (rxconf->rx_drop_en == 1)
		rxconf_i->rx_drop_en = 1;

	/*
	 * Adding a new member device may cause some of previously inherited
	 * offloads to be withdrawn from the internal rx_queue_offload_capa
	 * value. Thus, the new internal value of default Rx queue offloads
	 * has to be masked by rx_queue_offload_capa to make sure that only
	 * commonly supported offloads are preserved from both the previous
	 * value and the value being inherited from the new member device.
	 */
	rxconf_i->offloads = (rxconf_i->offloads | rxconf->offloads) &
			     internals->rx_queue_offload_capa;

	/*
	 * RETA size is GCD of all members RETA sizes, so, if all sizes will be
	 * the power of 2, the lower one is GCD
	 */
	if (internals->reta_size > di->reta_size)
		internals->reta_size = di->reta_size;
	if (internals->rss_key_len > di->hash_key_size) {
		RTE_BOND_LOG(WARNING, "member has different rss key size, "
				"configuring rss may fail");
		internals->rss_key_len = di->hash_key_size;
	}

	if (!internals->max_rx_pktlen &&
	    di->max_rx_pktlen < internals->candidate_max_rx_pktlen)
		internals->candidate_max_rx_pktlen = di->max_rx_pktlen;
}

static void
eth_bond_member_inherit_dev_info_tx_next(struct bond_dev_private *internals,
					const struct rte_eth_dev_info *di)
{
	struct rte_eth_txconf *txconf_i = &internals->default_txconf;
	const struct rte_eth_txconf *txconf = &di->default_txconf;

	internals->tx_offload_capa &= di->tx_offload_capa;
	internals->tx_queue_offload_capa &= di->tx_queue_offload_capa;

	/*
	 * Adding a new member device may cause some of previously inherited
	 * offloads to be withdrawn from the internal tx_queue_offload_capa
	 * value. Thus, the new internal value of default Tx queue offloads
	 * has to be masked by tx_queue_offload_capa to make sure that only
	 * commonly supported offloads are preserved from both the previous
	 * value and the value being inherited from the new member device.
	 */
	txconf_i->offloads = (txconf_i->offloads | txconf->offloads) &
			     internals->tx_queue_offload_capa;
}

static void
eth_bond_member_inherit_desc_lim_first(struct rte_eth_desc_lim *bond_desc_lim,
		const struct rte_eth_desc_lim *member_desc_lim)
{
	memcpy(bond_desc_lim, member_desc_lim, sizeof(*bond_desc_lim));
}

static int
eth_bond_member_inherit_desc_lim_next(struct rte_eth_desc_lim *bond_desc_lim,
		const struct rte_eth_desc_lim *member_desc_lim)
{
	bond_desc_lim->nb_max = RTE_MIN(bond_desc_lim->nb_max,
					member_desc_lim->nb_max);
	bond_desc_lim->nb_min = RTE_MAX(bond_desc_lim->nb_min,
					member_desc_lim->nb_min);
	bond_desc_lim->nb_align = RTE_MAX(bond_desc_lim->nb_align,
					  member_desc_lim->nb_align);

	if (bond_desc_lim->nb_min > bond_desc_lim->nb_max ||
	    bond_desc_lim->nb_align > bond_desc_lim->nb_max) {
		RTE_BOND_LOG(ERR, "Failed to inherit descriptor limits");
		return -EINVAL;
	}

	/* Treat maximum number of segments equal to 0 as unspecified */
	if (member_desc_lim->nb_seg_max != 0 &&
	    (bond_desc_lim->nb_seg_max == 0 ||
	     member_desc_lim->nb_seg_max < bond_desc_lim->nb_seg_max))
		bond_desc_lim->nb_seg_max = member_desc_lim->nb_seg_max;
	if (member_desc_lim->nb_mtu_seg_max != 0 &&
	    (bond_desc_lim->nb_mtu_seg_max == 0 ||
	     member_desc_lim->nb_mtu_seg_max < bond_desc_lim->nb_mtu_seg_max))
		bond_desc_lim->nb_mtu_seg_max = member_desc_lim->nb_mtu_seg_max;

	return 0;
}

static int
__eth_bond_member_add_lock_free(uint16_t bonding_port_id, uint16_t member_port_id)
{
	struct rte_eth_dev *bonding_eth_dev, *member_eth_dev;
	struct bond_dev_private *internals;
	struct rte_eth_link link_props;
	struct rte_eth_dev_info dev_info;
	int ret;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	if (valid_member_port_id(internals, member_port_id) != 0)
		return -1;

	member_eth_dev = &rte_eth_devices[member_port_id];
	if (member_eth_dev->data->dev_flags & RTE_ETH_DEV_BONDING_MEMBER) {
		RTE_BOND_LOG(ERR, "Member device is already a member of a bonding device");
		return -1;
	}

	ret = rte_eth_dev_info_get(member_port_id, &dev_info);
	if (ret != 0) {
		RTE_BOND_LOG(ERR,
			"%s: Error during getting device (port %u) info: %s\n",
			__func__, member_port_id, strerror(-ret));

		return ret;
	}
	if (dev_info.max_rx_pktlen < internals->max_rx_pktlen) {
		RTE_BOND_LOG(ERR, "Member (port %u) max_rx_pktlen too small",
			     member_port_id);
		return -1;
	}

	member_add(internals, member_eth_dev);

	/* We need to store members reta_size to be able to synchronize RETA for all
	 * member devices even if its sizes are different.
	 */
	internals->members[internals->member_count].reta_size = dev_info.reta_size;

	if (internals->member_count < 1) {
		/*
		 * if MAC is not user defined then use MAC of first member add to
		 * bonding device.
		 */
		if (!internals->user_defined_mac) {
			if (mac_address_set(bonding_eth_dev,
					    member_eth_dev->data->mac_addrs)) {
				RTE_BOND_LOG(ERR, "Failed to set MAC address");
				return -1;
			}
		}

		/* Make primary member */
		internals->primary_port = member_port_id;
		internals->current_primary_port = member_port_id;

		internals->speed_capa = dev_info.speed_capa;

		/* Inherit queues settings from first member */
		internals->nb_rx_queues = member_eth_dev->data->nb_rx_queues;
		internals->nb_tx_queues = member_eth_dev->data->nb_tx_queues;

		eth_bond_member_inherit_dev_info_rx_first(internals, &dev_info);
		eth_bond_member_inherit_dev_info_tx_first(internals, &dev_info);

		eth_bond_member_inherit_desc_lim_first(&internals->rx_desc_lim,
						      &dev_info.rx_desc_lim);
		eth_bond_member_inherit_desc_lim_first(&internals->tx_desc_lim,
						      &dev_info.tx_desc_lim);
	} else {
		int ret;

		internals->speed_capa &= dev_info.speed_capa;
		eth_bond_member_inherit_dev_info_rx_next(internals, &dev_info);
		eth_bond_member_inherit_dev_info_tx_next(internals, &dev_info);

		ret = eth_bond_member_inherit_desc_lim_next(&internals->rx_desc_lim,
							&dev_info.rx_desc_lim);
		if (ret != 0)
			return ret;

		ret = eth_bond_member_inherit_desc_lim_next(&internals->tx_desc_lim,
							&dev_info.tx_desc_lim);
		if (ret != 0)
			return ret;
	}

	/* Bond mode Broadcast & 8023AD don't support MBUF_FAST_FREE offload. */
	if (internals->mode == BONDING_MODE_8023AD ||
	    internals->mode == BONDING_MODE_BROADCAST)
		internals->tx_offload_capa &= ~RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	bonding_eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf &=
			internals->flow_type_rss_offloads;

	if (member_rte_flow_prepare(internals->member_count, internals) != 0) {
		RTE_BOND_LOG(ERR, "Failed to prepare new member flows: port=%d",
			     member_port_id);
		return -1;
	}

	/* Add additional MAC addresses to the member */
	if (member_add_mac_addresses(bonding_eth_dev, member_port_id) != 0) {
		RTE_BOND_LOG(ERR, "Failed to add mac address(es) to member %hu",
				member_port_id);
		return -1;
	}

	internals->member_count++;

	if (bonding_eth_dev->data->dev_started) {
		if (member_configure(bonding_eth_dev, member_eth_dev) != 0) {
			internals->member_count--;
			RTE_BOND_LOG(ERR, "rte_bond_members_configure: port=%d",
					member_port_id);
			return -1;
		}
		if (member_start(bonding_eth_dev, member_eth_dev) != 0) {
			internals->member_count--;
			RTE_BOND_LOG(ERR, "rte_bond_members_start: port=%d",
					member_port_id);
			return -1;
		}
	}

	/* Update all member devices MACs */
	mac_address_members_update(bonding_eth_dev);

	/*
	 * Register link status change callback with bonding device pointer as
	 * argument.
	 */
	rte_eth_dev_callback_register(member_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback, &bonding_eth_dev->data->port_id);

	/*
	 * If bonding device is started then we can add the member to our active
	 * member array.
	 */
	if (bonding_eth_dev->data->dev_started) {
		ret = rte_eth_link_get_nowait(member_port_id, &link_props);
		if (ret < 0) {
			rte_eth_dev_callback_unregister(member_port_id,
					RTE_ETH_EVENT_INTR_LSC,
					bond_ethdev_lsc_event_callback,
					&bonding_eth_dev->data->port_id);
			internals->member_count--;
			RTE_BOND_LOG(ERR,
				"Member (port %u) link get failed: %s\n",
				member_port_id, rte_strerror(-ret));
			return -1;
		}

		if (link_props.link_status == RTE_ETH_LINK_UP) {
			if (internals->active_member_count == 0 &&
			    !internals->user_defined_primary_port)
				bond_ethdev_primary_set(internals,
							member_port_id);
		}
	}

	/* Add member details to bonding device */
	member_eth_dev->data->dev_flags |= RTE_ETH_DEV_BONDING_MEMBER;

	member_vlan_filter_set(bonding_port_id, member_port_id);

	return 0;

}

int
rte_eth_bond_member_add(uint16_t bonding_port_id, uint16_t member_port_id)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;

	int retval;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	if (valid_member_port_id(internals, member_port_id) != 0)
		return -1;

	rte_spinlock_lock(&internals->lock);

	retval = __eth_bond_member_add_lock_free(bonding_port_id, member_port_id);

	rte_spinlock_unlock(&internals->lock);

	return retval;
}

static int
__eth_bond_member_remove_lock_free(uint16_t bonding_port_id,
				   uint16_t member_port_id)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;
	struct rte_eth_dev *member_eth_dev;
	struct rte_flow_error flow_error;
	struct rte_flow *flow;
	int i, member_idx;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	if (valid_member_port_id(internals, member_port_id) < 0)
		return -1;

	/* first remove from active member list */
	member_idx = find_member_by_id(internals->active_members,
		internals->active_member_count, member_port_id);

	if (member_idx < internals->active_member_count)
		deactivate_member(bonding_eth_dev, member_port_id);

	member_idx = -1;
	/* now find in member list */
	for (i = 0; i < internals->member_count; i++)
		if (internals->members[i].port_id == member_port_id) {
			member_idx = i;
			break;
		}

	if (member_idx < 0) {
		RTE_BOND_LOG(ERR, "Could not find member in port list, member count %u",
				internals->member_count);
		return -1;
	}

	/* Un-register link status change callback with bonding device pointer as
	 * argument*/
	rte_eth_dev_callback_unregister(member_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback,
			&rte_eth_devices[bonding_port_id].data->port_id);

	/* Restore original MAC address of member device */
	rte_eth_dev_default_mac_addr_set(member_port_id,
			&internals->members[member_idx].persisted_mac_addr);

	/* remove additional MAC addresses from the member */
	member_remove_mac_addresses(bonding_eth_dev, member_port_id);

	/*
	 * Remove bond device flows from member device.
	 * Note: don't restore flow isolate mode.
	 */
	TAILQ_FOREACH(flow, &internals->flow_list, next) {
		if (flow->flows[member_idx] != NULL) {
			rte_flow_destroy(member_port_id, flow->flows[member_idx],
					 &flow_error);
			flow->flows[member_idx] = NULL;
		}
	}

	/* Remove the dedicated queues flow */
	if (internals->mode == BONDING_MODE_8023AD &&
		internals->mode4.dedicated_queues.enabled == 1 &&
		internals->mode4.dedicated_queues.flow[member_port_id] != NULL) {
		rte_flow_destroy(member_port_id,
				internals->mode4.dedicated_queues.flow[member_port_id],
				&flow_error);
		internals->mode4.dedicated_queues.flow[member_port_id] = NULL;
	}

	member_eth_dev = &rte_eth_devices[member_port_id];
	member_remove(internals, member_eth_dev);
	member_eth_dev->data->dev_flags &= (~RTE_ETH_DEV_BONDING_MEMBER);

	/*  first member in the active list will be the primary by default,
	 *  otherwise use first device in list */
	if (internals->current_primary_port == member_port_id) {
		if (internals->active_member_count > 0)
			internals->current_primary_port = internals->active_members[0];
		else if (internals->member_count > 0)
			internals->current_primary_port = internals->members[0].port_id;
		else
			internals->primary_port = 0;
		mac_address_members_update(bonding_eth_dev);
	}

	if (internals->active_member_count < 1) {
		/*
		 * if no members are any longer attached to bonding device and MAC is not
		 * user defined then clear MAC of bonding device as it will be reset
		 * when a new member is added.
		 */
		if (internals->member_count < 1 && !internals->user_defined_mac)
			memset(rte_eth_devices[bonding_port_id].data->mac_addrs, 0,
				sizeof(*rte_eth_devices[bonding_port_id].data->mac_addrs));
	}
	if (internals->member_count == 0) {
		internals->rx_offload_capa = 0;
		internals->tx_offload_capa = 0;
		internals->rx_queue_offload_capa = 0;
		internals->tx_queue_offload_capa = 0;
		internals->flow_type_rss_offloads = RTE_ETH_RSS_PROTO_MASK;
		internals->reta_size = 0;
		internals->candidate_max_rx_pktlen = 0;
		internals->max_rx_pktlen = 0;
	}
	return 0;
}

int
rte_eth_bond_member_remove(uint16_t bonding_port_id, uint16_t member_port_id)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;
	int retval;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	rte_spinlock_lock(&internals->lock);

	retval = __eth_bond_member_remove_lock_free(bonding_port_id, member_port_id);

	rte_spinlock_unlock(&internals->lock);

	return retval;
}

int
rte_eth_bond_mode_set(uint16_t bonding_port_id, uint8_t mode)
{
	struct rte_eth_dev *bonding_eth_dev;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];

	if (check_for_main_bonding_ethdev(bonding_eth_dev) != 0 &&
			mode == BONDING_MODE_8023AD)
		return -1;

	return bond_ethdev_mode_set(bonding_eth_dev, mode);
}

int
rte_eth_bond_mode_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	return internals->mode;
}

int
rte_eth_bond_primary_set(uint16_t bonding_port_id, uint16_t member_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	if (valid_member_port_id(internals, member_port_id) != 0)
		return -1;

	internals->user_defined_primary_port = 1;
	internals->primary_port = member_port_id;

	bond_ethdev_primary_set(internals, member_port_id);

	return 0;
}

int
rte_eth_bond_primary_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	if (internals->member_count < 1)
		return -1;

	return internals->current_primary_port;
}

int
rte_eth_bond_members_get(uint16_t bonding_port_id, uint16_t members[],
			uint16_t len)
{
	struct bond_dev_private *internals;
	uint16_t i;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	if (members == NULL)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	if (internals->member_count > len)
		return -1;

	for (i = 0; i < internals->member_count; i++)
		members[i] = internals->members[i].port_id;

	return internals->member_count;
}

int
rte_eth_bond_active_members_get(uint16_t bonding_port_id, uint16_t members[],
		uint16_t len)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	if (members == NULL)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	if (internals->active_member_count > len)
		return -1;

	memcpy(members, internals->active_members,
	internals->active_member_count * sizeof(internals->active_members[0]));

	return internals->active_member_count;
}

int
rte_eth_bond_mac_address_set(uint16_t bonding_port_id,
		struct rte_ether_addr *mac_addr)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	/* Set MAC Address of Bonding Device */
	if (mac_address_set(bonding_eth_dev, mac_addr))
		return -1;

	internals->user_defined_mac = 1;

	/* Update all member devices MACs*/
	if (internals->member_count > 0)
		return mac_address_members_update(bonding_eth_dev);

	return 0;
}

int
rte_eth_bond_mac_address_reset(uint16_t bonding_port_id)
{
	struct rte_eth_dev *bonding_eth_dev;
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	bonding_eth_dev = &rte_eth_devices[bonding_port_id];
	internals = bonding_eth_dev->data->dev_private;

	internals->user_defined_mac = 0;

	if (internals->member_count > 0) {
		int member_port;
		/* Get the primary member location based on the primary port
		 * number as, while member_add(), we will keep the primary
		 * member based on member_count,but not based on the primary port.
		 */
		for (member_port = 0; member_port < internals->member_count;
		     member_port++) {
			if (internals->members[member_port].port_id ==
			    internals->primary_port)
				break;
		}

		/* Set MAC Address of Bonding Device */
		if (mac_address_set(bonding_eth_dev,
			&internals->members[member_port].persisted_mac_addr)
				!= 0) {
			RTE_BOND_LOG(ERR, "Failed to set MAC address on bonding device");
			return -1;
		}
		/* Update all member devices MAC addresses */
		return mac_address_members_update(bonding_eth_dev);
	}
	/* No need to update anything as no members present */
	return 0;
}

int
rte_eth_bond_xmit_policy_set(uint16_t bonding_port_id, uint8_t policy)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

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
rte_eth_bond_xmit_policy_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	return internals->balance_xmit_policy;
}

int
rte_eth_bond_link_monitoring_set(uint16_t bonding_port_id, uint32_t internal_ms)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;
	internals->link_status_polling_interval_ms = internal_ms;

	return 0;
}

int
rte_eth_bond_link_monitoring_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	return internals->link_status_polling_interval_ms;
}

int
rte_eth_bond_link_down_prop_delay_set(uint16_t bonding_port_id,
				       uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;
	internals->link_down_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_down_prop_delay_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	return internals->link_down_delay_ms;
}

int
rte_eth_bond_link_up_prop_delay_set(uint16_t bonding_port_id, uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;
	internals->link_up_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_up_prop_delay_get(uint16_t bonding_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonding_port_id(bonding_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonding_port_id].data->dev_private;

	return internals->link_up_delay_ms;
}
