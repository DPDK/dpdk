/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include "ulp_utils.h"
#include "bnxt_tf_common.h"

/*
 * Initialize the regfile structure for writing
 *
 * regfile [in] Ptr to a regfile instance
 *
 * returns 0 on error or 1 on success
 */
uint32_t
ulp_regfile_init(struct ulp_regfile *regfile)
{
	/* validate the arguments */
	if (!regfile) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}
	memset(regfile, 0, sizeof(struct ulp_regfile));
	return 1; /* Success */
}

/*
 * Read a value from the regfile
 *
 * regfile [in] The regfile instance. Must be initialized prior to being used
 *
 * field [in] The field to be read within the regfile.
 *
 * data [in/out]
 *
 * returns size, zero on failure
 */
uint32_t
ulp_regfile_read(struct ulp_regfile *regfile,
		 enum bnxt_ulp_rf_idx field,
		 uint64_t *data)
{
	/* validate the arguments */
	if (!regfile || field >= BNXT_ULP_RF_IDX_LAST) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}

	*data = regfile->entry[field].data;
	return sizeof(*data);
}

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
 * size [in] The size in bytes of the value being written into this
 * variable.
 *
 * returns 0 on success
 */
int32_t
ulp_regfile_write(struct ulp_regfile *regfile,
		  enum bnxt_ulp_rf_idx field,
		  uint64_t data)
{
	/* validate the arguments */
	if (!regfile || field >= BNXT_ULP_RF_IDX_LAST) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -EINVAL; /* failure */
	}

	regfile->entry[field].data = data;
	return 0; /* Success */
}

static void
ulp_bs_put_msb(uint8_t *bs, uint16_t bitpos, uint8_t bitlen, uint8_t val)
{
	uint8_t bitoffs = bitpos % 8;
	uint16_t index  = bitpos / 8;
	uint8_t mask;
	uint8_t tmp;
	int8_t shift;

	tmp = bs[index];
	mask = ((uint8_t)-1 >> (8 - bitlen));
	shift = 8 - bitoffs - bitlen;
	val &= mask;

	if (shift >= 0) {
		tmp &= ~(mask << shift);
		tmp |= val << shift;
		bs[index] = tmp;
	} else {
		tmp &= ~((uint8_t)-1 >> bitoffs);
		tmp |= val >> -shift;
		bs[index++] = tmp;

		tmp = bs[index];
		tmp &= ((uint8_t)-1 >> (bitlen - (8 - bitoffs)));
		tmp |= val << (8 + shift);
		bs[index] = tmp;
	}
}

static void
ulp_bs_put_lsb(uint8_t *bs, uint16_t bitpos, uint8_t bitlen, uint8_t val)
{
	uint8_t bitoffs = bitpos % 8;
	uint16_t index  = bitpos / 8;
	uint8_t mask;
	uint8_t tmp;
	uint8_t shift;
	uint8_t partial;

	tmp = bs[index];
	shift = bitoffs;

	if (bitoffs + bitlen <= 8) {
		mask = ((1 << bitlen) - 1) << shift;
		tmp &= ~mask;
		tmp |= ((val << shift) & mask);
		bs[index] = tmp;
	} else {
		partial = 8 - bitoffs;
		mask = ((1 << partial) - 1) << shift;
		tmp &= ~mask;
		tmp |= ((val << shift) & mask);
		bs[index++] = tmp;

		val >>= partial;
		partial = bitlen - partial;
		mask = ((1 << partial) - 1);
		tmp = bs[index];
		tmp &= ~mask;
		tmp |= (val & mask);
		bs[index] = tmp;
	}
}

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
ulp_bs_push_lsb(uint8_t *bs, uint16_t pos, uint8_t len, uint8_t *val)
{
	int i;
	int cnt = (len) / 8;
	int tlen = len;

	if (cnt > 0 && !(len % 8))
		cnt -= 1;

	for (i = 0; i < cnt; i++) {
		ulp_bs_put_lsb(bs, pos, 8, val[cnt - i]);
		pos += 8;
		tlen -= 8;
	}

	/* Handle the remainder bits */
	if (tlen)
		ulp_bs_put_lsb(bs, pos, tlen, val[0]);
	return len;
}

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
ulp_bs_push_msb(uint8_t *bs, uint16_t pos, uint8_t len, uint8_t *val)
{
	int i;
	int cnt = (len + 7) / 8;

	/* Handle any remainder bits */
	int tmp = len % 8;

	if (!tmp)
		tmp = 8;

	ulp_bs_put_msb(bs, pos, tmp, val[0]);

	pos += tmp;

	for (i = 1; i < cnt; i++) {
		ulp_bs_put_msb(bs, pos, 8, val[i]);
		pos += 8;
	}

	return len;
}

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
 * Notes - If bitlen is zero then set it to max.
 */
uint32_t
ulp_blob_init(struct ulp_blob *blob,
	      uint16_t bitlen,
	      enum bnxt_ulp_byte_order order)
{
	/* validate the arguments */
	if (!blob || bitlen > (8 * sizeof(blob->data))) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}
	if (bitlen)
		blob->bitlen = bitlen;
	else
		blob->bitlen = BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	blob->byte_order = order;
	blob->write_idx = 0;
	memset(blob->data, 0, sizeof(blob->data));
	return 1; /* Success */
}

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
#define ULP_BLOB_BYTE		8
#define ULP_BLOB_BYTE_HEX	0xFF
#define BLOB_MASK_CAL(x)	((0xFF << (x)) & 0xFF)
uint32_t
ulp_blob_push(struct ulp_blob *blob,
	      uint8_t *data,
	      uint32_t datalen)
{
	uint32_t rc;

	/* validate the arguments */
	if (!blob || datalen > (uint32_t)(blob->bitlen - blob->write_idx)) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}

	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		rc = ulp_bs_push_msb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	else
		rc = ulp_bs_push_lsb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	if (!rc) {
		BNXT_TF_DBG(ERR, "Failed to write blob\n");
		return 0;
	}
	blob->write_idx += datalen;
	return datalen;
}

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
		uint8_t *data, uint32_t datalen)
{
	uint32_t rc;
	uint8_t local_data[BNXT_ULP_FLMP_BLOB_SIZE];
	uint16_t mov_len;

	/* validate the arguments */
	if (!blob || datalen > (uint32_t)(blob->bitlen - blob->write_idx) ||
	    offset > blob->write_idx) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}

	mov_len = blob->write_idx - offset;
	/* If offset and data len are not 8 bit aligned then return error */
	if (ULP_BITS_IS_BYTE_NOT_ALIGNED(offset) ||
	    ULP_BITS_IS_BYTE_NOT_ALIGNED(datalen)) {
		BNXT_TF_DBG(ERR, "invalid argument, not aligned\n");
		return 0; /* failure */
	}

	/* copy the data so we can move the data */
	memcpy(local_data, &blob->data[ULP_BITS_2_BYTE_NR(offset)],
	       ULP_BITS_2_BYTE(mov_len));
	blob->write_idx = offset;
	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		rc = ulp_bs_push_msb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	else
		rc = ulp_bs_push_lsb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	if (!rc) {
		BNXT_TF_DBG(ERR, "Failed to write blob\n");
		return 0;
	}
	/* copy the previously stored data */
	memcpy(&blob->data[ULP_BITS_2_BYTE_NR(offset + datalen)], local_data,
	       ULP_BITS_2_BYTE(mov_len));
	blob->write_idx += (mov_len + datalen);
	return datalen;
}

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
 * NULL returned on error, pointer pushed value otherwise.
 */
uint8_t *
ulp_blob_push_64(struct ulp_blob *blob,
		 uint64_t *data,
		 uint32_t datalen)
{
	uint8_t *val = (uint8_t *)data;
	int rc;

	int size = (datalen + 7) / 8;

	if (!blob || !data ||
	    datalen > (uint32_t)(blob->bitlen - blob->write_idx)) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0;
	}

	rc = ulp_blob_push(blob, &val[8 - size], datalen);
	if (!rc)
		return 0;

	return &val[8 - size];
}

/*
 * Add data to the binary blob at the current offset.
 *
 * blob [in] The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * data [in] 32-bit value to be added to the blob.
 *
 * datalen [in] The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
uint8_t *
ulp_blob_push_32(struct ulp_blob *blob,
		 uint32_t *data,
		 uint32_t datalen)
{
	uint8_t *val = (uint8_t *)data;
	uint32_t rc;
	uint32_t size = ULP_BITS_2_BYTE(datalen);

	if (!data || size > sizeof(uint32_t)) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0;
	}

	rc = ulp_blob_push(blob, &val[sizeof(uint32_t) - size], datalen);
	if (!rc)
		return 0;

	return &val[sizeof(uint32_t) - size];
}

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
		    uint32_t datalen)
{
	uint8_t		*val = (uint8_t *)data;
	uint32_t	initial_size, write_size = datalen;
	uint32_t	size = 0;

	if (!blob || !data ||
	    datalen > (uint32_t)(blob->bitlen - blob->write_idx)) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -1;
	}

	initial_size = ULP_BYTE_2_BITS(sizeof(uint64_t)) -
	    (blob->write_idx % ULP_BYTE_2_BITS(sizeof(uint64_t)));
	while (write_size > 0) {
		if (initial_size && write_size > initial_size) {
			size = initial_size;
			initial_size = 0;
		} else if (initial_size && write_size <= initial_size) {
			size = write_size;
			initial_size = 0;
		} else if (write_size > ULP_BYTE_2_BITS(sizeof(uint64_t))) {
			size = ULP_BYTE_2_BITS(sizeof(uint64_t));
		} else {
			size = write_size;
		}
		if (!ulp_blob_push(blob, val, size)) {
			BNXT_TF_DBG(ERR, "push field failed\n");
			return -1;
		}
		val += ULP_BITS_2_BYTE(size);
		write_size -= size;
	}
	return datalen;
}

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
		  uint32_t datalen)
{
	if (datalen > (uint32_t)(blob->bitlen - blob->write_idx)) {
		BNXT_TF_DBG(ERR, "Pad too large for blob\n");
		return -1;
	}

	blob->write_idx += datalen;
	return datalen;
}

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
		   uint32_t align)
{
	int32_t pad = 0;

	pad = RTE_ALIGN(blob->write_idx, align) - blob->write_idx;
	if (pad > (int32_t)(blob->bitlen - blob->write_idx)) {
		BNXT_TF_DBG(ERR, "Pad too large for blob\n");
		return -1;
	}
	blob->write_idx += pad;
	return pad;
}

/* Get data from src and put into dst using little-endian format */
static void
ulp_bs_get_lsb(uint8_t *src, uint16_t bitpos, uint8_t bitlen, uint8_t *dst)
{
	uint8_t bitoffs = bitpos % ULP_BLOB_BYTE;
	uint16_t index  = ULP_BITS_2_BYTE_NR(bitpos);
	uint8_t mask, partial, shift;

	shift = bitoffs;
	partial = ULP_BLOB_BYTE - bitoffs;
	if (bitoffs + bitlen <= ULP_BLOB_BYTE) {
		mask = ((1 << bitlen) - 1) << shift;
		*dst = (src[index] & mask) >> shift;
	} else {
		mask = ((1 << partial) - 1) << shift;
		*dst = (src[index] & mask) >> shift;
		index++;
		partial = bitlen - partial;
		mask = ((1 << partial) - 1);
		*dst |= (src[index] & mask) << (ULP_BLOB_BYTE - bitoffs);
	}
}

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
		uint32_t offset, uint32_t len)
{
	uint32_t idx;
	uint32_t cnt = ULP_BITS_2_BYTE_NR(len);

	/* iterate bytewise to get data */
	for (idx = 0; idx < cnt; idx++) {
		ulp_bs_get_lsb(src, offset, ULP_BLOB_BYTE,
			       &dst[size - 1 - idx]);
		offset += ULP_BLOB_BYTE;
		len -= ULP_BLOB_BYTE;
	}

	/* Extract the last reminder data that is not 8 byte boundary */
	if (len)
		ulp_bs_get_lsb(src, offset, len, &dst[size - 1 - idx]);
}

/* Get data from src and put into dst using big-endian format */
static void
ulp_bs_get_msb(uint8_t *src, uint16_t bitpos, uint8_t bitlen, uint8_t *dst)
{
	uint8_t bitoffs = bitpos % ULP_BLOB_BYTE;
	uint16_t index  = ULP_BITS_2_BYTE_NR(bitpos);
	uint8_t mask;
	int32_t shift;

	shift = ULP_BLOB_BYTE - bitoffs - bitlen;
	if (shift >= 0) {
		mask = 0xFF >> -bitlen;
		*dst = (src[index] >> shift) & mask;
	} else {
		*dst = (src[index] & (0xFF >> bitoffs)) << -shift;
		*dst |= src[index + 1] >> -shift;
	}
}

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
		uint32_t offset, uint32_t len)
{
	uint32_t idx;
	uint32_t cnt = ULP_BITS_2_BYTE_NR(len);

	/* iterate bytewise to get data */
	for (idx = 0; idx < cnt; idx++) {
		ulp_bs_get_msb(src, offset, ULP_BLOB_BYTE, &dst[idx]);
		offset += ULP_BLOB_BYTE;
		len -= ULP_BLOB_BYTE;
	}

	/* Extract the last reminder data that is not 8 byte boundary */
	if (len)
		ulp_bs_get_msb(src, offset, len, &dst[idx]);
}

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
	      uint16_t offset, uint16_t len)
{
	/* validate the arguments */
	if (!blob || (offset + len) > blob->bitlen ||
	    ULP_BYTE_2_BITS(data_size) < len) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return -1; /* failure */
	}

	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		ulp_bs_pull_msb(blob->data, data, offset, len);
	else
		ulp_bs_pull_lsb(blob->data, data, data_size, offset, len);
	return 0;
}

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
		  uint16_t *datalen)
{
	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return NULL; /* failure */
	}
	*datalen = blob->write_idx;
	return blob->data;
}

/*
 * Get the data length of the binary blob.
 *
 * blob [in] The blob's data len to be retrieved.
 *
 * returns length of the binary blob
 */
uint16_t
ulp_blob_data_len_get(struct ulp_blob *blob)
{
	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}
	return blob->write_idx;
}

/*
 * Set the encap swap start index of the binary blob.
 *
 * blob [in] The blob's data to be retrieved. The blob must be
 * initialized prior to pushing data.
 *
 * returns void.
 */
void
ulp_blob_encap_swap_idx_set(struct ulp_blob *blob)
{
	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return; /* failure */
	}
	blob->encap_swap_idx = blob->write_idx;
}

/*
 * Perform the encap buffer swap to 64 bit reversal.
 *
 * blob [in] The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_encap_swap(struct ulp_blob *blob)
{
	uint32_t i, idx = 0, end_idx = 0, roundoff;
	uint8_t temp_val_1, temp_val_2;

	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return; /* failure */
	}
	idx = ULP_BITS_2_BYTE_NR(blob->encap_swap_idx);
	end_idx = ULP_BITS_2_BYTE(blob->write_idx);
	roundoff = ULP_BYTE_2_BITS(ULP_BITS_2_BYTE(end_idx));
	if (roundoff > end_idx) {
		blob->write_idx += ULP_BYTE_2_BITS(roundoff - end_idx);
		end_idx = roundoff;
	}
	while (idx <= end_idx) {
		for (i = 0; i < 4; i = i + 2) {
			temp_val_1 = blob->data[idx + i];
			temp_val_2 = blob->data[idx + i + 1];
			blob->data[idx + i] = blob->data[idx + 6 - i];
			blob->data[idx + i + 1] = blob->data[idx + 7 - i];
			blob->data[idx + 7 - i] = temp_val_2;
			blob->data[idx + 6 - i] = temp_val_1;
		}
		idx += 8;
	}
}

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
			      uint32_t chunk_size)
{
	uint32_t idx = 0, jdx = 0, num = 0;
	uint8_t xchar;
	uint8_t *buff;

	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return; /* failure */
	}

	buff = blob->data;
	num = ULP_BITS_2_BYTE(blob->write_idx) / chunk_size;
	for (idx = 0; idx < num; idx++) {
		for (jdx = 0; jdx < chunk_size / 2; jdx++) {
			xchar = buff[jdx];
			buff[jdx] = buff[(chunk_size - 1) - jdx];
			buff[(chunk_size - 1) - jdx] = xchar;
		}
		buff += chunk_size;
	}
}

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
ulp_blob_perform_64B_word_swap(struct ulp_blob *blob)
{
	uint32_t i, j, num;
	uint8_t xchar;
	uint32_t word_size = ULP_64B_IN_BYTES / 2;

	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return; /* failure */
	}
	num = ULP_BITS_2_BYTE(blob->write_idx);
	for (i = 0; i < num; i = i + ULP_64B_IN_BYTES) {
		for (j = 0; j < word_size; j++) {
			xchar = blob->data[i + j];
			blob->data[i + j] = blob->data[i + j + word_size];
			blob->data[i + j + word_size] = xchar;
		}
	}
}

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
ulp_blob_perform_64B_byte_swap(struct ulp_blob *blob)
{
	uint32_t i, j, num;
	uint8_t xchar;
	uint32_t offset = ULP_64B_IN_BYTES - 1;

	/* validate the arguments */
	if (!blob) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return; /* failure */
	}
	num = ULP_BITS_2_BYTE(blob->write_idx);
	for (i = 0; i < num; i = i + ULP_64B_IN_BYTES) {
		for (j = 0; j < (ULP_64B_IN_BYTES / 2); j++) {
			xchar = blob->data[i + j];
			blob->data[i + j] = blob->data[i + offset - j];
			blob->data[i + offset - j] = xchar;
		}
	}
}

static int32_t
ulp_blob_msb_block_merge(struct ulp_blob *dst, struct ulp_blob *src,
			 uint32_t block_size, uint32_t pad)
{
	uint32_t i, k, write_bytes, remaining;
	uint16_t num;
	uint8_t *src_buf = ulp_blob_data_get(src, &num);
	uint8_t bluff;

	for (i = 0; i < num;) {
		if (((dst->write_idx % block_size)  + (num - i)) > block_size)
			write_bytes = block_size -
				(dst->write_idx % block_size);
		else
			write_bytes = num - i;
		for (k = 0; k < ULP_BITS_2_BYTE_NR(write_bytes); k++) {
			ulp_bs_put_msb(dst->data, dst->write_idx, ULP_BLOB_BYTE,
				       *src_buf);
			dst->write_idx += ULP_BLOB_BYTE;
			src_buf++;
		}
		remaining = write_bytes % ULP_BLOB_BYTE;
		if (remaining) {
			bluff = (*src_buf) & ((uint8_t)-1 <<
					      (ULP_BLOB_BYTE - remaining));
			ulp_bs_put_msb(dst->data, dst->write_idx,
				       ULP_BLOB_BYTE, bluff);
			dst->write_idx += remaining;
		}
		if (write_bytes != (num - i)) {
			/* add the padding */
			ulp_blob_pad_push(dst, pad);
			if (remaining) {
				ulp_bs_put_msb(dst->data, dst->write_idx,
					       ULP_BLOB_BYTE - remaining,
					       *src_buf);
				dst->write_idx += ULP_BLOB_BYTE - remaining;
				src_buf++;
			}
		}
		i += write_bytes;
	}
	return 0;
}

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
		     uint32_t block_size, uint32_t pad)
{
	if (dst->byte_order == BNXT_ULP_BYTE_ORDER_BE &&
	    src->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		return ulp_blob_msb_block_merge(dst, src, block_size, pad);

	BNXT_TF_DBG(ERR, "block merge not implemented yet\n");
	return -EINVAL;
}

int32_t
ulp_blob_append(struct ulp_blob *dst, struct ulp_blob *src,
		uint16_t src_offset, uint16_t src_len)
{
	uint32_t k, remaining;
	uint16_t num;
	uint8_t bluff;
	uint8_t *src_buf = ulp_blob_data_get(src, &num);

	if ((src_offset + src_len) > num)
		return -EINVAL;

	/* Only supporting BE for now */
	if (src->byte_order != BNXT_ULP_BYTE_ORDER_BE ||
	    dst->byte_order != BNXT_ULP_BYTE_ORDER_BE)
		return -EINVAL;

	/* Handle if the source offset is not on a byte boundary */
	remaining = src_offset % ULP_BLOB_BYTE;
	if (remaining) {
		bluff = src_buf[src_offset / ULP_BLOB_BYTE] & ((uint8_t)-1 >>
				      (ULP_BLOB_BYTE - remaining));
		ulp_bs_put_msb(dst->data, dst->write_idx,
			       ULP_BLOB_BYTE, bluff);
		dst->write_idx += remaining;
		src_offset += remaining;
	}

	src_buf += ULP_BITS_2_BYTE_NR(src_offset);

	/* Push the byte aligned pieces */
	for (k = 0; k < ULP_BITS_2_BYTE_NR(src_len); k++) {
		ulp_bs_put_msb(dst->data, dst->write_idx, ULP_BLOB_BYTE,
			       *src_buf);
		dst->write_idx += ULP_BLOB_BYTE;
		src_buf++;
	}

	/* Handle the remaining if length is not a byte boundary */
	remaining = src_len % ULP_BLOB_BYTE;
	if (remaining) {
		bluff = (*src_buf) & ((uint8_t)-1 <<
				      (ULP_BLOB_BYTE - remaining));
		ulp_bs_put_msb(dst->data, dst->write_idx,
			       ULP_BLOB_BYTE, bluff);
		dst->write_idx += remaining;
	}

	return 0;
}

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
ulp_blob_buffer_copy(struct ulp_blob *dst, struct ulp_blob *src)
{
	if ((dst->write_idx + src->write_idx) > dst->bitlen) {
		BNXT_TF_DBG(ERR, "source buffer too large\n");
		return -EINVAL;
	}
	if (ULP_BITS_IS_BYTE_NOT_ALIGNED(dst->write_idx) ||
	    ULP_BITS_IS_BYTE_NOT_ALIGNED(src->write_idx)) {
		BNXT_TF_DBG(ERR, "source buffer is not aligned\n");
		return -EINVAL;
	}
	memcpy(&dst->data[ULP_BITS_2_BYTE_NR(dst->write_idx)],
	       src->data, ULP_BITS_2_BYTE_NR(src->write_idx));
	dst->write_idx += src->write_idx;
	return 0;
}

/*
 * Read data from the operand
 *
 * operand [in] A pointer to a 16 Byte operand
 *
 * val [in/out] The variable to copy the operand to
 *
 * bytes [in] The number of bytes to read into val
 *
 * returns number of bits read, zero on error
 */
uint16_t
ulp_operand_read(uint8_t *operand,
		 uint8_t *val,
		 uint16_t bytes)
{
	/* validate the arguments */
	if (!operand || !val) {
		BNXT_TF_DBG(ERR, "invalid argument\n");
		return 0; /* failure */
	}
	memcpy(val, operand, bytes);
	return bytes;
}

/*
 * Check the buffer is empty
 *
 * buf [in] The buffer
 * size [in] The size of the buffer
 *
 */
int32_t ulp_buffer_is_empty(const uint8_t *buf, uint32_t size)
{
	return buf[0] == 0 && !memcmp(buf, buf + 1, size - 1);
}

/* Function to check if bitmap is zero.Return 1 on success */
uint32_t ulp_bitmap_is_zero(uint8_t *bitmap, int32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return 0;
		bitmap++;
	}
	return 1;
}

/* Function to check if bitmap is ones. Return 1 on success */
uint32_t ulp_bitmap_is_ones(uint8_t *bitmap, int32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0xFF)
			return 0;
		bitmap++;
	}
	return 1;
}

/* Function to check if bitmap is not zero. Return 1 on success */
uint32_t ulp_bitmap_notzero(const uint8_t *bitmap, int32_t size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return 1;
		bitmap++;
	}
	return 0;
}

/* returns 0 if input is power of 2 */
int32_t ulp_util_is_power_of_2(uint64_t x)
{
	if (((x - 1) & x))
		return -1;
	return 0;
}
