/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <string.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_tcp.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_private.h"
#include "rte_eth_bond_8023ad_private.h"

#define DEFAULT_POLLING_INTERVAL_10_MS (10)

int
valid_bonded_ethdev(const struct rte_eth_dev *eth_dev)
{
	/* Check valid pointer */
	if (eth_dev->driver->pci_drv.name == NULL)
		return -1;

	/* return 0 if driver name matches */
	return eth_dev->driver->pci_drv.name != pmd_bond_driver_name;
}

int
valid_bonded_port_id(uint8_t port_id)
{
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	return valid_bonded_ethdev(&rte_eth_devices[port_id]);
}

int
valid_slave_port_id(uint8_t port_id)
{
	/* Verify that port id's are valid */
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	/* Verify that port_id refers to a non bonded port */
	if (!valid_bonded_ethdev(&rte_eth_devices[port_id]))
		return -1;

	return 0;
}

void
activate_slave(struct rte_eth_dev *eth_dev, uint8_t port_id)
{
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint8_t active_count = internals->active_slave_count;

	if (internals->mode == BONDING_MODE_8023AD)
		bond_mode_8023ad_activate_slave(eth_dev, port_id);

	if (internals->mode == BONDING_MODE_TLB
			|| internals->mode == BONDING_MODE_ALB) {

		internals->tlb_slaves_order[active_count] = port_id;
	}

	RTE_VERIFY(internals->active_slave_count <
			(RTE_DIM(internals->active_slaves) - 1));

	internals->active_slaves[internals->active_slave_count] = port_id;
	internals->active_slave_count++;

	if (internals->mode == BONDING_MODE_TLB)
		bond_tlb_activate_slave(internals);
	if (internals->mode == BONDING_MODE_ALB)
		bond_mode_alb_client_list_upd(eth_dev);
}

void
deactivate_slave(struct rte_eth_dev *eth_dev, uint8_t port_id)
{
	uint8_t slave_pos;
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	uint8_t active_count = internals->active_slave_count;

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

	RTE_VERIFY(active_count < RTE_DIM(internals->active_slaves));
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

uint8_t
number_of_sockets(void)
{
	int sockets = 0;
	int i;
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();

	for (i = 0; ((i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL)); i++) {
		if (sockets < ms[i].socket_id)
			sockets = ms[i].socket_id;
	}

	/* Number of sockets = maximum socket_id + 1 */
	return ++sockets;
}

const char pmd_bond_driver_name[] = "rte_bond_pmd";

static struct rte_pci_id pci_id_table = {
	.device_id = PCI_ANY_ID,
	.subsystem_device_id = PCI_ANY_ID,
	.vendor_id = PCI_ANY_ID,
	.subsystem_vendor_id = PCI_ANY_ID,
};

static struct eth_driver rte_bond_pmd = {
	.pci_drv = {
		.name = pmd_bond_driver_name,
		.drv_flags = RTE_PCI_DRV_INTR_LSC | RTE_PCI_DRV_DETACHABLE,
		.id_table = &pci_id_table,
	},
};

int
rte_eth_bond_create(const char *name, uint8_t mode, uint8_t socket_id)
{
	struct rte_pci_device *pci_dev = NULL;
	struct bond_dev_private *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_pci_driver *pci_drv = NULL;

	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (private) data
	 */

	if (name == NULL) {
		RTE_BOND_LOG(ERR, "Invalid name specified");
		goto err;
	}

	if (socket_id >= number_of_sockets()) {
		RTE_BOND_LOG(ERR,
				"Invalid socket id specified to create bonded device on.");
		goto err;
	}

	pci_dev = rte_zmalloc_socket(name, sizeof(*pci_dev), 0, socket_id);
	if (pci_dev == NULL) {
		RTE_BOND_LOG(ERR, "Unable to malloc pci dev on socket");
		goto err;
	}

	pci_drv = &rte_bond_pmd.pci_drv;

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, socket_id);
	if (internals == NULL) {
		RTE_BOND_LOG(ERR, "Unable to malloc internals on socket");
		goto err;
	}

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (eth_dev == NULL) {
		RTE_BOND_LOG(ERR, "Unable to allocate rte_eth_dev");
		goto err;
	}

	pci_dev->numa_node = socket_id;
	pci_drv->name = pmd_bond_driver_name;
	pci_dev->driver = pci_drv;

	eth_dev->driver = &rte_bond_pmd;
	eth_dev->data->dev_private = internals;
	eth_dev->data->nb_rx_queues = (uint16_t)1;
	eth_dev->data->nb_tx_queues = (uint16_t)1;

	TAILQ_INIT(&(eth_dev->link_intr_cbs));

	eth_dev->data->dev_link.link_status = 0;

	eth_dev->data->mac_addrs = rte_zmalloc_socket(name, ETHER_ADDR_LEN, 0,
			socket_id);
	if (eth_dev->data->mac_addrs == NULL) {
		RTE_BOND_LOG(ERR, "Unable to malloc mac_addrs");
		goto err;
	}

	eth_dev->data->dev_started = 0;
	eth_dev->data->promiscuous = 0;
	eth_dev->data->scattered_rx = 0;
	eth_dev->data->all_multicast = 0;

	eth_dev->dev_ops = &default_dev_ops;
	eth_dev->pci_dev = pci_dev;

	rte_spinlock_init(&internals->lock);

	internals->port_id = eth_dev->data->port_id;
	internals->mode = BONDING_MODE_INVALID;
	internals->current_primary_port = 0;
	internals->balance_xmit_policy = BALANCE_XMIT_POLICY_LAYER2;
	internals->xmit_hash = xmit_l2_hash;
	internals->user_defined_mac = 0;
	internals->link_props_set = 0;

	internals->link_status_polling_enabled = 0;

	internals->link_status_polling_interval_ms = DEFAULT_POLLING_INTERVAL_10_MS;
	internals->link_down_delay_ms = 0;
	internals->link_up_delay_ms = 0;

	internals->slave_count = 0;
	internals->active_slave_count = 0;
	internals->rx_offload_capa = 0;
	internals->tx_offload_capa = 0;

	memset(internals->active_slaves, 0, sizeof(internals->active_slaves));
	memset(internals->slaves, 0, sizeof(internals->slaves));

	/* Set mode 4 default configuration */
	bond_mode_8023ad_setup(eth_dev, NULL);
	if (bond_ethdev_mode_set(eth_dev, mode)) {
		RTE_BOND_LOG(ERR, "Failed to set bonded device %d mode too %d",
				 eth_dev->data->port_id, mode);
		goto err;
	}

	return eth_dev->data->port_id;

err:
	rte_free(pci_dev);
	rte_free(internals);
	if (eth_dev != NULL) {
		rte_free(eth_dev->data->mac_addrs);
		rte_eth_dev_release_port(eth_dev);
	}
	return -1;
}

int
rte_eth_bond_free(const char *name)
{
	struct rte_eth_dev *eth_dev = NULL;

	/* now free all data allocation - for eth_dev structure,
	 * dummy pci driver and internal (private) data
	 */

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -ENODEV;

	if (eth_dev->data->dev_started == 1) {
		bond_ethdev_stop(eth_dev);
		bond_ethdev_close(eth_dev);
	}

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	rte_free(eth_dev->pci_dev);
	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data->mac_addrs);

	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static int
__eth_bond_slave_add_lock_free(uint8_t bonded_port_id, uint8_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev, *slave_eth_dev;
	struct bond_dev_private *internals;
	struct bond_dev_private *temp_internals;
	struct rte_eth_link link_props;
	struct rte_eth_dev_info dev_info;

	int i, j;

	if (valid_slave_port_id(slave_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	/* Verify that new slave device is not already a slave of another
	 * bonded device */
	for (i = rte_eth_dev_count()-1; i >= 0; i--) {
		if (valid_bonded_ethdev(&rte_eth_devices[i]) == 0) {
			temp_internals = rte_eth_devices[i].data->dev_private;

			for (j = 0; j < temp_internals->slave_count; j++) {
				/* Device already a slave of a bonded device */
				if (temp_internals->slaves[j].port_id == slave_port_id) {
					RTE_BOND_LOG(ERR, "Slave port %d is already a slave",
							slave_port_id);
					return -1;
				}
			}
		}
	}

	slave_eth_dev = &rte_eth_devices[slave_port_id];

	/* Add slave details to bonded device */
	slave_add(internals, slave_eth_dev);

	rte_eth_dev_info_get(slave_port_id, &dev_info);

	if (internals->slave_count < 1) {
		/* if MAC is not user defined then use MAC of first slave add to
		 * bonded device */
		if (!internals->user_defined_mac)
			mac_address_set(bonded_eth_dev, slave_eth_dev->data->mac_addrs);

		/* Inherit eth dev link properties from first slave */
		link_properties_set(bonded_eth_dev,
				&(slave_eth_dev->data->dev_link));

		/* Make primary slave */
		internals->primary_port = slave_port_id;

		/* Take the first dev's offload capabilities */
		internals->rx_offload_capa = dev_info.rx_offload_capa;
		internals->tx_offload_capa = dev_info.tx_offload_capa;

	} else {
		/* Check slave link properties are supported if props are set,
		 * all slaves must be the same */
		if (internals->link_props_set) {
			if (link_properties_valid(&(bonded_eth_dev->data->dev_link),
									  &(slave_eth_dev->data->dev_link))) {
				RTE_BOND_LOG(ERR,
						"Slave port %d link speed/duplex not supported",
						slave_port_id);
				return -1;
			}
		} else {
			link_properties_set(bonded_eth_dev,
					&(slave_eth_dev->data->dev_link));
		}
		internals->rx_offload_capa &= dev_info.rx_offload_capa;
		internals->tx_offload_capa &= dev_info.tx_offload_capa;
	}

	internals->slave_count++;

	/* Update all slave devices MACs*/
	mac_address_slaves_update(bonded_eth_dev);

	if (bonded_eth_dev->data->dev_started) {
		if (slave_configure(bonded_eth_dev, slave_eth_dev) != 0) {
			RTE_BOND_LOG(ERR, "rte_bond_slaves_configure: port=%d",
					slave_port_id);
			return -1;
		}
	}

	/* Register link status change callback with bonded device pointer as
	 * argument*/
	rte_eth_dev_callback_register(slave_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback, &bonded_eth_dev->data->port_id);

	/* If bonded device is started then we can add the slave to our active
	 * slave array */
	if (bonded_eth_dev->data->dev_started) {
		rte_eth_link_get_nowait(slave_port_id, &link_props);

		 if (link_props.link_status == 1)
			activate_slave(bonded_eth_dev, slave_port_id);
	}
	return 0;

}

int
rte_eth_bond_slave_add(uint8_t bonded_port_id, uint8_t slave_port_id)
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
__eth_bond_slave_remove_lock_free(uint8_t bonded_port_id, uint8_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev;
	struct bond_dev_private *internals;

	int i, slave_idx;

	if (valid_slave_port_id(slave_port_id) != 0)
		return -1;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

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
	mac_address_set(&rte_eth_devices[slave_port_id],
			&(internals->slaves[slave_idx].persisted_mac_addr));

	slave_remove(internals, &rte_eth_devices[slave_port_id]);

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
		/* reset device link properties as no slaves are active */
		link_properties_reset(&rte_eth_devices[bonded_port_id]);

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
	}
	return 0;
}

int
rte_eth_bond_slave_remove(uint8_t bonded_port_id, uint8_t slave_port_id)
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
rte_eth_bond_mode_set(uint8_t bonded_port_id, uint8_t mode)
{
	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	return bond_ethdev_mode_set(&rte_eth_devices[bonded_port_id], mode);
}

int
rte_eth_bond_mode_get(uint8_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->mode;
}

int
rte_eth_bond_primary_set(uint8_t bonded_port_id, uint8_t slave_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	if (valid_slave_port_id(slave_port_id) != 0)
		return -1;

	internals =  rte_eth_devices[bonded_port_id].data->dev_private;

	internals->user_defined_primary_port = 1;
	internals->primary_port = slave_port_id;

	bond_ethdev_primary_set(internals, slave_port_id);

	return 0;
}

int
rte_eth_bond_primary_get(uint8_t bonded_port_id)
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
rte_eth_bond_slaves_get(uint8_t bonded_port_id, uint8_t slaves[], uint8_t len)
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
rte_eth_bond_active_slaves_get(uint8_t bonded_port_id, uint8_t slaves[],
		uint8_t len)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	if (slaves == NULL)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (internals->active_slave_count > len)
		return -1;

	memcpy(slaves, internals->active_slaves, internals->active_slave_count);

	return internals->active_slave_count;
}

int
rte_eth_bond_mac_address_set(uint8_t bonded_port_id,
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
rte_eth_bond_mac_address_reset(uint8_t bonded_port_id)
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
rte_eth_bond_xmit_policy_set(uint8_t bonded_port_id, uint8_t policy)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	switch (policy) {
	case BALANCE_XMIT_POLICY_LAYER2:
		internals->balance_xmit_policy = policy;
		internals->xmit_hash = xmit_l2_hash;
		break;
	case BALANCE_XMIT_POLICY_LAYER23:
		internals->balance_xmit_policy = policy;
		internals->xmit_hash = xmit_l23_hash;
		break;
	case BALANCE_XMIT_POLICY_LAYER34:
		internals->balance_xmit_policy = policy;
		internals->xmit_hash = xmit_l34_hash;
		break;

	default:
		return -1;
	}
	return 0;
}

int
rte_eth_bond_xmit_policy_get(uint8_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->balance_xmit_policy;
}

int
rte_eth_bond_link_monitoring_set(uint8_t bonded_port_id, uint32_t internal_ms)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_status_polling_interval_ms = internal_ms;

	return 0;
}

int
rte_eth_bond_link_monitoring_get(uint8_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_status_polling_interval_ms;
}

int
rte_eth_bond_link_down_prop_delay_set(uint8_t bonded_port_id, uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_down_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_down_prop_delay_get(uint8_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_down_delay_ms;
}

int
rte_eth_bond_link_up_prop_delay_set(uint8_t bonded_port_id, uint32_t delay_ms)

{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;
	internals->link_up_delay_ms = delay_ms;

	return 0;
}

int
rte_eth_bond_link_up_prop_delay_get(uint8_t bonded_port_id)
{
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	return internals->link_up_delay_ms;
}
