/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */

#ifndef __RTA_FIFO_LOAD_STORE_CMD_H__
#define __RTA_FIFO_LOAD_STORE_CMD_H__

extern enum rta_sec_era rta_sec_era;

static const uint32_t fifo_load_table[][2] = {
/*1*/	{ PKA0,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_A0 },
	{ PKA1,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_A1 },
	{ PKA2,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_A2 },
	{ PKA3,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_A3 },
	{ PKB0,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_B0 },
	{ PKB1,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_B1 },
	{ PKB2,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_B2 },
	{ PKB3,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_B3 },
	{ PKA,         FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_A },
	{ PKB,         FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_B },
	{ PKN,         FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_PK_N },
	{ SKIP,        FIFOLD_CLASS_SKIP },
	{ MSG1,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_MSG },
	{ MSG2,        FIFOLD_CLASS_CLASS2 | FIFOLD_TYPE_MSG },
	{ MSGOUTSNOOP, FIFOLD_CLASS_BOTH | FIFOLD_TYPE_MSG1OUT2 },
	{ MSGINSNOOP,  FIFOLD_CLASS_BOTH | FIFOLD_TYPE_MSG },
	{ IV1,         FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_IV },
	{ IV2,         FIFOLD_CLASS_CLASS2 | FIFOLD_TYPE_IV },
	{ AAD1,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_AAD },
	{ ICV1,        FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_ICV },
	{ ICV2,        FIFOLD_CLASS_CLASS2 | FIFOLD_TYPE_ICV },
	{ BIT_DATA,    FIFOLD_TYPE_BITDATA },
/*23*/	{ IFIFO,       FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_NOINFOFIFO }
};

/*
 * Allowed FIFO_LOAD input data types for each SEC Era.
 * Values represent the number of entries from fifo_load_table[] that are
 * supported.
 */
static const unsigned int fifo_load_table_sz[] = {22, 22, 23, 23,
						  23, 23, 23, 23};

static inline int
rta_fifo_load(struct program *program, uint32_t src,
	      uint64_t loc, uint32_t length, uint32_t flags)
{
	uint32_t opcode = 0;
	uint32_t ext_length = 0, val = 0;
	int ret = -EINVAL;
	bool is_seq_cmd = false;
	unsigned int start_pc = program->current_pc;

	/* write command type field */
	if (flags & SEQ) {
		opcode = CMD_SEQ_FIFO_LOAD;
		is_seq_cmd = true;
	} else {
		opcode = CMD_FIFO_LOAD;
	}

	/* Parameters checking */
	if (is_seq_cmd) {
		if ((flags & IMMED) || (flags & SGF)) {
			pr_err("SEQ FIFO LOAD: Invalid command\n");
			goto err;
		}
		if ((rta_sec_era <= RTA_SEC_ERA_5) && (flags & AIDF)) {
			pr_err("SEQ FIFO LOAD: Flag(s) not supported by SEC Era %d\n",
			       USER_SEC_ERA(rta_sec_era));
			goto err;
		}
		if ((flags & VLF) && ((flags & EXT) || (length >> 16))) {
			pr_err("SEQ FIFO LOAD: Invalid usage of VLF\n");
			goto err;
		}
	} else {
		if (src == SKIP) {
			pr_err("FIFO LOAD: Invalid src\n");
			goto err;
		}
		if ((flags & AIDF) || (flags & VLF)) {
			pr_err("FIFO LOAD: Invalid command\n");
			goto err;
		}
		if ((flags & IMMED) && (flags & SGF)) {
			pr_err("FIFO LOAD: Invalid usage of SGF and IMM\n");
			goto err;
		}
		if ((flags & IMMED) && ((flags & EXT) || (length >> 16))) {
			pr_err("FIFO LOAD: Invalid usage of EXT and IMM\n");
			goto err;
		}
	}

	/* write input data type field */
	ret = __rta_map_opcode(src, fifo_load_table,
			       fifo_load_table_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("FIFO LOAD: Source value is not supported. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}
	opcode |= val;

	if (flags & CLASS1)
		opcode |= FIFOLD_CLASS_CLASS1;
	if (flags & CLASS2)
		opcode |= FIFOLD_CLASS_CLASS2;
	if (flags & BOTH)
		opcode |= FIFOLD_CLASS_BOTH;

	/* write fields: SGF|VLF, IMM, [LC1, LC2, F1] */
	if (flags & FLUSH1)
		opcode |= FIFOLD_TYPE_FLUSH1;
	if (flags & LAST1)
		opcode |= FIFOLD_TYPE_LAST1;
	if (flags & LAST2)
		opcode |= FIFOLD_TYPE_LAST2;
	if (!is_seq_cmd) {
		if (flags & SGF)
			opcode |= FIFOLDST_SGF;
		if (flags & IMMED)
			opcode |= FIFOLD_IMM;
	} else {
		if (flags & VLF)
			opcode |= FIFOLDST_VLF;
		if (flags & AIDF)
			opcode |= FIFOLD_AIDF;
	}

	/*
	 * Verify if extended length is required. In case of BITDATA, calculate
	 * number of full bytes and additional valid bits.
	 */
	if ((flags & EXT) || (length >> 16)) {
		opcode |= FIFOLDST_EXT;
		if (src == BIT_DATA) {
			ext_length = (length / 8);
			length = (length % 8);
		} else {
			ext_length = length;
			length = 0;
		}
	}
	opcode |= (uint16_t) length;

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* write pointer or immediate data field */
	if (flags & IMMED)
		__rta_inline_data(program, loc, flags & __COPY_MASK, length);
	else if (!is_seq_cmd)
		__rta_out64(program, program->ps, loc);

	/* write extended length field */
	if (opcode & FIFOLDST_EXT)
		__rta_out32(program, ext_length);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

static const uint32_t fifo_store_table[][2] = {
/*1*/	{ PKA0,      FIFOST_TYPE_PKHA_A0 },
	{ PKA1,      FIFOST_TYPE_PKHA_A1 },
	{ PKA2,      FIFOST_TYPE_PKHA_A2 },
	{ PKA3,      FIFOST_TYPE_PKHA_A3 },
	{ PKB0,      FIFOST_TYPE_PKHA_B0 },
	{ PKB1,      FIFOST_TYPE_PKHA_B1 },
	{ PKB2,      FIFOST_TYPE_PKHA_B2 },
	{ PKB3,      FIFOST_TYPE_PKHA_B3 },
	{ PKA,       FIFOST_TYPE_PKHA_A },
	{ PKB,       FIFOST_TYPE_PKHA_B },
	{ PKN,       FIFOST_TYPE_PKHA_N },
	{ PKE,       FIFOST_TYPE_PKHA_E_JKEK },
	{ RNG,       FIFOST_TYPE_RNGSTORE },
	{ RNGOFIFO,  FIFOST_TYPE_RNGFIFO },
	{ AFHA_SBOX, FIFOST_TYPE_AF_SBOX_JKEK },
	{ MDHA_SPLIT_KEY, FIFOST_CLASS_CLASS2KEY | FIFOST_TYPE_SPLIT_KEK },
	{ MSG,       FIFOST_TYPE_MESSAGE_DATA },
	{ KEY1,      FIFOST_CLASS_CLASS1KEY | FIFOST_TYPE_KEY_KEK },
	{ KEY2,      FIFOST_CLASS_CLASS2KEY | FIFOST_TYPE_KEY_KEK },
	{ OFIFO,     FIFOST_TYPE_OUTFIFO_KEK},
	{ SKIP,      FIFOST_TYPE_SKIP },
/*22*/	{ METADATA,  FIFOST_TYPE_METADATA},
	{ MSG_CKSUM,  FIFOST_TYPE_MESSAGE_DATA2 }
};

/*
 * Allowed FIFO_STORE output data types for each SEC Era.
 * Values represent the number of entries from fifo_store_table[] that are
 * supported.
 */
static const unsigned int fifo_store_table_sz[] = {21, 21, 21, 21,
						   22, 22, 22, 23};

static inline int
rta_fifo_store(struct program *program, uint32_t src,
	       uint32_t encrypt_flags, uint64_t dst,
	       uint32_t length, uint32_t flags)
{
	uint32_t opcode = 0;
	uint32_t val = 0;
	int ret = -EINVAL;
	bool is_seq_cmd = false;
	unsigned int start_pc = program->current_pc;

	/* write command type field */
	if (flags & SEQ) {
		opcode = CMD_SEQ_FIFO_STORE;
		is_seq_cmd = true;
	} else {
		opcode = CMD_FIFO_STORE;
	}

	/* Parameter checking */
	if (is_seq_cmd) {
		if ((flags & VLF) && ((length >> 16) || (flags & EXT))) {
			pr_err("SEQ FIFO STORE: Invalid usage of VLF\n");
			goto err;
		}
		if (dst) {
			pr_err("SEQ FIFO STORE: Invalid command\n");
			goto err;
		}
		if ((src == METADATA) && (flags & (CONT | EXT))) {
			pr_err("SEQ FIFO STORE: Invalid flags\n");
			goto err;
		}
	} else {
		if (((src == RNGOFIFO) && ((dst) || (flags & EXT))) ||
		    (src == METADATA)) {
			pr_err("FIFO STORE: Invalid destination\n");
			goto err;
		}
	}
	if ((rta_sec_era == RTA_SEC_ERA_7) && (src == AFHA_SBOX)) {
		pr_err("FIFO STORE: AFHA S-box not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}

	/* write output data type field */
	ret = __rta_map_opcode(src, fifo_store_table,
			       fifo_store_table_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("FIFO STORE: Source type not supported. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}
	opcode |= val;

	if (encrypt_flags & TK)
		opcode |= (0x1 << FIFOST_TYPE_SHIFT);
	if (encrypt_flags & EKT) {
		if (rta_sec_era == RTA_SEC_ERA_1) {
			pr_err("FIFO STORE: AES-CCM source types not supported\n");
			ret = -EINVAL;
			goto err;
		}
		opcode |= (0x10 << FIFOST_TYPE_SHIFT);
		opcode &= (uint32_t)~(0x20 << FIFOST_TYPE_SHIFT);
	}

	/* write flags fields */
	if (flags & CONT)
		opcode |= FIFOST_CONT;
	if ((flags & VLF) && (is_seq_cmd))
		opcode |= FIFOLDST_VLF;
	if ((flags & SGF) && (!is_seq_cmd))
		opcode |= FIFOLDST_SGF;
	if (flags & CLASS1)
		opcode |= FIFOST_CLASS_CLASS1KEY;
	if (flags & CLASS2)
		opcode |= FIFOST_CLASS_CLASS2KEY;
	if (flags & BOTH)
		opcode |= FIFOST_CLASS_BOTH;

	/* Verify if extended length is required */
	if ((length >> 16) || (flags & EXT))
		opcode |= FIFOLDST_EXT;
	else
		opcode |= (uint16_t) length;

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* write pointer field */
	if ((!is_seq_cmd) && (dst))
		__rta_out64(program, program->ps, dst);

	/* write extended length field */
	if (opcode & FIFOLDST_EXT)
		__rta_out32(program, length);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_FIFO_LOAD_STORE_CMD_H__ */
