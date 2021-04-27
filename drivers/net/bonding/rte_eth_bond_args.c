/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_devargs.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_kvargs.h>

#include "rte_eth_bond.h"
#include "eth_bond_private.h"

const char *pmd_bond_init_valid_arguments[] = {
	PMD_BOND_SLAVE_PORT_KVARG,
	PMD_BOND_PRIMARY_SLAVE_KVARG,
	PMD_BOND_MODE_KVARG,
	PMD_BOND_XMIT_POLICY_KVARG,
	PMD_BOND_SOCKET_ID_KVARG,
	PMD_BOND_MAC_ADDR_KVARG,
	PMD_BOND_AGG_MODE_KVARG,
	"driver",
	NULL
};

static inline int
bond_pci_addr_cmp(const struct rte_device *dev, const void *_pci_addr)
{
	const struct rte_pci_device *pdev = RTE_DEV_TO_PCI_CONST(dev);
	const struct rte_pci_addr *paddr = _pci_addr;

	return rte_pci_addr_cmp(&pdev->addr, paddr);
}

static inline int
find_port_id_by_pci_addr(const struct rte_pci_addr *pci_addr)
{
	struct rte_bus *pci_bus;
	struct rte_device *dev;
	unsigned i;

	pci_bus = rte_bus_find_by_name("pci");
	if (pci_bus == NULL) {
		RTE_BOND_LOG(ERR, "No PCI bus found");
		return -1;
	}

	dev = pci_bus->find_device(NULL, bond_pci_addr_cmp, pci_addr);
	if (dev == NULL) {
		RTE_BOND_LOG(ERR, "unable to find PCI device");
		return -1;
	}

	RTE_ETH_FOREACH_DEV(i)
		if (rte_eth_devices[i].device == dev)
			return i;
	return -1;
}

static inline int
find_port_id_by_dev_name(const char *name)
{
	unsigned i;

	RTE_ETH_FOREACH_DEV(i) {
		if (rte_eth_devices[i].data == NULL)
			continue;

		if (strcmp(rte_eth_devices[i].device->name, name) == 0)
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
	if (rte_pci_addr_parse(port_str, &dev_addr) == 0) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		RTE_BOND_LOG(ERR, "Specified port (%s) is invalid", port_str);
		return -1;
	}
	return port_id;
}

int
bond_ethdev_parse_slave_port_kvarg(const char *key,
		const char *value, void *extra_args)
{
	struct bond_ethdev_slave_ports *slave_ports;

	if (value == NULL || extra_args == NULL)
		return -1;

	slave_ports = extra_args;

	if (strcmp(key, PMD_BOND_SLAVE_PORT_KVARG) == 0) {
		int port_id = parse_port_id(value);
		if (port_id < 0) {
			RTE_BOND_LOG(ERR, "Invalid slave port value (%s) specified",
				     value);
			return -1;
		} else
			slave_ports->slaves[slave_ports->slave_count++] =
					port_id;
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
	case BONDING_MODE_8023AD:
	case BONDING_MODE_TLB:
	case BONDING_MODE_ALB:
		return 0;
	default:
		RTE_BOND_LOG(ERR, "Invalid slave mode value (%s) specified", value);
		return -1;
	}
}

int
bond_ethdev_parse_slave_agg_mode_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint8_t *agg_mode;

	if (value == NULL || extra_args == NULL)
		return -1;

	agg_mode = extra_args;

	errno = 0;
	if (strncmp(value, "stable", 6) == 0)
		*agg_mode = AGG_STABLE;

	if (strncmp(value, "bandwidth", 9) == 0)
		*agg_mode = AGG_BANDWIDTH;

	if (strncmp(value, "count", 5) == 0)
		*agg_mode = AGG_COUNT;

	switch (*agg_mode) {
	case AGG_STABLE:
	case AGG_BANDWIDTH:
	case AGG_COUNT:
		return 0;
	default:
		RTE_BOND_LOG(ERR, "Invalid agg mode value stable/bandwidth/count");
		return -1;
	}
}

int
bond_ethdev_parse_socket_id_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	long socket_id;
	char *endptr;

	if (value == NULL || extra_args == NULL)
		return -1;

	errno = 0;
	socket_id = strtol(value, &endptr, 10);
	if (*endptr != 0 || errno != 0)
		return -1;

	/* validate socket id value */
	if (socket_id >= 0 && socket_id < RTE_MAX_NUMA_NODES) {
		*(int *)extra_args = (int)socket_id;
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

	*(uint16_t *)extra_args = (uint16_t)primary_slave_port_id;

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
	return rte_ether_unformat_addr(value, extra_args);
}

int
bond_ethdev_parse_time_ms_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint32_t time_ms;
	char *endptr;

	if (value == NULL || extra_args == NULL)
		return -1;

	errno = 0;
	time_ms = (uint32_t)strtol(value, &endptr, 10);
	if (*endptr != 0 || errno != 0)
		return -1;

	*(uint32_t *)extra_args = time_ms;

	return 0;
}
