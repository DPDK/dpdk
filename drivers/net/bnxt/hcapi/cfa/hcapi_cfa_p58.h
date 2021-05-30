/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_P58_H_
#define _HCAPI_CFA_P58_H_

/**
 * EEM Key entry sizes
 */
#define CFA_P58_EEM_KEY_MAX_SIZE 80
#define CFA_P58_EEM_KEY_RECORD_SIZE 80

#define CFA_P58_EM_FKB_NUM_WORDS 4
#define CFA_P58_EM_FKB_NUM_ENTRIES 64
#define CFA_P58_WC_TCAM_FKB_NUM_WORDS 4
#define CFA_P58_WC_TCAM_FKB_NUM_ENTRIES 64

/** CFA phase 5.8 fix formatted table(layout) ID definition
 *
 */
enum cfa_p58_tbl_id {
	CFA_P58_TBL_ILT = 0,
	CFA_P58_TBL_L2CTXT_TCAM,
	CFA_P58_TBL_L2CTXT_REMAP,
	CFA_P58_TBL_PROF_TCAM,
	CFA_P58_TBL_PROF_TCAM_REMAP,
	CFA_P58_TBL_WC_TCAM,
	CFA_P58_TBL_WC_TCAM_REC,
	CFA_P58_TBL_VEB_TCAM,
	CFA_P58_TBL_SP_TCAM,
	/** Default Profile TCAM/Lookup Action Record Pointer Table */
	CFA_P58_TBL_PROF_PARIF_DFLT_ACT_REC_PTR,
	/** Error Profile TCAM Miss Action Record Pointer Table */
	CFA_P58_TBL_PROF_PARIF_ERR_ACT_REC_PTR,
	/** VNIC/SVIF Properties Table */
	CFA_P58_TBL_VSPT,
	CFA_P58_TBL_MAX
};

#define CFA_P58_PROF_MAX_KEYS 4
enum cfa_p58_mac_sel_mode {
	CFA_P58_MAC_SEL_MODE_FIRST = 0,
	CFA_P58_MAC_SEL_MODE_LOWEST = 1,
};

struct cfa_p58_prof_key_cfg {
	uint8_t mac_sel[CFA_P58_PROF_MAX_KEYS];
#define CFA_P58_PROF_MAC_SEL_DMAC0 (1 << 0)
#define CFA_P58_PROF_MAC_SEL_T_MAC0 (1 << 1)
#define CFA_P58_PROF_MAC_SEL_OUTERMOST_MAC0 (1 << 2)
#define CFA_P58_PROF_MAC_SEL_DMAC1 (1 << 3)
#define CFA_P58_PROF_MAC_SEL_T_MAC1 (1 << 4)
#define CFA_P58_PROF_MAC_OUTERMOST_MAC1 (1 << 5)
	uint8_t vlan_sel[CFA_P58_PROF_MAX_KEYS];
#define CFA_P58_PROFILER_VLAN_SEL_INNER_HDR 0
#define CFA_P58_PROFILER_VLAN_SEL_TUNNEL_HDR 1
#define CFA_P58_PROFILER_VLAN_SEL_OUTERMOST_HDR 2
	uint8_t pass_cnt;
	enum cfa_p58_mac_sel_mode mode;
};

/**
 * Enumeration of SRAM entry types, used for allocation of
 * fixed SRAM entities. The memory model for CFA HCAPI
 * determines if an SRAM entry type is supported.
 */
enum cfa_p58_action_sram_entry_type {
	/* NOTE: Any additions to this enum must be reflected on FW
	 * side as well.
	 */

	/** SRAM Action Record */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_ACT,
	/** SRAM Action Encap 8 Bytes */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_ENCAP_8B,
	/** SRAM Action Encap 16 Bytes */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_ENCAP_16B,
	/** SRAM Action Encap 64 Bytes */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_ENCAP_64B,
	/** SRAM Action Modify IPv4 Source */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_SRC,
	/** SRAM Action Modify IPv4 Destination */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_DEST,
	/** SRAM Action Source Properties SMAC */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_SP_SMAC,
	/** SRAM Action Source Properties SMAC IPv4 */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV4,
	/** SRAM Action Source Properties SMAC IPv6 */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV6,
	/** SRAM Action Statistics 64 Bits */
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_STATS_64,
	CFA_P58_ACTION_SRAM_ENTRY_TYPE_MAX
};

/**
 * SRAM Action Record structure holding either an action index or an
 * action ptr.
 */
union cfa_p58_action_sram_act_record {
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
 * cfa_p58_action_param parameter definition
 */
struct cfa_p58_action_param {
	/**
	 * [in] receive or transmit direction
	 */
	uint8_t dir;
	/**
	 * [in] type of the sram allocation type
	 */
	enum cfa_p58_action_sram_entry_type type;
	/**
	 * [in] action record to set. The 'type' specified lists the
	 *	record definition to use in the passed in record.
	 */
	union cfa_p58_action_sram_act_record record;
	/**
	 * [in] number of elements in act_data
	 */
	uint32_t act_size;
	/**
	 * [in] ptr to array of action data
	 */
	uint64_t *act_data;
};
#endif /* _CFA_HW_P58_H_ */
