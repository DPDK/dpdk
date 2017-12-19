/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 *
 */

#include "qbman_sys.h"
#include <fsl_qbman_portal.h>

uint32_t qman_version;
#define QMAN_REV_4000   0x04000000
#define QMAN_REV_4100   0x04010000
#define QMAN_REV_4101   0x04010001

/* All QBMan command and result structures use this "valid bit" encoding */
#define QB_VALID_BIT ((uint32_t)0x80)

/* Management command result codes */
#define QBMAN_MC_RSLT_OK      0xf0

/* QBMan DQRR size is set at runtime in qbman_portal.c */

#define QBMAN_EQCR_SIZE 8

static inline uint8_t qm_cyc_diff(uint8_t ringsize, uint8_t first,
				  uint8_t last)
{
	/* 'first' is included, 'last' is excluded */
	if (first <= last)
		return last - first;
	return (2 * ringsize) + last - first;
}

/* --------------------- */
/* portal data structure */
/* --------------------- */

struct qbman_swp {
	struct qbman_swp_desc desc;
	/* The qbman_sys (ie. arch/OS-specific) support code can put anything it
	 * needs in here.
	 */
	struct qbman_swp_sys sys;
	/* Management commands */
	struct {
#ifdef QBMAN_CHECKING
		enum swp_mc_check {
			swp_mc_can_start, /* call __qbman_swp_mc_start() */
			swp_mc_can_submit, /* call __qbman_swp_mc_submit() */
			swp_mc_can_poll, /* call __qbman_swp_mc_result() */
		} check;
#endif
		uint32_t valid_bit; /* 0x00 or 0x80 */
	} mc;
	/* Push dequeues */
	uint32_t sdq;
	/* Volatile dequeues */
	struct {
		/* VDQCR supports a "1 deep pipeline", meaning that if you know
		 * the last-submitted command is already executing in the
		 * hardware (as evidenced by at least 1 valid dequeue result),
		 * you can write another dequeue command to the register, the
		 * hardware will start executing it as soon as the
		 * already-executing command terminates. (This minimises latency
		 * and stalls.) With that in mind, this "busy" variable refers
		 * to whether or not a command can be submitted, not whether or
		 * not a previously-submitted command is still executing. In
		 * other words, once proof is seen that the previously-submitted
		 * command is executing, "vdq" is no longer "busy".
		 */
		atomic_t busy;
		uint32_t valid_bit; /* 0x00 or 0x80 */
		/* We need to determine when vdq is no longer busy. This depends
		 * on whether the "busy" (last-submitted) dequeue command is
		 * targeting DQRR or main-memory, and detected is based on the
		 * presence of the dequeue command's "token" showing up in
		 * dequeue entries in DQRR or main-memory (respectively).
		 */
		struct qbman_result *storage; /* NULL if DQRR */
	} vdq;
	/* DQRR */
	struct {
		uint32_t next_idx;
		uint32_t valid_bit;
		uint8_t dqrr_size;
		int reset_bug;
	} dqrr;
	struct {
		uint32_t pi;
		uint32_t pi_vb;
		uint32_t ci;
		int available;
	} eqcr;
};

/* -------------------------- */
/* portal management commands */
/* -------------------------- */

/* Different management commands all use this common base layer of code to issue
 * commands and poll for results. The first function returns a pointer to where
 * the caller should fill in their MC command (though they should ignore the
 * verb byte), the second function commits merges in the caller-supplied command
 * verb (which should not include the valid-bit) and submits the command to
 * hardware, and the third function checks for a completed response (returns
 * non-NULL if only if the response is complete).
 */
void *qbman_swp_mc_start(struct qbman_swp *p);
void qbman_swp_mc_submit(struct qbman_swp *p, void *cmd, uint8_t cmd_verb);
void *qbman_swp_mc_result(struct qbman_swp *p);

/* Wraps up submit + poll-for-result */
static inline void *qbman_swp_mc_complete(struct qbman_swp *swp, void *cmd,
					  uint8_t cmd_verb)
{
	int loopvar = 1000;

	qbman_swp_mc_submit(swp, cmd, cmd_verb);
	do {
		cmd = qbman_swp_mc_result(swp);
	} while (!cmd && loopvar--);
	QBMAN_BUG_ON(!loopvar);

	return cmd;
}

/* ---------------------- */
/* Descriptors/cachelines */
/* ---------------------- */

/* To avoid needless dynamic allocation, the driver API often gives the caller
 * a "descriptor" type that the caller can instantiate however they like.
 * Ultimately though, it is just a cacheline of binary storage (or something
 * smaller when it is known that the descriptor doesn't need all 64 bytes) for
 * holding pre-formatted pieces of hardware commands. The performance-critical
 * code can then copy these descriptors directly into hardware command
 * registers more efficiently than trying to construct/format commands
 * on-the-fly. The API user sees the descriptor as an array of 32-bit words in
 * order for the compiler to know its size, but the internal details are not
 * exposed. The following macro is used within the driver for converting *any*
 * descriptor pointer to a usable array pointer. The use of a macro (instead of
 * an inline) is necessary to work with different descriptor types and to work
 * correctly with const and non-const inputs (and similarly-qualified outputs).
 */
#define qb_cl(d) (&(d)->donot_manipulate_directly[0])
