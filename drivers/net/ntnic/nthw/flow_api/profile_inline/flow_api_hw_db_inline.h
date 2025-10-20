/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_HW_DB_INLINE_H_
#define _FLOW_API_HW_DB_INLINE_H_

#include <stdint.h>

#include "flow_api.h"

#define HW_DB_INLINE_MAX_QST_PER_QSL 128
#define HW_DB_INLINE_MAX_ENCAP_SIZE 128

#define HW_DB_IDX                                                                                 \
	union {                                                                                   \
		struct {                                                                          \
			uint32_t id1 : 8;                                                         \
			uint32_t id2 : 8;                                                         \
			uint32_t id3 : 8;                                                         \
			uint32_t type : 7;                                                        \
			uint32_t error : 1;                                                       \
		};                                                                                \
		struct {                                                                          \
			uint32_t ids : 24;                                                        \
		};                                                                                \
		uint32_t raw;                                                                     \
	}

/* Strongly typed int types */
struct hw_db_idx {
	HW_DB_IDX;
};

struct hw_db_match_set_idx {
	HW_DB_IDX;
};

struct hw_db_action_set_idx {
	HW_DB_IDX;
};

struct hw_db_cot_idx {
	HW_DB_IDX;
};

struct hw_db_qsl_idx {
	HW_DB_IDX;
};

struct hw_db_cat_idx {
	HW_DB_IDX;
};

struct hw_db_slc_lr_idx {
	HW_DB_IDX;
};

struct hw_db_inline_tpe_data {
	uint32_t insert_len : 16;
	uint32_t new_outer : 1;
	uint32_t calc_eth_type_from_inner_ip : 1;
	uint32_t ttl_en : 1;
	uint32_t ttl_dyn : 5;
	uint32_t ttl_ofs : 8;

	struct {
		uint32_t en : 1;
		uint32_t reader_select : 3;
		uint32_t dyn : 5;
		uint32_t ofs : 14;
		uint32_t len : 5;
		uint32_t padding : 4;
	} writer[6];

	uint32_t len_a_en : 1;
	uint32_t len_a_pos_dyn : 5;
	uint32_t len_a_pos_ofs : 8;
	uint32_t len_a_add_dyn : 5;
	uint32_t len_a_add_ofs : 8;
	uint32_t len_a_sub_dyn : 5;

	uint32_t len_b_en : 1;
	uint32_t len_b_pos_dyn : 5;
	uint32_t len_b_pos_ofs : 8;
	uint32_t len_b_add_dyn : 5;
	uint32_t len_b_add_ofs : 8;
	uint32_t len_b_sub_dyn : 5;

	uint32_t len_c_en : 1;
	uint32_t len_c_pos_dyn : 5;
	uint32_t len_c_pos_ofs : 8;
	uint32_t len_c_add_dyn : 5;
	uint32_t len_c_add_ofs : 8;
	uint32_t len_c_sub_dyn : 5;
};

struct hw_db_inline_tpe_ext_data {
	uint32_t size;
	union {
		uint8_t hdr8[HW_DB_INLINE_MAX_ENCAP_SIZE];
		uint32_t hdr32[(HW_DB_INLINE_MAX_ENCAP_SIZE + 3) / 4];
	};
};

struct hw_db_tpe_idx {
	HW_DB_IDX;
};
struct hw_db_tpe_ext_idx {
	HW_DB_IDX;
};

struct hw_db_flm_idx {
	HW_DB_IDX;
};
struct hw_db_flm_ft {
	HW_DB_IDX;
};

struct hw_db_flm_scrub_idx {
	HW_DB_IDX;
};

struct hw_db_km_idx {
	HW_DB_IDX;
};

struct hw_db_km_ft {
	HW_DB_IDX;
};

struct hw_db_hsh_idx {
	HW_DB_IDX;
};

enum hw_db_idx_type {
	HW_DB_IDX_TYPE_NONE = 0,

	HW_DB_IDX_TYPE_MATCH_SET,
	HW_DB_IDX_TYPE_ACTION_SET,

	HW_DB_IDX_TYPE_COT,
	HW_DB_IDX_TYPE_CAT,
	HW_DB_IDX_TYPE_QSL,
	HW_DB_IDX_TYPE_SLC_LR,
	HW_DB_IDX_TYPE_TPE,
	HW_DB_IDX_TYPE_TPE_EXT,

	HW_DB_IDX_TYPE_FLM_RCP,
	HW_DB_IDX_TYPE_KM_RCP,
	HW_DB_IDX_TYPE_FLM_FT,
	HW_DB_IDX_TYPE_FLM_SCRUB,
	HW_DB_IDX_TYPE_KM_FT,
	HW_DB_IDX_TYPE_HSH,
};

/* Container types */
struct hw_db_inline_match_set_data {
	struct hw_db_cat_idx cat;
	struct hw_db_km_idx km;
	struct hw_db_km_ft km_ft;
	struct hw_db_action_set_idx action_set;
	int jump;

	uint8_t priority;
};

struct hw_db_inline_action_set_data {
	int contains_jump;
	union {
		int jump;
		struct {
			struct hw_db_cot_idx cot;
			struct hw_db_qsl_idx qsl;
			struct hw_db_slc_lr_idx slc_lr;
			struct hw_db_tpe_idx tpe;
			struct hw_db_hsh_idx hsh;
			struct hw_db_flm_scrub_idx scrub;
		};
	};
};

struct hw_db_inline_km_rcp_data {
	uint32_t rcp;
};

struct hw_db_inline_km_ft_data {
	struct hw_db_cat_idx cat;
	struct hw_db_km_idx km;
	struct hw_db_action_set_idx action_set;
};

struct hw_db_inline_flm_ft_data {
	/* Group zero flows should set jump. */
	/* Group nonzero flows should set group. */
	int is_group_zero;
	union {
		int jump;
		int group;
	};

	struct hw_db_action_set_idx action_set;
};

/* Functionality data types */
struct hw_db_inline_cat_data {
	uint32_t vlan_mask : 4;
	uint32_t mac_port_mask : 8;
	uint32_t ptc_mask_frag : 4;
	uint32_t ptc_mask_l2 : 7;
	uint32_t ptc_mask_l3 : 3;
	uint32_t ptc_mask_l4 : 5;
	uint32_t padding0 : 1;

	uint32_t ptc_mask_tunnel : 11;
	uint32_t ptc_mask_l3_tunnel : 3;
	uint32_t ptc_mask_l4_tunnel : 5;
	uint32_t err_mask_ttl_tunnel : 2;
	uint32_t err_mask_ttl : 2;
	uint32_t padding1 : 9;

	uint8_t ip_prot;
	uint8_t ip_prot_tunnel;
};

struct hw_db_inline_flm_rcp_data {
	uint64_t qw0_dyn : 5;
	uint64_t qw0_ofs : 8;
	uint64_t qw4_dyn : 5;
	uint64_t qw4_ofs : 8;
	uint64_t sw8_dyn : 5;
	uint64_t sw8_ofs : 8;
	uint64_t sw9_dyn : 5;
	uint64_t sw9_ofs : 8;
	uint64_t outer_prot : 1;
	uint64_t inner_prot : 1;
	uint64_t padding : 10;

	uint32_t mask[10];
};

struct hw_db_inline_qsl_data {
	uint32_t discard : 1;
	uint32_t drop : 1;
	uint32_t table_size : 7;
	uint32_t retransmit : 1;
	uint32_t padding : 22;

	struct {
		uint16_t queue : 7;
		uint16_t queue_en : 1;
		uint16_t tx_port : 3;
		uint16_t tx_port_en : 1;
		uint16_t padding : 4;
	} table[HW_DB_INLINE_MAX_QST_PER_QSL];
};

struct hw_db_inline_cot_data {
	uint32_t matcher_color_contrib : 4;
	uint32_t frag_rcp : 4;
	uint32_t padding : 24;
};

struct hw_db_inline_slc_lr_data {
	uint32_t head_slice_en : 1;
	uint32_t head_slice_dyn : 5;
	uint32_t head_slice_ofs : 8;
	uint32_t padding : 18;
};

struct hw_db_inline_hsh_data {
	uint32_t func;
	uint64_t hash_mask;
	uint8_t key[MAX_RSS_KEY_LEN];
};

struct hw_db_inline_scrub_data {
	uint32_t timeout;
};

/**/

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle);
void hw_db_inline_destroy(void *db_handle);

void hw_db_inline_deref_idxs(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_idx *idxs,
	uint32_t size);
const void *hw_db_inline_find_data(struct flow_nic_dev *ndev, void *db_handle,
	enum hw_db_idx_type type, struct hw_db_idx *idxs, uint32_t size);
struct hw_db_idx *hw_db_inline_find_idx(struct flow_nic_dev *ndev, void *db_handle,
	enum hw_db_idx_type type, struct hw_db_idx *idxs, uint32_t size);
void hw_db_inline_dump(struct flow_nic_dev *ndev, void *db_handle, const struct hw_db_idx *idxs,
	uint32_t size, FILE *file);
void hw_db_inline_dump_cfn(struct flow_nic_dev *ndev, void *db_handle, FILE *file);

/**/

struct hw_db_match_set_idx
hw_db_inline_match_set_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_match_set_data *data);
struct hw_db_action_set_idx
hw_db_inline_action_set_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_action_set_data *data);
/**/

struct hw_db_cot_idx hw_db_inline_cot_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cot_data *data);
struct hw_db_qsl_idx hw_db_inline_qsl_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_qsl_data *data);
struct hw_db_slc_lr_idx hw_db_inline_slc_lr_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_slc_lr_data *data);
struct hw_db_tpe_idx hw_db_inline_tpe_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_tpe_data *data);
struct hw_db_tpe_ext_idx hw_db_inline_tpe_ext_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_tpe_ext_data *data);
struct hw_db_hsh_idx hw_db_inline_hsh_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_hsh_data *data);
/**/

struct hw_db_cat_idx hw_db_inline_cat_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cat_data *data);
/**/

struct hw_db_km_idx hw_db_inline_km_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_rcp_data *data);
struct hw_db_km_ft hw_db_inline_km_ft_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_ft_data *data);
/**/

struct hw_db_flm_idx hw_db_inline_flm_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_rcp_data *data, int group);
struct hw_db_flm_ft hw_db_inline_flm_ft_default(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_ft_data *data);
struct hw_db_flm_ft hw_db_inline_flm_ft_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_ft_data *data);
struct hw_db_flm_scrub_idx hw_db_inline_scrub_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_scrub_data *data);
int hw_db_inline_setup_mbr_filter(struct flow_nic_dev *ndev, uint32_t cat_hw_id, uint32_t ft,
	uint32_t qsl_hw_id);

#endif	/* _FLOW_API_HW_DB_INLINE_H_ */
