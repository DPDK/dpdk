/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <rte_atomic.h>

#include "test.h"

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

int
test_atomic(void)
{
	rte_atomic16_t a16 = RTE_ATOMIC16_INIT(1);
	rte_atomic32_t a32 = RTE_ATOMIC32_INIT(1);
	rte_atomic64_t a64 = RTE_ATOMIC64_INIT(1);
	int x;

	rte_mb();
	rte_wmb();
	rte_rmb();

	rte_atomic16_init(&a16);
	rte_atomic16_set(&a16, 1);
	x = rte_atomic16_read(&a16);
	rte_atomic16_inc(&a16);
	rte_atomic16_dec(&a16);
	rte_atomic16_add(&a16, 5);
	rte_atomic16_sub(&a16, 5);
	x = rte_atomic16_test_and_set(&a16);
	x = rte_atomic16_add_return(&a16, 10);

	rte_atomic32_init(&a32);
	rte_atomic32_set(&a32, 1);
	x = rte_atomic32_read(&a32);
	rte_atomic32_inc(&a32);
	rte_atomic32_dec(&a32);
	rte_atomic32_add(&a32, 5);
	rte_atomic32_sub(&a32, 5);
	x = rte_atomic32_test_and_set(&a32);
	x = rte_atomic32_add_return(&a32, 10);

	rte_atomic64_init(&a64);
	rte_atomic64_set(&a64, 1);
	x = rte_atomic64_read(&a64);
	rte_atomic64_inc(&a64);
	rte_atomic64_dec(&a64);
	rte_atomic64_add(&a64, 5);
	rte_atomic64_sub(&a64, 5);
	x = rte_atomic64_test_and_set(&a64);
	x = rte_atomic64_add_return(&a64, 10);
	(void)x;

	return 1;
}

