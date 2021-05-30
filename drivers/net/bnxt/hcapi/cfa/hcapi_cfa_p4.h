/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_P4_H_
#define _HCAPI_CFA_P4_H_

/** CFA phase 4 fix formatted table(layout) ID definition
 *
 */
enum cfa_p4_tbl_id {
	CFA_P4_TBL_L2CTXT_TCAM = 0,
	CFA_P4_TBL_L2CTXT_REMAP,
	CFA_P4_TBL_PROF_TCAM,
	CFA_P4_TBL_PROF_TCAM_REMAP,
	CFA_P4_TBL_WC_TCAM,
	CFA_P4_TBL_WC_TCAM_REC,
	CFA_P4_TBL_WC_TCAM_REMAP,
	CFA_P4_TBL_VEB_TCAM,
	CFA_P4_TBL_SP_TCAM,
	CFA_P4_TBL_PROF_SPIF_DFLT_L2CTXT,
	CFA_P4_TBL_PROF_PARIF_DFLT_ACT_REC_PTR,
	CFA_P4_TBL_PROF_PARIF_ERR_ACT_REC_PTR,
	CFA_P4_TBL_LKUP_PARIF_DFLT_ACT_REC_PTR,
	CFA_P4_TBL_MAX
};

#define CFA_P4_PROF_MAX_KEYS 4
enum cfa_p4_mac_sel_mode {
	CFA_P4_MAC_SEL_MODE_FIRST = 0,
	CFA_P4_MAC_SEL_MODE_LOWEST = 1,
};

struct cfa_p4_prof_key_cfg {
	uint8_t mac_sel[CFA_P4_PROF_MAX_KEYS];
#define CFA_P4_PROF_MAC_SEL_DMAC0 (1 << 0)
#define CFA_P4_PROF_MAC_SEL_T_MAC0 (1 << 1)
#define CFA_P4_PROF_MAC_SEL_OUTERMOST_MAC0 (1 << 2)
#define CFA_P4_PROF_MAC_SEL_DMAC1 (1 << 3)
#define CFA_P4_PROF_MAC_SEL_T_MAC1 (1 << 4)
#define CFA_P4_PROF_MAC_OUTERMOST_MAC1 (1 << 5)
	uint8_t pass_cnt;
	enum cfa_p4_mac_sel_mode mode;
};

/**
 * Enumeration of SRAM entry types, used for allocation of
 * fixed SRAM entities. The memory model for CFA HCAPI
 * determines if an SRAM entry type is supported.
 */
enum cfa_p4_action_sram_entry_type {
	/* NOTE: Any additions to this enum must be reflected on FW
	 * side as well.
	 */

	/** SRAM Action Record */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FULL_ACTION,

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_0_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_1_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_2_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_3_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_4_ACTION,

	/** SRAM Action Encap 8 Bytes */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_8B,
	/** SRAM Action Encap 16 Bytes */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_16B,
	/** SRAM Action Encap 64 Bytes */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_64B,

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_PORT_SRC,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_PORT_DEST,

	/** SRAM Action Modify IPv4 Source */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_SRC,
	/** SRAM Action Modify IPv4 Destination */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_DEST,

	/** SRAM Action Source Properties SMAC */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC,
	/** SRAM Action Source Properties SMAC IPv4 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV4,
	/** SRAM Action Source Properties SMAC IPv6 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV6,
	/** SRAM Action Statistics 64 Bits */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_STATS_64,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MAX
};

/**
 * SRAM Action Record structure holding either an action index or an
 * action ptr.
 */
union cfa_p4_action_sram_act_record {
	/** SRAM Action idx specifies the offset of the SRAM
	 * element within its SRAM Entry Type block. This
	 * index can be written into i.e. an L2 Context. Use
	 * this type for all SRAM Action Record types except
	 * SRAM Full Action records. Use act_ptr instead.
	 */
	uint16_t act_idx;
	/** SRAM Full Action is special in that it needs an
	 * action record pointer. This pointer can be written
	 * into i.e. a Wildcard TCAM entry.
	 */
	uint32_t act_ptr;
};

/**
 * cfa_p4_action_param parameter definition
 */
struct cfa_p4_action_param {
	/**
	 * [in] receive or transmit direction
	 */
	uint8_t dir;
	/**
	 * [in] type of the sram allocation type
	 */
	enum cfa_p4_action_sram_entry_type type;
	/**
	 * [in] action record to set. The 'type' specified lists the
	 *	record definition to use in the passed in record.
	 */
	union cfa_p4_action_sram_act_record record;
	/**
	 * [in] number of elements in act_data
	 */
	uint32_t act_size;
	/**
	 * [in] ptr to array of action data
	 */
	uint64_t *act_data;
};

/**
 * EEM Key entry sizes
 */
#define CFA_P4_EEM_KEY_MAX_SIZE 52
#define CFA_P4_EEM_KEY_RECORD_SIZE 64

/**
 * cfa_eem_entry_hdr
 */
struct cfa_p4_eem_entry_hdr {
	uint32_t pointer;
	uint32_t word1;  /*
			  * The header is made up of two words,
			  * this is the first word. This field has multiple
			  * subfields, there is no suitable single name for
			  * it so just going with word1.
			  */
#define CFA_P4_EEM_ENTRY_VALID_SHIFT 31
#define CFA_P4_EEM_ENTRY_VALID_MASK 0x80000000
#define CFA_P4_EEM_ENTRY_L1_CACHEABLE_SHIFT 30
#define CFA_P4_EEM_ENTRY_L1_CACHEABLE_MASK 0x40000000
#define CFA_P4_EEM_ENTRY_STRENGTH_SHIFT 28
#define CFA_P4_EEM_ENTRY_STRENGTH_MASK 0x30000000
#define CFA_P4_EEM_ENTRY_RESERVED_SHIFT 17
#define CFA_P4_EEM_ENTRY_RESERVED_MASK 0x0FFE0000
#define CFA_P4_EEM_ENTRY_KEY_SIZE_SHIFT 8
#define CFA_P4_EEM_ENTRY_KEY_SIZE_MASK 0x0001FF00
#define CFA_P4_EEM_ENTRY_ACT_REC_SIZE_SHIFT 3
#define CFA_P4_EEM_ENTRY_ACT_REC_SIZE_MASK 0x000000F8
#define CFA_P4_EEM_ENTRY_ACT_REC_INT_SHIFT 2
#define CFA_P4_EEM_ENTRY_ACT_REC_INT_MASK 0x00000004
#define CFA_P4_EEM_ENTRY_EXT_FLOW_CTR_SHIFT 1
#define CFA_P4_EEM_ENTRY_EXT_FLOW_CTR_MASK 0x00000002
#define CFA_P4_EEM_ENTRY_ACT_PTR_MSB_SHIFT 0
#define CFA_P4_EEM_ENTRY_ACT_PTR_MSB_MASK 0x00000001
};

/**
 *  cfa_p4_eem_key_entry
 */
struct cfa_p4_eem_64b_entry {
	/** Key is 448 bits - 56 bytes */
	uint8_t key[CFA_P4_EEM_KEY_RECORD_SIZE - sizeof(struct cfa_p4_eem_entry_hdr)];
	/** Header is 8 bytes long */
	struct cfa_p4_eem_entry_hdr hdr;
};

#endif /* _CFA_HW_P4_H_ */
