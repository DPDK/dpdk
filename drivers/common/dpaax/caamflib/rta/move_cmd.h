/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016,2019 NXP
 */

#ifndef __RTA_MOVE_CMD_H__
#define __RTA_MOVE_CMD_H__

#define MOVE_SET_AUX_SRC	0x01
#define MOVE_SET_AUX_DST	0x02
#define MOVE_SET_AUX_LS		0x03
#define MOVE_SET_LEN_16b	0x04

#define MOVE_SET_AUX_MATH	0x10
#define MOVE_SET_AUX_MATH_SRC	(MOVE_SET_AUX_SRC | MOVE_SET_AUX_MATH)
#define MOVE_SET_AUX_MATH_DST	(MOVE_SET_AUX_DST | MOVE_SET_AUX_MATH)

#define MASK_16b  0xFF

/* MOVE command type */
#define __MOVE		1
#define __MOVEB		2
#define __MOVEDW	3

extern enum rta_sec_era rta_sec_era;

static const uint32_t move_src_table[][2] = {
/*1*/	{ CONTEXT1, MOVE_SRC_CLASS1CTX },
	{ CONTEXT2, MOVE_SRC_CLASS2CTX },
	{ OFIFO,    MOVE_SRC_OUTFIFO },
	{ DESCBUF,  MOVE_SRC_DESCBUF },
	{ MATH0,    MOVE_SRC_MATH0 },
	{ MATH1,    MOVE_SRC_MATH1 },
	{ MATH2,    MOVE_SRC_MATH2 },
	{ MATH3,    MOVE_SRC_MATH3 },
/*9*/	{ IFIFOABD, MOVE_SRC_INFIFO },
	{ IFIFOAB1, MOVE_SRC_INFIFO_CL | MOVE_AUX_LS },
	{ IFIFOAB2, MOVE_SRC_INFIFO_CL },
/*12*/	{ ABD,      MOVE_SRC_INFIFO_NO_NFIFO },
	{ AB1,      MOVE_SRC_INFIFO_NO_NFIFO | MOVE_AUX_LS },
	{ AB2,      MOVE_SRC_INFIFO_NO_NFIFO | MOVE_AUX_MS }
};

/* Allowed MOVE / MOVE_LEN sources for each SEC Era.
 * Values represent the number of entries from move_src_table[] that are
 * supported.
 */
static const unsigned int move_src_table_sz[] = {9, 11, 14, 14, 14, 14, 14, 14,
						 14, 14};

static const uint32_t move_dst_table[][2] = {
/*1*/	{ CONTEXT1,  MOVE_DEST_CLASS1CTX },
	{ CONTEXT2,  MOVE_DEST_CLASS2CTX },
	{ OFIFO,     MOVE_DEST_OUTFIFO },
	{ DESCBUF,   MOVE_DEST_DESCBUF },
	{ MATH0,     MOVE_DEST_MATH0 },
	{ MATH1,     MOVE_DEST_MATH1 },
	{ MATH2,     MOVE_DEST_MATH2 },
	{ MATH3,     MOVE_DEST_MATH3 },
	{ IFIFOAB1,  MOVE_DEST_CLASS1INFIFO },
	{ IFIFOAB2,  MOVE_DEST_CLASS2INFIFO },
	{ PKA,       MOVE_DEST_PK_A },
	{ KEY1,      MOVE_DEST_CLASS1KEY },
	{ KEY2,      MOVE_DEST_CLASS2KEY },
/*14*/	{ IFIFO,     MOVE_DEST_INFIFO },
/*15*/	{ ALTSOURCE,  MOVE_DEST_ALTSOURCE}
};

/* Allowed MOVE / MOVE_LEN destinations for each SEC Era.
 * Values represent the number of entries from move_dst_table[] that are
 * supported.
 */
static const
unsigned int move_dst_table_sz[] = {13, 14, 14, 15, 15, 15, 15, 15, 15, 15};

static inline int
set_move_offset(struct program *program __maybe_unused,
		uint64_t src, uint16_t src_offset,
		uint64_t dst, uint16_t dst_offset,
		uint16_t *offset, uint16_t *opt);

static inline int
math_offset(uint16_t offset);

static inline int
rta_move(struct program *program, int cmd_type, uint64_t src,
	 uint16_t src_offset, uint64_t dst,
	 uint16_t dst_offset, uint32_t length, uint32_t flags)
{
	uint32_t opcode = 0;
	uint16_t offset = 0, opt = 0;
	uint32_t val = 0;
	int ret = -EINVAL;
	bool is_move_len_cmd = false;
	unsigned int start_pc = program->current_pc;

	/* write command type */
	if (cmd_type == __MOVEB) {
		opcode = CMD_MOVEB;
	} else if (cmd_type == __MOVEDW) {
		opcode = CMD_MOVEDW;
	} else if (!(flags & IMMED)) {
		if ((length != MATH0) && (length != MATH1) &&
		    (length != MATH2) && (length != MATH3)) {
			pr_err("MOVE: MOVE_LEN length must be MATH[0-3]. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}

		opcode = CMD_MOVE_LEN;
		is_move_len_cmd = true;
	} else {
		opcode = CMD_MOVE;
	}

	/* write offset first, to check for invalid combinations or incorrect
	 * offset values sooner; decide which offset should be here
	 * (src or dst)
	 */
	ret = set_move_offset(program, src, src_offset, dst, dst_offset,
			      &offset, &opt);
	if (ret < 0)
		goto err;

	opcode |= (offset << MOVE_OFFSET_SHIFT) & MOVE_OFFSET_MASK;

	/* set AUX field if required */
	if (opt == MOVE_SET_AUX_SRC) {
		opcode |= ((src_offset / 16) << MOVE_AUX_SHIFT) & MOVE_AUX_MASK;
	} else if (opt == MOVE_SET_AUX_DST) {
		opcode |= ((dst_offset / 16) << MOVE_AUX_SHIFT) & MOVE_AUX_MASK;
	} else if (opt == MOVE_SET_AUX_LS) {
		opcode |= MOVE_AUX_LS;
	} else if (opt & MOVE_SET_AUX_MATH) {
		if (opt & MOVE_SET_AUX_SRC)
			offset = src_offset;
		else
			offset = dst_offset;

		ret = math_offset(offset);
		if (ret < 0) {
			pr_err("MOVE: Invalid offset in MATH register. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}

		opcode |= (uint32_t)ret;
	}

	/* write source field */
	ret = __rta_map_opcode((uint32_t)src, move_src_table,
			       move_src_table_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("MOVE: Invalid SRC. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= val;

	/* write destination field */
	ret = __rta_map_opcode((uint32_t)dst, move_dst_table,
			       move_dst_table_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("MOVE: Invalid DST. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= val;

	/* write flags */
	if (flags & (FLUSH1 | FLUSH2))
		opcode |= MOVE_AUX_MS;
	if (flags & (LAST2 | LAST1))
		opcode |= MOVE_AUX_LS;
	if (flags & WAITCOMP)
		opcode |= MOVE_WAITCOMP;

	if (!is_move_len_cmd) {
		/* write length */
		if (opt == MOVE_SET_LEN_16b)
			opcode |= (length & (MOVE_OFFSET_MASK | MOVE_LEN_MASK));
		else
			opcode |= (length & MOVE_LEN_MASK);
	} else {
		/* write mrsel */
		switch (length) {
		case (MATH0):
			/*
			 * opcode |= MOVELEN_MRSEL_MATH0;
			 * MOVELEN_MRSEL_MATH0 is 0
			 */
			break;
		case (MATH1):
			opcode |= MOVELEN_MRSEL_MATH1;
			break;
		case (MATH2):
			opcode |= MOVELEN_MRSEL_MATH2;
			break;
		case (MATH3):
			opcode |= MOVELEN_MRSEL_MATH3;
			break;
		}

		/* write size */
		if (rta_sec_era >= RTA_SEC_ERA_7) {
			if (flags & SIZE_WORD)
				opcode |= MOVELEN_SIZE_WORD;
			else if (flags & SIZE_BYTE)
				opcode |= MOVELEN_SIZE_BYTE;
			else if (flags & SIZE_DWORD)
				opcode |= MOVELEN_SIZE_DWORD;
		}
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

static inline int
set_move_offset(struct program *program __maybe_unused,
		uint64_t src, uint16_t src_offset,
		uint64_t dst, uint16_t dst_offset,
		uint16_t *offset, uint16_t *opt)
{
	switch (src) {
	case (CONTEXT1):
	case (CONTEXT2):
		if (dst == DESCBUF) {
			*opt = MOVE_SET_AUX_SRC;
			*offset = dst_offset;
		} else if ((dst == KEY1) || (dst == KEY2)) {
			if ((src_offset) && (dst_offset)) {
				pr_err("MOVE: Bad offset. SEC PC: %d; Instr: %d\n",
				       program->current_pc,
				       program->current_instruction);
				goto err;
			}
			if (dst_offset) {
				*opt = MOVE_SET_AUX_LS;
				*offset = dst_offset;
			} else {
				*offset = src_offset;
			}
		} else {
			if ((dst == MATH0) || (dst == MATH1) ||
			    (dst == MATH2) || (dst == MATH3)) {
				*opt = MOVE_SET_AUX_MATH_DST;
			} else if (((dst == OFIFO) || (dst == ALTSOURCE)) &&
			    (src_offset % 4)) {
				pr_err("MOVE: Bad offset alignment. SEC PC: %d; Instr: %d\n",
				       program->current_pc,
				       program->current_instruction);
				goto err;
			}

			*offset = src_offset;
		}
		break;

	case (OFIFO):
		if (dst == OFIFO) {
			pr_err("MOVE: Invalid DST. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		if (((dst == IFIFOAB1) || (dst == IFIFOAB2) ||
		     (dst == IFIFO) || (dst == PKA)) &&
		    (src_offset || dst_offset)) {
			pr_err("MOVE: Offset should be zero. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		*offset = dst_offset;
		break;

	case (DESCBUF):
		if ((dst == CONTEXT1) || (dst == CONTEXT2)) {
			*opt = MOVE_SET_AUX_DST;
		} else if ((dst == MATH0) || (dst == MATH1) ||
			   (dst == MATH2) || (dst == MATH3)) {
			*opt = MOVE_SET_AUX_MATH_DST;
		} else if (dst == DESCBUF) {
			pr_err("MOVE: Invalid DST. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		} else if (((dst == OFIFO) || (dst == ALTSOURCE)) &&
		    (src_offset % 4)) {
			pr_err("MOVE: Invalid offset alignment. SEC PC: %d; Instr %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}

		*offset = src_offset;
		break;

	case (MATH0):
	case (MATH1):
	case (MATH2):
	case (MATH3):
		if ((dst == OFIFO) || (dst == ALTSOURCE)) {
			if (src_offset % 4) {
				pr_err("MOVE: Bad offset alignment. SEC PC: %d; Instr: %d\n",
				       program->current_pc,
				       program->current_instruction);
				goto err;
			}
			*offset = src_offset;
		} else if ((dst == IFIFOAB1) || (dst == IFIFOAB2) ||
			   (dst == IFIFO) || (dst == PKA)) {
			*offset = src_offset;
		} else {
			*offset = dst_offset;

			/*
			 * This condition is basically the negation of:
			 * dst in { CONTEXT[1-2], MATH[0-3] }
			 */
			if ((dst != KEY1) && (dst != KEY2))
				*opt = MOVE_SET_AUX_MATH_SRC;
		}
		break;

	case (IFIFOABD):
	case (IFIFOAB1):
	case (IFIFOAB2):
	case (ABD):
	case (AB1):
	case (AB2):
		if ((dst == IFIFOAB1) || (dst == IFIFOAB2) ||
		    (dst == IFIFO) || (dst == PKA) || (dst == ALTSOURCE)) {
			pr_err("MOVE: Bad DST. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		} else {
			if (dst == OFIFO) {
				*opt = MOVE_SET_LEN_16b;
			} else {
				if (dst_offset % 4) {
					pr_err("MOVE: Bad offset alignment. SEC PC: %d; Instr: %d\n",
					       program->current_pc,
					       program->current_instruction);
					goto err;
				}
				*offset = dst_offset;
			}
		}
		break;
	default:
		break;
	}

	return 0;
 err:
	return -EINVAL;
}

static inline int
math_offset(uint16_t offset)
{
	switch (offset) {
	case 0:
		return 0;
	case 4:
		return MOVE_AUX_LS;
	case 6:
		return MOVE_AUX_MS;
	case 7:
		return MOVE_AUX_LS | MOVE_AUX_MS;
	}

	return -EINVAL;
}

#endif /* __RTA_MOVE_CMD_H__ */
