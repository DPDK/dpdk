/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016,2019 NXP
 */

#ifndef __RTA_STORE_CMD_H__
#define __RTA_STORE_CMD_H__

extern enum rta_sec_era rta_sec_era;

static const uint32_t store_src_table[][2] = {
/*1*/	{ KEY1SZ,       LDST_CLASS_1_CCB | LDST_SRCDST_WORD_KEYSZ_REG },
	{ KEY2SZ,       LDST_CLASS_2_CCB | LDST_SRCDST_WORD_KEYSZ_REG },
	{ DJQDA,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_JQDAR },
	{ MODE1,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_MODE_REG },
	{ MODE2,        LDST_CLASS_2_CCB | LDST_SRCDST_WORD_MODE_REG },
	{ DJQCTRL,      LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_JQCTRL },
	{ DATA1SZ,      LDST_CLASS_1_CCB | LDST_SRCDST_WORD_DATASZ_REG },
	{ DATA2SZ,      LDST_CLASS_2_CCB | LDST_SRCDST_WORD_DATASZ_REG },
	{ DSTAT,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_STAT },
	{ ICV1SZ,       LDST_CLASS_1_CCB | LDST_SRCDST_WORD_ICVSZ_REG },
	{ ICV2SZ,       LDST_CLASS_2_CCB | LDST_SRCDST_WORD_ICVSZ_REG },
	{ DPID,         LDST_CLASS_DECO | LDST_SRCDST_WORD_PID },
	{ CCTRL,        LDST_SRCDST_WORD_CHACTRL },
	{ ICTRL,        LDST_SRCDST_WORD_IRQCTRL },
	{ CLRW,         LDST_SRCDST_WORD_CLRW },
	{ MATH0,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH0 },
	{ CSTAT,        LDST_SRCDST_WORD_STAT },
	{ MATH1,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH1 },
	{ MATH2,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH2 },
	{ AAD1SZ,       LDST_CLASS_1_CCB | LDST_SRCDST_WORD_DECO_AAD_SZ },
	{ MATH3,        LDST_CLASS_DECO | LDST_SRCDST_WORD_DECO_MATH3 },
	{ IV1SZ,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_CLASS1_IV_SZ },
	{ PKASZ,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_A_SZ },
	{ PKBSZ,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_B_SZ },
	{ PKESZ,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_E_SZ },
	{ PKNSZ,        LDST_CLASS_1_CCB | LDST_SRCDST_WORD_PKHA_N_SZ },
	{ CONTEXT1,     LDST_CLASS_1_CCB | LDST_SRCDST_BYTE_CONTEXT },
	{ CONTEXT2,     LDST_CLASS_2_CCB | LDST_SRCDST_BYTE_CONTEXT },
	{ DESCBUF,      LDST_CLASS_DECO | LDST_SRCDST_WORD_DESCBUF },
/*30*/	{ JOBDESCBUF,   LDST_CLASS_DECO | LDST_SRCDST_WORD_DESCBUF_JOB },
	{ SHAREDESCBUF, LDST_CLASS_DECO | LDST_SRCDST_WORD_DESCBUF_SHARED },
/*32*/	{ JOBDESCBUF_EFF,   LDST_CLASS_DECO |
		LDST_SRCDST_WORD_DESCBUF_JOB_WE },
	{ SHAREDESCBUF_EFF, LDST_CLASS_DECO |
		LDST_SRCDST_WORD_DESCBUF_SHARED_WE },
/*34*/	{ GTR,          LDST_CLASS_DECO | LDST_SRCDST_WORD_GTR },
	{ STR,          LDST_CLASS_DECO | LDST_SRCDST_WORD_STR }
};

/*
 * Allowed STORE sources for each SEC ERA.
 * Values represent the number of entries from source_src_table[] that are
 * supported.
 */
static const unsigned int store_src_table_sz[] = {29, 31, 33, 33,
						  33, 33, 35, 35,
						  35, 35};

static inline int
rta_store(struct program *program, uint64_t src,
	  uint16_t offset, uint64_t dst, uint32_t length,
	  uint32_t flags)
{
	uint32_t opcode = 0, val;
	int ret = -EINVAL;
	unsigned int start_pc = program->current_pc;

	if (flags & SEQ)
		opcode = CMD_SEQ_STORE;
	else
		opcode = CMD_STORE;

	/* parameters check */
	if ((flags & IMMED) && (flags & SGF)) {
		pr_err("STORE: Invalid flag. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	if ((flags & IMMED) && (offset != 0)) {
		pr_err("STORE: Invalid flag. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	if ((flags & SEQ) && ((src == JOBDESCBUF) || (src == SHAREDESCBUF) ||
			      (src == JOBDESCBUF_EFF) ||
			      (src == SHAREDESCBUF_EFF))) {
		pr_err("STORE: Invalid SRC type. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	if (flags & IMMED)
		opcode |= LDST_IMM;

	if ((flags & SGF) || (flags & VLF))
		opcode |= LDST_VLF;

	/*
	 * source for data to be stored can be specified as:
	 *    - register location; set in src field[9-15];
	 *    - if IMMED flag is set, data is set in value field [0-31];
	 *      user can give this value as actual value or pointer to data
	 */
	if (!(flags & IMMED)) {
		ret = __rta_map_opcode((uint32_t)src, store_src_table,
				       store_src_table_sz[rta_sec_era], &val);
		if (ret < 0) {
			pr_err("STORE: Invalid source. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		opcode |= val;
	}

	/* DESC BUFFER: length / offset values are specified in 4-byte words */
	if ((src == DESCBUF) || (src == JOBDESCBUF) || (src == SHAREDESCBUF) ||
	    (src == JOBDESCBUF_EFF) || (src == SHAREDESCBUF_EFF)) {
		opcode |= (length >> 2);
		opcode |= (uint32_t)((offset >> 2) << LDST_OFFSET_SHIFT);
	} else {
		opcode |= length;
		opcode |= (uint32_t)(offset << LDST_OFFSET_SHIFT);
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	if ((src == JOBDESCBUF) || (src == SHAREDESCBUF) ||
	    (src == JOBDESCBUF_EFF) || (src == SHAREDESCBUF_EFF))
		return (int)start_pc;

	/* for STORE, a pointer to where the data will be stored if needed */
	if (!(flags & SEQ))
		__rta_out64(program, program->ps, dst);

	/* for IMMED data, place the data here */
	if (flags & IMMED)
		__rta_inline_data(program, src, flags & __COPY_MASK, length);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_STORE_CMD_H__ */
