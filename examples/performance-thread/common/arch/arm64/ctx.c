/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <rte_common.h>
#include <ctx.h>

void
ctx_switch(struct ctx *new_ctx __rte_unused, struct ctx *curr_ctx __rte_unused)
{
	/* SAVE CURRENT CONTEXT */
	asm volatile (
		/* Save SP */
		"mov x3, sp\n"
		"str x3, [x1, #0]\n"

		/* Save FP and LR */
		"stp x29, x30, [x1, #8]\n"

		/* Save Callee Saved Regs x19 - x28 */
		"stp x19, x20, [x1, #24]\n"
		"stp x21, x22, [x1, #40]\n"
		"stp x23, x24, [x1, #56]\n"
		"stp x25, x26, [x1, #72]\n"
		"stp x27, x28, [x1, #88]\n"

		/*
		 * Save bottom 64-bits of Callee Saved
		 * SIMD Regs v8 - v15
		 */
		"stp d8, d9, [x1, #104]\n"
		"stp d10, d11, [x1, #120]\n"
		"stp d12, d13, [x1, #136]\n"
		"stp d14, d15, [x1, #152]\n"
	);

	/* RESTORE NEW CONTEXT */
	asm volatile (
		/* Restore SP */
		"ldr x3, [x0, #0]\n"
		"mov sp, x3\n"

		/* Restore FP and LR */
		"ldp x29, x30, [x0, #8]\n"

		/* Restore Callee Saved Regs x19 - x28 */
		"ldp x19, x20, [x0, #24]\n"
		"ldp x21, x22, [x0, #40]\n"
		"ldp x23, x24, [x0, #56]\n"
		"ldp x25, x26, [x0, #72]\n"
		"ldp x27, x28, [x0, #88]\n"

		/*
		 * Restore bottom 64-bits of Callee Saved
		 * SIMD Regs v8 - v15
		 */
		"ldp d8, d9, [x0, #104]\n"
		"ldp d10, d11, [x0, #120]\n"
		"ldp d12, d13, [x0, #136]\n"
		"ldp d14, d15, [x0, #152]\n"
	);
}
