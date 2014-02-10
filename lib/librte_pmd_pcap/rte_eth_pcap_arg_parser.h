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

#ifndef _RTE_ETH_ARG_PARSER_H_
#define _RTE_ETH_ARG_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_ETH_PCAP_ARG_PARSER_MAX_ARGS 32

typedef int (*arg_handler_t)(char*, void*);

struct key_value {
	char *key;
	char *value;
};

struct args_dict {
	unsigned index;
	size_t size;
	struct key_value pairs[RTE_ETH_PCAP_ARG_PARSER_MAX_ARGS];
};

int rte_eth_pcap_tokenize_args(struct args_dict *dict, const char *name,
		const char *args);
int rte_eth_pcap_init_args_dict(struct args_dict *dict);
int rte_eth_pcap_add_pair_to_dict(struct args_dict *dict, char *key, char *val);
int rte_eth_pcap_parse_args(struct args_dict *dict, const char* name,
		const char *args, const char *valids[]);
int rte_eth_pcap_post_process_arguments(struct args_dict *dict,
		const char *arg_name, arg_handler_t handler, void *extra_args);
unsigned rte_eth_pcap_num_of_args(struct args_dict *dict, const char *key);
void rte_eth_pcap_free_dict(struct args_dict *dict);

#ifdef __cplusplus
}
#endif

#endif
