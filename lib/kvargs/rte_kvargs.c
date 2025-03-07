/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2013 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <eal_export.h>
#include <rte_os_shim.h>

#include "rte_kvargs.h"

/*
 * Receive a string with a list of arguments following the pattern
 * key=value,key=value,... and insert them into the list.
 * Params string will be copied to be modified.
 * list "[]" and list element splitter ",", "-" is treated as value.
 * Supported examples:
 *   k1=v1,k2=v2
 *   k1
 *   k1=x[0-1]y[1,3-5,9]z
 */
static int
rte_kvargs_tokenize(struct rte_kvargs *kvlist, const char *params)
{
	char *str, *start;
	bool in_list = false, end_key = false, end_value = false;
	bool save = false, end_pair = false;

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	kvlist->str = strdup(params);
	if (kvlist->str == NULL)
		return -1;

	/* browse each key/value pair and add it in kvlist */
	str = kvlist->str;
	start = str; /* start of current key or value */
	while (1) {
		switch (*str) {
		case '=': /* End of key. */
			end_key = true;
			save = true;
			break;
		case ',':
			/* End of value, skip comma in middle of range */
			if (!in_list) {
				if (end_key)
					end_value = true;
				else
					end_key = true;
				save = true;
				end_pair = true;
			}
			break;
		case '[': /* Start of list. */
			in_list = true;
			break;
		case ']': /* End of list.  */
			if (in_list)
				in_list = false;
			break;
		case '\0': /* End of string */
			if (end_key)
				end_value = true;
			else
				end_key = true;
			save = true;
			end_pair = true;
			break;
		default:
			break;
		}

		if (!save) {
			/* Continue if not end of key or value. */
			str++;
			continue;
		}

		if (kvlist->count >= RTE_KVARGS_MAX)
			return -1;

		if (end_value)
			/* Value parsed */
			kvlist->pairs[kvlist->count].value = start;
		else if (end_key)
			/* Key parsed. */
			kvlist->pairs[kvlist->count].key = start;

		if (end_pair) {
			if (end_value || str != start)
				/* Ignore empty pair. */
				kvlist->count++;
			end_key = false;
			end_value = false;
			end_pair = false;
		}

		if (*str == '\0') /* End of string. */
			break;
		*str = '\0';
		str++;
		start = str;
		save = false;
	}

	return 0;
}

/*
 * Determine whether a key is valid or not by looking
 * into a list of valid keys.
 */
static int
is_valid_key(const char * const valid[], const char *key_match)
{
	const char * const *valid_ptr;

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
		const char * const valid[])
{
	unsigned i, ret;
	struct rte_kvargs_pair *pair;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		ret = is_valid_key(valid, pair->key);
		if (!ret)
			return -1;
	}
	return 0;
}

/*
 * Return the number of times a given arg_name exists in the key/value list.
 * E.g. given a list = { rx = 0, rx = 1, tx = 2 } the number of args for
 * arg "rx" will be 2.
 */
RTE_EXPORT_SYMBOL(rte_kvargs_count)
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

static int
kvargs_process_common(const struct rte_kvargs *kvlist, const char *key_match,
		      arg_handler_t handler, void *opaque_arg, bool support_only_key)
{
	const struct rte_kvargs_pair *pair;
	unsigned i;

	if (kvlist == NULL)
		return -1;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (key_match == NULL || strcmp(pair->key, key_match) == 0) {
			if (!support_only_key && pair->value == NULL)
				return -1;
			if ((*handler)(pair->key, pair->value, opaque_arg) < 0)
				return -1;
		}
	}

	return 0;
}

/*
 * For each matching key in key=value, call the given handler function.
 */
RTE_EXPORT_SYMBOL(rte_kvargs_process)
int
rte_kvargs_process(const struct rte_kvargs *kvlist, const char *key_match, arg_handler_t handler,
		   void *opaque_arg)
{
	return kvargs_process_common(kvlist, key_match, handler, opaque_arg, false);
}

/*
 * For each matching key in key=value or only-key, call the given handler function.
 */
RTE_EXPORT_SYMBOL(rte_kvargs_process_opt)
int
rte_kvargs_process_opt(const struct rte_kvargs *kvlist, const char *key_match,
		       arg_handler_t handler, void *opaque_arg)
{
	return kvargs_process_common(kvlist, key_match, handler, opaque_arg, true);
}

/* free the rte_kvargs structure */
RTE_EXPORT_SYMBOL(rte_kvargs_free)
void
rte_kvargs_free(struct rte_kvargs *kvlist)
{
	if (!kvlist)
		return;

	free(kvlist->str);
	free(kvlist);
}

/* Lookup a value in an rte_kvargs list by its key and value. */
RTE_EXPORT_SYMBOL(rte_kvargs_get_with_value)
const char *
rte_kvargs_get_with_value(const struct rte_kvargs *kvlist, const char *key,
			  const char *value)
{
	unsigned int i;

	if (kvlist == NULL)
		return NULL;
	for (i = 0; i < kvlist->count; ++i) {
		if (key != NULL && strcmp(kvlist->pairs[i].key, key) != 0)
			continue;
		if (value != NULL && strcmp(kvlist->pairs[i].value, value) != 0)
			continue;
		return kvlist->pairs[i].value;
	}
	return NULL;
}

/* Lookup a value in an rte_kvargs list by its key. */
RTE_EXPORT_SYMBOL(rte_kvargs_get)
const char *
rte_kvargs_get(const struct rte_kvargs *kvlist, const char *key)
{
	if (kvlist == NULL || key == NULL)
		return NULL;
	return rte_kvargs_get_with_value(kvlist, key, NULL);
}

/*
 * Parse the arguments "key=value,key=value,..." string and return
 * an allocated structure that contains a key/value list. Also
 * check if only valid keys were used.
 */
RTE_EXPORT_SYMBOL(rte_kvargs_parse)
struct rte_kvargs *
rte_kvargs_parse(const char *args, const char * const valid_keys[])
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

RTE_EXPORT_SYMBOL(rte_kvargs_parse_delim)
struct rte_kvargs *
rte_kvargs_parse_delim(const char *args, const char * const valid_keys[],
		       const char *valid_ends)
{
	struct rte_kvargs *kvlist = NULL;
	char *copy;
	size_t len;

	if (valid_ends == NULL)
		return rte_kvargs_parse(args, valid_keys);

	copy = strdup(args);
	if (copy == NULL)
		return NULL;

	len = strcspn(copy, valid_ends);
	copy[len] = '\0';

	kvlist = rte_kvargs_parse(copy, valid_keys);

	free(copy);
	return kvlist;
}
