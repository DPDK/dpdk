/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_ETHDEV_H
#define ZXDH_ETHDEV_H

#include <rte_ether.h>
#include "ethdev_driver.h"
#include <rte_interrupts.h>
#include <eal_interrupts.h>

/* ZXDH PCI vendor/device ID. */
#define ZXDH_PCI_VENDOR_ID        0x1cf2

#define ZXDH_E310_PF_DEVICEID     0x8061
#define ZXDH_E310_VF_DEVICEID     0x8062
#define ZXDH_E312_PF_DEVICEID     0x8049
#define ZXDH_E312_VF_DEVICEID     0x8060

#define ZXDH_MAX_UC_MAC_ADDRS     32
#define ZXDH_MAX_MC_MAC_ADDRS     32
#define ZXDH_MAX_MAC_ADDRS        (ZXDH_MAX_UC_MAC_ADDRS + ZXDH_MAX_MC_MAC_ADDRS)

#define ZXDH_NUM_BARS             2
#define ZXDH_RX_QUEUES_MAX        128U
#define ZXDH_TX_QUEUES_MAX        128U
#define ZXDH_MIN_RX_BUFSIZE       64
#define ZXDH_MAX_RX_PKTLEN        14000U
#define ZXDH_QUEUE_DEPTH          1024
#define ZXDH_QUEUES_BASE          0
#define ZXDH_TOTAL_QUEUES_NUM     4096
#define ZXDH_QUEUES_NUM_MAX       256
#define ZXDH_QUERES_SHARE_BASE    (0x5000)

#define ZXDH_MBUF_BURST_SZ        64

union zxdh_virport_num {
	uint16_t vport;
	struct {
		uint16_t vfid:8;
		uint16_t pfid:3;
		uint16_t vf_flag:1;
		uint16_t epid:3;
		uint16_t direct_flag:1;
	};
};

struct zxdh_chnl_context {
	uint16_t valid;
	uint16_t ph_chno;
};

struct zxdh_hw {
	struct rte_eth_dev *eth_dev;
	struct zxdh_pci_common_cfg *common_cfg;
	struct zxdh_net_config *dev_cfg;
	struct rte_intr_handle *risc_intr;
	struct rte_intr_handle *dtb_intr;
	struct zxdh_virtqueue **vqs;
	struct zxdh_chnl_context channel_context[ZXDH_QUEUES_NUM_MAX];
	union zxdh_virport_num vport;

	uint64_t bar_addr[ZXDH_NUM_BARS];
	uint64_t host_features;
	uint64_t guest_features;
	uint32_t max_queue_pairs;
	uint32_t speed;
	uint32_t notify_off_multiplier;
	uint16_t *notify_base;
	uint16_t pcie_id;
	uint16_t device_id;
	uint16_t port_id;
	uint16_t vfid;
	uint16_t queue_num;

	uint8_t *isr;
	uint8_t weak_barriers;
	uint8_t intr_enabled;
	uint8_t use_msix;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];

	uint8_t duplex;
	uint8_t is_pf;
	uint8_t msg_chan_init;
	uint8_t phyport;
	uint8_t panel_id;
	uint8_t has_tx_offload;
	uint8_t has_rx_offload;
};

uint16_t zxdh_vport_to_vfid(union zxdh_virport_num v);

#endif /* ZXDH_ETHDEV_H */
