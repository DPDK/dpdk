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
#include <stdlib.h>

#include <rte_log.h>
#include <rte_string_fns.h>

#include "rte_kvargs.h"

/*
 * Receive a string with a list of arguments following the pattern
 * key=value;key=value;... and insert them into the list.
 * strtok() is used so the params string will be copied to be modified.
 */
static int
rte_kvargs_tokenize(struct rte_kvargs *kvlist, const char *params)
{
	unsigned i;
	char *str;
	char *ctx1 = NULL;
	char *ctx2 = NULL;

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	kvlist->str = strdup(params);
	if (kvlist->str == NULL) {
		RTE_LOG(ERR, PMD, "Cannot parse arguments: not enough memory\n");
		return -1;
	}

	/* browse each key/value pair and add it in kvlist */
	str = kvlist->str;
	while ((str = strtok_r(str, RTE_KVARGS_PAIRS_DELIM, &ctx1)) != NULL) {

		i = kvlist->count;
		if (i >= RTE_KVARGS_MAX) {
			RTE_LOG(ERR, PMD, "Cannot parse arguments: list full\n");
			return -1;
		}

		kvlist->pairs[i].key = strtok_r(str, RTE_KVARGS_KV_DELIM, &ctx2);
		kvlist->pairs[i].value = strtok_r(NULL, RTE_KVARGS_KV_DELIM, &ctx2);
		if (kvlist->pairs[i].key == NULL || kvlist->pairs[i].value == NULL) {
			RTE_LOG(ERR, PMD,
				"Cannot parse arguments: wrong key or value\n"
				"params=<%s>\n", params);
			return -1;
		}

		kvlist->count++;
		str = NULL;
	}

	return 0;
}

/*
 * Determine whether a key is valid or not by looking
 * into a list of valid keys.
 */
static int
is_valid_key(const char *valid[], const char *key_match)
{
	const char **valid_ptr;

	for (valid_ptr = valid; *valid_ptr != NULL; valid_ptr++) {
		if (strcmp(key_match, *valid_ptr) == 0)
			return 1;
	}
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
		if (key_match == NULL || strcmp(pair->key, key_match) == 0)
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
		if (key_match == NULL || strcmp(pair->key, key_match) == 0) {
			if ((*handler)(pair->key, pair->value, opaque_arg) < 0)
				return -1;
		}
	}
	return 0;
}

/* free the rte_kvargs structure */
void
rte_kvargs_free(struct rte_kvargs *kvlist)
{
	if (!kvlist)
		return;

	if (kvlist->str != NULL)
		free(kvlist->str);

	free(kvlist);
}

/*
 * Parse the arguments "key=value;key=value;..." string and return
 * an allocated structure that contains a key/value list. Also
 * check if only valid keys were used.
 */
struct rte_kvargs *
rte_kvargs_parse(const char *args, const char *valid_keys[])
{
	struct rte_kvargs *kvlist;

	kvlist = malloc(sizeof(*kvlist));
	if (kvlist == NULL)
		return NULL;
	memset(kvlist, 0, sizeof(*kvlist));

	if (rte_kvargs_tokenize(kvlist, args) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	if (valid_keys != NULL && check_for_valid_keys(kvlist, valid_keys) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	return kvlist;
}
