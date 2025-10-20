/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_BACKEND_H_
#define _HW_MOD_BACKEND_H_

#include <stdbool.h>
#include <string.h>

#include "ntlog.h"

#include "hw_mod_cat_v18.h"
#include "hw_mod_cat_v21.h"
#include "hw_mod_flm_v25.h"
#include "hw_mod_km_v7.h"
#include "hw_mod_qsl_v7.h"
#include "hw_mod_pdb_v9.h"
#include "hw_mod_slc_lr_v2.h"
#include "hw_mod_hsh_v5.h"
#include "hw_mod_tpe_v3.h"

#define MAX_PHYS_ADAPTERS 8

#define VER_MAJOR(ver) (((ver) >> 16) & 0xffff)
#define VER_MINOR(ver) ((ver) & 0xffff)

struct flow_api_backend_s;
struct common_func_s;

void *nthw_callocate_mod(struct common_func_s *mod, int sets, ...);
void nthw_zero_module_cache(struct common_func_s *mod);

#define ALL_ENTRIES -1000
#define ALL_BANK_ENTRIES -1001

#define INDEX_TOO_LARGE (-2)
#define INDEX_TOO_LARGE_LOG NT_LOG(INF, FILTER, "ERROR:%s: Index too large", __func__)

#define WORD_OFF_TOO_LARGE (-3)
#define WORD_OFF_TOO_LARGE_LOG NT_LOG(INF, FILTER, "ERROR:%s: Word offset too large", __func__)

#define UNSUP_FIELD (-5)
#define UNSUP_FIELD_LOG                                                         \
	NT_LOG(INF, FILTER, "ERROR:%s: Unsupported field in NIC module", __func__)

#define UNSUP_VER (-4)
#define UNSUP_VER_LOG                                                                       \
	NT_LOG(INF, FILTER, "ERROR:%s: Unsupported NIC module: %s ver %i.%i", __func__, _MOD_, \
		VER_MAJOR(_VER_), VER_MINOR(_VER_))

#define COUNT_ERROR (-4)
#define COUNT_ERROR_LOG(_RESOURCE_)                                                         \
	NT_LOG(INF, FILTER,                                                                      \
		"ERROR:%s: Insufficient resource [ %s ] : NIC module: %s ver %i.%i", __func__,  \
		#_RESOURCE_, _MOD_, VER_MAJOR(_VER_), VER_MINOR(_VER_))                          \

#define NOT_FOUND 0xffffffff

enum {
	EXTRA_INDEXES
};

#define GET(cached_val, val) ({ *(val) = *(cached_val); })

#define SET(cached_val, val) ({ *(cached_val) = *(val); })

#define GET_SET(cached_val, val)                                                                  \
	do {                                                                                      \
		uint32_t *temp_val = (val);                                                       \
		typeof(cached_val) *temp_cached_val = &(cached_val);                          \
		if (get)                                                                          \
			GET(temp_cached_val, temp_val);                                           \
		else                                                                              \
			SET(temp_cached_val, temp_val);                                           \
	} while (0)

#define GET_SIGNED(cached_val, val) ({ *(val) = (uint32_t)(*(cached_val)); })

#define SET_SIGNED(cached_val, val) ({ *(cached_val) = (int32_t)(*(val)); })

#define GET_SET_SIGNED(cached_val, val)                                                           \
	do {                                                                                      \
		uint32_t *temp_val = (val);                                                       \
		typeof(cached_val) *temp_cached_val = &(cached_val);                          \
		if (get)                                                                          \
			GET_SIGNED(temp_cached_val, temp_val);                                    \
		else                                                                              \
			SET_SIGNED(temp_cached_val, temp_val);                                    \
	} while (0)

#define FIND_EQUAL_INDEX(be_module_reg, type, idx, start, nb_elements)                            \
	do {                                                                                      \
		typeof(be_module_reg) *temp_be_module =                                       \
			(typeof(be_module_reg) *)be_module_reg;                               \
		typeof(idx) tmp_idx = (idx);                                                  \
		typeof(nb_elements) tmp_nb_elements = (nb_elements);                          \
		unsigned int start_idx = (unsigned int)(start);                                   \
		*value = NOT_FOUND;                                                               \
		for (unsigned int i = start_idx; i < tmp_nb_elements; i++) {                      \
			if ((unsigned int)(tmp_idx) == i)                                         \
				continue;                                                         \
			if (memcmp(&temp_be_module[tmp_idx], &temp_be_module[i], sizeof(type)) == \
			    0) {                                                                  \
				*value = i;                                                       \
				break;                                                            \
			}                                                                         \
		}                                                                                 \
	} while (0)

#define DO_COMPARE_INDEXS(be_module_reg, type, idx, cmp_idx)                                      \
	do {                                                                                      \
		typeof(be_module_reg) temp_be_module = (be_module_reg);                     \
		typeof(idx) tmp_idx = (idx);                                                  \
		typeof(cmp_idx) tmp_cmp_idx = (cmp_idx);                                      \
		if ((unsigned int)(tmp_idx) != (unsigned int)(tmp_cmp_idx))                     \
			if (memcmp(temp_be_module + tmp_idx, &temp_be_module[tmp_cmp_idx], \
				sizeof(type)) == 0) \
				break;                                              \
	} while (0)

static inline int is_non_zero(const void *addr, size_t n)
{
	size_t i = 0;
	const uint8_t *p = (const uint8_t *)addr;

	for (i = 0; i < n; i++)
		if (p[i] != 0)
			return 1;

	return 0;
}

/* Sideband info bit indicator */
#define SWX_INFO (1 << 6)

enum km_flm_if_select_e {
	KM_FLM_IF_FIRST = 0,
	KM_FLM_IF_SECOND = 1
};

#define FIELD_START_INDEX 100

#define COMMON_FUNC_INFO_S                                                                        \
	int ver;                                                                                  \
	void *base;                                                                               \
	unsigned int allocated_size;                                                              \
	int debug

enum frame_offs_e {
	DYN_SOF = 0,
	DYN_L2 = 1,
	DYN_FIRST_VLAN = 2,
	DYN_MPLS = 3,
	DYN_L3 = 4,
	DYN_ID_IPV4_6 = 5,
	DYN_FINAL_IP_DST = 6,
	DYN_L4 = 7,
	DYN_L4_PAYLOAD = 8,
	DYN_TUN_PAYLOAD = 9,
	DYN_TUN_L2 = 10,
	DYN_TUN_VLAN = 11,
	DYN_TUN_MPLS = 12,
	DYN_TUN_L3 = 13,
	DYN_TUN_ID_IPV4_6 = 14,
	DYN_TUN_FINAL_IP_DST = 15,
	DYN_TUN_L4 = 16,
	DYN_TUN_L4_PAYLOAD = 17,
	DYN_EOF = 18,
	DYN_L3_PAYLOAD_END = 19,
	DYN_TUN_L3_PAYLOAD_END = 20,
	SB_VNI = SWX_INFO | 1,
	SB_MAC_PORT = SWX_INFO | 2,
	SB_KCC_ID = SWX_INFO | 3
};

enum {
	QW0_SEL_EXCLUDE = 0,
	QW0_SEL_FIRST32 = 1,
	QW0_SEL_FIRST64 = 3,
	QW0_SEL_ALL128 = 4,
};

enum {
	QW4_SEL_EXCLUDE = 0,
	QW4_SEL_FIRST32 = 1,
	QW4_SEL_FIRST64 = 2,
	QW4_SEL_ALL128 = 3,
};

enum {
	DW8_SEL_EXCLUDE = 0,
	DW8_SEL_FIRST32 = 3,
};

enum {
	DW10_SEL_EXCLUDE = 0,
	DW10_SEL_FIRST32 = 2,
};

enum {
	SWX_SEL_EXCLUDE = 0,
	SWX_SEL_ALL32 = 1,
};

enum {
	PROT_OTHER = 0,
	PROT_L2_ETH2 = 1,
};

enum {
	PROT_L3_IPV4 = 1,
	PROT_L3_IPV6 = 2
};

enum {
	PROT_L4_TCP = 1,
	PROT_L4_UDP = 2,
	PROT_L4_SCTP = 3,
	PROT_L4_ICMP = 4
};

enum {
	PROT_TUN_GTPV1U = 6,
};

enum {
	PROT_TUN_L3_OTHER = 0,
	PROT_TUN_L3_IPV4 = 1,
	PROT_TUN_L3_IPV6 = 2
};

enum {
	PROT_TUN_L4_OTHER = 0,
	PROT_TUN_L4_TCP = 1,
	PROT_TUN_L4_UDP = 2,
	PROT_TUN_L4_SCTP = 3,
	PROT_TUN_L4_ICMP = 4
};

enum {
	CPY_SELECT_DSCP_IPV4 = 0,
	CPY_SELECT_DSCP_IPV6 = 1,
	CPY_SELECT_RQI_QFI = 2,
	CPY_SELECT_IPV4 = 3,
	CPY_SELECT_PORT = 4,
	CPY_SELECT_TEID = 5,
};

struct common_func_s {
	COMMON_FUNC_INFO_S;
};

struct cat_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_cat_funcs;
	uint32_t nb_flow_types;
	uint32_t nb_pm_ext;
	uint32_t nb_len;
	uint32_t kcc_size;
	uint32_t cts_num;
	uint32_t kcc_banks;
	uint32_t kcc_id_bit_size;
	uint32_t kcc_records;
	uint32_t km_if_count;
	int32_t km_if_m0;
	int32_t km_if_m1;

	union {
		struct hw_mod_cat_v18_s v18;
		struct hw_mod_cat_v21_s v21;
	};
};
enum hw_cat_e {
	/*
	 * functions initial CAT v18
	 */
	/* 00 */ HW_CAT_CFN_SET_ALL_DEFAULTS = 0,
	/* 01 */ HW_CAT_CFN_PRESET_ALL,
	/* 02 */ HW_CAT_CFN_COMPARE,
	/* 03 */ HW_CAT_CFN_FIND,
	/* 04 */ HW_CAT_CFN_COPY_FROM,
	/* 05 */ HW_CAT_COT_PRESET_ALL,
	/* 06 */ HW_CAT_COT_COMPARE,
	/* 07 */ HW_CAT_COT_FIND,
	/* 08 */ HW_CAT_COT_COPY_FROM,
	/* fields */
	/* 00 */ HW_CAT_CFN_ENABLE = FIELD_START_INDEX,
	/* 01 */ HW_CAT_CFN_INV,
	/* 02 */ HW_CAT_CFN_PTC_INV,
	/* 03 */ HW_CAT_CFN_PTC_ISL,
	/* 04 */ HW_CAT_CFN_PTC_CFP,
	/* 05 */ HW_CAT_CFN_PTC_MAC,
	/* 06 */ HW_CAT_CFN_PTC_L2,
	/* 07 */ HW_CAT_CFN_PTC_VNTAG,
	/* 08 */ HW_CAT_CFN_PTC_VLAN,
	/* 09 */ HW_CAT_CFN_PTC_MPLS,
	/* 10 */ HW_CAT_CFN_PTC_L3,
	/* 11 */ HW_CAT_CFN_PTC_FRAG,
	/* 12 */ HW_CAT_CFN_PTC_IP_PROT,
	/* 13 */ HW_CAT_CFN_PTC_L4,
	/* 14 */ HW_CAT_CFN_PTC_TUNNEL,
	/* 15 */ HW_CAT_CFN_PTC_TNL_L2,
	/* 16 */ HW_CAT_CFN_PTC_TNL_VLAN,
	/* 17 */ HW_CAT_CFN_PTC_TNL_MPLS,
	/* 18 */ HW_CAT_CFN_PTC_TNL_L3,
	/* 19 */ HW_CAT_CFN_PTC_TNL_FRAG,
	/* 20 */ HW_CAT_CFN_PTC_TNL_IP_PROT,
	/* 21 */ HW_CAT_CFN_PTC_TNL_L4,
	/* 22 */ HW_CAT_CFN_ERR_INV,
	/* 23 */ HW_CAT_CFN_ERR_CV,
	/* 24 */ HW_CAT_CFN_ERR_FCS,
	/* 25 */ HW_CAT_CFN_ERR_TRUNC,
	/* 26 */ HW_CAT_CFN_ERR_L3_CS,
	/* 27 */ HW_CAT_CFN_ERR_L4_CS,
	/* 28 */ HW_CAT_CFN_MAC_PORT,
	/* 29 */ HW_CAT_CFN_PM_CMP,
	/* 30 */ HW_CAT_CFN_PM_DCT,
	/* 31 */ HW_CAT_CFN_PM_EXT_INV,
	/* 32 */ HW_CAT_CFN_PM_CMB,
	/* 33 */ HW_CAT_CFN_PM_AND_INV,
	/* 34 */ HW_CAT_CFN_PM_OR_INV,
	/* 35 */ HW_CAT_CFN_PM_INV,
	/* 36 */ HW_CAT_CFN_LC,
	/* 37 */ HW_CAT_CFN_LC_INV,
	/* 38 */ HW_CAT_CFN_KM0_OR,
	/* 39 */ HW_CAT_CFN_KM1_OR,
	/* 40 */ HW_CAT_KCE_ENABLE_BM,
	/* 41 */ HW_CAT_KCS_CATEGORY,
	/* 42 */ HW_CAT_FTE_ENABLE_BM,
	/* 43 */ HW_CAT_CTE_ENABLE_BM,
	/* 44 */ HW_CAT_CTS_CAT_A,
	/* 45 */ HW_CAT_CTS_CAT_B,
	/* 46 */ HW_CAT_COT_COLOR,
	/* 47 */ HW_CAT_COT_KM,
	/* 48 */ HW_CAT_CCT_COLOR,
	/* 49 */ HW_CAT_CCT_KM,
	/* 50 */ HW_CAT_KCC_KEY,
	/* 51 */ HW_CAT_KCC_CATEGORY,
	/* 52 */ HW_CAT_KCC_ID,
	/* 53 */ HW_CAT_EXO_DYN,
	/* 54 */ HW_CAT_EXO_OFS,
	/* 55 */ HW_CAT_RCK_DATA,
	/* 56 */ HW_CAT_LEN_LOWER,
	/* 57 */ HW_CAT_LEN_UPPER,
	/* 58 */ HW_CAT_LEN_DYN1,
	/* 59 */ HW_CAT_LEN_DYN2,
	/* 60 */ HW_CAT_LEN_INV,
	/* 61 */ HW_CAT_CFN_ERR_TNL_L3_CS,
	/* 62 */ HW_CAT_CFN_ERR_TNL_L4_CS,
	/* 63 */ HW_CAT_CFN_ERR_TTL_EXP,
	/* 64 */ HW_CAT_CFN_ERR_TNL_TTL_EXP,
};

bool hw_mod_cat_present(struct flow_api_backend_s *be);
int hw_mod_cat_alloc(struct flow_api_backend_s *be);
void hw_mod_cat_free(struct flow_api_backend_s *be);
int hw_mod_cat_reset(struct flow_api_backend_s *be);
int hw_mod_cat_cfn_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_cat_cfn_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index, int word_off,
	uint32_t value);
/* KCE/KCS/FTE KM */
int hw_mod_cat_kce_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_kce_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_kce_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);
int hw_mod_cat_kcs_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_kcs_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_kcs_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);
int hw_mod_cat_fte_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_fte_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_fte_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);
/* KCE/KCS/FTE FLM */
int hw_mod_cat_kce_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_kce_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_kce_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);
int hw_mod_cat_kcs_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_kcs_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_kcs_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);
int hw_mod_cat_fte_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count);
int hw_mod_cat_fte_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value);
int hw_mod_cat_fte_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value);

int hw_mod_cat_cte_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_cat_cte_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value);
int hw_mod_cat_cte_get(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value);

int hw_mod_cat_cts_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_cat_cts_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value);
int hw_mod_cat_cts_get(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value);

int hw_mod_cat_cot_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_cat_cot_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value);

struct km_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_categories;
	uint32_t nb_cam_banks;
	uint32_t nb_cam_record_words;
	uint32_t nb_cam_records;
	uint32_t nb_tcam_banks;
	uint32_t nb_tcam_bank_width;
	/* not read from backend, but rather set using version */
	uint32_t nb_km_rcp_mask_a_word_size;
	/* --- || --- */
	uint32_t nb_km_rcp_mask_b_word_size;
	union {
		struct hw_mod_km_v7_s v7;
	};
};
enum hw_km_e {
	/* functions */
	HW_KM_RCP_PRESET_ALL = 0,
	HW_KM_CAM_PRESET_ALL,
	/* to sync and reset hw with cache - force write all entries in a bank */
	HW_KM_TCAM_BANK_RESET,
	/* fields */
	HW_KM_RCP_QW0_DYN = FIELD_START_INDEX,
	HW_KM_RCP_QW0_OFS,
	HW_KM_RCP_QW0_SEL_A,
	HW_KM_RCP_QW0_SEL_B,
	HW_KM_RCP_QW4_DYN,
	HW_KM_RCP_QW4_OFS,
	HW_KM_RCP_QW4_SEL_A,
	HW_KM_RCP_QW4_SEL_B,
	HW_KM_RCP_DW8_DYN,
	HW_KM_RCP_DW8_OFS,
	HW_KM_RCP_DW8_SEL_A,
	HW_KM_RCP_DW8_SEL_B,
	HW_KM_RCP_DW10_DYN,
	HW_KM_RCP_DW10_OFS,
	HW_KM_RCP_DW10_SEL_A,
	HW_KM_RCP_DW10_SEL_B,
	HW_KM_RCP_SWX_CCH,
	HW_KM_RCP_SWX_SEL_A,
	HW_KM_RCP_SWX_SEL_B,
	HW_KM_RCP_MASK_A,
	HW_KM_RCP_MASK_B,
	HW_KM_RCP_DUAL,
	HW_KM_RCP_PAIRED,
	HW_KM_RCP_EL_A,
	HW_KM_RCP_EL_B,
	HW_KM_RCP_INFO_A,
	HW_KM_RCP_INFO_B,
	HW_KM_RCP_FTM_A,
	HW_KM_RCP_FTM_B,
	HW_KM_RCP_BANK_A,
	HW_KM_RCP_BANK_B,
	HW_KM_RCP_KL_A,
	HW_KM_RCP_KL_B,
	HW_KM_RCP_KEYWAY_A,
	HW_KM_RCP_KEYWAY_B,
	HW_KM_RCP_SYNERGY_MODE,
	HW_KM_RCP_DW0_B_DYN,
	HW_KM_RCP_DW0_B_OFS,
	HW_KM_RCP_DW2_B_DYN,
	HW_KM_RCP_DW2_B_OFS,
	HW_KM_RCP_SW4_B_DYN,
	HW_KM_RCP_SW4_B_OFS,
	HW_KM_RCP_SW5_B_DYN,
	HW_KM_RCP_SW5_B_OFS,
	HW_KM_CAM_W0,
	HW_KM_CAM_W1,
	HW_KM_CAM_W2,
	HW_KM_CAM_W3,
	HW_KM_CAM_W4,
	HW_KM_CAM_W5,
	HW_KM_CAM_FT0,
	HW_KM_CAM_FT1,
	HW_KM_CAM_FT2,
	HW_KM_CAM_FT3,
	HW_KM_CAM_FT4,
	HW_KM_CAM_FT5,
	HW_KM_TCAM_T,
	HW_KM_TCI_COLOR,
	HW_KM_TCI_FT,
	HW_KM_TCQ_BANK_MASK,
	HW_KM_TCQ_QUAL
};
bool hw_mod_km_present(struct flow_api_backend_s *be);
int hw_mod_km_alloc(struct flow_api_backend_s *be);
void hw_mod_km_free(struct flow_api_backend_s *be);
int hw_mod_km_reset(struct flow_api_backend_s *be);
int hw_mod_km_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_km_rcp_set(struct flow_api_backend_s *be, enum hw_km_e field, int index, int word_off,
	uint32_t value);
int hw_mod_km_rcp_get(struct flow_api_backend_s *be, enum hw_km_e field, int index, int word_off,
	uint32_t *value);
int hw_mod_km_cam_flush(struct flow_api_backend_s *be, int start_bank, int start_record,
	int count);
int hw_mod_km_cam_set(struct flow_api_backend_s *be, enum hw_km_e field, int bank, int record,
	uint32_t value);

int hw_mod_km_tcam_flush(struct flow_api_backend_s *be, int start_bank, int count);
int hw_mod_km_tcam_set(struct flow_api_backend_s *be, enum hw_km_e field, int bank, int byte,
	int byte_val, uint32_t *value_set);
int hw_mod_km_tcam_get(struct flow_api_backend_s *be, enum hw_km_e field, int bank, int byte,
	int byte_val, uint32_t *value_set);
int hw_mod_km_tci_flush(struct flow_api_backend_s *be, int start_bank, int start_record,
	int count);
int hw_mod_km_tci_set(struct flow_api_backend_s *be, enum hw_km_e field, int bank, int record,
	uint32_t value);
struct flm_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_categories;
	uint32_t nb_size_mb;
	uint32_t nb_entry_size;
	uint32_t nb_variant;
	uint32_t nb_prios;
	uint32_t nb_pst_profiles;
	uint32_t nb_scrub_profiles;
	uint32_t nb_rpp_clock_in_ps;
	uint32_t nb_load_aps_max;
	union {
		struct hw_mod_flm_v25_s v25;
	};
};
enum hw_flm_e {
	/* functions */
	HW_FLM_CONTROL_PRESET_ALL = 0,
	HW_FLM_RCP_PRESET_ALL,
	HW_FLM_FLOW_LRN_DATA,
	HW_FLM_FLOW_INF_STA_DATA,
	/* Control fields */
	HW_FLM_CONTROL_ENABLE = FIELD_START_INDEX,
	HW_FLM_CONTROL_INIT,
	HW_FLM_CONTROL_LDS,
	HW_FLM_CONTROL_LFS,
	HW_FLM_CONTROL_LIS,
	HW_FLM_CONTROL_UDS,
	HW_FLM_CONTROL_UIS,
	HW_FLM_CONTROL_RDS,
	HW_FLM_CONTROL_RIS,
	HW_FLM_CONTROL_PDS,
	HW_FLM_CONTROL_PIS,
	HW_FLM_CONTROL_CRCWR,
	HW_FLM_CONTROL_CRCRD,
	HW_FLM_CONTROL_RBL,
	HW_FLM_CONTROL_EAB,
	HW_FLM_CONTROL_SPLIT_SDRAM_USAGE,
	HW_FLM_STATUS_CALIB_SUCCESS,
	HW_FLM_STATUS_CALIB_FAIL,
	HW_FLM_STATUS_INITDONE,
	HW_FLM_STATUS_IDLE,
	HW_FLM_STATUS_CRITICAL,
	HW_FLM_STATUS_PANIC,
	HW_FLM_STATUS_CRCERR,
	HW_FLM_STATUS_EFT_BP,
	HW_FLM_STATUS_CACHE_BUFFER_CRITICAL,
	HW_FLM_LOAD_BIN,
	HW_FLM_LOAD_LPS,
	HW_FLM_LOAD_APS,
	HW_FLM_PRIO_LIMIT0,
	HW_FLM_PRIO_FT0,
	HW_FLM_PRIO_LIMIT1,
	HW_FLM_PRIO_FT1,
	HW_FLM_PRIO_LIMIT2,
	HW_FLM_PRIO_FT2,
	HW_FLM_PRIO_LIMIT3,
	HW_FLM_PRIO_FT3,
	HW_FLM_PST_PRESET_ALL,
	HW_FLM_PST_BP,
	HW_FLM_PST_PP,
	HW_FLM_PST_TP,
	HW_FLM_RCP_LOOKUP,
	HW_FLM_RCP_QW0_DYN,
	HW_FLM_RCP_QW0_OFS,
	HW_FLM_RCP_QW0_SEL,
	HW_FLM_RCP_QW4_DYN,
	HW_FLM_RCP_QW4_OFS,
	HW_FLM_RCP_SW8_DYN,
	HW_FLM_RCP_SW8_OFS,
	HW_FLM_RCP_SW8_SEL,
	HW_FLM_RCP_SW9_DYN,
	HW_FLM_RCP_SW9_OFS,
	HW_FLM_RCP_MASK,
	HW_FLM_RCP_KID,
	HW_FLM_RCP_OPN,
	HW_FLM_RCP_IPN,
	HW_FLM_RCP_BYT_DYN,
	HW_FLM_RCP_BYT_OFS,
	HW_FLM_RCP_TXPLM,
	HW_FLM_RCP_AUTO_IPV4_MASK,
	HW_FLM_BUF_CTRL_LRN_FREE,
	HW_FLM_BUF_CTRL_INF_AVAIL,
	HW_FLM_BUF_CTRL_STA_AVAIL,
	HW_FLM_STAT_LRN_DONE,
	HW_FLM_STAT_LRN_IGNORE,
	HW_FLM_STAT_LRN_FAIL,
	HW_FLM_STAT_UNL_DONE,
	HW_FLM_STAT_UNL_IGNORE,
	HW_FLM_STAT_REL_DONE,
	HW_FLM_STAT_REL_IGNORE,
	HW_FLM_STAT_PRB_DONE,
	HW_FLM_STAT_PRB_IGNORE,
	HW_FLM_STAT_AUL_DONE,
	HW_FLM_STAT_AUL_IGNORE,
	HW_FLM_STAT_AUL_FAIL,
	HW_FLM_STAT_TUL_DONE,
	HW_FLM_STAT_FLOWS,
	HW_FLM_STAT_STA_DONE,	/* module ver 0.20 */
	HW_FLM_STAT_INF_DONE,	/* module ver 0.20 */
	HW_FLM_STAT_INF_SKIP,	/* module ver 0.20 */
	HW_FLM_STAT_PCK_HIT,	/* module ver 0.20 */
	HW_FLM_STAT_PCK_MISS,	/* module ver 0.20 */
	HW_FLM_STAT_PCK_UNH,	/* module ver 0.20 */
	HW_FLM_STAT_PCK_DIS,	/* module ver 0.20 */
	HW_FLM_STAT_CSH_HIT,	/* module ver 0.20 */
	HW_FLM_STAT_CSH_MISS,	/* module ver 0.20 */
	HW_FLM_STAT_CSH_UNH,	/* module ver 0.20 */
	HW_FLM_STAT_CUC_START,	/* module ver 0.20 */
	HW_FLM_STAT_CUC_MOVE,	/* module ver 0.20 */
	HW_FLM_SCAN_I,	/* module ver 0.22 */
	HW_FLM_SCRUB_PRESET_ALL,
	HW_FLM_SCRUB_T,	/* module ver 0.22 */
	HW_FLM_SCRUB_R,	/* module ver 0.24 */
	HW_FLM_SCRUB_DEL,	/* module ver 0.24 */
	HW_FLM_SCRUB_INF,	/* module ver 0.24 */
};

bool hw_mod_flm_present(struct flow_api_backend_s *be);
int hw_mod_flm_alloc(struct flow_api_backend_s *be);
void hw_mod_flm_free(struct flow_api_backend_s *be);
int hw_mod_flm_reset(struct flow_api_backend_s *be);

int hw_mod_flm_control_flush(struct flow_api_backend_s *be);
int hw_mod_flm_control_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value);

int hw_mod_flm_status_update(struct flow_api_backend_s *be);
int hw_mod_flm_status_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value);

int hw_mod_flm_scan_flush(struct flow_api_backend_s *be);
int hw_mod_flm_scan_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value);

int hw_mod_flm_load_bin_flush(struct flow_api_backend_s *be);
int hw_mod_flm_load_bin_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value);

int hw_mod_flm_prio_flush(struct flow_api_backend_s *be);
int hw_mod_flm_prio_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value);

int hw_mod_flm_pst_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_flm_pst_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value);

int hw_mod_flm_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_flm_rcp_set_mask(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t *value);
int hw_mod_flm_rcp_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value);

int hw_mod_flm_buf_ctrl_update(struct flow_api_backend_s *be);
int hw_mod_flm_buf_ctrl_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value);

int hw_mod_flm_stat_update(struct flow_api_backend_s *be);
int hw_mod_flm_stat_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value);

int hw_mod_flm_lrn_data_set_flush(struct flow_api_backend_s *be, enum hw_flm_e field,
	const uint32_t *value, uint32_t records,
	uint32_t *handled_records, uint32_t *inf_word_cnt,
	uint32_t *sta_word_cnt);
int hw_mod_flm_inf_sta_data_update_get(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *inf_value, uint32_t inf_size,
	uint32_t *inf_word_cnt, uint32_t *sta_value,
	uint32_t sta_size, uint32_t *sta_word_cnt);

uint32_t hw_mod_flm_scrub_timeout_decode(uint32_t t_enc);
uint32_t hw_mod_flm_scrub_timeout_encode(uint32_t t);
int hw_mod_flm_scrub_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_flm_scrub_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value);

struct hsh_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_rcp;/* number of HSH recipes supported by FPGA */
	/* indication if Toeplitz is supported by FPGA, i.e. 0 - unsupported, 1 - supported */
	uint32_t toeplitz;
	union {
		struct hw_mod_hsh_v5_s v5;
	};
};
enum hw_hsh_e {
	/* functions */
	HW_HSH_RCP_PRESET_ALL = 0,
	HW_HSH_RCP_COMPARE,
	HW_HSH_RCP_FIND,
	/* fields */
	HW_HSH_RCP_LOAD_DIST_TYPE = FIELD_START_INDEX,
	HW_HSH_RCP_MAC_PORT_MASK,
	HW_HSH_RCP_SORT,
	HW_HSH_RCP_QW0_PE,
	HW_HSH_RCP_QW0_OFS,
	HW_HSH_RCP_QW4_PE,
	HW_HSH_RCP_QW4_OFS,
	HW_HSH_RCP_W8_PE,
	HW_HSH_RCP_W8_OFS,
	HW_HSH_RCP_W8_SORT,
	HW_HSH_RCP_W9_PE,
	HW_HSH_RCP_W9_OFS,
	HW_HSH_RCP_W9_SORT,
	HW_HSH_RCP_W9_P,
	HW_HSH_RCP_P_MASK,
	HW_HSH_RCP_WORD_MASK,
	HW_HSH_RCP_SEED,
	HW_HSH_RCP_TNL_P,
	HW_HSH_RCP_HSH_VALID,
	HW_HSH_RCP_HSH_TYPE,
	HW_HSH_RCP_TOEPLITZ,
	HW_HSH_RCP_K,
	HW_HSH_RCP_AUTO_IPV4_MASK
};
bool hw_mod_hsh_present(struct flow_api_backend_s *be);
int hw_mod_hsh_alloc(struct flow_api_backend_s *be);
void hw_mod_hsh_free(struct flow_api_backend_s *be);
int hw_mod_hsh_reset(struct flow_api_backend_s *be);
int hw_mod_hsh_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_hsh_rcp_set(struct flow_api_backend_s *be, enum hw_hsh_e field, uint32_t index,
	uint32_t word_off, uint32_t value);

struct qsl_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_rcp_categories;
	uint32_t nb_qst_entries;
	union {
		struct hw_mod_qsl_v7_s v7;
	};
};
enum hw_qsl_e {
	/* functions */
	HW_QSL_RCP_PRESET_ALL = 0,
	HW_QSL_RCP_COMPARE,
	HW_QSL_RCP_FIND,
	HW_QSL_QST_PRESET_ALL,
	/* fields */
	HW_QSL_RCP_DISCARD = FIELD_START_INDEX,
	HW_QSL_RCP_DROP,
	HW_QSL_RCP_TBL_LO,
	HW_QSL_RCP_TBL_HI,
	HW_QSL_RCP_TBL_IDX,
	HW_QSL_RCP_TBL_MSK,
	HW_QSL_RCP_LR,
	HW_QSL_RCP_TSA,
	HW_QSL_RCP_VLI,
	HW_QSL_QST_QUEUE,
	HW_QSL_QST_EN,	/* Alias: HW_QSL_QST_QEN */
	HW_QSL_QST_TX_PORT,
	HW_QSL_QST_LRE,
	HW_QSL_QST_TCI,
	HW_QSL_QST_VEN,
	HW_QSL_QEN_EN,
	HW_QSL_UNMQ_DEST_QUEUE,
	HW_QSL_UNMQ_EN,
};
bool hw_mod_qsl_present(struct flow_api_backend_s *be);
int hw_mod_qsl_alloc(struct flow_api_backend_s *be);
void hw_mod_qsl_free(struct flow_api_backend_s *be);
int hw_mod_qsl_reset(struct flow_api_backend_s *be);
int hw_mod_qsl_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_qsl_rcp_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value);
int hw_mod_qsl_qst_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_qsl_qst_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value);
int hw_mod_qsl_qen_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_qsl_qen_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value);
int hw_mod_qsl_qen_get(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value);
int hw_mod_qsl_unmq_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_qsl_unmq_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value);

struct slc_lr_func_s {
	COMMON_FUNC_INFO_S;
	union {
		struct hw_mod_slc_lr_v2_s v2;
	};
};
enum hw_slc_lr_e {
	/* functions */
	HW_SLC_LR_RCP_PRESET_ALL = 0,
	HW_SLC_LR_RCP_COMPARE,
	HW_SLC_LR_RCP_FIND,
	/* fields */
	HW_SLC_LR_RCP_HEAD_SLC_EN = FIELD_START_INDEX,
	HW_SLC_LR_RCP_HEAD_DYN,
	HW_SLC_LR_RCP_HEAD_OFS,
	HW_SLC_LR_RCP_TAIL_SLC_EN,
	HW_SLC_LR_RCP_TAIL_DYN,
	HW_SLC_LR_RCP_TAIL_OFS,
	HW_SLC_LR_RCP_PCAP
};
bool hw_mod_slc_lr_present(struct flow_api_backend_s *be);
int hw_mod_slc_lr_alloc(struct flow_api_backend_s *be);
void hw_mod_slc_lr_free(struct flow_api_backend_s *be);
int hw_mod_slc_lr_reset(struct flow_api_backend_s *be);
int hw_mod_slc_lr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_slc_lr_rcp_set(struct flow_api_backend_s *be, enum hw_slc_lr_e field, uint32_t index,
	uint32_t value);

struct pdb_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_pdb_rcp_categories;

	union {
		struct hw_mod_pdb_v9_s v9;
	};
};
enum hw_pdb_e {
	/* functions */
	HW_PDB_RCP_PRESET_ALL = 0,
	HW_PDB_RCP_COMPARE,
	HW_PDB_RCP_FIND,
	/* fields */
	HW_PDB_RCP_DESCRIPTOR = FIELD_START_INDEX,
	HW_PDB_RCP_DESC_LEN,
	HW_PDB_RCP_TX_PORT,
	HW_PDB_RCP_TX_IGNORE,
	HW_PDB_RCP_TX_NOW,
	HW_PDB_RCP_CRC_OVERWRITE,
	HW_PDB_RCP_ALIGN,
	HW_PDB_RCP_OFS0_DYN,
	HW_PDB_RCP_OFS0_REL,
	HW_PDB_RCP_OFS1_DYN,
	HW_PDB_RCP_OFS1_REL,
	HW_PDB_RCP_OFS2_DYN,
	HW_PDB_RCP_OFS2_REL,
	HW_PDB_RCP_IP_PROT_TNL,
	HW_PDB_RCP_PPC_HSH,
	HW_PDB_RCP_DUPLICATE_EN,
	HW_PDB_RCP_DUPLICATE_BIT,
	HW_PDB_RCP_PCAP_KEEP_FCS,
	HW_PDB_CONFIG_TS_FORMAT,
	HW_PDB_CONFIG_PORT_OFS,
};
bool hw_mod_pdb_present(struct flow_api_backend_s *be);
int hw_mod_pdb_alloc(struct flow_api_backend_s *be);
void hw_mod_pdb_free(struct flow_api_backend_s *be);
int hw_mod_pdb_reset(struct flow_api_backend_s *be);
int hw_mod_pdb_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_pdb_rcp_set(struct flow_api_backend_s *be, enum hw_pdb_e field, uint32_t index,
	uint32_t value);

struct tpe_func_s {
	COMMON_FUNC_INFO_S;
	uint32_t nb_rcp_categories;
	uint32_t nb_ifr_categories;
	uint32_t nb_cpy_writers;
	uint32_t nb_rpl_depth;
	uint32_t nb_rpl_ext_categories;
	union {
		struct hw_mod_tpe_v3_s v3;
	};
};
enum hw_tpe_e {
	/* functions */
	HW_TPE_PRESET_ALL = 0,
	HW_TPE_FIND,
	HW_TPE_COMPARE,
	/* Control fields */
	HW_TPE_RPP_RCP_EXP = FIELD_START_INDEX,
	HW_TPE_IFR_RCP_IPV4_EN,
	HW_TPE_IFR_RCP_IPV4_DF_DROP,
	HW_TPE_IFR_RCP_IPV6_EN,
	HW_TPE_IFR_RCP_IPV6_DROP,
	HW_TPE_IFR_RCP_MTU,
	HW_TPE_IFR_COUNTERS_DROP,
	HW_TPE_INS_RCP_DYN,
	HW_TPE_INS_RCP_OFS,
	HW_TPE_INS_RCP_LEN,
	HW_TPE_RPL_RCP_DYN,
	HW_TPE_RPL_RCP_OFS,
	HW_TPE_RPL_RCP_LEN,
	HW_TPE_RPL_RCP_RPL_PTR,
	HW_TPE_RPL_RCP_EXT_PRIO,
	HW_TPE_RPL_RCP_ETH_TYPE_WR,
	HW_TPE_RPL_EXT_RPL_PTR,
	HW_TPE_RPL_EXT_META_RPL_LEN,	/* SW only */
	HW_TPE_RPL_RPL_VALUE,
	HW_TPE_CPY_RCP_READER_SELECT,
	HW_TPE_CPY_RCP_DYN,
	HW_TPE_CPY_RCP_OFS,
	HW_TPE_CPY_RCP_LEN,
	HW_TPE_HFU_RCP_LEN_A_WR,
	HW_TPE_HFU_RCP_LEN_A_OUTER_L4_LEN,
	HW_TPE_HFU_RCP_LEN_A_POS_DYN,
	HW_TPE_HFU_RCP_LEN_A_POS_OFS,
	HW_TPE_HFU_RCP_LEN_A_ADD_DYN,
	HW_TPE_HFU_RCP_LEN_A_ADD_OFS,
	HW_TPE_HFU_RCP_LEN_A_SUB_DYN,
	HW_TPE_HFU_RCP_LEN_B_WR,
	HW_TPE_HFU_RCP_LEN_B_POS_DYN,
	HW_TPE_HFU_RCP_LEN_B_POS_OFS,
	HW_TPE_HFU_RCP_LEN_B_ADD_DYN,
	HW_TPE_HFU_RCP_LEN_B_ADD_OFS,
	HW_TPE_HFU_RCP_LEN_B_SUB_DYN,
	HW_TPE_HFU_RCP_LEN_C_WR,
	HW_TPE_HFU_RCP_LEN_C_POS_DYN,
	HW_TPE_HFU_RCP_LEN_C_POS_OFS,
	HW_TPE_HFU_RCP_LEN_C_ADD_DYN,
	HW_TPE_HFU_RCP_LEN_C_ADD_OFS,
	HW_TPE_HFU_RCP_LEN_C_SUB_DYN,
	HW_TPE_HFU_RCP_TTL_WR,
	HW_TPE_HFU_RCP_TTL_POS_DYN,
	HW_TPE_HFU_RCP_TTL_POS_OFS,
	HW_TPE_CSU_RCP_OUTER_L3_CMD,
	HW_TPE_CSU_RCP_OUTER_L4_CMD,
	HW_TPE_CSU_RCP_INNER_L3_CMD,
	HW_TPE_CSU_RCP_INNER_L4_CMD,
};
bool hw_mod_tpe_present(struct flow_api_backend_s *be);
int hw_mod_tpe_alloc(struct flow_api_backend_s *be);
void hw_mod_tpe_free(struct flow_api_backend_s *be);
int hw_mod_tpe_reset(struct flow_api_backend_s *be);

int hw_mod_tpe_rpp_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_rpp_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_rpp_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_rpp_ifr_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_ifr_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_ifr_counters_update(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_ifr_counters_get(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t *value);

int hw_mod_tpe_ins_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_ins_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_rpl_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_rpl_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_rpl_ext_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_rpl_ext_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_rpl_rpl_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_rpl_rpl_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t *value);

int hw_mod_tpe_cpy_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_cpy_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_hfu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_hfu_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

int hw_mod_tpe_csu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count);
int hw_mod_tpe_csu_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value);

enum debug_mode_e {
	FLOW_BACKEND_DEBUG_MODE_NONE = 0x0000,
	FLOW_BACKEND_DEBUG_MODE_WRITE = 0x0001
};

struct flow_api_backend_ops {
	int version;
	int (*set_debug_mode)(void *dev, enum debug_mode_e mode);
	int (*get_nb_phy_port)(void *dev);
	int (*get_nb_rx_port)(void *dev);
	int (*get_ltx_avail)(void *dev);
	int (*get_nb_cat_funcs)(void *dev);
	int (*get_nb_categories)(void *dev);
	int (*get_nb_cat_km_if_cnt)(void *dev);
	int (*get_nb_cat_km_if_m0)(void *dev);
	int (*get_nb_cat_km_if_m1)(void *dev);

	int (*get_nb_queues)(void *dev);
	int (*get_nb_km_flow_types)(void *dev);
	int (*get_nb_pm_ext)(void *dev);
	int (*get_nb_len)(void *dev);
	int (*get_kcc_size)(void *dev);
	int (*get_kcc_banks)(void *dev);
	int (*get_nb_km_categories)(void *dev);
	int (*get_nb_km_cam_banks)(void *dev);
	int (*get_nb_km_cam_record_words)(void *dev);
	int (*get_nb_km_cam_records)(void *dev);
	int (*get_nb_km_tcam_banks)(void *dev);
	int (*get_nb_km_tcam_bank_width)(void *dev);
	int (*get_nb_flm_categories)(void *dev);
	int (*get_nb_flm_size_mb)(void *dev);
	int (*get_nb_flm_entry_size)(void *dev);
	int (*get_nb_flm_variant)(void *dev);
	int (*get_nb_flm_prios)(void *dev);
	int (*get_nb_flm_pst_profiles)(void *dev);
	int (*get_nb_flm_scrub_profiles)(void *dev);
	int (*get_nb_flm_load_aps_max)(void *dev);
	int (*get_nb_qsl_categories)(void *dev);
	int (*get_nb_qsl_qst_entries)(void *dev);
	int (*get_nb_pdb_categories)(void *dev);
	int (*get_nb_roa_categories)(void *dev);
	int (*get_nb_tpe_categories)(void *dev);
	int (*get_nb_tx_cpy_writers)(void *dev);
	int (*get_nb_tx_cpy_mask_mem)(void *dev);
	int (*get_nb_tx_rpl_depth)(void *dev);
	int (*get_nb_tx_rpl_ext_categories)(void *dev);
	int (*get_nb_tpe_ifr_categories)(void *dev);
	int (*get_nb_rpp_per_ps)(void *dev);
	int (*get_nb_hsh_categories)(void *dev);
	int (*get_nb_hsh_toeplitz)(void *dev);

	int (*alloc_rx_queue)(void *dev, int queue_id);
	int (*free_rx_queue)(void *dev, int hw_queue);

	/* CAT */
	bool (*get_cat_present)(void *dev);
	uint32_t (*get_cat_version)(void *dev);
	int (*cat_cfn_flush)(void *dev, const struct cat_func_s *cat, int cat_func, int cnt);
	int (*cat_kce_flush)(void *dev, const struct cat_func_s *cat, int km_if_idx, int index,
		int cnt);
	int (*cat_kcs_flush)(void *dev, const struct cat_func_s *cat, int km_if_idx, int cat_func,
		int cnt);
	int (*cat_fte_flush)(void *dev, const struct cat_func_s *cat, int km_if_idx, int index,
		int cnt);
	int (*cat_cte_flush)(void *dev, const struct cat_func_s *cat, int cat_func, int cnt);
	int (*cat_cts_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);
	int (*cat_cot_flush)(void *dev, const struct cat_func_s *cat, int cat_func, int cnt);
	int (*cat_cct_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);
	int (*cat_exo_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);
	int (*cat_rck_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);
	int (*cat_len_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);
	int (*cat_kcc_flush)(void *dev, const struct cat_func_s *cat, int index, int cnt);

	/* KM */
	bool (*get_km_present)(void *dev);
	uint32_t (*get_km_version)(void *dev);
	int (*km_rcp_flush)(void *dev, const struct km_func_s *km, int category, int cnt);
	int (*km_cam_flush)(void *dev, const struct km_func_s *km, int bank, int record, int cnt);
	int (*km_tcam_flush)(void *dev, const struct km_func_s *km, int bank, int byte, int value,
		int cnt);
	int (*km_tci_flush)(void *dev, const struct km_func_s *km, int bank, int record, int cnt);
	int (*km_tcq_flush)(void *dev, const struct km_func_s *km, int bank, int record, int cnt);

	/* FLM */
	bool (*get_flm_present)(void *dev);
	uint32_t (*get_flm_version)(void *dev);
	int (*flm_control_flush)(void *dev, const struct flm_func_s *flm);
	int (*flm_status_flush)(void *dev, const struct flm_func_s *flm);
	int (*flm_status_update)(void *dev, const struct flm_func_s *flm);
	int (*flm_scan_flush)(void *dev, const struct flm_func_s *flm);
	int (*flm_load_bin_flush)(void *dev, const struct flm_func_s *flm);
	int (*flm_prio_flush)(void *dev, const struct flm_func_s *flm);
	int (*flm_pst_flush)(void *dev, const struct flm_func_s *flm, int index, int cnt);
	int (*flm_rcp_flush)(void *dev, const struct flm_func_s *flm, int index, int cnt);
	int (*flm_scrub_flush)(void *dev, const struct flm_func_s *flm, int index, int cnt);
	int (*flm_buf_ctrl_update)(void *dev, const struct flm_func_s *flm);
	int (*flm_stat_update)(void *dev, const struct flm_func_s *flm);
	int (*flm_lrn_data_flush)(void *be_dev, const struct flm_func_s *flm,
		const uint32_t *lrn_data, uint32_t records,
		uint32_t *handled_records, uint32_t words_per_record,
		uint32_t *inf_word_cnt, uint32_t *sta_word_cnt);
	int (*flm_inf_sta_data_update)(void *be_dev, const struct flm_func_s *flm,
		uint32_t *inf_data, uint32_t inf_size,
		uint32_t *inf_word_cnt, uint32_t *sta_data,
		uint32_t sta_size, uint32_t *sta_word_cnt);

	/* HSH */
	bool (*get_hsh_present)(void *dev);
	uint32_t (*get_hsh_version)(void *dev);
	int (*hsh_rcp_flush)(void *dev, const struct hsh_func_s *hsh, int category, int cnt);

	/* QSL */
	bool (*get_qsl_present)(void *dev);
	uint32_t (*get_qsl_version)(void *dev);
	int (*qsl_rcp_flush)(void *dev, const struct qsl_func_s *qsl, int category, int cnt);
	int (*qsl_qst_flush)(void *dev, const struct qsl_func_s *qsl, int entry, int cnt);
	int (*qsl_qen_flush)(void *dev, const struct qsl_func_s *qsl, int entry, int cnt);
	int (*qsl_unmq_flush)(void *dev, const struct qsl_func_s *qsl, int entry, int cnt);

	/* SLC LR */
	bool (*get_slc_lr_present)(void *dev);
	uint32_t (*get_slc_lr_version)(void *dev);
	int (*slc_lr_rcp_flush)(void *dev, const struct slc_lr_func_s *slc_lr, int category,
		int cnt);

	/* PDB */
	bool (*get_pdb_present)(void *dev);
	uint32_t (*get_pdb_version)(void *dev);
	int (*pdb_rcp_flush)(void *dev, const struct pdb_func_s *pdb, int category, int cnt);
	int (*pdb_config_flush)(void *dev, const struct pdb_func_s *pdb);

	/* TPE */
	bool (*get_tpe_present)(void *dev);
	uint32_t (*get_tpe_version)(void *dev);
	int (*tpe_rpp_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_rpp_ifr_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_ifr_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_ifr_counters_update)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_ins_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_rpl_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_rpl_ext_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_rpl_rpl_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_cpy_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_hfu_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
	int (*tpe_csu_rcp_flush)(void *dev, const struct tpe_func_s *tpe, int index, int cnt);
};

struct flow_api_backend_s {
	void *be_dev;
	const struct flow_api_backend_ops *iface;

	/* flow filter FPGA modules */
	struct cat_func_s cat;
	struct km_func_s km;
	struct flm_func_s flm;
	struct hsh_func_s hsh;
	struct qsl_func_s qsl;
	struct slc_lr_func_s slc_lr;
	struct pdb_func_s pdb;
	struct tpe_func_s tpe;

	/* NIC attributes */
	unsigned int num_phy_ports;
	unsigned int num_rx_ports;

	/* flow filter resource capacities */
	unsigned int max_categories;
	unsigned int max_queues;
};

int nthw_flow_api_backend_init(struct flow_api_backend_s *dev,
	const struct flow_api_backend_ops *iface, void *be_dev);
int nthw_flow_api_backend_done(struct flow_api_backend_s *dev);

#endif  /* _HW_MOD_BACKEND_H_ */
