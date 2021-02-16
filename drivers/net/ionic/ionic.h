/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_H_
#define _IONIC_H_

#include <stdint.h>
#include <inttypes.h>

#include <rte_bus_pci.h>

#include "ionic_dev.h"
#include "ionic_if.h"
#include "ionic_osdep.h"

#define IONIC_DRV_NAME			"ionic"
#define IONIC_DRV_DESCRIPTION		"Pensando Ethernet NIC Driver"
#define IONIC_DRV_VERSION		"0.11.0-49"

/* Vendor ID */
#define IONIC_PENSANDO_VENDOR_ID	0x1dd8

/* Device IDs */
#define IONIC_DEV_ID_ETH_PF		0x1002
#define IONIC_DEV_ID_ETH_VF		0x1003
#define IONIC_DEV_ID_ETH_MGMT		0x1004

enum ionic_mac_type {
	IONIC_MAC_UNKNOWN = 0,
	IONIC_MAC_CAPRI,
	IONIC_NUM_MACS
};

struct ionic_mac_info {
	enum ionic_mac_type type;
};

struct ionic_hw {
	struct ionic_mac_info mac;
	uint16_t device_id;
	uint16_t vendor_id;
};

/*
 * Structure to store private data for each driver instance (for each adapter).
 */
struct ionic_adapter {
	struct ionic_hw hw;
	struct ionic_dev idev;
	const char *name;
	struct ionic_dev_bar bars[IONIC_BARS_MAX];
	struct ionic_identity	ident;
	struct ionic_lif *lif;
	uint32_t num_bars;
	uint32_t max_ntxqs_per_lif;
	uint32_t max_nrxqs_per_lif;
	uint32_t max_mac_addrs;
	uint32_t link_speed;
	uint32_t nintrs;
	bool intrs[IONIC_INTR_CTRL_REGS_MAX];
	bool link_up;
	char fw_version[IONIC_DEVINFO_FWVERS_BUFLEN];
	struct rte_pci_device *pci_dev;
	LIST_ENTRY(ionic_adapter) pci_adapters;
};

/** ionic_admin_ctx - Admin command context.
 * @pending_work:       Flag that indicates a completion.
 * @cmd:                Admin command (64B) to be copied to the queue.
 * @comp:               Admin completion (16B) copied from the queue.
 */
struct ionic_admin_ctx {
	bool pending_work;
	union ionic_adminq_cmd cmd;
	union ionic_adminq_comp comp;
};

int ionic_adminq_post_wait(struct ionic_lif *lif, struct ionic_admin_ctx *ctx);

int ionic_dev_cmd_wait_check(struct ionic_dev *idev, unsigned long max_wait);
int ionic_setup(struct ionic_adapter *adapter);

int ionic_identify(struct ionic_adapter *adapter);
int ionic_init(struct ionic_adapter *adapter);
int ionic_reset(struct ionic_adapter *adapter);

int ionic_port_identify(struct ionic_adapter *adapter);
int ionic_port_init(struct ionic_adapter *adapter);
int ionic_port_reset(struct ionic_adapter *adapter);

#endif /* _IONIC_H_ */
