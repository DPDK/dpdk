/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_PARSER_H_
#define _ICE_PARSER_H_

#include "ice_metainit.h"
#include "ice_imem.h"
#include "ice_pg_cam.h"
#include "ice_bst_tcam.h"
#include "ice_ptype_mk.h"
#include "ice_mk_grp.h"
#include "ice_proto_grp.h"
#include "ice_flg_rd.h"
#include "ice_xlt_kb.h"
#include "ice_parser_rt.h"
#include "ice_tmatch.h"

struct ice_parser {
	struct ice_hw *hw; /* pointer to the hardware structure */

	/* load data from section ICE_SID_RX_PARSER_IMEM */
	struct ice_imem_item *imem_table;
	/* load data from section ICE_SID_RXPARSER_METADATA_INIT */
	struct ice_metainit_item *mi_table;
	/* load data from section ICE_SID_RXPARSER_CAM */
	struct ice_pg_cam_item *pg_cam_table;
	/* load data from section ICE_SID_RXPARSER_PG_SPILL */
	struct ice_pg_cam_item *pg_sp_cam_table;
	/* load data from section ICE_SID_RXPARSER_NOMATCH_CAM */
	struct ice_pg_nm_cam_item *pg_nm_cam_table;
	/* load data from section ICE_SID_RXPARSER_NOMATCH_SPILL */
	struct ice_pg_nm_cam_item *pg_nm_sp_cam_table;
	/* load data from section ICE_SID_RXPARSER_BOOST_TCAM */
	struct ice_bst_tcam_item *bst_tcam_table;
	/* load data from section ICE_SID_LBL_RXPARSER_TMEM */
	struct ice_lbl_item *bst_lbl_table;
	/* load data from section ICE_SID_RXPARSER_MARKER_PTYPE */
	struct ice_ptype_mk_tcam_item *ptype_mk_tcam_table;
	/* load data from section ICE_SID_RXPARSER_MARKER_GRP */
	struct ice_mk_grp_item *mk_grp_table;
	/* load data from section ICE_SID_RXPARSER_PROTO_GRP */
	struct ice_proto_grp_item *proto_grp_table;
	/* load data from section ICE_SID_RXPARSER_FLAG_REDIR */
	struct ice_flg_rd_item *flg_rd_table;
	/* load data from section ICE_SID_XLT_KEY_BUILDER_SW */
	struct ice_xlt_kb *xlt_kb_sw;
	/* load data from section ICE_SID_XLT_KEY_BUILDER_ACL */
	struct ice_xlt_kb *xlt_kb_acl;
	/* load data from section ICE_SID_XLT_KEY_BUILDER_FD */
	struct ice_xlt_kb *xlt_kb_fd;
	/* load data from section ICE_SID_XLT_KEY_BUILDER_RSS */
	struct ice_xlt_kb *xlt_kb_rss;
	struct ice_parser_rt rt; /* parser runtime */
};

enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr);
void ice_parser_destroy(struct ice_parser *psr);
void ice_parser_dvm_set(struct ice_parser *psr, bool on);
enum ice_status ice_parser_vxlan_tunnel_set(struct ice_parser *psr,
					    u16 udp_port, bool on);
enum ice_status ice_parser_geneve_tunnel_set(struct ice_parser *psr,
					     u16 udp_port, bool on);
enum ice_status ice_parser_ecpri_tunnel_set(struct ice_parser *psr,
					    u16 udp_port, bool on);

struct ice_parser_proto_off {
	u8 proto_id; /* hardware protocol ID */
	u16 offset;  /* offset where the  protocol header start */
};

struct ice_parser_result {
	u16 ptype; /* 16 bits hardware PTYPE */
	/* protocol and header offset pairs */
	struct ice_parser_proto_off po[16];
	int po_num; /* number of pairs must <= 16 */
	u64 flags_psr; /* 64 bits parser flags */
	u64 flags_pkt; /* 64 bits packet flags */
	u16 flags_sw; /* 16 bits key builder flag for SW */
	u16 flags_acl; /* 16 bits key builder flag for ACL */
	u16 flags_fd; /* 16 bits key builder flag for FD */
	u16 flags_rss; /* 16 bits key builder flag for RSS */
};

enum ice_status ice_parser_run(struct ice_parser *psr, const u8 *pkt_buf,
			       int pkt_len, struct ice_parser_result *rslt);
void ice_parser_result_dump(struct ice_hw *hw, struct ice_parser_result *rslt);

struct ice_parser_fv {
	u8 proto_id; /* hardware protocol ID */
	u16 offset; /* offset from the start of the protocol header */
	u16 spec; /* 16 bits pattern to match */
	u16 msk; /* 16 bits pattern mask */
};

struct ice_parser_profile {
	struct ice_parser_fv fv[48]; /* field vector array */
	int fv_num; /* field vector number must <= 48 */
	u16 flags; /* 16 bits key builder flag */
	u16 flags_msk; /* key builder flag masker */
	/* 1024 bits PTYPE bitmap */
	ice_declare_bitmap(ptypes, ICE_FLOW_PTYPE_MAX);
};

enum ice_status ice_parser_profile_init(struct ice_parser_result *rslt,
					const u8 *pkt_buf, const u8 *msk_buf,
					int buf_len, enum ice_block blk,
					bool prefix_match,
					struct ice_parser_profile *prof);
void ice_parser_profile_dump(struct ice_hw *hw,
			     struct ice_parser_profile *prof);
bool ice_check_ddp_support_proto_id(struct ice_hw *hw,
				    enum ice_prot_id proto_id);
#endif /* _ICE_PARSER_H_ */
