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
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_ip.h>
#include <rte_string_fns.h>

#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#endif
#include <cmdline_parse.h>

#include "test.h"

#ifdef RTE_LIBRTE_HASH

/* Types of hash table performance test that can be performed */
enum hash_test_t {
	ADD_ON_EMPTY,		/*< Add keys to empty table */
	DELETE_ON_EMPTY,	/*< Attempt to delete keys from empty table */
	LOOKUP_ON_EMPTY,	/*< Attempt to find keys in an empty table */
	ADD_UPDATE,		/*< Add/update keys in a full table */
	DELETE,			/*< Delete keys from a full table */
	LOOKUP			/*< Find keys in a full table */
};

/* Function type for hash table operations. */
typedef int32_t (*hash_operation)(const struct rte_hash *h, const void *key);

/* Structure to hold parameters used to run a hash table performance test */
struct tbl_perf_test_params {
	enum hash_test_t test_type;
	uint32_t num_iterations;
	uint32_t entries;
	uint32_t bucket_entries;
	uint32_t key_len;
	rte_hash_function hash_func;
	uint32_t hash_func_init_val;
};

#define ITERATIONS 10000
#define LOCAL_FBK_HASH_ENTRIES_MAX (1 << 15)

/*******************************************************************************
 * Hash table performance test configuration section.
 */
struct tbl_perf_test_params tbl_perf_params[] =
{
/* Small table, add */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |     HashFunc | InitVal */
{ ADD_ON_EMPTY,        1024,     1024,           1,      16,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      16,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      16,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      16,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      16,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      32,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      32,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      32,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      32,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      32,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      48,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      48,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      48,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      48,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      48,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      64,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      64,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      64,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      64,     rte_jhash,  0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      64,     rte_jhash,  0},
/* Small table, update */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |     HashFunc | InitVal */
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      16,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      16,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      16,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      16,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      16,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      32,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      32,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      32,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      32,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      32,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      48,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      48,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      48,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      48,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      48,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      64,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      64,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      64,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      64,     rte_jhash,  0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      64,     rte_jhash,  0},
/* Small table, lookup */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |     HashFunc | InitVal */
{       LOOKUP,  ITERATIONS,     1024,           1,      16,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           2,      16,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           4,      16,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           8,      16,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,          16,      16,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           1,      32,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           2,      32,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           4,      32,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           8,      32,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,          16,      32,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           1,      48,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           2,      48,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           4,      48,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           8,      48,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,          16,      48,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           1,      64,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           2,      64,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           4,      64,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,           8,      64,     rte_jhash,  0},
{       LOOKUP,  ITERATIONS,     1024,          16,      64,     rte_jhash,  0},
/* Big table, add */
/* Test type  | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      16,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      16,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      16,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      16,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      16,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      32,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      32,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      32,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      32,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      32,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      48,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      48,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      48,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      48,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      48,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      64,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      64,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      64,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      64,    rte_jhash,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      64,    rte_jhash,   0},
/* Big table, update */
/* Test type  | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      16,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      16,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      16,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      16,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      16,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      32,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      32,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      32,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      32,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      32,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      48,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      48,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      48,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      48,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      48,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      64,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      64,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      64,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      64,    rte_jhash,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      64,    rte_jhash,   0},
/* Big table, lookup */
/* Test type  | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{       LOOKUP,  ITERATIONS,  1048576,           1,      16,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      16,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      16,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      16,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      16,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      32,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      32,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      32,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      32,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      32,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      48,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      48,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      48,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      48,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      48,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      64,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      64,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      64,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      64,    rte_jhash,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      64,    rte_jhash,   0},

/* Small table, add */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{ ADD_ON_EMPTY,        1024,     1024,           1,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           1,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           2,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           4,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,           8,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,        1024,     1024,          16,      64, rte_hash_crc,   0},
/* Small table, update */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           1,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           2,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           4,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,           8,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,     1024,          16,      64, rte_hash_crc,   0},
/* Small table, lookup */
/*  Test type | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{       LOOKUP,  ITERATIONS,     1024,           1,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           2,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           4,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           8,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,          16,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           1,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           2,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           4,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           8,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,          16,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           1,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           2,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           4,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           8,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,          16,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           1,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           2,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           4,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,           8,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,     1024,          16,      64, rte_hash_crc,   0},
/* Big table, add */
/* Test type  | Iterations | Entries | BucketSize | KeyLen |    HashFunc | InitVal */
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      16, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      32, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      48, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           1,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           2,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           4,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,           8,      64, rte_hash_crc,   0},
{ ADD_ON_EMPTY,     1048576,  1048576,          16,      64, rte_hash_crc,   0},
/* Big table, update */
/* Test type  | Iterations | Entries | BucketSize | KeyLen | HashFunc | InitVal */
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      16, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      32, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      48, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           1,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           2,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           4,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,           8,      64, rte_hash_crc,   0},
{   ADD_UPDATE,  ITERATIONS,  1048576,          16,      64, rte_hash_crc,   0},
/* Big table, lookup */
/* Test type  | Iterations | Entries | BucketSize | KeyLen | HashFunc | InitVal */
{       LOOKUP,  ITERATIONS,  1048576,           1,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      16, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      32, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      48, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           1,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           2,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           4,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,           8,      64, rte_hash_crc,   0},
{       LOOKUP,  ITERATIONS,  1048576,          16,      64, rte_hash_crc,   0},
};

/******************************************************************************/

/*******************************************************************************
 * Hash function performance test configuration section. Each performance test
 * will be performed HASHTEST_ITERATIONS times.
 *
 * The five arrays below control what tests are performed. Every combination
 * from the array entries is tested.
 */
#define HASHTEST_ITERATIONS 1000000

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
static rte_hash_function hashtest_funcs[] = {rte_jhash, rte_hash_crc};
#else
static rte_hash_function hashtest_funcs[] = {rte_jhash};
#endif
static uint32_t hashtest_initvals[] = {0};
static uint32_t hashtest_key_lens[] = {2, 4, 5, 6, 7, 8, 10, 11, 15, 16, 21, 31, 32, 33, 63, 64};
/******************************************************************************/

/*
 * Check condition and return an error if true. Assumes that "handle" is the
 * name of the hash structure pointer to be freed.
 */
#define RETURN_IF_ERROR(cond, str, ...) do {				\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__, ##__VA_ARGS__); \
		if (handle) rte_hash_free(handle);			\
		return -1;						\
	}								\
} while(0)

#define RETURN_IF_ERROR_FBK(cond, str, ...) do {				\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__, ##__VA_ARGS__); \
		if (handle) rte_fbk_hash_free(handle);			\
		return -1;						\
	}								\
} while(0)

/* 5-tuple key type */
struct flow_key {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint8_t proto;
} __attribute__((packed));

/*
 * Hash function that always returns the same value, to easily test what
 * happens when a bucket is full.
 */
static uint32_t pseudo_hash(__attribute__((unused)) const void *keys,
			    __attribute__((unused)) uint32_t key_len,
			    __attribute__((unused)) uint32_t init_val)
{
	return 3;
}

/*
 * Print out result of unit test hash operation.
 */
#if defined(UNIT_TEST_HASH_VERBOSE)
static void print_key_info(const char *msg, const struct flow_key *key,
								int32_t pos)
{
	uint8_t *p = (uint8_t *)key;
	unsigned i;

	printf("%s key:0x", msg);
	for (i = 0; i < sizeof(struct flow_key); i++) {
		printf("%02X", p[i]);
	}
	printf(" @ pos %d\n", pos);
}
#else
static void print_key_info(__attribute__((unused)) const char *msg,
		__attribute__((unused)) const struct flow_key *key,
		__attribute__((unused)) int32_t pos)
{
}
#endif

/* Keys used by unit test functions */
static struct flow_key keys[5] = { {
	.ip_src = IPv4(0x03, 0x02, 0x01, 0x00),
	.ip_dst = IPv4(0x07, 0x06, 0x05, 0x04),
	.port_src = 0x0908,
	.port_dst = 0x0b0a,
	.proto = 0x0c,
}, {
	.ip_src = IPv4(0x13, 0x12, 0x11, 0x10),
	.ip_dst = IPv4(0x17, 0x16, 0x15, 0x14),
	.port_src = 0x1918,
	.port_dst = 0x1b1a,
	.proto = 0x1c,
}, {
	.ip_src = IPv4(0x23, 0x22, 0x21, 0x20),
	.ip_dst = IPv4(0x27, 0x26, 0x25, 0x24),
	.port_src = 0x2928,
	.port_dst = 0x2b2a,
	.proto = 0x2c,
}, {
	.ip_src = IPv4(0x33, 0x32, 0x31, 0x30),
	.ip_dst = IPv4(0x37, 0x36, 0x35, 0x34),
	.port_src = 0x3938,
	.port_dst = 0x3b3a,
	.proto = 0x3c,
}, {
	.ip_src = IPv4(0x43, 0x42, 0x41, 0x40),
	.ip_dst = IPv4(0x47, 0x46, 0x45, 0x44),
	.port_src = 0x4948,
	.port_dst = 0x4b4a,
	.proto = 0x4c,
} };

/* Parameters used for hash table in unit test functions. Name set later. */
static struct rte_hash_parameters ut_params = {
	.entries = 64,
	.bucket_entries = 4,
	.key_len = sizeof(struct flow_key), /* 13 */
	.hash_func = rte_jhash,
	.hash_func_init_val = 0,
	.socket_id = 0,
};

/*
 * Basic sequence of operations for a single key:
 *	- add
 *	- lookup (hit)
 *	- delete
 *	- lookup (miss)
 */
static int test_add_delete(void)
{
	struct rte_hash *handle;
	int pos0, expectedPos0;

	ut_params.name = "test1";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 < 0, "failed to add key (pos0=%d)", pos0);
	expectedPos0 = pos0;

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to delete key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	rte_hash_free(handle);
	return 0;
}

/*
 * Sequence of operations for a single key:
 * 	- delete: miss
 * 	- add
 * 	- lookup: hit
 * 	- add: update
 * 	- lookup: hit (updated data)
 * 	- delete: hit
 * 	- delete: miss
 * 	- lookup: miss
 */
static int test_add_update_delete(void)
{
	struct rte_hash *handle;
	int pos0, expectedPos0;

	ut_params.name = "test2";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found non-existent key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 < 0, "failed to add key (pos0=%d)", pos0);
	expectedPos0 = pos0;

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_add_key(handle, &keys[0]);
	print_key_info("Add", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to re-add key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to find key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != expectedPos0,
			"failed to delete key (pos0=%d)", pos0);

	pos0 = rte_hash_del_key(handle, &keys[0]);
	print_key_info("Del", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: deleted already deleted key (pos0=%d)", pos0);

	pos0 = rte_hash_lookup(handle, &keys[0]);
	print_key_info("Lkp", &keys[0], pos0);
	RETURN_IF_ERROR(pos0 != -ENOENT,
			"fail: found key after deleting! (pos0=%d)", pos0);

	rte_hash_free(handle);
	return 0;
}

/*
 * Sequence of operations for find existing hash table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
static int test_hash_find_existing(void)
{
	struct rte_hash *handle = NULL, *result = NULL;

	/* Create hash table. */
	ut_params.name = "hash_find_existing";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Try to find existing hash table */
	result = rte_hash_find_existing("hash_find_existing");
	RETURN_IF_ERROR(result != handle, "could not find existing hash table");

	/* Try to find non-existing hash table */
	result = rte_hash_find_existing("hash_find_non_existing");
	RETURN_IF_ERROR(!(result == NULL), "found table that shouldn't exist");

	/* Cleanup. */
	rte_hash_free(handle);

	return 0;
}

/*
 * Sequence of operations for 5 keys
 *	- add keys
 *	- lookup keys: hit
 *	- add keys (update)
 *	- lookup keys: hit (updated data)
 *	- delete keys : hit
 *	- lookup keys: miss
 */
static int test_five_keys(void)
{
	struct rte_hash *handle;
	const void *key_array[5] = {0};
	int pos[5];
	int expected_pos[5];
	unsigned i;
	int ret;

	ut_params.name = "test3";
	handle = rte_hash_create(&ut_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Add */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
				"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}

	/* Lookup */
	for(i = 0; i < 5; i++)
		key_array[i] = &keys[i];

	ret = rte_hash_lookup_multi(handle, &key_array[0], 5, (int32_t *)pos);
	if(ret == 0)
		for(i = 0; i < 5; i++) {
			print_key_info("Lkp", key_array[i], pos[i]);
			RETURN_IF_ERROR(pos[i] != expected_pos[i],
					"failed to find key (pos[%u]=%d)", i, pos[i]);
		}

	/* Add - update */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to add key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	/* Delete */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_del_key(handle, &keys[i]);
		print_key_info("Del", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
				"failed to delete key (pos[%u]=%d)", i, pos[i]);
	}

	/* Lookup */
	for (i = 0; i < 5; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != -ENOENT,
				"failed to find key (pos[%u]=%d)", i, pos[i]);
	}

	rte_hash_free(handle);

	return 0;
}

/*
 * Add keys to the same bucket until bucket full.
 * 	- add 5 keys to the same bucket (hash created with 4 keys per bucket):
 * 	 	first 4 successful, 5th unsuccessful
 *	- lookup the 5 keys: 4 hits, 1 miss
 *	- add the 5 keys again: 4 OK, one error as bucket is full
 *	- lookup the 5 keys: 4 hits (updated data), 1 miss
 * 	- delete the 5 keys: 5 OK (even if the 5th is not in the table)
 * 	- lookup the 5 keys: 5 misses
 * 	- add the 5th key: OK
 * 	- lookup the 5th key: hit
 */
static int test_full_bucket(void)
{
	struct rte_hash_parameters params_pseudo_hash = {
		.name = "test4",
		.entries = 64,
		.bucket_entries = 4,
		.key_len = sizeof(struct flow_key), /* 13 */
		.hash_func = pseudo_hash,
		.hash_func_init_val = 0,
		.socket_id = 0,
	};
	struct rte_hash *handle;
	int pos[5];
	int expected_pos[5];
	unsigned i;

	handle = rte_hash_create(&params_pseudo_hash);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	/* Fill bucket*/
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] < 0,
			"failed to add key (pos[%u]=%d)", i, pos[i]);
		expected_pos[i] = pos[i];
	}
	/* This shouldn't work because the bucket is full */
	pos[4] = rte_hash_add_key(handle, &keys[4]);
	print_key_info("Add", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != -ENOSPC,
			"fail: added key to full bucket (pos[4]=%d)", pos[4]);

	/* Lookup */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}
	pos[4] = rte_hash_lookup(handle, &keys[4]);
	print_key_info("Lkp", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != -ENOENT,
			"fail: found non-existent key (pos[4]=%d)", pos[4]);

	/* Add - update */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_add_key(handle, &keys[i]);
		print_key_info("Add", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to add key (pos[%u]=%d)", i, pos[i]);
	}
	pos[4] = rte_hash_add_key(handle, &keys[4]);
	print_key_info("Add", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != -ENOSPC,
			"fail: added key to full bucket (pos[4]=%d)", pos[4]);

	/* Lookup */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to find key (pos[%u]=%d)", i, pos[i]);
	}
	pos[4] = rte_hash_lookup(handle, &keys[4]);
	print_key_info("Lkp", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != -ENOENT,
			"fail: found non-existent key (pos[4]=%d)", pos[4]);

	/* Delete 1 key, check other keys are still found */
	pos[1] = rte_hash_del_key(handle, &keys[1]);
	print_key_info("Del", &keys[1], pos[1]);
	RETURN_IF_ERROR(pos[1] != expected_pos[1],
			"failed to delete key (pos[1]=%d)", pos[1]);
	pos[3] = rte_hash_lookup(handle, &keys[3]);
	print_key_info("Lkp", &keys[3], pos[3]);
	RETURN_IF_ERROR(pos[3] != expected_pos[3],
			"failed lookup after deleting key from same bucket "
			"(pos[3]=%d)", pos[3]);

	/* Go back to previous state */
	pos[1] = rte_hash_add_key(handle, &keys[1]);
	print_key_info("Add", &keys[1], pos[1]);
	expected_pos[1] = pos[1];
	RETURN_IF_ERROR(pos[1] < 0, "failed to add key (pos[1]=%d)", pos[1]);

	/* Delete */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_del_key(handle, &keys[i]);
		print_key_info("Del", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != expected_pos[i],
			"failed to delete key (pos[%u]=%d)", i, pos[i]);
	}
	pos[4] = rte_hash_del_key(handle, &keys[4]);
	print_key_info("Del", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != -ENOENT,
			"fail: deleted non-existent key (pos[4]=%d)", pos[4]);

	/* Lookup */
	for (i = 0; i < 4; i++) {
		pos[i] = rte_hash_lookup(handle, &keys[i]);
		print_key_info("Lkp", &keys[i], pos[i]);
		RETURN_IF_ERROR(pos[i] != -ENOENT,
			"fail: found non-existent key (pos[%u]=%d)", i, pos[i]);
	}

	/* Add and lookup the 5th key */
	pos[4] = rte_hash_add_key(handle, &keys[4]);
	print_key_info("Add", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] < 0, "failed to add key (pos[4]=%d)", pos[4]);
	expected_pos[4] = pos[4];
	pos[4] = rte_hash_lookup(handle, &keys[4]);
	print_key_info("Lkp", &keys[4], pos[4]);
	RETURN_IF_ERROR(pos[4] != expected_pos[4],
			"failed to find key (pos[4]=%d)", pos[4]);

	rte_hash_free(handle);

	/* Cover the NULL case. */
	rte_hash_free(0);
	return 0;
}

/*
 * To help print out name of hash functions.
 */
static const char *get_hash_name(rte_hash_function f)
{
	if (f == rte_jhash)
		return "jhash";

	if (f == rte_hash_crc)
		return "rte_hash_crc";

	return "UnknownHash";
}

/*
 * Find average of array of numbers.
 */
static double
get_avg(const uint32_t *array, uint32_t size)
{
	double sum = 0;
	unsigned i;
	for (i = 0; i < size; i++)
		sum += array[i];
	return sum / (double)size;
}

/*
 * Do a single performance test, of one type of operation.
 *
 * @param h
 *   hash table to run test on
 * @param func
 *   function to call (add, delete or lookup function)
 * @param avg_occupancy
 *   The average number of entries in each bucket of the hash table
 * @param invalid_pos_count
 *   The amount of errors (e.g. due to a full bucket).
 * @return
 *   The average number of ticks per hash function call. A negative number
 *   signifies failure.
 */
static double
run_single_tbl_perf_test(const struct rte_hash *h, hash_operation func,
		const struct tbl_perf_test_params *params, double *avg_occupancy,
		uint32_t *invalid_pos_count)
{
	uint64_t begin, end, ticks = 0;
	uint8_t *key = NULL;
	uint32_t *bucket_occupancies = NULL;
	uint32_t num_buckets, i, j;
	int32_t pos;

	/* Initialise */
	num_buckets = params->entries / params->bucket_entries;
	key = (uint8_t *) rte_zmalloc("hash key",
			params->key_len * sizeof(uint8_t), 16);
	if (key == NULL)
		return -1;

	bucket_occupancies = (uint32_t *) rte_zmalloc("bucket occupancies",
			num_buckets * sizeof(uint32_t), 16);
	if (bucket_occupancies == NULL) {
		rte_free(key);
		return -1;
	}

	ticks = 0;
	*invalid_pos_count = 0;

	for (i = 0; i < params->num_iterations; i++) {
		/* Prepare inputs for the current iteration */
		for (j = 0; j < params->key_len; j++)
			key[j] = (uint8_t) rte_rand();

		/* Perform operation, and measure time it takes */
		begin = rte_rdtsc();
		pos = func(h, key);
		end = rte_rdtsc();
		ticks += end - begin;

		/* Other work per iteration */
		if (pos < 0)
			*invalid_pos_count += 1;
		else
			bucket_occupancies[pos / params->bucket_entries]++;
	}
	*avg_occupancy = get_avg(bucket_occupancies, num_buckets);

	rte_free(bucket_occupancies);
	rte_free(key);

	return (double)ticks / params->num_iterations;
}

/*
 * To help print out what tests are being done.
 */
static const char *
get_tbl_perf_test_desc(enum hash_test_t type)
{
	switch (type){
	case ADD_ON_EMPTY: return "Add on Empty";
	case DELETE_ON_EMPTY: return "Delete on Empty";
	case LOOKUP_ON_EMPTY: return "Lookup on Empty";
	case ADD_UPDATE: return "Add Update";
	case DELETE: return "Delete";
	case LOOKUP: return "Lookup";
	default: return "UNKNOWN";
	}
}

/*
 * Run a hash table performance test based on params.
 */
static int
run_tbl_perf_test(struct tbl_perf_test_params *params)
{
	static unsigned calledCount = 5;
	struct rte_hash_parameters hash_params = {
		.entries = params->entries,
		.bucket_entries = params->bucket_entries,
		.key_len = params->key_len,
		.hash_func = params->hash_func,
		.hash_func_init_val = params->hash_func_init_val,
		.socket_id = 0,
	};
	struct rte_hash *handle;
	double avg_occupancy = 0, ticks = 0;
	uint32_t num_iterations, invalid_pos;
	char name[RTE_HASH_NAMESIZE];
	char hashname[RTE_HASH_NAMESIZE];

	rte_snprintf(name, 32, "test%u", calledCount++);
	hash_params.name = name;

	handle = rte_hash_create(&hash_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	switch (params->test_type){
	case ADD_ON_EMPTY:
		ticks = run_single_tbl_perf_test(handle, rte_hash_add_key,
				params, &avg_occupancy, &invalid_pos);
		break;
	case DELETE_ON_EMPTY:
		ticks = run_single_tbl_perf_test(handle, rte_hash_del_key,
				params, &avg_occupancy, &invalid_pos);
		break;
	case LOOKUP_ON_EMPTY:
		ticks = run_single_tbl_perf_test(handle, rte_hash_lookup,
				params, &avg_occupancy, &invalid_pos);
		break;
	case ADD_UPDATE:
		num_iterations = params->num_iterations;
		params->num_iterations = params->entries;
		run_single_tbl_perf_test(handle, rte_hash_add_key, params,
				&avg_occupancy, &invalid_pos);
		params->num_iterations = num_iterations;
		ticks = run_single_tbl_perf_test(handle, rte_hash_add_key,
				params, &avg_occupancy, &invalid_pos);
		break;
	case DELETE:
		num_iterations = params->num_iterations;
		params->num_iterations = params->entries;
		run_single_tbl_perf_test(handle, rte_hash_add_key, params,
				&avg_occupancy, &invalid_pos);

		params->num_iterations = num_iterations;
		ticks = run_single_tbl_perf_test(handle, rte_hash_del_key,
				params, &avg_occupancy, &invalid_pos);
		break;
	case LOOKUP:
		num_iterations = params->num_iterations;
		params->num_iterations = params->entries;
		run_single_tbl_perf_test(handle, rte_hash_add_key, params,
				&avg_occupancy, &invalid_pos);

		params->num_iterations = num_iterations;
		ticks = run_single_tbl_perf_test(handle, rte_hash_lookup,
				params, &avg_occupancy, &invalid_pos);
		break;
	default: return -1;
	}

	rte_snprintf(hashname, RTE_HASH_NAMESIZE, "%s", get_hash_name(params->hash_func));

	printf("%-12s, %-15s, %-16u, %-7u, %-18u, %-8u, %-19.2f, %.2f\n",
		hashname,
		get_tbl_perf_test_desc(params->test_type),
		(unsigned) params->key_len,
		(unsigned) params->entries,
		(unsigned) params->bucket_entries,
		(unsigned) invalid_pos,
		avg_occupancy,
		ticks
	);

	/* Free */
	rte_hash_free(handle);
	return 0;
}

/*
 * Run all hash table performance tests.
 */
static int run_all_tbl_perf_tests(void)
{
	unsigned i;

	printf(" *** Hash table performance test results ***\n");
	printf("Hash Func.  , Operation      , Key size (bytes), Entries, "
	       "Entries per bucket, Errors  , Avg. bucket entries, Ticks/Op.\n");

	/* Loop through every combination of test parameters */
	for (i = 0;
	     i < sizeof(tbl_perf_params) / sizeof(struct tbl_perf_test_params);
	     i++) {

		/* Perform test */
		if (run_tbl_perf_test(&tbl_perf_params[i]) < 0)
			return -1;
	}
	return 0;
}

/*
 * Test a hash function.
 */
static void run_hash_func_test(rte_hash_function f, uint32_t init_val,
		uint32_t key_len)
{
	static uint8_t key[RTE_HASH_KEY_LENGTH_MAX];
	uint64_t ticks = 0, start, end;
	unsigned i, j;

	for (i = 0; i < HASHTEST_ITERATIONS; i++) {

		for (j = 0; j < key_len; j++)
			key[j] = (uint8_t) rte_rand();

		start = rte_rdtsc();
		f(key, key_len, init_val);
		end = rte_rdtsc();
		ticks += end - start;
	}

	printf("%-12s, %-18u, %-13u, %.02f\n", get_hash_name(f), (unsigned) key_len,
			(unsigned) init_val, (double)ticks / HASHTEST_ITERATIONS);
}

/*
 * Test all hash functions.
 */
static void run_hash_func_tests(void)
{
	unsigned i, j, k;

	printf("\n\n *** Hash function performance test results ***\n");
	printf(" Number of iterations for each test = %d\n",
			HASHTEST_ITERATIONS);
	printf("Hash Func.  , Key Length (bytes), Initial value, Ticks/Op.\n");

	for (i = 0;
	     i < sizeof(hashtest_funcs) / sizeof(rte_hash_function);
	     i++) {
		for (j = 0;
		     j < sizeof(hashtest_initvals) / sizeof(uint32_t);
		     j++) {
			for (k = 0;
			     k < sizeof(hashtest_key_lens) / sizeof(uint32_t);
			     k++) {
				run_hash_func_test(hashtest_funcs[i],
						hashtest_initvals[j],
						hashtest_key_lens[k]);
			}
		}
	}
}

/******************************************************************************/
static int
fbk_hash_unit_test(void)
{
	struct rte_fbk_hash_params params = {
		.name = "fbk_hash_test",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_1 = {
		.name = "invalid_1",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX + 1, /* Not power of 2 */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_2 = {
		.name = "invalid_4",
		.entries = 4,
		.entries_per_bucket = 3,         /* Not power of 2 */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_3 = {
		.name = "invalid_2",
		.entries = 0,                    /* Entries is 0 */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_4 = {
		.name = "invalid_3",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 0,         /* Entries per bucket is 0 */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_5 = {
		.name = "invalid_4",
		.entries = 4,
		.entries_per_bucket = 8,         /* Entries per bucket > entries */
		.socket_id = 0,
	};

	struct rte_fbk_hash_params invalid_params_6 = {
		.name = "invalid_5",
		.entries = RTE_FBK_HASH_ENTRIES_MAX * 2,   /* Entries > max allowed */
		.entries_per_bucket = 4,
		.socket_id = 0,
	};

	struct rte_fbk_hash_params params_jhash = {
		.name = "valid",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
		.hash_func = rte_jhash_1word,              /* Tests for different hash_func */
		.init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT,
	};

	struct rte_fbk_hash_params params_nohash = {
		.name = "valid nohash",
		.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
		.entries_per_bucket = 4,
		.socket_id = 0,
		.hash_func = 0,                            /* Tests for null hash_func */
		.init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT,
	};

	struct rte_fbk_hash_table *handle;
	uint32_t keys[5] =
		{0xc6e18639, 0xe67c201c, 0xd4c8cffd, 0x44728691, 0xd5430fa9};
	uint16_t vals[5] = {28108, 5699, 38490, 2166, 61571};
	int status;
	unsigned i;
	double used_entries;

	/* Try creating hashes with invalid parameters */
	handle = rte_fbk_hash_create(&invalid_params_1);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_2);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_3);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_4);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_5);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	handle = rte_fbk_hash_create(&invalid_params_6);
	RETURN_IF_ERROR_FBK(handle != NULL, "fbk hash creation should have failed");

	/* Create empty jhash hash. */
	handle = rte_fbk_hash_create(&params_jhash);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk jhash hash creation failed");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Create empty jhash hash. */
	handle = rte_fbk_hash_create(&params_nohash);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk nohash hash creation failed");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Create empty hash. */
	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != 0, \
				"load factor right after creation is not zero but it should be");
	/* Add keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash add failed");
	}

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != (unsigned)((((double)5)/LOCAL_FBK_HASH_ENTRIES_MAX)*LOCAL_FBK_HASH_ENTRIES_MAX), \
				"load factor now is not as expected");
	/* Find value of added keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[i],
				"fbk hash lookup failed");
	}

	/* Change value of added keys. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[4 - i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash update failed");
	}

	/* Find new values. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[4-i],
				"fbk hash lookup failed");
	}

	/* Delete keys individually. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_delete_key(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash delete failed");
	}

	used_entries = rte_fbk_hash_get_load_factor(handle) * LOCAL_FBK_HASH_ENTRIES_MAX;
	RETURN_IF_ERROR_FBK((unsigned)used_entries != 0, \
				"load factor right after deletion is not zero but it should be");
	/* Lookup should now fail. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status == 0,
				"fbk hash lookup should have failed");
	}

	/* Add keys again. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_add_key(handle, keys[i], vals[i]);
		RETURN_IF_ERROR_FBK(status != 0, "fbk hash add failed");
	}

	/* Make sure they were added. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status != vals[i],
				"fbk hash lookup failed");
	}

	/* Clear all entries. */
	rte_fbk_hash_clear_all(handle);

	/* Lookup should fail. */
	for (i = 0; i < 5; i++) {
		status = rte_fbk_hash_lookup(handle, keys[i]);
		RETURN_IF_ERROR_FBK(status == 0,
				"fbk hash lookup should have failed");
	}

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	/* Cover the NULL case. */
	rte_fbk_hash_free(0);

	return 0;
}

/* Control operation of performance testing of fbk hash. */
#define LOAD_FACTOR 0.667	/* How full to make the hash table. */
#define TEST_SIZE 1000000	/* How many operations to time. */
#define TEST_ITERATIONS 30	/* How many measurements to take. */
#define ENTRIES (1 << 15)	/* How many entries. */

static int
fbk_hash_perf_test(void)
{
	struct rte_fbk_hash_params params = {
		.name = "fbk_hash_test",
		.entries = ENTRIES,
		.entries_per_bucket = 4,
		.socket_id = 0,
	};
	struct rte_fbk_hash_table *handle;
	uint32_t keys[ENTRIES] = {0};
	unsigned indexes[TEST_SIZE];
	uint64_t lookup_time = 0;
	unsigned added = 0;
	unsigned value = 0;
	unsigned i, j;

	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	/* Generate random keys and values. */
	for (i = 0; i < ENTRIES; i++) {
		uint32_t key = (uint32_t)rte_rand();
		key = ((uint64_t)key << 32) | (uint64_t)rte_rand();
		uint16_t val = (uint16_t)rte_rand();

		if (rte_fbk_hash_add_key(handle, key, val) == 0) {
			keys[added] = key;
			added++;
		}
		if (added > (LOAD_FACTOR * ENTRIES)) {
			break;
		}
	}

	for (i = 0; i < TEST_ITERATIONS; i++) {
		uint64_t begin;
		uint64_t end;

		/* Generate random indexes into keys[] array. */
		for (j = 0; j < TEST_SIZE; j++) {
			indexes[j] = rte_rand() % added;
		}

		begin = rte_rdtsc();
		/* Do lookups */
		for (j = 0; j < TEST_SIZE; j++) {
			value += rte_fbk_hash_lookup(handle, keys[indexes[j]]);
		}
		end = rte_rdtsc();
		lookup_time += (double)(end - begin);
	}

	printf("\n\n *** FBK Hash function performance test results ***\n");
	/*
	 * The use of the 'value' variable ensures that the hash lookup is not
	 * being optimised out by the compiler.
	 */
	if (value != 0)
		printf("Number of ticks per lookup = %g\n",
			(double)lookup_time /
			((double)TEST_ITERATIONS * (double)TEST_SIZE));

	rte_fbk_hash_free(handle);

	return 0;
}

/*
 * Sequence of operations for find existing fbk hash table
 *
 *  - create table
 *  - find existing table: hit
 *  - find non-existing table: miss
 *
 */
static int test_fbk_hash_find_existing(void)
{
	struct rte_fbk_hash_params params = {
			.name = "fbk_hash_find_existing",
			.entries = LOCAL_FBK_HASH_ENTRIES_MAX,
			.entries_per_bucket = 4,
			.socket_id = 0,
	};
	struct rte_fbk_hash_table *handle = NULL, *result = NULL;

	/* Create hash table. */
	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	/* Try to find existing fbk hash table */
	result = rte_fbk_hash_find_existing("fbk_hash_find_existing");
	RETURN_IF_ERROR_FBK(result != handle, "could not find existing fbk hash table");

	/* Try to find non-existing fbk hash table */
	result = rte_fbk_hash_find_existing("fbk_hash_find_non_existing");
	RETURN_IF_ERROR_FBK(!(result == NULL), "found fbk table that shouldn't exist");

	/* Cleanup. */
	rte_fbk_hash_free(handle);

	return 0;
}

/*
 * Do tests for hash creation with bad parameters.
 */
static int test_hash_creation_with_bad_parameters(void)
{
	struct rte_hash *handle;
	struct rte_hash_parameters params;

	handle = rte_hash_create(NULL);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully without any parameter\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_0";
	params.entries = RTE_HASH_ENTRIES_MAX + 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully with entries in parameter exceeded\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_1";
	params.bucket_entries = RTE_HASH_BUCKET_ENTRIES_MAX + 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully with bucket_entries in parameter exceeded\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_2";
	params.entries = params.bucket_entries - 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully if entries less than bucket_entries in parameter\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_3";
	params.entries = params.entries - 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully if entries in parameter is not power of 2\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_4";
	params.bucket_entries = params.bucket_entries - 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully if bucket_entries in parameter is not power of 2\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_5";
	params.key_len = 0;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully if key_len in parameter is zero\n");
		return -1;
	}

	memcpy(&params, &ut_params, sizeof(params));
	params.name = "creation_with_bad_parameters_6";
	params.key_len = RTE_HASH_KEY_LENGTH_MAX + 1;
	handle = rte_hash_create(&params);
	if (handle != NULL) {
		rte_hash_free(handle);
		printf("Impossible creating hash sucessfully if key_len is greater than the maximun\n");
		return -1;
	}

	return 0;
}

static uint8_t key[16] = {0x00, 0x01, 0x02, 0x03,
			0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b,
			0x0c, 0x0d, 0x0e, 0x0f};
static struct rte_hash_parameters hash_params_ex = {
	.name = NULL,
	.entries = 64,
	.bucket_entries = 4,
	.key_len = 0,
	.hash_func = NULL,
	.hash_func_init_val = 0,
	.socket_id = 0,
};

/*
 * add/delete key with jhash2
 */
static int
test_hash_add_delete_jhash2(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash2";
	hash_params_ex.key_len = 4;
	hash_params_ex.hash_func = (rte_hash_function)rte_jhash2;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL) {
		printf("test_hash_add_delete_jhash2 fail to create hash\n");
		goto fail_jhash2;
	}
	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0) {
		printf("test_hash_add_delete_jhash2 fail to add hash key\n");
		goto fail_jhash2;
	}

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2) {
		printf("test_hash_add_delete_jhash2 delete different key from being added\n");
		goto fail_jhash2;
	}
	ret = 0;

fail_jhash2:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete (2) key with jhash2
 */
static int
test_hash_add_delete_2_jhash2(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_2_jhash2";
	hash_params_ex.key_len = 8;
	hash_params_ex.hash_func = (rte_hash_function)rte_jhash2;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_2_jhash2;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_2_jhash2;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_2_jhash2;

	ret = 0;

fail_2_jhash2:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

static uint32_t
test_hash_jhash_1word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	length =length;

	return rte_jhash_1word(k[0], initval);
}

static uint32_t
test_hash_jhash_2word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	length =length;

	return rte_jhash_2words(k[0], k[1], initval);
}

static uint32_t
test_hash_jhash_3word(const void *key, uint32_t length, uint32_t initval)
{
	const uint32_t *k = key;

	length =length;

	return rte_jhash_3words(k[0], k[1], k[2], initval);
}

/*
 * add/delete key with jhash 1word
 */
static int
test_hash_add_delete_jhash_1word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_1word";
	hash_params_ex.key_len = 4;
	hash_params_ex.hash_func = test_hash_jhash_1word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_1word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_1word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_1word;

	ret = 0;

fail_jhash_1word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete key with jhash 2word
 */
static int
test_hash_add_delete_jhash_2word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_2word";
	hash_params_ex.key_len = 8;
	hash_params_ex.hash_func = test_hash_jhash_2word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_2word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_2word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_2word;

	ret = 0;

fail_jhash_2word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * add/delete key with jhash 3word
 */
static int
test_hash_add_delete_jhash_3word(void)
{
	int ret = -1;
	struct rte_hash *handle;
	int32_t pos1, pos2;

	hash_params_ex.name = "hash_test_jhash_3word";
	hash_params_ex.key_len = 12;
	hash_params_ex.hash_func = test_hash_jhash_3word;

	handle = rte_hash_create(&hash_params_ex);
	if (handle == NULL)
		goto fail_jhash_3word;

	pos1 = rte_hash_add_key(handle, (void *)&key[0]);
	if (pos1 < 0)
		goto fail_jhash_3word;

	pos2 = rte_hash_del_key(handle, (void *)&key[0]);
	if (pos2 < 0 || pos1 != pos2)
		goto fail_jhash_3word;

	ret = 0;

fail_jhash_3word:
	if (handle != NULL)
		rte_hash_free(handle);

	return ret;
}

/*
 * Do all unit and performance tests.
 */
int test_hash(void)
{
	if (test_add_delete() < 0)
		return -1;
	if (test_hash_add_delete_jhash2() < 0)
		return -1;
	if (test_hash_add_delete_2_jhash2() < 0)
		return -1;
	if (test_hash_add_delete_jhash_1word() < 0)
		return -1;
	if (test_hash_add_delete_jhash_2word() < 0)
		return -1;
	if (test_hash_add_delete_jhash_3word() < 0)
		return -1;
	if (test_hash_find_existing() < 0)
		return -1;
	if (test_add_update_delete() < 0)
		return -1;
	if (test_five_keys() < 0)
		return -1;
	if (test_full_bucket() < 0)
		return -1;
	if (run_all_tbl_perf_tests() < 0)
		return -1;
	run_hash_func_tests();

	if (test_fbk_hash_find_existing() < 0)
		return -1;
	if (fbk_hash_unit_test() < 0)
		return -1;
	if (fbk_hash_perf_test() < 0)
		return -1;
	if (test_hash_creation_with_bad_parameters() < 0)
		return -1;
	return 0;
}
#else

int
test_hash(void)
{
	printf("The Hash library is not included in this build\n");
	return 0;
}

#endif
