/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
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

#ifndef __RTA_LOAD_CMD_H__
#define __RTA_LOAD_CMD_H__

extern enum rta_sec_era rta_sec_era;

/* Allowed length and offset masks for each SEC Era in case DST = DCTRL */
static const uint32_t load_len_mask_allowed[] = {
	0x000000ee,
	0x000000fe,
	0x000000fe,
	0x000000fe,
	0x000000fe,
	0x000000fe,
	0x000000fe,
	0x000000fe
};

static const uint32_t load_off_mask_allowed[] = {
	0x0000000f,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff
};

#define IMM_MUST 0
#define IMM_CAN  1
#define IMM_NO   2
#define IMM_DSNM 3 /* it doesn't matter the src type */

enum e_lenoff {
	LENOF_03,
	LENOF_4,
	LENOF_48,
	LENOF_448,
	LENOF_18,
	LENOF_32,
	LENOF_24,
	LENOF_16,
	LENOF_8,
	LENOF_128,
	LENOF_256,
	DSNM /* it doesn't matter the length/offset values */
};

struct load_map {
	uint32_t dst;
	uint32_t dst_opcode;
	enum e_lenoff len_off;
	uint8_t imm_src;

};

static const struct load_map load_dst[] = {
/*1*/	{ KEY1SZ,  LDST_CLASS_1_CCB | LDST_SRCDST_WORD_KEYSZ_REG,
		   LENOF_4,   IMM_MUST },
	{ KEY2SZ,  LDST_CLASS_2_CCB | LDST_SRCDST_WORD_KEYSZ_REG,
		   LENOF_4,   IMM_MUST },
	{ DATA1SZ, LDST_CLASS_1_CCB | LDST_SRCDST_WORD_DATASZ_REG,
		   LENOF_448, IMM_MUST },
	{ DATA2SZ, LDST_CLASS_2_CCB | LDST_SRCDST_WORD_DATASZ_REG,
		   LENOF_448, IMM_MUST },
	{ ICV1SZ,  LDST_CLASS_1_CCB | LDST_SRCDST_WORD_ICVSZ_REG,
		   LENOF_4,   IMM_MUST },
	{ ICV2SZ,  LDST_CLASS_2_CCB | LDST_SRCDST_WORD_ICVSZ_REG,
		   LENOF_4,   IMM_MUST },
	{ CCTRL,   LDST_CLASS_IND_CCB | LDST_SRCDST_WORD_CHACTRL,
		   LENOF_4,   IMM_MUST },
	{ DCTRL,   LDST_CLASS_DECO | LDST_IMM | LDST_SRCDST_WORD_DECOCTRL,
		   DSNM,      IMM_DSNM },
	{ ICTRL,   LDST_CLASS_IND_CCB | LDST_SRCDST_WORD_IRQCTRL,
		   LENOF_4,   IMM_MUST },
	{ DPOVRD,  LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_PCLOVRD,
		   LENOF_4,   IMM_MUST },
	{ CLRW,    LDST_CLASS_IND_CCB | LDST_SRCDST_WORD_CLRW,
		   LENOF_4,   IMM_MUST },
	{ AAD1SZ,  LDST_CLASS_1_CCB | LDST_SRCDST_WORD_DECO_AAD_SZ,
		   LENOF_4,   IMM_MUST },
	{ IV1SZ,   LDST_CLASS_1_CCB | LDST_SRCDST_WORD_CLASS1_IV_SZ,
		   LENOF_4,   IMM_MUST },
	{ ALTDS1,  LDST_CLASS_1_CCB | LDST_SRCDST_WORD_ALTDS_CLASS1,
		   LENOF_448, IMM_MUST },
	{ PKASZ,   LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_A_SZ,
		   LENOF_4,   IMM_MUST, },
	{ PKBSZ,   LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_B_SZ,
		   LENOF_4,   IMM_MUST },
	{ PKNSZ,   LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_N_SZ,
		   LENOF_4,   IMM_MUST },
	{ PKESZ,   LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_E_SZ,
		   LENOF_4,   IMM_MUST },
	{ NFIFO,   LDST_CLASS_IND_CCB | LDST_SRCDST_WORD_INFO_FIFO,
		   LENOF_48,  IMM_MUST },
	{ IFIFO,   LDST_SRCDST_BYTE_INFIFO,  LENOF_18, IMM_MUST },
	{ OFIFO,   LDST_SRCDST_BYTE_OUTFIFO, LENOF_18, IMM_MUST },
	{ MATH0,   LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH0,
		   LENOF_32,  IMM_CAN },
	{ MATH1,   LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH1,
		   LENOF_24,  IMM_CAN },
	{ MATH2,   LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH2,
		   LENOF_16,  IMM_CAN },
	{ MATH3,   LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH3,
		   LENOF_8,   IMM_CAN },
	{ CONTEXT1, LDST_CLASS_1_CCB | LDST_SRCDST_BYTE_CONTEXT,
		   LENOF_128, IMM_CAN },
	{ CONTEXT2, LDST_CLASS_2_CCB | LDST_SRCDST_BYTE_CONTEXT,
		   LENOF_128, IMM_CAN },
	{ KEY1,    LDST_CLASS_1_CCB | LDST_SRCDST_BYTE_KEY,
		   LENOF_32,  IMM_CAN },
	{ KEY2,    LDST_CLASS_2_CCB | LDST_SRCDST_BYTE_KEY,
		   LENOF_32,  IMM_CAN },
	{ DESCBUF, LDST_CLASS_DECO | LDST_SRCDST_WORD_DESCBUF,
		   LENOF_256,  IMM_NO },
	{ DPID,    LDST_CLASS_DECO | LDST_SRCDST_WORD_PID,
		   LENOF_448, IMM_MUST },
/*32*/	{ IDFNS,   LDST_SRCDST_WORD_IFNSR, LENOF_18,  IMM_MUST },
	{ ODFNS,   LDST_SRCDST_WORD_OFNSR, LENOF_18,  IMM_MUST },
	{ ALTSOURCE, LDST_SRCDST_BYTE_ALTSOURCE, LENOF_18,  IMM_MUST },
/*35*/	{ NFIFO_SZL, LDST_SRCDST_WORD_INFO_FIFO_SZL, LENOF_48, IMM_MUST },
	{ NFIFO_SZM, LDST_SRCDST_WORD_INFO_FIFO_SZM, LENOF_03, IMM_MUST },
	{ NFIFO_L, LDST_SRCDST_WORD_INFO_FIFO_L, LENOF_48, IMM_MUST },
	{ NFIFO_M, LDST_SRCDST_WORD_INFO_FIFO_M, LENOF_03, IMM_MUST },
	{ SZL,     LDST_SRCDST_WORD_SZL, LENOF_48, IMM_MUST },
/*40*/	{ SZM,     LDST_SRCDST_WORD_SZM, LENOF_03, IMM_MUST }
};

/*
 * Allowed LOAD destinations for each SEC Era.
 * Values represent the number of entries from load_dst[] that are supported.
 */
static const unsigned int load_dst_sz[] = { 31, 34, 34, 40, 40, 40, 40, 40 };

static inline int
load_check_len_offset(int pos, uint32_t length, uint32_t offset)
{
	if ((load_dst[pos].dst == DCTRL) &&
	    ((length & ~load_len_mask_allowed[rta_sec_era]) ||
	     (offset & ~load_off_mask_allowed[rta_sec_era])))
		goto err;

	switch (load_dst[pos].len_off) {
	case (LENOF_03):
		if ((length > 3) || (offset))
			goto err;
		break;
	case (LENOF_4):
		if ((length != 4) || (offset != 0))
			goto err;
		break;
	case (LENOF_48):
		if (!(((length == 4) && (offset == 0)) ||
		      ((length == 8) && (offset == 0))))
			goto err;
		break;
	case (LENOF_448):
		if (!(((length == 4) && (offset == 0)) ||
		      ((length == 4) && (offset == 4)) ||
		      ((length == 8) && (offset == 0))))
			goto err;
		break;
	case (LENOF_18):
		if ((length < 1) || (length > 8) || (offset != 0))
			goto err;
		break;
	case (LENOF_32):
		if ((length > 32) || (offset > 32) || ((offset + length) > 32))
			goto err;
		break;
	case (LENOF_24):
		if ((length > 24) || (offset > 24) || ((offset + length) > 24))
			goto err;
		break;
	case (LENOF_16):
		if ((length > 16) || (offset > 16) || ((offset + length) > 16))
			goto err;
		break;
	case (LENOF_8):
		if ((length > 8) || (offset > 8) || ((offset + length) > 8))
			goto err;
		break;
	case (LENOF_128):
		if ((length > 128) || (offset > 128) ||
		    ((offset + length) > 128))
			goto err;
		break;
	case (LENOF_256):
		if ((length < 1) || (length > 256) || ((length + offset) > 256))
			goto err;
		break;
	case (DSNM):
		break;
	default:
		goto err;
	}

	return 0;
err:
	return -EINVAL;
}

static inline int
rta_load(struct program *program, uint64_t src, uint64_t dst,
	 uint32_t offset, uint32_t length, uint32_t flags)
{
	uint32_t opcode = 0;
	int pos = -1, ret = -EINVAL;
	unsigned int start_pc = program->current_pc, i;

	if (flags & SEQ)
		opcode = CMD_SEQ_LOAD;
	else
		opcode = CMD_LOAD;

	if ((length & 0xffffff00) || (offset & 0xffffff00)) {
		pr_err("LOAD: Bad length/offset passed. Should be 8 bits\n");
		goto err;
	}

	if (flags & SGF)
		opcode |= LDST_SGF;
	if (flags & VLF)
		opcode |= LDST_VLF;

	/* check load destination, length and offset and source type */
	for (i = 0; i < load_dst_sz[rta_sec_era]; i++)
		if (dst == load_dst[i].dst) {
			pos = (int)i;
			break;
		}
	if (-1 == pos) {
		pr_err("LOAD: Invalid dst. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	if (flags & IMMED) {
		if (load_dst[pos].imm_src == IMM_NO) {
			pr_err("LOAD: Invalid source type. SEC Program Line: %d\n",
			       program->current_pc);
			goto err;
		}
		opcode |= LDST_IMM;
	} else if (load_dst[pos].imm_src == IMM_MUST) {
		pr_err("LOAD IMM: Invalid source type. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	ret = load_check_len_offset(pos, length, offset);
	if (ret < 0) {
		pr_err("LOAD: Invalid length/offset. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	opcode |= load_dst[pos].dst_opcode;

	/* DESC BUFFER: length / offset values are specified in 4-byte words */
	if (dst == DESCBUF) {
		opcode |= (length >> 2);
		opcode |= ((offset >> 2) << LDST_OFFSET_SHIFT);
	} else {
		opcode |= length;
		opcode |= (offset << LDST_OFFSET_SHIFT);
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* DECO CONTROL: skip writing pointer of imm data */
	if (dst == DCTRL)
		return (int)start_pc;

	/*
	 * For data copy, 3 possible ways to specify how to copy data:
	 *  - IMMED & !COPY: copy data directly from src( max 8 bytes)
	 *  - IMMED & COPY: copy data imm from the location specified by user
	 *  - !IMMED and is not SEQ cmd: copy the address
	 */
	if (flags & IMMED)
		__rta_inline_data(program, src, flags & __COPY_MASK, length);
	else if (!(flags & SEQ))
		__rta_out64(program, program->ps, src);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_LOAD_CMD_H__*/
