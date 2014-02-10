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
#include <string.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include "rte_eth_pcap_arg_parser.h"

/*
 * Initializes a non NULL dictionary reference to be used later on.
 */
inline int
rte_eth_pcap_init_args_dict(struct args_dict *dict)
{
	dict->index = 0;
	dict->size = RTE_ETH_PCAP_ARG_PARSER_MAX_ARGS;
	memset(dict->pairs, 0, dict->size);
	return 0;
}

/*
 * Adds a key-value pair to a given non-NULL dictionary reference.
 * The final key will be the name+key.
 * Returns error in case the dictionary is full or if the key is duplicated.
 */
inline int
rte_eth_pcap_add_pair_to_dict(struct args_dict *dict,
		char *key,
		char *val)
{
	unsigned i;
	struct key_value* entry;

	/* is the dictionary full? */
	if (dict->index >= dict->size) {
		RTE_LOG(ERR, PMD, "Couldn't add %s, dictionary is full\n", key);
		return -1;
	}

	/* Check if the key is duplicated */
	for (i = 0; i < dict->index; i++) {
		entry = &dict->pairs[i];
		if (strcmp(entry->key, key) == 0) {
			RTE_LOG(ERR, PMD, "Couldn't add %s, duplicated key\n", key);
			return -1;
		}
	}

	entry = &dict->pairs[dict->index];
	entry->key = key;
	entry->value = val;
	dict->index++;
	return 0;

}

#define RTE_ETH_PCAP_PAIRS_DELIM		';'
#define RTE_ETH_PCAP_KEY_VALUE_DELIM	'='
/*
 * Receives a string with a list of arguments following the pattern
 * key=value;key=value;... and inserts them into the non NULL dictionary.
 * strtok is used so the params string will be copied to be modified.
 */
inline int
rte_eth_pcap_tokenize_args(struct args_dict *dict,
		const char *name,
		const char *params)
{
	int i;
	char *args;
	char *pairs[RTE_ETH_PCAP_ARG_PARSER_MAX_ARGS];
	char *pair[2];
	int num_of_pairs;

	/* If params are empty, nothing to do */
	if (params == NULL || params[0] == 0) {
		RTE_LOG(ERR, PMD, "Couldn't parse %s device, empty arguments\n", name);
		return -1;
	}

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	args = strdup(params);
	if(args == NULL){
		RTE_LOG(ERR, PMD, "Couldn't parse %s device \n", name);
		return -1;
	}

	num_of_pairs = rte_strsplit(args, strnlen(args, sysconf(_SC_ARG_MAX)), pairs,
			RTE_ETH_PCAP_ARG_PARSER_MAX_ARGS, RTE_ETH_PCAP_PAIRS_DELIM);

	for (i = 0; i < num_of_pairs; i++) {
		pair[0] = NULL;
		pair[1] = NULL;

		rte_strsplit(pairs[i], strnlen(pairs[i], sysconf(_SC_ARG_MAX)), pair, 2,
				RTE_ETH_PCAP_KEY_VALUE_DELIM);

		if (pair[0] == NULL || pair[1] == NULL || pair[0][0] == 0
				|| pair[1][0] == 0) {
			RTE_LOG(ERR, PMD,
					"Couldn't parse %s device, wrong key or value \n", name);
			goto error;
		}

		if (rte_eth_pcap_add_pair_to_dict(dict, pair[0], pair[1]) < 0)
			goto error;
	}
	return 0;

error:
	rte_free(args);
	return -1;
}

/*
 * Determines whether a key is valid or not by looking
 * into a list of valid keys.
 */
static inline int
is_valid_key(const char *valid[],
		struct key_value *pair)
{
	const char **valid_ptr;

	for (valid_ptr = valid; *valid_ptr != NULL; valid_ptr++)
		if (strstr(pair->key, *valid_ptr) != NULL)
			return 1;
	return 0;
}

/*
 * Determines whether all keys are valid or not by looking
 * into a list of valid keys.
 */
static inline int
check_for_valid_keys(struct args_dict *dict,
		const char *valid[])
{
	unsigned k_index, ret;
	struct key_value *pair;

	for (k_index = 0; k_index < dict->index; k_index++) {
		pair = &dict->pairs[k_index];
		ret = is_valid_key(valid, pair);
		if (!ret) {
			RTE_LOG(ERR, PMD,
					"Error parsing device, invalid key %s\n", pair->key);
			return -1;
		}
	}
	return 0;
}

/*
 * Returns the number of times a given arg_name exists on a dictionary.
 * E.g. given a dict = { rx0 = 0, rx1 = 1, tx0 = 2 } the number of args for
 * arg "rx" will be 2.
 */
inline unsigned
rte_eth_pcap_num_of_args(struct args_dict *dict, const char *arg_name)
{
	unsigned k_index;
	struct key_value *pair;
	unsigned num_of_keys;

	num_of_keys = 0;
	for (k_index = 0; k_index < dict->index; k_index++) {
		pair = &dict->pairs[k_index];
		if (strcmp(pair->key, arg_name) == 0)
			num_of_keys++;
	}

	return num_of_keys;
}

/*
 * Calls the handler function for a given arg_name passing the
 * value on the dictionary for that key and a given extra argument.
 */
inline int
rte_eth_pcap_post_process_arguments(struct args_dict *dict,
		const char *arg_name,
		arg_handler_t handler,
		void *extra_args)
{
	unsigned k_index;
	struct key_value *pair;

	for (k_index = 0; k_index < dict->index; k_index++) {
		pair = &dict->pairs[k_index];
		if (strstr(pair->key, arg_name) != NULL) {
			if ((*handler)(pair->value, extra_args) < 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Parses the arguments "key=value;key=value;..." string and returns
 * a simple dictionary implementation containing these pairs. It also
 * checks if only valid keys were used.
 */
inline int
rte_eth_pcap_parse_args(struct args_dict *dict,
		const char *name,
		const char *args,
		const char *valids[])
{

	int ret;

	ret = rte_eth_pcap_tokenize_args(dict, name, args);
	if (ret < 0)
		return ret;

	return check_for_valid_keys(dict, valids);
}

