/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */

#ifndef __RTA_SIGNATURE_CMD_H__
#define __RTA_SIGNATURE_CMD_H__

static inline int
rta_signature(struct program *program, uint32_t sign_type)
{
	uint32_t opcode = CMD_SIGNATURE;
	unsigned int start_pc = program->current_pc;

	switch (sign_type) {
	case (SIGN_TYPE_FINAL):
	case (SIGN_TYPE_FINAL_RESTORE):
	case (SIGN_TYPE_FINAL_NONZERO):
	case (SIGN_TYPE_IMM_2):
	case (SIGN_TYPE_IMM_3):
	case (SIGN_TYPE_IMM_4):
		opcode |= sign_type;
		break;
	default:
		pr_err("SIGNATURE Command: Invalid type selection\n");
		goto err;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return -EINVAL;
}

#endif /* __RTA_SIGNATURE_CMD_H__ */
