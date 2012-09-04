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

#include <rte_hash.h>

#include "test.h"

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

/* Parameters used for hash table in unit test functions. */
static struct rte_hash_parameters ut_params = {
	"name", /* name */
	64,  /* entries */
	4, /* bucket_entries */
	8, /* key_len */
	0, /* hash_func */
	0, /* hash_func_init_val */
	0, /* socket_id */
};

struct key {
	char key[8];
};

/* Keys used by unit test functions */
static struct key keys[1] = {
	{
		{ 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, },
	}
};

int test_hash(void)
{
	struct rte_hash *handle;
	int32_t pos0;

	handle = rte_hash_create(&ut_params);
	if (handle == 0) {
		return -1;
	}
	pos0 = rte_hash_add_key(handle, &keys[0]);
	pos0 = rte_hash_lookup(handle, &keys[0]);
	pos0 = rte_hash_del_key(handle, &keys[0]);
	rte_hash_free(handle);
	(void)pos0; /* remove compiler warning */
	return 0;
}
