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

struct ulp_rte_hdr_bitmap {
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

/*
 * Structure to hold the action property details.
 * It is a array of 128 bytes.
 */
struct ulp_rte_act_prop {
	uint8_t	act_details[BNXT_ULP_ACT_PROP_IDX_LAST];
};

/* Device specific parameters */
struct bnxt_ulp_device_params {
	uint8_t				description[16];
	uint32_t			global_fid_enable;
	enum bnxt_ulp_byte_order	byte_order;
	uint8_t				encap_byte_swap;
	uint32_t			lfid_entries;
	uint32_t			lfid_entry_size;
	uint64_t			gfid_entries;
	uint32_t			gfid_entry_size;
	uint64_t			num_flows;
	uint32_t			num_resources_per_flow;
};

/* Flow Mapper */
struct bnxt_ulp_mapper_tbl_list_info {
	uint32_t	device_name;
	uint32_t	start_tbl_idx;
	uint32_t	num_tbls;
};

struct bnxt_ulp_mapper_class_tbl_info {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t	table_type;
	uint8_t		direction;
	uint8_t		mem;
	uint32_t	priority;
	uint8_t		srch_b4_alloc;
	uint32_t	critical_resource;

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

	/* Information for accessing the ulp_ident_list */
	uint32_t	ident_start_idx;
	uint16_t	ident_nums;

	uint8_t		mark_enable;
	enum bnxt_ulp_regfile_index	regfile_wr_idx;
};

struct bnxt_ulp_mapper_act_tbl_info {
	enum bnxt_ulp_resource_func	resource_func;
	enum tf_tbl_type table_type;
	uint8_t		direction;
	uint8_t		srch_b4_alloc;
	uint32_t	result_start_idx;
	uint16_t	result_bit_size;
	uint16_t	encap_num_fields;
	uint16_t	result_num_fields;

	enum bnxt_ulp_regfile_index	regfile_wr_idx;
};

struct bnxt_ulp_mapper_class_key_field_info {
	uint8_t			name[64];
	enum bnxt_ulp_mask_opc	mask_opcode;
	enum bnxt_ulp_spec_opc	spec_opcode;
	uint16_t		field_bit_size;
	uint8_t			mask_operand[16];
	uint8_t			spec_operand[16];
};

struct bnxt_ulp_mapper_result_field_info {
	uint8_t				name[64];
	enum bnxt_ulp_result_opc	result_opcode;
	uint16_t			field_bit_size;
	uint8_t				result_operand[16];
};

struct bnxt_ulp_mapper_ident_info {
	uint8_t		name[64];
	uint32_t	resource_func;

	uint16_t	ident_type;
	uint16_t	ident_bit_size;
	uint16_t	ident_bit_pos;
	enum bnxt_ulp_regfile_index	regfile_wr_idx;
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
extern struct bnxt_ulp_mapper_class_tbl_info	ulp_class_tbl_list[];
extern struct bnxt_ulp_mapper_act_tbl_info	ulp_act_tbl_list[];

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

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
