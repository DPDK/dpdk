/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2022 NXP
 */

#ifndef __DESC_SDAP_H__
#define __DESC_SDAP_H__

#include "rta.h"
#include "common.h"
#include "pdcp.h"

/* The file defines all the functions to do PDCP without protocol support in
 * SEC
 */

/* Enable SDAP support */
#define SDAP_SUPPORT
#ifdef SDAP_SUPPORT
#define SDAP_BYTE_SIZE 1
#define SDAP_BITS_SIZE (SDAP_BYTE_SIZE * 8)
#endif

/**
 * rta_inline_pdcp_query() - Provide indications if a key can be passed as
 *                           immediate data or shall be referenced in a
 *                           shared descriptor.
 * Return: 0 if data can be inlined or 1 if referenced.
 */
static inline int
rta_inline_pdcp_sdap_query(enum auth_type_pdcp auth_alg,
		      enum cipher_type_pdcp cipher_alg,
		      __rte_unused enum pdcp_sn_size sn_size,
		      __rte_unused int8_t hfn_ovd)
{
	if ((cipher_alg != PDCP_CIPHER_TYPE_NULL) &&
			(auth_alg != PDCP_AUTH_TYPE_NULL))
		return 2;
	else
		return 0;
}

static inline void key_loading_opti(struct program *p,
				    struct alginfo *cipherdata,
				    struct alginfo *authdata)
{
	LABEL(lbl_skip_key_loading_jump);
	REFERENCE(ref_skip_key_loading_jump);

	/* Optimisation to bypass key loading (and decryption of the keys):
	 * Jump command testing:
	 * - SHRD: Descriptor is shared
	 * - SELF: The shared descriptor is in the same DECO
	 * - BOTH: The Class 1 and 2 CHA have finished
	 * -> If this is true, we jump and skip loading of the keys as they are
	 *    already loaded
	 */
	ref_skip_key_loading_jump =
		JUMP(p, lbl_skip_key_loading_jump, LOCAL_JUMP, ALL_TRUE,
		     SHRD | SELF | BOTH);

	/* Load the keys */
	if (cipherdata) {
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
	}

	if (authdata) {
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
	}

	/* Save the place where we want the jump to go */
	SET_LABEL(p, lbl_skip_key_loading_jump);
	/* Update the jump command with the position where to jump */
	PATCH_JUMP(p, ref_skip_key_loading_jump, lbl_skip_key_loading_jump);
}

static inline int pdcp_sdap_get_sn_parameters(enum pdcp_sn_size sn_size,
					      bool swap, uint32_t *offset,
					      uint32_t *length,
					      uint32_t *sn_mask)
{
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		*offset = 7;
		*length = 1;
		*sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					     PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
		*offset = 7;
		*length = 1;
		*sn_mask = (swap == false) ? PDCP_7BIT_SN_MASK :
					     PDCP_7BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_12:
		*offset = 6;
		*length = 2;
		*sn_mask = (swap == false) ? PDCP_12BIT_SN_MASK :
					     PDCP_12BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_15:
		*offset = 6;
		*length = 2;
		*sn_mask = (swap == false) ? PDCP_U_PLANE_15BIT_SN_MASK :
					     PDCP_U_PLANE_15BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		*offset = 5;
		*length = 3;
		*sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					     PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	default:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

#ifdef SDAP_SUPPORT
	*length += SDAP_BYTE_SIZE;
	*offset -= SDAP_BYTE_SIZE;
#endif

	return 0;
}

static inline int pdcp_sdap_insert_no_int_op(struct program *p,
					     bool swap __maybe_unused,
					     struct alginfo *cipherdata,
					     unsigned int dir,
					     enum pdcp_sn_size sn_size,
					     enum pdb_type_e pdb_type)
{
	int op;
	uint32_t sn_mask = 0;
	uint32_t length = 0;
	uint32_t offset = 0;
	int hfn_bearer_dir_offset_in_descbuf =
		(pdb_type == PDCP_PDB_TYPE_FULL_PDB) ?
			FULL_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET :
			REDUCED_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET;

	if (pdcp_sdap_get_sn_parameters(sn_size, swap, &offset, &length,
					&sn_mask))
		return -ENOTSUP;

	/* Load key */
	key_loading_opti(p, cipherdata, NULL);

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
#ifdef SDAP_SUPPORT
	rta_mathi(p, MATH0,
		  ((swap == true) ? MATH_FUN_RSHIFT : MATH_FUN_LSHIFT),
		  SDAP_BITS_SIZE, MATH1, 8, 0);
	MATHB(p, MATH1, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#else
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#endif

	SEQSTORE(p, MATH0, offset, length, 0);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, hfn_bearer_dir_offset_in_descbuf,
			MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);

	MATHB(p, SEQINSZ, SUB, MATH3, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH3, VSEQOUTSZ, 4, 0);

	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	op = dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC;
	switch (cipherdata->algtype) {
	case PDCP_CIPHER_TYPE_SNOW:
		/* Copy the IV */
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, WAITCOMP | IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8, OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, op);
		break;

	case PDCP_CIPHER_TYPE_AES:
		/* The first 64 bits are 0 */
		MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, WAITCOMP | IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, op);
		break;

	case PDCP_CIPHER_TYPE_ZUC:
		/* The LSB and MSB is the same for ZUC context */
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 0x08, IMMED);
		MOVEB(p, MATH2, 0, CONTEXT1, 0x08, 0x08, WAITCOMP | IMMED);

		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE, OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, op);
		break;

	default:
		pr_err("%s: Invalid encrypt algorithm selected: %d\n",
		       "pdcp_sdap_insert_15bit_op", cipherdata->algtype);
		return -EINVAL;
	}

	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

	return 0;
}

static inline int
pdcp_sdap_insert_enc_only_op(struct program *p, bool swap __maybe_unused,
			     struct alginfo *cipherdata,
			     struct alginfo *authdata __maybe_unused,
			     unsigned int dir, enum pdcp_sn_size sn_size,
			     enum pdb_type_e pdb_type)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	int hfn_bearer_dir_offset_in_descbuf =
		(pdb_type == PDCP_PDB_TYPE_FULL_PDB) ?
			FULL_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET :
			REDUCED_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET;

	if (pdcp_sdap_get_sn_parameters(sn_size, swap, &offset, &length,
					&sn_mask))
		return -ENOTSUP;

	/* Load key */
	key_loading_opti(p, cipherdata, NULL);

	/* Load header */
	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);

#ifdef SDAP_SUPPORT
	rta_mathi(p, MATH0,
		  ((swap == true) ? MATH_FUN_RSHIFT : MATH_FUN_LSHIFT),
		  SDAP_BITS_SIZE, MATH1, 8, 0);
	MATHB(p, MATH1, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#else
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#endif

	/* Word (32 bit) swap */
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	/* Load words from PDB: word 02 (HFN) + word 03 (bearer_dir)*/
	MOVEB(p, DESCBUF, hfn_bearer_dir_offset_in_descbuf,
			MATH2, 0, 8, WAITCOMP | IMMED);
	/* Create basic IV */
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);

	/* Write header */
	SEQSTORE(p, MATH0, offset, length, 0);

	MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

	switch (cipherdata->algtype) {
	case PDCP_CIPHER_TYPE_SNOW:
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, WAITCOMP | IMMED);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8, OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC :
							      DIR_DEC);
		break;

	case PDCP_CIPHER_TYPE_AES:
		MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, WAITCOMP | IMMED);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC :
							      DIR_DEC);
		break;

	case PDCP_CIPHER_TYPE_ZUC:
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 0x08, IMMED);
		MOVEB(p, MATH2, 0, CONTEXT1, 0x08, 0x08, WAITCOMP | IMMED);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE, OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC :
							      DIR_DEC);
		break;

	default:
		pr_err("%s: Invalid encrypt algorithm selected: %d\n",
		       "pdcp_sdap_insert_enc_only_op", cipherdata->algtype);
		return -EINVAL;
	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOLOAD(p, MSG1, 0, VLF);
		FIFOLOAD(p, MSG1, PDCP_NULL_INT_MAC_I_VAL, 4,
			 LAST1 | FLUSH1 | IMMED);
	} else {
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);
		MOVE(p, OFIFO, 0, MATH1, 4, PDCP_MAC_I_LEN, WAITCOMP | IMMED);
		MATHB(p, MATH1, XOR, PDCP_NULL_INT_MAC_I_VAL, NONE, 4, IMMED2);
		JUMP(p, PDCP_NULL_INT_ICV_CHECK_FAILED_STATUS, HALT_STATUS,
		     ALL_FALSE, MATH_Z);
	}

	return 0;
}

/*
 * This function leverage the use of in/out snooping as SNOW and ZUC both
 * have a class 1 and class 2 CHA. It also supports AES as cipher.
 * Supported:
 *  - cipher:
 *      - AES-CTR
 *      - SNOW F8
 *      - ZUC F8
 *  - authentication
 *      - SNOW F8
 *      - ZUC F8
 */
static inline int
pdcp_sdap_insert_snoop_op(struct program *p, bool swap __maybe_unused,
			  struct alginfo *cipherdata, struct alginfo *authdata,
			  unsigned int dir, enum pdcp_sn_size sn_size,
			  enum pdb_type_e pdb_type)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	uint32_t int_op_alg = 0;
	uint32_t int_op_aai = 0;
	uint32_t cipher_op_alg = 0;
	uint32_t cipher_op_aai = 0;
	int hfn_bearer_dir_offset_in_descbuf =
		(pdb_type == PDCP_PDB_TYPE_FULL_PDB) ?
			FULL_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET :
			REDUCED_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET;

	if (pdcp_sdap_get_sn_parameters(sn_size, swap, &offset, &length,
					&sn_mask))
		return -ENOTSUP;

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, SUB, length, VSEQINSZ, 4, IMMED2);

	key_loading_opti(p, cipherdata, authdata);

	/* Load the PDCP header from the input data
	 * Note: SEQINSZ is decremented by length
	 */
	SEQLOAD(p, MATH0, offset, length, 0);
	/* Wait the SN is loaded */
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);

	/* Pass the PDCP header to integrity block */
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);

#ifdef SDAP_SUPPORT
	/* If SDAP is enabled, the least significant byte is the SDAP header
	 * Remove it by shifting the register
	 */
	rta_mathi(p, MATH0,
		  ((swap == true) ? MATH_FUN_RSHIFT : MATH_FUN_LSHIFT),
		  SDAP_BITS_SIZE, MATH1, 8, 0);
	/* Mask the PDCP header to keep only the SN */
	MATHB(p, MATH1, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#else
	/* Mask the PDCP header to keep only the SN */
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#endif

	/* Do a byte swap, it places the SN in upper part of the MATH reg */
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

	/* Load the HFN / Beare / Dir from the PDB
	 * CAAM word are 32bit hence loading 8 byte loads 2 words:
	 *  - The HFN at offset hfn_bearer_dir_offset_in_descbuf
	 *  - The Bearer / Dir at next word
	 */
	MOVEB(p, DESCBUF, hfn_bearer_dir_offset_in_descbuf,
			MATH2, 0, 8, WAITCOMP | IMMED);

	/* Create the 4 first byte of the ICV by or-ing the math registers */
	MATHB(p, MATH1, OR, MATH2, MATH1, 8, 0);

	/* Set the IV of class 1 CHA */
	if (cipherdata->algtype == PDCP_CIPHER_TYPE_AES) {
		MOVEB(p, MATH1, 0, CONTEXT1, 16, 8, IMMED);
	} else {
		/* Set the IV for the confidentiality CHA */
		MOVEB(p, MATH1, 0, CONTEXT1, 0, 8, IMMED);
	}

	/* Set the IV of class 2 CHA */
	if (authdata->algtype == PDCP_AUTH_TYPE_ZUC) {
		/* Set the IV for the integrity CHA */
		MOVEB(p, MATH1, 0, CONTEXT2, 0, 8, WAITCOMP | IMMED);
	} else if (authdata->algtype == PDCP_AUTH_TYPE_SNOW) {
		MOVEB(p, MATH1, 0, CONTEXT2, 0, 4, WAITCOMP | IMMED);

		/* Generate the bottom snow IV for integrity
		 * Note: MATH1 lowest 32bits is as follow:
		 * | bearer (5) | Dir (1) | zero (26) |
		 * the resulting math regs will be:
		 *               MATH3                           MATH2
		 * | zero (5) | Dir (1) | zero (26) | | Bearer (5) | zero (27) |
		 */
		if (swap == false) {
			MATHB(p, MATH1, AND, upper_32_bits(PDCP_BEARER_MASK),
			      MATH2, 4, IMMED2);
			MATHB(p, MATH1, AND, lower_32_bits(PDCP_DIR_MASK),
			      MATH3, 4, IMMED2);
		} else {
			MATHB(p, MATH1, AND, lower_32_bits(PDCP_BEARER_MASK_BE),
			      MATH2, 4, IMMED2);
			MATHB(p, MATH1, AND, upper_32_bits(PDCP_DIR_MASK_BE),
			      MATH3, 4, IMMED2);
		}
		/* Word swap MATH3 reg */
		MATHB(p, MATH3, SHLD, MATH3, MATH3, 8, 0);

		/* Don't understand, seems to be doing a move of 12 byte
		 * (read MATH2 and overread MATH3)
		 */
		MOVEB(p, MATH2, 4, OFIFO, 0, 12, IMMED);

		/* Add the rest of the snow IV to the context */
		MOVE(p, OFIFO, 0, CONTEXT2, 4, 12, IMMED);
	}

	/* Set the variable size of data the register will write */
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		/* We will add the integrity data so add its length */
		MATHI(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	} else {
		/* We will check the integrity data so remove its length */
		MATHI(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
		/* Do not take the ICV in the out-snooping configuration */
		MATHI(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQINSZ, 4, IMMED2);
	}

	/* We write the PDCP header to output*/
	SEQSTORE(p, MATH0, offset, length, 0);

	/* Definition of the flow of output data */
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		/* We write data according to VSEQOUTSZ */
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
	} else {
		/* We write data according to VSEQOUTSZ */
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
	}

	/* Get parameters for authentication */
	if (authdata->algtype == PDCP_AUTH_TYPE_ZUC) {
		int_op_alg = OP_ALG_ALGSEL_ZUCA;
		int_op_aai = OP_ALG_AAI_F9;
	} else if (authdata->algtype == PDCP_AUTH_TYPE_SNOW) {
		int_op_alg = OP_ALG_ALGSEL_SNOW_F9;
		int_op_aai = OP_ALG_AAI_F9;
	} else {
		pr_err("%s no support for auth alg: %d\n", __func__,
		       authdata->algtype);
		return -1;
	}

	/* Get parameters for ciphering */
	if (cipherdata->algtype == PDCP_CIPHER_TYPE_ZUC) {
		cipher_op_alg = OP_ALG_ALGSEL_ZUCE;
		cipher_op_aai = OP_ALG_AAI_F8;
	} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_SNOW) {
		cipher_op_alg = OP_ALG_ALGSEL_SNOW_F8;
		cipher_op_aai = OP_ALG_AAI_F8;
	} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_AES) {
		cipher_op_alg = OP_ALG_ALGSEL_AES;
		cipher_op_aai = OP_ALG_AAI_CTR;
	} else {
		pr_err("%s no support for cipher alg: %d\n", __func__,
		       authdata->algtype);
		return -1;
	}

	/* Configure the CHA, the class 2 CHA must be configured first or an
	 * error will be generated
	 */

	/* Configure the class 2 CHA (integrity )*/
	ALG_OPERATION(p, int_op_alg, int_op_aai, OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? ICV_CHECK_DISABLE :
						      ICV_CHECK_ENABLE,
		      DIR_ENC);

	/* Configure class 1 CHA (confidentiality)*/
	ALG_OPERATION(p, cipher_op_alg, cipher_op_aai, OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	/* Definition of the flow of input data */
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		/* We read data according to VSEQINSZ
		 * Note: we perform an in-snooping, eg the data will be read
		 * only once. they will be sent to both the integrity CHA and
		 * confidentiality CHA
		 */
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);

		/* When the integrity CHA is finished, send the ICV stored in
		 * the context to the confidentiality CHA for encryption
		 */
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		/* We read data according to VSEQINSZ
		 * Note: we perform an out-snooping, eg the data will be read
		 * only once. The will first be sent to the confidentiality
		 * CHA for decryption, then the CAAM will direct them to the
		 * integrity CHA to verify the ICV (which is at the end of the
		 * sequence)
		 */
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST2);

		/* Process the ICV by class 1 CHA */
		SEQFIFOLOAD(p, MSG1, 4, LAST1 | FLUSH1);

		/* Wait for class 1 CHA to finish, the ICV data are stalling in
		 * the output fifo
		 */
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CLASS1 | NOP | NIFP);

		LOAD(p, 0, DCTRL, 0, LDLEN_RST_CHA_OFIFO_PTR, IMMED);

		/* Save the content left in the Output FIFO (the ICV) to MATH0
		 */
		MOVE(p, OFIFO, 0, MATH0, 0, 4, WAITCOMP | IMMED);

		/* Configure a NFIFO entry to take data from the altsource
		 * and send it to the class 2 CHA as an ICV
		 */
		NFIFOADD(p, IFIFO, ICV2, 4, LAST2);

		/* Move the content of MATH0 (OFIFO offset) to altsource
		 * Note: As configured by the altsource, this will send
		 * the
		 */
		MOVE(p, MATH0, 0, IFIFO, 0, 4, WAITCOMP | IMMED);
	}

	if (authdata->algtype == PDCP_CIPHER_TYPE_ZUC) {
		/* Reset ZUCA mode and done interrupt
		 * Note: If it is not done, DECO generate an error: 200031ca
		 * -> ZUCA ICV failed
		 */
		LOAD(p, CLRW_CLR_C2MODE, CLRW, 0, 4, IMMED);
		LOAD(p, CIRQ_ZADI, ICTRL, 0, 4, IMMED);
	}

	return 0;
}

/* Function used when the integrity algorithm is a class 1 CHA so outsnooping
 * is not possible
 * Supported:
 *  - cipher:
 *      - AES-CTR
 *      - SNOW F8
 *      - ZUC F8
 *  - authentication
 *      - AES-CMAC
 */
static inline int pdcp_sdap_insert_no_snoop_op(
	struct program *p, bool swap __maybe_unused, struct alginfo *cipherdata,
	struct alginfo *authdata, unsigned int dir, enum pdcp_sn_size sn_size,
	enum pdb_type_e pdb_type)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	uint32_t cipher_alg_op = 0;
	uint32_t cipher_alg_aai = 0;
	int hfn_bearer_dir_offset_in_descbuf =
		(pdb_type == PDCP_PDB_TYPE_FULL_PDB) ?
			FULL_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET :
			REDUCED_PDB_DESCBUF_HFN_BEARER_DIR_OFFSET;

	if (pdcp_sdap_get_sn_parameters(sn_size, swap, &offset, &length,
					&sn_mask))
		return -ENOTSUP;

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);

#ifdef SDAP_SUPPORT
	rta_mathi(p, MATH0,
		  ((swap == true) ? MATH_FUN_RSHIFT : MATH_FUN_LSHIFT),
		  SDAP_BITS_SIZE, MATH1, 8, 0);
	MATHB(p, MATH1, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#else
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
#endif

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, hfn_bearer_dir_offset_in_descbuf,
			MATH2, 0, 0x08, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);

	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		/* Load authentication key */
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		/* Set the iv for AES authentication */
		MOVEB(p, MATH2, 0, IFIFOAB1, 0, 8, IMMED);

		/* Pass the header */
		MOVEB(p, MATH0, offset, IFIFOAB1, 0, length, IMMED);

		/* Configure variable size for I/O */
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, VSEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		/* Perform the authentication */
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_DEC);

		/* Configure the read of data */
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		/* Save the ICV generated */
		MOVEB(p, CONTEXT1, 0, MATH3, 0, 4, WAITCOMP | IMMED);

		/* The CHA will be reused so we need to clear it */
		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		/* Load confidentiality key */
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		/* Load the IV for ciphering */
		if (cipherdata->algtype == PDCP_CIPHER_TYPE_AES) {
			MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_AES;
			cipher_alg_aai = OP_ALG_AAI_CTR;
		} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_ZUC) {
			/* Set the IV for the confidentiality CHA */
			MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_ZUCE;
			cipher_alg_aai = OP_ALG_AAI_F8;
		} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_SNOW) {
			/* Set the IV for the confidentiality CHA */
			MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_SNOW_F8;
			cipher_alg_aai = OP_ALG_AAI_F8;
		}

		/* Rewind the pointer on input data to reread it */
		SEQINPTR(p, 0, PDCP_NULL_MAX_FRAME_LEN, RTO);

		/* Define the ciphering operation */
		ALG_OPERATION(p, cipher_alg_op, cipher_alg_aai,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_ENC);

		/* Define the data to write */
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		/* Skip the header which does not need to be encrypted */
		SEQFIFOLOAD(p, SKIP, length, 0);

		/* Read the rest of the data */
		SEQFIFOLOAD(p, MSG1, 0, VLF);

		/* Send the ICV stored in MATH3 for encryption */
		MOVEB(p, MATH3, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		/* Load the IV for ciphering */
		if (cipherdata->algtype == PDCP_CIPHER_TYPE_AES) {
			MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_AES;
			cipher_alg_aai = OP_ALG_AAI_CTR;
		} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_ZUC) {
			/* Set the IV for the confidentiality CHA */
			MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_ZUCE;
			cipher_alg_aai = OP_ALG_AAI_F8;
		} else if (cipherdata->algtype == PDCP_CIPHER_TYPE_SNOW) {
			/* Set the IV for the confidentiality CHA */
			MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
			cipher_alg_op = OP_ALG_ALGSEL_SNOW_F8;
			cipher_alg_aai = OP_ALG_AAI_F8;
		}
		MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, IMMED);

		/* Read all the data */
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

		/* Do not write back the ICV */
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		/* Load the key for ciphering */
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		/* Write all the data */
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);

		/* Define the ciphering algorithm */
		ALG_OPERATION(p, cipher_alg_op, cipher_alg_aai,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_DEC);

		/* Read all the data */
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		/* Save the ICV which is stalling in output FIFO to MATH3 */
		MOVEB(p, OFIFO, 0, MATH3, 0, 4, IMMED);

		/* Reset class 1 CHA */
		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		/* Load the key for authentication */
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		/* Start a new sequence */
		SEQINPTR(p, 0, 0, SOP);

		/* Define the operation to verify the ICV */
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_ENABLE, DIR_DEC);

		/* Set the variable size input */
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 8, IMMED);

		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		/* Define an NFIFO entry to load the ICV saved */
		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS1 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC1 |
		     NFIFOENTRY_FC1 | 4, NFIFO_SZL, 0, 4, IMMED);

		/* Load the ICV */
		MOVEB(p, MATH3, 0, ALTSOURCE, 0, 4, IMMED);
	}

	return 0;
}

static inline int
pdcp_sdap_insert_cplane_null_op(struct program *p,
			   bool swap __maybe_unused,
			   struct alginfo *cipherdata,
			   struct alginfo *authdata,
			   unsigned int dir,
			   enum pdcp_sn_size sn_size,
			   enum pdb_type_e pdb_type __maybe_unused)
{
	return pdcp_insert_cplane_null_op(p, swap, cipherdata, authdata, dir,
					  sn_size);
}

static inline int
pdcp_sdap_insert_cplane_int_only_op(struct program *p,
			   bool swap __maybe_unused,
			   struct alginfo *cipherdata,
			   struct alginfo *authdata,
			   unsigned int dir,
			   enum pdcp_sn_size sn_size,
			   enum pdb_type_e pdb_type __maybe_unused)
{
	return pdcp_insert_cplane_int_only_op(p, swap, cipherdata, authdata,
				dir, sn_size);
}

static int pdcp_sdap_insert_with_int_op(
	struct program *p, bool swap __maybe_unused, struct alginfo *cipherdata,
	struct alginfo *authdata, enum pdcp_sn_size sn_size,
	unsigned int dir,
	enum pdb_type_e pdb_type)
{
	static int (
		*pdcp_cp_fp[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID])(
		struct program *, bool swap, struct alginfo *, struct alginfo *,
		unsigned int dir, enum pdcp_sn_size, enum pdb_type_e pdb_type) = {
		{
			/* NULL */
			pdcp_sdap_insert_cplane_null_op,     /* NULL */
			pdcp_sdap_insert_cplane_int_only_op, /* SNOW f9 */
			pdcp_sdap_insert_cplane_int_only_op, /* AES CMAC */
			pdcp_sdap_insert_cplane_int_only_op  /* ZUC-I */
		},
		{
			/* SNOW f8 */
			pdcp_sdap_insert_enc_only_op, /* NULL */
			pdcp_sdap_insert_snoop_op,    /* SNOW f9 */
			pdcp_sdap_insert_no_snoop_op, /* AES CMAC */
			pdcp_sdap_insert_snoop_op     /* ZUC-I */
		},
		{
			/* AES CTR */
			pdcp_sdap_insert_enc_only_op, /* NULL */
			pdcp_sdap_insert_snoop_op,    /* SNOW f9 */
			pdcp_sdap_insert_no_snoop_op, /* AES CMAC */
			pdcp_sdap_insert_snoop_op     /* ZUC-I */
		},
		{
			/* ZUC-E */
			pdcp_sdap_insert_enc_only_op, /* NULL */
			pdcp_sdap_insert_snoop_op,    /* SNOW f9 */
			pdcp_sdap_insert_no_snoop_op, /* AES CMAC */
			pdcp_sdap_insert_snoop_op     /* ZUC-I */
		},
	};
	int err;

	err = pdcp_cp_fp[cipherdata->algtype]
			[authdata->algtype](p, swap, cipherdata, authdata, dir,
					sn_size, pdb_type);
	if (err)
		return err;

	return 0;
}

static inline int
cnstr_shdsc_pdcp_sdap_u_plane(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       enum pdcp_sn_size sn_size,
			       uint32_t hfn,
			       unsigned short bearer,
			       unsigned short direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       uint32_t caps_mode)
{
	struct program prg;
	struct program *p = &prg;
	int err;
	enum pdb_type_e pdb_type;
	static enum rta_share_type
		desc_share[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
			{
				/* NULL */
				SHR_WAIT,   /* NULL */
				SHR_WAIT, /* SNOW f9 */
				SHR_WAIT, /* AES CMAC */
				SHR_WAIT  /* ZUC-I */
			},
			{
				/* SNOW f8 */
				SHR_WAIT, /* NULL */
				SHR_WAIT, /* SNOW f9 */
				SHR_WAIT,   /* AES CMAC */
				SHR_WAIT    /* ZUC-I */
			},
			{
				/* AES CTR */
				SHR_WAIT, /* NULL */
				SHR_WAIT, /* SNOW f9 */
				SHR_WAIT, /* AES CMAC */
				SHR_WAIT    /* ZUC-I */
			},
			{
				/* ZUC-E */
				SHR_WAIT, /* NULL */
				SHR_WAIT,   /* SNOW f9 */
				SHR_WAIT,   /* AES CMAC */
				SHR_WAIT    /* ZUC-I */
			},
		};

	LABEL(pdb_end);

	/* Check the confidentiality algorithm is supported by the code */
	switch (cipherdata->algtype) {
	case PDCP_CIPHER_TYPE_NULL:
	case PDCP_CIPHER_TYPE_SNOW:
	case PDCP_CIPHER_TYPE_AES:
	case PDCP_CIPHER_TYPE_ZUC:
		break;
	default:
		pr_err("Cipher algorithm not supported: %d\n",
				cipherdata->algtype);
		return -ENOTSUP;
	}

	/* Check the authentication algorithm is supported by the code */
	if (authdata) {
		switch (authdata->algtype) {
		case PDCP_AUTH_TYPE_NULL:
		case PDCP_AUTH_TYPE_SNOW:
		case PDCP_AUTH_TYPE_AES:
		case PDCP_AUTH_TYPE_ZUC:
			break;
		default:
			pr_err("Auth algorithm not supported: %d\n",
					authdata->algtype);
			return -ENOTSUP;
		}
	}

	/* Check the Sequence Number size is supported by the code */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
	case PDCP_SN_SIZE_18:
		break;
	default:
		pr_err("SN size not supported: %d\n", sn_size);
		return -ENOTSUP;
	}

	/* Initialize the program */
	PROGRAM_CNTXT_INIT(p, descbuf, 0);

	if (swap)
		PROGRAM_SET_BSWAP(p);

	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	/* Select the shared descriptor sharing mode */
	if (authdata)
		SHR_HDR(p, desc_share[cipherdata->algtype][authdata->algtype],
			0, 0);
	else
		SHR_HDR(p, SHR_WAIT, 0, 0);

	/* Construct the PDB */
	pdb_type = cnstr_pdcp_u_plane_pdb(p, sn_size, hfn, bearer, direction,
					  hfn_threshold, cipherdata, authdata);
	if (pdb_type == PDCP_PDB_TYPE_INVALID) {
		pr_err("Error creating PDCP UPlane PDB\n");
		return -EINVAL;
	}
	SET_LABEL(p, pdb_end);

	/* Inser the HFN override operation */
	err = insert_hfn_ov_op(p, sn_size, pdb_type, false);
	if (err)
		return err;

	/* Create the descriptor */
	if (!authdata) {
		if (cipherdata->algtype == PDCP_CIPHER_TYPE_NULL) {
			insert_copy_frame_op(p, cipherdata,
					     OP_TYPE_ENCAP_PROTOCOL);
		} else {
			err = pdcp_sdap_insert_no_int_op(p, swap, cipherdata,
							 caps_mode,
							 sn_size, pdb_type);
			if (err) {
				pr_err("Fail pdcp_sdap_insert_no_int_op\n");
				return err;
			}
		}
	} else {
		err = pdcp_sdap_insert_with_int_op(p, swap, cipherdata,
						   authdata, sn_size,
						   caps_mode, pdb_type);
		if (err) {
			pr_err("Fail pdcp_sdap_insert_with_int_op\n");
			return err;
		}
	}

	PATCH_HDR(p, 0, pdb_end);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_pdcp_sdap_u_plane_encap - Function for creating a PDCP-SDAP
 *                                       User Plane encapsulation descriptor.
 * @descbuf: pointer to buffer for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @sn_size: selects Sequence Number Size: 7/12/15 bits
 * @hfn: starting Hyper Frame Number to be used together with the SN from the
 *       PDCP frames.
 * @bearer: radio bearer ID
 * @direction: the direction of the PDCP frame (UL/DL)
 * @hfn_threshold: HFN value that once reached triggers a warning from SEC that
 *                 keys should be renegotiated at the earliest convenience.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values are those from cipher_type_pdcp enum.
 *
 * Return: size of descriptor written in words or negative number on error.
 *         Once the function returns, the value of this parameter can be used
 *         for reclaiming the space that wasn't used for the descriptor.
 *
 * Note: descbuf must be large enough to contain a full 256 byte long
 * descriptor; after the function returns, by subtracting the actual number of
 * bytes used, the user can reuse the remaining buffer space for other purposes.
 */
static inline int
cnstr_shdsc_pdcp_sdap_u_plane_encap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       enum pdcp_sn_size sn_size,
			       uint32_t hfn,
			       unsigned short bearer,
			       unsigned short direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata)
{
	return cnstr_shdsc_pdcp_sdap_u_plane(descbuf, ps, swap, sn_size,
			hfn, bearer, direction, hfn_threshold, cipherdata,
			authdata, OP_TYPE_ENCAP_PROTOCOL);
}

/**
 * cnstr_shdsc_pdcp_sdap_u_plane_decap - Function for creating a PDCP-SDAP
 *                                       User Plane decapsulation descriptor.
 * @descbuf: pointer to buffer for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @sn_size: selects Sequence Number Size: 7/12/15 bits
 * @hfn: starting Hyper Frame Number to be used together with the SN from the
 *       PDCP frames.
 * @bearer: radio bearer ID
 * @direction: the direction of the PDCP frame (UL/DL)
 * @hfn_threshold: HFN value that once reached triggers a warning from SEC that
 *                 keys should be renegotiated at the earliest convenience.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values are those from cipher_type_pdcp enum.
 *
 * Return: size of descriptor written in words or negative number on error.
 *         Once the function returns, the value of this parameter can be used
 *         for reclaiming the space that wasn't used for the descriptor.
 *
 * Note: descbuf must be large enough to contain a full 256 byte long
 * descriptor; after the function returns, by subtracting the actual number of
 * bytes used, the user can reuse the remaining buffer space for other purposes.
 */
static inline int
cnstr_shdsc_pdcp_sdap_u_plane_decap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       enum pdcp_sn_size sn_size,
			       uint32_t hfn,
			       unsigned short bearer,
			       unsigned short direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata)
{
	return cnstr_shdsc_pdcp_sdap_u_plane(descbuf, ps, swap, sn_size, hfn,
			bearer, direction, hfn_threshold, cipherdata, authdata,
			OP_TYPE_DECAP_PROTOCOL);
}

#endif /* __DESC_SDAP_H__ */
