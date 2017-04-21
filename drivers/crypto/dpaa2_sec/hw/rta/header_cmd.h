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

#ifndef __RTA_HEADER_CMD_H__
#define __RTA_HEADER_CMD_H__

extern enum rta_sec_era rta_sec_era;

/* Allowed job header flags for each SEC Era. */
static const uint32_t job_header_flags[] = {
	DNR | TD | MTD | SHR | REO,
	DNR | TD | MTD | SHR | REO | RSMS,
	DNR | TD | MTD | SHR | REO | RSMS,
	DNR | TD | MTD | SHR | REO | RSMS,
	DNR | TD | MTD | SHR | REO | RSMS | EXT,
	DNR | TD | MTD | SHR | REO | RSMS | EXT,
	DNR | TD | MTD | SHR | REO | RSMS | EXT,
	DNR | TD | MTD | SHR | REO | EXT
};

/* Allowed shared header flags for each SEC Era. */
static const uint32_t shr_header_flags[] = {
	DNR | SC | PD,
	DNR | SC | PD | CIF,
	DNR | SC | PD | CIF,
	DNR | SC | PD | CIF | RIF,
	DNR | SC | PD | CIF | RIF,
	DNR | SC | PD | CIF | RIF,
	DNR | SC | PD | CIF | RIF,
	DNR | SC | PD | CIF | RIF
};

static inline int
rta_shr_header(struct program *program,
	       enum rta_share_type share,
	       unsigned int start_idx,
	       uint32_t flags)
{
	uint32_t opcode = CMD_SHARED_DESC_HDR;
	unsigned int start_pc = program->current_pc;

	if (flags & ~shr_header_flags[rta_sec_era]) {
		pr_err("SHR_DESC: Flag(s) not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}

	switch (share) {
	case SHR_ALWAYS:
		opcode |= HDR_SHARE_ALWAYS;
		break;
	case SHR_SERIAL:
		opcode |= HDR_SHARE_SERIAL;
		break;
	case SHR_NEVER:
		/*
		 * opcode |= HDR_SHARE_NEVER;
		 * HDR_SHARE_NEVER is 0
		 */
		break;
	case SHR_WAIT:
		opcode |= HDR_SHARE_WAIT;
		break;
	default:
		pr_err("SHR_DESC: SHARE VALUE is not supported. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	opcode |= HDR_ONE;
	opcode |= (start_idx << HDR_START_IDX_SHIFT) & HDR_START_IDX_MASK;

	if (flags & DNR)
		opcode |= HDR_DNR;
	if (flags & CIF)
		opcode |= HDR_CLEAR_IFIFO;
	if (flags & SC)
		opcode |= HDR_SAVECTX;
	if (flags & PD)
		opcode |= HDR_PROP_DNR;
	if (flags & RIF)
		opcode |= HDR_RIF;

	__rta_out32(program, opcode);
	program->current_instruction++;

	if (program->current_instruction == 1)
		program->shrhdr = program->buffer;

	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return -EINVAL;
}

static inline int
rta_job_header(struct program *program,
	       enum rta_share_type share,
	       unsigned int start_idx,
	       uint64_t shr_desc, uint32_t flags,
	       uint32_t ext_flags)
{
	uint32_t opcode = CMD_DESC_HDR;
	uint32_t hdr_ext = 0;
	unsigned int start_pc = program->current_pc;

	if (flags & ~job_header_flags[rta_sec_era]) {
		pr_err("JOB_DESC: Flag(s) not supported by SEC Era %d\n",
		       USER_SEC_ERA(rta_sec_era));
		goto err;
	}

	switch (share) {
	case SHR_ALWAYS:
		opcode |= HDR_SHARE_ALWAYS;
		break;
	case SHR_SERIAL:
		opcode |= HDR_SHARE_SERIAL;
		break;
	case SHR_NEVER:
		/*
		 * opcode |= HDR_SHARE_NEVER;
		 * HDR_SHARE_NEVER is 0
		 */
		break;
	case SHR_WAIT:
		opcode |= HDR_SHARE_WAIT;
		break;
	case SHR_DEFER:
		opcode |= HDR_SHARE_DEFER;
		break;
	default:
		pr_err("JOB_DESC: SHARE VALUE is not supported. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	if ((flags & TD) && (flags & REO)) {
		pr_err("JOB_DESC: REO flag not supported for trusted descriptors. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	if ((rta_sec_era < RTA_SEC_ERA_7) && (flags & MTD) && !(flags & TD)) {
		pr_err("JOB_DESC: Trying to MTD a descriptor that is not a TD. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	if ((flags & EXT) && !(flags & SHR) && (start_idx < 2)) {
		pr_err("JOB_DESC: Start index must be >= 2 in case of no SHR and EXT. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	opcode |= HDR_ONE;
	opcode |= ((start_idx << HDR_START_IDX_SHIFT) & HDR_START_IDX_MASK);

	if (flags & EXT) {
		opcode |= HDR_EXT;

		if (ext_flags & DSV) {
			hdr_ext |= HDR_EXT_DSEL_VALID;
			hdr_ext |= ext_flags & DSEL_MASK;
		}

		if (ext_flags & FTD) {
			if (rta_sec_era <= RTA_SEC_ERA_5) {
				pr_err("JOB_DESC: Fake trusted descriptor not supported by SEC Era %d\n",
				       USER_SEC_ERA(rta_sec_era));
				goto err;
			}

			hdr_ext |= HDR_EXT_FTD;
		}
	}
	if (flags & RSMS)
		opcode |= HDR_RSLS;
	if (flags & DNR)
		opcode |= HDR_DNR;
	if (flags & TD)
		opcode |= HDR_TRUSTED;
	if (flags & MTD)
		opcode |= HDR_MAKE_TRUSTED;
	if (flags & REO)
		opcode |= HDR_REVERSE;
	if (flags & SHR)
		opcode |= HDR_SHARED;

	__rta_out32(program, opcode);
	program->current_instruction++;

	if (program->current_instruction == 1) {
		program->jobhdr = program->buffer;

		if (opcode & HDR_SHARED)
			__rta_out64(program, program->ps, shr_desc);
	}

	if (flags & EXT)
		__rta_out32(program, hdr_ext);

	/* Note: descriptor length is set in program_finalize routine */
	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return -EINVAL;
}

#endif /* __RTA_HEADER_CMD_H__ */
