/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_HW_H_
#define _RNP_HW_H_

#include "rnp_osdep.h"

#define RNP_MAX_PORT_OF_PF	(4)

struct rnp_hw;
/* Mailbox Operate Info */
enum RNP_MBX_ID {
	RNP_MBX_PF = 0,
	RNP_MBX_VF,
	RNP_MBX_FW = 64,
};

struct rnp_mbx_ops {
	int (*read)(struct rnp_hw *hw,
			u32 *msg,
			u16 size,
			enum RNP_MBX_ID);
	int (*write)(struct rnp_hw *hw,
			u32 *msg,
			u16 size,
			enum RNP_MBX_ID);
	int (*read_posted)(struct rnp_hw *hw,
			u32 *msg,
			u16 size,
			enum RNP_MBX_ID);
	int (*write_posted)(struct rnp_hw *hw,
			u32 *msg,
			u16 size,
			enum RNP_MBX_ID);
	int (*check_for_msg)(struct rnp_hw *hw, enum RNP_MBX_ID);
	int (*check_for_ack)(struct rnp_hw *hw, enum RNP_MBX_ID);
	int (*check_for_rst)(struct rnp_hw *hw, enum RNP_MBX_ID);
};

struct rnp_mbx_sync {
	u16 req;
	u16 ack;
};

struct rnp_mbx_info {
	const struct rnp_mbx_ops *ops;
	u32 usec_delay;         /* retry interval delay time */
	u32 timeout;            /* retry ops timeout limit */
	u16 size;               /* data buffer size*/
	u16 vf_num;             /* Virtual Function num */
	u16 pf_num;             /* Physical Function num */
	u16 sriov_st;           /* Sriov state */
	u16 en_vfs;		/* user enabled vf num */
	bool is_pf;

	struct rnp_mbx_sync syncs[RNP_MBX_FW + 1];
};

struct rnp_eth_port;
/* mac operations */
enum rnp_mpf_modes {
	RNP_MPF_MODE_NONE = 0,
	RNP_MPF_MODE_ALLMULTI, /* Multitle Promisc */
	RNP_MPF_MODE_PROMISC,  /* Unicast Promisc */
};

struct rnp_mac_ops {
	/* get default mac address */
	int (*get_macaddr)(struct rnp_eth_port *port, u8 *mac);
	/* update mac packet filter mode */
	int (*update_mpfm)(struct rnp_eth_port *port, u32 mode, bool en);
};

struct rnp_eth_adapter;
struct rnp_fw_info {
	char cookie_name[RTE_MEMZONE_NAMESIZE];
	struct rnp_dma_mem mem;
	void *cookie_pool;
	bool fw_irq_en;
	bool msg_alloced;

	u64 fw_features;
	spinlock_t fw_lock; /* mc-sp Protect firmware logic */
};

#define rnp_call_hwif_impl(port, f, arg...) \
	(((f) != NULL) ? ((f) (port, arg)) : (-ENODEV))

enum rnp_nic_mode {
	RNP_SINGLE_40G = 0,
	RNP_SINGLE_10G = 1,
	RNP_DUAL_10G = 2,
	RNP_QUAD_10G = 3,
};

/* hw device description */
struct rnp_hw {
	struct rnp_eth_adapter *back;	/* backup to the adapter handle */
	void __iomem *e_ctrl;           /* ethernet control bar */
	void __iomem *c_ctrl;           /* crypto control bar */
	void __iomem *mac_base[RNP_MAX_PORT_OF_PF]; /* mac ctrl register base */
	u32 c_blen;                     /* crypto bar size */

	/* pci device info */
	u16 device_id;
	u16 vendor_id;
	u16 max_vfs;			/* device max support vf */

	char device_name[RTE_DEV_NAME_MAX_LEN];

	u8 max_port_num;	/* max sub port of this nic */
	u8 lane_mask;		/* lane enabled bit */
	u8 nic_mode;
	u16 pf_vf_num;
	/* hardware port sequence info */
	u8 phy_port_ids[RNP_MAX_PORT_OF_PF];	/* port id: for lane0~3: value:0 ~ 7*/
	u8 lane_of_port[RNP_MAX_PORT_OF_PF];	/* lane_id: hw lane map port 1:0 0:1 or 0:0 1:1 */
	bool lane_is_sgmii[RNP_MAX_PORT_OF_PF];
	struct rnp_mbx_info mbx;
	struct rnp_fw_info fw_info;
	u16 min_dma_size;

	spinlock_t rxq_reset_lock; /* reset op isn't thread safe */
	spinlock_t txq_reset_lock; /* reset op isn't thread safe */

};

#endif /* _RNP_HW_H_ */
