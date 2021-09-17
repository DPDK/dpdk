/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
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
};

enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr);
void ice_parser_destroy(struct ice_parser *psr);
#endif /* _ICE_PARSER_H_ */
