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

#ifndef __RTA_MATH_CMD_H__
#define __RTA_MATH_CMD_H__

extern enum rta_sec_era rta_sec_era;

static const uint32_t math_op1[][2] = {
/*1*/	{ MATH0,     MATH_SRC0_REG0 },
	{ MATH1,     MATH_SRC0_REG1 },
	{ MATH2,     MATH_SRC0_REG2 },
	{ MATH3,     MATH_SRC0_REG3 },
	{ SEQINSZ,   MATH_SRC0_SEQINLEN },
	{ SEQOUTSZ,  MATH_SRC0_SEQOUTLEN },
	{ VSEQINSZ,  MATH_SRC0_VARSEQINLEN },
	{ VSEQOUTSZ, MATH_SRC0_VARSEQOUTLEN },
	{ ZERO,      MATH_SRC0_ZERO },
/*10*/	{ NONE,      0 }, /* dummy value */
	{ DPOVRD,    MATH_SRC0_DPOVRD },
	{ ONE,       MATH_SRC0_ONE }
};

/*
 * Allowed MATH op1 sources for each SEC Era.
 * Values represent the number of entries from math_op1[] that are supported.
 */
static const unsigned int math_op1_sz[] = {10, 10, 12, 12, 12, 12, 12, 12};

static const uint32_t math_op2[][2] = {
/*1*/	{ MATH0,     MATH_SRC1_REG0 },
	{ MATH1,     MATH_SRC1_REG1 },
	{ MATH2,     MATH_SRC1_REG2 },
	{ MATH3,     MATH_SRC1_REG3 },
	{ ABD,       MATH_SRC1_INFIFO },
	{ OFIFO,     MATH_SRC1_OUTFIFO },
	{ ONE,       MATH_SRC1_ONE },
/*8*/	{ NONE,      0 }, /* dummy value */
	{ JOBSRC,    MATH_SRC1_JOBSOURCE },
	{ DPOVRD,    MATH_SRC1_DPOVRD },
	{ VSEQINSZ,  MATH_SRC1_VARSEQINLEN },
	{ VSEQOUTSZ, MATH_SRC1_VARSEQOUTLEN },
/*13*/	{ ZERO,      MATH_SRC1_ZERO }
};

/*
 * Allowed MATH op2 sources for each SEC Era.
 * Values represent the number of entries from math_op2[] that are supported.
 */
static const unsigned int math_op2_sz[] = {8, 9, 13, 13, 13, 13, 13, 13};

static const uint32_t math_result[][2] = {
/*1*/	{ MATH0,     MATH_DEST_REG0 },
	{ MATH1,     MATH_DEST_REG1 },
	{ MATH2,     MATH_DEST_REG2 },
	{ MATH3,     MATH_DEST_REG3 },
	{ SEQINSZ,   MATH_DEST_SEQINLEN },
	{ SEQOUTSZ,  MATH_DEST_SEQOUTLEN },
	{ VSEQINSZ,  MATH_DEST_VARSEQINLEN },
	{ VSEQOUTSZ, MATH_DEST_VARSEQOUTLEN },
/*9*/	{ NONE,      MATH_DEST_NONE },
	{ DPOVRD,    MATH_DEST_DPOVRD }
};

/*
 * Allowed MATH result destinations for each SEC Era.
 * Values represent the number of entries from math_result[] that are
 * supported.
 */
static const unsigned int math_result_sz[] = {9, 9, 10, 10, 10, 10, 10, 10};

static inline int
rta_math(struct program *program, uint64_t operand1,
	 uint32_t op, uint64_t operand2, uint32_t result,
	 int length, uint32_t options)
{
	uint32_t opcode = CMD_MATH;
	uint32_t val = 0;
	int ret = -EINVAL;
	unsigned int start_pc = program->current_pc;

	if (((op == MATH_FUN_BSWAP) && (rta_sec_era < RTA_SEC_ERA_4)) ||
	    ((op == MATH_FUN_ZBYT) && (rta_sec_era < RTA_SEC_ERA_2))) {
		pr_err("MATH: operation not supported by SEC Era %d. SEC PC: %d; Instr: %d\n",
		       USER_SEC_ERA(rta_sec_era), program->current_pc,
		       program->current_instruction);
		goto err;
	}

	if (options & SWP) {
		if (rta_sec_era < RTA_SEC_ERA_7) {
			pr_err("MATH: operation not supported by SEC Era %d. SEC PC: %d; Instr: %d\n",
			       USER_SEC_ERA(rta_sec_era), program->current_pc,
			       program->current_instruction);
			goto err;
		}

		if ((options & IFB) ||
		    (!(options & IMMED) && !(options & IMMED2)) ||
		    ((options & IMMED) && (options & IMMED2))) {
			pr_err("MATH: SWP - invalid configuration. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
	}

	/*
	 * SHLD operation is different from others and we
	 * assume that we can have _NONE as first operand
	 * or _SEQINSZ as second operand
	 */
	if ((op != MATH_FUN_SHLD) && ((operand1 == NONE) ||
				      (operand2 == SEQINSZ))) {
		pr_err("MATH: Invalid operand. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	/*
	 * We first check if it is unary operation. In that
	 * case second operand must be _NONE
	 */
	if (((op == MATH_FUN_ZBYT) || (op == MATH_FUN_BSWAP)) &&
	    (operand2 != NONE)) {
		pr_err("MATH: Invalid operand2. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	/* Write first operand field */
	if (options & IMMED) {
		opcode |= MATH_SRC0_IMM;
	} else {
		ret = __rta_map_opcode((uint32_t)operand1, math_op1,
				       math_op1_sz[rta_sec_era], &val);
		if (ret < 0) {
			pr_err("MATH: operand1 not supported. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		opcode |= val;
	}

	/* Write second operand field */
	if (options & IMMED2) {
		opcode |= MATH_SRC1_IMM;
	} else {
		ret = __rta_map_opcode((uint32_t)operand2, math_op2,
				       math_op2_sz[rta_sec_era], &val);
		if (ret < 0) {
			pr_err("MATH: operand2 not supported. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		opcode |= val;
	}

	/* Write result field */
	ret = __rta_map_opcode(result, math_result, math_result_sz[rta_sec_era],
			       &val);
	if (ret < 0) {
		pr_err("MATH: result not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= val;

	/*
	 * as we encode operations with their "real" values, we do not
	 * to translate but we do need to validate the value
	 */
	switch (op) {
	/*Binary operators */
	case (MATH_FUN_ADD):
	case (MATH_FUN_ADDC):
	case (MATH_FUN_SUB):
	case (MATH_FUN_SUBB):
	case (MATH_FUN_OR):
	case (MATH_FUN_AND):
	case (MATH_FUN_XOR):
	case (MATH_FUN_LSHIFT):
	case (MATH_FUN_RSHIFT):
	case (MATH_FUN_SHLD):
	/* Unary operators */
	case (MATH_FUN_ZBYT):
	case (MATH_FUN_BSWAP):
		opcode |= op;
		break;
	default:
		pr_err("MATH: operator is not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		ret = -EINVAL;
		goto err;
	}

	opcode |= (options & ~(IMMED | IMMED2));

	/* Verify length */
	switch (length) {
	case (1):
		opcode |= MATH_LEN_1BYTE;
		break;
	case (2):
		opcode |= MATH_LEN_2BYTE;
		break;
	case (4):
		opcode |= MATH_LEN_4BYTE;
		break;
	case (8):
		opcode |= MATH_LEN_8BYTE;
		break;
	default:
		pr_err("MATH: length is not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		ret = -EINVAL;
		goto err;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* Write immediate value */
	if ((options & IMMED) && !(options & IMMED2)) {
		__rta_out64(program, (length > 4) && !(options & IFB),
			    operand1);
	} else if ((options & IMMED2) && !(options & IMMED)) {
		__rta_out64(program, (length > 4) && !(options & IFB),
			    operand2);
	} else if ((options & IMMED) && (options & IMMED2)) {
		__rta_out32(program, lower_32_bits(operand1));
		__rta_out32(program, lower_32_bits(operand2));
	}

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

static inline int
rta_mathi(struct program *program, uint64_t operand,
	  uint32_t op, uint8_t imm, uint32_t result,
	  int length, uint32_t options)
{
	uint32_t opcode = CMD_MATHI;
	uint32_t val = 0;
	int ret = -EINVAL;
	unsigned int start_pc = program->current_pc;

	if (rta_sec_era < RTA_SEC_ERA_6) {
		pr_err("MATHI: Command not supported by SEC Era %d. SEC PC: %d; Instr: %d\n",
		       USER_SEC_ERA(rta_sec_era), program->current_pc,
		       program->current_instruction);
		goto err;
	}

	if (((op == MATH_FUN_FBYT) && (options & SSEL))) {
		pr_err("MATHI: Illegal combination - FBYT and SSEL. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	if ((options & SWP) && (rta_sec_era < RTA_SEC_ERA_7)) {
		pr_err("MATHI: SWP not supported by SEC Era %d. SEC PC: %d; Instr: %d\n",
		       USER_SEC_ERA(rta_sec_era), program->current_pc,
		       program->current_instruction);
		goto err;
	}

	/* Write first operand field */
	if (!(options & SSEL))
		ret = __rta_map_opcode((uint32_t)operand, math_op1,
				       math_op1_sz[rta_sec_era], &val);
	else
		ret = __rta_map_opcode((uint32_t)operand, math_op2,
				       math_op2_sz[rta_sec_era], &val);
	if (ret < 0) {
		pr_err("MATHI: operand not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	if (!(options & SSEL))
		opcode |= val;
	else
		opcode |= (val << (MATHI_SRC1_SHIFT - MATH_SRC1_SHIFT));

	/* Write second operand field */
	opcode |= (imm << MATHI_IMM_SHIFT);

	/* Write result field */
	ret = __rta_map_opcode(result, math_result, math_result_sz[rta_sec_era],
			       &val);
	if (ret < 0) {
		pr_err("MATHI: result not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}
	opcode |= (val << (MATHI_DEST_SHIFT - MATH_DEST_SHIFT));

	/*
	 * as we encode operations with their "real" values, we do not have to
	 * translate but we do need to validate the value
	 */
	switch (op) {
	case (MATH_FUN_ADD):
	case (MATH_FUN_ADDC):
	case (MATH_FUN_SUB):
	case (MATH_FUN_SUBB):
	case (MATH_FUN_OR):
	case (MATH_FUN_AND):
	case (MATH_FUN_XOR):
	case (MATH_FUN_LSHIFT):
	case (MATH_FUN_RSHIFT):
	case (MATH_FUN_FBYT):
		opcode |= op;
		break;
	default:
		pr_err("MATHI: operator not supported. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		ret = -EINVAL;
		goto err;
	}

	opcode |= options;

	/* Verify length */
	switch (length) {
	case (1):
		opcode |= MATH_LEN_1BYTE;
		break;
	case (2):
		opcode |= MATH_LEN_2BYTE;
		break;
	case (4):
		opcode |= MATH_LEN_4BYTE;
		break;
	case (8):
		opcode |= MATH_LEN_8BYTE;
		break;
	default:
		pr_err("MATHI: length %d not supported. SEC PC: %d; Instr: %d\n",
		       length, program->current_pc,
		       program->current_instruction);
		ret = -EINVAL;
		goto err;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_MATH_CMD_H__ */
