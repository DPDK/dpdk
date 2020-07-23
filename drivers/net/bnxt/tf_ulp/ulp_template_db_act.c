/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

uint16_t ulp_act_sig_tbl[BNXT_ULP_ACT_SIG_TBL_MAX_SZ] = {
	[BNXT_ULP_ACT_HID_015a] = 1,
	[BNXT_ULP_ACT_HID_00eb] = 2,
	[BNXT_ULP_ACT_HID_0043] = 3,
	[BNXT_ULP_ACT_HID_03d8] = 4,
	[BNXT_ULP_ACT_HID_02c1] = 5,
	[BNXT_ULP_ACT_HID_015e] = 6,
	[BNXT_ULP_ACT_HID_00ef] = 7,
	[BNXT_ULP_ACT_HID_0047] = 8,
	[BNXT_ULP_ACT_HID_03dc] = 9,
	[BNXT_ULP_ACT_HID_02c5] = 10,
	[BNXT_ULP_ACT_HID_025b] = 11,
	[BNXT_ULP_ACT_HID_01ec] = 12,
	[BNXT_ULP_ACT_HID_0144] = 13,
	[BNXT_ULP_ACT_HID_04d9] = 14,
	[BNXT_ULP_ACT_HID_03c2] = 15,
	[BNXT_ULP_ACT_HID_025f] = 16,
	[BNXT_ULP_ACT_HID_01f0] = 17,
	[BNXT_ULP_ACT_HID_0148] = 18,
	[BNXT_ULP_ACT_HID_04dd] = 19,
	[BNXT_ULP_ACT_HID_03c6] = 20,
	[BNXT_ULP_ACT_HID_0000] = 21,
	[BNXT_ULP_ACT_HID_0002] = 22,
	[BNXT_ULP_ACT_HID_0800] = 23,
	[BNXT_ULP_ACT_HID_0101] = 24,
	[BNXT_ULP_ACT_HID_0020] = 25,
	[BNXT_ULP_ACT_HID_0901] = 26,
	[BNXT_ULP_ACT_HID_0121] = 27,
	[BNXT_ULP_ACT_HID_0004] = 28,
	[BNXT_ULP_ACT_HID_0804] = 29,
	[BNXT_ULP_ACT_HID_0105] = 30,
	[BNXT_ULP_ACT_HID_0024] = 31,
	[BNXT_ULP_ACT_HID_0905] = 32,
	[BNXT_ULP_ACT_HID_0125] = 33,
	[BNXT_ULP_ACT_HID_0001] = 34,
	[BNXT_ULP_ACT_HID_0005] = 35,
	[BNXT_ULP_ACT_HID_0009] = 36,
	[BNXT_ULP_ACT_HID_000d] = 37,
	[BNXT_ULP_ACT_HID_0021] = 38,
	[BNXT_ULP_ACT_HID_0029] = 39,
	[BNXT_ULP_ACT_HID_0025] = 40,
	[BNXT_ULP_ACT_HID_002d] = 41,
	[BNXT_ULP_ACT_HID_0801] = 42,
	[BNXT_ULP_ACT_HID_0809] = 43,
	[BNXT_ULP_ACT_HID_0805] = 44,
	[BNXT_ULP_ACT_HID_080d] = 45,
	[BNXT_ULP_ACT_HID_0c15] = 46,
	[BNXT_ULP_ACT_HID_0c19] = 47,
	[BNXT_ULP_ACT_HID_02f6] = 48,
	[BNXT_ULP_ACT_HID_04f8] = 49,
	[BNXT_ULP_ACT_HID_01df] = 50,
	[BNXT_ULP_ACT_HID_07e5] = 51,
	[BNXT_ULP_ACT_HID_06ce] = 52,
	[BNXT_ULP_ACT_HID_02fa] = 53,
	[BNXT_ULP_ACT_HID_04fc] = 54,
	[BNXT_ULP_ACT_HID_01e3] = 55,
	[BNXT_ULP_ACT_HID_07e9] = 56,
	[BNXT_ULP_ACT_HID_06d2] = 57,
	[BNXT_ULP_ACT_HID_03f7] = 58,
	[BNXT_ULP_ACT_HID_05f9] = 59,
	[BNXT_ULP_ACT_HID_02e0] = 60,
	[BNXT_ULP_ACT_HID_08e6] = 61,
	[BNXT_ULP_ACT_HID_07cf] = 62,
	[BNXT_ULP_ACT_HID_03fb] = 63,
	[BNXT_ULP_ACT_HID_05fd] = 64,
	[BNXT_ULP_ACT_HID_02e4] = 65,
	[BNXT_ULP_ACT_HID_08ea] = 66,
	[BNXT_ULP_ACT_HID_07d3] = 67,
	[BNXT_ULP_ACT_HID_040d] = 68,
	[BNXT_ULP_ACT_HID_040f] = 69,
	[BNXT_ULP_ACT_HID_0413] = 70,
	[BNXT_ULP_ACT_HID_0c0d] = 71,
	[BNXT_ULP_ACT_HID_0567] = 72,
	[BNXT_ULP_ACT_HID_0a49] = 73,
	[BNXT_ULP_ACT_HID_050e] = 74,
	[BNXT_ULP_ACT_HID_0d0e] = 75,
	[BNXT_ULP_ACT_HID_0668] = 76,
	[BNXT_ULP_ACT_HID_0b4a] = 77,
	[BNXT_ULP_ACT_HID_0411] = 78,
	[BNXT_ULP_ACT_HID_056b] = 79,
	[BNXT_ULP_ACT_HID_0a4d] = 80,
	[BNXT_ULP_ACT_HID_0c11] = 81,
	[BNXT_ULP_ACT_HID_0512] = 82,
	[BNXT_ULP_ACT_HID_0d12] = 83,
	[BNXT_ULP_ACT_HID_066c] = 84,
	[BNXT_ULP_ACT_HID_0b4e] = 85
};

struct bnxt_ulp_act_match_info ulp_act_match_list[] = {
	[1] = {
	.act_hid = BNXT_ULP_ACT_HID_015a,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[2] = {
	.act_hid = BNXT_ULP_ACT_HID_00eb,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[3] = {
	.act_hid = BNXT_ULP_ACT_HID_0043,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[4] = {
	.act_hid = BNXT_ULP_ACT_HID_03d8,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[5] = {
	.act_hid = BNXT_ULP_ACT_HID_02c1,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[6] = {
	.act_hid = BNXT_ULP_ACT_HID_015e,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_00ef,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[8] = {
	.act_hid = BNXT_ULP_ACT_HID_0047,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[9] = {
	.act_hid = BNXT_ULP_ACT_HID_03dc,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[10] = {
	.act_hid = BNXT_ULP_ACT_HID_02c5,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[11] = {
	.act_hid = BNXT_ULP_ACT_HID_025b,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[12] = {
	.act_hid = BNXT_ULP_ACT_HID_01ec,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[13] = {
	.act_hid = BNXT_ULP_ACT_HID_0144,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_04d9,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[15] = {
	.act_hid = BNXT_ULP_ACT_HID_03c2,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[16] = {
	.act_hid = BNXT_ULP_ACT_HID_025f,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[17] = {
	.act_hid = BNXT_ULP_ACT_HID_01f0,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[18] = {
	.act_hid = BNXT_ULP_ACT_HID_0148,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[19] = {
	.act_hid = BNXT_ULP_ACT_HID_04dd,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[20] = {
	.act_hid = BNXT_ULP_ACT_HID_03c6,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[21] = {
	.act_hid = BNXT_ULP_ACT_HID_0000,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[22] = {
	.act_hid = BNXT_ULP_ACT_HID_0002,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[23] = {
	.act_hid = BNXT_ULP_ACT_HID_0800,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[24] = {
	.act_hid = BNXT_ULP_ACT_HID_0101,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[25] = {
	.act_hid = BNXT_ULP_ACT_HID_0020,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[26] = {
	.act_hid = BNXT_ULP_ACT_HID_0901,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[27] = {
	.act_hid = BNXT_ULP_ACT_HID_0121,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[28] = {
	.act_hid = BNXT_ULP_ACT_HID_0004,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[29] = {
	.act_hid = BNXT_ULP_ACT_HID_0804,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[30] = {
	.act_hid = BNXT_ULP_ACT_HID_0105,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[31] = {
	.act_hid = BNXT_ULP_ACT_HID_0024,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[32] = {
	.act_hid = BNXT_ULP_ACT_HID_0905,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[33] = {
	.act_hid = BNXT_ULP_ACT_HID_0125,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[34] = {
	.act_hid = BNXT_ULP_ACT_HID_0001,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[35] = {
	.act_hid = BNXT_ULP_ACT_HID_0005,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[36] = {
	.act_hid = BNXT_ULP_ACT_HID_0009,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[37] = {
	.act_hid = BNXT_ULP_ACT_HID_000d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[38] = {
	.act_hid = BNXT_ULP_ACT_HID_0021,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[39] = {
	.act_hid = BNXT_ULP_ACT_HID_0029,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[40] = {
	.act_hid = BNXT_ULP_ACT_HID_0025,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[41] = {
	.act_hid = BNXT_ULP_ACT_HID_002d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[42] = {
	.act_hid = BNXT_ULP_ACT_HID_0801,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[43] = {
	.act_hid = BNXT_ULP_ACT_HID_0809,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[44] = {
	.act_hid = BNXT_ULP_ACT_HID_0805,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[45] = {
	.act_hid = BNXT_ULP_ACT_HID_080d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[46] = {
	.act_hid = BNXT_ULP_ACT_HID_0c15,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[47] = {
	.act_hid = BNXT_ULP_ACT_HID_0c19,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 4
	},
	[48] = {
	.act_hid = BNXT_ULP_ACT_HID_02f6,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[49] = {
	.act_hid = BNXT_ULP_ACT_HID_04f8,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[50] = {
	.act_hid = BNXT_ULP_ACT_HID_01df,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[51] = {
	.act_hid = BNXT_ULP_ACT_HID_07e5,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[52] = {
	.act_hid = BNXT_ULP_ACT_HID_06ce,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[53] = {
	.act_hid = BNXT_ULP_ACT_HID_02fa,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[54] = {
	.act_hid = BNXT_ULP_ACT_HID_04fc,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[55] = {
	.act_hid = BNXT_ULP_ACT_HID_01e3,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[56] = {
	.act_hid = BNXT_ULP_ACT_HID_07e9,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[57] = {
	.act_hid = BNXT_ULP_ACT_HID_06d2,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[58] = {
	.act_hid = BNXT_ULP_ACT_HID_03f7,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[59] = {
	.act_hid = BNXT_ULP_ACT_HID_05f9,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[60] = {
	.act_hid = BNXT_ULP_ACT_HID_02e0,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[61] = {
	.act_hid = BNXT_ULP_ACT_HID_08e6,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[62] = {
	.act_hid = BNXT_ULP_ACT_HID_07cf,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[63] = {
	.act_hid = BNXT_ULP_ACT_HID_03fb,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[64] = {
	.act_hid = BNXT_ULP_ACT_HID_05fd,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[65] = {
	.act_hid = BNXT_ULP_ACT_HID_02e4,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[66] = {
	.act_hid = BNXT_ULP_ACT_HID_08ea,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[67] = {
	.act_hid = BNXT_ULP_ACT_HID_07d3,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACTION_BIT_SET_IPV4_DST |
		BNXT_ULP_ACTION_BIT_SET_TP_SRC |
		BNXT_ULP_ACTION_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 5
	},
	[68] = {
	.act_hid = BNXT_ULP_ACT_HID_040d,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[69] = {
	.act_hid = BNXT_ULP_ACT_HID_040f,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[70] = {
	.act_hid = BNXT_ULP_ACT_HID_0413,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[71] = {
	.act_hid = BNXT_ULP_ACT_HID_0c0d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[72] = {
	.act_hid = BNXT_ULP_ACT_HID_0567,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[73] = {
	.act_hid = BNXT_ULP_ACT_HID_0a49,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[74] = {
	.act_hid = BNXT_ULP_ACT_HID_050e,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[75] = {
	.act_hid = BNXT_ULP_ACT_HID_0d0e,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[76] = {
	.act_hid = BNXT_ULP_ACT_HID_0668,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[77] = {
	.act_hid = BNXT_ULP_ACT_HID_0b4a,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[78] = {
	.act_hid = BNXT_ULP_ACT_HID_0411,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[79] = {
	.act_hid = BNXT_ULP_ACT_HID_056b,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[80] = {
	.act_hid = BNXT_ULP_ACT_HID_0a4d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[81] = {
	.act_hid = BNXT_ULP_ACT_HID_0c11,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[82] = {
	.act_hid = BNXT_ULP_ACT_HID_0512,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[83] = {
	.act_hid = BNXT_ULP_ACT_HID_0d12,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[84] = {
	.act_hid = BNXT_ULP_ACT_HID_066c,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[85] = {
	.act_hid = BNXT_ULP_ACT_HID_0b4e,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_ACTION_BIT_SET_VLAN_VID |
		BNXT_ULP_ACTION_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	}
};

struct bnxt_ulp_mapper_tbl_list_info ulp_act_tmpl_list[] = {
	[((1 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 0,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((2 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 2,
	.start_tbl_idx = 5,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((3 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 2,
	.start_tbl_idx = 7,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((4 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 9,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((5 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 14,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((6 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 3,
	.start_tbl_idx = 19,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_act_tbl_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 1,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 2,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 3,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 15,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 41,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 42,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 68,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 69,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 95,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 96,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 99,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 102,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 114,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 140,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 141,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 142,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 143,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 155,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 181,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 182,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 194,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_act_result_field_list[] = {
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_DECAP_FUNC_THRU_L2,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_DROP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_TUN_TYPE_VXLAN,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L4_TYPE_UDP_CSUM,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_DECAP_FUNC_THRU_L2,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_DROP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	}
};
