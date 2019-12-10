/* SPDX-License-Identifier: BSD-3-Clause or GPL-2.0+
 * Copyright 2008-2013 Freescale Semiconductor, Inc.
 * Copyright 2019-2020 NXP
 */

#ifndef __DESC_PDCP_H__
#define __DESC_PDCP_H__

#include "rta.h"
#include "common.h"

/**
 * DOC: PDCP Shared Descriptor Constructors
 *
 * Shared descriptors for PDCP protocol.
 */

/**
 * PDCP_NULL_MAX_FRAME_LEN - The maximum frame frame length that is supported by
 *                           PDCP NULL protocol.
 */
#define PDCP_NULL_MAX_FRAME_LEN		0x00002FFF

/**
 * PDCP_MAC_I_LEN - The length of the MAC-I for PDCP protocol operation
 */
#define PDCP_MAC_I_LEN			0x00000004

/**
 * PDCP_MAX_FRAME_LEN_STATUS - The status returned in FD status/command field in
 *                             case the input frame is larger than
 *                             PDCP_NULL_MAX_FRAME_LEN.
 */
#define PDCP_MAX_FRAME_LEN_STATUS	0xF1

/**
 * PDCP_C_PLANE_SN_MASK - This mask is used in the PDCP descriptors for
 *                        extracting the sequence number (SN) from the PDCP
 *                        Control Plane header. For PDCP Control Plane, the SN
 *                        is constant (5 bits) as opposed to PDCP Data Plane
 *                        (7/12/15 bits).
 */
#define PDCP_C_PLANE_SN_MASK		0x1F000000
#define PDCP_C_PLANE_SN_MASK_BE		0x0000001F

/**
 * PDCP_12BIT_SN_MASK - This mask is used in the PDCP descriptors for
 *                              extracting the sequence number (SN) from the
 *                              PDCP User Plane header.
 */
#define PDCP_12BIT_SN_MASK		0xFF0F0000
#define PDCP_12BIT_SN_MASK_BE		0x00000FFF

/**
 * PDCP_U_PLANE_15BIT_SN_MASK - This mask is used in the PDCP descriptors for
 *                              extracting the sequence number (SN) from the
 *                              PDCP User Plane header. For PDCP Control Plane,
 *                              the SN is constant (5 bits) as opposed to PDCP
 *                              Data Plane (7/12/15 bits).
 */
#define PDCP_U_PLANE_15BIT_SN_MASK	0xFF7F0000
#define PDCP_U_PLANE_15BIT_SN_MASK_BE	0x00007FFF

/**
 * PDCP_U_PLANE_18BIT_SN_MASK - This mask is used in the PDCP descriptors for
 *                              extracting the sequence number (SN) from the
 *                              PDCP User Plane header.
 */
#define PDCP_U_PLANE_18BIT_SN_MASK	0xFFFF0300
#define PDCP_U_PLANE_18BIT_SN_MASK_BE	0x0003FFFF

/**
 * PDCP_BEARER_MASK - This mask is used masking out the bearer for PDCP
 *                    processing with SNOW f9 in LTE.
 *
 * The value on which this mask is applied is formatted as below:
 *     Count-C (32 bit) | Bearer (5 bit) | Direction (1 bit) | 0 (26 bits)
 *
 * Applying this mask is done for creating the upper 64 bits of the IV needed
 * for SNOW f9.
 *
 * The lower 32 bits of the mask are used for masking the direction for AES
 * CMAC IV.
 */
#define PDCP_BEARER_MASK		0x00000004FFFFFFFFull
#define PDCP_BEARER_MASK_BE		0xFFFFFFFF04000000ull

/**
 * PDCP_DIR_MASK - This mask is used masking out the direction for PDCP
 *                 processing with SNOW f9 in LTE.
 *
 * The value on which this mask is applied is formatted as below:
 *     Bearer (5 bit) | Direction (1 bit) | 0 (26 bits)
 *
 * Applying this mask is done for creating the lower 32 bits of the IV needed
 * for SNOW f9.
 *
 * The upper 32 bits of the mask are used for masking the direction for AES
 * CMAC IV.
 */
#define PDCP_DIR_MASK			0x00000000000000F8ull
#define PDCP_DIR_MASK_BE			0xF800000000000000ull

/**
 * PDCP_NULL_INT_MAC_I_VAL - The value of the PDCP PDU MAC-I in case NULL
 *                           integrity is used.
 */

#define PDCP_NULL_INT_MAC_I_VAL		0x00000000

/**
 * PDCP_NULL_INT_ICV_CHECK_FAILED_STATUS - The status used to report ICV check
 *                                         failed in case of NULL integrity
 *                                         Control Plane processing.
 */
#define PDCP_NULL_INT_ICV_CHECK_FAILED_STATUS	0x0A
/**
 * PDCP_DPOVRD_HFN_OV_EN - Value to be used in the FD status/cmd field to
 *                         indicate the HFN override mechanism is active for the
 *                         frame.
 */
#define PDCP_DPOVRD_HFN_OV_EN		0x80000000

/**
 * PDCP_P4080REV2_HFN_OV_BUFLEN - The length in bytes of the supplementary space
 *                                that must be provided by the user at the
 *                                beginning of the input frame buffer for
 *                                P4080 REV 2.
 *
 * The format of the frame buffer is the following:
 *
 *  |<---PDCP_P4080REV2_HFN_OV_BUFLEN-->|
 * //===================================||============||==============\\
 * || PDCP_DPOVRD_HFN_OV_EN | HFN value || PDCP Header|| PDCP Payload ||
 * \\===================================||============||==============//
 *
 * If HFN override mechanism is not desired, then the MSB of the first 4 bytes
 * must be set to 0b.
 */
#define PDCP_P4080REV2_HFN_OV_BUFLEN	4

/**
 * enum cipher_type_pdcp - Type selectors for cipher types in PDCP protocol OP
 *                         instructions.
 * @PDCP_CIPHER_TYPE_NULL: NULL
 * @PDCP_CIPHER_TYPE_SNOW: SNOW F8
 * @PDCP_CIPHER_TYPE_AES: AES
 * @PDCP_CIPHER_TYPE_ZUC: ZUCE
 * @PDCP_CIPHER_TYPE_INVALID: invalid option
 */
enum cipher_type_pdcp {
	PDCP_CIPHER_TYPE_NULL,
	PDCP_CIPHER_TYPE_SNOW,
	PDCP_CIPHER_TYPE_AES,
	PDCP_CIPHER_TYPE_ZUC,
	PDCP_CIPHER_TYPE_INVALID
};

/**
 * enum auth_type_pdcp - Type selectors for integrity types in PDCP protocol OP
 *                       instructions.
 * @PDCP_AUTH_TYPE_NULL: NULL
 * @PDCP_AUTH_TYPE_SNOW: SNOW F9
 * @PDCP_AUTH_TYPE_AES: AES CMAC
 * @PDCP_AUTH_TYPE_ZUC: ZUCA
 * @PDCP_AUTH_TYPE_INVALID: invalid option
 */
enum auth_type_pdcp {
	PDCP_AUTH_TYPE_NULL,
	PDCP_AUTH_TYPE_SNOW,
	PDCP_AUTH_TYPE_AES,
	PDCP_AUTH_TYPE_ZUC,
	PDCP_AUTH_TYPE_INVALID
};

/**
 * enum pdcp_dir - Type selectors for direction for PDCP protocol
 * @PDCP_DIR_UPLINK: uplink direction
 * @PDCP_DIR_DOWNLINK: downlink direction
 * @PDCP_DIR_INVALID: invalid option
 */
enum pdcp_dir {
	PDCP_DIR_UPLINK = 0,
	PDCP_DIR_DOWNLINK = 1,
	PDCP_DIR_INVALID
};

/**
 * enum pdcp_plane - PDCP domain selectors
 * @PDCP_CONTROL_PLANE: Control Plane
 * @PDCP_DATA_PLANE: Data Plane
 * @PDCP_SHORT_MAC: Short MAC
 */
enum pdcp_plane {
	PDCP_CONTROL_PLANE,
	PDCP_DATA_PLANE,
	PDCP_SHORT_MAC
};

/**
 * enum pdcp_sn_size - Sequence Number Size selectors for PDCP protocol
 * @PDCP_SN_SIZE_5: 5bit sequence number
 * @PDCP_SN_SIZE_7: 7bit sequence number
 * @PDCP_SN_SIZE_12: 12bit sequence number
 * @PDCP_SN_SIZE_15: 15bit sequence number
 * @PDCP_SN_SIZE_18: 18bit sequence number
 */
enum pdcp_sn_size {
	PDCP_SN_SIZE_5 = 5,
	PDCP_SN_SIZE_7 = 7,
	PDCP_SN_SIZE_12 = 12,
	PDCP_SN_SIZE_15 = 15,
	PDCP_SN_SIZE_18 = 18
};

/*
 * PDCP Control Plane Protocol Data Blocks
 */
#define PDCP_C_PLANE_PDB_HFN_SHIFT		5
#define PDCP_C_PLANE_PDB_BEARER_SHIFT		27
#define PDCP_C_PLANE_PDB_DIR_SHIFT		26
#define PDCP_C_PLANE_PDB_HFN_THR_SHIFT		5

#define PDCP_U_PLANE_PDB_OPT_SHORT_SN		0x2
#define PDCP_U_PLANE_PDB_OPT_15B_SN		0x4
#define PDCP_U_PLANE_PDB_OPT_18B_SN		0x6
#define PDCP_U_PLANE_PDB_SHORT_SN_HFN_SHIFT	7
#define PDCP_U_PLANE_PDB_LONG_SN_HFN_SHIFT	12
#define PDCP_U_PLANE_PDB_15BIT_SN_HFN_SHIFT	15
#define PDCP_U_PLANE_PDB_18BIT_SN_HFN_SHIFT	18
#define PDCP_U_PLANE_PDB_BEARER_SHIFT		27
#define PDCP_U_PLANE_PDB_DIR_SHIFT		26
#define PDCP_U_PLANE_PDB_SHORT_SN_HFN_THR_SHIFT	7
#define PDCP_U_PLANE_PDB_LONG_SN_HFN_THR_SHIFT	12
#define PDCP_U_PLANE_PDB_15BIT_SN_HFN_THR_SHIFT	15
#define PDCP_U_PLANE_PDB_18BIT_SN_HFN_THR_SHIFT	18

struct pdcp_pdb {
	union {
		uint32_t opt;
		uint32_t rsvd;
	} opt_res;
	uint32_t hfn_res;	/* HyperFrame number,(27, 25 or 21 bits),
				 * left aligned & right-padded with zeros.
				 */
	uint32_t bearer_dir_res;/* Bearer(5 bits), packet direction (1 bit),
				 * left aligned & right-padded with zeros.
				 */
	uint32_t hfn_thr_res;	/* HyperFrame number threshold (27, 25 or 21
				 * bits), left aligned & right-padded with
				 * zeros.
				 */
};

/*
 * PDCP internal PDB types
 */
enum pdb_type_e {
	PDCP_PDB_TYPE_NO_PDB,
	PDCP_PDB_TYPE_FULL_PDB,
	PDCP_PDB_TYPE_REDUCED_PDB,
	PDCP_PDB_TYPE_INVALID
};

/**
 * rta_inline_pdcp_query() - Provide indications if a key can be passed as
 *                           immediate data or shall be referenced in a
 *                           shared descriptor.
 * Return: 0 if data can be inlined or 1 if referenced.
 */
static inline int
rta_inline_pdcp_query(enum auth_type_pdcp auth_alg,
		      enum cipher_type_pdcp cipher_alg,
		      enum pdcp_sn_size sn_size,
		      int8_t hfn_ovd)
{
	/**
	 * Shared Descriptors for some of the cases does not fit in the
	 * MAX_DESC_SIZE of the descriptor especially when non-protocol
	 * descriptors are formed as in 18bit cases and when HFN override
	 * is enabled as 2 extra words are added in the job descriptor.
	 * The cases which exceed are for RTA_SEC_ERA=8 and HFN override
	 * enabled and 18bit uplane and either of following Algo combinations.
	 * - SNOW-AES
	 * - AES-SNOW
	 * - SNOW-SNOW
	 * - ZUC-SNOW
	 *
	 * We cannot make inline for all cases, as this will impact performance
	 * due to extra memory accesses for the keys.
	 */
	if ((rta_sec_era == RTA_SEC_ERA_8) && hfn_ovd &&
			(sn_size == PDCP_SN_SIZE_18) &&
			((cipher_alg == PDCP_CIPHER_TYPE_SNOW &&
				auth_alg == PDCP_AUTH_TYPE_AES) ||
			(cipher_alg == PDCP_CIPHER_TYPE_AES &&
				auth_alg == PDCP_AUTH_TYPE_SNOW) ||
			(cipher_alg == PDCP_CIPHER_TYPE_SNOW &&
				auth_alg == PDCP_AUTH_TYPE_SNOW) ||
			(cipher_alg == PDCP_CIPHER_TYPE_ZUC &&
				auth_alg == PDCP_AUTH_TYPE_SNOW))) {

		return 1;
	}

	return 0;
}

/*
 * Function for appending the portion of a PDCP Control Plane shared descriptor
 * which performs NULL encryption and integrity (i.e. copies the input frame
 * to the output frame, appending 32 bits of zeros at the end (MAC-I for
 * NULL integrity).
 */
static inline int
pdcp_insert_cplane_null_op(struct program *p,
			   bool swap __maybe_unused,
			   struct alginfo *cipherdata __maybe_unused,
			   struct alginfo *authdata __maybe_unused,
			   unsigned int dir,
			   enum pdcp_sn_size sn_size __maybe_unused,
			   unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	LABEL(local_offset);
	REFERENCE(move_cmd_read_descbuf);
	REFERENCE(move_cmd_write_descbuf);

	if (rta_sec_era > RTA_SEC_ERA_2) {
		MATHB(p, SEQINSZ, ADD, ZERO, VSEQINSZ, 4, 0);
		if (dir == OP_TYPE_ENCAP_PROTOCOL)
			MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
		else
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
	} else {
		MATHB(p, SEQINSZ, ADD, ONE, VSEQINSZ, 4, 0);
		MATHB(p, VSEQINSZ, SUB, ONE, VSEQINSZ, 4, 0);

		if (dir == OP_TYPE_ENCAP_PROTOCOL) {
			MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
			MATHB(p, VSEQINSZ, SUB, ONE, MATH0, 4, 0);
		} else {
			MATHB(p, VSEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQINSZ, 4,
			      IMMED2);
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
			MATHB(p, VSEQOUTSZ, SUB, ONE, MATH0, 4, 0);
		}

		MATHB(p, MATH0, ADD, ONE, MATH0, 4, 0);

		/*
		 * Since MOVELEN is available only starting with
		 * SEC ERA 3, use poor man's MOVELEN: create a MOVE
		 * command dynamically by writing the length from M1 by
		 * OR-ing the command in the M1 register and MOVE the
		 * result into the descriptor buffer. Care must be taken
		 * wrt. the location of the command because of SEC
		 * pipelining. The actual MOVEs are written at the end
		 * of the descriptor due to calculations needed on the
		 * offset in the descriptor for the MOVE command.
		 */
		move_cmd_read_descbuf = MOVE(p, DESCBUF, 0, MATH0, 0, 6,
					     IMMED);
		move_cmd_write_descbuf = MOVE(p, MATH0, 0, DESCBUF, 0, 8,
					      WAITCOMP | IMMED);
	}
	MATHB(p, VSEQINSZ, SUB, PDCP_NULL_MAX_FRAME_LEN, NONE, 4,
	      IMMED2);
	JUMP(p, PDCP_MAX_FRAME_LEN_STATUS, HALT_STATUS, ALL_FALSE, MATH_N);

	if (rta_sec_era > RTA_SEC_ERA_2) {
		if (dir == OP_TYPE_ENCAP_PROTOCOL)
			MATHB(p, VSEQINSZ, ADD, ZERO, MATH0, 4, 0);
		else
			MATHB(p, VSEQOUTSZ, ADD, ZERO, MATH0, 4, 0);
	}
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);
	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

	if (rta_sec_era > RTA_SEC_ERA_2) {
		MOVE(p, AB1, 0, OFIFO, 0, MATH0, 0);
	} else {
		SET_LABEL(p, local_offset);

		/* Shut off automatic Info FIFO entries */
		LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);
		/* Placeholder for MOVE command with length from M1 register */
		MOVE(p, IFIFOAB1, 0, OFIFO, 0, 0, IMMED);
		/* Enable automatic Info FIFO entries */
		LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MATHB(p, MATH1, XOR, MATH1, MATH0, 8, 0);
		MOVE(p, MATH0, 0, OFIFO, 0, 4, IMMED);
	}

	if (rta_sec_era < RTA_SEC_ERA_3) {
		PATCH_MOVE(p, move_cmd_read_descbuf, local_offset);
		PATCH_MOVE(p, move_cmd_write_descbuf, local_offset);
	}

	return 0;
}

static inline int
insert_copy_frame_op(struct program *p,
		     struct alginfo *cipherdata __maybe_unused,
		     unsigned int dir __maybe_unused)
{
	LABEL(local_offset);
	REFERENCE(move_cmd_read_descbuf);
	REFERENCE(move_cmd_write_descbuf);

	if (rta_sec_era > RTA_SEC_ERA_2) {
		MATHB(p, SEQINSZ, ADD, ZERO, VSEQINSZ,  4, 0);
		MATHB(p, SEQINSZ, ADD, ZERO, VSEQOUTSZ,  4, 0);
	} else {
		MATHB(p, SEQINSZ, ADD, ONE, VSEQINSZ,  4, 0);
		MATHB(p, VSEQINSZ, SUB, ONE, VSEQINSZ,  4, 0);
		MATHB(p, SEQINSZ, ADD, ONE, VSEQOUTSZ,  4, 0);
		MATHB(p, VSEQOUTSZ, SUB, ONE, VSEQOUTSZ,  4, 0);
		MATHB(p, VSEQINSZ, SUB, ONE, MATH0,  4, 0);
		MATHB(p, MATH0, ADD, ONE, MATH0,  4, 0);

		/*
		 * Since MOVELEN is available only starting with
		 * SEC ERA 3, use poor man's MOVELEN: create a MOVE
		 * command dynamically by writing the length from M1 by
		 * OR-ing the command in the M1 register and MOVE the
		 * result into the descriptor buffer. Care must be taken
		 * wrt. the location of the command because of SEC
		 * pipelining. The actual MOVEs are written at the end
		 * of the descriptor due to calculations needed on the
		 * offset in the descriptor for the MOVE command.
		 */
		move_cmd_read_descbuf = MOVE(p, DESCBUF, 0, MATH0, 0, 6,
					     IMMED);
		move_cmd_write_descbuf = MOVE(p, MATH0, 0, DESCBUF, 0, 8,
					      WAITCOMP | IMMED);
	}
	MATHB(p, SEQINSZ, SUB, PDCP_NULL_MAX_FRAME_LEN, NONE,  4,
	      IFB | IMMED2);
	JUMP(p, PDCP_MAX_FRAME_LEN_STATUS, HALT_STATUS, ALL_FALSE, MATH_N);

	if (rta_sec_era > RTA_SEC_ERA_2)
		MATHB(p, VSEQINSZ, ADD, ZERO, MATH0,  4, 0);

	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);
	if (rta_sec_era > RTA_SEC_ERA_2) {
		MOVE(p, AB1, 0, OFIFO, 0, MATH0, 0);
	} else {
		SET_LABEL(p, local_offset);

		/* Shut off automatic Info FIFO entries */
		LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);

		/* Placeholder for MOVE command with length from M0 register */
		MOVE(p, IFIFOAB1, 0, OFIFO, 0, 0, IMMED);

		/* Enable automatic Info FIFO entries */
		LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
	}

	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	if (rta_sec_era < RTA_SEC_ERA_3) {
		PATCH_MOVE(p, move_cmd_read_descbuf, local_offset);
		PATCH_MOVE(p, move_cmd_write_descbuf, local_offset);
	}
	return 0;
}

static inline int
pdcp_insert_cplane_int_only_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata __maybe_unused,
			       struct alginfo *authdata, unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	/* 12 bit SN is only supported for protocol offload case */
	if (rta_sec_era >= RTA_SEC_ERA_8 && sn_size == PDCP_SN_SIZE_12) {
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER_RN,
			 (uint16_t)authdata->algtype);
		return 0;
	}

	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}
	LABEL(local_offset);
	REFERENCE(move_cmd_read_descbuf);
	REFERENCE(move_cmd_write_descbuf);

	switch (authdata->algtype) {
	case PDCP_AUTH_TYPE_SNOW:
		/* Insert Auth Key */
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		SEQLOAD(p, MATH0, offset, length, 0);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);

		if (rta_sec_era > RTA_SEC_ERA_2 ||
		    (rta_sec_era == RTA_SEC_ERA_2 &&
				   era_2_sw_hfn_ovrd == 0)) {
			SEQINPTR(p, 0, length, RTO);
		} else {
			SEQINPTR(p, 0, 5, RTO);
			SEQFIFOLOAD(p, SKIP, 4, 0);
		}

		if (swap == false) {
			MATHB(p, MATH0, AND, sn_mask, MATH1,  8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1,  8, 0);

			MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);

			MATHB(p, MATH2, AND, PDCP_BEARER_MASK, MATH2, 8,
			      IMMED2);
			MOVEB(p, DESCBUF, 0x0C, MATH3, 0, 4, WAITCOMP | IMMED);
			MATHB(p, MATH3, AND, PDCP_DIR_MASK, MATH3, 8, IMMED2);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVEB(p, MATH2, 0, CONTEXT2, 0, 0x0C, WAITCOMP | IMMED);
		} else {
			MATHB(p, MATH0, AND, sn_mask, MATH1,  8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1,  8, 0);

			MOVE(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
			MATHB(p, MATH2, AND, PDCP_BEARER_MASK_BE, MATH2, 8,
			      IMMED2);

			MOVE(p, DESCBUF, 0x0C, MATH3, 0, 4, WAITCOMP | IMMED);
			MATHB(p, MATH3, AND, PDCP_DIR_MASK_BE, MATH3, 8,
			      IMMED2);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVE(p, MATH2, 0, CONTEXT2, 0, 0x0C, WAITCOMP | IMMED);
		}

		if (dir == OP_TYPE_DECAP_PROTOCOL) {
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, MATH1, 4,
			      IMMED2);
		} else {
			if (rta_sec_era > RTA_SEC_ERA_2) {
				MATHB(p, SEQINSZ, SUB, ZERO, MATH1, 4,
				      0);
			} else {
				MATHB(p, SEQINSZ, ADD, ONE, MATH1, 4,
				      0);
				MATHB(p, MATH1, SUB, ONE, MATH1, 4,
				      0);
			}
		}

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MATHB(p, MATH1, SUB, ZERO, VSEQINSZ, 4, 0);
			MATHB(p, MATH1, SUB, ZERO, VSEQOUTSZ, 4, 0);
		} else {
			MATHB(p, ZERO, ADD, MATH1, VSEQINSZ, 4, 0);
			MATHB(p, ZERO, ADD, MATH1, VSEQOUTSZ, 4, 0);

			/*
			 * Since MOVELEN is available only starting with
			 * SEC ERA 3, use poor man's MOVELEN: create a MOVE
			 * command dynamically by writing the length from M1 by
			 * OR-ing the command in the M1 register and MOVE the
			 * result into the descriptor buffer. Care must be taken
			 * wrt. the location of the command because of SEC
			 * pipelining. The actual MOVEs are written at the end
			 * of the descriptor due to calculations needed on the
			 * offset in the descriptor for the MOVE command.
			 */
			move_cmd_read_descbuf = MOVE(p, DESCBUF, 0, MATH1, 0, 6,
						     IMMED);
			move_cmd_write_descbuf = MOVE(p, MATH1, 0, DESCBUF, 0,
						      8, WAITCOMP | IMMED);
		}

		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9, OP_ALG_AAI_F9,
			      OP_ALG_AS_INITFINAL,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
				     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
			      DIR_ENC);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			SEQFIFOLOAD(p, MSGINSNOOP, 0,
				    VLF | LAST1 | LAST2 | FLUSH1);
			MOVE(p, AB1, 0, OFIFO, 0, MATH1, 0);
		} else {
			SEQFIFOLOAD(p, MSGINSNOOP, 0,
				    VLF | LAST1 | LAST2 | FLUSH1);
			SET_LABEL(p, local_offset);

			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);
			/*
			 * Placeholder for MOVE command with length from M1
			 * register
			 */
			MOVE(p, IFIFOAB1, 0, OFIFO, 0, 0, IMMED);
			/* Enable automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
		}

		if (dir == OP_TYPE_DECAP_PROTOCOL)
			SEQFIFOLOAD(p, ICV2, 4, LAST2);
		else
			SEQSTORE(p, CONTEXT2, 0, 4, 0);

		break;

	case PDCP_AUTH_TYPE_AES:
		/* Insert Auth Key */
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		SEQLOAD(p, MATH0, offset, length, 0);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
		if (rta_sec_era > RTA_SEC_ERA_2 ||
		    (rta_sec_era == RTA_SEC_ERA_2 &&
		     era_2_sw_hfn_ovrd == 0)) {
			SEQINPTR(p, 0, length, RTO);
		} else {
			SEQINPTR(p, 0, 5, RTO);
			SEQFIFOLOAD(p, SKIP, 4, 0);
		}

		if (swap == false) {
			MATHB(p, MATH0, AND, sn_mask, MATH1, 8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

			MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVEB(p, MATH2, 0, IFIFOAB1, 0, 8, IMMED);
		} else {
			MATHB(p, MATH0, AND, sn_mask, MATH1, 8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

			MOVE(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVE(p, MATH2, 0, IFIFOAB1, 0, 8, IMMED);
		}

		if (dir == OP_TYPE_DECAP_PROTOCOL) {
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, MATH1, 4,
			      IMMED2);
		} else {
			if (rta_sec_era > RTA_SEC_ERA_2) {
				MATHB(p, SEQINSZ, SUB, ZERO, MATH1, 4,
				      0);
			} else {
				MATHB(p, SEQINSZ, ADD, ONE, MATH1, 4,
				      0);
				MATHB(p, MATH1, SUB, ONE, MATH1, 4,
				      0);
			}
		}

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MATHB(p, MATH1, SUB, ZERO, VSEQINSZ, 4, 0);
			MATHB(p, MATH1, SUB, ZERO, VSEQOUTSZ, 4, 0);
		} else {
			MATHB(p, ZERO, ADD, MATH1, VSEQINSZ, 4, 0);
			MATHB(p, ZERO, ADD, MATH1, VSEQOUTSZ, 4, 0);

			/*
			 * Since MOVELEN is available only starting with
			 * SEC ERA 3, use poor man's MOVELEN: create a MOVE
			 * command dynamically by writing the length from M1 by
			 * OR-ing the command in the M1 register and MOVE the
			 * result into the descriptor buffer. Care must be taken
			 * wrt. the location of the command because of SEC
			 * pipelining. The actual MOVEs are written at the end
			 * of the descriptor due to calculations needed on the
			 * offset in the descriptor for the MOVE command.
			 */
			move_cmd_read_descbuf = MOVE(p, DESCBUF, 0, MATH1, 0, 6,
						     IMMED);
			move_cmd_write_descbuf = MOVE(p, MATH1, 0, DESCBUF, 0,
						      8, WAITCOMP | IMMED);
		}
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
				     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
			      DIR_ENC);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MOVE(p, AB2, 0, OFIFO, 0, MATH1, 0);
			SEQFIFOLOAD(p, MSGINSNOOP, 0,
				    VLF | LAST1 | LAST2 | FLUSH1);
		} else {
			SEQFIFOLOAD(p, MSGINSNOOP, 0,
				    VLF | LAST1 | LAST2 | FLUSH1);
			SET_LABEL(p, local_offset);

			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);

			/*
			 * Placeholder for MOVE command with length from
			 * M1 register
			 */
			MOVE(p, IFIFOAB2, 0, OFIFO, 0, 0, IMMED);

			/* Enable automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
		}

		if (dir == OP_TYPE_DECAP_PROTOCOL)
			SEQFIFOLOAD(p, ICV1, 4, LAST1 | FLUSH1);
		else
			SEQSTORE(p, CONTEXT1, 0, 4, 0);

		break;

	case PDCP_AUTH_TYPE_ZUC:
		if (rta_sec_era < RTA_SEC_ERA_5) {
			pr_err("Invalid era for selected algorithm\n");
			return -ENOTSUP;
		}
		/* Insert Auth Key */
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		SEQLOAD(p, MATH0, offset, length, 0);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
		SEQINPTR(p, 0, length, RTO);
		if (swap == false) {
			MATHB(p, MATH0, AND, sn_mask, MATH1, 8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

			MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, IMMED);

		} else {
			MATHB(p, MATH0, AND, sn_mask, MATH1, 8,
			      IFB | IMMED2);
			MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

			MOVE(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
			MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
			MOVE(p, MATH2, 0, CONTEXT2, 0, 8, IMMED);
		}
		if (dir == OP_TYPE_DECAP_PROTOCOL)
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, MATH1, 4,
			      IMMED2);
		else
			MATHB(p, SEQINSZ, SUB, ZERO, MATH1, 4, 0);

		MATHB(p, MATH1, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, MATH1, SUB, ZERO, VSEQOUTSZ, 4, 0);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA,
			      OP_ALG_AAI_F9,
			      OP_ALG_AS_INITFINAL,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
				     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
			      DIR_ENC);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);
		MOVE(p, AB1, 0, OFIFO, 0, MATH1, 0);

		if (dir == OP_TYPE_DECAP_PROTOCOL)
			SEQFIFOLOAD(p, ICV2, 4, LAST2);
		else
			SEQSTORE(p, CONTEXT2, 0, 4, 0);

		break;

	default:
		pr_err("%s: Invalid integrity algorithm selected: %d\n",
		       "pdcp_insert_cplane_int_only_op", authdata->algtype);
		return -EINVAL;
	}

	if (rta_sec_era < RTA_SEC_ERA_3) {
		PATCH_MOVE(p, move_cmd_read_descbuf, local_offset);
		PATCH_MOVE(p, move_cmd_write_descbuf, local_offset);
	}

	return 0;
}

static inline int
pdcp_insert_cplane_enc_only_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata __maybe_unused,
			       unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	/* Insert Cipher Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18 &&
			!(rta_sec_era == RTA_SEC_ERA_8 &&
				authdata->algtype == 0))
			|| (rta_sec_era == RTA_SEC_ERA_10)) {
		if (sn_size == PDCP_SN_SIZE_5)
			PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_CTRL_MIXED,
				 (uint16_t)cipherdata->algtype << 8);
		else
			PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER_RN,
				 (uint16_t)cipherdata->algtype << 8);
		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_12:
		offset = 6;
		length = 2;
		sn_mask = (swap == false) ? PDCP_12BIT_SN_MASK :
					PDCP_12BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
	SEQSTORE(p, MATH0, offset, length, 0);
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);

	switch (cipherdata->algtype) {
	case PDCP_CIPHER_TYPE_SNOW:
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, WAITCOMP | IMMED);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		} else {
			MATHB(p, SEQINSZ, SUB, ONE, MATH1, 4, 0);
			MATHB(p, MATH1, ADD, ONE, VSEQINSZ, 4, 0);
		}

		if (dir == OP_TYPE_ENCAP_PROTOCOL)
			MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
		else
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
					DIR_ENC : DIR_DEC);
		break;

	case PDCP_CIPHER_TYPE_AES:
		MOVEB(p, MATH2, 0, CONTEXT1, 0x10, 0x10, WAITCOMP | IMMED);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		} else {
			MATHB(p, SEQINSZ, SUB, ONE, MATH1, 4, 0);
			MATHB(p, MATH1, ADD, ONE, VSEQINSZ, 4, 0);
		}

		if (dir == OP_TYPE_ENCAP_PROTOCOL)
			MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
		else
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
					DIR_ENC : DIR_DEC);
		break;

	case PDCP_CIPHER_TYPE_ZUC:
		if (rta_sec_era < RTA_SEC_ERA_5) {
			pr_err("Invalid era for selected algorithm\n");
			return -ENOTSUP;
		}

		MOVEB(p, MATH2, 0, CONTEXT1, 0, 0x08, IMMED);
		MOVEB(p, MATH2, 0, CONTEXT1, 0x08, 0x08, WAITCOMP | IMMED);
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		if (dir == OP_TYPE_ENCAP_PROTOCOL)
			MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);
		else
			MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4,
			      IMMED2);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      dir == OP_TYPE_ENCAP_PROTOCOL ?
					DIR_ENC : DIR_DEC);
		break;

	default:
		pr_err("%s: Invalid encrypt algorithm selected: %d\n",
		       "pdcp_insert_cplane_enc_only_op", cipherdata->algtype);
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
		JUMP(p, PDCP_NULL_INT_ICV_CHECK_FAILED_STATUS,
		     HALT_STATUS, ALL_FALSE, MATH_Z);
	}

	return 0;
}

static inline int
pdcp_insert_uplane_snow_snow_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      unsigned int dir,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	if (rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) {
		int pclid;

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, SUB, length, VSEQINSZ, 4, IMMED2);

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	SEQSTORE(p, MATH0, offset, length, 0);
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH1, 8, 0);
	MOVEB(p, MATH1, 0, CONTEXT1, 0, 8, IMMED);
	MOVEB(p, MATH1, 0, CONTEXT2, 0, 4, WAITCOMP | IMMED);
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
	MATHB(p, MATH3, SHLD, MATH3, MATH3, 8, 0);

	MOVEB(p, MATH2, 4, OFIFO, 0, 12, IMMED);
	MOVE(p, OFIFO, 0, CONTEXT2, 4, 12, IMMED);
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	} else {
		MATHI(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
		MATHI(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQINSZ, 4, IMMED2);
	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
	else
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);

	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_DEC);
	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
		      OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST2);
		SEQFIFOLOAD(p, MSG1, 4, LAST1 | FLUSH1);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CLASS1 | NOP | NIFP);

		if (rta_sec_era >= RTA_SEC_ERA_6)
			LOAD(p, 0, DCTRL, 0, LDLEN_RST_CHA_OFIFO_PTR, IMMED);

		MOVE(p, OFIFO, 0, MATH0, 0, 4, WAITCOMP | IMMED);

		NFIFOADD(p, IFIFO, ICV2, 4, LAST2);

		if (rta_sec_era <= RTA_SEC_ERA_2) {
			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);
			MOVE(p, MATH0, 0, IFIFOAB2, 0, 4, WAITCOMP | IMMED);
		} else {
			MOVE(p, MATH0, 0, IFIFO, 0, 4, WAITCOMP | IMMED);
		}
	}

	return 0;
}

static inline int
pdcp_insert_uplane_zuc_zuc_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      unsigned int dir,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	LABEL(keyjump);
	REFERENCE(pkeyjump);

	if (rta_sec_era < RTA_SEC_ERA_5) {
		pr_err("Invalid era for selected algorithm\n");
		return -ENOTSUP;
	}

	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF | BOTH);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	SET_LABEL(p, keyjump);
	PATCH_JUMP(p, pkeyjump, keyjump);

	if (rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) {
		int pclid;

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);

	MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);

	MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, WAITCOMP | IMMED);

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

	MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
	} else {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST1 | FLUSH1);
	}

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_ENC);

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
		      OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		/* Save ICV */
		MOVEB(p, OFIFO, 0, MATH0, 0, 4, IMMED);

		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS2 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC2 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVEB(p, MATH0, 0, ALTSOURCE, 0, 4, WAITCOMP | IMMED);
	}

	/* Reset ZUCA mode and done interrupt */
	LOAD(p, CLRW_CLR_C2MODE, CLRW, 0, 4, IMMED);
	LOAD(p, CIRQ_ZADI, ICTRL, 0, 4, IMMED);

	return 0;
}

static inline int
pdcp_insert_uplane_aes_aes_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      unsigned int dir,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18)) {
		/* Insert Auth Key */
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		/* Insert Cipher Key */
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER_RN,
			 ((uint16_t)cipherdata->algtype << 8) |
			  (uint16_t)authdata->algtype);
		return 0;
	}

	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;

	default:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 8, MATH2, 0, 0x08, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		MOVEB(p, MATH2, 0, IFIFOAB1, 0, 0x08, IMMED);
		MOVEB(p, MATH0, offset, IFIFOAB1, 0, length, IMMED);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, VSEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);
		MOVEB(p, CONTEXT1, 0, MATH3, 0, 4, WAITCOMP | IMMED);

		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, IMMED);
		SEQINPTR(p, 0, PDCP_NULL_MAX_FRAME_LEN, RTO);

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		SEQFIFOLOAD(p, SKIP, length, 0);

		SEQFIFOLOAD(p, MSG1, 0, VLF);
		MOVEB(p, MATH3, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, IMMED);
		MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, IMMED);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);

		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		MOVEB(p, OFIFO, 0, MATH3, 0, 4, IMMED);

		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		SEQINPTR(p, 0, 0, SOP);

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_ENABLE,
			      DIR_DEC);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 8, IMMED);

		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS1 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC1 |
		     NFIFOENTRY_FC1 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVEB(p, MATH3, 0, ALTSOURCE, 0, 4, IMMED);
	}

	return 0;
}

static inline int
pdcp_insert_cplane_acc_op(struct program *p,
			  bool swap __maybe_unused,
			  struct alginfo *cipherdata,
			  struct alginfo *authdata,
			  unsigned int dir,
			  enum pdcp_sn_size sn_size,
			  unsigned char era_2_hfn_ovrd __maybe_unused)
{
	/* Insert Auth Key */
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	/* Insert Cipher Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	if (sn_size == PDCP_SN_SIZE_5)
		PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_CTRL,
			 (uint16_t)cipherdata->algtype);
	else
		PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER_RN,
			 ((uint16_t)cipherdata->algtype << 8) |
			  (uint16_t)authdata->algtype);

	return 0;
}

static inline int
pdcp_insert_cplane_snow_aes_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	LABEL(back_to_sd_offset);
	LABEL(end_desc);
	LABEL(local_offset);
	LABEL(jump_to_beginning);
	LABEL(fifo_load_mac_i_offset);
	REFERENCE(seqin_ptr_read);
	REFERENCE(seqin_ptr_write);
	REFERENCE(seq_out_read);
	REFERENCE(jump_back_to_sd_cmd);
	REFERENCE(move_mac_i_to_desc_buf);

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
				cipherdata->keylen, INLINE_KEY(cipherdata));
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
				authdata->keylen, INLINE_KEY(authdata));

		if (sn_size == PDCP_SN_SIZE_5)
			PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_CTRL_MIXED,
				 ((uint16_t)cipherdata->algtype << 8) |
				 (uint16_t)authdata->algtype);
		else
			PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER_RN,
				 ((uint16_t)cipherdata->algtype << 8) |
				 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 0x08, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	SEQSTORE(p, MATH0, offset, length, 0);
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		if (rta_sec_era > RTA_SEC_ERA_2 ||
		    (rta_sec_era == RTA_SEC_ERA_2 &&
				   era_2_sw_hfn_ovrd == 0)) {
			SEQINPTR(p, 0, length, RTO);
		} else {
			SEQINPTR(p, 0, 5, RTO);
			SEQFIFOLOAD(p, SKIP, 4, 0);
		}
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		MOVEB(p, MATH2, 0, IFIFOAB1, 0, 0x08, IMMED);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
			MATHB(p, SEQINSZ, SUB, ZERO, MATH1, 4, 0);
			MATHB(p, VSEQINSZ, ADD, PDCP_MAC_I_LEN - 1, VSEQOUTSZ,
			      4, IMMED2);
		} else {
			MATHB(p, SEQINSZ, SUB, MATH3, VSEQINSZ, 4, 0);
			MATHB(p, VSEQINSZ, ADD, PDCP_MAC_I_LEN - 1, VSEQOUTSZ,
			      4, IMMED2);
			/*
			 * Note: Although the calculations below might seem a
			 * little off, the logic is the following:
			 *
			 * - SEQ IN PTR RTO below needs the full length of the
			 *   frame; in case of P4080_REV_2_HFN_OV_WORKAROUND,
			 *   this means the length of the frame to be processed
			 *   + 4 bytes (the HFN override flag and value).
			 *   The length of the frame to be processed minus 1
			 *   byte is in the VSIL register (because
			 *   VSIL = SIL + 3, due to 1 byte, the header being
			 *   already written by the SEQ STORE above). So for
			 *   calculating the length to use in RTO, I add one
			 *   to the VSIL value in order to obtain the total
			 *   frame length. This helps in case of P4080 which
			 *   can have the value 0 as an operand in a MATH
			 *   command only as SRC1 When the HFN override
			 *   workaround is not enabled, the length of the
			 *   frame is given by the SIL register; the
			 *   calculation is similar to the one in the SEC 4.2
			 *   and SEC 5.3 cases.
			 */
			if (era_2_sw_hfn_ovrd)
				MATHB(p, VSEQOUTSZ, ADD, ONE, MATH1, 4,
				      0);
			else
				MATHB(p, SEQINSZ, ADD, MATH3, MATH1, 4,
				      0);
		}
		/*
		 * Placeholder for filling the length in
		 * SEQIN PTR RTO below
		 */
		seqin_ptr_read = MOVE(p, DESCBUF, 0, MATH1, 0, 6, IMMED);
		seqin_ptr_write = MOVE(p, MATH1, 0, DESCBUF, 0, 8,
				       WAITCOMP | IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);
		MOVEB(p, CONTEXT1, 0, MATH3, 0, 4, WAITCOMP | IMMED);
		if (rta_sec_era <= RTA_SEC_ERA_3)
			LOAD(p, CLRW_CLR_C1KEY |
			     CLRW_CLR_C1CTX |
			     CLRW_CLR_C1ICV |
			     CLRW_CLR_C1DATAS |
			     CLRW_CLR_C1MODE,
			     CLRW, 0, 4, IMMED);
		else
			LOAD(p, CLRW_RESET_CLS1_CHA |
			     CLRW_CLR_C1KEY |
			     CLRW_CLR_C1CTX |
			     CLRW_CLR_C1ICV |
			     CLRW_CLR_C1DATAS |
			     CLRW_CLR_C1MODE,
			     CLRW, 0, 4, IMMED);

		if (rta_sec_era <= RTA_SEC_ERA_3)
			LOAD(p, CCTRL_RESET_CHA_ALL, CCTRL, 0, 4, IMMED);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
		SET_LABEL(p, local_offset);
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
		SEQINPTR(p, 0, 0, RTO);

		if (rta_sec_era == RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd) {
			SEQFIFOLOAD(p, SKIP, 5, 0);
			MATHB(p, SEQINSZ, ADD, ONE, SEQINSZ, 4, 0);
		}

		MATHB(p, SEQINSZ, SUB, length, VSEQINSZ, 4, IMMED2);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		if (rta_sec_era > RTA_SEC_ERA_2 ||
		    (rta_sec_era == RTA_SEC_ERA_2 &&
				   era_2_sw_hfn_ovrd == 0))
			SEQFIFOLOAD(p, SKIP, length, 0);

		SEQFIFOLOAD(p, MSG1, 0, VLF);
		MOVEB(p, MATH3, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
		PATCH_MOVE(p, seqin_ptr_read, local_offset);
		PATCH_MOVE(p, seqin_ptr_write, local_offset);
	} else {
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);

		if (rta_sec_era >= RTA_SEC_ERA_5)
			MOVE(p, CONTEXT1, 0, CONTEXT2, 0, 8, IMMED);

		if (rta_sec_era > RTA_SEC_ERA_2)
			MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		else
			MATHB(p, SEQINSZ, SUB, MATH3, VSEQINSZ, 4, 0);

		MATHI(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
/*
 * TODO: To be changed when proper support is added in RTA (can't load a
 * command that is also written by RTA (or patch it for that matter).
 * Change when proper RTA support is added.
 */
		if (p->ps)
			WORD(p, 0x168B0004);
		else
			WORD(p, 0x16880404);

		jump_back_to_sd_cmd = JUMP(p, 0, LOCAL_JUMP, ALL_TRUE, 0);
		/*
		 * Placeholder for command reading  the SEQ OUT command in
		 * JD. Done for rereading the decrypted data and performing
		 * the integrity check
		 */
/*
 * TODO: RTA currently doesn't support patching of length of a MOVE command
 * Thus, it is inserted as a raw word, as per PS setting.
 */
		if (p->ps)
			seq_out_read = MOVE(p, DESCBUF, 0, MATH1, 0, 20,
					    WAITCOMP | IMMED);
		else
			seq_out_read = MOVE(p, DESCBUF, 0, MATH1, 0, 16,
					    WAITCOMP | IMMED);

		MATHB(p, MATH1, XOR, CMD_SEQ_IN_PTR ^ CMD_SEQ_OUT_PTR, MATH1, 4,
		      IMMED2);
		/* Placeholder for overwriting the SEQ IN  with SEQ OUT */
/*
 * TODO: RTA currently doesn't support patching of length of a MOVE command
 * Thus, it is inserted as a raw word, as per PS setting.
 */
		if (p->ps)
			MOVE(p, MATH1, 0, DESCBUF, 0, 24, IMMED);
		else
			MOVE(p, MATH1, 0, DESCBUF, 0, 20, IMMED);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		if (rta_sec_era >= RTA_SEC_ERA_4)
			MOVE(p, CONTEXT1, 0, CONTEXT2, 0, 8, IMMED);
		else
			MOVE(p, CONTEXT1, 0, MATH3, 0, 8, IMMED);

		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		if (rta_sec_era <= RTA_SEC_ERA_3)
			move_mac_i_to_desc_buf = MOVE(p, OFIFO, 0, DESCBUF, 0,
						      4, WAITCOMP | IMMED);
		else
			MOVE(p, OFIFO, 0, MATH3, 0, 4, IMMED);

		if (rta_sec_era <= RTA_SEC_ERA_3)
			LOAD(p, CCTRL_RESET_CHA_ALL, CCTRL, 0, 4, IMMED);
		else
			LOAD(p, CLRW_RESET_CLS1_CHA |
			     CLRW_CLR_C1KEY |
			     CLRW_CLR_C1CTX |
			     CLRW_CLR_C1ICV |
			     CLRW_CLR_C1DATAS |
			     CLRW_CLR_C1MODE,
			     CLRW, 0, 4, IMMED);

		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		/*
		 * Placeholder for jump in SD for executing the new SEQ IN PTR
		 * command (which is actually the old SEQ OUT PTR command
		 * copied over from JD.
		 */
		SET_LABEL(p, jump_to_beginning);
		JUMP(p, 1 - jump_to_beginning, LOCAL_JUMP, ALL_TRUE, 0);
		SET_LABEL(p, back_to_sd_offset);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_ENABLE,
			      DIR_DEC);

		/* Read the # of bytes written in the output buffer + 1 (HDR) */
		MATHI(p, VSEQOUTSZ, ADD, length, VSEQINSZ, 4, IMMED2);

		if (rta_sec_era <= RTA_SEC_ERA_3)
			MOVE(p, MATH3, 0, IFIFOAB1, 0, 8, IMMED);
		else
			MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 8, IMMED);

		if (rta_sec_era == RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd)
			SEQFIFOLOAD(p, SKIP, 4, 0);

		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		if (rta_sec_era >= RTA_SEC_ERA_4) {
			LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
			     NFIFOENTRY_DEST_CLASS1 |
			     NFIFOENTRY_DTYPE_ICV |
			     NFIFOENTRY_LC1 |
			     NFIFOENTRY_FC1 | 4, NFIFO_SZL, 0, 4, IMMED);
			MOVE(p, MATH3, 0, ALTSOURCE, 0, 4, IMMED);
		} else {
			SET_LABEL(p, fifo_load_mac_i_offset);
			FIFOLOAD(p, ICV1, fifo_load_mac_i_offset, 4,
				 LAST1 | FLUSH1 | IMMED);
		}

		SET_LABEL(p, end_desc);

		if (!p->ps) {
			PATCH_MOVE(p, seq_out_read, end_desc + 1);
			PATCH_JUMP(p, jump_back_to_sd_cmd,
				   back_to_sd_offset + jump_back_to_sd_cmd - 5);

			if (rta_sec_era <= RTA_SEC_ERA_3)
				PATCH_MOVE(p, move_mac_i_to_desc_buf,
					   fifo_load_mac_i_offset + 1);
		} else {
			PATCH_MOVE(p, seq_out_read, end_desc + 2);
			PATCH_JUMP(p, jump_back_to_sd_cmd,
				   back_to_sd_offset + jump_back_to_sd_cmd - 5);

			if (rta_sec_era <= RTA_SEC_ERA_3)
				PATCH_MOVE(p, move_mac_i_to_desc_buf,
					   fifo_load_mac_i_offset + 1);
		}
	}

	return 0;
}

static inline int
pdcp_insert_cplane_aes_snow_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		int pclid;

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, SUB, length, VSEQINSZ, 4, IMMED2);

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	SEQSTORE(p, MATH0, offset, length, 0);
	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH1, 8, 0);
	MOVEB(p, MATH1, 0, CONTEXT1, 16, 8, IMMED);
	MOVEB(p, MATH1, 0, CONTEXT2, 0, 4, IMMED);
	if (swap == false) {
		MATHB(p, MATH1, AND, upper_32_bits(PDCP_BEARER_MASK), MATH2, 4,
		      IMMED2);
		MATHB(p, MATH1, AND, lower_32_bits(PDCP_DIR_MASK), MATH3, 4,
		      IMMED2);
	} else {
		MATHB(p, MATH1, AND, lower_32_bits(PDCP_BEARER_MASK_BE), MATH2,
			4, IMMED2);
		MATHB(p, MATH1, AND, upper_32_bits(PDCP_DIR_MASK_BE), MATH3,
			4, IMMED2);
	}
	MATHB(p, MATH3, SHLD, MATH3, MATH3, 8, 0);
	MOVEB(p, MATH2, 4, OFIFO, 0, 12, IMMED);
	MOVE(p, OFIFO, 0, CONTEXT2, 4, 12, IMMED);
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	} else {
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, MATH1, 4, IMMED2);

		MATHB(p, ZERO, ADD, MATH1, VSEQOUTSZ, 4, 0);
		MATHB(p, ZERO, ADD, MATH1, VSEQINSZ, 4, 0);
	}

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
	else
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);

	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_DEC);
	ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
		      OP_ALG_AAI_CTR,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST2);
		SEQFIFOLOAD(p, MSG1, 4, LAST1 | FLUSH1);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CLASS1 | NOP | NIFP);

		if (rta_sec_era >= RTA_SEC_ERA_6)
			LOAD(p, 0, DCTRL, 0, LDLEN_RST_CHA_OFIFO_PTR, IMMED);

		MOVE(p, OFIFO, 0, MATH0, 0, 4, WAITCOMP | IMMED);

		NFIFOADD(p, IFIFO, ICV2, 4, LAST2);

		if (rta_sec_era <= RTA_SEC_ERA_2) {
			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);
			MOVE(p, MATH0, 0, IFIFOAB2, 0, 4, WAITCOMP | IMMED);
		} else {
			MOVE(p, MATH0, 0, IFIFO, 0, 4, WAITCOMP | IMMED);
		}
	}

	return 0;
}

static inline int
pdcp_insert_cplane_snow_zuc_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;

	LABEL(keyjump);
	REFERENCE(pkeyjump);

	if (rta_sec_era < RTA_SEC_ERA_5) {
		pr_err("Invalid era for selected algorithm\n");
		return -ENOTSUP;
	}

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		int pclid;
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
		    INLINE_KEY(authdata));

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);
		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}

	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF | BOTH);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	SET_LABEL(p, keyjump);
	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
	MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, WAITCOMP | IMMED);

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

	MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
	} else {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST1 | FLUSH1);
	}

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_ENC);

	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
		      OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		/* Save ICV */
		MOVE(p, OFIFO, 0, MATH0, 0, 4, IMMED);
		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS2 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC2 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVE(p, MATH0, 0, ALTSOURCE, 0, 4, WAITCOMP | IMMED);
	}

	/* Reset ZUCA mode and done interrupt */
	LOAD(p, CLRW_CLR_C2MODE, CLRW, 0, 4, IMMED);
	LOAD(p, CIRQ_ZADI, ICTRL, 0, 4, IMMED);

	PATCH_JUMP(p, pkeyjump, keyjump);
	return 0;
}

static inline int
pdcp_insert_cplane_aes_zuc_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      unsigned int dir,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	LABEL(keyjump);
	REFERENCE(pkeyjump);

	if (rta_sec_era < RTA_SEC_ERA_5) {
		pr_err("Invalid era for selected algorithm\n");
		return -ENOTSUP;
	}

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		int pclid;
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
		    INLINE_KEY(authdata));

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}
	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF | BOTH);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	SET_LABEL(p, keyjump);

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	MOVEB(p, MATH2, 0, CONTEXT1, 16, 8, IMMED);
	MOVEB(p, MATH2, 0, CONTEXT2, 0, 8, WAITCOMP | IMMED);

	if (dir == OP_TYPE_ENCAP_PROTOCOL)
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
	else
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

	MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
	} else {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST1 | FLUSH1);
	}

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_ENC);

	ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
		      OP_ALG_AAI_CTR,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		/* Save ICV */
		MOVE(p, OFIFO, 0, MATH0, 0, 4, IMMED);

		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS2 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC2 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVE(p, MATH0, 0, ALTSOURCE, 0, 4, WAITCOMP | IMMED);
	}

	/* Reset ZUCA mode and done interrupt */
	LOAD(p, CLRW_CLR_C2MODE, CLRW, 0, 4, IMMED);
	LOAD(p, CIRQ_ZADI, ICTRL, 0, 4, IMMED);

	PATCH_JUMP(p, pkeyjump, keyjump);

	return 0;
}

static inline int
pdcp_insert_cplane_zuc_snow_op(struct program *p,
			       bool swap __maybe_unused,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned int dir,
			       enum pdcp_sn_size sn_size,
			       unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	LABEL(keyjump);
	REFERENCE(pkeyjump);

	if (rta_sec_era < RTA_SEC_ERA_5) {
		pr_err("Invalid era for selected algorithm\n");
		return -ENOTSUP;
	}

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		int pclid;
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
		    INLINE_KEY(authdata));

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);

		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;

	}
	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF | BOTH);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	SET_LABEL(p, keyjump);
	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MOVEB(p, MATH0, offset, IFIFOAB2, 0, length, IMMED);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH1, 8, 0);
	MOVEB(p, MATH1, 0, CONTEXT1, 0, 8, IMMED);
	MOVEB(p, MATH1, 0, CONTEXT2, 0, 4, IMMED);
	if (swap == false) {
		MATHB(p, MATH1, AND, upper_32_bits(PDCP_BEARER_MASK), MATH2,
		      4, IMMED2);
		MATHB(p, MATH1, AND, lower_32_bits(PDCP_DIR_MASK), MATH3,
		      4, IMMED2);
	} else {
		MATHB(p, MATH1, AND, lower_32_bits(PDCP_BEARER_MASK_BE), MATH2,
			4, IMMED2);
		MATHB(p, MATH1, AND, upper_32_bits(PDCP_DIR_MASK_BE), MATH3,
			4, IMMED2);
	}
	MATHB(p, MATH3, SHLD, MATH3, MATH3, 8, 0);
	MOVEB(p, MATH2, 4, OFIFO, 0, 12, IMMED);
	MOVE(p, OFIFO, 0, CONTEXT2, 4, 12, IMMED);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MATHB(p, SEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
	} else {
		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);
		MATHB(p, VSEQOUTSZ, SUB, ZERO, VSEQINSZ, 4, 0);
	}

	SEQSTORE(p, MATH0, offset, length, 0);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST2);
	} else {
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST2);
	}

	ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9,
		      OP_ALG_AAI_F9,
		      OP_ALG_AS_INITFINAL,
		      dir == OP_TYPE_ENCAP_PROTOCOL ?
			     ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      DIR_DEC);

	ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
		      OP_ALG_AAI_F8,
		      OP_ALG_AS_INITFINAL,
		      ICV_CHECK_DISABLE,
		      dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC);

	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		SEQFIFOLOAD(p, MSG1, 4, LAST1 | FLUSH1);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CLASS1 | NOP | NIFP);

		if (rta_sec_era >= RTA_SEC_ERA_6)
			/*
			 * For SEC ERA 6, there's a problem with the OFIFO
			 * pointer, and thus it needs to be reset here before
			 * moving to M0.
			 */
			LOAD(p, 0, DCTRL, 0, LDLEN_RST_CHA_OFIFO_PTR, IMMED);

		/* Put ICV to M0 before sending it to C2 for comparison. */
		MOVEB(p, OFIFO, 0, MATH0, 0, 4, WAITCOMP | IMMED);

		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS2 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC2 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVEB(p, MATH0, 0, ALTSOURCE, 0, 4, IMMED);
	}

	PATCH_JUMP(p, pkeyjump, keyjump);
	return 0;
}

static inline int
pdcp_insert_cplane_zuc_aes_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      unsigned int dir,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd __maybe_unused)
{
	uint32_t offset = 0, length = 0, sn_mask = 0;
	if (rta_sec_era < RTA_SEC_ERA_5) {
		pr_err("Invalid era for selected algorithm\n");
		return -ENOTSUP;
	}

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size != PDCP_SN_SIZE_18) ||
		(rta_sec_era == RTA_SEC_ERA_10)) {
		int pclid;

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
				cipherdata->keylen, INLINE_KEY(cipherdata));
		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
				authdata->keylen, INLINE_KEY(authdata));

		if (sn_size == PDCP_SN_SIZE_5)
			pclid = OP_PCLID_LTE_PDCP_CTRL_MIXED;
		else
			pclid = OP_PCLID_LTE_PDCP_USER_RN;

		PROTOCOL(p, dir, pclid,
			 ((uint16_t)cipherdata->algtype << 8) |
			 (uint16_t)authdata->algtype);
		return 0;
	}
	/* Non-proto is supported only for 5bit cplane and 18bit uplane */
	switch (sn_size) {
	case PDCP_SN_SIZE_5:
		offset = 7;
		length = 1;
		sn_mask = (swap == false) ? PDCP_C_PLANE_SN_MASK :
					PDCP_C_PLANE_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_18:
		offset = 5;
		length = 3;
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
		break;
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
	case PDCP_SN_SIZE_15:
		pr_err("Invalid sn_size for %s\n", __func__);
		return -ENOTSUP;
	}

	SEQLOAD(p, MATH0, offset, length, 0);
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 4, MATH2, 0, 0x08, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);
	SEQSTORE(p, MATH0, offset, length, 0);
	if (dir == OP_TYPE_ENCAP_PROTOCOL) {
		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		MOVEB(p, MATH2, 0, IFIFOAB1, 0, 0x08, IMMED);
		MOVEB(p, MATH0, offset, IFIFOAB1, 0, length, IMMED);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, VSEQINSZ, ADD, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);
		MOVEB(p, CONTEXT1, 0, MATH3, 0, 4, WAITCOMP | IMMED);
		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);
		SEQINPTR(p, 0, PDCP_NULL_MAX_FRAME_LEN, RTO);

		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		SEQFIFOLOAD(p, SKIP, length, 0);

		SEQFIFOLOAD(p, MSG1, 0, VLF);
		MOVEB(p, MATH3, 0, IFIFOAB1, 0, 4, LAST1 | FLUSH1 | IMMED);
	} else {
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, IMMED);

		MOVE(p, CONTEXT1, 0, CONTEXT2, 0, 8, IMMED);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

		MATHB(p, SEQINSZ, SUB, PDCP_MAC_I_LEN, VSEQOUTSZ, 4, IMMED2);

		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));

		MOVE(p, CONTEXT1, 0, CONTEXT2, 0, 8, IMMED);

		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_DEC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF | CONT);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		MOVEB(p, OFIFO, 0, MATH3, 0, 4, IMMED);

		LOAD(p, CLRW_RESET_CLS1_CHA |
		     CLRW_CLR_C1KEY |
		     CLRW_CLR_C1CTX |
		     CLRW_CLR_C1ICV |
		     CLRW_CLR_C1DATAS |
		     CLRW_CLR_C1MODE,
		     CLRW, 0, 4, IMMED);

		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));

		SEQINPTR(p, 0, 0, SOP);

		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_ENABLE,
			      DIR_DEC);

		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);

		MOVE(p, CONTEXT2, 0, IFIFOAB1, 0, 8, IMMED);

		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

		LOAD(p, NFIFOENTRY_STYPE_ALTSOURCE |
		     NFIFOENTRY_DEST_CLASS1 |
		     NFIFOENTRY_DTYPE_ICV |
		     NFIFOENTRY_LC1 |
		     NFIFOENTRY_FC1 | 4, NFIFO_SZL, 0, 4, IMMED);
		MOVEB(p, MATH3, 0, ALTSOURCE, 0, 4, IMMED);
	}

	return 0;
}

static inline int
pdcp_insert_uplane_no_int_op(struct program *p,
			    bool swap __maybe_unused,
			    struct alginfo *cipherdata,
			    unsigned int dir,
			    enum pdcp_sn_size sn_size)
{
	int op;
	uint32_t sn_mask;

	/* Insert Cipher Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	if ((rta_sec_era >= RTA_SEC_ERA_8 && sn_size == PDCP_SN_SIZE_15) ||
			(rta_sec_era >= RTA_SEC_ERA_10)) {
		PROTOCOL(p, dir, OP_PCLID_LTE_PDCP_USER,
			 (uint16_t)cipherdata->algtype);
		return 0;
	}

	if (sn_size == PDCP_SN_SIZE_15) {
		SEQLOAD(p, MATH0, 6, 2, 0);
		sn_mask = (swap == false) ? PDCP_U_PLANE_15BIT_SN_MASK :
					PDCP_U_PLANE_15BIT_SN_MASK_BE;
	} else { /* SN Size == PDCP_SN_SIZE_18 */
		SEQLOAD(p, MATH0, 5, 3, 0);
		sn_mask = (swap == false) ? PDCP_U_PLANE_18BIT_SN_MASK :
					PDCP_U_PLANE_18BIT_SN_MASK_BE;
	}
	JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
	MATHB(p, MATH0, AND, sn_mask, MATH1, 8, IFB | IMMED2);

	if (sn_size == PDCP_SN_SIZE_15)
		SEQSTORE(p, MATH0, 6, 2, 0);
	else /* SN Size == PDCP_SN_SIZE_18 */
		SEQSTORE(p, MATH0, 5, 3, 0);

	MATHB(p, MATH1, SHLD, MATH1, MATH1, 8, 0);
	MOVEB(p, DESCBUF, 8, MATH2, 0, 8, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, MATH2, MATH2, 8, 0);

	MATHB(p, SEQINSZ, SUB, MATH3, VSEQINSZ, 4, 0);
	MATHB(p, SEQINSZ, SUB, MATH3, VSEQOUTSZ, 4, 0);

	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	op = dir == OP_TYPE_ENCAP_PROTOCOL ? DIR_ENC : DIR_DEC;
	switch (cipherdata->algtype) {
	case PDCP_CIPHER_TYPE_SNOW:
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 8, WAITCOMP | IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F8,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      op);
		break;

	case PDCP_CIPHER_TYPE_AES:
		MOVEB(p, MATH2, 0, CONTEXT1, 0x10, 0x10, WAITCOMP | IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CTR,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      op);
		break;

	case PDCP_CIPHER_TYPE_ZUC:
		if (rta_sec_era < RTA_SEC_ERA_5) {
			pr_err("Invalid era for selected algorithm\n");
			return -ENOTSUP;
		}
		MOVEB(p, MATH2, 0, CONTEXT1, 0, 0x08, IMMED);
		MOVEB(p, MATH2, 0, CONTEXT1, 0x08, 0x08, WAITCOMP | IMMED);

		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCE,
			      OP_ALG_AAI_F8,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      op);
		break;

	default:
		pr_err("%s: Invalid encrypt algorithm selected: %d\n",
		       "pdcp_insert_uplane_15bit_op", cipherdata->algtype);
		return -EINVAL;
	}

	SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | FLUSH1);

	return 0;
}

/*
 * Function for inserting the snippet of code responsible for creating
 * the HFN override code via either DPOVRD or via the input frame.
 */
static inline int
insert_hfn_ov_op(struct program *p,
		 uint32_t shift,
		 enum pdb_type_e pdb_type,
		 unsigned char era_2_sw_hfn_ovrd)
{
	uint32_t imm = PDCP_DPOVRD_HFN_OV_EN;
	uint16_t hfn_pdb_offset;
	LABEL(keyjump);
	REFERENCE(pkeyjump);

	if (rta_sec_era == RTA_SEC_ERA_2 && !era_2_sw_hfn_ovrd)
		return 0;

	switch (pdb_type) {
	case PDCP_PDB_TYPE_NO_PDB:
		/*
		 * If there is no PDB, then HFN override mechanism does not
		 * make any sense, thus in this case the function will
		 * return the pointer to the current position in the
		 * descriptor buffer
		 */
		return 0;

	case PDCP_PDB_TYPE_REDUCED_PDB:
		hfn_pdb_offset = 4;
		break;

	case PDCP_PDB_TYPE_FULL_PDB:
		hfn_pdb_offset = 8;
		break;

	default:
		return -EINVAL;
	}

	if (rta_sec_era > RTA_SEC_ERA_2) {
		MATHB(p, DPOVRD, AND, imm, NONE, 8, IFB | IMMED2);
	} else {
		SEQLOAD(p, MATH0, 4, 4, 0);
		JUMP(p, 1, LOCAL_JUMP, ALL_TRUE, CALM);
		MATHB(p, MATH0, AND, imm, NONE, 8, IFB | IMMED2);
		SEQSTORE(p, MATH0, 4, 4, 0);
	}

	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, MATH_Z);

	if (rta_sec_era > RTA_SEC_ERA_2)
		MATHI(p, DPOVRD, LSHIFT, shift, MATH0, 4, IMMED2);
	else
		MATHB(p, MATH0, LSHIFT, shift, MATH0, 4, IMMED2);

	MATHB(p, MATH0, SHLD, MATH0, MATH0, 8, 0);
	MOVE(p, MATH0, 0, DESCBUF, hfn_pdb_offset, 4, IMMED);

	if (rta_sec_era >= RTA_SEC_ERA_8)
		/*
		 * For ERA8, DPOVRD could be handled by the PROTOCOL command
		 * itself. For now, this is not done. Thus, clear DPOVRD here
		 * to alleviate any side-effects.
		 */
		MATHB(p, DPOVRD, AND, ZERO, DPOVRD, 4, STL);

	SET_LABEL(p, keyjump);
	PATCH_JUMP(p, pkeyjump, keyjump);
	return 0;
}

/*
 * PDCP Control PDB creation function
 */
static inline enum pdb_type_e
cnstr_pdcp_c_plane_pdb(struct program *p,
		       uint32_t hfn,
		       enum pdcp_sn_size sn_size,
		       unsigned char bearer,
		       unsigned char direction,
		       uint32_t hfn_threshold,
		       struct alginfo *cipherdata,
		       struct alginfo *authdata)
{
	struct pdcp_pdb pdb;
	enum pdb_type_e
		pdb_mask[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
			{	/* NULL */
				PDCP_PDB_TYPE_NO_PDB,		/* NULL */
				PDCP_PDB_TYPE_FULL_PDB,		/* SNOW f9 */
				PDCP_PDB_TYPE_FULL_PDB,		/* AES CMAC */
				PDCP_PDB_TYPE_FULL_PDB		/* ZUC-I */
			},
			{	/* SNOW f8 */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_FULL_PDB,		/* SNOW f9 */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* AES CMAC */
				PDCP_PDB_TYPE_REDUCED_PDB	/* ZUC-I */
			},
			{	/* AES CTR */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* SNOW f9 */
				PDCP_PDB_TYPE_FULL_PDB,		/* AES CMAC */
				PDCP_PDB_TYPE_REDUCED_PDB	/* ZUC-I */
			},
			{	/* ZUC-E */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* SNOW f9 */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* AES CMAC */
				PDCP_PDB_TYPE_FULL_PDB		/* ZUC-I */
			},
	};

	if (rta_sec_era >= RTA_SEC_ERA_8) {
		memset(&pdb, 0x00, sizeof(struct pdcp_pdb));

		/* To support 12-bit seq numbers, we use u-plane opt in pdb.
		 * SEC supports 5-bit only with c-plane opt in pdb.
		 */
		if (sn_size == PDCP_SN_SIZE_12) {
			pdb.hfn_res = hfn << PDCP_U_PLANE_PDB_LONG_SN_HFN_SHIFT;
			pdb.bearer_dir_res = (uint32_t)
				((bearer << PDCP_U_PLANE_PDB_BEARER_SHIFT) |
				 (direction << PDCP_U_PLANE_PDB_DIR_SHIFT));

			pdb.hfn_thr_res =
			hfn_threshold << PDCP_U_PLANE_PDB_LONG_SN_HFN_THR_SHIFT;

		} else {
			/* This means 5-bit c-plane.
			 * Here we use c-plane opt in pdb
			 */

			/* This is a HW issue. Bit 2 should be set to zero,
			 * but it does not work this way. Override here.
			 */
			pdb.opt_res.rsvd = 0x00000002;

			/* Copy relevant information from user to PDB */
			pdb.hfn_res = hfn << PDCP_C_PLANE_PDB_HFN_SHIFT;
			pdb.bearer_dir_res = (uint32_t)
				((bearer << PDCP_C_PLANE_PDB_BEARER_SHIFT) |
				(direction << PDCP_C_PLANE_PDB_DIR_SHIFT));
			pdb.hfn_thr_res =
			hfn_threshold << PDCP_C_PLANE_PDB_HFN_THR_SHIFT;
		}

		/* copy PDB in descriptor*/
		__rta_out32(p, pdb.opt_res.opt);
		__rta_out32(p, pdb.hfn_res);
		__rta_out32(p, pdb.bearer_dir_res);
		__rta_out32(p, pdb.hfn_thr_res);

		return PDCP_PDB_TYPE_FULL_PDB;
	}

	switch (pdb_mask[cipherdata->algtype][authdata->algtype]) {
	case PDCP_PDB_TYPE_NO_PDB:
		break;

	case PDCP_PDB_TYPE_REDUCED_PDB:
		__rta_out32(p, (hfn << PDCP_C_PLANE_PDB_HFN_SHIFT));
		__rta_out32(p,
			    (uint32_t)((bearer <<
					PDCP_C_PLANE_PDB_BEARER_SHIFT) |
					(direction <<
					 PDCP_C_PLANE_PDB_DIR_SHIFT)));
		break;

	case PDCP_PDB_TYPE_FULL_PDB:
		memset(&pdb, 0x00, sizeof(struct pdcp_pdb));

		/* This is a HW issue. Bit 2 should be set to zero,
		 * but it does not work this way. Override here.
		 */
		pdb.opt_res.rsvd = 0x00000002;

		/* Copy relevant information from user to PDB */
		pdb.hfn_res = hfn << PDCP_C_PLANE_PDB_HFN_SHIFT;
		pdb.bearer_dir_res = (uint32_t)
			((bearer << PDCP_C_PLANE_PDB_BEARER_SHIFT) |
			 (direction << PDCP_C_PLANE_PDB_DIR_SHIFT));
		pdb.hfn_thr_res =
			hfn_threshold << PDCP_C_PLANE_PDB_HFN_THR_SHIFT;

		/* copy PDB in descriptor*/
		__rta_out32(p, pdb.opt_res.opt);
		__rta_out32(p, pdb.hfn_res);
		__rta_out32(p, pdb.bearer_dir_res);
		__rta_out32(p, pdb.hfn_thr_res);

		break;

	default:
		return PDCP_PDB_TYPE_INVALID;
	}

	return pdb_mask[cipherdata->algtype][authdata->algtype];
}

/*
 * PDCP UPlane PDB creation function
 */
static inline enum pdb_type_e
cnstr_pdcp_u_plane_pdb(struct program *p,
		       enum pdcp_sn_size sn_size,
		       uint32_t hfn, unsigned short bearer,
		       unsigned short direction,
		       uint32_t hfn_threshold,
		       struct alginfo *cipherdata,
		       struct alginfo *authdata)
{
	struct pdcp_pdb pdb;
	enum pdb_type_e pdb_type = PDCP_PDB_TYPE_FULL_PDB;
	enum pdb_type_e
		pdb_mask[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
			{	/* NULL */
				PDCP_PDB_TYPE_NO_PDB,		/* NULL */
				PDCP_PDB_TYPE_FULL_PDB,		/* SNOW f9 */
				PDCP_PDB_TYPE_FULL_PDB,		/* AES CMAC */
				PDCP_PDB_TYPE_FULL_PDB		/* ZUC-I */
			},
			{	/* SNOW f8 */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_FULL_PDB,		/* SNOW f9 */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* AES CMAC */
				PDCP_PDB_TYPE_REDUCED_PDB	/* ZUC-I */
			},
			{	/* AES CTR */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* SNOW f9 */
				PDCP_PDB_TYPE_FULL_PDB,		/* AES CMAC */
				PDCP_PDB_TYPE_REDUCED_PDB	/* ZUC-I */
			},
			{	/* ZUC-E */
				PDCP_PDB_TYPE_FULL_PDB,		/* NULL */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* SNOW f9 */
				PDCP_PDB_TYPE_REDUCED_PDB,	/* AES CMAC */
				PDCP_PDB_TYPE_FULL_PDB		/* ZUC-I */
			},
	};

	/* Read options from user */
	/* Depending on sequence number length, the HFN and HFN threshold
	 * have different lengths.
	 */
	memset(&pdb, 0x00, sizeof(struct pdcp_pdb));

	switch (sn_size) {
	case PDCP_SN_SIZE_7:
		pdb.opt_res.opt |= PDCP_U_PLANE_PDB_OPT_SHORT_SN;
		pdb.hfn_res = hfn << PDCP_U_PLANE_PDB_SHORT_SN_HFN_SHIFT;
		pdb.hfn_thr_res =
			hfn_threshold<<PDCP_U_PLANE_PDB_SHORT_SN_HFN_THR_SHIFT;
		break;

	case PDCP_SN_SIZE_12:
		pdb.opt_res.opt &= (uint32_t)(~PDCP_U_PLANE_PDB_OPT_SHORT_SN);
		pdb.hfn_res = hfn << PDCP_U_PLANE_PDB_LONG_SN_HFN_SHIFT;
		pdb.hfn_thr_res =
			hfn_threshold<<PDCP_U_PLANE_PDB_LONG_SN_HFN_THR_SHIFT;
		break;

	case PDCP_SN_SIZE_15:
		pdb.opt_res.opt = (uint32_t)(PDCP_U_PLANE_PDB_OPT_15B_SN);
		pdb.hfn_res = hfn << PDCP_U_PLANE_PDB_15BIT_SN_HFN_SHIFT;
		pdb.hfn_thr_res =
			hfn_threshold<<PDCP_U_PLANE_PDB_15BIT_SN_HFN_THR_SHIFT;
		break;

	case PDCP_SN_SIZE_18:
		pdb.opt_res.opt = (uint32_t)(PDCP_U_PLANE_PDB_OPT_18B_SN);
		pdb.hfn_res = hfn << PDCP_U_PLANE_PDB_18BIT_SN_HFN_SHIFT;
		pdb.hfn_thr_res =
			hfn_threshold<<PDCP_U_PLANE_PDB_18BIT_SN_HFN_THR_SHIFT;

		if (rta_sec_era <= RTA_SEC_ERA_8) {
			if (cipherdata && authdata)
				pdb_type = pdb_mask[cipherdata->algtype]
						   [authdata->algtype];
		}
		break;

	default:
		pr_err("Invalid Sequence Number Size setting in PDB\n");
		return -EINVAL;
	}

	pdb.bearer_dir_res = (uint32_t)
				((bearer << PDCP_U_PLANE_PDB_BEARER_SHIFT) |
				 (direction << PDCP_U_PLANE_PDB_DIR_SHIFT));

	switch (pdb_type) {
	case PDCP_PDB_TYPE_NO_PDB:
		break;

	case PDCP_PDB_TYPE_REDUCED_PDB:
		__rta_out32(p, pdb.hfn_res);
		__rta_out32(p, pdb.bearer_dir_res);
		break;

	case PDCP_PDB_TYPE_FULL_PDB:
		/* copy PDB in descriptor*/
		__rta_out32(p, pdb.opt_res.opt);
		__rta_out32(p, pdb.hfn_res);
		__rta_out32(p, pdb.bearer_dir_res);
		__rta_out32(p, pdb.hfn_thr_res);

		break;

	default:
		return PDCP_PDB_TYPE_INVALID;
	}

	return pdb_type;
}
/**
 * cnstr_shdsc_pdcp_c_plane_encap - Function for creating a PDCP Control Plane
 *                                  encapsulation descriptor.
 * @descbuf: pointer to buffer for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @hfn: starting Hyper Frame Number to be used together with the SN from the
 *       PDCP frames.
 * @sn_size: size of sequence numbers, only 5/12 bit sequence numbers are valid
 * @bearer: radio bearer ID
 * @direction: the direction of the PDCP frame (UL/DL)
 * @hfn_threshold: HFN value that once reached triggers a warning from SEC that
 *                 keys should be renegotiated at the earliest convenience.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values are those from cipher_type_pdcp enum.
 * @authdata: pointer to authentication transform definitions
 *            Valid algorithm values are those from auth_type_pdcp enum.
 * @era_2_sw_hfn_ovrd: if software HFN override mechanism is desired for
 *                     this descriptor. Note: Can only be used for
 *                     SEC ERA 2.
 * Return: size of descriptor written in words or negative number on error.
 *         Once the function returns, the value of this parameter can be used
 *         for reclaiming the space that wasn't used for the descriptor.
 *
 * Note: descbuf must be large enough to contain a full 256 byte long
 * descriptor; after the function returns, by subtracting the actual number of
 * bytes used, the user can reuse the remaining buffer space for other purposes.
 */
static inline int
cnstr_shdsc_pdcp_c_plane_encap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       uint32_t hfn,
			       enum pdcp_sn_size sn_size,
			       unsigned char bearer,
			       unsigned char direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned char era_2_sw_hfn_ovrd)
{
	static int
		(*pdcp_cp_fp[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID])
			(struct program*, bool swap, struct alginfo *,
			 struct alginfo *, unsigned int, enum pdcp_sn_size,
			unsigned char __maybe_unused) = {
		{	/* NULL */
			pdcp_insert_cplane_null_op,	/* NULL */
			pdcp_insert_cplane_int_only_op,	/* SNOW f9 */
			pdcp_insert_cplane_int_only_op,	/* AES CMAC */
			pdcp_insert_cplane_int_only_op	/* ZUC-I */
		},
		{	/* SNOW f8 */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_acc_op,	/* SNOW f9 */
			pdcp_insert_cplane_snow_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_snow_zuc_op	/* ZUC-I */
		},
		{	/* AES CTR */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_aes_snow_op,	/* SNOW f9 */
			pdcp_insert_cplane_acc_op,	/* AES CMAC */
			pdcp_insert_cplane_aes_zuc_op	/* ZUC-I */
		},
		{	/* ZUC-E */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_zuc_snow_op,	/* SNOW f9 */
			pdcp_insert_cplane_zuc_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_acc_op	/* ZUC-I */
		},
	};
	static enum rta_share_type
		desc_share[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
		{	/* NULL */
			SHR_WAIT,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
		{	/* SNOW f8 */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* AES CTR */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* ZUC-E */
			SHR_ALWAYS,	/* NULL */
			SHR_WAIT,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
	};
	enum pdb_type_e pdb_type;
	struct program prg;
	struct program *p = &prg;
	int err;
	LABEL(pdb_end);

	if (rta_sec_era != RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd) {
		pr_err("Cannot select SW HFN override for other era than 2");
		return -EINVAL;
	}

	if (sn_size != PDCP_SN_SIZE_12 && sn_size != PDCP_SN_SIZE_5) {
		pr_err("C-plane supports only 5-bit and 12-bit sequence numbers\n");
		return -EINVAL;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, desc_share[cipherdata->algtype][authdata->algtype], 0, 0);

	pdb_type = cnstr_pdcp_c_plane_pdb(p,
			hfn,
			sn_size,
			bearer,
			direction,
			hfn_threshold,
			cipherdata,
			authdata);

	SET_LABEL(p, pdb_end);

	err = insert_hfn_ov_op(p, sn_size, pdb_type,
			       era_2_sw_hfn_ovrd);
	if (err)
		return err;

	err = pdcp_cp_fp[cipherdata->algtype][authdata->algtype](p,
		swap,
		cipherdata,
		authdata,
		OP_TYPE_ENCAP_PROTOCOL,
		sn_size,
		era_2_sw_hfn_ovrd);
	if (err)
		return err;

	PATCH_HDR(p, 0, pdb_end);

	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_pdcp_c_plane_decap - Function for creating a PDCP Control Plane
 *                                  decapsulation descriptor.
 * @descbuf: pointer to buffer for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @hfn: starting Hyper Frame Number to be used together with the SN from the
 *       PDCP frames.
 * @sn_size: size of sequence numbers, only 5/12 bit sequence numbers are valid
 * @bearer: radio bearer ID
 * @direction: the direction of the PDCP frame (UL/DL)
 * @hfn_threshold: HFN value that once reached triggers a warning from SEC that
 *                 keys should be renegotiated at the earliest convenience.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values are those from cipher_type_pdcp enum.
 * @authdata: pointer to authentication transform definitions
 *            Valid algorithm values are those from auth_type_pdcp enum.
 * @era_2_sw_hfn_ovrd: if software HFN override mechanism is desired for
 *                     this descriptor. Note: Can only be used for
 *                     SEC ERA 2.
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
cnstr_shdsc_pdcp_c_plane_decap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       uint32_t hfn,
			       enum pdcp_sn_size sn_size,
			       unsigned char bearer,
			       unsigned char direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned char era_2_sw_hfn_ovrd)
{
	static int
		(*pdcp_cp_fp[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID])
			(struct program*, bool swap, struct alginfo *,
			 struct alginfo *, unsigned int, enum pdcp_sn_size,
			 unsigned char) = {
		{	/* NULL */
			pdcp_insert_cplane_null_op,	/* NULL */
			pdcp_insert_cplane_int_only_op,	/* SNOW f9 */
			pdcp_insert_cplane_int_only_op,	/* AES CMAC */
			pdcp_insert_cplane_int_only_op	/* ZUC-I */
		},
		{	/* SNOW f8 */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_acc_op,	/* SNOW f9 */
			pdcp_insert_cplane_snow_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_snow_zuc_op	/* ZUC-I */
		},
		{	/* AES CTR */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_aes_snow_op,	/* SNOW f9 */
			pdcp_insert_cplane_acc_op,	/* AES CMAC */
			pdcp_insert_cplane_aes_zuc_op	/* ZUC-I */
		},
		{	/* ZUC-E */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_zuc_snow_op,	/* SNOW f9 */
			pdcp_insert_cplane_zuc_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_acc_op	/* ZUC-I */
		},
	};
	static enum rta_share_type
		desc_share[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
		{	/* NULL */
			SHR_WAIT,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
		{	/* SNOW f8 */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* AES CTR */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* ZUC-E */
			SHR_ALWAYS,	/* NULL */
			SHR_WAIT,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
	};
	enum pdb_type_e pdb_type;
	struct program prg;
	struct program *p = &prg;
	int err;
	LABEL(pdb_end);

	if (rta_sec_era != RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd) {
		pr_err("Cannot select SW HFN override for other era than 2");
		return -EINVAL;
	}

	if (sn_size != PDCP_SN_SIZE_12 && sn_size != PDCP_SN_SIZE_5) {
		pr_err("C-plane supports only 5-bit and 12-bit sequence numbers\n");
		return -EINVAL;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, desc_share[cipherdata->algtype][authdata->algtype], 0, 0);

	pdb_type = cnstr_pdcp_c_plane_pdb(p,
			hfn,
			sn_size,
			bearer,
			direction,
			hfn_threshold,
			cipherdata,
			authdata);

	SET_LABEL(p, pdb_end);

	err = insert_hfn_ov_op(p, sn_size, pdb_type,
			       era_2_sw_hfn_ovrd);
	if (err)
		return err;

	err = pdcp_cp_fp[cipherdata->algtype][authdata->algtype](p,
		swap,
		cipherdata,
		authdata,
		OP_TYPE_DECAP_PROTOCOL,
		sn_size,
		era_2_sw_hfn_ovrd);
	if (err)
		return err;

	PATCH_HDR(p, 0, pdb_end);

	return PROGRAM_FINALIZE(p);
}

static int
pdcp_insert_uplane_with_int_op(struct program *p,
			      bool swap __maybe_unused,
			      struct alginfo *cipherdata,
			      struct alginfo *authdata,
			      enum pdcp_sn_size sn_size,
			      unsigned char era_2_sw_hfn_ovrd,
			      unsigned int dir)
{
	static int
		(*pdcp_cp_fp[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID])
			(struct program*, bool swap, struct alginfo *,
			 struct alginfo *, unsigned int, enum pdcp_sn_size,
			unsigned char __maybe_unused) = {
		{	/* NULL */
			pdcp_insert_cplane_null_op,	/* NULL */
			pdcp_insert_cplane_int_only_op,	/* SNOW f9 */
			pdcp_insert_cplane_int_only_op,	/* AES CMAC */
			pdcp_insert_cplane_int_only_op	/* ZUC-I */
		},
		{	/* SNOW f8 */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_uplane_snow_snow_op, /* SNOW f9 */
			pdcp_insert_cplane_snow_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_snow_zuc_op	/* ZUC-I */
		},
		{	/* AES CTR */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_aes_snow_op,	/* SNOW f9 */
			pdcp_insert_uplane_aes_aes_op,	/* AES CMAC */
			pdcp_insert_cplane_aes_zuc_op	/* ZUC-I */
		},
		{	/* ZUC-E */
			pdcp_insert_cplane_enc_only_op,	/* NULL */
			pdcp_insert_cplane_zuc_snow_op,	/* SNOW f9 */
			pdcp_insert_cplane_zuc_aes_op,	/* AES CMAC */
			pdcp_insert_uplane_zuc_zuc_op	/* ZUC-I */
		},
	};
	int err;

	err = pdcp_cp_fp[cipherdata->algtype][authdata->algtype](p,
		swap,
		cipherdata,
		authdata,
		dir,
		sn_size,
		era_2_sw_hfn_ovrd);
	if (err)
		return err;

	return 0;
}


/**
 * cnstr_shdsc_pdcp_u_plane_encap - Function for creating a PDCP User Plane
 *                                  encapsulation descriptor.
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
 * @era_2_sw_hfn_ovrd: if software HFN override mechanism is desired for
 *                     this descriptor. Note: Can only be used for
 *                     SEC ERA 2.
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
cnstr_shdsc_pdcp_u_plane_encap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       enum pdcp_sn_size sn_size,
			       uint32_t hfn,
			       unsigned short bearer,
			       unsigned short direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned char era_2_sw_hfn_ovrd)
{
	struct program prg;
	struct program *p = &prg;
	int err;
	enum pdb_type_e pdb_type;
	static enum rta_share_type
		desc_share[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
		{	/* NULL */
			SHR_WAIT,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
		{	/* SNOW f8 */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* AES CTR */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* ZUC-E */
			SHR_ALWAYS,	/* NULL */
			SHR_WAIT,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
	};
	LABEL(pdb_end);

	if (rta_sec_era != RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd) {
		pr_err("Cannot select SW HFN ovrd for other era than 2");
		return -EINVAL;
	}

	if (authdata && !authdata->algtype && rta_sec_era < RTA_SEC_ERA_8) {
		pr_err("Cannot use u-plane auth with era < 8");
		return -EINVAL;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	if (authdata)
		SHR_HDR(p, desc_share[cipherdata->algtype][authdata->algtype], 0, 0);
	else
		SHR_HDR(p, SHR_ALWAYS, 0, 0);
	pdb_type = cnstr_pdcp_u_plane_pdb(p, sn_size, hfn,
					  bearer, direction, hfn_threshold,
					  cipherdata, authdata);
	if (pdb_type == PDCP_PDB_TYPE_INVALID) {
		pr_err("Error creating PDCP UPlane PDB\n");
		return -EINVAL;
	}
	SET_LABEL(p, pdb_end);

	err = insert_hfn_ov_op(p, sn_size, pdb_type, era_2_sw_hfn_ovrd);
	if (err)
		return err;

	switch (sn_size) {
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
		switch (cipherdata->algtype) {
		case PDCP_CIPHER_TYPE_ZUC:
			if (rta_sec_era < RTA_SEC_ERA_5) {
				pr_err("Invalid era for selected algorithm\n");
				return -ENOTSUP;
			}
			/* fallthrough */
		case PDCP_CIPHER_TYPE_AES:
		case PDCP_CIPHER_TYPE_SNOW:
		case PDCP_CIPHER_TYPE_NULL:
			if (rta_sec_era == RTA_SEC_ERA_8 &&
					authdata && authdata->algtype == 0){
				err = pdcp_insert_uplane_with_int_op(p, swap,
						cipherdata, authdata,
						sn_size, era_2_sw_hfn_ovrd,
						OP_TYPE_ENCAP_PROTOCOL);
				if (err)
					return err;
				break;
			}

			if (pdb_type != PDCP_PDB_TYPE_FULL_PDB) {
				pr_err("PDB type must be FULL for PROTO desc\n");
				return -EINVAL;
			}

			/* Insert auth key if requested */
			if (authdata && authdata->algtype) {
				KEY(p, KEY2, authdata->key_enc_flags,
				    (uint64_t)authdata->key, authdata->keylen,
				    INLINE_KEY(authdata));
			}
			/* Insert Cipher Key */
			KEY(p, KEY1, cipherdata->key_enc_flags,
			    (uint64_t)cipherdata->key, cipherdata->keylen,
			    INLINE_KEY(cipherdata));

			if (authdata)
				PROTOCOL(p, OP_TYPE_ENCAP_PROTOCOL,
					 OP_PCLID_LTE_PDCP_USER_RN,
					 ((uint16_t)cipherdata->algtype << 8) |
					 (uint16_t)authdata->algtype);
			else
				PROTOCOL(p, OP_TYPE_ENCAP_PROTOCOL,
					 OP_PCLID_LTE_PDCP_USER,
					 (uint16_t)cipherdata->algtype);
			break;
		default:
			pr_err("%s: Invalid encrypt algorithm selected: %d\n",
			       "cnstr_pcl_shdsc_pdcp_u_plane_decap",
			       cipherdata->algtype);
			return -EINVAL;
		}
		break;

	case PDCP_SN_SIZE_15:
	case PDCP_SN_SIZE_18:
		if (authdata) {
			err = pdcp_insert_uplane_with_int_op(p, swap,
					cipherdata, authdata,
					sn_size, era_2_sw_hfn_ovrd,
					OP_TYPE_ENCAP_PROTOCOL);
			if (err)
				return err;

			break;
		}

		switch (cipherdata->algtype) {
		case PDCP_CIPHER_TYPE_NULL:
			insert_copy_frame_op(p,
					     cipherdata,
					     OP_TYPE_ENCAP_PROTOCOL);
			break;

		default:
			err = pdcp_insert_uplane_no_int_op(p, swap, cipherdata,
					OP_TYPE_ENCAP_PROTOCOL, sn_size);
			if (err)
				return err;
			break;
		}
		break;

	case PDCP_SN_SIZE_5:
	default:
		pr_err("Invalid SN size selected\n");
		return -ENOTSUP;
	}

	PATCH_HDR(p, 0, pdb_end);
	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_pdcp_u_plane_decap - Function for creating a PDCP User Plane
 *                                  decapsulation descriptor.
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
 * @era_2_sw_hfn_ovrd: if software HFN override mechanism is desired for
 *                     this descriptor. Note: Can only be used for
 *                     SEC ERA 2.
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
cnstr_shdsc_pdcp_u_plane_decap(uint32_t *descbuf,
			       bool ps,
			       bool swap,
			       enum pdcp_sn_size sn_size,
			       uint32_t hfn,
			       unsigned short bearer,
			       unsigned short direction,
			       uint32_t hfn_threshold,
			       struct alginfo *cipherdata,
			       struct alginfo *authdata,
			       unsigned char era_2_sw_hfn_ovrd)
{
	struct program prg;
	struct program *p = &prg;
	int err;
	enum pdb_type_e pdb_type;
	static enum rta_share_type
		desc_share[PDCP_CIPHER_TYPE_INVALID][PDCP_AUTH_TYPE_INVALID] = {
		{	/* NULL */
			SHR_WAIT,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
		{	/* SNOW f8 */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* AES CTR */
			SHR_ALWAYS,	/* NULL */
			SHR_ALWAYS,	/* SNOW f9 */
			SHR_ALWAYS,	/* AES CMAC */
			SHR_WAIT	/* ZUC-I */
		},
		{	/* ZUC-E */
			SHR_ALWAYS,	/* NULL */
			SHR_WAIT,	/* SNOW f9 */
			SHR_WAIT,	/* AES CMAC */
			SHR_ALWAYS	/* ZUC-I */
		},
	};

	LABEL(pdb_end);

	if (rta_sec_era != RTA_SEC_ERA_2 && era_2_sw_hfn_ovrd) {
		pr_err("Cannot select SW HFN override for other era than 2");
		return -EINVAL;
	}

	if (authdata && !authdata->algtype && rta_sec_era < RTA_SEC_ERA_8) {
		pr_err("Cannot use u-plane auth with era < 8");
		return -EINVAL;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	if (authdata)
		SHR_HDR(p, desc_share[cipherdata->algtype][authdata->algtype], 0, 0);
	else
		SHR_HDR(p, SHR_ALWAYS, 0, 0);

	pdb_type = cnstr_pdcp_u_plane_pdb(p, sn_size, hfn, bearer,
					  direction, hfn_threshold,
					  cipherdata, authdata);
	if (pdb_type == PDCP_PDB_TYPE_INVALID) {
		pr_err("Error creating PDCP UPlane PDB\n");
		return -EINVAL;
	}
	SET_LABEL(p, pdb_end);

	err = insert_hfn_ov_op(p, sn_size, pdb_type, era_2_sw_hfn_ovrd);
	if (err)
		return err;

	switch (sn_size) {
	case PDCP_SN_SIZE_7:
	case PDCP_SN_SIZE_12:
		switch (cipherdata->algtype) {
		case PDCP_CIPHER_TYPE_ZUC:
			if (rta_sec_era < RTA_SEC_ERA_5) {
				pr_err("Invalid era for selected algorithm\n");
				return -ENOTSUP;
			}
			/* fallthrough */
		case PDCP_CIPHER_TYPE_AES:
		case PDCP_CIPHER_TYPE_SNOW:
		case PDCP_CIPHER_TYPE_NULL:
			if (pdb_type != PDCP_PDB_TYPE_FULL_PDB) {
				pr_err("PDB type must be FULL for PROTO desc\n");
				return -EINVAL;
			}

			/* Insert auth key if requested */
			if (authdata && authdata->algtype)
				KEY(p, KEY2, authdata->key_enc_flags,
				    (uint64_t)authdata->key, authdata->keylen,
				    INLINE_KEY(authdata));
			else if (authdata && authdata->algtype == 0) {
				err = pdcp_insert_uplane_with_int_op(p, swap,
						cipherdata, authdata,
						sn_size, era_2_sw_hfn_ovrd,
						OP_TYPE_DECAP_PROTOCOL);
				if (err)
					return err;
				break;
			}

			/* Insert Cipher Key */
			KEY(p, KEY1, cipherdata->key_enc_flags,
			    cipherdata->key, cipherdata->keylen,
			    INLINE_KEY(cipherdata));
			if (authdata)
				PROTOCOL(p, OP_TYPE_DECAP_PROTOCOL,
					 OP_PCLID_LTE_PDCP_USER_RN,
					 ((uint16_t)cipherdata->algtype << 8) |
					 (uint16_t)authdata->algtype);
			else
				PROTOCOL(p, OP_TYPE_DECAP_PROTOCOL,
					 OP_PCLID_LTE_PDCP_USER,
					 (uint16_t)cipherdata->algtype);
			break;
		default:
			pr_err("%s: Invalid encrypt algorithm selected: %d\n",
			       "cnstr_pcl_shdsc_pdcp_u_plane_decap",
			       cipherdata->algtype);
			return -EINVAL;
		}
		break;

	case PDCP_SN_SIZE_15:
	case PDCP_SN_SIZE_18:
		if (authdata) {
			err = pdcp_insert_uplane_with_int_op(p, swap,
					cipherdata, authdata,
					sn_size, era_2_sw_hfn_ovrd,
					OP_TYPE_DECAP_PROTOCOL);
			if (err)
				return err;

			break;
		}

		switch (cipherdata->algtype) {
		case PDCP_CIPHER_TYPE_NULL:
			insert_copy_frame_op(p,
					     cipherdata,
					     OP_TYPE_DECAP_PROTOCOL);
			break;

		default:
			err = pdcp_insert_uplane_no_int_op(p, swap, cipherdata,
				OP_TYPE_DECAP_PROTOCOL, sn_size);
			if (err)
				return err;
			break;
		}
		break;

	case PDCP_SN_SIZE_5:
	default:
		pr_err("Invalid SN size selected\n");
		return -ENOTSUP;
	}

	PATCH_HDR(p, 0, pdb_end);
	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_pdcp_short_mac - Function for creating a PDCP Short MAC
 *                              descriptor.
 * @descbuf: pointer to buffer for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @authdata: pointer to authentication transform definitions
 *            Valid algorithm values are those from auth_type_pdcp enum.
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
cnstr_shdsc_pdcp_short_mac(uint32_t *descbuf,
			   bool ps,
			   bool swap,
			   struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;
	uint32_t iv[3] = {0, 0, 0};
	LABEL(local_offset);
	REFERENCE(move_cmd_read_descbuf);
	REFERENCE(move_cmd_write_descbuf);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	SHR_HDR(p, SHR_ALWAYS, 1, 0);

	if (rta_sec_era > RTA_SEC_ERA_2) {
		MATHB(p, SEQINSZ, SUB, ZERO, VSEQINSZ, 4, 0);
		MATHB(p, SEQINSZ, SUB, ZERO, MATH1, 4, 0);
	} else {
		MATHB(p, SEQINSZ, ADD, ONE, MATH1, 4, 0);
		MATHB(p, MATH1, SUB, ONE, MATH1, 4, 0);
		MATHB(p, ZERO, ADD, MATH1, VSEQINSZ, 4, 0);
		MOVE(p, MATH1, 0, MATH0, 0, 8, IMMED);

		/*
		 * Since MOVELEN is available only starting with
		 * SEC ERA 3, use poor man's MOVELEN: create a MOVE
		 * command dynamically by writing the length from M1 by
		 * OR-ing the command in the M1 register and MOVE the
		 * result into the descriptor buffer. Care must be taken
		 * wrt. the location of the command because of SEC
		 * pipelining. The actual MOVEs are written at the end
		 * of the descriptor due to calculations needed on the
		 * offset in the descriptor for the MOVE command.
		 */
		move_cmd_read_descbuf = MOVE(p, DESCBUF, 0, MATH0, 0, 6,
					     IMMED);
		move_cmd_write_descbuf = MOVE(p, MATH0, 0, DESCBUF, 0, 8,
					      WAITCOMP | IMMED);
	}
	MATHB(p, ZERO, ADD, MATH1, VSEQOUTSZ, 4, 0);

	switch (authdata->algtype) {
	case PDCP_AUTH_TYPE_NULL:
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		if (rta_sec_era > RTA_SEC_ERA_2) {
			MOVE(p, AB1, 0, OFIFO, 0, MATH1, 0);
		} else {
			SET_LABEL(p, local_offset);

			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);

			/* Placeholder for MOVE command with length from M1
			 * register
			 */
			MOVE(p, IFIFOAB1, 0, OFIFO, 0, 0, IMMED);

			/* Enable automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
		}

		LOAD(p, (uintptr_t)iv, MATH0, 0, 8, IMMED | COPY);
		SEQFIFOLOAD(p, MSG1, 0, VLF | LAST1 | LAST2 | FLUSH1);
		SEQSTORE(p, MATH0, 0, 4, 0);

		break;

	case PDCP_AUTH_TYPE_SNOW:
		iv[0] = 0xFFFFFFFF;
		iv[1] = swap ? swab32(0x04000000) : 0x04000000;
		iv[2] = swap ? swab32(0xF8000000) : 0xF8000000;

		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		LOAD(p, (uintptr_t)&iv, CONTEXT2, 0, 12, IMMED | COPY);
		ALG_OPERATION(p, OP_ALG_ALGSEL_SNOW_F9,
			      OP_ALG_AAI_F9,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MOVE(p, AB1, 0, OFIFO, 0, MATH1, 0);
		} else {
			SET_LABEL(p, local_offset);


			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);

			/* Placeholder for MOVE command with length from M1
			 * register
			 */
			MOVE(p, IFIFOAB1, 0, OFIFO, 0, 0, IMMED);

			/* Enable automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
		}
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);
		SEQSTORE(p, CONTEXT2, 0, 4, 0);

		break;

	case PDCP_AUTH_TYPE_AES:
		iv[0] = 0xFFFFFFFF;
		iv[1] = swap ? swab32(0xFC000000) : 0xFC000000;
		iv[2] = 0x00000000; /* unused */

		KEY(p, KEY1, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		LOAD(p, (uintptr_t)&iv, MATH0, 0, 8, IMMED | COPY);
		MOVE(p, MATH0, 0, IFIFOAB1, 0, 8, IMMED);
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES,
			      OP_ALG_AAI_CMAC,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);

		if (rta_sec_era > RTA_SEC_ERA_2) {
			MOVE(p, AB2, 0, OFIFO, 0, MATH1, 0);
		} else {
			SET_LABEL(p, local_offset);

			/* Shut off automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_DISABLE_AUTO_NFIFO, 0, IMMED);

			/* Placeholder for MOVE command with length from M1
			 * register
			 */
			MOVE(p, IFIFOAB2, 0, OFIFO, 0, 0, IMMED);

			/* Enable automatic Info FIFO entries */
			LOAD(p, 0, DCTRL, LDOFF_ENABLE_AUTO_NFIFO, 0, IMMED);
		}
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);
		SEQSTORE(p, CONTEXT1, 0, 4, 0);

		break;

	case PDCP_AUTH_TYPE_ZUC:
		if (rta_sec_era < RTA_SEC_ERA_5) {
			pr_err("Invalid era for selected algorithm\n");
			return -ENOTSUP;
		}
		iv[0] = 0xFFFFFFFF;
		iv[1] = swap ? swab32(0xFC000000) : 0xFC000000;
		iv[2] = 0x00000000; /* unused */

		KEY(p, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		LOAD(p, (uintptr_t)&iv, CONTEXT2, 0, 12, IMMED | COPY);
		ALG_OPERATION(p, OP_ALG_ALGSEL_ZUCA,
			      OP_ALG_AAI_F9,
			      OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE,
			      DIR_ENC);
		SEQFIFOSTORE(p, MSG, 0, 0, VLF);
		MOVE(p, AB1, 0, OFIFO, 0, MATH1, 0);
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);
		SEQSTORE(p, CONTEXT2, 0, 4, 0);

		break;

	default:
		pr_err("%s: Invalid integrity algorithm selected: %d\n",
		       "cnstr_shdsc_pdcp_short_mac", authdata->algtype);
		return -EINVAL;
	}


	if (rta_sec_era < RTA_SEC_ERA_3) {
		PATCH_MOVE(p, move_cmd_read_descbuf, local_offset);
		PATCH_MOVE(p, move_cmd_write_descbuf, local_offset);
	}

	return PROGRAM_FINALIZE(p);
}

#endif /* __DESC_PDCP_H__ */
