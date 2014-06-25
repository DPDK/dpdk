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

#include <rte_devargs.h>
#include <rte_kvargs.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_private.h"

const char *pmd_bond_init_valid_arguments[] = {
	PMD_BOND_SLAVE_PORT_KVARG,
	PMD_BOND_PRIMARY_SLAVE_KVARG,
	PMD_BOND_MODE_KVARG,
	PMD_BOND_XMIT_POLICY_KVARG,
	PMD_BOND_SOCKET_ID_KVARG,
	PMD_BOND_MAC_ADDR_KVARG,

	NULL
};

static inline int
find_port_id_by_pci_addr(const struct rte_pci_addr *pci_addr)
{
	struct rte_pci_addr *eth_pci_addr;
	unsigned i;

	for (i = 0; i < rte_eth_dev_count(); i++) {

		if (rte_eth_devices[i].pci_dev == NULL)
			continue;

		eth_pci_addr = &(rte_eth_devices[i].pci_dev->addr);

		if (pci_addr->bus == eth_pci_addr->bus &&
			pci_addr->devid == eth_pci_addr->devid &&
			pci_addr->domain == eth_pci_addr->domain &&
			pci_addr->function == eth_pci_addr->function)
			return i;
	}
	return -1;
}

static inline int
find_port_id_by_dev_name(const char *name)
{
	unsigned i;

	for (i = 0; i < rte_eth_dev_count(); i++) {
		if (rte_eth_devices[i].data == NULL)
			continue;

		if (strcmp(rte_eth_devices[i].data->name, name) == 0)
			return i;
	}
	return -1;
}

/**
 * Parses a port identifier string to a port id by pci address, then by name,
 * and finally port id.
 */
static inline int
parse_port_id(const char *port_str)
{
	struct rte_pci_addr dev_addr;
	int port_id;

	/* try parsing as pci address, physical devices */
	if (eal_parse_pci_DomBDF(port_str, &dev_addr) == 0) {
		port_id = find_port_id_by_pci_addr(&dev_addr);
		if (port_id < 0)
			return -1;
	} else {
		/* try parsing as device name, virtual devices */
		port_id = find_port_id_by_dev_name(port_str);
		if (port_id < 0) {
			char *end;
			errno = 0;

			/* try parsing as port id */
			port_id = strtol(port_str, &end, 10);
			if (*end != 0 || errno != 0)
				return -1;
		}
	}

	if (port_id < 0 || port_id > RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, PMD, "Invalid slave port value (%s) specified.\n",
				port_str);
		return -1;
	}
	return port_id;
}

int
bond_ethdev_parse_slave_port_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct bond_ethdev_slave_ports *slave_ports;

	if (value == NULL || extra_args == NULL)
		return -1;

	slave_ports = extra_args;

	if (strcmp(key, PMD_BOND_SLAVE_PORT_KVARG) == 0) {
		int port_id = parse_port_id(value);
		if (port_id < 0)
			return -1;
		else
			slave_ports->slaves[slave_ports->slave_count++] =
					(uint8_t)port_id;
	}
	return 0;
}

int
bond_ethdev_parse_slave_mode_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint8_t *mode;
	char *endptr;

	if (value == NULL || extra_args == NULL)
		return -1;

	mode = extra_args;

	errno = 0;
	*mode = strtol(value, &endptr, 10);
	if (*endptr != 0 || errno != 0)
		return -1;

	/* validate mode value */
	switch (*mode) {
	case BONDING_MODE_ROUND_ROBIN:
	case BONDING_MODE_ACTIVE_BACKUP:
	case BONDING_MODE_BALANCE:
	case BONDING_MODE_BROADCAST:
		return 0;
	default:
		return -1;
	}
}

int
bond_ethdev_parse_socket_id_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int socket_id;
	char *endptr;

	if (value == NULL || extra_args == NULL)
		return -1;

	errno = 0;
	socket_id = (uint8_t)strtol(value, &endptr, 10);
	if (*endptr != 0 || errno != 0)
		return -1;

	/* validate mode value */
	if (socket_id >= 0 && socket_id < number_of_sockets()) {
		*(uint8_t *)extra_args = (uint8_t)socket_id;
		return 0;
	}
	return -1;
}

int
bond_ethdev_parse_primary_slave_port_id_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int primary_slave_port_id;

	if (value == NULL || extra_args == NULL)
		return -1;

	primary_slave_port_id = parse_port_id(value);
	if (primary_slave_port_id < 0)
		return -1;

	*(uint8_t *)extra_args = (uint8_t)primary_slave_port_id;

	return 0;
}

int
bond_ethdev_parse_balance_xmit_policy_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint8_t *xmit_policy;

	if (value == NULL || extra_args == NULL)
		return -1;

	xmit_policy = extra_args;

	if (strcmp(PMD_BOND_XMIT_POLICY_LAYER2_KVARG, value) == 0)
		*xmit_policy = BALANCE_XMIT_POLICY_LAYER2;
	else if (strcmp(PMD_BOND_XMIT_POLICY_LAYER23_KVARG, value) == 0)
		*xmit_policy = BALANCE_XMIT_POLICY_LAYER23;
	else if (strcmp(PMD_BOND_XMIT_POLICY_LAYER34_KVARG, value) == 0)
		*xmit_policy = BALANCE_XMIT_POLICY_LAYER34;
	else
		return -1;

	return 0;
}

int
bond_ethdev_parse_bond_mac_addr_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	if (value == NULL || extra_args == NULL)
		return -1;

	/* Parse MAC */
	return cmdline_parse_etheraddr(NULL, value, extra_args);
}
