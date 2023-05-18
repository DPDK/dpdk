/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_SEC_DATA_OFFSET				4
#define ICE_SID_RXPARSER_IMEM_ENTRY_SIZE		48
#define ICE_SID_RXPARSER_METADATA_INIT_ENTRY_SIZE	24
#define ICE_SID_RXPARSER_CAM_ENTRY_SIZE			16
#define ICE_SID_RXPARSER_PG_SPILL_ENTRY_SIZE		17
#define ICE_SID_RXPARSER_NOMATCH_CAM_ENTRY_SIZE		12
#define ICE_SID_RXPARSER_NOMATCH_SPILL_ENTRY_SIZE	13
#define ICE_SID_RXPARSER_BOOST_TCAM_ENTRY_SIZE		88
#define ICE_SID_RXPARSER_MARKER_TYPE_ENTRY_SIZE		24
#define ICE_SID_RXPARSER_MARKER_GRP_ENTRY_SIZE		8
#define ICE_SID_RXPARSER_PROTO_GRP_ENTRY_SIZE		24
#define ICE_SID_RXPARSER_FLAG_REDIR_ENTRY_SIZE		1

#define ICE_SEC_LBL_DATA_OFFSET				2
#define ICE_SID_LBL_ENTRY_SIZE				66

void ice_lbl_dump(struct ice_hw *hw, struct ice_lbl_item *item)
{
	ice_info(hw, "index = %d\n", item->idx);
	ice_info(hw, "label = %s\n", item->label);
}

void ice_parse_item_dflt(struct ice_hw *hw, u16 idx, void *item,
			 void *data, int size)
{
	ice_memcpy(item, data, size, ICE_DMA_TO_NONDMA);
}

/**
 * ice_parser_sect_item_get - parse a item from a section
 * @sect_type: section type
 * @section: section object
 * @index: index of the item to get
 * @offset: dummy as prototype of ice_pkg_enum_entry's last parameter
 */
void *ice_parser_sect_item_get(u32 sect_type, void *section,
			       u32 index, u32 *offset)
{
	struct ice_pkg_sect_hdr *hdr;
	int data_off = ICE_SEC_DATA_OFFSET;
	int size;

	if (!section)
		return NULL;

	switch (sect_type) {
	case ICE_SID_RXPARSER_IMEM:
		size = ICE_SID_RXPARSER_IMEM_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_METADATA_INIT:
		size = ICE_SID_RXPARSER_METADATA_INIT_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_CAM:
		size = ICE_SID_RXPARSER_CAM_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_PG_SPILL:
		size = ICE_SID_RXPARSER_PG_SPILL_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_NOMATCH_CAM:
		size = ICE_SID_RXPARSER_NOMATCH_CAM_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_NOMATCH_SPILL:
		size = ICE_SID_RXPARSER_NOMATCH_SPILL_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_BOOST_TCAM:
		size = ICE_SID_RXPARSER_BOOST_TCAM_ENTRY_SIZE;
		break;
	case ICE_SID_LBL_RXPARSER_TMEM:
		data_off = ICE_SEC_LBL_DATA_OFFSET;
		size = ICE_SID_LBL_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_MARKER_PTYPE:
		size = ICE_SID_RXPARSER_MARKER_TYPE_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_MARKER_GRP:
		size = ICE_SID_RXPARSER_MARKER_GRP_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_PROTO_GRP:
		size = ICE_SID_RXPARSER_PROTO_GRP_ENTRY_SIZE;
		break;
	case ICE_SID_RXPARSER_FLAG_REDIR:
		size = ICE_SID_RXPARSER_FLAG_REDIR_ENTRY_SIZE;
		break;
	default:
		return NULL;
	}

	hdr = (struct ice_pkg_sect_hdr *)section;
	if (index >= LE16_TO_CPU(hdr->count))
		return NULL;

	return (void *)((uintptr_t)section + data_off + index * size);
}

/**
 * ice_parser_create_table - create a item table from a section
 * @hw: pointer to the hardware structure
 * @sect_type: section type
 * @item_size: item size in byte
 * @length: number of items in the table to create
 * @item_get: the function will be parsed to ice_pkg_enum_entry
 * @parse_item: the function to parse the item
 * @no_offset: ignore header offset, calculate index from 0
 */
void *ice_parser_create_table(struct ice_hw *hw, u32 sect_type,
			      u32 item_size, u32 length,
			      void *(*item_get)(u32 sect_type, void *section,
						u32 index, u32 *offset),
			      void (*parse_item)(struct ice_hw *hw, u16 idx,
						 void *item, void *data,
						 int size),
			      bool no_offset)
{
	struct ice_seg *seg = hw->seg;
	struct ice_pkg_enum state;
	u16 idx = 0xffff;
	void *table;
	void *data;

	if (!seg)
		return NULL;

	table = ice_malloc(hw, item_size * length);
	if (!table) {
		ice_debug(hw, ICE_DBG_PARSER, "failed to allocate memory for table type %d.\n",
			  sect_type);
		return NULL;
	}

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);
	do {
		data = ice_pkg_enum_entry(seg, &state, sect_type, NULL,
					  item_get);
		seg = NULL;
		if (data) {
			struct ice_pkg_sect_hdr *hdr =
				(struct ice_pkg_sect_hdr *)state.sect;

			if (no_offset)
				idx++;
			else
				idx = hdr->offset + state.entry_idx;
			parse_item(hw, idx,
				   (void *)((uintptr_t)table + idx * item_size),
				   data, item_size);
		}
	} while (data);

	return table;
}

/**
 * ice_parser_create - create a parser instance
 * @hw: pointer to the hardware structure
 * @psr: output parameter for a new parser instance be created
 */
enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr)
{
	enum ice_status status;
	struct ice_parser *p;

	p = (struct ice_parser *)ice_malloc(hw, sizeof(struct ice_parser));
	if (!p)
		return ICE_ERR_NO_MEMORY;

	p->hw = hw;
	p->rt.psr = p;

	p->imem_table = ice_imem_table_get(hw);
	if (!p->imem_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->mi_table = ice_metainit_table_get(hw);
	if (!p->mi_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->pg_cam_table = ice_pg_cam_table_get(hw);
	if (!p->pg_cam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->pg_sp_cam_table = ice_pg_sp_cam_table_get(hw);
	if (!p->pg_sp_cam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->pg_nm_cam_table = ice_pg_nm_cam_table_get(hw);
	if (!p->pg_nm_cam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->pg_nm_sp_cam_table = ice_pg_nm_sp_cam_table_get(hw);
	if (!p->pg_nm_sp_cam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->bst_tcam_table = ice_bst_tcam_table_get(hw);
	if (!p->bst_tcam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->bst_lbl_table = ice_bst_lbl_table_get(hw);
	if (!p->bst_lbl_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->ptype_mk_tcam_table = ice_ptype_mk_tcam_table_get(hw);
	if (!p->ptype_mk_tcam_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->mk_grp_table = ice_mk_grp_table_get(hw);
	if (!p->mk_grp_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->proto_grp_table = ice_proto_grp_table_get(hw);
	if (!p->proto_grp_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->flg_rd_table = ice_flg_rd_table_get(hw);
	if (!p->flg_rd_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->xlt_kb_sw = ice_xlt_kb_get_sw(hw);
	if (!p->xlt_kb_sw) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->xlt_kb_acl = ice_xlt_kb_get_acl(hw);
	if (!p->xlt_kb_acl) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->xlt_kb_fd = ice_xlt_kb_get_fd(hw);
	if (!p->xlt_kb_fd) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	p->xlt_kb_rss = ice_xlt_kb_get_rss(hw);
	if (!p->xlt_kb_rss) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	*psr = p;
	return ICE_SUCCESS;
err:
	ice_parser_destroy(p);
	return status;
}

/**
 * ice_parser_destroy - destroy a parser instance
 * @psr: pointer to a parser instance
 */
void ice_parser_destroy(struct ice_parser *psr)
{
	ice_free(psr->hw, psr->imem_table);
	ice_free(psr->hw, psr->mi_table);
	ice_free(psr->hw, psr->pg_cam_table);
	ice_free(psr->hw, psr->pg_sp_cam_table);
	ice_free(psr->hw, psr->pg_nm_cam_table);
	ice_free(psr->hw, psr->pg_nm_sp_cam_table);
	ice_free(psr->hw, psr->bst_tcam_table);
	ice_free(psr->hw, psr->bst_lbl_table);
	ice_free(psr->hw, psr->ptype_mk_tcam_table);
	ice_free(psr->hw, psr->mk_grp_table);
	ice_free(psr->hw, psr->proto_grp_table);
	ice_free(psr->hw, psr->flg_rd_table);
	ice_free(psr->hw, psr->xlt_kb_sw);
	ice_free(psr->hw, psr->xlt_kb_acl);
	ice_free(psr->hw, psr->xlt_kb_fd);
	ice_free(psr->hw, psr->xlt_kb_rss);

	ice_free(psr->hw, psr);
}

/**
 * ice_parser_run - parse on a packet in binary and return the result
 * @psr: pointer to a parser instance
 * @pkt_buf: packet data
 * @pkt_len: packet length
 * @rslt: input/output parameter to save parser result.
 */
enum ice_status ice_parser_run(struct ice_parser *psr, const u8 *pkt_buf,
			       int pkt_len, struct ice_parser_result *rslt)
{
	ice_parser_rt_reset(&psr->rt);
	ice_parser_rt_pktbuf_set(&psr->rt, pkt_buf, pkt_len);

	return ice_parser_rt_execute(&psr->rt, rslt);
}

/**
 * ice_parser_result_dump - dump a parser result info
 * @hw: pointer to the hardware structure
 * @rslt: parser result info to dump
 */
void ice_parser_result_dump(struct ice_hw *hw, struct ice_parser_result *rslt)
{
	int i;

	ice_info(hw, "ptype = %d\n", rslt->ptype);
	for (i = 0; i < rslt->po_num; i++)
		ice_info(hw, "proto = %d, offset = %d\n",
			 rslt->po[i].proto_id, rslt->po[i].offset);

	ice_info(hw, "flags_psr = 0x%016" PRIx64 "\n", rslt->flags_psr);
	ice_info(hw, "flags_pkt = 0x%016" PRIx64 "\n", rslt->flags_pkt);
	ice_info(hw, "flags_sw = 0x%04x\n", rslt->flags_sw);
	ice_info(hw, "flags_fd = 0x%04x\n", rslt->flags_fd);
	ice_info(hw, "flags_rss = 0x%04x\n", rslt->flags_rss);
}

static void _bst_vm_set(struct ice_parser *psr, const char *prefix, bool on)
{
	struct ice_bst_tcam_item *item;
	u16 i = 0;

	while (true) {
		item = ice_bst_tcam_search(psr->bst_tcam_table,
					   psr->bst_lbl_table,
					   prefix, &i);
		if (!item)
			break;
		item->key[0] = (u8)(on ? 0xff : 0xfe);
		item->key_inv[0] = (u8)(on ? 0xff : 0xfe);
		i++;
	}
}

/**
 * ice_parser_dvm_set - configure double vlan mode for parser
 * @psr: pointer to a parser instance
 * @on: true to turn on; false to turn off
 */
void ice_parser_dvm_set(struct ice_parser *psr, bool on)
{
	_bst_vm_set(psr, "BOOST_MAC_VLAN_DVM", on);
	_bst_vm_set(psr, "BOOST_MAC_VLAN_SVM", !on);
}

static enum ice_status
_tunnel_port_set(struct ice_parser *psr, const char *prefix, u16 udp_port,
		 bool on)
{
	u8 *buf = (u8 *)&udp_port;
	struct ice_bst_tcam_item *item;
	u16 i = 0;

	while (true) {
		item = ice_bst_tcam_search(psr->bst_tcam_table,
					   psr->bst_lbl_table,
					   prefix, &i);
		if (!item)
			break;

		/* found empty slot to add */
		if (on && item->key[16] == 0xfe && item->key_inv[16] == 0xfe) {
			item->key_inv[15] = buf[0];
			item->key_inv[16] = buf[1];
			item->key[15] = (u8)(0xff - buf[0]);
			item->key[16] = (u8)(0xff - buf[1]);

			return ICE_SUCCESS;
		/* found a matched slot to delete */
		} else if (!on && (item->key_inv[15] == buf[0] ||
			   item->key_inv[16] == buf[1])) {
			item->key_inv[15] = 0xff;
			item->key_inv[16] = 0xfe;
			item->key[15] = 0xff;
			item->key[16] = 0xfe;

			return ICE_SUCCESS;
		}
		i++;
	}

	return ICE_ERR_PARAM;
}

/**
 * ice_parser_vxlan_tunnel_set - configure vxlan tunnel for parser
 * @psr: pointer to a parser instance
 * @udp_port: vxlan tunnel port in UDP header
 * @on: true to turn on; false to turn off
 */
enum ice_status ice_parser_vxlan_tunnel_set(struct ice_parser *psr,
					    u16 udp_port, bool on)
{
	return _tunnel_port_set(psr, "TNL_VXLAN", udp_port, on);
}

/**
 * ice_parser_geneve_tunnel_set - configure geneve tunnel for parser
 * @psr: pointer to a parser instance
 * @udp_port: geneve tunnel port in UDP header
 * @on: true to turn on; false to turn off
 */
enum ice_status ice_parser_geneve_tunnel_set(struct ice_parser *psr,
					     u16 udp_port, bool on)
{
	return _tunnel_port_set(psr, "TNL_GENEVE", udp_port, on);
}

/**
 * ice_parser_ecpri_tunnel_set - configure ecpri tunnel for parser
 * @psr: pointer to a parser instance
 * @udp_port: ecpri tunnel port in UDP header
 * @on: true to turn on; false to turn off
 */
enum ice_status ice_parser_ecpri_tunnel_set(struct ice_parser *psr,
					    u16 udp_port, bool on)
{
	return _tunnel_port_set(psr, "TNL_UDP_ECPRI", udp_port, on);
}

static bool _nearest_proto_id(struct ice_parser_result *rslt, u16 offset,
			      u8 *proto_id, u16 *proto_off)
{
	u16 dist = 0xffff;
	u8 p = 0;
	int i;

	for (i = 0; i < rslt->po_num; i++) {
		if (offset < rslt->po[i].offset)
			continue;
		if (offset - rslt->po[i].offset < dist) {
			p = rslt->po[i].proto_id;
			dist = offset - rslt->po[i].offset;
		}
	}

	if (dist % 2)
		return false;

	*proto_id = p;
	*proto_off = dist;

	return true;
}

/** default flag mask to cover GTP_EH_PDU, GTP_EH_PDU_LINK and TUN2
 * In future, the flag masks should learn from DDP
 */
#define ICE_KEYBUILD_FLAG_MASK_DEFAULT_SW	0x4002
#define ICE_KEYBUILD_FLAG_MASK_DEFAULT_ACL	0x0000
#define ICE_KEYBUILD_FLAG_MASK_DEFAULT_FD	0x6080
#define ICE_KEYBUILD_FLAG_MASK_DEFAULT_RSS	0x6010

/**
 * ice_parser_profile_init  - initialize a FXP profile base on parser result
 * @rslt: a instance of a parser result
 * @pkt_buf: packet data buffer
 * @msk_buf: packet mask buffer
 * @buf_len: packet length
 * @blk: FXP pipeline stage
 * @prefix_match: match protocol stack exactly or only prefix
 * @prof: input/output parameter to save the profile
 */
enum ice_status ice_parser_profile_init(struct ice_parser_result *rslt,
					const u8 *pkt_buf, const u8 *msk_buf,
					int buf_len, enum ice_block blk,
					bool prefix_match,
					struct ice_parser_profile *prof)
{
	u8 proto_id = 0xff;
	u16 proto_off = 0;
	u16 off;

	ice_memset(prof, 0, sizeof(*prof), ICE_NONDMA_MEM);
	ice_set_bit(rslt->ptype, prof->ptypes);
	if (blk == ICE_BLK_SW) {
		prof->flags = rslt->flags_sw;
		prof->flags_msk = ICE_KEYBUILD_FLAG_MASK_DEFAULT_SW;
	} else if (blk == ICE_BLK_ACL) {
		prof->flags = rslt->flags_acl;
		prof->flags_msk = ICE_KEYBUILD_FLAG_MASK_DEFAULT_ACL;
	} else if (blk == ICE_BLK_FD) {
		prof->flags = rslt->flags_fd;
		prof->flags_msk = ICE_KEYBUILD_FLAG_MASK_DEFAULT_FD;
	} else if (blk == ICE_BLK_RSS) {
		prof->flags = rslt->flags_rss;
		prof->flags_msk = ICE_KEYBUILD_FLAG_MASK_DEFAULT_RSS;
	} else {
		return ICE_ERR_PARAM;
	}

	for (off = 0; off < buf_len - 1; off++) {
		if (msk_buf[off] == 0 && msk_buf[off + 1] == 0)
			continue;
		if (!_nearest_proto_id(rslt, off, &proto_id, &proto_off))
			continue;
		if (prof->fv_num >= 32)
			return ICE_ERR_PARAM;

		prof->fv[prof->fv_num].proto_id = proto_id;
		prof->fv[prof->fv_num].offset = proto_off;
		prof->fv[prof->fv_num].spec = *(const u16 *)&pkt_buf[off];
		prof->fv[prof->fv_num].msk = *(const u16 *)&msk_buf[off];
		prof->fv_num++;
	}

	return ICE_SUCCESS;
}

/**
 * ice_parser_profile_dump - dump an FXP profile info
 * @hw: pointer to the hardware structure
 * @prof: profile info to dump
 */
void ice_parser_profile_dump(struct ice_hw *hw, struct ice_parser_profile *prof)
{
	u16 i;

	ice_info(hw, "ptypes:\n");
	for (i = 0; i < ICE_FLOW_PTYPE_MAX; i++)
		if (ice_is_bit_set(prof->ptypes, i))
			ice_info(hw, "\t%d\n", i);

	for (i = 0; i < prof->fv_num; i++)
		ice_info(hw, "proto = %d, offset = %d spec = 0x%04x, mask = 0x%04x\n",
			 prof->fv[i].proto_id, prof->fv[i].offset,
			 prof->fv[i].spec, prof->fv[i].msk);

	ice_info(hw, "flags = 0x%04x\n", prof->flags);
	ice_info(hw, "flags_msk = 0x%04x\n", prof->flags_msk);
}

/**
 * ice_check_ddp_support_proto_id - check DDP package file support protocol ID
 * @hw: pointer to the HW struct
 * @proto_id: protocol ID value
 *
 * This function maintains the compatibility of the program process by checking
 * whether the current DDP file supports the required protocol ID.
 */
bool ice_check_ddp_support_proto_id(struct ice_hw *hw,
				    enum ice_prot_id proto_id)
{
	struct ice_proto_grp_item *proto_grp_table;
	struct ice_proto_grp_item *proto_grp;
	bool exist = false;
	u16 idx, i;

	proto_grp_table = ice_proto_grp_table_get(hw);
	if (!proto_grp_table)
		return false;

	for (idx = 0; idx < ICE_PROTO_GRP_TABLE_SIZE; idx++) {
		proto_grp = &proto_grp_table[idx];
		for (i = 0; i < ICE_PROTO_COUNT_PER_GRP; i++) {
			if (proto_grp->po[i].proto_id == proto_id) {
				exist = true;
				goto exit;
			}
		}
	}

exit:
	ice_free(hw, proto_grp_table);
	return exist;
}
