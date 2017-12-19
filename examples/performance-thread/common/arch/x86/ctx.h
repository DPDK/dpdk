/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


#ifndef CTX_H
#define CTX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU context registers
 */
struct ctx {
	void	*rsp;		/* 0  */
	void	*rbp;		/* 8  */
	void	*rip;		/* 16 */
	void	*rbx;		/* 24 */
	void	*r12;		/* 32 */
	void	*r13;		/* 40 */
	void	*r14;		/* 48 */
	void	*r15;		/* 56 */
};


void
ctx_switch(struct ctx *new_ctx, struct ctx *curr_ctx);


#ifdef __cplusplus
}
#endif

#endif /* RTE_CTX_H_ */
