/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
