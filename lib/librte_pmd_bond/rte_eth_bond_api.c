/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_tcp.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_private.h"

int
valid_bonded_ethdev(struct rte_eth_dev *eth_dev)
{
	size_t len;

	/* Check valid pointer */
	if (eth_dev->driver->pci_drv.name == NULL || driver_name == NULL)
		return -1;

	/* Check string lengths are equal */
	len = strlen(driver_name);
	if (strlen(eth_dev->driver->pci_drv.name) != len)
		return -1;

	/* Compare strings */
	return strncmp(eth_dev->driver->pci_drv.name, driver_name, len);
}

int
valid_port_id(uint8_t port_id)
{
	/* Verify that port id is valid */
	int ethdev_count = rte_eth_dev_count();
	if (port_id >= ethdev_count) {
		RTE_LOG(ERR, PMD,
				"%s: port Id %d is greater than rte_eth_dev_count %d\n",
				__func__, port_id, ethdev_count);
		return -1;
	}

	return 0;
}

int
valid_bonded_port_id(uint8_t port_id)
{
	/* Verify that port id's are valid */
	if (valid_port_id(port_id))
		return -1;

	/* Verify that bonded_port_id refers to a bonded port */
	if (valid_bonded_ethdev(&rte_eth_devices[port_id])) {
		RTE_LOG(ERR, PMD,
				"%s: Specified port Id %d is not a bonded eth_dev device\n",
				__func__, port_id);
		return -1;
	}

	return 0;
}

int
valid_slave_port_id(uint8_t port_id)
{
	/* Verify that port id's are valid */
	if (valid_port_id(port_id))
		return -1;

	/* Verify that port_id refers to a non bonded port */
	if (!valid_bonded_ethdev(&rte_eth_devices[port_id]))
		return -1;

	return 0;
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

const char *driver_name = "Link Bonding PMD";

int
rte_eth_bond_create(const char *name, uint8_t mode, uint8_t socket_id)
{
	struct rte_pci_device *pci_dev = NULL;
	struct bond_dev_private *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct eth_driver *eth_drv = NULL;
	struct rte_pci_driver *pci_drv = NULL;
	struct rte_pci_id *pci_id_table = NULL;
	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (private) data
	 */

	if (name == NULL) {
		RTE_LOG(ERR, PMD, "Invalid name specified\n");
		goto err;
	}

	if (socket_id >= number_of_sockets()) {
		RTE_LOG(ERR, PMD,
				"%s: invalid socket id specified to create bonded device on.\n",
				__func__);
		goto err;
	}

	pci_dev = rte_zmalloc_socket(name, sizeof(*pci_dev), 0, socket_id);
	if (pci_dev == NULL) {
		RTE_LOG(ERR, PMD, "Unable to malloc pci dev on socket\n");
		goto err;
	}

	eth_drv = rte_zmalloc_socket(name, sizeof(*eth_drv), 0, socket_id);
	if (eth_drv == NULL) {
		RTE_LOG(ERR, PMD, "Unable to malloc eth_drv on socket\n");
		goto err;
	}

	pci_drv = rte_zmalloc_socket(name, sizeof(*pci_drv), 0, socket_id);
	if (pci_drv == NULL) {
		RTE_LOG(ERR, PMD, "Unable to malloc pci_drv on socket\n");
		goto err;
	}
	pci_id_table = rte_zmalloc_socket(name, sizeof(*pci_id_table), 0, socket_id);
	if (pci_drv == NULL) {
		RTE_LOG(ERR, PMD, "Unable to malloc pci_id_table on socket\n");
		goto err;
	}

	pci_drv->id_table = pci_id_table;

	pci_drv->id_table->device_id = PCI_ANY_ID;
	pci_drv->id_table->subsystem_device_id = PCI_ANY_ID;
	pci_drv->id_table->vendor_id = PCI_ANY_ID;
	pci_drv->id_table->subsystem_vendor_id = PCI_ANY_ID;

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, socket_id);
	if (internals == NULL) {
		RTE_LOG(ERR, PMD, "Unable to malloc internals on socket\n");
		goto err;
	}

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		RTE_LOG(ERR, PMD, "Unable to allocate rte_eth_dev\n");
		goto err;
	}

	pci_dev->numa_node = socket_id;
	pci_drv->name = driver_name;

	eth_drv->pci_drv = (struct rte_pci_driver)(*pci_drv);
	eth_dev->driver = eth_drv;

	eth_dev->data->dev_private = internals;
	eth_dev->data->nb_rx_queues = (uint16_t)1;
	eth_dev->data->nb_tx_queues = (uint16_t)1;

	eth_dev->data->dev_link.link_status = 0;

	eth_dev->data->mac_addrs = rte_zmalloc_socket(name, ETHER_ADDR_LEN, 0,
			socket_id);

	eth_dev->data->dev_started = 0;
	eth_dev->data->promiscuous = 0;
	eth_dev->data->scattered_rx = 0;
	eth_dev->data->all_multicast = 0;

	eth_dev->dev_ops = &default_dev_ops;
	eth_dev->pci_dev = pci_dev;

	if (bond_ethdev_mode_set(eth_dev, mode)) {
		RTE_LOG(ERR, PMD,
				"%s: failed to set bonded device %d mode too %d\n",
				__func__, eth_dev->data->port_id, mode);
		goto err;
	}

	internals->current_primary_port = 0;
	internals->balance_xmit_policy = BALANCE_XMIT_POLICY_LAYER2;
	internals->user_defined_mac = 0;
	internals->link_props_set = 0;
	internals->slave_count = 0;
	internals->active_slave_count = 0;

	memset(internals->active_slaves, 0, sizeof(internals->active_slaves));
	memset(internals->slaves, 0, sizeof(internals->slaves));

	memset(internals->presisted_slaves_conf, 0,
			sizeof(internals->presisted_slaves_conf));

	return eth_dev->data->port_id;

err:
	if (pci_dev)
		rte_free(pci_dev);
	if (pci_drv)
		rte_free(pci_drv);
	if (pci_id_table)
		rte_free(pci_id_table);
	if (eth_drv)
		rte_free(eth_drv);
	if (internals)
		rte_free(internals);
	return -1;
}

int
rte_eth_bond_slave_add(uint8_t bonded_port_id, uint8_t slave_port_id)
{
	struct rte_eth_dev *bonded_eth_dev, *slave_eth_dev;
	struct bond_dev_private *internals;
	struct bond_dev_private *temp_internals;
	struct rte_eth_link link_props;

	int i, j;

	/* Verify that port id's are valid bonded and slave ports */
	if (valid_bonded_port_id(bonded_port_id) != 0)
		goto err_add;

	if (valid_slave_port_id(slave_port_id) != 0)
		goto err_add;

	/*
	 * Verify that new slave device is not already a slave of another bonded
	 * device */
	for (i = rte_eth_dev_count()-1; i >= 0; i--) {
		if (valid_bonded_ethdev(&rte_eth_devices[i]) == 0) {
			temp_internals = rte_eth_devices[i].data->dev_private;
			for (j = 0; j < temp_internals->slave_count; j++) {
				/* Device already a slave of a bonded device */
				if (temp_internals->slaves[j] == slave_port_id)
					goto err_add;
			}
		}
	}

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	internals = bonded_eth_dev->data->dev_private;

	slave_eth_dev = &rte_eth_devices[slave_port_id];

	if (internals->slave_count > 0) {
		/* Check that new slave device is the same type as the other slaves
		 * and not repetitive */
		for (i = 0; i < internals->slave_count; i++) {
			if (slave_eth_dev->pci_dev->driver->id_table->device_id !=
					rte_eth_devices[internals->slaves[i]].pci_dev->driver->id_table->device_id ||
				internals->slaves[i] == slave_port_id)
				goto err_add;
		}
	}

	/* Add slave details to bonded device */
	internals->slaves[internals->slave_count] = slave_port_id;

	slave_config_store(internals, slave_eth_dev);

	if (internals->slave_count < 1) {
		/* if MAC is not user defined then use MAC of first slave add to bonded
		 * device */
		if (!internals->user_defined_mac)
			mac_address_set(bonded_eth_dev, slave_eth_dev->data->mac_addrs);

		/* Inherit eth dev link properties from first slave */
		link_properties_set(bonded_eth_dev, &(slave_eth_dev->data->dev_link));

		/* Make primary slave */
		internals->primary_port = slave_port_id;
	} else {
		/* Check slave link properties are supported if props are set,
		 * all slaves must be the same */
		if (internals->link_props_set) {
			if (link_properties_valid(&(bonded_eth_dev->data->dev_link),
									  &(slave_eth_dev->data->dev_link))) {
				RTE_LOG(ERR, PMD,
						"%s: Slave port %d link speed/duplex not supported\n",
						__func__, slave_port_id);
				goto err_add;
			}
		} else {
			link_properties_set(bonded_eth_dev,
					&(slave_eth_dev->data->dev_link));
		}
	}

	internals->slave_count++;

	/* Update all slave devices MACs*/
	mac_address_slaves_update(bonded_eth_dev);

	if (bonded_eth_dev->data->dev_started) {
		if (slave_configure(bonded_eth_dev, slave_eth_dev) != 0) {
			RTE_LOG(ERR, PMD, "rte_bond_slaves_configure: port=%d\n",
					slave_port_id);
			goto err_add;
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

		 if (link_props.link_status == 1) {
			internals->active_slaves[internals->active_slave_count++] =
					slave_port_id;
		}
	}

	return 0;

err_add:
	RTE_LOG(ERR, PMD, "Failed to add port %d as slave\n", slave_port_id);
	return -1;

}

int
rte_eth_bond_slave_remove(uint8_t bonded_port_id, uint8_t slave_port_id)
{
	struct bond_dev_private *internals;
	struct slave_conf *slave_conf;

	int i;
	int pos = -1;

	/* Verify that port id's are valid bonded and slave ports */
	if (valid_bonded_port_id(bonded_port_id) != 0)
		goto err_del;

	if (valid_slave_port_id(slave_port_id) != 0)
		goto err_del;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	/* first remove from active slave list */
	for (i = 0; i < internals->active_slave_count; i++) {
		if (internals->active_slaves[i] == slave_port_id)
			pos = i;

		/* shift active slaves up active array list */
		if (pos >= 0 && i < (internals->active_slave_count - 1))
			internals->active_slaves[i] = internals->active_slaves[i+1];
	}

	if (pos >= 0)
		internals->active_slave_count--;

	pos = -1;
	/* now remove from slave list */
	for (i = 0; i < internals->slave_count; i++) {
		if (internals->slaves[i] == slave_port_id)
			pos = i;

		/* shift slaves up list */
		if (pos >= 0 && i < internals->slave_count)
			internals->slaves[i] = internals->slaves[i+1];
	}

	if (pos < 0)
		goto err_del;

	/* Un-register link status change callback with bonded device pointer as
	 * argument*/
	rte_eth_dev_callback_unregister(slave_port_id, RTE_ETH_EVENT_INTR_LSC,
			bond_ethdev_lsc_event_callback,
			&rte_eth_devices[bonded_port_id].data->port_id);

	/* Restore original MAC address of slave device */
	slave_conf = slave_config_get(internals, slave_port_id);

	mac_address_set(&rte_eth_devices[slave_port_id], &(slave_conf->mac_addr));

	slave_config_clear(internals, &rte_eth_devices[slave_port_id]);

	internals->slave_count--;

	/*  first slave in the active list will be the primary by default,
	 *  otherwise use first device in list */
	if (internals->current_primary_port == slave_port_id) {
		if (internals->active_slave_count > 0)
			internals->current_primary_port = internals->active_slaves[0];
		else if (internals->slave_count > 0)
			internals->current_primary_port = internals->slaves[0];
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

	return 0;

err_del:
	RTE_LOG(ERR, PMD,
			"Cannot remove slave device (not present in bonded device)\n");
	return -1;

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

	if (valid_bonded_port_id(bonded_port_id) != 0)
		return -1;

	if (slaves == NULL)
		return -1;

	internals = rte_eth_devices[bonded_port_id].data->dev_private;

	if (internals->slave_count > len)
		return -1;

	memcpy(slaves, internals->slaves, internals->slave_count);

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
		struct slave_conf *conf;
		conf = slave_config_get(internals, internals->primary_port);

		/* Set MAC Address of Bonded Device */
		if (mac_address_set(bonded_eth_dev, &conf->mac_addr) != 0)
			return -1;

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
	case BALANCE_XMIT_POLICY_LAYER23:
	case BALANCE_XMIT_POLICY_LAYER34:
		internals->balance_xmit_policy = policy;
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
