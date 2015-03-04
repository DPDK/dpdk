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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_ip.h>
#include <rte_string_fns.h>

#include "test.h"

#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

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
 * Hash function performance test configuration section. Each performance test
 * will be performed HASHTEST_ITERATIONS times.
 *
 * The five arrays below control what tests are performed. Every combination
 * from the array entries is tested.
 */
#define HASHTEST_ITERATIONS 1000000

static rte_hash_function hashtest_funcs[] = {rte_jhash, rte_hash_crc};
static uint32_t hashtest_initvals[] = {0};
static uint32_t hashtest_key_lens[] = {2, 4, 5, 6, 7, 8, 10, 11, 15, 16, 21, 31, 32, 33, 63, 64};
/******************************************************************************/

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
		if (keys) rte_free(keys);				\
		return -1;						\
	}								\
} while(0)

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
	key = rte_zmalloc("hash key",
			  params->key_len * sizeof(uint8_t), 16);
	if (key == NULL)
		return -1;

	bucket_occupancies = rte_calloc("bucket occupancies",
					num_buckets, sizeof(uint32_t), 16);
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
		.socket_id = rte_socket_id(),
	};
	struct rte_hash *handle;
	double avg_occupancy = 0, ticks = 0;
	uint32_t num_iterations, invalid_pos;
	char name[RTE_HASH_NAMESIZE];
	char hashname[RTE_HASH_NAMESIZE];

	snprintf(name, 32, "test%u", calledCount++);
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

	snprintf(hashname, RTE_HASH_NAMESIZE, "%s", get_hash_name(params->hash_func));

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
		.socket_id = rte_socket_id(),
	};
	struct rte_fbk_hash_table *handle = NULL;
	uint32_t *keys = NULL;
	unsigned indexes[TEST_SIZE];
	uint64_t lookup_time = 0;
	unsigned added = 0;
	unsigned value = 0;
	unsigned i, j;

	handle = rte_fbk_hash_create(&params);
	RETURN_IF_ERROR_FBK(handle == NULL, "fbk hash creation failed");

	keys = rte_zmalloc(NULL, ENTRIES * sizeof(*keys), 0);
	RETURN_IF_ERROR_FBK(keys == NULL,
		"fbk hash: memory allocation for key store failed");

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
 * Do all unit and performance tests.
 */
static int
test_hash_perf(void)
{
	if (run_all_tbl_perf_tests() < 0)
		return -1;
	run_hash_func_tests();

	if (fbk_hash_perf_test() < 0)
		return -1;
	return 0;
}

static struct test_command hash_perf_cmd = {
	.command = "hash_perf_autotest",
	.callback = test_hash_perf,
};
REGISTER_TEST_COMMAND(hash_perf_cmd);
