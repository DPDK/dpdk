/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016,2019 NXP
 */

#ifndef __RTA_NFIFO_CMD_H__
#define __RTA_NFIFO_CMD_H__

extern enum rta_sec_era rta_sec_era;

static const uint32_t nfifo_src[][2] = {
/*1*/	{ IFIFO,       NFIFOENTRY_STYPE_DFIFO },
	{ OFIFO,       NFIFOENTRY_STYPE_OFIFO },
	{ PAD,         NFIFOENTRY_STYPE_PAD },
/*4*/	{ MSGOUTSNOOP, NFIFOENTRY_STYPE_SNOOP | NFIFOENTRY_DEST_BOTH },
/*5*/	{ ALTSOURCE,   NFIFOENTRY_STYPE_ALTSOURCE },
	{ OFIFO_SYNC,  NFIFOENTRY_STYPE_OFIFO_SYNC },
/*7*/	{ MSGOUTSNOOP_ALT, NFIFOENTRY_STYPE_SNOOP_ALT | NFIFOENTRY_DEST_BOTH }
};

/*
 * Allowed NFIFO LOAD sources for each SEC Era.
 * Values represent the number of entries from nfifo_src[] that are supported.
 */
static const unsigned int nfifo_src_sz[] = {4, 5, 5, 5, 5, 5, 5, 7, 7, 7};

static const uint32_t nfifo_data[][2] = {
	{ MSG,   NFIFOENTRY_DTYPE_MSG },
	{ MSG1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_MSG },
	{ MSG2,  NFIFOENTRY_DEST_CLASS2 | NFIFOENTRY_DTYPE_MSG },
	{ IV1,   NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_IV },
	{ IV2,   NFIFOENTRY_DEST_CLASS2 | NFIFOENTRY_DTYPE_IV },
	{ ICV1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_ICV },
	{ ICV2,  NFIFOENTRY_DEST_CLASS2 | NFIFOENTRY_DTYPE_ICV },
	{ SAD1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_SAD },
	{ AAD1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_AAD },
	{ AAD2,  NFIFOENTRY_DEST_CLASS2 | NFIFOENTRY_DTYPE_AAD },
	{ AFHA_SBOX, NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_SBOX },
	{ SKIP,  NFIFOENTRY_DTYPE_SKIP },
	{ PKE,   NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_E },
	{ PKN,   NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_N },
	{ PKA,   NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_A },
	{ PKA0,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_A0 },
	{ PKA1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_A1 },
	{ PKA2,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_A2 },
	{ PKA3,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_A3 },
	{ PKB,   NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_B },
	{ PKB0,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_B0 },
	{ PKB1,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_B1 },
	{ PKB2,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_B2 },
	{ PKB3,  NFIFOENTRY_DEST_CLASS1 | NFIFOENTRY_DTYPE_PK_B3 },
	{ AB1,   NFIFOENTRY_DEST_CLASS1 },
	{ AB2,   NFIFOENTRY_DEST_CLASS2 },
	{ ABD,   NFIFOENTRY_DEST_DECO }
};

static const uint32_t nfifo_flags[][2] = {
/*1*/	{ LAST1,         NFIFOENTRY_LC1 },
	{ LAST2,         NFIFOENTRY_LC2 },
	{ FLUSH1,        NFIFOENTRY_FC1 },
	{ BP,            NFIFOENTRY_BND },
	{ PAD_ZERO,      NFIFOENTRY_PTYPE_ZEROS },
	{ PAD_NONZERO,   NFIFOENTRY_PTYPE_RND_NOZEROS },
	{ PAD_INCREMENT, NFIFOENTRY_PTYPE_INCREMENT },
	{ PAD_RANDOM,    NFIFOENTRY_PTYPE_RND },
	{ PAD_ZERO_N1,   NFIFOENTRY_PTYPE_ZEROS_NZ },
	{ PAD_NONZERO_0, NFIFOENTRY_PTYPE_RND_NZ_LZ },
	{ PAD_N1,        NFIFOENTRY_PTYPE_N },
/*12*/	{ PAD_NONZERO_N, NFIFOENTRY_PTYPE_RND_NZ_N },
	{ FLUSH2,        NFIFOENTRY_FC2 },
	{ OC,            NFIFOENTRY_OC }
};

/*
 * Allowed NFIFO LOAD flags for each SEC Era.
 * Values represent the number of entries from nfifo_flags[] that are supported.
 */
static const unsigned int nfifo_flags_sz[] = {12, 14, 14, 14, 14, 14,
					      14, 14, 14, 14};

static const uint32_t nfifo_pad_flags[][2] = {
	{ BM, NFIFOENTRY_BM },
	{ PS, NFIFOENTRY_PS },
	{ PR, NFIFOENTRY_PR }
};

/*
 * Allowed NFIFO LOAD pad flags for each SEC Era.
 * Values represent the number of entries from nfifo_pad_flags[] that are
 * supported.
 */
static const unsigned int nfifo_pad_flags_sz[] = {2, 2, 2, 2, 3, 3, 3, 3, 3, 3};

static inline int
rta_nfifo_load(struct program *program, uint32_t src,
	       uint32_t data, uint32_t length, uint32_t flags)
{
	uint32_t opcode = 0, val;
	int ret = -EINVAL;
	uint32_t load_cmd = CMD_LOAD | LDST_IMM | LDST_CLASS_IND_CCB |
			    LDST_SRCDST_WORD_INFO_FIFO;
	unsigned int start_pc = program->current_pc;

	if ((data == AFHA_SBOX) && (rta_sec_era == RTA_SEC_ERA_7)) {
		pr_err("NFIFO: AFHA S-box not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}

	/* write source field */
	ret = __rta_map_opcode(src, nfifo_src, nfifo_src_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("NFIFO: Invalid SRC. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= val;

	/* write type field */
	ret = __rta_map_opcode(data, nfifo_data, ARRAY_SIZE(nfifo_data), &val);
	if (ret < 0) {
		pr_err("NFIFO: Invalid data. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= val;

	/* write DL field */
	if (!(flags & EXT)) {
		opcode |= length & NFIFOENTRY_DLEN_MASK;
		load_cmd |= 4;
	} else {
		load_cmd |= 8;
	}

	/* write flags */
	__rta_map_flags(flags, nfifo_flags, nfifo_flags_sz[rta_sec_era],
			&opcode);

	/* in case of padding, check the destination */
	if (src == PAD)
		__rta_map_flags(flags, nfifo_pad_flags,
				nfifo_pad_flags_sz[rta_sec_era], &opcode);

	/* write LOAD command first */
	__rta_out32(program, load_cmd);
	__rta_out32(program, opcode);

	if (flags & EXT)
		__rta_out32(program, length & NFIFOENTRY_DLEN_MASK);

	program->current_instruction++;

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_NFIFO_CMD_H__ */
