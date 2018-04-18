/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2012 Hasan Alayli <halayli@gmail.com>
 */

#if defined(__x86_64__)
__asm__ (
".text\n"
".p2align 4,,15\n"
".globl ctx_switch\n"
".globl _ctx_switch\n"
"ctx_switch:\n"
"_ctx_switch:\n"
"	movq %rsp, 0(%rsi)	# save stack_pointer\n"
"	movq %rbp, 8(%rsi)	# save frame_pointer\n"
"	movq (%rsp), %rax	# save insn_pointer\n"
"	movq %rax, 16(%rsi)\n"
"	movq %rbx, 24(%rsi)\n	# save rbx,r12-r15\n"
"	movq 24(%rdi), %rbx\n"
"	movq %r15, 56(%rsi)\n"
"	movq %r14, 48(%rsi)\n"
"	movq 48(%rdi), %r14\n"
"	movq 56(%rdi), %r15\n"
"	movq %r13, 40(%rsi)\n"
"	movq %r12, 32(%rsi)\n"
"	movq 32(%rdi), %r12\n"
"	movq 40(%rdi), %r13\n"
"	movq 0(%rdi), %rsp	# restore stack_pointer\n"
"	movq 16(%rdi), %rax	# restore insn_pointer\n"
"	movq 8(%rdi), %rbp	# restore frame_pointer\n"
"	movq %rax, (%rsp)\n"
"	ret\n"
	);
#else
#pragma GCC error "__x86_64__ is not defined"
#endif
