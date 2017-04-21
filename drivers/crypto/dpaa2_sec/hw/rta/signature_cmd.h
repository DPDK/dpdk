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
