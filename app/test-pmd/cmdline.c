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

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#ifndef __linux__
#include <net/socket.h>
#endif
#include <netinet/in.h>

#include <sys/queue.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "testpmd.h"

/* *** HELP *** */
struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	cmdline_printf(cl,
		       "\n"
		       "TEST PMD\n"
		       "--------\n"
		       "\n"
		       "This commandline can be used to configure forwarding\n"
		       "\n");
	cmdline_printf(cl,
		       "Display informations:\n"
		       "---------------------\n"
		       "- show port info|stats|fdir X|all\n"
		       "    Diplays information or stats on port X, or all\n"
		       "- clear port stats X|all\n"
		       "    Clear stats for port X, or all\n"
		       "- show config rxtx|cores|fwd\n"
		       "    Displays the given configuration\n"
		       "- read reg port_id reg_off\n"
		       "    Displays value of a port register\n"
		       "- read regfield port_id reg_off bit_x bit_y\n"
		       "    Displays value of a port register bit field\n"
		       "- read regbit port_id reg_off bit_x\n"
		       "    Displays value of a port register bit\n"
		       "- read rxd port_id queue_id rxd_id\n"
		       "    Displays a RX descriptor of a port RX queue\n"
		       "- read txd port_id queue_id txd_id\n"
		       "    Displays a TX descriptor of a port TX queue\n"
		       "\n");
	cmdline_printf(cl,
		       "Configure:\n"
		       "----------\n"
		       "Modifications are taken into account once "
		       "forwarding is restarted.\n"
		       "- set default\n"
		       "    Set forwarding to default configuration\n"
		       "- set nbport|nbcore|burst|verbose X\n"
		       "    Set number of ports, number of cores, number "
		       "of packets per burst,\n    or verbose level to X\n"
		       "- set txpkts x[,y]*\n"
		       "    Set the length of each segment of TXONLY packets\n"
		       "- set coremask|portmask X\n"
		       "    Set the hexadecimal mask of forwarding cores / "
		       "forwarding ports\n"
		       "- set corelist|portlist x[,y]*\n"
		       "    Set the list of forwarding cores / forwarding "
		       "ports\n"
		       "- rx_vlan add/rm vlan_id|all port_id\n"
		       "    Add/remove vlan_id, or all identifiers, to/from "
		       "the set of VLAN Identifiers\n    filtered by port_id\n"
		       "- tx_vlan set vlan_id port_id\n"
		       "    Enable hardware insertion of a VLAN header with "
		       "the Tag Identifier vlan_id\n    in packets sent on"
		       "port_id\n"
		       "- tx_vlan reset port_id\n"
		       "    Disable hardware insertion of a VLAN header in "
		       "packets sent on port_id\n"
		       "- tx_checksum set mask port_id\n"
		       "    Enable hardware insertion of checksum offload with "
		       "the 4-bit mask (0~0xf)\n    in packets sent on port_id\n"
		       "    Please check the NIC datasheet for HW limits\n"
		       "      bit 0 - insert ip checksum offload if set \n"
		       "      bit 1 - insert udp checksum offload if set \n"
		       "      bit 2 - insert tcp checksum offload if set\n"
		       "      bit 3 - insert sctp checksum offload if set\n"
#ifdef RTE_LIBRTE_IEEE1588
		       "- set fwd io|mac|rxonly|txonly|csum|ieee1588\n"
		       "    Set IO, MAC, RXONLY, TXONLY, CSUM or IEEE1588 "
		       "packet forwarding mode\n"
#else
		       "- set fwd io|mac|rxonly|txonly|csum\n"
		       "    Set IO, MAC, RXONLY, CSUM or TXONLY packet "
		       "forwarding mode\n"
#endif
		       "- mac_addr add|remove X <xx:xx:xx:xx:xx:xx>\n"
		       "    Add/Remove the MAC address <xx:xx:xx:xx:xx:xx> on port X\n"
		       "- set promisc|allmulti [all|X] on|off\n"
		       "    Set/unset promisc|allmulti mode on port X, or all\n"
		       "- set flow_ctrl rx on|off tx on|off high_water low_water "
						"pause_time send_xon port_id \n"
		       "    Set the link flow control parameter on the port \n"
		       "- write reg port_id reg_off value\n"
		       "    Set value of a port register\n"
		       "- write regfield port_id reg_off bit_x bit_y value\n"
		       "    Set bit field value of a port register\n"
		       "- write regbit port_id reg_off bit_x value\n"
		       "    Set bit value of a port register\n"
		       "\n");
	cmdline_printf(cl,
		       "Control forwarding:\n"
		       "-------------------\n"
		       "- start\n"
		       "    Start packet forwarding with current config\n"
		       "- start tx_first\n"
		       "    Start packet forwarding with current config"
		       " after sending one burst\n    of packets\n"
		       "- stop\n"
		       "    Stop packet forwarding, and displays accumulated"
		       " stats\n"
		       "\n");
	cmdline_printf(cl,
		       "Flow director mode:\n"
		       "-------------------\n"
		       "- add_signature_filter port_id ip|udp|tcp|sctp src\n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id queue queue_id\n"
		       "- upd_signature_filter port_id ip|udp|tcp|sctp src \n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id queue queue_id\n"
		       "- rm_signature_filter port_id ip|udp|tcp|sctp src\n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id\n"
		       "- add_perfect_filter port_id ip|udp|tcp|sctp src\n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id queue \n"
		       "    queue_id soft soft_id\n"
		       "- upd_perfect_filter port_id ip|udp|tcp|sctp src\n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id queue queue_id\n"
		       "- rm_perfect_filter port_id ip|udp|tcp|sctp src\n"
		       "    ip_src_address port_src dst ip_dst_address port_dst\n"
		       "    flexbytes flexbytes_values vlan vlan_id soft soft_id\n"
		       "- set_masks_filter port_id only_ip_flow 0|1 src_mask\n"
		       "    ip_src_mask  port_src_mask dst_mask ip_dst_mask\n"
		       "    port_dst_mask flexbytes 0|1 vlan_id 0|1 vlan_prio 0|1\n"
		       "\n");
	cmdline_printf(cl,
		       "Misc:\n"
		       "-----\n"
		       "- quit\n"
		       "    Quit to prompt in linux, and reboot on baremetal\n"
		       "\n");
}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,
	.data = NULL,
	.help_str = "show help",
	.tokens = {
		(void *)&cmd_help_help,
		NULL,
	},
};

/* *** stop *** */
struct cmd_stop_result {
	cmdline_fixed_string_t stop;
};

static void cmd_stop_parsed(__attribute__((unused)) void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	stop_packet_forwarding();
}

cmdline_parse_token_string_t cmd_stop_stop =
	TOKEN_STRING_INITIALIZER(struct cmd_stop_result, stop, "stop");

cmdline_parse_inst_t cmd_stop = {
	.f = cmd_stop_parsed,
	.data = NULL,
	.help_str = "stop - stop packet forwarding",
	.tokens = {
		(void *)&cmd_stop_stop,
		NULL,
	},
};

/* *** SET CORELIST and PORTLIST CONFIGURATION *** */

static unsigned int
parse_item_list(char* str, const char* item_name, unsigned int max_items,
		unsigned int *parsed_items, int check_unique_values)
{
	unsigned int nb_item;
	unsigned int value;
	unsigned int i;
	unsigned int j;
	int value_ok;
	char c;

	/*
	 * First parse all items in the list and store their value.
	 */
	value = 0;
	nb_item = 0;
	value_ok = 0;
	for (i = 0; i < strnlen(str, STR_TOKEN_SIZE); i++) {
		c = str[i];
		if ((c >= '0') && (c <= '9')) {
			value = (unsigned int) (value * 10 + (c - '0'));
			value_ok = 1;
			continue;
		}
		if (c != ',') {
			printf("character %c is not a decimal digit\n", c);
			return (0);
		}
		if (! value_ok) {
			printf("No valid value before comma\n");
			return (0);
		}
		if (nb_item < max_items) {
			parsed_items[nb_item] = value;
			value_ok = 0;
			value = 0;
		}
		nb_item++;
	}
	if (nb_item >= max_items) {
		printf("Number of %s = %u > %u (maximum items)\n",
		       item_name, nb_item + 1, max_items);
		return (0);
	}
	parsed_items[nb_item++] = value;
	if (! check_unique_values)
		return (nb_item);

	/*
	 * Then, check that all values in the list are differents.
	 * No optimization here...
	 */
	for (i = 0; i < nb_item; i++) {
		for (j = i + 1; j < nb_item; j++) {
			if (parsed_items[j] == parsed_items[i]) {
				printf("duplicated %s %u at index %u and %u\n",
				       item_name, parsed_items[i], i, j);
				return (0);
			}
		}
	}
	return (nb_item);
}

struct cmd_set_list_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t list_name;
	cmdline_fixed_string_t list_of_items;
};

static void cmd_set_list_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_set_list_result *res;
	union {
		unsigned int lcorelist[RTE_MAX_LCORE];
		unsigned int portlist[RTE_MAX_ETHPORTS];
	} parsed_items;
	unsigned int nb_item;

	res = parsed_result;
	if (!strcmp(res->list_name, "corelist")) {
		nb_item = parse_item_list(res->list_of_items, "core",
					  RTE_MAX_LCORE,
					  parsed_items.lcorelist, 1);
		if (nb_item > 0)
			set_fwd_lcores_list(parsed_items.lcorelist, nb_item);
		return;
	}
	if (!strcmp(res->list_name, "portlist")) {
		nb_item = parse_item_list(res->list_of_items, "port",
					  RTE_MAX_ETHPORTS,
					  parsed_items.portlist, 1);
		if (nb_item > 0)
			set_fwd_ports_list(parsed_items.portlist, nb_item);
	}
}

cmdline_parse_token_string_t cmd_set_list_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_list_result, cmd_keyword,
				 "set");
cmdline_parse_token_string_t cmd_set_list_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_list_result, list_name,
				 "corelist#portlist");
cmdline_parse_token_string_t cmd_set_list_of_items =
	TOKEN_STRING_INITIALIZER(struct cmd_set_list_result, list_of_items,
				 NULL);

cmdline_parse_inst_t cmd_set_fwd_list = {
	.f = cmd_set_list_parsed,
	.data = NULL,
	.help_str = "set corelist|portlist x[,y]*",
	.tokens = {
		(void *)&cmd_set_list_keyword,
		(void *)&cmd_set_list_name,
		(void *)&cmd_set_list_of_items,
		NULL,
	},
};

/* *** SET COREMASK and PORTMASK CONFIGURATION *** */

struct cmd_setmask_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mask;
	uint64_t hexavalue;
};

static void cmd_set_mask_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_setmask_result *res = parsed_result;

	if (!strcmp(res->mask, "coremask"))
		set_fwd_lcores_mask(res->hexavalue);
	else if (!strcmp(res->mask, "portmask"))
		set_fwd_ports_mask(res->hexavalue);
}

cmdline_parse_token_string_t cmd_setmask_set =
	TOKEN_STRING_INITIALIZER(struct cmd_setmask_result, set, "set");
cmdline_parse_token_string_t cmd_setmask_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_setmask_result, mask,
				 "coremask#portmask");
cmdline_parse_token_num_t cmd_setmask_value =
	TOKEN_NUM_INITIALIZER(struct cmd_setmask_result, hexavalue, UINT64);

cmdline_parse_inst_t cmd_set_fwd_mask = {
	.f = cmd_set_mask_parsed,
	.data = NULL,
	.help_str = "set coremask|portmask hexadecimal value",
	.tokens = {
		(void *)&cmd_setmask_set,
		(void *)&cmd_setmask_mask,
		(void *)&cmd_setmask_value,
		NULL,
	},
};

/*
 * SET NBPORT, NBCORE, PACKET BURST, and VERBOSE LEVEL CONFIGURATION
 */
struct cmd_set_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t what;
	uint16_t value;
};

static void cmd_set_parsed(void *parsed_result,
			   __attribute__((unused)) struct cmdline *cl,
			   __attribute__((unused)) void *data)
{
	struct cmd_set_result *res = parsed_result;
	if (!strcmp(res->what, "nbport"))
		set_fwd_ports_number(res->value);
	else if (!strcmp(res->what, "nbcore"))
		set_fwd_lcores_number(res->value);
	else if (!strcmp(res->what, "burst"))
		set_nb_pkt_per_burst(res->value);
	else if (!strcmp(res->what, "verbose"))
		set_verbose_level(res->value);
}

cmdline_parse_token_string_t cmd_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_result, set, "set");
cmdline_parse_token_string_t cmd_set_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_result, what,
				 "nbport#nbcore#burst#verbose");
cmdline_parse_token_num_t cmd_set_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_result, value, UINT16);

cmdline_parse_inst_t cmd_set_numbers = {
	.f = cmd_set_parsed,
	.data = NULL,
	.help_str = "set nbport|nbcore|burst|verbose value",
	.tokens = {
		(void *)&cmd_set_set,
		(void *)&cmd_set_what,
		(void *)&cmd_set_value,
		NULL,
	},
};

/* *** SET SEGMENT LENGTHS OF TXONLY PACKETS *** */

struct cmd_set_txpkts_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t txpkts;
	cmdline_fixed_string_t seg_lengths;
};

static void
cmd_set_txpkts_parsed(void *parsed_result,
		      __attribute__((unused)) struct cmdline *cl,
		      __attribute__((unused)) void *data)
{
	struct cmd_set_txpkts_result *res;
	unsigned seg_lengths[RTE_MAX_SEGS_PER_PKT];
	unsigned int nb_segs;

	res = parsed_result;
	nb_segs = parse_item_list(res->seg_lengths, "segment lengths",
				  RTE_MAX_SEGS_PER_PKT, seg_lengths, 0);
	if (nb_segs > 0)
		set_tx_pkt_segments(seg_lengths, nb_segs);
}

cmdline_parse_token_string_t cmd_set_txpkts_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txpkts_result,
				 cmd_keyword, "set");
cmdline_parse_token_string_t cmd_set_txpkts_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txpkts_result,
				 txpkts, "txpkts");
cmdline_parse_token_string_t cmd_set_txpkts_lengths =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txpkts_result,
				 seg_lengths, NULL);

cmdline_parse_inst_t cmd_set_txpkts = {
	.f = cmd_set_txpkts_parsed,
	.data = NULL,
	.help_str = "set txpkts x[,y]*",
	.tokens = {
		(void *)&cmd_set_txpkts_keyword,
		(void *)&cmd_set_txpkts_name,
		(void *)&cmd_set_txpkts_lengths,
		NULL,
	},
};

/* *** ADD/REMOVE ALL VLAN IDENTIFIERS TO/FROM A PORT VLAN RX FILTER *** */
struct cmd_rx_vlan_filter_all_result {
	cmdline_fixed_string_t rx_vlan;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t all;
	uint8_t port_id;
};

static void
cmd_rx_vlan_filter_all_parsed(void *parsed_result,
			      __attribute__((unused)) struct cmdline *cl,
			      __attribute__((unused)) void *data)
{
	struct cmd_rx_vlan_filter_all_result *res = parsed_result;

	if (!strcmp(res->what, "add"))
		rx_vlan_all_filter_set(res->port_id, 1);
	else
		rx_vlan_all_filter_set(res->port_id, 0);
}

cmdline_parse_token_string_t cmd_rx_vlan_filter_all_rx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_rx_vlan_filter_all_result,
				 rx_vlan, "rx_vlan");
cmdline_parse_token_string_t cmd_rx_vlan_filter_all_what =
	TOKEN_STRING_INITIALIZER(struct cmd_rx_vlan_filter_all_result,
				 what, "add#rm");
cmdline_parse_token_string_t cmd_rx_vlan_filter_all_all =
	TOKEN_STRING_INITIALIZER(struct cmd_rx_vlan_filter_all_result,
				 all, "all");
cmdline_parse_token_num_t cmd_rx_vlan_filter_all_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_rx_vlan_filter_all_result,
			      port_id, UINT8);

cmdline_parse_inst_t cmd_rx_vlan_filter_all = {
	.f = cmd_rx_vlan_filter_all_parsed,
	.data = NULL,
	.help_str = "add/remove all identifiers to/from the set of VLAN "
	"Identifiers filtered by a port",
	.tokens = {
		(void *)&cmd_rx_vlan_filter_all_rx_vlan,
		(void *)&cmd_rx_vlan_filter_all_what,
		(void *)&cmd_rx_vlan_filter_all_all,
		(void *)&cmd_rx_vlan_filter_all_portid,
		NULL,
	},
};

/* *** ADD/REMOVE A VLAN IDENTIFIER TO/FROM A PORT VLAN RX FILTER *** */
struct cmd_rx_vlan_filter_result {
	cmdline_fixed_string_t rx_vlan;
	cmdline_fixed_string_t what;
	uint16_t vlan_id;
	uint8_t port_id;
};

static void
cmd_rx_vlan_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_rx_vlan_filter_result *res = parsed_result;

	if (!strcmp(res->what, "add"))
		rx_vlan_filter_set(res->port_id, res->vlan_id, 1);
	else
		rx_vlan_filter_set(res->port_id, res->vlan_id, 0);
}

cmdline_parse_token_string_t cmd_rx_vlan_filter_rx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_rx_vlan_filter_result,
				 rx_vlan, "rx_vlan");
cmdline_parse_token_string_t cmd_rx_vlan_filter_what =
	TOKEN_STRING_INITIALIZER(struct cmd_rx_vlan_filter_result,
				 what, "add#rm");
cmdline_parse_token_num_t cmd_rx_vlan_filter_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_rx_vlan_filter_result,
			      vlan_id, UINT16);
cmdline_parse_token_num_t cmd_rx_vlan_filter_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_rx_vlan_filter_result,
			      port_id, UINT8);

cmdline_parse_inst_t cmd_rx_vlan_filter = {
	.f = cmd_rx_vlan_filter_parsed,
	.data = NULL,
	.help_str = "add/remove a VLAN identifier to/from the set of VLAN "
	"Identifiers filtered by a port",
	.tokens = {
		(void *)&cmd_rx_vlan_filter_rx_vlan,
		(void *)&cmd_rx_vlan_filter_what,
		(void *)&cmd_rx_vlan_filter_vlanid,
		(void *)&cmd_rx_vlan_filter_portid,
		NULL,
	},
};

/* *** ENABLE HARDWARE INSERTION OF VLAN HEADER IN TX PACKETS *** */
struct cmd_tx_vlan_set_result {
	cmdline_fixed_string_t tx_vlan;
	cmdline_fixed_string_t set;
	uint16_t vlan_id;
	uint8_t port_id;
};

static void
cmd_tx_vlan_set_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_tx_vlan_set_result *res = parsed_result;

	tx_vlan_set(res->port_id, res->vlan_id);
}

cmdline_parse_token_string_t cmd_tx_vlan_set_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_result,
				 tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_result,
				 set, "set");
cmdline_parse_token_num_t cmd_tx_vlan_set_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      vlan_id, UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      port_id, UINT8);

cmdline_parse_inst_t cmd_tx_vlan_set = {
	.f = cmd_tx_vlan_set_parsed,
	.data = NULL,
	.help_str = "enable hardware insertion of a VLAN header with a given "
	"TAG Identifier in packets sent on a port",
	.tokens = {
		(void *)&cmd_tx_vlan_set_tx_vlan,
		(void *)&cmd_tx_vlan_set_set,
		(void *)&cmd_tx_vlan_set_vlanid,
		(void *)&cmd_tx_vlan_set_portid,
		NULL,
	},
};

/* *** DISABLE HARDWARE INSERTION OF VLAN HEADER IN TX PACKETS *** */
struct cmd_tx_vlan_reset_result {
	cmdline_fixed_string_t tx_vlan;
	cmdline_fixed_string_t reset;
	uint8_t port_id;
};

static void
cmd_tx_vlan_reset_parsed(void *parsed_result,
			 __attribute__((unused)) struct cmdline *cl,
			 __attribute__((unused)) void *data)
{
	struct cmd_tx_vlan_reset_result *res = parsed_result;

	tx_vlan_reset(res->port_id);
}

cmdline_parse_token_string_t cmd_tx_vlan_reset_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_reset_result,
				 tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_reset_reset =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_reset_result,
				 reset, "reset");
cmdline_parse_token_num_t cmd_tx_vlan_reset_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_reset_result,
			      port_id, UINT8);

cmdline_parse_inst_t cmd_tx_vlan_reset = {
	.f = cmd_tx_vlan_reset_parsed,
	.data = NULL,
	.help_str = "disable hardware insertion of a VLAN header in packets "
	"sent on a port",
	.tokens = {
		(void *)&cmd_tx_vlan_reset_tx_vlan,
		(void *)&cmd_tx_vlan_reset_reset,
		(void *)&cmd_tx_vlan_reset_portid,
		NULL,
	},
};


/* *** ENABLE HARDWARE INSERTION OF CHECKSUM IN TX PACKETS *** */
struct cmd_tx_cksum_set_result {
	cmdline_fixed_string_t tx_cksum;
	cmdline_fixed_string_t set;
	uint8_t cksum_mask;
	uint8_t port_id;
};

static void
cmd_tx_cksum_set_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_tx_cksum_set_result *res = parsed_result;

	tx_cksum_set(res->port_id, res->cksum_mask);
}

cmdline_parse_token_string_t cmd_tx_cksum_set_tx_cksum =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_cksum_set_result,
				tx_cksum, "tx_checksum");
cmdline_parse_token_string_t cmd_tx_cksum_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_cksum_set_result,
				set, "set");
cmdline_parse_token_num_t cmd_tx_cksum_set_cksum_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_cksum_set_result,
				cksum_mask, UINT8);
cmdline_parse_token_num_t cmd_tx_cksum_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_cksum_set_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_tx_cksum_set = {
	.f = cmd_tx_cksum_set_parsed,
	.data = NULL,
	.help_str = "enable hardware insertion of L3/L4checksum with a given "
	"mask in packets sent on a port, the bit mapping is given as, Bit 0 for ip"
	"Bit 1 for UDP, Bit 2 for TCP, Bit 3 for SCTP",
	.tokens = {
		(void *)&cmd_tx_cksum_set_tx_cksum,
		(void *)&cmd_tx_cksum_set_set,
		(void *)&cmd_tx_cksum_set_cksum_mask,
		(void *)&cmd_tx_cksum_set_portid,
		NULL,
	},
};

/* *** SET FORWARDING MODE *** */
struct cmd_set_fwd_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t fwd;
	cmdline_fixed_string_t mode;
};

static void cmd_set_fwd_mode_parsed(void *parsed_result,
				    __attribute__((unused)) struct cmdline *cl,
				    __attribute__((unused)) void *data)
{
	struct cmd_set_fwd_mode_result *res = parsed_result;

	set_pkt_forwarding_mode(res->mode);
}

cmdline_parse_token_string_t cmd_setfwd_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, set, "set");
cmdline_parse_token_string_t cmd_setfwd_fwd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, fwd, "fwd");
cmdline_parse_token_string_t cmd_setfwd_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, mode,
#ifdef RTE_LIBRTE_IEEE1588
				 "io#mac#rxonly#txonly#csum#ieee1588");
#else
				 "io#mac#rxonly#txonly#csum");
#endif

cmdline_parse_inst_t cmd_set_fwd_mode = {
	.f = cmd_set_fwd_mode_parsed,
	.data = NULL,
#ifdef RTE_LIBRTE_IEEE1588
	.help_str = "set fwd io|mac|rxonly|txonly|csum|ieee1588 - set IO, MAC,"
	" RXONLY, TXONLY, CSUM or IEEE1588 packet forwarding mode",
#else
	.help_str = "set fwd io|mac|rxonly|txonly|csum - set IO, MAC,"
	" RXONLY, CSUM or TXONLY packet forwarding mode",
#endif
	.tokens = {
		(void *)&cmd_setfwd_set,
		(void *)&cmd_setfwd_fwd,
		(void *)&cmd_setfwd_mode,
		NULL,
	},
};

/* *** SET PROMISC MODE *** */
struct cmd_set_promisc_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t promisc;
	cmdline_fixed_string_t port_all; /* valid if "allports" argument == 1 */
	uint8_t port_num;                /* valid if "allports" argument == 0 */
	cmdline_fixed_string_t mode;
};

static void cmd_set_promisc_mode_parsed(void *parsed_result,
					__attribute__((unused)) struct cmdline *cl,
					void *allports)
{
	struct cmd_set_promisc_mode_result *res = parsed_result;
	int enable;
	portid_t i;

	if (!strcmp(res->mode, "on"))
		enable = 1;
	else
		enable = 0;

	/* all ports */
	if (allports) {
		for (i = 0; i < nb_ports; i++) {
			if (enable)
				rte_eth_promiscuous_enable(i);
			else
				rte_eth_promiscuous_disable(i);
		}
	}
	else {
		if (enable)
			rte_eth_promiscuous_enable(res->port_num);
		else
			rte_eth_promiscuous_disable(res->port_num);
	}
}

cmdline_parse_token_string_t cmd_setpromisc_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_promisc_mode_result, set, "set");
cmdline_parse_token_string_t cmd_setpromisc_promisc =
	TOKEN_STRING_INITIALIZER(struct cmd_set_promisc_mode_result, promisc,
				 "promisc");
cmdline_parse_token_string_t cmd_setpromisc_portall =
	TOKEN_STRING_INITIALIZER(struct cmd_set_promisc_mode_result, port_all,
				 "all");
cmdline_parse_token_num_t cmd_setpromisc_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_promisc_mode_result, port_num,
			      UINT8);
cmdline_parse_token_string_t cmd_setpromisc_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_promisc_mode_result, mode,
				 "on#off");

cmdline_parse_inst_t cmd_set_promisc_mode_all = {
	.f = cmd_set_promisc_mode_parsed,
	.data = (void *)1,
	.help_str = "set promisc all on|off: set promisc mode for all ports",
	.tokens = {
		(void *)&cmd_setpromisc_set,
		(void *)&cmd_setpromisc_promisc,
		(void *)&cmd_setpromisc_portall,
		(void *)&cmd_setpromisc_mode,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_promisc_mode_one = {
	.f = cmd_set_promisc_mode_parsed,
	.data = (void *)0,
	.help_str = "set promisc X on|off: set promisc mode on port X",
	.tokens = {
		(void *)&cmd_setpromisc_set,
		(void *)&cmd_setpromisc_promisc,
		(void *)&cmd_setpromisc_portnum,
		(void *)&cmd_setpromisc_mode,
		NULL,
	},
};

/* *** SET ALLMULTI MODE *** */
struct cmd_set_allmulti_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t allmulti;
	cmdline_fixed_string_t port_all; /* valid if "allports" argument == 1 */
	uint8_t port_num;                /* valid if "allports" argument == 0 */
	cmdline_fixed_string_t mode;
};

static void cmd_set_allmulti_mode_parsed(void *parsed_result,
					__attribute__((unused)) struct cmdline *cl,
					void *allports)
{
	struct cmd_set_allmulti_mode_result *res = parsed_result;
	int enable;
	portid_t i;

	if (!strcmp(res->mode, "on"))
		enable = 1;
	else
		enable = 0;

	/* all ports */
	if (allports) {
		for (i = 0; i < nb_ports; i++) {
			if (enable)
				rte_eth_allmulticast_enable(i);
			else
				rte_eth_allmulticast_disable(i);
		}
	}
	else {
		if (enable)
			rte_eth_allmulticast_enable(res->port_num);
		else
			rte_eth_allmulticast_disable(res->port_num);
	}
}

cmdline_parse_token_string_t cmd_setallmulti_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_allmulti_mode_result, set, "set");
cmdline_parse_token_string_t cmd_setallmulti_allmulti =
	TOKEN_STRING_INITIALIZER(struct cmd_set_allmulti_mode_result, allmulti,
				 "allmulti");
cmdline_parse_token_string_t cmd_setallmulti_portall =
	TOKEN_STRING_INITIALIZER(struct cmd_set_allmulti_mode_result, port_all,
				 "all");
cmdline_parse_token_num_t cmd_setallmulti_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_allmulti_mode_result, port_num,
			      UINT8);
cmdline_parse_token_string_t cmd_setallmulti_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_allmulti_mode_result, mode,
				 "on#off");

cmdline_parse_inst_t cmd_set_allmulti_mode_all = {
	.f = cmd_set_allmulti_mode_parsed,
	.data = (void *)1,
	.help_str = "set allmulti all on|off: set allmulti mode for all ports",
	.tokens = {
		(void *)&cmd_setallmulti_set,
		(void *)&cmd_setallmulti_allmulti,
		(void *)&cmd_setallmulti_portall,
		(void *)&cmd_setallmulti_mode,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_allmulti_mode_one = {
	.f = cmd_set_allmulti_mode_parsed,
	.data = (void *)0,
	.help_str = "set allmulti X on|off: set allmulti mode on port X",
	.tokens = {
		(void *)&cmd_setallmulti_set,
		(void *)&cmd_setallmulti_allmulti,
		(void *)&cmd_setallmulti_portnum,
		(void *)&cmd_setallmulti_mode,
		NULL,
	},
};

/* *** ADD/REMOVE A PKT FILTER *** */
struct cmd_pkt_filter_result {
	cmdline_fixed_string_t pkt_filter;
	uint8_t  port_id;
	cmdline_fixed_string_t protocol;
	cmdline_fixed_string_t src;
	cmdline_ipaddr_t ip_src;
	uint16_t port_src;
	cmdline_fixed_string_t dst;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_dst;
	cmdline_fixed_string_t flexbytes;
	uint16_t flexbytes_value;
	cmdline_fixed_string_t vlan;
	uint16_t  vlan_id;
	cmdline_fixed_string_t queue;
	int8_t  queue_id;
	cmdline_fixed_string_t soft;
	uint8_t  soft_id;
};

static void
cmd_pkt_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct rte_fdir_filter fdir_filter;
	struct cmd_pkt_filter_result *res = parsed_result;

	memset(&fdir_filter, 0, sizeof(struct rte_fdir_filter));

	if (res->ip_src.family == AF_INET)
		fdir_filter.ip_src.ipv4_addr = res->ip_src.addr.ipv4.s_addr;
	else
		memcpy(&(fdir_filter.ip_src.ipv6_addr),
		       &(res->ip_src.addr.ipv6),
		       sizeof(struct in6_addr));

	if (res->ip_dst.family == AF_INET)
		fdir_filter.ip_dst.ipv4_addr = res->ip_dst.addr.ipv4.s_addr;
	else
		memcpy(&(fdir_filter.ip_dst.ipv6_addr),
		       &(res->ip_dst.addr.ipv6),
		       sizeof(struct in6_addr));

	fdir_filter.port_dst = rte_cpu_to_be_16(res->port_dst);
	fdir_filter.port_src = rte_cpu_to_be_16(res->port_src);

	if (!strcmp(res->protocol, "udp"))
		fdir_filter.l4type = RTE_FDIR_L4TYPE_UDP;
	else if (!strcmp(res->protocol, "tcp"))
		fdir_filter.l4type = RTE_FDIR_L4TYPE_TCP;
	else if (!strcmp(res->protocol, "sctp"))
		fdir_filter.l4type = RTE_FDIR_L4TYPE_SCTP;
	else /* default only IP */
		fdir_filter.l4type = RTE_FDIR_L4TYPE_NONE;

	if (res->ip_dst.family == AF_INET6)
		fdir_filter.iptype = RTE_FDIR_IPTYPE_IPV6;
	else
		fdir_filter.iptype = RTE_FDIR_IPTYPE_IPV4;

	fdir_filter.vlan_id    = rte_cpu_to_be_16(res->vlan_id);
	fdir_filter.flex_bytes = rte_cpu_to_be_16(res->flexbytes_value);

	if (!strcmp(res->pkt_filter, "add_signature_filter"))
		fdir_add_signature_filter(res->port_id, res->queue_id,
					  &fdir_filter);
	else if (!strcmp(res->pkt_filter, "upd_signature_filter"))
		fdir_update_signature_filter(res->port_id, res->queue_id,
					     &fdir_filter);
	else if (!strcmp(res->pkt_filter, "rm_signature_filter"))
		fdir_remove_signature_filter(res->port_id, &fdir_filter);
	else if (!strcmp(res->pkt_filter, "add_perfect_filter"))
		fdir_add_perfect_filter(res->port_id, res->soft_id,
					res->queue_id,
					(uint8_t) (res->queue_id < 0),
					&fdir_filter);
	else if (!strcmp(res->pkt_filter, "upd_perfect_filter"))
		fdir_update_perfect_filter(res->port_id, res->soft_id,
					   res->queue_id,
					   (uint8_t) (res->queue_id < 0),
					   &fdir_filter);
	else if (!strcmp(res->pkt_filter, "rm_perfect_filter"))
		fdir_remove_perfect_filter(res->port_id, res->soft_id,
					   &fdir_filter);

}


cmdline_parse_token_num_t cmd_pkt_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_pkt_filter_protocol =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 protocol, "ip#tcp#udp#sctp");
cmdline_parse_token_string_t cmd_pkt_filter_src =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 src, "src");
cmdline_parse_token_ipaddr_t cmd_pkt_filter_ip_src =
	TOKEN_IPADDR_INITIALIZER(struct cmd_pkt_filter_result,
				 ip_src);
cmdline_parse_token_num_t cmd_pkt_filter_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      port_src, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 dst, "dst");
cmdline_parse_token_ipaddr_t cmd_pkt_filter_ip_dst =
	TOKEN_IPADDR_INITIALIZER(struct cmd_pkt_filter_result,
				 ip_dst);
cmdline_parse_token_num_t cmd_pkt_filter_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      port_dst, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_flexbytes =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 flexbytes, "flexbytes");
cmdline_parse_token_num_t cmd_pkt_filter_flexbytes_value =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      flexbytes_value, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 vlan, "vlan");
cmdline_parse_token_num_t cmd_pkt_filter_vlan_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      vlan_id, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 queue, "queue");
cmdline_parse_token_num_t cmd_pkt_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      queue_id, INT8);
cmdline_parse_token_string_t cmd_pkt_filter_soft =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 soft, "soft");
cmdline_parse_token_num_t cmd_pkt_filter_soft_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_result,
			      soft_id, UINT16);


cmdline_parse_token_string_t cmd_pkt_filter_add_signature_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "add_signature_filter");
cmdline_parse_inst_t cmd_add_signature_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "add a signature filter",
	.tokens = {
		(void *)&cmd_pkt_filter_add_signature_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		(void *)&cmd_pkt_filter_queue,
		(void *)&cmd_pkt_filter_queue_id,
		NULL,
	},
};


cmdline_parse_token_string_t cmd_pkt_filter_upd_signature_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "upd_signature_filter");
cmdline_parse_inst_t cmd_upd_signature_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "update a signature filter",
	.tokens = {
		(void *)&cmd_pkt_filter_upd_signature_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		(void *)&cmd_pkt_filter_queue,
		(void *)&cmd_pkt_filter_queue_id,
		NULL,
	},
};


cmdline_parse_token_string_t cmd_pkt_filter_rm_signature_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "rm_signature_filter");
cmdline_parse_inst_t cmd_rm_signature_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "remove a signature filter",
	.tokens = {
		(void *)&cmd_pkt_filter_rm_signature_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		NULL
		},
};


cmdline_parse_token_string_t cmd_pkt_filter_add_perfect_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "add_perfect_filter");
cmdline_parse_inst_t cmd_add_perfect_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "add a perfect filter",
	.tokens = {
		(void *)&cmd_pkt_filter_add_perfect_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		(void *)&cmd_pkt_filter_queue,
		(void *)&cmd_pkt_filter_queue_id,
		(void *)&cmd_pkt_filter_soft,
		(void *)&cmd_pkt_filter_soft_id,
		NULL,
	},
};


cmdline_parse_token_string_t cmd_pkt_filter_upd_perfect_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "upd_perfect_filter");
cmdline_parse_inst_t cmd_upd_perfect_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "update a perfect filter",
	.tokens = {
		(void *)&cmd_pkt_filter_upd_perfect_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		(void *)&cmd_pkt_filter_queue,
		(void *)&cmd_pkt_filter_queue_id,
		(void *)&cmd_pkt_filter_soft,
		(void *)&cmd_pkt_filter_soft_id,
		NULL,
	},
};


cmdline_parse_token_string_t cmd_pkt_filter_rm_perfect_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_result,
				 pkt_filter, "rm_perfect_filter");
cmdline_parse_inst_t cmd_rm_perfect_filter = {
	.f = cmd_pkt_filter_parsed,
	.data = NULL,
	.help_str = "remove a perfect filter",
	.tokens = {
		(void *)&cmd_pkt_filter_rm_perfect_filter,
		(void *)&cmd_pkt_filter_port_id,
		(void *)&cmd_pkt_filter_protocol,
		(void *)&cmd_pkt_filter_src,
		(void *)&cmd_pkt_filter_ip_src,
		(void *)&cmd_pkt_filter_port_src,
		(void *)&cmd_pkt_filter_dst,
		(void *)&cmd_pkt_filter_ip_dst,
		(void *)&cmd_pkt_filter_port_dst,
		(void *)&cmd_pkt_filter_flexbytes,
		(void *)&cmd_pkt_filter_flexbytes_value,
		(void *)&cmd_pkt_filter_vlan,
		(void *)&cmd_pkt_filter_vlan_id,
		(void *)&cmd_pkt_filter_soft,
		(void *)&cmd_pkt_filter_soft_id,
		NULL,
	},
};

/* *** SETUP MASKS FILTER *** */
struct cmd_pkt_filter_masks_result {
	cmdline_fixed_string_t filter_mask;
	uint8_t  port_id;
	cmdline_fixed_string_t src_mask;
	uint32_t ip_src_mask;
	uint16_t port_src_mask;
	cmdline_fixed_string_t dst_mask;
	uint32_t ip_dst_mask;
	uint16_t port_dst_mask;
	cmdline_fixed_string_t flexbytes;
	uint8_t flexbytes_value;
	cmdline_fixed_string_t vlan_id;
	uint8_t  vlan_id_value;
	cmdline_fixed_string_t vlan_prio;
	uint8_t  vlan_prio_value;
	cmdline_fixed_string_t only_ip_flow;
	uint8_t  only_ip_flow_value;
};

static void
cmd_pkt_filter_masks_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct rte_fdir_masks fdir_masks;
	struct cmd_pkt_filter_masks_result *res = parsed_result;

	memset(&fdir_masks, 0, sizeof(struct rte_fdir_masks));

	fdir_masks.only_ip_flow  = res->only_ip_flow_value;
	fdir_masks.vlan_id       = res->vlan_id_value;
	fdir_masks.vlan_prio     = res->vlan_prio_value;
	fdir_masks.dst_ipv4_mask = res->ip_dst_mask;
	fdir_masks.src_ipv4_mask = res->ip_src_mask;
	fdir_masks.src_port_mask = res->port_src_mask;
	fdir_masks.dst_port_mask = res->port_dst_mask;
	fdir_masks.flexbytes     = res->flexbytes_value;

	fdir_set_masks(res->port_id, &fdir_masks);
}

cmdline_parse_token_string_t cmd_pkt_filter_masks_filter_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 filter_mask, "set_masks_filter");
cmdline_parse_token_num_t cmd_pkt_filter_masks_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_pkt_filter_masks_only_ip_flow =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 only_ip_flow, "only_ip_flow");
cmdline_parse_token_num_t cmd_pkt_filter_masks_only_ip_flow_value =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      only_ip_flow_value, UINT8);
cmdline_parse_token_string_t cmd_pkt_filter_masks_src_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 src_mask, "src_mask");
cmdline_parse_token_num_t cmd_pkt_filter_masks_ip_src_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      ip_src_mask, UINT32);
cmdline_parse_token_num_t cmd_pkt_filter_masks_port_src_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      port_src_mask, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_masks_dst_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 src_mask, "dst_mask");
cmdline_parse_token_num_t cmd_pkt_filter_masks_ip_dst_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      ip_dst_mask, UINT32);
cmdline_parse_token_num_t cmd_pkt_filter_masks_port_dst_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      port_dst_mask, UINT16);
cmdline_parse_token_string_t cmd_pkt_filter_masks_flexbytes =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 flexbytes, "flexbytes");
cmdline_parse_token_num_t cmd_pkt_filter_masks_flexbytes_value =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      flexbytes_value, UINT8);
cmdline_parse_token_string_t cmd_pkt_filter_masks_vlan_id =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 vlan_id, "vlan_id");
cmdline_parse_token_num_t cmd_pkt_filter_masks_vlan_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      vlan_id_value, UINT8);
cmdline_parse_token_string_t cmd_pkt_filter_masks_vlan_prio =
	TOKEN_STRING_INITIALIZER(struct cmd_pkt_filter_masks_result,
				 vlan_prio, "vlan_prio");
cmdline_parse_token_num_t cmd_pkt_filter_masks_vlan_prio_value =
	TOKEN_NUM_INITIALIZER(struct cmd_pkt_filter_masks_result,
			      vlan_prio_value, UINT8);

cmdline_parse_inst_t cmd_set_masks_filter = {
	.f = cmd_pkt_filter_masks_parsed,
	.data = NULL,
	.help_str = "setup masks filter",
	.tokens = {
		(void *)&cmd_pkt_filter_masks_filter_mask,
		(void *)&cmd_pkt_filter_masks_port_id,
		(void *)&cmd_pkt_filter_masks_only_ip_flow,
		(void *)&cmd_pkt_filter_masks_only_ip_flow_value,
		(void *)&cmd_pkt_filter_masks_src_mask,
		(void *)&cmd_pkt_filter_masks_ip_src_mask,
		(void *)&cmd_pkt_filter_masks_port_src_mask,
		(void *)&cmd_pkt_filter_masks_dst_mask,
		(void *)&cmd_pkt_filter_masks_ip_dst_mask,
		(void *)&cmd_pkt_filter_masks_port_dst_mask,
		(void *)&cmd_pkt_filter_masks_flexbytes,
		(void *)&cmd_pkt_filter_masks_flexbytes_value,
		(void *)&cmd_pkt_filter_masks_vlan_id,
		(void *)&cmd_pkt_filter_masks_vlan_id_value,
		(void *)&cmd_pkt_filter_masks_vlan_prio,
		(void *)&cmd_pkt_filter_masks_vlan_prio_value,
		NULL,
	},
};

/* *** SETUP ETHERNET LINK FLOW CONTROL *** */
struct cmd_link_flow_ctrl_set_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t flow_ctrl;
	cmdline_fixed_string_t rx;
	cmdline_fixed_string_t rx_lfc_mode;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t tx_lfc_mode;
	uint32_t high_water;
	uint32_t low_water;
	uint16_t pause_time;
	uint16_t send_xon;
	uint8_t  port_id;
};

static void
cmd_link_flow_ctrl_set_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_link_flow_ctrl_set_result *res = parsed_result;
	struct rte_eth_fc_conf fc_conf;
	int rx_fc_enable, tx_fc_enable;
	int ret;

	/*
	 * Rx on/off, flow control is enabled/disabled on RX side. This can indicate
	 * the RTE_FC_TX_PAUSE, Transmit pause frame at the Rx side.
	 * Tx on/off, flow control is enabled/disabled on TX side. This can indicate
	 * the RTE_FC_RX_PAUSE, Respond to the pause frame at the Tx side.
	 */
	static enum rte_eth_fc_mode rx_tx_onoff_2_lfc_mode[2][2] = {
			{RTE_FC_NONE, RTE_FC_TX_PAUSE}, {RTE_FC_RX_PAUSE, RTE_FC_FULL}
	};

	rx_fc_enable = (!strcmp(res->rx_lfc_mode, "on")) ? 1 : 0;
	tx_fc_enable = (!strcmp(res->tx_lfc_mode, "on")) ? 1 : 0;

	fc_conf.mode       = rx_tx_onoff_2_lfc_mode[rx_fc_enable][tx_fc_enable];
	fc_conf.high_water = res->high_water;
	fc_conf.low_water  = res->low_water;
	fc_conf.pause_time = res->pause_time;
	fc_conf.send_xon   = res->send_xon;

	ret = rte_eth_dev_flow_ctrl_set(res->port_id, &fc_conf);
	if (ret != 0)
		printf("bad flow contrl parameter, return code = %d \n", ret);
}

cmdline_parse_token_string_t cmd_lfc_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				set, "set");
cmdline_parse_token_string_t cmd_lfc_set_flow_ctrl =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				flow_ctrl, "flow_ctrl");
cmdline_parse_token_string_t cmd_lfc_set_rx =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				rx, "rx");
cmdline_parse_token_string_t cmd_lfc_set_rx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				rx_lfc_mode, "on#off");
cmdline_parse_token_string_t cmd_lfc_set_tx =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				tx, "tx");
cmdline_parse_token_string_t cmd_lfc_set_tx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				tx_lfc_mode, "on#off");
cmdline_parse_token_num_t cmd_lfc_set_high_water =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				high_water, UINT32);
cmdline_parse_token_num_t cmd_lfc_set_low_water =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				low_water, UINT32);
cmdline_parse_token_num_t cmd_lfc_set_pause_time =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				pause_time, UINT16);
cmdline_parse_token_num_t cmd_lfc_set_send_xon =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				send_xon, UINT16);
cmdline_parse_token_num_t cmd_lfc_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_link_flow_control_set = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = NULL,
	.help_str = "Configure the Ethernet link flow control...",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_rx,
		(void *)&cmd_lfc_set_rx_mode,
		(void *)&cmd_lfc_set_tx,
		(void *)&cmd_lfc_set_tx_mode,
		(void *)&cmd_lfc_set_high_water,
		(void *)&cmd_lfc_set_low_water,
		(void *)&cmd_lfc_set_pause_time,
		(void *)&cmd_lfc_set_send_xon,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

/* *** RESET CONFIGURATION *** */
struct cmd_reset_result {
	cmdline_fixed_string_t reset;
	cmdline_fixed_string_t def;
};

static void cmd_reset_parsed(__attribute__((unused)) void *parsed_result,
			     struct cmdline *cl,
			     __attribute__((unused)) void *data)
{
	cmdline_printf(cl, "Reset to default forwarding configuration...\n");
	set_def_fwd_config();
}

cmdline_parse_token_string_t cmd_reset_set =
	TOKEN_STRING_INITIALIZER(struct cmd_reset_result, reset, "set");
cmdline_parse_token_string_t cmd_reset_def =
	TOKEN_STRING_INITIALIZER(struct cmd_reset_result, def,
				 "default");

cmdline_parse_inst_t cmd_reset = {
	.f = cmd_reset_parsed,
	.data = NULL,
	.help_str = "set default: reset default forwarding configuration",
	.tokens = {
		(void *)&cmd_reset_set,
		(void *)&cmd_reset_def,
		NULL,
	},
};

/* *** START FORWARDING *** */
struct cmd_start_result {
	cmdline_fixed_string_t start;
};

cmdline_parse_token_string_t cmd_start_start =
	TOKEN_STRING_INITIALIZER(struct cmd_start_result, start, "start");

static void cmd_start_parsed(__attribute__((unused)) void *parsed_result,
			     __attribute__((unused)) struct cmdline *cl,
			     __attribute__((unused)) void *data)
{
	start_packet_forwarding(0);
}

cmdline_parse_inst_t cmd_start = {
	.f = cmd_start_parsed,
	.data = NULL,
	.help_str = "start packet forwarding",
	.tokens = {
		(void *)&cmd_start_start,
		NULL,
	},
};

/* *** START FORWARDING WITH ONE TX BURST FIRST *** */
struct cmd_start_tx_first_result {
	cmdline_fixed_string_t start;
	cmdline_fixed_string_t tx_first;
};

static void
cmd_start_tx_first_parsed(__attribute__((unused)) void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	start_packet_forwarding(1);
}

cmdline_parse_token_string_t cmd_start_tx_first_start =
	TOKEN_STRING_INITIALIZER(struct cmd_start_tx_first_result, start,
				 "start");
cmdline_parse_token_string_t cmd_start_tx_first_tx_first =
	TOKEN_STRING_INITIALIZER(struct cmd_start_tx_first_result,
				 tx_first, "tx_first");

cmdline_parse_inst_t cmd_start_tx_first = {
	.f = cmd_start_tx_first_parsed,
	.data = NULL,
	.help_str = "start packet forwarding, after sending 1 burst of packets",
	.tokens = {
		(void *)&cmd_start_tx_first_start,
		(void *)&cmd_start_tx_first_tx_first,
		NULL,
	},
};

/* *** SHOW CFG *** */
struct cmd_showcfg_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t cfg;
	cmdline_fixed_string_t what;
};

static void cmd_showcfg_parsed(void *parsed_result,
			       __attribute__((unused)) struct cmdline *cl,
			       __attribute__((unused)) void *data)
{
	struct cmd_showcfg_result *res = parsed_result;
	if (!strcmp(res->what, "rxtx"))
		rxtx_config_display();
	else if (!strcmp(res->what, "cores"))
		fwd_lcores_config_display();
	else if (!strcmp(res->what, "fwd"))
		fwd_config_display();
}

cmdline_parse_token_string_t cmd_showcfg_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, show, "show");
cmdline_parse_token_string_t cmd_showcfg_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, cfg, "config");
cmdline_parse_token_string_t cmd_showcfg_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, what,
				 "rxtx#cores#fwd");

cmdline_parse_inst_t cmd_showcfg = {
	.f = cmd_showcfg_parsed,
	.data = NULL,
	.help_str = "show config rxtx|cores|fwd",
	.tokens = {
		(void *)&cmd_showcfg_show,
		(void *)&cmd_showcfg_port,
		(void *)&cmd_showcfg_what,
		NULL,
	},
};

/* *** SHOW ALL PORT INFO *** */
struct cmd_showportall_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t all;
};

static void cmd_showportall_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	portid_t i;

	struct cmd_showportall_result *res = parsed_result;
	if (!strcmp(res->show, "clear")) {
		if (!strcmp(res->what, "stats"))
			for (i = 0; i < nb_ports; i++)
				nic_stats_clear(i);
	} else if (!strcmp(res->what, "info"))
		for (i = 0; i < nb_ports; i++)
			port_infos_display(i);
	else if (!strcmp(res->what, "stats"))
		for (i = 0; i < nb_ports; i++)
			nic_stats_display(i);
	else if (!strcmp(res->what, "fdir"))
		for (i = 0; i < nb_ports; i++)
			fdir_get_infos(i);
}

cmdline_parse_token_string_t cmd_showportall_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, show,
				 "show#clear");
cmdline_parse_token_string_t cmd_showportall_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, port, "port");
cmdline_parse_token_string_t cmd_showportall_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, what,
				 "info#stats#fdir");
cmdline_parse_token_string_t cmd_showportall_all =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, all, "all");
cmdline_parse_inst_t cmd_showportall = {
	.f = cmd_showportall_parsed,
	.data = NULL,
	.help_str = "show|clear port info|stats|fdir all",
	.tokens = {
		(void *)&cmd_showportall_show,
		(void *)&cmd_showportall_port,
		(void *)&cmd_showportall_what,
		(void *)&cmd_showportall_all,
		NULL,
	},
};

/* *** SHOW PORT INFO *** */
struct cmd_showport_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t what;
	uint8_t portnum;
};

static void cmd_showport_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_showport_result *res = parsed_result;
	if (!strcmp(res->show, "clear")) {
		if (!strcmp(res->what, "stats"))
			nic_stats_clear(res->portnum);
	} else if (!strcmp(res->what, "info"))
		port_infos_display(res->portnum);
	else if (!strcmp(res->what, "stats"))
		nic_stats_display(res->portnum);
	else if (!strcmp(res->what, "fdir"))
		 fdir_get_infos(res->portnum);
}

cmdline_parse_token_string_t cmd_showport_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, show,
				 "show#clear");
cmdline_parse_token_string_t cmd_showport_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, port, "port");
cmdline_parse_token_string_t cmd_showport_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, what,
				 "info#stats#fdir");
cmdline_parse_token_num_t cmd_showport_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_result, portnum, INT32);

cmdline_parse_inst_t cmd_showport = {
	.f = cmd_showport_parsed,
	.data = NULL,
	.help_str = "show|clear port info|stats|fdir X (X = port number)",
	.tokens = {
		(void *)&cmd_showport_show,
		(void *)&cmd_showport_port,
		(void *)&cmd_showport_what,
		(void *)&cmd_showport_portnum,
		NULL,
	},
};

/* *** READ PORT REGISTER *** */
struct cmd_read_reg_result {
	cmdline_fixed_string_t read;
	cmdline_fixed_string_t reg;
	uint8_t port_id;
	uint32_t reg_off;
};

static void
cmd_read_reg_parsed(void *parsed_result,
		    __attribute__((unused)) struct cmdline *cl,
		    __attribute__((unused)) void *data)
{
	struct cmd_read_reg_result *res = parsed_result;
	port_reg_display(res->port_id, res->reg_off);
}

cmdline_parse_token_string_t cmd_read_reg_read =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_result, read, "read");
cmdline_parse_token_string_t cmd_read_reg_reg =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_result, reg, "reg");
cmdline_parse_token_num_t cmd_read_reg_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_read_reg_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_result, reg_off, UINT32);

cmdline_parse_inst_t cmd_read_reg = {
	.f = cmd_read_reg_parsed,
	.data = NULL,
	.help_str = "read reg port_id reg_off",
	.tokens = {
		(void *)&cmd_read_reg_read,
		(void *)&cmd_read_reg_reg,
		(void *)&cmd_read_reg_port_id,
		(void *)&cmd_read_reg_reg_off,
		NULL,
	},
};

/* *** READ PORT REGISTER BIT FIELD *** */
struct cmd_read_reg_bit_field_result {
	cmdline_fixed_string_t read;
	cmdline_fixed_string_t regfield;
	uint8_t port_id;
	uint32_t reg_off;
	uint8_t bit1_pos;
	uint8_t bit2_pos;
};

static void
cmd_read_reg_bit_field_parsed(void *parsed_result,
			      __attribute__((unused)) struct cmdline *cl,
			      __attribute__((unused)) void *data)
{
	struct cmd_read_reg_bit_field_result *res = parsed_result;
	port_reg_bit_field_display(res->port_id, res->reg_off,
				   res->bit1_pos, res->bit2_pos);
}

cmdline_parse_token_string_t cmd_read_reg_bit_field_read =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_bit_field_result, read,
				 "read");
cmdline_parse_token_string_t cmd_read_reg_bit_field_regfield =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_bit_field_result,
				 regfield, "regfield");
cmdline_parse_token_num_t cmd_read_reg_bit_field_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, port_id,
			      UINT8);
cmdline_parse_token_num_t cmd_read_reg_bit_field_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, reg_off,
			      UINT32);
cmdline_parse_token_num_t cmd_read_reg_bit_field_bit1_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, bit1_pos,
			      UINT8);
cmdline_parse_token_num_t cmd_read_reg_bit_field_bit2_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, bit2_pos,
			      UINT8);

cmdline_parse_inst_t cmd_read_reg_bit_field = {
	.f = cmd_read_reg_bit_field_parsed,
	.data = NULL,
	.help_str = "read regfield port_id reg_off bit_x bit_y "
	"(read register bit field between bit_x and bit_y included)",
	.tokens = {
		(void *)&cmd_read_reg_bit_field_read,
		(void *)&cmd_read_reg_bit_field_regfield,
		(void *)&cmd_read_reg_bit_field_port_id,
		(void *)&cmd_read_reg_bit_field_reg_off,
		(void *)&cmd_read_reg_bit_field_bit1_pos,
		(void *)&cmd_read_reg_bit_field_bit2_pos,
		NULL,
	},
};

/* *** READ PORT REGISTER BIT *** */
struct cmd_read_reg_bit_result {
	cmdline_fixed_string_t read;
	cmdline_fixed_string_t regbit;
	uint8_t port_id;
	uint32_t reg_off;
	uint8_t bit_pos;
};

static void
cmd_read_reg_bit_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_read_reg_bit_result *res = parsed_result;
	port_reg_bit_display(res->port_id, res->reg_off, res->bit_pos);
}

cmdline_parse_token_string_t cmd_read_reg_bit_read =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_bit_result, read, "read");
cmdline_parse_token_string_t cmd_read_reg_bit_regbit =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_bit_result,
				 regbit, "regbit");
cmdline_parse_token_num_t cmd_read_reg_bit_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_read_reg_bit_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, reg_off, UINT32);
cmdline_parse_token_num_t cmd_read_reg_bit_bit_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, bit_pos, UINT8);

cmdline_parse_inst_t cmd_read_reg_bit = {
	.f = cmd_read_reg_bit_parsed,
	.data = NULL,
	.help_str = "read regbit port_id reg_off bit_x (0 <= bit_x <= 31)",
	.tokens = {
		(void *)&cmd_read_reg_bit_read,
		(void *)&cmd_read_reg_bit_regbit,
		(void *)&cmd_read_reg_bit_port_id,
		(void *)&cmd_read_reg_bit_reg_off,
		(void *)&cmd_read_reg_bit_bit_pos,
		NULL,
	},
};

/* *** WRITE PORT REGISTER *** */
struct cmd_write_reg_result {
	cmdline_fixed_string_t write;
	cmdline_fixed_string_t reg;
	uint8_t port_id;
	uint32_t reg_off;
	uint32_t value;
};

static void
cmd_write_reg_parsed(void *parsed_result,
		     __attribute__((unused)) struct cmdline *cl,
		     __attribute__((unused)) void *data)
{
	struct cmd_write_reg_result *res = parsed_result;
	port_reg_set(res->port_id, res->reg_off, res->value);
}

cmdline_parse_token_string_t cmd_write_reg_write =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_result, write, "write");
cmdline_parse_token_string_t cmd_write_reg_reg =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_result, reg, "reg");
cmdline_parse_token_num_t cmd_write_reg_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_write_reg_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, reg_off, UINT32);
cmdline_parse_token_num_t cmd_write_reg_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, value, UINT32);

cmdline_parse_inst_t cmd_write_reg = {
	.f = cmd_write_reg_parsed,
	.data = NULL,
	.help_str = "write reg port_id reg_off reg_value",
	.tokens = {
		(void *)&cmd_write_reg_write,
		(void *)&cmd_write_reg_reg,
		(void *)&cmd_write_reg_port_id,
		(void *)&cmd_write_reg_reg_off,
		(void *)&cmd_write_reg_value,
		NULL,
	},
};

/* *** WRITE PORT REGISTER BIT FIELD *** */
struct cmd_write_reg_bit_field_result {
	cmdline_fixed_string_t write;
	cmdline_fixed_string_t regfield;
	uint8_t port_id;
	uint32_t reg_off;
	uint8_t bit1_pos;
	uint8_t bit2_pos;
	uint32_t value;
};

static void
cmd_write_reg_bit_field_parsed(void *parsed_result,
			       __attribute__((unused)) struct cmdline *cl,
			       __attribute__((unused)) void *data)
{
	struct cmd_write_reg_bit_field_result *res = parsed_result;
	port_reg_bit_field_set(res->port_id, res->reg_off,
			  res->bit1_pos, res->bit2_pos, res->value);
}

cmdline_parse_token_string_t cmd_write_reg_bit_field_write =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_bit_field_result, write,
				 "write");
cmdline_parse_token_string_t cmd_write_reg_bit_field_regfield =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_bit_field_result,
				 regfield, "regfield");
cmdline_parse_token_num_t cmd_write_reg_bit_field_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, port_id,
			      UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_field_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, reg_off,
			      UINT32);
cmdline_parse_token_num_t cmd_write_reg_bit_field_bit1_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, bit1_pos,
			      UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_field_bit2_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, bit2_pos,
			      UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_field_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, value,
			      UINT32);

cmdline_parse_inst_t cmd_write_reg_bit_field = {
	.f = cmd_write_reg_bit_field_parsed,
	.data = NULL,
	.help_str = "write regfield port_id reg_off bit_x bit_y reg_value"
	"(set register bit field between bit_x and bit_y included)",
	.tokens = {
		(void *)&cmd_write_reg_bit_field_write,
		(void *)&cmd_write_reg_bit_field_regfield,
		(void *)&cmd_write_reg_bit_field_port_id,
		(void *)&cmd_write_reg_bit_field_reg_off,
		(void *)&cmd_write_reg_bit_field_bit1_pos,
		(void *)&cmd_write_reg_bit_field_bit2_pos,
		(void *)&cmd_write_reg_bit_field_value,
		NULL,
	},
};

/* *** WRITE PORT REGISTER BIT *** */
struct cmd_write_reg_bit_result {
	cmdline_fixed_string_t write;
	cmdline_fixed_string_t regbit;
	uint8_t port_id;
	uint32_t reg_off;
	uint8_t bit_pos;
	uint8_t value;
};

static void
cmd_write_reg_bit_parsed(void *parsed_result,
			 __attribute__((unused)) struct cmdline *cl,
			 __attribute__((unused)) void *data)
{
	struct cmd_write_reg_bit_result *res = parsed_result;
	port_reg_bit_set(res->port_id, res->reg_off, res->bit_pos, res->value);
}

cmdline_parse_token_string_t cmd_write_reg_bit_write =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_bit_result, write,
				 "write");
cmdline_parse_token_string_t cmd_write_reg_bit_regbit =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_bit_result,
				 regbit, "regbit");
cmdline_parse_token_num_t cmd_write_reg_bit_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, reg_off, UINT32);
cmdline_parse_token_num_t cmd_write_reg_bit_bit_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, bit_pos, UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, value, UINT8);

cmdline_parse_inst_t cmd_write_reg_bit = {
	.f = cmd_write_reg_bit_parsed,
	.data = NULL,
	.help_str = "write regbit port_id reg_off bit_x 0/1 (0 <= bit_x <= 31)",
	.tokens = {
		(void *)&cmd_write_reg_bit_write,
		(void *)&cmd_write_reg_bit_regbit,
		(void *)&cmd_write_reg_bit_port_id,
		(void *)&cmd_write_reg_bit_reg_off,
		(void *)&cmd_write_reg_bit_bit_pos,
		(void *)&cmd_write_reg_bit_value,
		NULL,
	},
};

/* *** READ A RING DESCRIPTOR OF A PORT RX/TX QUEUE *** */
struct cmd_read_rxd_txd_result {
	cmdline_fixed_string_t read;
	cmdline_fixed_string_t rxd_txd;
	uint8_t port_id;
	uint16_t queue_id;
	uint16_t desc_id;
};

static void
cmd_read_rxd_txd_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_read_rxd_txd_result *res = parsed_result;

	if (!strcmp(res->rxd_txd, "rxd"))
		rx_ring_desc_display(res->port_id, res->queue_id, res->desc_id);
	else if (!strcmp(res->rxd_txd, "txd"))
		tx_ring_desc_display(res->port_id, res->queue_id, res->desc_id);
}

cmdline_parse_token_string_t cmd_read_rxd_txd_read =
	TOKEN_STRING_INITIALIZER(struct cmd_read_rxd_txd_result, read, "read");
cmdline_parse_token_string_t cmd_read_rxd_txd_rxd_txd =
	TOKEN_STRING_INITIALIZER(struct cmd_read_rxd_txd_result, rxd_txd,
				 "rxd#txd");
cmdline_parse_token_num_t cmd_read_rxd_txd_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_read_rxd_txd_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, queue_id, UINT16);
cmdline_parse_token_num_t cmd_read_rxd_txd_desc_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, desc_id, UINT16);

cmdline_parse_inst_t cmd_read_rxd_txd = {
	.f = cmd_read_rxd_txd_parsed,
	.data = NULL,
	.help_str = "read rxd|txd port_id queue_id rxd_id",
	.tokens = {
		(void *)&cmd_read_rxd_txd_read,
		(void *)&cmd_read_rxd_txd_rxd_txd,
		(void *)&cmd_read_rxd_txd_port_id,
		(void *)&cmd_read_rxd_txd_queue_id,
		(void *)&cmd_read_rxd_txd_desc_id,
		NULL,
	},
};

/* *** QUIT *** */
struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
			    struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	pmd_test_exit();
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "exit application",
	.tokens = {
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* *** ADD/REMOVE MAC ADDRESS FROM A PORT *** */
struct cmd_mac_addr_result {
	cmdline_fixed_string_t mac_addr_cmd;
	cmdline_fixed_string_t what;
	uint8_t port_num;
	struct ether_addr address;
};

static void cmd_mac_addr_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_mac_addr_result *res = parsed_result;
	int ret;

	if (strcmp(res->what, "add") == 0)
		ret = rte_eth_dev_mac_addr_add(res->port_num, &res->address, 0);
	else
		ret = rte_eth_dev_mac_addr_remove(res->port_num, &res->address);

	/* check the return value and print it if is < 0 */
	if(ret < 0)
		printf("mac_addr_cmd error: (%s)\n", strerror(-ret));

}

cmdline_parse_token_string_t cmd_mac_addr_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_mac_addr_result, mac_addr_cmd,
				"mac_addr");
cmdline_parse_token_string_t cmd_mac_addr_what =
	TOKEN_STRING_INITIALIZER(struct cmd_mac_addr_result, what,
				"add#remove");
cmdline_parse_token_num_t cmd_mac_addr_portnum =
		TOKEN_NUM_INITIALIZER(struct cmd_mac_addr_result, port_num, UINT8);
cmdline_parse_token_string_t cmd_mac_addr_addr =
		TOKEN_ETHERADDR_INITIALIZER(struct cmd_mac_addr_result, address);

cmdline_parse_inst_t cmd_mac_addr = {
	.f = cmd_mac_addr_parsed,
	.data = (void *)0,
	.help_str = "mac_addr add|remove X <address>: "
			"add/remove MAC address on port X",
	.tokens = {
		(void *)&cmd_mac_addr_cmd,
		(void *)&cmd_mac_addr_what,
		(void *)&cmd_mac_addr_portnum,
		(void *)&cmd_mac_addr_addr,
		NULL,
	},
};


/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_showport,
	(cmdline_parse_inst_t *)&cmd_showportall,
	(cmdline_parse_inst_t *)&cmd_showcfg,
	(cmdline_parse_inst_t *)&cmd_start,
	(cmdline_parse_inst_t *)&cmd_start_tx_first,
	(cmdline_parse_inst_t *)&cmd_reset,
	(cmdline_parse_inst_t *)&cmd_set_numbers,
	(cmdline_parse_inst_t *)&cmd_set_txpkts,
	(cmdline_parse_inst_t *)&cmd_set_fwd_list,
	(cmdline_parse_inst_t *)&cmd_set_fwd_mask,
	(cmdline_parse_inst_t *)&cmd_set_fwd_mode,
	(cmdline_parse_inst_t *)&cmd_set_promisc_mode_one,
	(cmdline_parse_inst_t *)&cmd_set_promisc_mode_all,
	(cmdline_parse_inst_t *)&cmd_set_allmulti_mode_one,
	(cmdline_parse_inst_t *)&cmd_set_allmulti_mode_all,
	(cmdline_parse_inst_t *)&cmd_rx_vlan_filter_all,
	(cmdline_parse_inst_t *)&cmd_rx_vlan_filter,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_set,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_reset,
	(cmdline_parse_inst_t *)&cmd_tx_cksum_set,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set,
	(cmdline_parse_inst_t *)&cmd_read_reg,
	(cmdline_parse_inst_t *)&cmd_read_reg_bit_field,
	(cmdline_parse_inst_t *)&cmd_read_reg_bit,
	(cmdline_parse_inst_t *)&cmd_write_reg,
	(cmdline_parse_inst_t *)&cmd_write_reg_bit_field,
	(cmdline_parse_inst_t *)&cmd_write_reg_bit,
	(cmdline_parse_inst_t *)&cmd_read_rxd_txd,
	(cmdline_parse_inst_t *)&cmd_add_signature_filter,
	(cmdline_parse_inst_t *)&cmd_upd_signature_filter,
	(cmdline_parse_inst_t *)&cmd_rm_signature_filter,
	(cmdline_parse_inst_t *)&cmd_add_perfect_filter,
	(cmdline_parse_inst_t *)&cmd_upd_perfect_filter,
	(cmdline_parse_inst_t *)&cmd_rm_perfect_filter,
	(cmdline_parse_inst_t *)&cmd_set_masks_filter,
	(cmdline_parse_inst_t *)&cmd_stop,
	(cmdline_parse_inst_t *)&cmd_mac_addr,
	NULL,
};

/* prompt function, called from main on MASTER lcore */
void
prompt(void)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "testpmd> ");
	if (cl == NULL) {
		return;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
