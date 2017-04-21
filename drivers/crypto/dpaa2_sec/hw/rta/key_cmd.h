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

#ifndef __RTA_KEY_CMD_H__
#define __RTA_KEY_CMD_H__

extern enum rta_sec_era rta_sec_era;

/* Allowed encryption flags for each SEC Era */
static const uint32_t key_enc_flags[] = {
	ENC,
	ENC | NWB | EKT | TK,
	ENC | NWB | EKT | TK,
	ENC | NWB | EKT | TK,
	ENC | NWB | EKT | TK,
	ENC | NWB | EKT | TK,
	ENC | NWB | EKT | TK | PTS,
	ENC | NWB | EKT | TK | PTS
};

static inline int
rta_key(struct program *program, uint32_t key_dst,
	uint32_t encrypt_flags, uint64_t src, uint32_t length,
	uint32_t flags)
{
	uint32_t opcode = 0;
	bool is_seq_cmd = false;
	unsigned int start_pc = program->current_pc;

	if (encrypt_flags & ~key_enc_flags[rta_sec_era]) {
		pr_err("KEY: Flag(s) not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}

	/* write cmd type */
	if (flags & SEQ) {
		opcode = CMD_SEQ_KEY;
		is_seq_cmd = true;
	} else {
		opcode = CMD_KEY;
	}

	/* check parameters */
	if (is_seq_cmd) {
		if ((flags & IMMED) || (flags & SGF)) {
			pr_err("SEQKEY: Invalid flag. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		if ((rta_sec_era <= RTA_SEC_ERA_5) &&
		    ((flags & VLF) || (flags & AIDF))) {
			pr_err("SEQKEY: Flag(s) not supported by SEC Era %d\n",
			       USER_SEC_ERA(rta_sec_era));
			goto err;
		}
	} else {
		if ((flags & AIDF) || (flags & VLF)) {
			pr_err("KEY: Invalid flag. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
		if ((flags & SGF) && (flags & IMMED)) {
			pr_err("KEY: Invalid flag. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
	}

	if ((encrypt_flags & PTS) &&
	    ((encrypt_flags & ENC) || (encrypt_flags & NWB) ||
	     (key_dst == PKE))) {
		pr_err("KEY: Invalid flag / destination. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	if (key_dst == AFHA_SBOX) {
		if (rta_sec_era == RTA_SEC_ERA_7) {
			pr_err("KEY: AFHA S-box not supported by SEC Era %d\n",
			       USER_SEC_ERA(rta_sec_era));
			goto err;
		}

		if (flags & IMMED) {
			pr_err("KEY: Invalid flag. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}

		/*
		 * Sbox data loaded into the ARC-4 processor must be exactly
		 * 258 bytes long, or else a data sequence error is generated.
		 */
		if (length != 258) {
			pr_err("KEY: Invalid length. SEC PC: %d; Instr: %d\n",
			       program->current_pc,
			       program->current_instruction);
			goto err;
		}
	}

	/* write key destination and class fields */
	switch (key_dst) {
	case (KEY1):
		opcode |= KEY_DEST_CLASS1;
		break;
	case (KEY2):
		opcode |= KEY_DEST_CLASS2;
		break;
	case (PKE):
		opcode |= KEY_DEST_CLASS1 | KEY_DEST_PKHA_E;
		break;
	case (AFHA_SBOX):
		opcode |= KEY_DEST_CLASS1 | KEY_DEST_AFHA_SBOX;
		break;
	case (MDHA_SPLIT_KEY):
		opcode |= KEY_DEST_CLASS2 | KEY_DEST_MDHA_SPLIT;
		break;
	default:
		pr_err("KEY: Invalid destination. SEC PC: %d; Instr: %d\n",
		       program->current_pc, program->current_instruction);
		goto err;
	}

	/* write key length */
	length &= KEY_LENGTH_MASK;
	opcode |= length;

	/* write key command specific flags */
	if (encrypt_flags & ENC) {
		/* Encrypted (black) keys must be padded to 8 bytes (CCM) or
		 * 16 bytes (ECB) depending on EKT bit. AES-CCM encrypted keys
		 * (EKT = 1) have 6-byte nonce and 6-byte MAC after padding.
		 */
		opcode |= KEY_ENC;
		if (encrypt_flags & EKT) {
			opcode |= KEY_EKT;
			length = ALIGN(length, 8);
			length += 12;
		} else {
			length = ALIGN(length, 16);
		}
		if (encrypt_flags & TK)
			opcode |= KEY_TK;
	}
	if (encrypt_flags & NWB)
		opcode |= KEY_NWB;
	if (encrypt_flags & PTS)
		opcode |= KEY_PTS;

	/* write general command flags */
	if (!is_seq_cmd) {
		if (flags & IMMED)
			opcode |= KEY_IMM;
		if (flags & SGF)
			opcode |= KEY_SGF;
	} else {
		if (flags & AIDF)
			opcode |= KEY_AIDF;
		if (flags & VLF)
			opcode |= KEY_VLF;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	if (flags & IMMED)
		__rta_inline_data(program, src, flags & __COPY_MASK, length);
	else
		__rta_out64(program, program->ps, src);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return -EINVAL;
}

#endif /* __RTA_KEY_CMD_H__ */
