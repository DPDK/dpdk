/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2012 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 */

#ifndef __NETCFG_H
#define __NETCFG_H

#include <fman.h>
#include <argp.h>

/* Configuration information related to a specific ethernet port */
struct fm_eth_port_cfg {
	/**< A list of PCD FQ ranges, obtained from FMC configuration */
	struct list_head *list;
	/**< The "Rx default" FQID, obtained from FMC configuration */
	uint32_t rx_def;
	/**< Other interface details are in the fman driver interface */
	struct fman_if *fman_if;
};

struct netcfg_info {
	uint8_t num_ethports;
	/**< Number of ports */
	struct fm_eth_port_cfg port_cfg[0];
	/**< Variable structure array of size num_ethports */
};

struct interface_info {
	char *name;
	struct ether_addr mac_addr;
	struct ether_addr peer_mac;
	int mac_present;
	int fman_enabled_mac_interface;
};

struct netcfg_interface {
	uint8_t numof_netcfg_interface;
	uint8_t numof_fman_enabled_macless;
	struct interface_info interface_info[0];
};

/* pcd_file: FMC netpcd XML ("policy") file, that contains PCD information.
 * cfg_file: FMC config XML file
 * Returns the configuration information in newly allocated memory.
 */
struct netcfg_info *netcfg_acquire(void);

/* cfg_ptr: configuration information pointer.
 * Frees the resources allocated by the configuration layer.
 */
void netcfg_release(struct netcfg_info *cfg_ptr);

#ifdef RTE_LIBRTE_DPAA_DEBUG_DRIVER
/* cfg_ptr: configuration information pointer.
 * This function dumps configuration data to stdout.
 */
void dump_netcfg(struct netcfg_info *cfg_ptr);
#endif

#endif /* __NETCFG_H */
