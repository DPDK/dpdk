/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _OTX_EP_MBOX_H_
#define _OTX_EP_MBOX_H_

/*
 * When a new command is implemented, VF Mbox version should be bumped.
 */
enum octep_pfvf_mbox_version {
	OTX_EP_MBOX_VERSION_V0,
	OTX_EP_MBOX_VERSION_V1,
};

#define OTX_EP_MBOX_VERSION_CURRENT OTX_EP_MBOX_VERSION_V1

enum otx_ep_mbox_opcode {
	OTX_EP_MBOX_CMD_VERSION,
	OTX_EP_MBOX_CMD_SET_MTU,
	OTX_EP_MBOX_CMD_SET_MAC_ADDR,
	OTX_EP_MBOX_CMD_GET_MAC_ADDR,
	OTX_EP_MBOX_CMD_GET_LINK_INFO,
	OTX_EP_MBOX_CMD_GET_STATS,
	OTX_EP_MBOX_CMD_SET_RX_STATE,
	OTX_EP_MBOX_CMD_SET_LINK_STATUS,
	OTX_EP_MBOX_CMD_GET_LINK_STATUS,
	OTX_EP_MBOX_CMD_GET_MTU,
	OTX_EP_MBOX_CMD_DEV_REMOVE,
	OTX_EP_MBOX_CMD_MAX,
};

enum otx_ep_mbox_word_type {
	OTX_EP_MBOX_TYPE_CMD,
	OTX_EP_MBOX_TYPE_RSP_ACK,
	OTX_EP_MBOX_TYPE_RSP_NACK,
};

enum otx_ep_mbox_cmd_status {
	OTX_EP_MBOX_CMD_STATUS_NOT_SETUP = 1,
	OTX_EP_MBOX_CMD_STATUS_TIMEDOUT = 2,
	OTX_EP_MBOX_CMD_STATUS_NACK = 3,
	OTX_EP_MBOX_CMD_STATUS_BUSY = 4
};

enum otx_ep_mbox_state {
	OTX_EP_MBOX_STATE_IDLE = 0,
	OTX_EP_MBOX_STATE_BUSY = 1,
};

enum otx_ep_link_status {
	OTX_EP_LINK_STATUS_DOWN,
	OTX_EP_LINK_STATUS_UP,
};

enum otx_ep_link_duplex {
	OTX_EP_LINK_HALF_DUPLEX,
	OTX_EP_LINK_FULL_DUPLEX,
};

enum otx_ep_link_autoneg {
	OTX_EP_LINK_FIXED,
	OTX_EP_LINK_AUTONEG,
};

#define OTX_EP_MBOX_TIMEOUT_MS     1200
#define OTX_EP_MBOX_MAX_RETRIES    2
#define OTX_EP_MBOX_MAX_DATA_SIZE  6
#define OTX_EP_MBOX_MAX_DATA_BUF_SIZE 256
#define OTX_EP_MBOX_MORE_FRAG_FLAG 1
#define OTX_EP_MBOX_WRITE_WAIT_TIME msecs_to_jiffies(1)

union otx_ep_mbox_word {
	uint64_t u64;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t rsvd:6;
		uint64_t data:48;
	} s;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t frag:1;
		uint64_t rsvd:5;
		uint8_t data[6];
	} s_data;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t rsvd:6;
		uint64_t version:48;
	} s_version;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t rsvd:6;
		uint8_t mac_addr[6];
	} s_set_mac;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t rsvd:6;
		uint64_t mtu:48;
	} s_set_mtu;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t rsvd:6;
		uint64_t mtu:48;
	} s_get_mtu;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t state:1;
		uint64_t rsvd:53;
	} s_link_state;
	struct {
		uint64_t opcode:8;
		uint64_t type:2;
		uint64_t status:1;
		uint64_t rsvd:53;
	} s_link_status;
} __rte_packed;

/* Hardware interface link state information. */
struct otx_ep_iface_link_info {
	/* Bitmap of Supported link speeds/modes. */
	uint64_t supported_modes;

	/* Bitmap of Advertised link speeds/modes. */
	uint64_t advertised_modes;

	/* Negotiated link speed in Mbps. */
	uint32_t speed;

	/* MTU */
	uint16_t mtu;

	/* Autonegotiation state. */
#define OCTEP_VF_LINK_MODE_AUTONEG_SUPPORTED   BIT(0)
#define OCTEP_VF_LINK_MODE_AUTONEG_ADVERTISED  BIT(1)
	uint8_t autoneg;

	/* Pause frames setting. */
#define OCTEP_VF_LINK_MODE_PAUSE_SUPPORTED   BIT(0)
#define OCTEP_VF_LINK_MODE_PAUSE_ADVERTISED  BIT(1)
	uint8_t pause;

	/* Admin state of the link (ifconfig <iface> up/down */
	uint8_t  admin_up;

	/* Operational state of the link: physical link is up down */
	uint8_t  oper_up;
};

int otx_ep_mbox_set_mtu(struct rte_eth_dev *eth_dev, uint16_t mtu);
int otx_ep_mbox_set_mac_addr(struct rte_eth_dev *eth_dev,
			     struct rte_ether_addr *mac_addr);
int otx_ep_mbox_get_mac_addr(struct rte_eth_dev *eth_dev,
			     struct rte_ether_addr *mac_addr);
int otx_ep_mbox_get_link_status(struct rte_eth_dev *eth_dev,
				uint8_t *oper_up);
int otx_ep_mbox_get_link_info(struct rte_eth_dev *eth_dev, struct rte_eth_link *link);
void otx_ep_mbox_enable_interrupt(struct otx_ep_device *otx_ep);
void otx_ep_mbox_disable_interrupt(struct otx_ep_device *otx_ep);
int otx_ep_mbox_get_max_pkt_len(struct rte_eth_dev *eth_dev);
int otx_ep_mbox_version_check(struct rte_eth_dev *eth_dev);
int otx_ep_mbox_send_dev_exit(struct rte_eth_dev *eth_dev);
#endif
