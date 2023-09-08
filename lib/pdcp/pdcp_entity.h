/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_ENTITY_H
#define PDCP_ENTITY_H

#include <rte_common.h>
#include <rte_crypto_sym.h>
#include <rte_mempool.h>
#include <rte_pdcp.h>
#include <rte_security.h>

#include "pdcp_reorder.h"

struct entity_priv;

#define PDCP_HFN_MIN 0

/* IV generation function based on the entity configuration */
typedef void (*iv_gen_t)(struct rte_crypto_op *cop, const struct entity_priv *en_priv,
			 uint32_t count);

struct entity_state {
	uint32_t rx_next;
	uint32_t tx_next;
	uint32_t rx_deliv;
	uint32_t rx_reord;
};

union auth_iv_partial {
	/* For AES-CMAC, there is no IV, but message gets prepended */
	struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		uint64_t count : 32;
		uint64_t zero_38_39 : 2;
		uint64_t direction : 1;
		uint64_t bearer : 5;
		uint64_t zero_40_63 : 24;
#else
		uint64_t count : 32;
		uint64_t bearer : 5;
		uint64_t direction : 1;
		uint64_t zero_38_39 : 2;
		uint64_t zero_40_63 : 24;
#endif
	} aes_cmac;
	struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		uint64_t count : 32;
		uint64_t zero_37_39 : 3;
		uint64_t bearer : 5;
		uint64_t zero_40_63 : 24;

		uint64_t rsvd_65_71 : 7;
		uint64_t direction_64 : 1;
		uint64_t rsvd_72_111 : 40;
		uint64_t rsvd_113_119 : 7;
		uint64_t direction_112 : 1;
		uint64_t rsvd_120_127 : 8;
#else
		uint64_t count : 32;
		uint64_t bearer : 5;
		uint64_t zero_37_39 : 3;
		uint64_t zero_40_63 : 24;

		uint64_t direction_64 : 1;
		uint64_t rsvd_65_71 : 7;
		uint64_t rsvd_72_111 : 40;
		uint64_t direction_112 : 1;
		uint64_t rsvd_113_119 : 7;
		uint64_t rsvd_120_127 : 8;
#endif
	} zs;
	uint64_t u64[2];
};

union cipher_iv_partial {
	struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		uint64_t count : 32;
		uint64_t zero_38_39 : 2;
		uint64_t direction : 1;
		uint64_t bearer : 5;
		uint64_t zero_40_63 : 24;
#else
		uint64_t count : 32;
		uint64_t bearer : 5;
		uint64_t direction : 1;
		uint64_t zero_38_39 : 2;
		uint64_t zero_40_63 : 24;
#endif
		uint64_t zero_64_127;
	} aes_ctr;
	struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		uint64_t count : 32;
		uint64_t zero_38_39 : 2;
		uint64_t direction : 1;
		uint64_t bearer : 5;
		uint64_t zero_40_63 : 24;
#else
		uint64_t count : 32;
		uint64_t bearer : 5;
		uint64_t direction : 1;
		uint64_t zero_38_39 : 2;
		uint64_t zero_40_63 : 24;
#endif
		uint64_t rsvd_64_127;
	} zs;
	uint64_t u64[2];
};

enum timer_state {
	TIMER_STOP,
	TIMER_RUNNING,
	TIMER_EXPIRED,
};

struct pdcp_t_reordering {
	/** Represent timer state */
	enum timer_state state;
	/** User defined callback handles */
	struct rte_pdcp_t_reordering handle;
};

struct pdcp_cnt_bitmap {
	/** Number of entries that can be stored. */
	uint32_t size;
	/** Bitmap of the count values already received.*/
	struct rte_bitmap *bmp;
};

/*
 * Layout of PDCP entity: [rte_pdcp_entity] [entity_priv] [entity_dl/ul] [reorder/bitmap]
 */

struct entity_priv {
	/** Crypto sym session. */
	struct rte_cryptodev_sym_session *crypto_sess;
	/** Entity specific IV generation function. */
	iv_gen_t iv_gen;
	/** Pre-prepared auth IV. */
	union auth_iv_partial auth_iv_part;
	/** Pre-prepared cipher IV. */
	union cipher_iv_partial cipher_iv_part;
	/** Entity state variables. */
	struct entity_state state;
	/** Flags. */
	struct {
		/** PDCP PDU has 4 byte MAC-I. */
		uint64_t is_authenticated : 1;
		/** Cipher offset & length in bits. */
		uint64_t is_cipher_in_bits : 1;
		/** Auth offset & length in bits. */
		uint64_t is_auth_in_bits : 1;
		/** Is UL/transmitting PDCP entity. */
		uint64_t is_ul_entity : 1;
		/** Is NULL auth. */
		uint64_t is_null_auth : 1;
		/** Is status report required.*/
		uint64_t is_status_report_required : 1;
		/** Is out-of-order delivery enabled */
		uint64_t is_out_of_order_delivery : 1;
	} flags;
	/** Crypto op pool. */
	struct rte_mempool *cop_pool;
	/** Control PDU pool. */
	struct rte_mempool *ctrl_pdu_pool;
	/** PDCP header size. */
	uint8_t hdr_sz;
	/** PDCP AAD size. For AES-CMAC, additional message is prepended for the operation. */
	uint8_t aad_sz;
	/** PDCP cipher skip size. When enabled, SDAP header needs to be skipped from ciphering */
	uint8_t cipher_skip_sz;
	/** Device ID of the device to be used for offload. */
	uint8_t dev_id;
};

struct entity_priv_dl_part {
	/** PDCP would need to track the count values that are already received.*/
	struct pdcp_cnt_bitmap bitmap;
	/** t-Reordering handles */
	struct pdcp_t_reordering t_reorder;
	/** Reorder packet buffer */
	struct pdcp_reorder reorder;
	/** Bitmap memory region */
	uint8_t bitmap_mem[0];
};

struct entity_priv_ul_part {
	/*
	 * NOTE: when re-establish is supported, plain PDCP packets & COUNT values need to be
	 * cached.
	 */
	uint8_t dummy;
};

static inline struct entity_priv *
entity_priv_get(const struct rte_pdcp_entity *entity) {
	return RTE_PTR_ADD(entity, sizeof(struct rte_pdcp_entity));
}

static inline struct entity_priv_dl_part *
entity_dl_part_get(const struct rte_pdcp_entity *entity) {
	return RTE_PTR_ADD(entity, sizeof(struct rte_pdcp_entity) + sizeof(struct entity_priv));
}

static inline struct entity_priv_ul_part *
entity_ul_part_get(const struct rte_pdcp_entity *entity) {
	return RTE_PTR_ADD(entity, sizeof(struct rte_pdcp_entity) + sizeof(struct entity_priv));
}

static inline int
pdcp_hdr_size_get(enum rte_security_pdcp_sn_size sn_size)
{
	return RTE_ALIGN_MUL_CEIL(sn_size, 8) / 8;
}

static inline uint32_t
pdcp_window_size_get(enum rte_security_pdcp_sn_size sn_size)
{
	return 1 << (sn_size - 1);
}

static inline uint32_t
pdcp_sn_mask_get(enum rte_security_pdcp_sn_size sn_size)
{
	return (1 << sn_size) - 1;
}

static inline uint32_t
pdcp_sn_from_count_get(uint32_t count, enum rte_security_pdcp_sn_size sn_size)
{
	return (count & pdcp_sn_mask_get(sn_size));
}

static inline uint32_t
pdcp_hfn_mask_get(enum rte_security_pdcp_sn_size sn_size)
{
	return ~pdcp_sn_mask_get(sn_size);
}

static inline uint32_t
pdcp_hfn_from_count_get(uint32_t count, enum rte_security_pdcp_sn_size sn_size)
{
	return (count & pdcp_hfn_mask_get(sn_size)) >> sn_size;
}

static inline uint32_t
pdcp_count_from_hfn_sn_get(uint32_t hfn, uint32_t sn, enum rte_security_pdcp_sn_size sn_size)
{
	return (((hfn << sn_size) & pdcp_hfn_mask_get(sn_size)) | (sn & pdcp_sn_mask_get(sn_size)));
}

static inline uint32_t
pdcp_hfn_max(enum rte_security_pdcp_sn_size sn_size)
{
	return (1 << (32 - sn_size)) - 1;
}

#endif /* PDCP_ENTITY_H */
