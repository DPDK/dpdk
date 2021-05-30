/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Tue Dec  1 11:40:24 2020 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/*
 * Action signature table:
 * maps hash id to ulp_act_match_list[] index
 */
uint16_t ulp_act_sig_tbl[BNXT_ULP_ACT_SIG_TBL_MAX_SZ] = {
	[BNXT_ULP_ACT_HID_0000] = 1,
	[BNXT_ULP_ACT_HID_0001] = 2,
	[BNXT_ULP_ACT_HID_0400] = 3,
	[BNXT_ULP_ACT_HID_0331] = 4,
	[BNXT_ULP_ACT_HID_0010] = 5,
	[BNXT_ULP_ACT_HID_0731] = 6,
	[BNXT_ULP_ACT_HID_0341] = 7,
	[BNXT_ULP_ACT_HID_0002] = 8,
	[BNXT_ULP_ACT_HID_0003] = 9,
	[BNXT_ULP_ACT_HID_0402] = 10,
	[BNXT_ULP_ACT_HID_0333] = 11,
	[BNXT_ULP_ACT_HID_0012] = 12,
	[BNXT_ULP_ACT_HID_0733] = 13,
	[BNXT_ULP_ACT_HID_0343] = 14
};

/* Array for the act matcher list */
struct bnxt_ulp_act_match_info ulp_act_match_list[] = {
	[1] = {
	.act_hid = BNXT_ULP_ACT_HID_0000,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[2] = {
	.act_hid = BNXT_ULP_ACT_HID_0001,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[3] = {
	.act_hid = BNXT_ULP_ACT_HID_0400,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[4] = {
	.act_hid = BNXT_ULP_ACT_HID_0331,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[5] = {
	.act_hid = BNXT_ULP_ACT_HID_0010,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[6] = {
	.act_hid = BNXT_ULP_ACT_HID_0731,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_0341,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[8] = {
	.act_hid = BNXT_ULP_ACT_HID_0002,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[9] = {
	.act_hid = BNXT_ULP_ACT_HID_0003,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[10] = {
	.act_hid = BNXT_ULP_ACT_HID_0402,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[11] = {
	.act_hid = BNXT_ULP_ACT_HID_0333,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[12] = {
	.act_hid = BNXT_ULP_ACT_HID_0012,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[13] = {
	.act_hid = BNXT_ULP_ACT_HID_0733,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_0343,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	}
};
