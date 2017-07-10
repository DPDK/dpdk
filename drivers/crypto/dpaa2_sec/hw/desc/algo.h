/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DESC_ALGO_H__
#define __DESC_ALGO_H__

#include "hw/rta.h"
#include "common.h"

/**
 * DOC: Algorithms - Shared Descriptor Constructors
 *
 * Shared descriptors for algorithms (i.e. not for protocols).
 */

/**
 * cnstr_shdsc_snow_f8 - SNOW/f8 (UEA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @cipherdata: pointer to block cipher transform definitions
 * @dir: Cipher direction (DIR_ENC/DIR_DEC)
 * @count: UEA2 count value (32 bits)
 * @bearer: UEA2 bearer ID (5 bits)
 * @direction: UEA2 direction (1 bit)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_snow_f8(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *cipherdata, uint8_t dir,
		    uint32_t count, uint8_t bearer, uint8_t direction)
{
	struct program prg;
	struct program *p = &prg;
	uint32_t ct = count;
	uint8_t br = bearer;
	uint8_t dr = direction;
	uint32_t context[2] = {ct, (br << 27) | (dr << 26)};

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap) {
		PROGRAM_SET_BSWAP(p);

		context[0] = swab32(context[0]);
		context[1] = swab32(context[1]);
	}

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8, OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL, 0, dir);
	LOAD(p, (uintptr_t)context, CONTEXT1, 0, 8, IMMED | COPY);
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1);
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_snow_f9 - SNOW/f9 (UIA2) as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @authdata: pointer to authentication transform definitions
 * @dir: cipher direction (DIR_ENC/DIR_DEC)
 * @count: UEA2 count value (32 bits)
 * @fresh: UEA2 fresh value ID (32 bits)
 * @direction: UEA2 direction (1 bit)
 * @datalen: size of data
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_snow_f9(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *authdata, uint8_t dir, uint32_t count,
		    uint32_t fresh, uint8_t direction, uint32_t datalen)
{
	struct program prg;
	struct program *p = &prg;
	uint64_t ct = count;
	uint64_t fr = fresh;
	uint64_t dr = direction;
	uint64_t context[2];

	context[0] = (ct << 32) | (dr << 26);
	context[1] = fr << 32;

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap) {
		PROGRAM_SET_BSWAP(p);

		context[0] = swab64(context[0]);
		context[1] = swab64(context[1]);
	}
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9, OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL, 0, dir);
	LOAD(p, (uintptr_t)context, CONTEXT2, 0, 16, IMMED | COPY);
	SEQFIFOLOAD(p, BIT_DATA, datalen, CLASS2 | LAST2);
	/* Save lower half of MAC out into a 32-bit sequence */
	SEQSTORE(p, CONTEXT2, 0, 4, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_blkcipher - block cipher transformation
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
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
		      struct alginfo *cipherdata, uint8_t *iv,
		      uint32_t ivlen, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;
	uint32_t iv_off = 0;
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
	SHR_HDR(p, SHR_SERIAL, 1, SC);

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

	if (iv)
		/* IV load, convert size */
		LOAD(p, (uintptr_t)iv, CONTEXT1, iv_off, ivlen, IMMED | COPY);
	else
		/* IV is present first before the actual message */
		SEQLOAD(p, CONTEXT1, iv_off, ivlen, 0);

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
	SHR_HDR(p, SHR_SERIAL, 1, SC);

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
		      struct alginfo *cipherdata, uint8_t dir,
		      uint32_t count, uint8_t bearer, uint8_t direction)
{
	struct program prg;
	struct program *p = &prg;
	uint64_t ct = count;
	uint64_t br = bearer;
	uint64_t dr = direction;
	uint32_t context[2] = { ct, (br << 27) | (dr << 26) };

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap) {
		PROGRAM_SET_BSWAP(p);

		context[0] = swab32(context[0]);
		context[1] = swab32(context[1]);
	}
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQOUTSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_KASUMI, OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL, 0, dir);
	LOAD(p, (uintptr_t)context, CONTEXT1, 0, 8, IMMED | COPY);
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
 * @dir: cipher direction (DIR_ENC/DIR_DEC)
 * @count: count value (32 bits)
 * @fresh: fresh value ID (32 bits)
 * @direction: direction (1 bit)
 * @datalen: size of data
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_kasumi_f9(uint32_t *descbuf, bool ps, bool swap,
		      struct alginfo *authdata, uint8_t dir,
		      uint32_t count, uint32_t fresh, uint8_t direction,
		      uint32_t datalen)
{
	struct program prg;
	struct program *p = &prg;
	uint16_t ctx_offset = 16;
	uint32_t context[6] = {count, direction << 26, fresh, 0, 0, 0};

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap) {
		PROGRAM_SET_BSWAP(p);

		context[0] = swab32(context[0]);
		context[1] = swab32(context[1]);
		context[2] = swab32(context[2]);
	}
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	KEY(p, KEY1, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));
	MATHB(p, SEQINSZ, SUB, MATH2, VSEQINSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_KASUMI, OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL, 0, dir);
	LOAD(p, (uintptr_t)context, CONTEXT1, 0, 24, IMMED | COPY);
	SEQFIFOLOAD(p, BIT_DATA, datalen, CLASS1 | LAST1);
	/* Save output MAC of DWORD 2 into a 32-bit sequence */
	SEQSTORE(p, CONTEXT1, ctx_offset, 4, 0);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_gcm_encap - AES-GCM encap as a shared descriptor
 * @descbuf: pointer to descriptor-under-construction buffer
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
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

	SHR_HDR(p, SHR_SERIAL, 1, SC);

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

	MATHB(p, ZERO, ADD, MATH3, VSEQOUTSZ, 4, 0);

	/* skip assoc data */
	SEQFIFOSTORE(p, SKIP, 0, 0, VLF);

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
 * @cipherdata: pointer to block cipher transform definitions
 *		Valid algorithm values - OP_ALG_ALGSEL_AES ANDed with
 *		OP_ALG_AAI_GCM.
 * @icvsize: integrity check value (ICV) size (truncated or full)
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_gcm_decap(uint32_t *descbuf, bool ps, bool swap,
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

	SHR_HDR(p, SHR_SERIAL, 1, SC);

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

	MATHB(p, ZERO, ADD, MATH3, VSEQOUTSZ, 4, 0);

	/* skip assoc data */
	SEQFIFOSTORE(p, SKIP, 0, 0, VLF);

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

#endif /* __DESC_ALGO_H__ */
