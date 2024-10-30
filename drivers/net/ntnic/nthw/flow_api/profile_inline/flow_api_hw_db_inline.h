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

enum hw_db_idx_type {
	HW_DB_IDX_TYPE_NONE = 0,
	HW_DB_IDX_TYPE_COT,
};

/* Functionality data types */
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

struct hw_db_inline_hsh_data {
	uint32_t func;
	uint64_t hash_mask;
	uint8_t key[MAX_RSS_KEY_LEN];
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

#endif	/* _FLOW_API_HW_DB_INLINE_H_ */
