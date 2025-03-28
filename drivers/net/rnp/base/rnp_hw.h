/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_HW_H_
#define _RNP_HW_H_

#include "rnp_osdep.h"

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

struct rnp_eth_adapter;

/* hw device description */
struct rnp_hw {
	struct rnp_eth_adapter *back;	/* backup to the adapter handle */
	void __iomem *e_ctrl;           /* ethernet control bar */
	void __iomem *c_ctrl;           /* crypto control bar */
	u32 c_blen;                     /* crypto bar size */

	/* pci device info */
	u16 device_id;
	u16 vendor_id;
	u16 max_vfs;			/* device max support vf */

	u16 pf_vf_num;
	struct rnp_mbx_info mbx;
};

#endif /* _RNP_HW_H_ */
