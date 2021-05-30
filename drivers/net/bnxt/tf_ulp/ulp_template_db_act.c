/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Tue Dec  8 14:57:13 2020 */

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
	[BNXT_ULP_ACT_HID_0325] = 4,
	[BNXT_ULP_ACT_HID_0010] = 5,
	[BNXT_ULP_ACT_HID_0725] = 6,
	[BNXT_ULP_ACT_HID_0335] = 7,
	[BNXT_ULP_ACT_HID_0002] = 8,
	[BNXT_ULP_ACT_HID_0003] = 9,
	[BNXT_ULP_ACT_HID_0402] = 10,
	[BNXT_ULP_ACT_HID_0327] = 11,
	[BNXT_ULP_ACT_HID_0012] = 12,
	[BNXT_ULP_ACT_HID_0727] = 13,
	[BNXT_ULP_ACT_HID_0337] = 14,
	[BNXT_ULP_ACT_HID_01de] = 15,
	[BNXT_ULP_ACT_HID_00c6] = 16,
	[BNXT_ULP_ACT_HID_0506] = 17,
	[BNXT_ULP_ACT_HID_01ed] = 18,
	[BNXT_ULP_ACT_HID_03ef] = 19,
	[BNXT_ULP_ACT_HID_0516] = 20,
	[BNXT_ULP_ACT_HID_01df] = 21,
	[BNXT_ULP_ACT_HID_01e4] = 22,
	[BNXT_ULP_ACT_HID_00cc] = 23,
	[BNXT_ULP_ACT_HID_0504] = 24,
	[BNXT_ULP_ACT_HID_01ef] = 25,
	[BNXT_ULP_ACT_HID_03ed] = 26,
	[BNXT_ULP_ACT_HID_0514] = 27,
	[BNXT_ULP_ACT_HID_00db] = 28,
	[BNXT_ULP_ACT_HID_00df] = 29
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
	.act_hid = BNXT_ULP_ACT_HID_0325,
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
	.act_hid = BNXT_ULP_ACT_HID_0725,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_0335,
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
	.act_hid = BNXT_ULP_ACT_HID_0327,
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
	.act_hid = BNXT_ULP_ACT_HID_0727,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_0337,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[15] = {
	.act_hid = BNXT_ULP_ACT_HID_01de,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[16] = {
	.act_hid = BNXT_ULP_ACT_HID_00c6,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[17] = {
	.act_hid = BNXT_ULP_ACT_HID_0506,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[18] = {
	.act_hid = BNXT_ULP_ACT_HID_01ed,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[19] = {
	.act_hid = BNXT_ULP_ACT_HID_03ef,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[20] = {
	.act_hid = BNXT_ULP_ACT_HID_0516,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[21] = {
	.act_hid = BNXT_ULP_ACT_HID_01df,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[22] = {
	.act_hid = BNXT_ULP_ACT_HID_01e4,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[23] = {
	.act_hid = BNXT_ULP_ACT_HID_00cc,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[24] = {
	.act_hid = BNXT_ULP_ACT_HID_0504,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[25] = {
	.act_hid = BNXT_ULP_ACT_HID_01ef,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[26] = {
	.act_hid = BNXT_ULP_ACT_HID_03ed,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[27] = {
	.act_hid = BNXT_ULP_ACT_HID_0514,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[28] = {
	.act_hid = BNXT_ULP_ACT_HID_00db,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[29] = {
	.act_hid = BNXT_ULP_ACT_HID_00df,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	}
};
