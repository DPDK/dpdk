/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "rte_ethtool.h"
#include "ethapp.h"

#define EEPROM_DUMP_CHUNKSIZE 1024


struct pcmd_get_params {
	cmdline_fixed_string_t cmd;
};
struct pcmd_int_params {
	cmdline_fixed_string_t cmd;
	uint16_t port;
};
struct pcmd_intstr_params {
	cmdline_fixed_string_t cmd;
	uint16_t port;
	cmdline_fixed_string_t opt;
};
struct pcmd_intmac_params {
	cmdline_fixed_string_t cmd;
	uint16_t port;
	struct ether_addr mac;
};
struct pcmd_str_params {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t opt;
};
struct pcmd_vlan_params {
	cmdline_fixed_string_t cmd;
	uint16_t port;
	cmdline_fixed_string_t mode;
	uint16_t vid;
};
struct pcmd_intintint_params {
	cmdline_fixed_string_t cmd;
	uint16_t port;
	uint16_t tx;
	uint16_t rx;
};


/* Parameter-less commands */
cmdline_parse_token_string_t pcmd_quit_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_get_params, cmd, "quit");
cmdline_parse_token_string_t pcmd_stats_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_get_params, cmd, "stats");
cmdline_parse_token_string_t pcmd_drvinfo_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_get_params, cmd, "drvinfo");
cmdline_parse_token_string_t pcmd_link_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_get_params, cmd, "link");

/* Commands taking just port id */
cmdline_parse_token_string_t pcmd_open_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_int_params, cmd, "open");
cmdline_parse_token_string_t pcmd_stop_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_int_params, cmd, "stop");
cmdline_parse_token_string_t pcmd_rxmode_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_int_params, cmd, "rxmode");
cmdline_parse_token_string_t pcmd_portstats_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_int_params, cmd, "portstats");
cmdline_parse_token_num_t pcmd_int_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_int_params, port, UINT16);

/* Commands taking port id and string */
cmdline_parse_token_string_t pcmd_eeprom_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params, cmd, "eeprom");
cmdline_parse_token_string_t pcmd_mtu_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params, cmd, "mtu");
cmdline_parse_token_string_t pcmd_regs_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params, cmd, "regs");

cmdline_parse_token_num_t pcmd_intstr_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_intstr_params, port, UINT16);
cmdline_parse_token_string_t pcmd_intstr_token_opt =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params, opt, NULL);

/* Commands taking port id and a MAC address string */
cmdline_parse_token_string_t pcmd_macaddr_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intmac_params, cmd, "macaddr");
cmdline_parse_token_num_t pcmd_intmac_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_intmac_params, port, UINT16);
cmdline_parse_token_etheraddr_t pcmd_intmac_token_mac =
	TOKEN_ETHERADDR_INITIALIZER(struct pcmd_intmac_params, mac);

/* Command taking just a MAC address */
cmdline_parse_token_string_t pcmd_validate_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intmac_params, cmd, "validate");


/* Commands taking port id and two integers */
cmdline_parse_token_string_t pcmd_ringparam_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intintint_params, cmd,
		"ringparam");
cmdline_parse_token_num_t pcmd_intintint_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_intintint_params, port, UINT16);
cmdline_parse_token_num_t pcmd_intintint_token_tx =
	TOKEN_NUM_INITIALIZER(struct pcmd_intintint_params, tx, UINT16);
cmdline_parse_token_num_t pcmd_intintint_token_rx =
	TOKEN_NUM_INITIALIZER(struct pcmd_intintint_params, rx, UINT16);


/* Pause commands */
cmdline_parse_token_string_t pcmd_pause_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params, cmd, "pause");
cmdline_parse_token_num_t pcmd_pause_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_intstr_params, port, UINT16);
cmdline_parse_token_string_t pcmd_pause_token_opt =
	TOKEN_STRING_INITIALIZER(struct pcmd_intstr_params,
		opt, "all#tx#rx#none");

/* VLAN commands */
cmdline_parse_token_string_t pcmd_vlan_token_cmd =
	TOKEN_STRING_INITIALIZER(struct pcmd_vlan_params, cmd, "vlan");
cmdline_parse_token_num_t pcmd_vlan_token_port =
	TOKEN_NUM_INITIALIZER(struct pcmd_vlan_params, port, UINT16);
cmdline_parse_token_string_t pcmd_vlan_token_mode =
	TOKEN_STRING_INITIALIZER(struct pcmd_vlan_params, mode, "add#del");
cmdline_parse_token_num_t pcmd_vlan_token_vid =
	TOKEN_NUM_INITIALIZER(struct pcmd_vlan_params, vid, UINT16);


static void
pcmd_quit_callback(__rte_unused void *ptr_params,
	struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	cmdline_quit(ctx);
}


static void
pcmd_drvinfo_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct ethtool_drvinfo info;
	int id_port;

	for (id_port = 0; id_port < rte_eth_dev_count(); id_port++) {
		if (rte_ethtool_get_drvinfo(id_port, &info)) {
			printf("Error getting info for port %i\n", id_port);
			return;
		}
		printf("Port %i driver: %s (ver: %s)\n",
			id_port, info.driver, info.version
		      );
	}
}


static void
pcmd_link_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	int num_ports = rte_eth_dev_count();
	int id_port, stat_port;

	for (id_port = 0; id_port < num_ports; id_port++) {
		if (!rte_eth_dev_is_valid_port(id_port))
			continue;
		stat_port = rte_ethtool_get_link(id_port);
		switch (stat_port) {
		case 0:
			printf("Port %i: Down\n", id_port);
			break;
		case 1:
			printf("Port %i: Up\n", id_port);
			break;
		default:
			printf("Port %i: Error getting link status\n",
				id_port
				);
			break;
		}
	}
	printf("\n");
}


static void
pcmd_regs_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_intstr_params *params = ptr_params;
	int len_regs;
	struct ethtool_regs regs;
	unsigned char *buf_data;
	FILE *fp_regs;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	len_regs = rte_ethtool_get_regs_len(params->port);
	if (len_regs > 0) {
		printf("Port %i: %i bytes\n", params->port, len_regs);
		buf_data = malloc(len_regs);
		if (buf_data == NULL) {
			printf("Error allocating %i bytes for buffer\n",
				len_regs);
			return;
		}
		if (!rte_ethtool_get_regs(params->port, &regs, buf_data)) {
			fp_regs = fopen(params->opt, "wb");
			if (fp_regs == NULL) {
				printf("Error opening '%s' for writing\n",
					params->opt);
			} else {
				if ((int)fwrite(buf_data,
						1, len_regs,
						fp_regs) != len_regs)
					printf("Error writing '%s'\n",
						params->opt);
				fclose(fp_regs);
			}
		}
		free(buf_data);
	} else if (len_regs == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error getting registers\n", params->port);
}


static void
pcmd_eeprom_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_intstr_params *params = ptr_params;
	struct ethtool_eeprom info_eeprom;
	int len_eeprom;
	int pos_eeprom;
	int stat;
	unsigned char bytes_eeprom[EEPROM_DUMP_CHUNKSIZE];
	FILE *fp_eeprom;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	len_eeprom = rte_ethtool_get_eeprom_len(params->port);
	if (len_eeprom > 0) {
		fp_eeprom = fopen(params->opt, "wb");
		if (fp_eeprom == NULL) {
			printf("Error opening '%s' for writing\n",
				params->opt);
			return;
		}
		printf("Total EEPROM length: %i bytes\n", len_eeprom);
		info_eeprom.len = EEPROM_DUMP_CHUNKSIZE;
		for (pos_eeprom = 0;
				pos_eeprom < len_eeprom;
				pos_eeprom += EEPROM_DUMP_CHUNKSIZE) {
			info_eeprom.offset = pos_eeprom;
			if (pos_eeprom + EEPROM_DUMP_CHUNKSIZE > len_eeprom)
				info_eeprom.len = len_eeprom - pos_eeprom;
			else
				info_eeprom.len = EEPROM_DUMP_CHUNKSIZE;
			stat = rte_ethtool_get_eeprom(
				params->port, &info_eeprom, bytes_eeprom
				);
			if (stat != 0) {
				printf("EEPROM read error %i\n", stat);
				break;
			}
			if (fwrite(bytes_eeprom,
					1, info_eeprom.len,
					fp_eeprom) != info_eeprom.len) {
				printf("Error writing '%s'\n", params->opt);
				break;
			}
		}
		fclose(fp_eeprom);
	} else if (len_eeprom == 0)
		printf("Port %i: Device does not have EEPROM\n", params->port);
	else if (len_eeprom == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error getting EEPROM\n", params->port);
}


static void
pcmd_pause_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	void *ptr_data)
{
	struct pcmd_intstr_params *params = ptr_params;
	struct ethtool_pauseparam info;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	if (ptr_data != NULL) {
		stat = rte_ethtool_get_pauseparam(params->port, &info);
	} else {
		memset(&info, 0, sizeof(info));
		if (strcasecmp("all", params->opt) == 0) {
			info.tx_pause = 1;
			info.rx_pause = 1;
		} else if (strcasecmp("tx", params->opt) == 0) {
			info.tx_pause = 1;
			info.rx_pause = 0;
		} else if (strcasecmp("rx", params->opt) == 0) {
			info.tx_pause = 0;
			info.rx_pause = 1;
		} else {
			info.tx_pause = 0;
			info.rx_pause = 0;
		}
		/* Assume auto-negotiation wanted */
		info.autoneg = 1;
		stat = rte_ethtool_set_pauseparam(params->port, &info);
	}
	if (stat == 0) {
		if (info.rx_pause && info.tx_pause)
			printf("Port %i: Tx & Rx Paused\n", params->port);
		else if (info.rx_pause)
			printf("Port %i: Rx Paused\n", params->port);
		else if (info.tx_pause)
			printf("Port %i: Tx Paused\n", params->port);
		else
			printf("Port %i: Tx & Rx not paused\n", params->port);
	} else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error %i\n", params->port, stat);
}


static void
pcmd_open_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_int_params *params = ptr_params;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	lock_port(params->port);
	stat = rte_ethtool_net_open(params->port);
	mark_port_active(params->port);
	unlock_port(params->port);
	if (stat == 0)
		return;
	else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error opening device\n", params->port);
}

static void
pcmd_stop_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_int_params *params = ptr_params;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	lock_port(params->port);
	stat = rte_ethtool_net_stop(params->port);
	mark_port_inactive(params->port);
	unlock_port(params->port);
	if (stat == 0)
		return;
	else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error stopping device\n", params->port);
}


static void
pcmd_rxmode_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_intstr_params *params = ptr_params;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	stat = rte_ethtool_net_set_rx_mode(params->port);
	if (stat == 0)
		return;
	else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error setting rx mode\n", params->port);
}


static void
pcmd_macaddr_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	void *ptr_data)
{
	struct pcmd_intmac_params *params = ptr_params;
	struct ether_addr mac_addr;
	int stat;

	stat = 0;
	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	if (ptr_data != NULL) {
		lock_port(params->port);
		stat = rte_ethtool_net_set_mac_addr(params->port,
			&params->mac);
		mark_port_newmac(params->port);
		unlock_port(params->port);
		if (stat == 0) {
			printf("MAC address changed\n");
			return;
		}
	} else {
		stat = rte_ethtool_net_get_mac_addr(params->port, &mac_addr);
		if (stat == 0) {
			printf(
				"Port %i MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
				params->port,
				mac_addr.addr_bytes[0],
				mac_addr.addr_bytes[1],
				mac_addr.addr_bytes[2],
				mac_addr.addr_bytes[3],
				mac_addr.addr_bytes[4],
				mac_addr.addr_bytes[5]);
			return;
		}
	}

	printf("Port %i: Error %s\n", params->port,
	       strerror(-stat));
}

static void
pcmd_mtu_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_intstr_params *params = ptr_params;
	int stat;
	int new_mtu;
	char *ptr_parse_end;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	new_mtu = atoi(params->opt);
	new_mtu = strtoul(params->opt, &ptr_parse_end, 10);
	if (*ptr_parse_end != '\0' ||
			new_mtu < ETHER_MIN_MTU ||
			new_mtu > ETHER_MAX_JUMBO_FRAME_LEN) {
		printf("Port %i: Invalid MTU value\n", params->port);
		return;
	}
	stat = rte_ethtool_net_change_mtu(params->port, new_mtu);
	if (stat == 0)
		printf("Port %i: MTU set to %i\n", params->port, new_mtu);
	else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error setting MTU\n", params->port);
}



static void pcmd_portstats_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_int_params *params = ptr_params;
	struct rte_eth_stats stat_info;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	stat = rte_ethtool_net_get_stats64(params->port, &stat_info);
	if (stat == 0) {
		printf("Port %i stats\n", params->port);
		printf("   In: %" PRIu64 " (%" PRIu64 " bytes)\n"
			"  Out: %"PRIu64" (%"PRIu64 " bytes)\n"
			"  Err: %"PRIu64"\n",
			stat_info.ipackets,
			stat_info.ibytes,
			stat_info.opackets,
			stat_info.obytes,
			stat_info.ierrors+stat_info.oerrors
		      );
	} else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error fetching statistics\n", params->port);
}

static void pcmd_ringparam_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	void *ptr_data)
{
	struct pcmd_intintint_params *params = ptr_params;
	struct ethtool_ringparam ring_data;
	struct ethtool_ringparam ring_params;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	if (ptr_data == NULL) {
		stat = rte_ethtool_get_ringparam(params->port, &ring_data);
		if (stat == 0) {
			printf("Port %i ring parameters\n"
				"  Rx Pending: %i (%i max)\n"
				"  Tx Pending: %i (%i max)\n",
				params->port,
				ring_data.rx_pending,
				ring_data.rx_max_pending,
				ring_data.tx_pending,
				ring_data.tx_max_pending);
		}
	} else {
		if (params->tx < 1 || params->rx < 1) {
			printf("Error: Invalid parameters\n");
			return;
		}
		memset(&ring_params, 0, sizeof(struct ethtool_ringparam));
		ring_params.tx_pending = params->tx;
		ring_params.rx_pending = params->rx;
		lock_port(params->port);
		stat = rte_ethtool_set_ringparam(params->port, &ring_params);
		unlock_port(params->port);
	}
	if (stat == 0)
		return;
	else if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else
		printf("Port %i: Error fetching statistics\n", params->port);
}

static void pcmd_validate_callback(void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_intmac_params *params = ptr_params;

	if (rte_ethtool_net_validate_addr(0, &params->mac))
		printf("Address is unicast\n");
	else
		printf("Address is not unicast\n");
}


static void pcmd_vlan_callback(__rte_unused void *ptr_params,
	__rte_unused struct cmdline *ctx,
	__rte_unused void *ptr_data)
{
	struct pcmd_vlan_params *params = ptr_params;
	int stat;

	if (!rte_eth_dev_is_valid_port(params->port)) {
		printf("Error: Invalid port number %i\n", params->port);
		return;
	}
	stat = 0;

	if (strcasecmp("add", params->mode) == 0) {
		stat = rte_ethtool_net_vlan_rx_add_vid(
			params->port, params->vid
			);
		if (stat == 0)
			printf("VLAN vid %i added\n", params->vid);

	} else if (strcasecmp("del", params->mode) == 0) {
		stat = rte_ethtool_net_vlan_rx_kill_vid(
			params->port, params->vid
			);
		if (stat == 0)
			printf("VLAN vid %i removed\n", params->vid);
	} else {
		/* Should not happen! */
		printf("Error: Bad mode %s\n", params->mode);
	}
	if (stat == -ENOTSUP)
		printf("Port %i: Operation not supported\n", params->port);
	else if (stat == -ENOSYS)
		printf("Port %i: VLAN filtering disabled\n", params->port);
	else if (stat != 0)
		printf("Port %i: Error changing VLAN setup (code %i)\n",
			params->port, -stat);
}


cmdline_parse_inst_t pcmd_quit = {
	.f = pcmd_quit_callback,
	.data = NULL,
	.help_str = "quit\n     Exit program",
	.tokens = {(void *)&pcmd_quit_token_cmd, NULL},
};
cmdline_parse_inst_t pcmd_drvinfo = {
	.f = pcmd_drvinfo_callback,
	.data = NULL,
	.help_str = "drvinfo\n     Print driver info",
	.tokens = {(void *)&pcmd_drvinfo_token_cmd, NULL},
};
cmdline_parse_inst_t pcmd_link = {
	.f = pcmd_link_callback,
	.data = NULL,
	.help_str = "link\n     Print port link states",
	.tokens = {(void *)&pcmd_link_token_cmd, NULL},
};
cmdline_parse_inst_t pcmd_regs = {
	.f = pcmd_regs_callback,
	.data = NULL,
	.help_str = "regs <port_id> <filename>\n"
		"     Dump port register(s) to file",
	.tokens = {
		(void *)&pcmd_regs_token_cmd,
		(void *)&pcmd_intstr_token_port,
		(void *)&pcmd_intstr_token_opt,
		NULL
	},
};
cmdline_parse_inst_t pcmd_eeprom = {
	.f = pcmd_eeprom_callback,
	.data = NULL,
	.help_str = "eeprom <port_id> <filename>\n    Dump EEPROM to file",
	.tokens = {
		(void *)&pcmd_eeprom_token_cmd,
		(void *)&pcmd_intstr_token_port,
		(void *)&pcmd_intstr_token_opt,
		NULL
	},
};
cmdline_parse_inst_t pcmd_pause_noopt = {
	.f = pcmd_pause_callback,
	.data = (void *)0x01,
	.help_str = "pause <port_id>\n     Print port pause state",
	.tokens = {
		(void *)&pcmd_pause_token_cmd,
		(void *)&pcmd_pause_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_pause = {
	.f = pcmd_pause_callback,
	.data = NULL,
	.help_str =
		"pause <port_id> <all|tx|rx|none>\n     Pause/unpause port",
	.tokens = {
		(void *)&pcmd_pause_token_cmd,
		(void *)&pcmd_pause_token_port,
		(void *)&pcmd_pause_token_opt,
		NULL
	},
};
cmdline_parse_inst_t pcmd_open = {
	.f = pcmd_open_callback,
	.data = NULL,
	.help_str = "open <port_id>\n     Open port",
	.tokens = {
		(void *)&pcmd_open_token_cmd,
		(void *)&pcmd_int_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_stop = {
	.f = pcmd_stop_callback,
	.data = NULL,
	.help_str = "stop <port_id>\n     Stop port",
	.tokens = {
		(void *)&pcmd_stop_token_cmd,
		(void *)&pcmd_int_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_rxmode = {
	.f = pcmd_rxmode_callback,
	.data = NULL,
	.help_str = "rxmode <port_id>\n     Toggle port Rx mode",
	.tokens = {
		(void *)&pcmd_rxmode_token_cmd,
		(void *)&pcmd_int_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_macaddr_get = {
	.f = pcmd_macaddr_callback,
	.data = NULL,
	.help_str = "macaddr <port_id>\n"
		"     Get MAC address",
	.tokens = {
		(void *)&pcmd_macaddr_token_cmd,
		(void *)&pcmd_intstr_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_macaddr = {
	.f = pcmd_macaddr_callback,
	.data = (void *)0x01,
	.help_str =
		"macaddr <port_id> <mac_addr>\n"
		"     Set MAC address",
	.tokens = {
		(void *)&pcmd_macaddr_token_cmd,
		(void *)&pcmd_intmac_token_port,
		(void *)&pcmd_intmac_token_mac,
		NULL
	},
};
cmdline_parse_inst_t pcmd_mtu = {
	.f = pcmd_mtu_callback,
	.data = NULL,
	.help_str = "mtu <port_id> <mtu_value>\n"
		"     Change MTU",
	.tokens = {
		(void *)&pcmd_mtu_token_cmd,
		(void *)&pcmd_intstr_token_port,
		(void *)&pcmd_intstr_token_opt,
		NULL
	},
};
cmdline_parse_inst_t pcmd_portstats = {
	.f = pcmd_portstats_callback,
	.data = NULL,
	.help_str = "portstats <port_id>\n"
		"     Print port eth statistics",
	.tokens = {
		(void *)&pcmd_portstats_token_cmd,
		(void *)&pcmd_int_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_ringparam = {
	.f = pcmd_ringparam_callback,
	.data = NULL,
	.help_str = "ringparam <port_id>\n"
		"     Print ring parameters",
	.tokens = {
		(void *)&pcmd_ringparam_token_cmd,
		(void *)&pcmd_intintint_token_port,
		NULL
	},
};
cmdline_parse_inst_t pcmd_ringparam_set = {
	.f = pcmd_ringparam_callback,
	.data = (void *)1,
	.help_str = "ringparam <port_id> <tx_param> <rx_param>\n"
		"     Set ring parameters",
	.tokens = {
		(void *)&pcmd_ringparam_token_cmd,
		(void *)&pcmd_intintint_token_port,
		(void *)&pcmd_intintint_token_tx,
		(void *)&pcmd_intintint_token_rx,
		NULL
	},
};
cmdline_parse_inst_t pcmd_validate = {
	.f = pcmd_validate_callback,
	.data = NULL,
	.help_str = "validate <mac_addr>\n"
		"     Check that MAC address is valid unicast address",
	.tokens = {
		(void *)&pcmd_validate_token_cmd,
		(void *)&pcmd_intmac_token_mac,
		NULL
	},
};
cmdline_parse_inst_t pcmd_vlan = {
	.f = pcmd_vlan_callback,
	.data = NULL,
	.help_str = "vlan <port_id> <add|del> <vlan_id>\n"
		"     Add/remove VLAN id",
	.tokens = {
		(void *)&pcmd_vlan_token_cmd,
		(void *)&pcmd_vlan_token_port,
		(void *)&pcmd_vlan_token_mode,
		(void *)&pcmd_vlan_token_vid,
		NULL
	},
};


cmdline_parse_ctx_t list_prompt_commands[] = {
	(cmdline_parse_inst_t *)&pcmd_drvinfo,
	(cmdline_parse_inst_t *)&pcmd_eeprom,
	(cmdline_parse_inst_t *)&pcmd_link,
	(cmdline_parse_inst_t *)&pcmd_macaddr_get,
	(cmdline_parse_inst_t *)&pcmd_macaddr,
	(cmdline_parse_inst_t *)&pcmd_mtu,
	(cmdline_parse_inst_t *)&pcmd_open,
	(cmdline_parse_inst_t *)&pcmd_pause_noopt,
	(cmdline_parse_inst_t *)&pcmd_pause,
	(cmdline_parse_inst_t *)&pcmd_portstats,
	(cmdline_parse_inst_t *)&pcmd_regs,
	(cmdline_parse_inst_t *)&pcmd_ringparam,
	(cmdline_parse_inst_t *)&pcmd_ringparam_set,
	(cmdline_parse_inst_t *)&pcmd_rxmode,
	(cmdline_parse_inst_t *)&pcmd_stop,
	(cmdline_parse_inst_t *)&pcmd_validate,
	(cmdline_parse_inst_t *)&pcmd_vlan,
	(cmdline_parse_inst_t *)&pcmd_quit,
	NULL
};


void ethapp_main(void)
{
	struct cmdline *ctx_cmdline;

	ctx_cmdline = cmdline_stdin_new(list_prompt_commands, "EthApp> ");
	cmdline_interact(ctx_cmdline);
	cmdline_stdin_exit(ctx_cmdline);
}
