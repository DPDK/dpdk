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

struct hw_db_cot_idx {
	HW_DB_IDX;
};

struct hw_db_cat_idx {
	HW_DB_IDX;
};

struct hw_db_slc_lr_idx {
	HW_DB_IDX;
};

enum hw_db_idx_type {
	HW_DB_IDX_TYPE_NONE = 0,
	HW_DB_IDX_TYPE_COT,
	HW_DB_IDX_TYPE_CAT,
	HW_DB_IDX_TYPE_SLC_LR,
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

struct hw_db_inline_action_set_data {
	int contains_jump;
	union {
		int jump;
		struct {
			struct hw_db_cot_idx cot;
		};
	};
};

/**/

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle);
void hw_db_inline_destroy(void *db_handle);

void hw_db_inline_deref_idxs(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_idx *idxs,
	uint32_t size);

/**/
struct hw_db_cot_idx hw_db_inline_cot_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cot_data *data);
void hw_db_inline_cot_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cot_idx idx);
void hw_db_inline_cot_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cot_idx idx);

struct hw_db_slc_lr_idx hw_db_inline_slc_lr_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_slc_lr_data *data);
void hw_db_inline_slc_lr_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx);
void hw_db_inline_slc_lr_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx);

/**/

struct hw_db_cat_idx hw_db_inline_cat_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cat_data *data);
void hw_db_inline_cat_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cat_idx idx);
void hw_db_inline_cat_deref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_cat_idx idx);

/**/

int hw_db_inline_setup_mbr_filter(struct flow_nic_dev *ndev, uint32_t cat_hw_id, uint32_t ft,
	uint32_t qsl_hw_id);

#endif	/* _FLOW_API_HW_DB_INLINE_H_ */
