/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TEMPLATE_STRUCT_H_
#define _ULP_TEMPLATE_STRUCT_H_

#include <stdint.h>
#include "rte_ether.h"
#include "rte_icmp.h"
#include "rte_ip.h"
#include "rte_tcp.h"
#include "rte_udp.h"
#include "rte_esp.h"
#include "rte_sctp.h"
#include "rte_flow.h"
#include "tf_core.h"

/* Number of fields for each protocol */
#define BNXT_ULP_PROTO_HDR_SVIF_NUM	2
#define BNXT_ULP_PROTO_HDR_ETH_NUM	3
#define BNXT_ULP_PROTO_HDR_S_VLAN_NUM	3
#define BNXT_ULP_PROTO_HDR_VLAN_NUM	6
#define BNXT_ULP_PROTO_HDR_IPV4_NUM	10
#define BNXT_ULP_PROTO_HDR_IPV6_NUM	8
#define BNXT_ULP_PROTO_HDR_UDP_NUM	4
#define BNXT_ULP_PROTO_HDR_TCP_NUM	9
#define BNXT_ULP_PROTO_HDR_VXLAN_NUM	4
#define BNXT_ULP_PROTO_HDR_GRE_NUM	2
#define BNXT_ULP_PROTO_HDR_ICMP_NUM	5
#define BNXT_ULP_PROTO_HDR_MAX		128
#define BNXT_ULP_PROTO_HDR_ENCAP_MAX	64
#define BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX	1

/* Direction attributes */
#define BNXT_ULP_FLOW_ATTR_TRANSFER	0x1
#define BNXT_ULP_FLOW_ATTR_INGRESS	0x2
#define BNXT_ULP_FLOW_ATTR_EGRESS	0x4

struct ulp_rte_hdr_bitmap {
	uint64_t	bits;
};

struct ulp_rte_field_bitmap {
	uint64_t	bits;
};

/* Structure to store the protocol fields */
#define RTE_PARSER_FLOW_HDR_FIELD_SIZE		16
struct ulp_rte_hdr_field {
	uint8_t		spec[RTE_PARSER_FLOW_HDR_FIELD_SIZE];
	uint8_t		mask[RTE_PARSER_FLOW_HDR_FIELD_SIZE];
	uint32_t	size;
};

struct ulp_rte_act_bitmap {
	uint64_t	bits;
};

/* Structure to hold the action property details. */
struct ulp_rte_act_prop {
	uint8_t	act_details[BNXT_ULP_ACT_PROP_IDX_LAST];
};

/* Structure to be used for passing all the parser functions */
struct ulp_rte_parser_params {
	struct ulp_rte_hdr_bitmap	hdr_bitmap;
	struct ulp_rte_hdr_bitmap	enc_hdr_bitmap;
	struct ulp_rte_hdr_bitmap	hdr_fp_bit;
	struct ulp_rte_field_bitmap	fld_bitmap;
	struct ulp_rte_field_bitmap	fld_s_bitmap;
	struct ulp_rte_hdr_field	hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	struct ulp_rte_hdr_field	enc_field[BNXT_ULP_PROTO_HDR_ENCAP_MAX];
	uint64_t			comp_fld[BNXT_ULP_CF_IDX_LAST];
	uint32_t			field_idx;
	struct ulp_rte_act_bitmap	act_bitmap;
	struct ulp_rte_act_prop		act_prop;
	uint32_t			dir_attr;
	uint32_t			priority;
	uint32_t			fid;
	uint32_t			parent_flow;
	uint32_t			child_flow;
	uint16_t			func_id;
	uint16_t			port_id;
	uint32_t			class_id;
	uint32_t			act_tmpl;
	struct bnxt_ulp_context		*ulp_ctx;
	uint32_t			hdr_sig_id;
	uint64_t			flow_sig_id;
	uint32_t			flow_pattern_id;
	uint32_t			act_pattern_id;
	uint8_t				app_id;
	uint8_t				tun_idx;

};

/* Flow Parser Header Information Structure */
struct bnxt_ulp_rte_hdr_info {
	enum bnxt_ulp_hdr_type					hdr_type;
	/* Flow Parser Protocol Header Function Prototype */
	int (*proto_hdr_func)(const struct rte_flow_item	*item_list,
			      struct ulp_rte_parser_params	*params);
};

/* Flow Parser Header Information Structure Array defined in template source*/
extern struct bnxt_ulp_rte_hdr_info	ulp_hdr_info[];
extern struct bnxt_ulp_rte_hdr_info	ulp_vendor_hdr_info[];

/* Flow Parser Action Information Structure */
struct bnxt_ulp_rte_act_info {
	enum bnxt_ulp_act_type					act_type;
	/* Flow Parser Protocol Action Function Prototype */
	int32_t (*proto_act_func)
		(const struct rte_flow_action	*action_item,
		 struct ulp_rte_parser_params	*params);
};

/* Flow Parser Action Information Structure Array defined in template source*/
extern struct bnxt_ulp_rte_act_info	ulp_act_info[];
extern struct bnxt_ulp_rte_act_info	ulp_vendor_act_info[];

/* Flow Matcher structures */
struct bnxt_ulp_header_match_info {
	struct ulp_rte_hdr_bitmap		hdr_bitmap;
	uint32_t				start_idx;
	uint32_t				num_entries;
	uint32_t				class_tmpl_id;
	uint32_t				act_vnic;
};

struct ulp_rte_bitmap {
	uint64_t	bits;
};

struct bnxt_ulp_class_match_info {
	struct ulp_rte_bitmap	hdr_sig;
	struct ulp_rte_bitmap	field_sig;
	uint32_t		class_hid;
	uint32_t		class_tid;
	uint8_t			act_vnic;
	uint8_t			wc_pri;
	uint8_t			app_sig;
	uint32_t		hdr_sig_id;
	uint64_t		flow_sig_id;
	uint32_t		flow_pattern_id;
};

/* Flow Matcher templates Structure for class entries */
extern uint16_t ulp_class_sig_tbl[];
extern struct bnxt_ulp_class_match_info ulp_class_match_list[];

/* Flow Matcher Action structures */
struct bnxt_ulp_action_match_info {
	struct ulp_rte_act_bitmap		act_bitmap;
	uint32_t				act_tmpl_id;
};

struct bnxt_ulp_act_match_info {
	struct ulp_rte_bitmap	act_sig;
	uint32_t		act_hid;
	uint32_t		act_tid;
	uint32_t		act_pattern_id;
	uint8_t			app_sig;
};

/* Flow Matcher templates Structure for action entries */
extern	uint16_t ulp_act_sig_tbl[];
extern struct bnxt_ulp_act_match_info ulp_act_match_list[];

/* Device Specific Tables for mapper */
struct bnxt_ulp_mapper_cond_info {
	enum bnxt_ulp_cond_opc cond_opcode;
	uint32_t cond_operand;
};

struct bnxt_ulp_mapper_cond_list_info {
	enum bnxt_ulp_cond_list_opc cond_list_opcode;
	uint32_t cond_start_idx;
	uint32_t cond_nums;
	int32_t cond_true_goto;
	int32_t cond_false_goto;
};

struct bnxt_ulp_mapper_func_info {
	enum bnxt_ulp_func_opc		func_opc;
	enum bnxt_ulp_func_src		func_src1;
	enum bnxt_ulp_func_src		func_src2;
	uint16_t			func_opr1;
	uint16_t			func_opr2;
	uint16_t			func_dst_opr;
};

struct bnxt_ulp_template_device_tbls {
	struct bnxt_ulp_mapper_tmpl_info *tmpl_list;
	uint32_t tmpl_list_size;
	struct bnxt_ulp_mapper_tbl_info *tbl_list;
	uint32_t tbl_list_size;
	struct bnxt_ulp_mapper_key_info *key_info_list;
	uint32_t key_info_list_size;
	struct bnxt_ulp_mapper_field_info *result_field_list;
	uint32_t result_field_list_size;
	struct bnxt_ulp_mapper_ident_info *ident_list;
	uint32_t ident_list_size;
	struct bnxt_ulp_mapper_cond_info *cond_list;
	uint32_t cond_list_size;
};

struct bnxt_ulp_dyn_size_map {
	uint32_t		slab_size;
	enum tf_tbl_type	tbl_type;
};

/* Device specific parameters */
struct bnxt_ulp_device_params {
	uint8_t				description[16];
	enum bnxt_ulp_byte_order	key_byte_order;
	enum bnxt_ulp_byte_order	result_byte_order;
	enum bnxt_ulp_byte_order	encap_byte_order;
	enum bnxt_ulp_byte_order	wc_key_byte_order;
	enum bnxt_ulp_byte_order	em_byte_order;
	uint8_t				encap_byte_swap;
	uint8_t				num_phy_ports;
	uint32_t			mark_db_lfid_entries;
	uint64_t			mark_db_gfid_entries;
	uint64_t			int_flow_db_num_entries;
	uint64_t			ext_flow_db_num_entries;
	uint32_t			flow_count_db_entries;
	uint32_t			fdb_parent_flow_entries;
	uint32_t			num_resources_per_flow;
	uint32_t			ext_cntr_table_type;
	uint64_t			byte_count_mask;
	uint64_t			packet_count_mask;
	uint32_t			byte_count_shift;
	uint32_t			packet_count_shift;
	uint32_t			dynamic_pad_en;
	uint32_t			dynamic_sram_en;
	uint32_t			dyn_encap_list_size;
	struct bnxt_ulp_dyn_size_map	dyn_encap_sizes[4];
	uint32_t			dyn_modify_list_size;
	struct bnxt_ulp_dyn_size_map	dyn_modify_sizes[4];
	uint16_t			em_blk_size_bits;
	uint16_t			em_blk_align_bits;
	uint16_t			em_key_align_bytes;
	uint16_t			em_result_size_bits;
	uint16_t			wc_slice_width;
	uint16_t			wc_max_slices;
	uint32_t			wc_mode_list[4];
	uint32_t			wc_mod_list_max_size;
	uint32_t			wc_ctl_size_bits;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;
};

/* Flow Mapper */
struct bnxt_ulp_mapper_tmpl_info {
	uint32_t		device_name;
	uint32_t		start_tbl_idx;
	uint32_t		num_tbls;
	struct bnxt_ulp_mapper_cond_list_info reject_info;
};

struct bnxt_ulp_mapper_tbl_info {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	enum bnxt_ulp_resource_sub_type	resource_sub_type;
	struct bnxt_ulp_mapper_cond_list_info execute_info;
	struct bnxt_ulp_mapper_func_info func_info;
	enum bnxt_ulp_cond_opc cond_opcode;
	uint32_t cond_operand;
	uint8_t				direction;
	enum bnxt_ulp_pri_opc		pri_opcode;
	uint32_t			pri_operand;

	/* conflict resolution opcode */
	enum bnxt_ulp_accept_opc	accept_opcode;

	enum bnxt_ulp_critical_resource		critical_resource;

	/* Information for accessing the ulp_key_field_list */
	uint32_t	key_start_idx;
	uint16_t	key_bit_size;
	uint16_t	key_num_fields;
	/* Size of the blob that holds the key */
	uint16_t	blob_key_bit_size;
	uint16_t	record_size;

	/* Information for accessing the ulp_class_result_field_list */
	uint32_t	result_start_idx;
	uint16_t	result_bit_size;
	uint16_t	result_num_fields;
	uint16_t	encap_num_fields;

	/* Information for accessing the ulp_ident_list */
	uint32_t	ident_start_idx;
	uint16_t	ident_nums;

	enum bnxt_ulp_mark_db_opc	mark_db_opcode;

	/* Table opcode for table operations */
	uint32_t			tbl_opcode;
	uint32_t			tbl_operand;
	enum bnxt_ulp_generic_tbl_lkup_type gen_tbl_lkup_type;

	/* FDB table opcode */
	enum bnxt_ulp_fdb_opc		fdb_opcode;
	uint32_t			fdb_operand;

	/* Shared session */
	enum bnxt_ulp_shared_session	shared_session;
};

struct bnxt_ulp_mapper_field_info {
	uint8_t				description[64];
	uint16_t			field_bit_size;
	enum bnxt_ulp_field_opc		field_opc;
	enum bnxt_ulp_field_src		field_src1;
	uint8_t				field_opr1[16];
	enum bnxt_ulp_field_src		field_src2;
	uint8_t				field_opr2[16];
	enum bnxt_ulp_field_src		field_src3;
	uint8_t				field_opr3[16];
};

struct bnxt_ulp_mapper_key_info {
	struct bnxt_ulp_mapper_field_info	field_info_spec;
	struct bnxt_ulp_mapper_field_info	field_info_mask;
};

struct bnxt_ulp_mapper_ident_info {
	uint8_t		description[64];
	uint32_t	resource_func;

	uint16_t	ident_type;
	uint16_t	ident_bit_size;
	uint16_t	ident_bit_pos;
	enum bnxt_ulp_rf_idx	regfile_idx;
};

struct bnxt_ulp_glb_resource_info {
	uint8_t				app_id;
	enum bnxt_ulp_device_id		device_id;
	enum tf_dir			direction;
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	enum bnxt_ulp_glb_rf_idx	glb_regfile_index;
};

struct bnxt_ulp_resource_resv_info {
	uint8_t				app_id;
	enum bnxt_ulp_device_id		device_id;
	enum tf_dir			direction;
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	uint32_t			count;
};

struct bnxt_ulp_app_capabilities_info {
	uint8_t				app_id;
	enum bnxt_ulp_device_id		device_id;
	uint32_t			flags;
};

struct bnxt_ulp_cache_tbl_params {
	uint16_t num_entries;
};

struct bnxt_ulp_generic_tbl_params {
	const char			*name;
	uint16_t			result_num_entries;
	uint16_t			result_num_bytes;
	enum bnxt_ulp_byte_order	result_byte_order;
	uint32_t			hash_tbl_entries;
	uint16_t			num_buckets;
	uint16_t			key_num_bytes;
};

struct bnxt_ulp_shared_act_info {
	uint64_t act_bitmask;
};

/*
 * Flow Mapper Static Data Externs:
 * Access to the below static data should be done through access functions and
 * directly throughout the code.
 */

/*
 * The ulp_device_params is indexed by the dev_id.
 * This table maintains the device specific parameters.
 */
extern struct bnxt_ulp_device_params ulp_device_params[];

/*
 * The ulp_act_prop_map_table provides the mapping to index and size of action
 * properties.
 */
extern uint32_t ulp_act_prop_map_table[];

/*
 * The ulp_glb_resource_tbl provides the list of global resources that need to
 * be initialized and where to store them.
 */
extern struct bnxt_ulp_glb_resource_info ulp_glb_resource_tbl[];

/*
 * The ulp_app_glb_resource_tbl provides the list of shared resources required
 * in the event that shared session is enabled.
 */
extern struct bnxt_ulp_glb_resource_info ulp_app_glb_resource_tbl[];

/*
 * The ulp_resource_resv_list provides the list of tf resources required when
 * calling tf_open.
 */
extern struct bnxt_ulp_resource_resv_info ulp_resource_resv_list[];

/*
 * The ulp_app_resource_resv_list provides the list of tf resources required
 * when calling tf_open.
 */
extern struct bnxt_ulp_resource_resv_info ulp_app_resource_resv_list[];

/*
 * The_app_cap_info_list provides the list of ULP capabilities per app/device.
 */
extern struct bnxt_ulp_app_capabilities_info ulp_app_cap_info_list[];

/*
 * The ulp_cache_tbl_parms table provides the sizes of the cache tables the
 * mapper must dynamically allocate during initialization.
 */
extern struct bnxt_ulp_cache_tbl_params ulp_cache_tbl_params[];

/*
 * The ulp_generic_tbl_parms table provides the sizes of the generic tables the
 * mapper must dynamically allocate during initialization.
 */
extern struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[];
/*
 * The ulp_global template table is used to initialize default entries
 * that could be reused by other templates.
 */
extern uint32_t ulp_glb_template_tbl[];

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
