/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016,2019-2021 NXP
 *
 */

#ifndef __DESC_ALGO_H__
#define __DESC_ALGO_H__

#include "rta.h"
#include "common.h"

/**
 * DOC: Algorithms - Shared Descriptor Constructors
 *
 * Shared descriptors for algorithms (i.e. not for protocols).
 */

/**
 * cnstr_shdsc_zuce - ZUC Enc (EEA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @cipherdata: pointer to block cipher transform definitions
 * @dir: Cipher direction (DIR_ENC/DIR_DEC)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_zuce(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *cipherdata, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	SEQLOAD(p, CONTEXT1, 0, 16, 0);

	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE, OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL, 0, dir);
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1);
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_zuca - ZUC Auth (EIA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @authdata: pointer to authentication transform definitions
 * @chk_icv: Whether to compare and verify ICV (true/false)
 * @authlen: size of digest
 *
 * The IV prepended before hmac payload must be 8 bytes consisting
 * of COUNT||BEARER||DIR. The COUNT is of 32-bits, bearer is of 5 bits and
 * direction is of 1 bit - totalling to 38 bits.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_zuca(uint32_t *descbuf, bool ps, bool swap,
		 struct alginfo *authdata, uint8_t chk_icv,
		 uint32_t authlen)
{
	struct program prg;
	struct program *p = &prg;
	int dir = chk_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
	    authdata->keylen, INLINE_KEY(authdata));

	SEQLOAD(p, CONTEXT2, 0, 8, 0);

	if (chk_icv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, authlen, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA, OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL, chk_icv, dir);

	SEQFIFOLOAD(p, MSG2, 0, VLF | CLASS2 | LAST2);

	if (chk_icv == ICV_CHECK_ENABLE)
		SEQFIFOLOAD(p, ICV2, authlen, LAST2);
	else
		/* Save lower half of MAC out into a 32-bit sequence */
		SEQSTORE(p, CONTEXT2, 0, authlen, 0);

	return PROGRAM_FINALIZE(p);
}


/**
 * cnstr_shdsc_snow_f8 - SNOW/f8 (UEA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @cipherdata: pointer to block cipher transform definitions
 * @dir: Cipher direction (DIR_ENC/DIR_DEC)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_snow_f8(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *cipherdata, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	SEQLOAD(p, CONTEXT1, 0, 16, 0);

	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8, OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL, 0, dir);
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1);
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	return PROGRAM_FINALIZE(p);
}

/**
 * conv_to_zuc_eia_iv - ZUCA IV 16-byte to 8-byte convert
 * function for 3G.
 * @iv: 16 bytes of original IV data.
 *
 * From the original IV, we extract 32-bits of COUNT,
 * 5-bits of bearer and 1-bit of direction.
 * Refer to CAAM refman for ZUCA IV format. Then these values are
 * appended as COUNT||BEARER||DIR continuously to make a 38-bit block.
 * This 38-bit block is copied left justified into 8-byte array used as
 * converted IV.
 *
 * Return: 8-bytes of IV data as understood by SEC HW
 */

static inline uint8_t *conv_to_zuc_eia_iv(uint8_t *iv)
{
	uint8_t dir = (iv[14] & 0x80) ? 4 : 0;

	iv[12] = iv[4] | dir;
	iv[13] = 0;
	iv[14] = 0;
	iv[15] = 0;

	iv[8] = iv[0];
	iv[9] = iv[1];
	iv[10] = iv[2];
	iv[11] = iv[3];

	return (iv + 8);
}

/**
 * conv_to_snow_f9_iv - SNOW/f9 (UIA2) IV 16 byte to 12 byte convert
 * function for 3G.
 * @iv: 16 byte original IV data
 *
 * Return: 12 byte IV data as understood by SEC HW
 */

static inline uint8_t *conv_to_snow_f9_iv(uint8_t *iv)
{
	uint8_t temp = (iv[8] == iv[0]) ? 0 : 4;

	iv[12] = iv[4];
	iv[13] = iv[5];
	iv[14] = iv[6];
	iv[15] = iv[7];

	iv[8] = temp;
	iv[9] = 0x00;
	iv[10] = 0x00;
	iv[11] = 0x00;

	iv[4] = iv[0];
	iv[5] = iv[1];
	iv[6] = iv[2];
	iv[7] = iv[3];

	return (iv + 4);
}

/**
 * cnstr_shdsc_snow_f9 - SNOW/f9 (UIA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @authdata: pointer to authentication transform definitions
 * @chk_icv: check or generate ICV value
 * @authlen: size of digest
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_snow_f9(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *authdata, uint8_t chk_icv,
		    uint32_t authlen)
{
	struct program prg;
	struct program *p = &prg;
	int dir = chk_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
	    authdata->keylen, INLINE_KEY(authdata));

	SEQLOAD(p, CONTEXT2, 0, 12, 0);

	if (chk_icv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, authlen, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9, OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL, chk_icv, dir);

	SEQFIFOLOAD(p, MSG2, 0, VLF | CLASS2 | LAST2);

	if (chk_icv == ICV_CHECK_ENABLE)
		SEQFIFOLOAD(p, ICV2, authlen, LAST2);
	else
		/* Save lower half of MAC out into a 32-bit sequence */
		SEQSTORE(p, CONTEXT2, 0, authlen, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_blkcipher - block cipher transformation
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values one of OP_ALG_ALGSEL_* {DES, 3DES, AES}
 *              Valid modes for:
 *                  AES: OP_ALG_AAI_* {CBC, CTR}
 *                  DES, 3DES: OP_ALG_AAI_CBC
 * @iv: IV data; if NULL, "ivlen" bytes from the input frame will be read as IV
 * @ivlen: IV length
 * @dir: DIR_ENC/DIR_DEC
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_blkcipher(uint32_t *descbuf, bool ps, bool swap,
		      enum rta_share_type share,
		      struct alginfo *cipherdata,
		      uint32_t ivlen, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;
	uint32_t iv_off = 0, counter;
	const bool need_dk = (dir == DIR_DEC) &&
			     (cipherdata->algtype == OP_ALG_ALGSEL_AES) &&
			     (cipherdata->algmode == OP_ALG_AAI_CBC);
	LABEL(keyjmp);
	LABEL(skipdk);
	REFERENCE(pkeyjmp);
	REFERENCE(pskipdk);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, share, 1, SC);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SHRD);
	/* Insert Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	if (need_dk) {
		ALG_OPERATION(p, cipherdata->algtype, cipherdata->algmode,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, dir);

		pskipdk = JUMP(p, skipdk, LOCAL_JUMP, ALL_TRUE, 0);
	}
	SET_LABEL(p, keyjmp);

	if (need_dk) {
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, cipherdata->algmode |
			      OP_ALG_AAI_DK, OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE, dir);
		SET_LABEL(p, skipdk);
	} else {
		ALG_OPERATION(p, cipherdata->algtype, cipherdata->algmode,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, dir);
	}

	if (cipherdata->algmode == OP_ALG_AAI_CTR)
		iv_off = 16;

	/* IV is present first before the actual message */
	SEQLOAD(p, CONTEXT1, iv_off, ivlen, 0);

	/* If IV len is less than 16 bytes, set 'counter' as 1 */
	if (cipherdata->algmode == OP_ALG_AAI_CTR && ivlen < 16) {
		counter = 1;
		if (!swap)
			counter = swab32(1);

		LOAD(p, counter, CONTEXT1, (iv_off + ivlen), 16 - ivlen, IMMED);
	}

	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);

	/* Insert sequence load/store with VLF */
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1);
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	PATCH_JUMP(p, pkeyjmp, keyjmp);
	if (need_dk)
		PATCH_JUMP(p, pskipdk, skipdk);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_hmac - HMAC shared
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @authdata: pointer to authentication transform definitions;
 *            message digest algorithm: OP_ALG_ALGSEL_MD5/ SHA1-512.
 * @do_icv: 0 if ICV checking is not desired, any other value if ICV checking
 *          is needed for all the packets processed by this shared descriptor
 * @trunc_len: Length of the truncated ICV to be written in the output buffer, 0
 *             if no truncation is needed
 *
 * Note: There's no support for keys longer than the block size of the
 * underlying hash function, according to the selected algorithm.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_hmac(uint32_t *descbuf, bool ps, bool swap,
		 enum rta_share_type share,
		 struct alginfo *authdata, uint8_t do_icv,
		 uint8_t trunc_len)
{
	struct program prg;
	struct program *p = &prg;
	uint8_t storelen, opicv, dir;
	LABEL(keyjmp);
	LABEL(jmpprecomp);
	REFERENCE(pkeyjmp);
	REFERENCE(pjmpprecomp);

	/* Compute fixed-size store based on alg selection */
	switch (authdata->algtype) {
	case OP_ALG_ALGSEL_MD5:
		storelen = 16;
		break;
	case OP_ALG_ALGSEL_SHA1:
		storelen = 20;
		break;
	case OP_ALG_ALGSEL_SHA224:
		storelen = 28;
		break;
	case OP_ALG_ALGSEL_SHA256:
		storelen = 32;
		break;
	case OP_ALG_ALGSEL_SHA384:
		storelen = 48;
		break;
	case OP_ALG_ALGSEL_SHA512:
		storelen = 64;
		break;
	default:
		return -EINVAL;
	}

	trunc_len = trunc_len && (trunc_len < storelen) ? trunc_len : storelen;

	opicv = do_icv ? ICV_CHECK_ENABLE : ICV_CHECK_DISABLE;
	dir = do_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, share, 1, SC);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SHRD);
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	/* Do operation */
	ALG_OPERATION(p, authdata->algtype, OP_ALG_AAI_HMAC,
		      OP_ALG_AS_INITFINAL, opicv, dir);

	pjmpprecomp = JUMP(p, jmpprecomp, LOCAL_JUMP, ALL_TRUE, 0);
	SET_LABEL(p, keyjmp);

	ALG_OPERATION(p, authdata->algtype, OP_ALG_AAI_HMAC_PRECOMP,
		      OP_ALG_AS_INITFINAL, opicv, dir);

	SET_LABEL(p, jmpprecomp);

	/* compute sequences */
	if (opicv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, trunc_len, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);

	/* Do load (variable length) */
	SEQFIFOLOAD(p, MSG2, 0, VLF | LAST2);

	if (opicv == ICV_CHECK_ENABLE)
		SEQFIFOLOAD(p, ICV2, trunc_len, LAST2);
	else
		SEQSTORE(p, CONTEXT2, 0, trunc_len, 0);

	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_JUMP(p, pjmpprecomp, jmpprecomp);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_hash - HASH shared
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @authdata: pointer to authentication transform definitions;
 *            message digest algorithm: OP_ALG_ALGSEL_MD5/ SHA1-512.
 * @do_icv: 0 if ICV checking is not desired, any other value if ICV checking
 *          is needed for all the packets processed by this shared descriptor
 * @trunc_len: Length of the truncated ICV to be written in the output buffer, 0
 *             if no truncation is needed
 *
 * Note: There's no support for keys longer than the block size of the
 * underlying hash function, according to the selected algorithm.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_hash(uint32_t *descbuf, bool ps, bool swap,
		 enum rta_share_type share,
		 struct alginfo *authdata, uint8_t do_icv,
		 uint8_t trunc_len)
{
	struct program prg;
	struct program *p = &prg;
	uint8_t storelen, opicv, dir;

	/* Compute fixed-size store based on alg selection */
	switch (authdata->algtype) {
	case OP_ALG_ALGSEL_MD5:
		storelen = 16;
		break;
	case OP_ALG_ALGSEL_SHA1:
		storelen = 20;
		break;
	case OP_ALG_ALGSEL_SHA224:
		storelen = 28;
		break;
	case OP_ALG_ALGSEL_SHA256:
		storelen = 32;
		break;
	case OP_ALG_ALGSEL_SHA384:
		storelen = 48;
		break;
	case OP_ALG_ALGSEL_SHA512:
		storelen = 64;
		break;
	default:
		return -EINVAL;
	}

	trunc_len = trunc_len && (trunc_len < storelen) ? trunc_len : storelen;

	opicv = do_icv ? ICV_CHECK_ENABLE : ICV_CHECK_DISABLE;
	dir = do_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, share, 1, SC);

	/* Do operation */
	/* compute sequences */
	if (opicv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, trunc_len, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);

	ALG_OPERATION(p, authdata->algtype,
		      OP_ALG_AAI_HASH,
		      OP_ALG_AS_INITFINAL, opicv, dir);
	SEQFIFOLOAD(p, MSG2, 0, VLF | LAST2);

	if (opicv == ICV_CHECK_ENABLE)
		SEQFIFOLOAD(p, ICV2, trunc_len, LAST2);
	else
		SEQSTORE(p, CONTEXT2, 0, trunc_len, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_kasumi_f8 - KASUMI F8 (Confidentiality) as a shared descriptor
 *                         (ETSI "Document 1: f8 and f9 specification")
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @cipherdata: pointer to block cipher transform definitions
 * @dir: cipher direction (DIR_ENC/DIR_DEC)
 * @count: count value (32 bits)
 * @bearer: bearer ID (5 bits)
 * @direction: direction (1 bit)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_kasumi_f8(uint32_t *descbuf, bool ps, bool swap,
		      struct alginfo *cipherdata, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	SEQLOAD(p, CONTEXT1, 0, 8, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_KASUMI, OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL, 0, dir);
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1);
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_kasumi_f9 -  KASUMI F9 (Integrity) as a shared descriptor
 *                          (ETSI "Document 1: f8 and f9 specification")
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @authdata: pointer to authentication transform definitions
 * @chk_icv: check or generate ICV value
 * @authlen: size of digest
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_kasumi_f9(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *authdata, uint8_t chk_icv,
		    uint32_t authlen)
{
	struct program prg;
	struct program *p = &prg;
	int dir = chk_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
	    authdata->keylen, INLINE_KEY(authdata));

	SEQLOAD(p, CONTEXT2, 0, 12, 0);

	if (chk_icv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, authlen, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

	ALG_OPERATION(p, OP_ALG_ALGSEL_KASUMI, OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL, chk_icv, dir);

	SEQFIFOLOAD(p, MSG2, 0, VLF | CLASS2 | LAST2);

	if (chk_icv == ICV_CHECK_ENABLE)
		SEQFIFOLOAD(p, ICV2, authlen, LAST2);
	else
		/* Save lower half of MAC out into a 32-bit sequence */
		SEQSTORE(p, CONTEXT2, 0, authlen, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_crc - CRC32 Accelerator (IEEE 802 CRC32 protocol mode)
 * @descbuf: pointer to descriptor-under-construction buffer
 * @swap: must be true when core endianness doesn't match SEC endianness
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_crc(uint32_t *descbuf, bool swap)
{
	struct program prg;
	struct program *p = &prg;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);

	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_CRC,
		      OP_ALG_AAI_802 | OP_ALG_AAI_DOC,
		      OP_ALG_AS_FINALIZE, 0, DIR_ENC);
	SEQFIFOLOAD(p, MSG2, 0, VLF | LAST2);
	SEQSTORE(p, CONTEXT2, 0, 4, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_gcm_encap - AES-GCM encap as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @cipherdata: pointer to block cipher transform definitions
 *		Valid algorithm values - OP_ALG_ALGSEL_AES ANDed with
 *		OP_ALG_AAI_GCM.
 * @ivlen: Initialization vector length
 * @icvsize: integrity check value (ICV) size (truncated or full)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_gcm_encap(uint32_t *descbuf, bool ps, bool swap,
		      enum rta_share_type share,
		      struct alginfo *cipherdata,
		      uint32_t ivlen, uint32_t icvsize)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	LABEL(zeroassocjump2);
	LABEL(zeroassocjump1);
	LABEL(zeropayloadjump);
	REFERENCE(pkeyjmp);
	REFERENCE(pzeroassocjump2);
	REFERENCE(pzeroassocjump1);
	REFERENCE(pzeropayloadjump);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);

	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, share, 1, SC);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SELF | SHRD);
	/* Insert Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	SET_LABEL(p, keyjmp);

	/* class 1 operation */
	ALG_OPERATION(p, cipherdata->algtype, cipherdata->algmode,
		      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_ENC);

	MATHB(p, DPOVRD, AND, 0x7fffffff, MATH3, 4, IMMED2);

	/* if assoclen + cryptlen is ZERO, skip to ICV write */
	MATHB(p, SEQINSZ, SUB, ivlen, VSEQOUTSZ, 4, IMMED2);
	pzeroassocjump2 = JUMP(p, zeroassocjump2, LOCAL_JUMP, ALL_TRUE, MATH_Z);

	SEQFIFOLOAD(p, IV1, ivlen, FLUSH1);

	/* if assoclen is ZERO, skip reading the assoc data */
	MATHB(p, ZERO, ADD, MATH3, VSEQINSZ, 4, 0);
	pzeroassocjump1 = JUMP(p, zeroassocjump1, LOCAL_JUMP, ALL_TRUE, MATH_Z);

	/* cryptlen = seqinlen - assoclen */
	MATHB(p, SEQINSZ, SUB, MATH3, VSEQOUTSZ, 4, 0);

	/* if cryptlen is ZERO jump to zero-payload commands */
	pzeropayloadjump = JUMP(p, zeropayloadjump, LOCAL_JUMP, ALL_TRUE,
				MATH_Z);

	/* read assoc data */
	SEQFIFOLOAD(p, AAD1, 0, CLASS1 | VLF | FLUSH1);
	SET_LABEL(p, zeroassocjump1);

	MATHB(p, SEQINSZ, SUB, MATH0, VSEQINSZ, 4, 0);

	/* write encrypted data */
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	/* read payload data */
	SEQFIFOLOAD(p, MSG1, 0, CLASS1 | VLF | LAST1);

	/* jump the zero-payload commands */
	JUMP(p, 4, LOCAL_JUMP, ALL_TRUE, 0);

	/* zero-payload commands */
	SET_LABEL(p, zeropayloadjump);

	/* read assoc data */
	SEQFIFOLOAD(p, AAD1, 0, CLASS1 | VLF | LAST1);

	JUMP(p, 2, LOCAL_JUMP, ALL_TRUE, 0);

	/* There is no input data */
	SET_LABEL(p, zeroassocjump2);

	SEQFIFOLOAD(p, IV1, ivlen, FLUSH1 | LAST1);

	/* write ICV */
	SEQSTORE(p, CONTEXT1, 0, icvsize, 0);

	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_JUMP(p, pzeroassocjump2, zeroassocjump2);
	PATCH_JUMP(p, pzeroassocjump1, zeroassocjump1);
	PATCH_JUMP(p, pzeropayloadjump, zeropayloadjump);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_gcm_decap - AES-GCM decap as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @cipherdata: pointer to block cipher transform definitions
 *		Valid algorithm values - OP_ALG_ALGSEL_AES ANDed with
 *		OP_ALG_AAI_GCM.
 * @icvsize: integrity check value (ICV) size (truncated or full)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_gcm_decap(uint32_t *descbuf, bool ps, bool swap,
		      enum rta_share_type share,
		      struct alginfo *cipherdata,
		      uint32_t ivlen, uint32_t icvsize)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	LABEL(zeroassocjump1);
	LABEL(zeropayloadjump);
	REFERENCE(pkeyjmp);
	REFERENCE(pzeroassocjump1);
	REFERENCE(pzeropayloadjump);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);

	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, share, 1, SC);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SELF | SHRD);
	/* Insert Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	SET_LABEL(p, keyjmp);

	/* class 1 operation */
	ALG_OPERATION(p, cipherdata->algtype, cipherdata->algmode,
		      OP_ALG_AS_INITFINAL, ICV_CHECK_ENABLE, DIR_DEC);

	MATHB(p, DPOVRD, AND, 0x7fffffff, MATH3, 4, IMMED2);
	SEQFIFOLOAD(p, IV1, ivlen, FLUSH1);

	/* if assoclen is ZERO, skip reading the assoc data */
	MATHB(p, ZERO, ADD, MATH3, VSEQINSZ, 4, 0);
	pzeroassocjump1 = JUMP(p, zeroassocjump1, LOCAL_JUMP, ALL_TRUE, MATH_Z);

	/* read assoc data */
	SEQFIFOLOAD(p, AAD1, 0, CLASS1 | VLF | FLUSH1);

	SET_LABEL(p, zeroassocjump1);

	/* cryptlen = seqoutlen - assoclen */
	MATHB(p, SEQOUTSZ, SUB, MATH0, VSEQINSZ, 4, 0);

	/* jump to zero-payload command if cryptlen is zero */
	pzeropayloadjump = JUMP(p, zeropayloadjump, LOCAL_JUMP, ALL_TRUE,
				MATH_Z);

	MATHB(p, SEQOUTSZ, SUB, MATH0, VSEQOUTSZ, 4, 0);

	/* store encrypted data */
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	/* read payload data */
	SEQFIFOLOAD(p, MSG1, 0, CLASS1 | VLF | FLUSH1);

	/* zero-payload command */
	SET_LABEL(p, zeropayloadjump);

	/* read ICV */
	SEQFIFOLOAD(p, ICV1, icvsize, CLASS1 | LAST1);

	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_JUMP(p, pzeroassocjump1, zeroassocjump1);
	PATCH_JUMP(p, pzeropayloadjump, zeropayloadjump);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_aes_mac - AES_XCBC_MAC, CMAC cases
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @share: sharing type of shared descriptor
 * @authdata: pointer to authentication transform definitions;
 *		   message digest algorithm: OP_ALG_ALGSEL_AES.
 * @do_icv: 0 if ICV checking is not desired, any other value if ICV checking
 *          is needed for all the packets processed by this shared descriptor
 * @trunc_len: Length of the truncated ICV to be written in the output buffer,
 *             0 if no truncation is needed
 *
 * Note: There's no support for keys longer than the block size of the
 * underlying hash function, according to the selected algorithm.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_aes_mac(uint32_t *descbuf, bool ps, bool swap,
		enum rta_share_type share,
		struct alginfo *authdata, uint8_t do_icv,
		uint8_t trunc_len)
{
	struct program prg;
	struct program *p = &prg;
	uint8_t opicv, dir;

	opicv = do_icv ? ICV_CHECK_ENABLE : ICV_CHECK_DISABLE;
	dir = do_icv ? DIR_DEC : DIR_ENC;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, share, 1, SC);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
		INLINE_KEY(authdata));

	/* compute sequences */
	if (opicv == ICV_CHECK_ENABLE)
		MATHB(p, SEQINSZ, SUB, trunc_len, VSEQINSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);

	/* Do operation */
	ALG_OPERATION_NP(p, authdata->algtype, authdata->algmode,
		OP_ALG_AS_INITFINAL, opicv, dir);

	/* Do load (variable length) */
	SEQFIFOLOAD(p, MSG2, 0, VLF | LAST2);

	if (opicv == ICV_CHECK_ENABLE) {
		LOAD(p, trunc_len, ICV2SZ, 0, 4, IMMED);
		SEQFIFOLOAD(p, ICV2, trunc_len, LAST2);
	} else
		SEQSTORE(p, CONTEXT2, 0, trunc_len, 0);

	return PROGRAM_FINALIZE(p);
}

#endif /* __DESC_ALGO_H__ */
