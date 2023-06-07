/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef _SFC_TBLS_H
#define _SFC_TBLS_H

#include "efx.h"

#include "sfc_tbl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Table types:
 *   CAM - Content addressable memory
 *  BCAM - Binary CAM
 *  TCAM - Ternary CAM
 * STCAM - Semi-ternary CAM
 *
 * Short description:
 * TCAM:  Each entry has a key, mask, response and priority. An entry matches
 *        when (key & mask) == (request & mask). In the case of multiple
 *        matches, the entry with the highest priority wins; Each entry may
 *        have its own mask, but TCAM table definitions may place constraints
 *        on the possible masks allowed for each of the individual fields.
 * STCAM: A limited form of TCAM in which only a limited number of masks and
 *        associated priorities), up to some maximum fixed by the definition
 *        of the table, may be in use at any one time.
 * BCAM:  Each entry has only a key and response, with the whole request
 *        matched against the key (like a typical hash table or "map").
 * Direct (sometimes "DCAM", although it's not really content-addressable):
 *        Essentially just an array, where the key bits are used simply as an
 *        index.
 */

/* Priority is used only for TCAM or STCAM, use 0 in case of BCAM */
#define SFC_TBLS_BCAM_PRIORITY		0

/* Mask ID is used only for STCAM with ALLOC_MASKS flag, use 0 for BCAM */
#define SFC_TBLS_BCAM_MASK_ID		0

/* Mask is used only for STCAM */
#define SFC_TBLS_BCAM_MASK_WIDTH	0

/** Options for HW tables support status */
enum sfc_tbls_status {
	SFC_TBLS_STATUS_UNKNOWN = 0,
	SFC_TBLS_STATUS_UNSUPPORTED,
	SFC_TBLS_STATUS_SUPPORTED,
};

/**
 * Entry point to access HW tables
 *
 * SFC driver can access hardware (HW) tables.
 * Interaction with HW tables is done through the MCDI table access API
 * that is implemented in EFX.
 *
 * In order to manipulate data on HW tables it's necessary to
 * - discover the list of supported tables;
 * - read a table descriptor to get details of the structure
 *   of the table;
 * - get named fields of the table;
 * - insert/delete/update table entries based on given fields
 *   and information about the table
 *
 * All table layout data should be saved in a cache.
 * The cache allows to avoid getting the table descriptor each time when you want
 * to manipulate table entries. It just contains the table
 * descriptors and all associated data. The cache is based on the RTE hash map and
 * it uses a table ID as a key.
 * The sfc_tbl_meta library serves as a wrapper over the cache and allows to user
 * to get all information about the tables without worrying about the cache.
 *
 * +------------------------+
 * | Cache is uninitialized |<----------------------------------+
 * +------------------------+					|
 *	|							|
 *	| sfc_attach()						|
 *	| sfc_tbls_attach() -- (fail) -- sfc_tbls_detach()------+
 *	V					^
 * +------------------------+			|
 * |  Cache is initialized  |			+-------+
 * +------------------------+				|
 *	| sfc_start()					|
 *	| sfc_tbls_start() -- (fail) -- sfc_tbls_stop()-+
 *	V						|
 * +------------------------+				|
 * | Cache is initialized   |				|
 * | and valid              |				|
 * +------------------------+				|
 *	|						|
 *	| sfc_restart()					|
 *	V						|
 * +------------------------+				|
 * | Cache is initialized   |				|
 * | but can be invalid     |				|
 * +------------------------+---------------------------+
 */
struct sfc_tbls {
	struct sfc_tbl_meta_cache	meta;
	enum sfc_tbls_status		status;
};

struct sfc_adapter;

static inline bool
sfc_tbls_id_is_supported(struct sfc_adapter *sa,
			 efx_table_id_t table_id)
{
	return (sfc_tbl_meta_lookup(sa, table_id) == NULL ? false : true);
}

int sfc_tbls_attach(struct sfc_adapter *sa);
void sfc_tbls_detach(struct sfc_adapter *sa);
int sfc_tbls_start(struct sfc_adapter *sa);

static inline void
sfc_tbls_stop(struct sfc_adapter *sa)
{
	sfc_tbls_detach(sa);
}

static inline int
sfc_tbls_bcam_entry_insert(efx_nic_t *enp, efx_table_id_t table_id, uint16_t key_width,
			   uint16_t resp_width, uint8_t *data, unsigned int data_size)
{
	return efx_table_entry_insert(enp, table_id, SFC_TBLS_BCAM_PRIORITY,
				      SFC_TBLS_BCAM_MASK_ID, key_width,
				      SFC_TBLS_BCAM_MASK_WIDTH, resp_width,
				      data, data_size);
}

static inline int
sfc_tbls_bcam_entry_delete(efx_nic_t *enp, efx_table_id_t table_id, uint16_t key_width,
			   uint8_t *data, unsigned int data_size)
{
	return efx_table_entry_delete(enp, table_id, SFC_TBLS_BCAM_MASK_ID,
				      key_width, SFC_TBLS_BCAM_MASK_WIDTH,
				      data, data_size);
}

/**
 * All manipulations with HW tables entries require forming
 * a key and response.
 * The key and response fields follow, consecutively, each
 * packed as follows:
 *  - the key/response is logically treated as a single wide N-bit value;
 *  - fields have been placed in these logical values per the "lbn" and "width"
 *    information from the table field descriptors;
 *  - the wide N-bit value is padded at the MSB end up to a 32-bit boundary;
 *  - the values are put into the table op request with bits[31:0] of the wide
 *    value in the first 32-bit word, bits[63:32] in the second 32-bit word, etc.
 *
 * Below is an API that helps to form  MCDI insertion/deletion request.
 * Workflow:
 * 1) Allocate an array of EFX_TABLE_ENTRY_LENGTH_MAX bytes.
 * 2) Read a descriptor of the table that you want to use.
 * 3) Fill the array using sfc_tbls_field_set_* functions to form a key.
 *    Each field of the key has LBN and width. This information can be
 *    found in a field's descriptor.
 * 4) Use sfc_tbls_next_req_fields() to get a pointer where the response
 *    must start. It's required as the key and response need to be
 *    zero-padded at the MSB end to multiples of 32 bits.
 * 5) Fill the response the same way.
 * 6) Use sfc_tbls_next_req_fields() to get the end of the data request.
 *    It will help you to get the real size of the data request.
 */

/**
 * Get a pointer to the beginning of the next 32-bit wide fields
 * that go after a given width.
 * It should be used to get a pointer to the response's start and the end
 * of the data for an MCDI request.
 *
 * @param data		Pointer to the data to make an offset from
 * @param width		Width of fields to offset
 *
 * @note @p width is expected to be a key's or response's size.
 *
 * @return Pointer to the beginning of the next field.
 */
static inline uint32_t *
sfc_tbls_next_req_fields(uint32_t *data, uint16_t width)
{
	return data + EFX_DIV_ROUND_UP(width, sizeof(*data) * CHAR_BIT);
}

/**
 * Insert value into a field in the @p data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param value		uint32_t value to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_u32(uint32_t data[], unsigned int data_size,
			    uint16_t lbn, uint16_t width, uint32_t value);

/**
 * Insert value into a field in the @p data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param value		uint16_t value to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_u16(uint32_t data[], unsigned int data_size,
			    uint16_t lbn, uint16_t width, uint16_t value);

/**
 * Insert value into a field in the @p data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param value		uint8_t value to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_u8(uint32_t data[], unsigned int data_size,
			   uint16_t lbn, uint16_t width, uint8_t value);

/**
 * Insert IP address into a field in the @p data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param ip		IP address to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_ip(uint32_t data[], unsigned int data_size,
			   uint16_t lbn, uint16_t width, const uint32_t *ip);

/**
 * Insert value into a field in the data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param value		uint64_t value to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_u64(uint32_t data[], unsigned int data_size,
			    uint16_t lbn, uint16_t width, uint64_t value);

/**
 * Insert value into a field in the @p data buffer starting at
 * bit offset @p lbn and containing @p width bits.
 *
 * @param data		Data buffer
 * @param data_size	Size of the data buffer
 * @param lbn		Offset
 * @param width		Width of @p value in bits
 * @param value		Bit value to insert
 *
 * @note @p width and @p lbn must to be obtained from the field's descriptor.
 */
void sfc_tbls_field_set_bit(uint32_t data[], unsigned int data_size,
			    uint16_t lbn, uint16_t width, bool value);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_TBLS_H */
