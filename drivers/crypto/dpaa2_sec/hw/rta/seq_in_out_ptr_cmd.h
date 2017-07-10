/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
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

#ifndef __RTA_SEQ_IN_OUT_PTR_CMD_H__
#define __RTA_SEQ_IN_OUT_PTR_CMD_H__

extern enum rta_sec_era rta_sec_era;

/* Allowed SEQ IN PTR flags for each SEC Era. */
static const uint32_t seq_in_ptr_flags[] = {
	RBS | INL | SGF | PRE | EXT | RTO,
	RBS | INL | SGF | PRE | EXT | RTO | RJD,
	RBS | INL | SGF | PRE | EXT | RTO | RJD,
	RBS | INL | SGF | PRE | EXT | RTO | RJD,
	RBS | INL | SGF | PRE | EXT | RTO | RJD | SOP,
	RBS | INL | SGF | PRE | EXT | RTO | RJD | SOP,
	RBS | INL | SGF | PRE | EXT | RTO | RJD | SOP,
	RBS | INL | SGF | PRE | EXT | RTO | RJD | SOP
};

/* Allowed SEQ OUT PTR flags for each SEC Era. */
static const uint32_t seq_out_ptr_flags[] = {
	SGF | PRE | EXT,
	SGF | PRE | EXT | RTO,
	SGF | PRE | EXT | RTO,
	SGF | PRE | EXT | RTO,
	SGF | PRE | EXT | RTO | RST | EWS,
	SGF | PRE | EXT | RTO | RST | EWS,
	SGF | PRE | EXT | RTO | RST | EWS,
	SGF | PRE | EXT | RTO | RST | EWS
};

static inline int
rta_seq_in_ptr(struct program *program, uint64_t src,
	       uint32_t length, uint32_t flags)
{
	uint32_t opcode = CMD_SEQ_IN_PTR;
	unsigned int start_pc = program->current_pc;
	int ret = -EINVAL;

	/* Parameters checking */
	if ((flags & RTO) && (flags & PRE)) {
		pr_err("SEQ IN PTR: Invalid usage of RTO and PRE flags\n");
		goto err;
	}
	if (flags & ~seq_in_ptr_flags[rta_sec_era]) {
		pr_err("SEQ IN PTR: Flag(s) not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}
	if ((flags & INL) && (flags & RJD)) {
		pr_err("SEQ IN PTR: Invalid usage of INL and RJD flags\n");
		goto err;
	}
	if ((src) && (flags & (SOP | RTO | PRE))) {
		pr_err("SEQ IN PTR: Invalid usage of RTO or PRE flag\n");
		goto err;
	}
	if ((flags & SOP) && (flags & (RBS | PRE | RTO | EXT))) {
		pr_err("SEQ IN PTR: Invalid usage of SOP and (RBS or PRE or RTO or EXT) flags\n");
		goto err;
	}

	/* write flag fields */
	if (flags & RBS)
		opcode |= SQIN_RBS;
	if (flags & INL)
		opcode |= SQIN_INL;
	if (flags & SGF)
		opcode |= SQIN_SGF;
	if (flags & PRE)
		opcode |= SQIN_PRE;
	if (flags & RTO)
		opcode |= SQIN_RTO;
	if (flags & RJD)
		opcode |= SQIN_RJD;
	if (flags & SOP)
		opcode |= SQIN_SOP;
	if ((length >> 16) || (flags & EXT)) {
		if (flags & SOP) {
			pr_err("SEQ IN PTR: Invalid usage of SOP and EXT flags\n");
			goto err;
		}

		opcode |= SQIN_EXT;
	} else {
		opcode |= length & SQIN_LEN_MASK;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* write pointer or immediate data field */
	if (!(opcode & (SQIN_PRE | SQIN_RTO | SQIN_SOP)))
		__rta_out64(program, program->ps, src);

	/* write extended length field */
	if (opcode & SQIN_EXT)
		__rta_out32(program, length);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

static inline int
rta_seq_out_ptr(struct program *program, uint64_t dst,
		uint32_t length, uint32_t flags)
{
	uint32_t opcode = CMD_SEQ_OUT_PTR;
	unsigned int start_pc = program->current_pc;
	int ret = -EINVAL;

	/* Parameters checking */
	if (flags & ~seq_out_ptr_flags[rta_sec_era]) {
		pr_err("SEQ OUT PTR: Flag(s) not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}
	if ((flags & RTO) && (flags & PRE)) {
		pr_err("SEQ OUT PTR: Invalid usage of RTO and PRE flags\n");
		goto err;
	}
	if ((dst) && (flags & (RTO | PRE))) {
		pr_err("SEQ OUT PTR: Invalid usage of RTO or PRE flag\n");
		goto err;
	}
	if ((flags & RST) && !(flags & RTO)) {
		pr_err("SEQ OUT PTR: RST flag must be used with RTO flag\n");
		goto err;
	}

	/* write flag fields */
	if (flags & SGF)
		opcode |= SQOUT_SGF;
	if (flags & PRE)
		opcode |= SQOUT_PRE;
	if (flags & RTO)
		opcode |= SQOUT_RTO;
	if (flags & RST)
		opcode |= SQOUT_RST;
	if (flags & EWS)
		opcode |= SQOUT_EWS;
	if ((length >> 16) || (flags & EXT))
		opcode |= SQOUT_EXT;
	else
		opcode |= length & SQOUT_LEN_MASK;

	__rta_out32(program, opcode);
	program->current_instruction++;

	/* write pointer or immediate data field */
	if (!(opcode & (SQOUT_PRE | SQOUT_RTO)))
		__rta_out64(program, program->ps, dst);

	/* write extended length field */
	if (opcode & SQOUT_EXT)
		__rta_out32(program, length);

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_SEQ_IN_OUT_PTR_CMD_H__ */
