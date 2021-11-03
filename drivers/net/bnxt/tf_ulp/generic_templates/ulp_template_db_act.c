/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Wed Aug 25 14:37:06 2021 */

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
	[BNXT_ULP_ACT_HID_01ab] = 4,
	[BNXT_ULP_ACT_HID_0010] = 5,
	[BNXT_ULP_ACT_HID_05ab] = 6,
	[BNXT_ULP_ACT_HID_01bb] = 7,
	[BNXT_ULP_ACT_HID_0002] = 8,
	[BNXT_ULP_ACT_HID_0003] = 9,
	[BNXT_ULP_ACT_HID_0402] = 10,
	[BNXT_ULP_ACT_HID_01ad] = 11,
	[BNXT_ULP_ACT_HID_0012] = 12,
	[BNXT_ULP_ACT_HID_05ad] = 13,
	[BNXT_ULP_ACT_HID_01bd] = 14,
	[BNXT_ULP_ACT_HID_0613] = 15,
	[BNXT_ULP_ACT_HID_02a9] = 16,
	[BNXT_ULP_ACT_HID_0054] = 17,
	[BNXT_ULP_ACT_HID_0622] = 18,
	[BNXT_ULP_ACT_HID_0454] = 19,
	[BNXT_ULP_ACT_HID_0064] = 20,
	[BNXT_ULP_ACT_HID_0614] = 21,
	[BNXT_ULP_ACT_HID_0615] = 22,
	[BNXT_ULP_ACT_HID_02ab] = 23,
	[BNXT_ULP_ACT_HID_0056] = 24,
	[BNXT_ULP_ACT_HID_0624] = 25,
	[BNXT_ULP_ACT_HID_0456] = 26,
	[BNXT_ULP_ACT_HID_0066] = 27,
	[BNXT_ULP_ACT_HID_048d] = 28,
	[BNXT_ULP_ACT_HID_048f] = 29,
	[BNXT_ULP_ACT_HID_04bc] = 30,
	[BNXT_ULP_ACT_HID_00a9] = 31,
	[BNXT_ULP_ACT_HID_020f] = 32,
	[BNXT_ULP_ACT_HID_0153] = 33,
	[BNXT_ULP_ACT_HID_04a9] = 34,
	[BNXT_ULP_ACT_HID_01fc] = 35,
	[BNXT_ULP_ACT_HID_04be] = 36,
	[BNXT_ULP_ACT_HID_00ab] = 37,
	[BNXT_ULP_ACT_HID_0211] = 38,
	[BNXT_ULP_ACT_HID_0155] = 39,
	[BNXT_ULP_ACT_HID_04ab] = 40,
	[BNXT_ULP_ACT_HID_01fe] = 41,
	[BNXT_ULP_ACT_HID_0667] = 42,
	[BNXT_ULP_ACT_HID_0254] = 43,
	[BNXT_ULP_ACT_HID_03ba] = 44,
	[BNXT_ULP_ACT_HID_02fe] = 45,
	[BNXT_ULP_ACT_HID_0654] = 46,
	[BNXT_ULP_ACT_HID_03a7] = 47,
	[BNXT_ULP_ACT_HID_0669] = 48,
	[BNXT_ULP_ACT_HID_0256] = 49,
	[BNXT_ULP_ACT_HID_03bc] = 50,
	[BNXT_ULP_ACT_HID_0300] = 51,
	[BNXT_ULP_ACT_HID_0656] = 52,
	[BNXT_ULP_ACT_HID_03a9] = 53,
	[BNXT_ULP_ACT_HID_021b] = 54,
	[BNXT_ULP_ACT_HID_021c] = 55,
	[BNXT_ULP_ACT_HID_021e] = 56,
	[BNXT_ULP_ACT_HID_063f] = 57,
	[BNXT_ULP_ACT_HID_0510] = 58,
	[BNXT_ULP_ACT_HID_03c6] = 59,
	[BNXT_ULP_ACT_HID_0082] = 60,
	[BNXT_ULP_ACT_HID_06bb] = 61,
	[BNXT_ULP_ACT_HID_021d] = 62,
	[BNXT_ULP_ACT_HID_0641] = 63,
	[BNXT_ULP_ACT_HID_0512] = 64,
	[BNXT_ULP_ACT_HID_03c8] = 65,
	[BNXT_ULP_ACT_HID_0084] = 66,
	[BNXT_ULP_ACT_HID_06bd] = 67,
	[BNXT_ULP_ACT_HID_06d7] = 68,
	[BNXT_ULP_ACT_HID_02c4] = 69,
	[BNXT_ULP_ACT_HID_042a] = 70,
	[BNXT_ULP_ACT_HID_036e] = 71,
	[BNXT_ULP_ACT_HID_06c4] = 72,
	[BNXT_ULP_ACT_HID_0417] = 73,
	[BNXT_ULP_ACT_HID_06d9] = 74,
	[BNXT_ULP_ACT_HID_02c6] = 75,
	[BNXT_ULP_ACT_HID_042c] = 76,
	[BNXT_ULP_ACT_HID_0370] = 77,
	[BNXT_ULP_ACT_HID_06c6] = 78,
	[BNXT_ULP_ACT_HID_0419] = 79,
	[BNXT_ULP_ACT_HID_0119] = 80,
	[BNXT_ULP_ACT_HID_046f] = 81,
	[BNXT_ULP_ACT_HID_05d5] = 82,
	[BNXT_ULP_ACT_HID_0519] = 83,
	[BNXT_ULP_ACT_HID_0106] = 84,
	[BNXT_ULP_ACT_HID_05c2] = 85,
	[BNXT_ULP_ACT_HID_011b] = 86,
	[BNXT_ULP_ACT_HID_0471] = 87,
	[BNXT_ULP_ACT_HID_05d7] = 88,
	[BNXT_ULP_ACT_HID_051b] = 89,
	[BNXT_ULP_ACT_HID_0108] = 90,
	[BNXT_ULP_ACT_HID_05c4] = 91,
	[BNXT_ULP_ACT_HID_00a2] = 92,
	[BNXT_ULP_ACT_HID_00a4] = 93
};

/* Array for the act matcher list */
struct bnxt_ulp_act_match_info ulp_act_match_list[] = {
	[1] = {
	.act_hid = BNXT_ULP_ACT_HID_0000,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[2] = {
	.act_hid = BNXT_ULP_ACT_HID_0001,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[3] = {
	.act_hid = BNXT_ULP_ACT_HID_0400,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[4] = {
	.act_hid = BNXT_ULP_ACT_HID_01ab,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[5] = {
	.act_hid = BNXT_ULP_ACT_HID_0010,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[6] = {
	.act_hid = BNXT_ULP_ACT_HID_05ab,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_01bb,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[8] = {
	.act_hid = BNXT_ULP_ACT_HID_0002,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[9] = {
	.act_hid = BNXT_ULP_ACT_HID_0003,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[10] = {
	.act_hid = BNXT_ULP_ACT_HID_0402,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[11] = {
	.act_hid = BNXT_ULP_ACT_HID_01ad,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[12] = {
	.act_hid = BNXT_ULP_ACT_HID_0012,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[13] = {
	.act_hid = BNXT_ULP_ACT_HID_05ad,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_01bd,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[15] = {
	.act_hid = BNXT_ULP_ACT_HID_0613,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[16] = {
	.act_hid = BNXT_ULP_ACT_HID_02a9,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[17] = {
	.act_hid = BNXT_ULP_ACT_HID_0054,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[18] = {
	.act_hid = BNXT_ULP_ACT_HID_0622,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[19] = {
	.act_hid = BNXT_ULP_ACT_HID_0454,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[20] = {
	.act_hid = BNXT_ULP_ACT_HID_0064,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[21] = {
	.act_hid = BNXT_ULP_ACT_HID_0614,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[22] = {
	.act_hid = BNXT_ULP_ACT_HID_0615,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[23] = {
	.act_hid = BNXT_ULP_ACT_HID_02ab,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[24] = {
	.act_hid = BNXT_ULP_ACT_HID_0056,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[25] = {
	.act_hid = BNXT_ULP_ACT_HID_0624,
	.act_pattern_id = 24,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[26] = {
	.act_hid = BNXT_ULP_ACT_HID_0456,
	.act_pattern_id = 25,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[27] = {
	.act_hid = BNXT_ULP_ACT_HID_0066,
	.act_pattern_id = 26,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[28] = {
	.act_hid = BNXT_ULP_ACT_HID_048d,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[29] = {
	.act_hid = BNXT_ULP_ACT_HID_048f,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[30] = {
	.act_hid = BNXT_ULP_ACT_HID_04bc,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[31] = {
	.act_hid = BNXT_ULP_ACT_HID_00a9,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[32] = {
	.act_hid = BNXT_ULP_ACT_HID_020f,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[33] = {
	.act_hid = BNXT_ULP_ACT_HID_0153,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[34] = {
	.act_hid = BNXT_ULP_ACT_HID_04a9,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[35] = {
	.act_hid = BNXT_ULP_ACT_HID_01fc,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[36] = {
	.act_hid = BNXT_ULP_ACT_HID_04be,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[37] = {
	.act_hid = BNXT_ULP_ACT_HID_00ab,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[38] = {
	.act_hid = BNXT_ULP_ACT_HID_0211,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[39] = {
	.act_hid = BNXT_ULP_ACT_HID_0155,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[40] = {
	.act_hid = BNXT_ULP_ACT_HID_04ab,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[41] = {
	.act_hid = BNXT_ULP_ACT_HID_01fe,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[42] = {
	.act_hid = BNXT_ULP_ACT_HID_0667,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[43] = {
	.act_hid = BNXT_ULP_ACT_HID_0254,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[44] = {
	.act_hid = BNXT_ULP_ACT_HID_03ba,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[45] = {
	.act_hid = BNXT_ULP_ACT_HID_02fe,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[46] = {
	.act_hid = BNXT_ULP_ACT_HID_0654,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[47] = {
	.act_hid = BNXT_ULP_ACT_HID_03a7,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[48] = {
	.act_hid = BNXT_ULP_ACT_HID_0669,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[49] = {
	.act_hid = BNXT_ULP_ACT_HID_0256,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[50] = {
	.act_hid = BNXT_ULP_ACT_HID_03bc,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[51] = {
	.act_hid = BNXT_ULP_ACT_HID_0300,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[52] = {
	.act_hid = BNXT_ULP_ACT_HID_0656,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[53] = {
	.act_hid = BNXT_ULP_ACT_HID_03a9,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[54] = {
	.act_hid = BNXT_ULP_ACT_HID_021b,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[55] = {
	.act_hid = BNXT_ULP_ACT_HID_021c,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[56] = {
	.act_hid = BNXT_ULP_ACT_HID_021e,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[57] = {
	.act_hid = BNXT_ULP_ACT_HID_063f,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[58] = {
	.act_hid = BNXT_ULP_ACT_HID_0510,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[59] = {
	.act_hid = BNXT_ULP_ACT_HID_03c6,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[60] = {
	.act_hid = BNXT_ULP_ACT_HID_0082,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[61] = {
	.act_hid = BNXT_ULP_ACT_HID_06bb,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[62] = {
	.act_hid = BNXT_ULP_ACT_HID_021d,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[63] = {
	.act_hid = BNXT_ULP_ACT_HID_0641,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[64] = {
	.act_hid = BNXT_ULP_ACT_HID_0512,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[65] = {
	.act_hid = BNXT_ULP_ACT_HID_03c8,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[66] = {
	.act_hid = BNXT_ULP_ACT_HID_0084,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[67] = {
	.act_hid = BNXT_ULP_ACT_HID_06bd,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[68] = {
	.act_hid = BNXT_ULP_ACT_HID_06d7,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[69] = {
	.act_hid = BNXT_ULP_ACT_HID_02c4,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[70] = {
	.act_hid = BNXT_ULP_ACT_HID_042a,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[71] = {
	.act_hid = BNXT_ULP_ACT_HID_036e,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[72] = {
	.act_hid = BNXT_ULP_ACT_HID_06c4,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[73] = {
	.act_hid = BNXT_ULP_ACT_HID_0417,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[74] = {
	.act_hid = BNXT_ULP_ACT_HID_06d9,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[75] = {
	.act_hid = BNXT_ULP_ACT_HID_02c6,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[76] = {
	.act_hid = BNXT_ULP_ACT_HID_042c,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[77] = {
	.act_hid = BNXT_ULP_ACT_HID_0370,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[78] = {
	.act_hid = BNXT_ULP_ACT_HID_06c6,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[79] = {
	.act_hid = BNXT_ULP_ACT_HID_0419,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[80] = {
	.act_hid = BNXT_ULP_ACT_HID_0119,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[81] = {
	.act_hid = BNXT_ULP_ACT_HID_046f,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[82] = {
	.act_hid = BNXT_ULP_ACT_HID_05d5,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[83] = {
	.act_hid = BNXT_ULP_ACT_HID_0519,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[84] = {
	.act_hid = BNXT_ULP_ACT_HID_0106,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[85] = {
	.act_hid = BNXT_ULP_ACT_HID_05c2,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[86] = {
	.act_hid = BNXT_ULP_ACT_HID_011b,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[87] = {
	.act_hid = BNXT_ULP_ACT_HID_0471,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[88] = {
	.act_hid = BNXT_ULP_ACT_HID_05d7,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[89] = {
	.act_hid = BNXT_ULP_ACT_HID_051b,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[90] = {
	.act_hid = BNXT_ULP_ACT_HID_0108,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[91] = {
	.act_hid = BNXT_ULP_ACT_HID_05c4,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[92] = {
	.act_hid = BNXT_ULP_ACT_HID_00a2,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[93] = {
	.act_hid = BNXT_ULP_ACT_HID_00a4,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	}
};
