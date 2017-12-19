/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
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
