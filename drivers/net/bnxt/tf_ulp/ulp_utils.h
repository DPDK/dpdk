/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_UTILS_H_
#define _ULP_UTILS_H_

#include "bnxt.h"
#include "ulp_template_db_enum.h"

#define ULP_BUFFER_ALIGN_8_BITS		8
#define ULP_BUFFER_ALIGN_8_BYTE		8
#define ULP_BUFFER_ALIGN_16_BYTE	16
#define ULP_BUFFER_ALIGN_64_BYTE	64
#define ULP_64B_IN_BYTES		8

/*
 * Macros for bitmap sets and gets
 * These macros can be used if the val are power of 2.
 */
#define ULP_BITMAP_SET(bitmap, val)	((bitmap) |= (val))
#define ULP_BITMAP_RESET(bitmap, val)	((bitmap) &= ~(val))
#define ULP_BITMAP_ISSET(bitmap, val)	((bitmap) & (val))
#define ULP_BITMAP_CMP(b1, b2)  memcmp(&(b1)->bits, \
				&(b2)->bits, sizeof((b1)->bits))
/*
 * Macros for bitmap sets and gets
 * These macros can be used if the val are not power of 2 and
 * are simple index values.
 */
#define ULP_INDEX_BITMAP_SIZE	(sizeof(uint64_t) * 8)
#define ULP_INDEX_BITMAP_CSET(i)	(1UL << \
			((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE)))

#define ULP_INDEX_BITMAP_SET(b, i)	((b) |= \
			(1UL << ((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE))))

#define ULP_INDEX_BITMAP_RESET(b, i)	((b) &= \
			(~(1UL << ((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE)))))

#define ULP_INDEX_BITMAP_GET(b, i)		(((b) >> \
			((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE))) & 1)

#define ULP_DEVICE_PARAMS_INDEX(tid, dev_id)	\
	(((tid) << BNXT_ULP_LOG2_MAX_NUM_DEV) | (dev_id))

/* Macro to convert bytes to bits */
#define ULP_BYTE_2_BITS(byte_x)		((byte_x) * 8)
/* Macro to convert bits to bytes */
#define ULP_BITS_2_BYTE(bits_x)		(((bits_x) + 7) / 8)
/* Macro to convert bits to bytes with no round off*/
#define ULP_BITS_2_BYTE_NR(bits_x)	((bits_x) / 8)

/* Macro to round off to next multiple of 8*/
#define ULP_BYTE_ROUND_OFF_8(x)	(((x) + 7) & ~7)

/* Macro to check bits are byte aligned */
#define ULP_BITS_IS_BYTE_NOT_ALIGNED(x)	((x) % 8)

/* Macros to read the computed fields */
#define ULP_COMP_FLD_IDX_RD(params, idx) \
	rte_be_to_cpu_64((params)->comp_fld[(idx)])

#define ULP_COMP_FLD_IDX_WR(params, idx, val)	\
	((params)->comp_fld[(idx)] = rte_cpu_to_be_64((uint64_t)(val)))
/*
 * Making the blob statically sized to 128 bytes for now.
 * The blob must be initialized with ulp_blob_init prior to using.
 */
#define BNXT_ULP_FLMP_BLOB_SIZE	(128)
#define BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS	ULP_BYTE_2_BITS(BNXT_ULP_FLMP_BLOB_SIZE)
struct ulp_blob {
	enum bnxt_ulp_byte_order	byte_order;
	uint16_t			write_idx;
	uint16_t			bitlen;
	uint8_t				data[BNXT_ULP_FLMP_BLOB_SIZE];
	uint16_t			encap_swap_idx;
};

/*
 * The data can likely be only 32 bits for now.  Just size check
 * the data when being written.
 */
#define ULP_REGFILE_ENTRY_SIZE	(sizeof(uint32_t))
struct ulp_regfile_entry {
	uint64_t	data;
	uint32_t	size;
};

struct ulp_regfile {
	struct ulp_regfile_entry entry[BNXT_ULP_RF_IDX_LAST];
};

/*
 * Initialize the regfile structure for writing
 *
 * regfile [in] Ptr to a regfile instance
 *
 * returns 0 on error or 1 on success
 */
uint32_t
ulp_regfile_init(struct ulp_regfile *regfile);

/*
 * Read a value from the regfile
 *
 * regfile [in] The regfile instance.  Must be initialized prior to being used
 *
 * field [in] The field to be read within the regfile.
 *
 * returns the byte array
 */
uint32_t
ulp_regfile_read(struct ulp_regfile *regfile,
		 enum bnxt_ulp_rf_idx field,
		 uint64_t *data);

/*
 * Write a value to the regfile
 *
 * regfile [in] The regfile instance.  Must be initialized prior to being used
 *
 * field [in] The field to be written within the regfile.
 *
 * data [in] The value is written into this variable.  It is going to be in the
 * same byte order as it was written.
 *
 * returns zero on success
 */
int32_t
ulp_regfile_write(struct ulp_regfile *regfile,
		  enum bnxt_ulp_rf_idx field,
		  uint64_t data);

/*
 * Add data to the byte array in Little endian format.
 *
 * bs [in] The byte array where data is pushed
 *
 * pos [in] The offset where data is pushed
 *
 * len [in] The number of bits to be added to the data array.
 *
 * val [in] The data to be added to the data array.
 *
 * returns the number of bits pushed.
 */
uint32_t
ulp_bs_push_lsb(uint8_t *bs, uint16_t pos, uint8_t len, uint8_t *val);

/*
 * Add data to the byte array in Big endian format.
 *
 * bs [in] The byte array where data is pushed
 *
 * pos [in] The offset where data is pushed
 *
 * len [in] The number of bits to be added to the data array.
 *
 * val [in] The data to be added to the data array.
 *
 * returns the number of bits pushed.
 */
uint32_t
ulp_bs_push_msb(uint8_t *bs, uint16_t pos, uint8_t len, uint8_t *val);

/*
 * Initializes the blob structure for creating binary blob
 *
 * blob [in] The blob to be initialized
 *
 * bitlen [in] The bit length of the blob
 *
 * order [in] The byte order for the blob.  Currently only supporting
 * big endian.  All fields are packed with this order.
 *
 * returns 0 on error or 1 on success
 */
uint32_t
ulp_blob_init(struct ulp_blob *blob,
	      uint16_t bitlen,
	      enum bnxt_ulp_byte_order order);

/*
 * Add data to the binary blob at the current offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * data [in] A pointer to bytes to be added to the blob.
 *
 * datalen [in] The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error.
 */
uint32_t
ulp_blob_push(struct ulp_blob *blob,
	      uint8_t *data,
	      uint32_t datalen);

/*
 * Insert data into the binary blob at the given offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * offset [in] The offset where the data needs to be inserted.
 *
 * data [in/out] A pointer to bytes to be added to the blob.
 *
 * datalen [in] The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error.
 */
uint32_t
ulp_blob_insert(struct ulp_blob *blob, uint32_t offset,
		uint8_t *data, uint32_t datalen);

/*
 * Add data to the binary blob at the current offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * data [in] 64-bit value to be added to the blob.
 *
 * datalen [in] The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, ptr to pushed data otherwise
 */
uint8_t *
ulp_blob_push_64(struct ulp_blob *blob,
		 uint64_t *data,
		 uint32_t datalen);

/*
 * Add data to the binary blob at the current offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * data [in] 32-bit value to be added to the blob.
 *
 * datalen [in] The number of bits to be added ot the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
uint8_t *
ulp_blob_push_32(struct ulp_blob *blob,
		 uint32_t *data,
		 uint32_t datalen);

/*
 * Add encap data to the binary blob at the current offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * data [in] value to be added to the blob.
 *
 * datalen [in] The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
int32_t
ulp_blob_push_encap(struct ulp_blob *blob,
		    uint8_t *data,
		    uint32_t datalen);

/*
 * Get the data portion of the binary blob.
 *
 * blob [in] The blob's data to be retrieved. The blob must be
 * initialized prior to pushing data.
 *
 * datalen [out] The number of bits to that are filled.
 *
 * returns a byte array of the blob data.  Returns NULL on error.
 */
uint8_t *
ulp_blob_data_get(struct ulp_blob *blob,
		  uint16_t *datalen);

/*
 * Get the data length of the binary blob.
 *
 * blob [in] The blob's data len to be retrieved.
 *
 * returns length of the binary blob
 */
uint16_t
ulp_blob_data_len_get(struct ulp_blob *blob);

/*
 * Get data from the byte array in Little endian format.
 *
 * src [in] The byte array where data is extracted from
 *
 * dst [out] The byte array where data is pulled into
 *
 * size [in] The size of dst array in bytes
 *
 * offset [in] The offset where data is pulled
 *
 * len [in] The number of bits to be extracted from the data array
 *
 * returns None.
 */
void
ulp_bs_pull_lsb(uint8_t *src, uint8_t *dst, uint32_t size,
		uint32_t offset, uint32_t len);

/*
 * Get data from the byte array in Big endian format.
 *
 * src [in] The byte array where data is extracted from
 *
 * dst [out] The byte array where data is pulled into
 *
 * offset [in] The offset where data is pulled
 *
 * len [in] The number of bits to be extracted from the data array
 *
 * returns None.
 */
void
ulp_bs_pull_msb(uint8_t *src, uint8_t *dst,
		uint32_t offset, uint32_t len);

/*
 * Extract data from the binary blob using given offset.
 *
 * blob [in] The blob that data is extracted from. The blob must
 * be initialized prior to pulling data.
 *
 * data [in] A pointer to put the data.
 * data_size [in] size of the data buffer in bytes.
 *offset [in] - Offset in the blob to extract the data in bits format.
 * len [in] The number of bits to be pulled from the blob.
 *
 * Output: zero on success, -1 on failure
 */
int32_t
ulp_blob_pull(struct ulp_blob *blob, uint8_t *data, uint32_t data_size,
	      uint16_t offset, uint16_t len);

/*
 * Adds pad to an initialized blob at the current offset
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * datalen [in] The number of bits of pad to add
 *
 * returns the number of pad bits added, -1 on failure
 */
int32_t
ulp_blob_pad_push(struct ulp_blob *blob,
		  uint32_t datalen);

/*
 * Adds pad to an initialized blob at the current offset based on
 * the alignment.
 *
 * blob [in] The blob that needs to be aligned
 *
 * align [in] Alignment in bits.
 *
 * returns the number of pad bits added, -1 on failure
 */
int32_t
ulp_blob_pad_align(struct ulp_blob *blob,
		   uint32_t align);

/*
 * Set the 64 bit swap start index of the binary blob.
 *
 * blob [in] The blob's data to be retrieved. The blob must be
 * initialized prior to pushing data.
 *
 * returns void.
 */
void
ulp_blob_encap_swap_idx_set(struct ulp_blob *blob);

/*
 * Perform the encap buffer swap to 64 bit reversal.
 *
 * blob [in] The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_encap_swap(struct ulp_blob *blob);

/*
 * Perform the blob buffer reversal byte wise.
 * This api makes the first byte the last and
 * vice-versa.
 *
 * blob [in] The blob's data to be used for swap.
 * chunk_size[in] the swap is done within the chunk in bytes
 *
 * returns void.
 */
void
ulp_blob_perform_byte_reverse(struct ulp_blob *blob,
			      uint32_t chunk_size);

/*
 * Perform the blob buffer 64 bit word swap.
 * This api makes the first 4 bytes the last in
 * a given 64 bit value and vice-versa.
 *
 * blob [in] The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_64B_word_swap(struct ulp_blob *blob);

/*
 * Perform the blob buffer 64 bit byte swap.
 * This api makes the first byte the last in
 * a given 64 bit value and vice-versa.
 *
 * blob [in] The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_64B_byte_swap(struct ulp_blob *blob);

/*
 * Perform the blob buffer merge.
 * This api makes the src blob merged to the dst blob.
 * The block size and pad size help in padding the dst blob
 *
 * dst [in] The destination blob, the blob to be merged.
 * src [in] The src blob.
 * block_size [in] The size of the block after which padding gets applied.
 * pad [in] The size of the pad to be applied.
 *
 * returns 0 on success.
 */
int32_t
ulp_blob_block_merge(struct ulp_blob *dst, struct ulp_blob *src,
		     uint32_t block_size, uint32_t pad);

/*
 * Append bits from src blob to dst blob.
 * Only works on BE blobs
 *
 * dst [in/out] The destination blob to append to
 *
 * src [in] The src blob to append from
 *
 * src_offset [in] The bit offset from src to start at
 *
 * src_len [in] The number of bits to append to dst
 *
 * returns 0 on success, non-zero on error
 */
int32_t
ulp_blob_append(struct ulp_blob *dst, struct ulp_blob *src,
		uint16_t src_offset, uint16_t src_len);

/*
 * Perform the blob buffer copy.
 * This api makes the src blob merged to the dst blob.
 *
 * dst [in] The destination blob, the blob to be merged.
 * src [in] The src blob.
 *
 * returns 0 on success.
 */
int32_t
ulp_blob_buffer_copy(struct ulp_blob *dst, struct ulp_blob *src);

/*
 * Read data from the operand
 *
 * operand [in] A pointer to a 16 Byte operand
 *
 * val [in/out] The variable to copy the operand to
 *
 * bitlen [in] The number of bits to read into val
 *
 * returns number of bits read, zero on error
 */
uint16_t
ulp_operand_read(uint8_t *operand,
		 uint8_t *val,
		 uint16_t bitlen);

/*
 * Check the buffer is empty
 *
 * buf [in] The buffer
 * size [in] The size of the buffer
 */
int32_t ulp_buffer_is_empty(const uint8_t *buf, uint32_t size);

/* Function to check if bitmap is zero.Return 1 on success */
uint32_t ulp_bitmap_is_zero(uint8_t *bitmap, int32_t size);

/* Function to check if bitmap is ones. Return 1 on success */
uint32_t ulp_bitmap_is_ones(uint8_t *bitmap, int32_t size);

/* Function to check if bitmap is not zero. Return 1 on success */
uint32_t ulp_bitmap_notzero(const uint8_t *bitmap, int32_t size);

/* returns 0 if input is power of 2 */
int32_t ulp_util_is_power_of_2(uint64_t x);
#endif /* _ULP_UTILS_H_ */
