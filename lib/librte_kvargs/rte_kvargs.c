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
#include <string.h>
#include <sys/user.h>
#include <linux/binfmts.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include "rte_kvargs.h"

/*
 * Initialize a rte_kvargs structure to an empty key/value list.
 */
int
rte_kvargs_init(struct rte_kvargs *kvlist)
{
	kvlist->count = 0;
	memset(kvlist->pairs, 0, sizeof(kvlist->pairs));
	return 0;
}

/*
 * Add a key-value pair at the end of a given key/value list.
 * Return an error if the list is full or if the key is duplicated.
 */
static int
rte_kvargs_add_pair(struct rte_kvargs *kvlist, char *key, char *val)
{
	unsigned i;
	struct rte_kvargs_pair* entry;

	/* is the list full? */
	if (kvlist->count >= RTE_KVARGS_MAX) {
		RTE_LOG(ERR, PMD, "Couldn't add %s, key/value list is full\n", key);
		return -1;
	}

	/* Check if the key is duplicated */
	for (i = 0; i < kvlist->count; i++) {
		entry = &kvlist->pairs[i];
		if (strcmp(entry->key, key) == 0) {
			RTE_LOG(ERR, PMD, "Couldn't add %s, duplicated key\n", key);
			return -1;
		}
	}

	entry = &kvlist->pairs[kvlist->count];
	entry->key = key;
	entry->value = val;
	kvlist->count++;
	return 0;
}

/*
 * Receive a string with a list of arguments following the pattern
 * key=value;key=value;... and insert them into the list.
 * strtok() is used so the params string will be copied to be modified.
 */
static int
rte_kvargs_tokenize(struct rte_kvargs *kvlist, const char *params)
{
	unsigned i, count;
	char *args;
	char *pairs[RTE_KVARGS_MAX];
	char *pair[2];

	/* If params are empty, nothing to do */
	if (params == NULL || params[0] == 0) {
		RTE_LOG(ERR, PMD, "Cannot parse empty arguments\n");
		return -1;
	}

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	args = strdup(params);
	if(args == NULL){
		RTE_LOG(ERR, PMD, "Cannot parse arguments: not enough memory\n");
		return -1;
	}

	count = rte_strsplit(args, strnlen(args, MAX_ARG_STRLEN), pairs,
			RTE_KVARGS_MAX, RTE_KVARGS_PAIRS_DELIM);

	for (i = 0; i < count; i++) {
		pair[0] = NULL;
		pair[1] = NULL;

		rte_strsplit(pairs[i], strnlen(pairs[i], MAX_ARG_STRLEN), pair, 2,
				RTE_KVARGS_KV_DELIM);

		if (pair[0] == NULL || pair[1] == NULL || pair[0][0] == 0
				|| pair[1][0] == 0) {
			RTE_LOG(ERR, PMD,
				"Cannot parse arguments: wrong key or value\n"
				"params=<%s>\n", params);
			goto error;
		}

		if (rte_kvargs_add_pair(kvlist, pair[0], pair[1]) < 0)
			goto error;
	}
	return 0;

error:
	rte_free(args);
	return -1;
}

/*
 * Determine whether a key is valid or not by looking
 * into a list of valid keys.
 */
static int
is_valid_key(const char *valid[], const char *key_match)
{
	const char **valid_ptr;

	for (valid_ptr = valid; *valid_ptr != NULL; valid_ptr++)
		if (strstr(key_match, *valid_ptr) != NULL)
			return 1;
	return 0;
}

/*
 * Determine whether all keys are valid or not by looking
 * into a list of valid keys.
 */
static int
check_for_valid_keys(struct rte_kvargs *kvlist,
		const char *valid[])
{
	unsigned i, ret;
	struct rte_kvargs_pair *pair;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		ret = is_valid_key(valid, pair->key);
		if (!ret) {
			RTE_LOG(ERR, PMD,
				"Error parsing device, invalid key <%s>\n",
				pair->key);
			return -1;
		}
	}
	return 0;
}

/*
 * Return the number of times a given arg_name exists in the key/value list.
 * E.g. given a list = { rx = 0, rx = 1, tx = 2 } the number of args for
 * arg "rx" will be 2.
 */
unsigned
rte_kvargs_count(const struct rte_kvargs *kvlist, const char *key_match)
{
	const struct rte_kvargs_pair *pair;
	unsigned i, ret;

	ret = 0;
	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (strcmp(pair->key, key_match) == 0)
			ret++;
	}

	return ret;
}

/*
 * For each matching key, call the given handler function.
 */
int
rte_kvargs_process(const struct rte_kvargs *kvlist,
		const char *key_match,
		arg_handler_t handler,
		void *opaque_arg)
{
	const struct rte_kvargs_pair *pair;
	unsigned i;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (strstr(pair->key, key_match) != NULL) {
			if ((*handler)(pair->value, opaque_arg) < 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Parse the arguments "key=value;key=value;..." string and return
 * an allocated structure that contains a key/value list. Also
 * check if only valid keys were used.
 */
int
rte_kvargs_parse(struct rte_kvargs *kvlist,
		const char *args,
		const char *valid_keys[])
{

	int ret;

	ret = rte_kvargs_tokenize(kvlist, args);
	if (ret < 0)
		return ret;

	if (valid_keys == NULL)
		return 0;

	return check_for_valid_keys(kvlist, valid_keys);
}
