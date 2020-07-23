/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
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
#define BNXT_ULP_PROTO_HDR_SVIF_NUM	1
#define BNXT_ULP_PROTO_HDR_ETH_NUM	3
#define BNXT_ULP_PROTO_HDR_S_VLAN_NUM	3
#define BNXT_ULP_PROTO_HDR_VLAN_NUM	6
#define BNXT_ULP_PROTO_HDR_IPV4_NUM	10
#define BNXT_ULP_PROTO_HDR_IPV6_NUM	8
#define BNXT_ULP_PROTO_HDR_UDP_NUM	4
#define BNXT_ULP_PROTO_HDR_TCP_NUM	9
#define BNXT_ULP_PROTO_HDR_VXLAN_NUM	4
#define BNXT_ULP_PROTO_HDR_MAX		128
#define BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX	0

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
	struct ulp_rte_hdr_bitmap	hdr_fp_bit;
	struct ulp_rte_field_bitmap	fld_bitmap;
	struct ulp_rte_hdr_field	hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	uint32_t			comp_fld[BNXT_ULP_CF_IDX_LAST];
	uint32_t			field_idx;
	uint32_t			vlan_idx;
	struct ulp_rte_act_bitmap	act_bitmap;
	struct ulp_rte_act_prop		act_prop;
	uint32_t			dir_attr;
	struct bnxt_ulp_context		*ulp_ctx;
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
};

/* Flow Matcher templates Structure for action entries */
extern	uint16_t ulp_act_sig_tbl[];
extern struct bnxt_ulp_act_match_info ulp_act_match_list[];

/* Device specific parameters */
struct bnxt_ulp_device_params {
	uint8_t				description[16];
	enum bnxt_ulp_flow_mem_type	flow_mem_type;
	enum bnxt_ulp_byte_order	byte_order;
	uint8_t				encap_byte_swap;
	uint8_t				num_phy_ports;
	uint32_t			mark_db_lfid_entries;
	uint64_t			mark_db_gfid_entries;
	uint64_t			flow_db_num_entries;
	uint32_t			flow_count_db_entries;
	uint32_t			num_resources_per_flow;
	uint32_t			ext_cntr_table_type;
	uint64_t			byte_count_mask;
	uint64_t			packet_count_mask;
	uint32_t			byte_count_shift;
	uint32_t			packet_count_shift;
};

/* Flow Mapper */
struct bnxt_ulp_mapper_tbl_list_info {
	uint32_t		device_name;
	uint32_t		start_tbl_idx;
	uint32_t		num_tbls;
	enum bnxt_ulp_fdb_type	flow_db_table_type;
};

struct bnxt_ulp_mapper_tbl_info {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	enum bnxt_ulp_resource_sub_type	resource_sub_type;
	enum bnxt_ulp_cond_opcode	cond_opcode;
	uint32_t			cond_operand;
	uint8_t				direction;
	uint32_t			priority;
	enum bnxt_ulp_search_before_alloc	srch_b4_alloc;
	enum bnxt_ulp_critical_resource		critical_resource;

	/* Information for accessing the ulp_key_field_list */
	uint32_t	key_start_idx;
	uint16_t	key_bit_size;
	uint16_t	key_num_fields;
	/* Size of the blob that holds the key */
	uint16_t	blob_key_bit_size;

	/* Information for accessing the ulp_class_result_field_list */
	uint32_t	result_start_idx;
	uint16_t	result_bit_size;
	uint16_t	result_num_fields;
	uint16_t	encap_num_fields;

	/* Information for accessing the ulp_ident_list */
	uint32_t	ident_start_idx;
	uint16_t	ident_nums;

	enum bnxt_ulp_mark_db_opcode	mark_db_opcode;
	enum bnxt_ulp_index_opcode	index_opcode;
	uint32_t			index_operand;
};

struct bnxt_ulp_mapper_class_key_field_info {
	uint8_t				description[64];
	enum bnxt_ulp_mapper_opc	mask_opcode;
	enum bnxt_ulp_mapper_opc	spec_opcode;
	uint16_t			field_bit_size;
	uint8_t				mask_operand[16];
	uint8_t				spec_operand[16];
};

struct bnxt_ulp_mapper_result_field_info {
	uint8_t				description[64];
	enum bnxt_ulp_mapper_opc	result_opcode;
	uint16_t			field_bit_size;
	uint8_t				result_operand[16];
	uint8_t				result_operand_true[16];
	uint8_t				result_operand_false[16];
};

struct bnxt_ulp_mapper_ident_info {
	uint8_t		description[64];
	uint32_t	resource_func;

	uint16_t	ident_type;
	uint16_t	ident_bit_size;
	uint16_t	ident_bit_pos;
	enum bnxt_ulp_regfile_index	regfile_idx;
};

struct bnxt_ulp_glb_resource_info {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	enum bnxt_ulp_glb_regfile_index	glb_regfile_index;
	enum tf_dir			direction;
};

struct bnxt_ulp_cache_tbl_params {
	uint16_t num_entries;
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
 * The ulp_class_tmpl_list and ulp_act_tmpl_list are indexed by the dev_id
 * and template id (either class or action) returned by the matcher.
 * The result provides the start index and number of entries in the connected
 * ulp_class_tbl_list/ulp_act_tbl_list.
 */
extern struct bnxt_ulp_mapper_tbl_list_info	ulp_class_tmpl_list[];
extern struct bnxt_ulp_mapper_tbl_list_info	ulp_act_tmpl_list[];

/*
 * The ulp_class_tbl_list and ulp_act_tbl_list are indexed based on the results
 * of the template lists.  Each entry describes the high level details of the
 * table entry to include the start index and number of instructions in the
 * field lists.
 */
extern struct bnxt_ulp_mapper_tbl_info	ulp_class_tbl_list[];
extern struct bnxt_ulp_mapper_tbl_info	ulp_act_tbl_list[];

/*
 * The ulp_class_result_field_list provides the instructions for creating result
 * records such as tcam/em results.
 */
extern struct bnxt_ulp_mapper_result_field_info	ulp_class_result_field_list[];

/*
 * The ulp_data_field_list provides the instructions for creating an action
 * record.  It uses the same structure as the result list, but is only used for
 * actions.
 */
extern
struct bnxt_ulp_mapper_result_field_info ulp_act_result_field_list[];

/*
 * The ulp_act_prop_map_table provides the mapping to index and size of action
 * tcam and em tables.
 */
extern
struct bnxt_ulp_mapper_class_key_field_info	ulp_class_key_field_list[];

/*
 * The ulp_ident_list provides the instructions for creating identifiers such
 * as profile ids.
 */
extern struct bnxt_ulp_mapper_ident_info	ulp_ident_list[];

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
 * The ulp_cache_tbl_parms table provides the sizes of the cache tables the
 * mapper must dynamically allocate during initialization.
 */
extern struct bnxt_ulp_cache_tbl_params ulp_cache_tbl_params[];

/*
 * The ulp_global template table is used to initialize default entries
 * that could be reused by other templates.
 */
extern uint32_t ulp_glb_template_tbl[];

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
