/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _TB_MEM_H_
#define _TB_MEM_H_

/**
 * @file
 *
 * RTE ACL temporary (build phase) memory management.
 * Contains structures and functions to manage temporary (used by build only)
 * memory. Memory allocated in large blocks to speed 'free' when trie is
 * destructed (finish of build phase).
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_acl_osdep.h>
#include <setjmp.h>

struct tb_mem_block {
	struct tb_mem_block *next;
	struct tb_mem_pool  *pool;
	size_t               size;
	uint8_t             *mem;
};

struct tb_mem_pool {
	struct tb_mem_block *block;
	size_t               alignment;
	size_t               min_alloc;
	size_t               alloc;
	/* jump target in case of memory allocation failure. */
	sigjmp_buf           fail;
};

void *tb_alloc(struct tb_mem_pool *pool, size_t size);
void tb_free_pool(struct tb_mem_pool *pool);

#ifdef __cplusplus
}
#endif

#endif /* _TB_MEM_H_ */
