/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
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

#ifndef _RTE_KVARGS_H_
#define _RTE_KVARGS_H_

/**
 * @file
 * RTE Argument parsing
 *
 * This module can be used to parse arguments whose format is
 * key1=value1,key2=value2,key3=value3,...
 *
 * The same key can appear several times with the same or a different
 * value. Indeed, the arguments are stored as a list of key/values
 * associations and not as a dictionary.
 *
 * This file provides some helpers that are especially used by virtual
 * ethernet devices at initialization for arguments parsing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of key/value associations */
#define RTE_KVARGS_MAX 32

/** separator character used between each pair */
#define RTE_KVARGS_PAIRS_DELIM	","

/** separator character used between key and value */
#define RTE_KVARGS_KV_DELIM	"="

/** Type of callback function used by rte_kvargs_process() */
typedef int (*arg_handler_t)(const char *key, const char *value, void *opaque);

/** A key/value association */
struct rte_kvargs_pair {
	char *key;      /**< the name (key) of the association  */
	char *value;    /**< the value associated to that key */
};

/** Store a list of key/value associations */
struct rte_kvargs {
	char *str;      /**< copy of the argument string */
	unsigned count; /**< number of entries in the list */
	struct rte_kvargs_pair pairs[RTE_KVARGS_MAX]; /**< list of key/values */
};

/**
 * Allocate a rte_kvargs and store key/value associations from a string
 *
 * The function allocates and fills a rte_kvargs structure from a given
 * string whose format is key1=value1,key2=value2,...
 *
 * The structure can be freed with rte_kvargs_free().
 *
 * @param args
 *   The input string containing the key/value associations
 * @param valid_keys
 *   A list of valid keys (table of const char *, the last must be NULL).
 *   This argument is ignored if NULL
 *
 * @return
 *   - A pointer to an allocated rte_kvargs structure on success
 *   - NULL on error
 */
struct rte_kvargs *rte_kvargs_parse(const char *args,
		const char *const valid_keys[]);

/**
 * Free a rte_kvargs structure
 *
 * Free a rte_kvargs structure previously allocated with
 * rte_kvargs_parse().
 *
 * @param kvlist
 *   The rte_kvargs structure
 */
void rte_kvargs_free(struct rte_kvargs *kvlist);

/**
 * Call a handler function for each key/value matching the key
 *
 * For each key/value association that matches the given key, calls the
 * handler function with the for a given arg_name passing the value on the
 * dictionary for that key and a given extra argument. If *kvlist* is NULL
 * function does nothing.
 *
 * @param kvlist
 *   The rte_kvargs structure
 * @param key_match
 *   The key on which the handler should be called, or NULL to process handler
 *   on all associations
 * @param handler
 *   The function to call for each matching key
 * @param opaque_arg
 *   A pointer passed unchanged to the handler
 *
 * @return
 *   - 0 on success
 *   - Negative on error
 */
int rte_kvargs_process(const struct rte_kvargs *kvlist,
	const char *key_match, arg_handler_t handler, void *opaque_arg);

/**
 * Count the number of associations matching the given key
 *
 * @param kvlist
 *   The rte_kvargs structure
 * @param key_match
 *   The key that should match, or NULL to count all associations

 * @return
 *   The number of entries
 */
unsigned rte_kvargs_count(const struct rte_kvargs *kvlist,
	const char *key_match);

#ifdef __cplusplus
}
#endif

#endif
