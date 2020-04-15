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

/*
 * structure to hold the action property details
 * It is a array of 128 bytes
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

struct bnxt_ulp_mapper_result_field_info {
	uint8_t				name[64];
	enum bnxt_ulp_result_opc	result_opcode;
	uint16_t			field_bit_size;
	uint8_t				result_operand[16];
};

/*
 * The ulp_device_params is indexed by the dev_id
 * This table maintains the device specific parameters
 */
extern struct bnxt_ulp_device_params ulp_device_params[];

/*
 * The ulp_data_field_list provides the instructions for creating an action
 * record.  It uses the same structure as the result list, but is only used for
 * actions.
 */
extern
struct bnxt_ulp_mapper_result_field_info ulp_act_result_field_list[];

/*
 * The ulp_act_prop_map_table provides the mapping to index and size of action
 * properties.
 */
extern uint32_t ulp_act_prop_map_table[];

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
