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

/*
 * Layout of PDCP entity: [rte_pdcp_entity] [entity_priv] [entity_dl/ul]
 */

struct entity_priv {
	/** Crypto sym session. */
	struct rte_cryptodev_sym_session *crypto_sess;
	/** Entity specific IV generation function. */
	iv_gen_t iv_gen;
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
	} flags;
	/** Crypto op pool. */
	struct rte_mempool *cop_pool;
	/** PDCP header size. */
	uint8_t hdr_sz;
	/** PDCP AAD size. For AES-CMAC, additional message is prepended for the operation. */
	uint8_t aad_sz;
	/** Device ID of the device to be used for offload. */
	uint8_t dev_id;
};

struct entity_priv_dl_part {
	/* NOTE: when in-order-delivery is supported, post PDCP packets would need to cached. */
	uint8_t dummy;
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
