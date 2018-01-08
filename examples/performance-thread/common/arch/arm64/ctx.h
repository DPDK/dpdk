/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
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
	void	*sp;		/* 0  */
	void	*fp;		/* 8 */
	void	*lr;		/* 16  */

	/* Callee Saved Generic Registers */
	void	*r19;		/* 24 */
	void	*r20;		/* 32 */
	void	*r21;		/* 40 */
	void	*r22;		/* 48 */
	void	*r23;		/* 56 */
	void	*r24;		/* 64 */
	void	*r25;		/* 72 */
	void	*r26;		/* 80 */
	void	*r27;		/* 88 */
	void	*r28;		/* 96 */

	/*
	 * Callee Saved SIMD Registers. Only the bottom 64-bits
	 * of these registers needs to be saved.
	 */
	void	*v8;		/* 104 */
	void	*v9;		/* 112 */
	void	*v10;		/* 120 */
	void	*v11;		/* 128 */
	void	*v12;		/* 136 */
	void	*v13;		/* 144 */
	void	*v14;		/* 152 */
	void	*v15;		/* 160 */
};


void
ctx_switch(struct ctx *new_ctx, struct ctx *curr_ctx);


#ifdef __cplusplus
}
#endif

#endif /* RTE_CTX_H_ */
