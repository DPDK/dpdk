/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016,2019 NXP
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
	DNR | TD | MTD | SHR | REO | EXT,
	DNR | TD | MTD | SHR | REO | EXT,
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
	if (rta_sec_era >= RTA_SEC_ERA_10)
		opcode |= (start_idx << HDR_START_IDX_SHIFT) &
				HDR_START_IDX_MASK_ERA10;
	else
		opcode |= (start_idx << HDR_START_IDX_SHIFT) &
				HDR_START_IDX_MASK;

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

	if ((flags & EXT) && !(flags & SHR) && (start_idx < 2)) {
		pr_err("JOB_DESC: Start index must be >= 2 in case of no SHR and EXT. SEC Program Line: %d\n",
		       program->current_pc);
		goto err;
	}

	opcode |= HDR_ONE;
	if (rta_sec_era >= RTA_SEC_ERA_10)
		opcode |= (start_idx << HDR_START_IDX_SHIFT) &
				HDR_START_IDX_MASK_ERA10;
	else
		opcode |= (start_idx << HDR_START_IDX_SHIFT) &
				HDR_START_IDX_MASK;

	if (flags & EXT) {
		opcode |= HDR_EXT;

		if (ext_flags & DSV) {
			hdr_ext |= HDR_EXT_DSEL_VALID;
			hdr_ext |= ext_flags & DSEL_MASK;
		}

		if (ext_flags & FTD)
			hdr_ext |= HDR_EXT_FTD;
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
