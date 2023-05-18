/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include "ice_common.h"

#define ICE_XLT_KB_TBL_OFF 12
#define ICE_XLT_KB_TBL_ENTRY_SIZE 24

static void _xlt_kb_entry_dump(struct ice_hw *hw,
			       struct ice_xlt_kb_entry *entry, int idx)
{
	int i;

	ice_info(hw, "key builder entry %d\n", idx);
	ice_info(hw, "\txlt1_ad_sel = %d\n", entry->xlt1_ad_sel);
	ice_info(hw, "\txlt2_ad_sel = %d\n", entry->xlt2_ad_sel);

	for (i = 0; i < ICE_XLT_KB_FLAG0_14_CNT; i++)
		ice_info(hw, "\tflg%d_sel = %d\n", i, entry->flg0_14_sel[i]);

	ice_info(hw, "\txlt1_md_sel = %d\n", entry->xlt1_md_sel);
	ice_info(hw, "\txlt2_md_sel = %d\n", entry->xlt2_md_sel);
}

/**
 * ice_imem_dump - dump a xlt key build info
 * @hw: pointer to the hardware structure
 * @kb: key build to dump
 */
void ice_xlt_kb_dump(struct ice_hw *hw, struct ice_xlt_kb *kb)
{
	int i;

	ice_info(hw, "xlt1_pm = %d\n", kb->xlt1_pm);
	ice_info(hw, "xlt2_pm = %d\n", kb->xlt2_pm);
	ice_info(hw, "prof_id_pm = %d\n", kb->prof_id_pm);
	ice_info(hw, "flag15 low  = 0x%08x\n", (u32)kb->flag15);
	ice_info(hw, "flag15 high = 0x%08x\n", (u32)(kb->flag15 >> 32));

	for (i = 0; i < ICE_XLT_KB_TBL_CNT; i++)
		_xlt_kb_entry_dump(hw, &kb->entries[i], i);
}

/** The function parses a 192 bits XLT Key Build entry with below format:
 *  BIT 0-31:	reserved
 *  BIT 32-34:	XLT1 AdSel (entry->xlt1_ad_sel)
 *  BIT 35-37:	XLT2 AdSel (entry->xlt2_ad_sel)
 *  BIT 38-46:	Flag 0 Select (entry->flg0_14_sel[0])
 *  BIT 47-55:	Flag 1 Select (entry->flg0_14_sel[1])
 *  BIT 56-64:	Flag 2 Select (entry->flg0_14_sel[2])
 *  BIT 65-73:	Flag 3 Select (entry->flg0_14_sel[3])
 *  BIT 74-82:	Flag 4 Select (entry->flg0_14_sel[4])
 *  BIT 83-91:	Flag 5 Select (entry->flg0_14_sel[5])
 *  BIT 92-100:	Flag 6 Select (entry->flg0_14_sel[6])
 *  BIT 101-109:Flag 7 Select (entry->flg0_14_sel[7])
 *  BIT 110-118:Flag 8 Select (entry->flg0_14_sel[8])
 *  BIT 119-127:Flag 9 Select (entry->flg0_14_sel[9])
 *  BIT 128-136:Flag 10 Select (entry->flg0_14_sel[10])
 *  BIT 137-145:Flag 11 Select (entry->flg0_14_sel[11])
 *  BIT 146-154:Flag 12 Select (entry->flg0_14_sel[12])
 *  BIT 155-163:Flag 13 Select (entry->flg0_14_sel[13])
 *  BIT 164-172:Flag 14 Select (entry->flg0_14_sel[14])
 *  BIT 173-181:reserved
 *  BIT 182-186:XLT1 MdSel (entry->xlt1_md_sel)
 *  BIT 187-191:XLT2 MdSel (entry->xlt2_md_sel)
 */
static void _kb_entry_init(struct ice_xlt_kb_entry *entry, u8 *data)
{
	u64 d64 = *(u64 *)&data[4];

	entry->xlt1_ad_sel = (u8)(d64 & 0x7);
	entry->xlt2_ad_sel = (u8)((d64 >> 3) & 0x7);
	entry->flg0_14_sel[0] = (u16)((d64 >> 6) & 0x1ff);
	entry->flg0_14_sel[1] = (u16)((d64 >> 15) & 0x1ff);
	entry->flg0_14_sel[2] = (u16)((d64 >> 24) & 0x1ff);
	entry->flg0_14_sel[3] = (u16)((d64 >> 33) & 0x1ff);
	entry->flg0_14_sel[4] = (u16)((d64 >> 42) & 0x1ff);
	entry->flg0_14_sel[5] = (u16)((d64 >> 51) & 0x1ff);

	d64 = (*(u64 *)&data[11] >> 4);
	entry->flg0_14_sel[6] = (u16)(d64 & 0x1ff);
	entry->flg0_14_sel[7] = (u16)((d64 >> 9) & 0x1ff);
	entry->flg0_14_sel[8] = (u16)((d64 >> 18) & 0x1ff);
	entry->flg0_14_sel[9] = (u16)((d64 >> 27) & 0x1ff);
	entry->flg0_14_sel[10] = (u16)((d64 >> 36) & 0x1ff);
	entry->flg0_14_sel[11] = (u16)((d64 >> 45) & 0x1ff);

	d64 = (*(u64 *)&data[18] >> 2);
	entry->flg0_14_sel[12] = (u16)(d64 & 0x1ff);
	entry->flg0_14_sel[13] = (u16)((d64 >> 9) & 0x1ff);
	entry->flg0_14_sel[14] = (u16)((d64 >> 18) & 0x1ff);

	entry->xlt1_md_sel = (u8)((d64 >> 36) & 0x1f);
	entry->xlt2_md_sel = (u8)((d64 >> 41) & 0x1f);
}

/** The function parses a 204 bytes XLT Key Build Table with below format:
 *  byte 0:	XLT1 Partition Mode (kb->xlt1_pm)
 *  byte 1:	XLT2 Partition Mode (kb->xlt2_pm)
 *  byte 2:	Profile ID Partition Mode (kb->prof_id_pm)
 *  byte 3:	reserved
 *  byte 4-11:	Flag15 Mask (kb->flag15)
 *  byte 12-203:8 Key Build entries (kb->entries)
 */
static void _parse_kb_data(struct ice_hw *hw, struct ice_xlt_kb *kb, void *data)
{
	u8 *buf = (u8 *)data;
	int i;

	kb->xlt1_pm = buf[0];
	kb->xlt2_pm = buf[1];
	kb->prof_id_pm = buf[2];

	kb->flag15 = *(u64 *)&buf[4];
	for (i = 0; i < ICE_XLT_KB_TBL_CNT; i++)
		_kb_entry_init(&kb->entries[i],
			       &buf[ICE_XLT_KB_TBL_OFF +
				    i * ICE_XLT_KB_TBL_ENTRY_SIZE]);

	if (hw->debug_mask & ICE_DBG_PARSER)
		ice_xlt_kb_dump(hw, kb);
}

static struct ice_xlt_kb *_xlt_kb_get(struct ice_hw *hw, u32 sect_type)
{
	struct ice_seg *seg = hw->seg;
	struct ice_pkg_enum state;
	struct ice_xlt_kb *kb;
	void *data;

	if (!seg)
		return NULL;

	kb = (struct ice_xlt_kb *)ice_malloc(hw, sizeof(*kb));
	if (!kb) {
		ice_debug(hw, ICE_DBG_PARSER, "failed to allocate memory for xlt key builder type %d.\n",
			  sect_type);
		return NULL;
	}

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);
	data = ice_pkg_enum_section(seg, &state, sect_type);
	if (!data) {
		ice_debug(hw, ICE_DBG_PARSER, "failed to find section type %d.\n",
			  sect_type);
		return NULL;
	}

	_parse_kb_data(hw, kb, data);

	return kb;
}

/**
 * ice_xlt_kb_get_sw - create switch xlt key build
 * @hw: pointer to the hardware structure
 */
struct ice_xlt_kb *ice_xlt_kb_get_sw(struct ice_hw *hw)
{
	return _xlt_kb_get(hw, ICE_SID_XLT_KEY_BUILDER_SW);
}

/**
 * ice_xlt_kb_get_acl - create acl xlt key build
 * @hw: pointer to the hardware structure
 */
struct ice_xlt_kb *ice_xlt_kb_get_acl(struct ice_hw *hw)
{
	return _xlt_kb_get(hw, ICE_SID_XLT_KEY_BUILDER_ACL);
}

/**
 * ice_xlt_kb_get_fd - create fdir xlt key build
 * @hw: pointer to the hardware structure
 */
struct ice_xlt_kb *ice_xlt_kb_get_fd(struct ice_hw *hw)
{
	return _xlt_kb_get(hw, ICE_SID_XLT_KEY_BUILDER_FD);
}

/**
 * ice_xlt_kb_get_fd - create rss xlt key build
 * @hw: pointer to the hardware structure
 */
struct ice_xlt_kb *ice_xlt_kb_get_rss(struct ice_hw *hw)
{
	return _xlt_kb_get(hw, ICE_SID_XLT_KEY_BUILDER_RSS);
}

/**
 * ice_xlt_kb_flag_get - aggregate 64 bits packet flag into 16 bits xlt flag
 * @kb: xlt key build
 * @pkt_flag: 64 bits packet flag
 */
u16 ice_xlt_kb_flag_get(struct ice_xlt_kb *kb, u64 pkt_flag)
{
	struct ice_xlt_kb_entry *entry = &kb->entries[0];
	u16 flg = 0;
	int i;

	/* check flag 15 */
	if (kb->flag15 & pkt_flag)
		flg = (u16)(1u << 15);

	/* check flag 0 - 14 */
	for (i = 0; i < 15; i++) {
		/* only check first entry */
		u16 idx = (u16)(entry->flg0_14_sel[i] & 0x3f);

		if (pkt_flag & (1ul << idx))
			flg |=  (u16)(1u << i);
	}

	return flg;
}
