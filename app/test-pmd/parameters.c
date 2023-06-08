/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_flow.h>

#include "testpmd.h"

static void
usage(char* progname)
{
	printf("\nUsage: %s [EAL options] -- [testpmd options]\n\n",
	       progname);
#ifdef RTE_LIB_CMDLINE
	printf("  --interactive: run in interactive mode.\n");
	printf("  --cmdline-file: execute cli commands before startup.\n");
#endif
	printf("  --auto-start: start forwarding on init "
	       "[always when non-interactive].\n");
	printf("  --help: display this message and quit.\n");
	printf("  --tx-first: start forwarding sending a burst first "
	       "(only if interactive is disabled).\n");
	printf("  --stats-period=PERIOD: statistics will be shown "
	       "every PERIOD seconds (only if interactive is disabled).\n");
	printf("  --display-xstats xstat_name1[,...]: comma-separated list of "
	       "extended statistics to show. Used with --stats-period "
	       "specified or interactive commands that show Rx/Tx statistics "
	       "(i.e. 'show port stats').\n");
	printf("  --num-procs=N: set the total number of multi-process instances.\n");
	printf("  --proc-id=id: set the id of the current process from "
	       "multi-process instances (0 <= id < num-procs).\n");
	printf("  --nb-cores=N: set the number of forwarding cores "
	       "(1 <= N <= %d).\n", nb_lcores);
	printf("  --nb-ports=N: set the number of forwarding ports "
	       "(1 <= N <= %d).\n", nb_ports);
	printf("  --coremask=COREMASK: hexadecimal bitmask of cores running "
	       "the packet forwarding test. The main lcore is reserved for "
	       "command line parsing only, and cannot be masked on for "
	       "packet forwarding.\n");
	printf("  --portmask=PORTMASK: hexadecimal bitmask of ports used "
	       "by the packet forwarding test.\n");
	printf("  --portlist=PORTLIST: list of forwarding ports\n");
	printf("  --numa: enable NUMA-aware allocation of RX/TX rings and of "
	       "RX memory buffers (mbufs).\n");
	printf("  --no-numa: disable NUMA-aware allocation.\n");
	printf("  --port-numa-config=(port,socket)[,(port,socket)]: "
	       "specify the socket on which the memory pool "
	       "used by the port will be allocated.\n");
	printf("  --ring-numa-config=(port,flag,socket)[,(port,flag,socket)]: "
	       "specify the socket on which the TX/RX rings for "
	       "the port will be allocated "
	       "(flag: 1 for RX; 2 for TX; 3 for RX and TX).\n");
	printf("  --socket-num=N: set socket from which all memory is allocated "
	       "in NUMA mode.\n");
	printf("  --mbuf-size=N,[N1[,..Nn]: set the data size of mbuf to "
	       "N bytes. If multiple numbers are specified the extra pools "
	       "will be created to receive packets based on the features "
	       "supported, like packet split, multi-rx-mempool.\n");
	printf("  --total-num-mbufs=N: set the number of mbufs to be allocated "
	       "in mbuf pools.\n");
	printf("  --max-pkt-len=N: set the maximum size of packet to N bytes.\n");
	printf("  --max-lro-pkt-size=N: set the maximum LRO aggregated packet "
	       "size to N bytes.\n");
#ifdef RTE_LIB_CMDLINE
	printf("  --eth-peers-configfile=name: config file with ethernet addresses "
	       "of peer ports.\n");
	printf("  --eth-peer=X,M:M:M:M:M:M: set the MAC address of the X peer "
	       "port (0 <= X < %d).\n", RTE_MAX_ETHPORTS);
#endif
#ifdef RTE_LIB_LATENCYSTATS
	printf("  --latencystats=N: enable latency and jitter statistics "
	       "monitoring on forwarding lcore id N.\n");
#endif
	printf("  --disable-crc-strip: disable CRC stripping by hardware.\n");
	printf("  --enable-scatter: enable scattered Rx.\n");
	printf("  --enable-lro: enable large receive offload.\n");
	printf("  --enable-rx-cksum: enable rx hardware checksum offload.\n");
	printf("  --enable-rx-timestamp: enable rx hardware timestamp offload.\n");
	printf("  --enable-hw-vlan: enable hardware vlan.\n");
	printf("  --enable-hw-vlan-filter: enable hardware vlan filter.\n");
	printf("  --enable-hw-vlan-strip: enable hardware vlan strip.\n");
	printf("  --enable-hw-vlan-extend: enable hardware vlan extend.\n");
	printf("  --enable-hw-qinq-strip: enable hardware qinq strip.\n");
	printf("  --enable-drop-en: enable per queue packet drop.\n");
	printf("  --disable-rss: disable rss.\n");
	printf("  --port-topology=<paired|chained|loop>: set port topology (paired "
	       "is default).\n");
	printf("  --forward-mode=N: set forwarding mode (N: %s).\n",
	       list_pkt_forwarding_modes());
	printf("  --forward-mode=5tswap: set forwarding mode to "
			"swap L2,L3,L4 for MAC, IPv4/IPv6 and TCP/UDP only.\n");
	printf("  --rss-ip: set RSS functions to IPv4/IPv6 only .\n");
	printf("  --rss-udp: set RSS functions to IPv4/IPv6 + UDP.\n");
	printf("  --rss-level-inner: set RSS hash level to innermost\n");
	printf("  --rss-level-outer: set RSS hash level to outermost\n");
	printf("  --rxq=N: set the number of RX queues per port to N.\n");
	printf("  --rxd=N: set the number of descriptors in RX rings to N.\n");
	printf("  --txq=N: set the number of TX queues per port to N.\n");
	printf("  --txd=N: set the number of descriptors in TX rings to N.\n");
	printf("  --hairpinq=N: set the number of hairpin queues per port to "
	       "N.\n");
	printf("  --burst=N: set the number of packets per burst to N.\n");
	printf("  --flowgen-clones=N: set the number of single packet clones to send in flowgen mode. Should be less than burst value.\n");
	printf("  --flowgen-flows=N: set the number of flows in flowgen mode to N (1 <= N <= INT32_MAX).\n");
	printf("  --mbcache=N: set the cache of mbuf memory pool to N.\n");
	printf("  --rxpt=N: set prefetch threshold register of RX rings to N.\n");
	printf("  --rxht=N: set the host threshold register of RX rings to N.\n");
	printf("  --rxfreet=N: set the free threshold of RX descriptors to N "
	       "(0 <= N < value of rxd).\n");
	printf("  --rxwt=N: set the write-back threshold register of RX rings to N.\n");
	printf("  --txpt=N: set the prefetch threshold register of TX rings to N.\n");
	printf("  --txht=N: set the nhost threshold register of TX rings to N.\n");
	printf("  --txwt=N: set the write-back threshold register of TX rings to N.\n");
	printf("  --txfreet=N: set the transmit free threshold of TX rings to N "
	       "(0 <= N <= value of txd).\n");
	printf("  --txrst=N: set the transmit RS bit threshold of TX rings to N "
	       "(0 <= N <= value of txd).\n");
	printf("  --no-flush-rx: Don't flush RX streams before forwarding."
	       " Used mainly with PCAP drivers.\n");
	printf("  --rxoffs=X[,Y]*: set RX segment offsets for split.\n");
	printf("  --rxpkts=X[,Y]*: set RX segment sizes to split.\n");
	printf("  --rxhdrs=eth[,ipv4]*: set RX segment protocol to split.\n");
	printf("  --txpkts=X[,Y]*: set TX segment sizes"
		" or total packet length.\n");
	printf("  --multi-rx-mempool: enable multi-rx-mempool support\n");
	printf("  --txonly-multi-flow: generate multiple flows in txonly mode\n");
	printf("  --tx-ip=src,dst: IP addresses in Tx-only mode\n");
	printf("  --tx-udp=src[,dst]: UDP ports in Tx-only mode\n");
	printf("  --eth-link-speed: force link speed.\n");
	printf("  --rxq-share=X: number of ports per shared Rx queue groups, defaults to UINT32_MAX (1 group)\n");
	printf("  --disable-link-check: disable check on link status when "
	       "starting/stopping ports.\n");
	printf("  --disable-device-start: do not automatically start port\n");
	printf("  --no-lsc-interrupt: disable link status change interrupt.\n");
	printf("  --no-rmv-interrupt: disable device removal interrupt.\n");
	printf("  --bitrate-stats=N: set the logical core N to perform "
		"bit-rate calculation.\n");
	printf("  --print-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|flow_aged|err_recovering|recovery_success|recovery_failed|all>: "
	       "enable print of designated event or all of them.\n");
	printf("  --mask-event <unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|intr_rmv|flow_aged|err_recovering|recovery_success|recovery_failed||all>: "
	       "disable print of designated event or all of them.\n");
	printf("  --flow-isolate-all: "
	       "requests flow API isolated mode on all ports at initialization time.\n");
	printf("  --disable-flow-flush: disable port flow flush when stop port.\n");
	printf("  --tx-offloads=0xXXXXXXXX: hexadecimal bitmask of TX queue offloads\n");
	printf("  --rx-offloads=0xXXXXXXXX: hexadecimal bitmask of RX queue offloads\n");
	printf("  --hot-plug: enable hot plug for device.\n");
	printf("  --vxlan-gpe-port=N: UPD port of tunnel VXLAN-GPE\n");
	printf("  --geneve-parsed-port=N: UPD port to parse GENEVE tunnel protocol\n");
#ifndef RTE_EXEC_ENV_WINDOWS
	printf("  --mlockall: lock all memory\n");
	printf("  --no-mlockall: do not lock all memory\n");
#endif
	printf("  --mp-alloc <native|anon|xmem|xmemhuge>: mempool allocation method.\n"
	       "    native: use regular DPDK memory to create and populate mempool\n"
	       "    anon: use regular DPDK memory to create and anonymous memory to populate mempool\n"
	       "    xmem: use anonymous memory to create and populate mempool\n"
	       "    xmemhuge: use anonymous hugepage memory to create and populate mempool\n");
	printf("  --noisy-forward-mode=<io|mac|macswap|5tswap>: set the sub-fwd mode, defaults to io\n");
	printf("  --noisy-tx-sw-buffer-size=N: size of FIFO buffer\n");
	printf("  --noisy-tx-sw-buffer-flushtime=N: flush FIFO after N ms\n");
	printf("  --noisy-lkup-memory=N: allocate N MB of VNF memory\n");
	printf("  --noisy-lkup-num-writes=N: do N random writes per packet\n");
	printf("  --noisy-lkup-num-reads=N: do N random reads per packet\n");
	printf("  --noisy-lkup-num-reads-writes=N: do N random reads and writes per packet\n");
	printf("  --no-iova-contig: mempool memory can be IOVA non contiguous. "
	       "valid only with --mp-alloc=anon\n");
	printf("  --rx-mq-mode=0xX: hexadecimal bitmask of RX mq mode can be "
	       "enabled\n");
	printf("  --record-core-cycles: enable measurement of CPU cycles.\n");
	printf("  --record-burst-stats: enable display of RX and TX bursts.\n");
	printf("  --hairpin-mode=0xXX: bitmask set the hairpin port mode.\n"
	       "    0x10 - explicit Tx rule, 0x02 - hairpin ports paired\n"
	       "    0x01 - hairpin ports loop, 0x00 - hairpin port self\n");
}

#ifdef RTE_LIB_CMDLINE
static int
init_peer_eth_addrs(const char *config_filename)
{
	FILE *config_file;
	portid_t i;
	char buf[50];

	config_file = fopen(config_filename, "r");
	if (config_file == NULL) {
		perror("Failed to open eth config file\n");
		return -1;
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (fgets(buf, sizeof(buf), config_file) == NULL)
			break;

		if (rte_ether_unformat_addr(buf, &peer_eth_addrs[i]) < 0) {
			fprintf(stderr, "Bad MAC address format on line %d\n",
				i + 1);
			fclose(config_file);
			return -1;
		}
	}
	fclose(config_file);
	nb_peer_eth_addrs = (portid_t) i;
	return 0;
}
#endif

/*
 * Parse the coremask given as argument (hexadecimal string) and set
 * the global configuration of forwarding cores.
 */
static void
parse_fwd_coremask(const char *coremask)
{
	char *end;
	unsigned long long int cm;

	/* parse hexadecimal string */
	end = NULL;
	cm = strtoull(coremask, &end, 16);
	if ((coremask[0] == '\0') || (end == NULL) || (*end != '\0'))
		rte_exit(EXIT_FAILURE, "Invalid fwd core mask\n");
	else if (set_fwd_lcores_mask((uint64_t) cm) < 0)
		rte_exit(EXIT_FAILURE, "coremask is not valid\n");
}

/*
 * Parse the coremask given as argument (hexadecimal string) and set
 * the global configuration of forwarding cores.
 */
static void
parse_fwd_portmask(const char *portmask)
{
	char *end;
	unsigned long long int pm;

	/* parse hexadecimal string */
	end = NULL;
	pm = strtoull(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		rte_exit(EXIT_FAILURE, "Invalid fwd port mask\n");
	else
		set_fwd_ports_mask((uint64_t) pm);
}

static void
print_invalid_socket_id_error(void)
{
	unsigned int i = 0;

	fprintf(stderr, "Invalid socket id, options are: ");
	for (i = 0; i < num_sockets; i++) {
		fprintf(stderr, "%u%s", socket_ids[i],
			(i == num_sockets - 1) ? "\n" : ",");
	}
}

static int
parse_portnuma_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	uint8_t i, socket_id;
	portid_t port_id;
	unsigned size;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_SOCKET,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];

	/* reset from value set at definition */
	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		port_id = (portid_t)int_fld[FLD_PORT];
		if (port_id_is_invalid(port_id, ENABLED_WARN) ||
			port_id == (portid_t)RTE_PORT_ALL) {
			print_valid_ports();
			return -1;
		}
		socket_id = (uint8_t)int_fld[FLD_SOCKET];
		if (new_socket_id(socket_id)) {
			if (num_sockets >= RTE_MAX_NUMA_NODES) {
				print_invalid_socket_id_error();
				return -1;
			}
			socket_ids[num_sockets++] = socket_id;
		}
		port_numa[port_id] = socket_id;
	}

	return 0;
}

static int
parse_ringnuma_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	uint8_t i, ring_flag, socket_id;
	portid_t port_id;
	unsigned size;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_FLAG,
		FLD_SOCKET,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	#define RX_RING_ONLY 0x1
	#define TX_RING_ONLY 0x2
	#define RXTX_RING    0x3

	/* reset from value set at definition */
	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		port_id = (portid_t)int_fld[FLD_PORT];
		if (port_id_is_invalid(port_id, ENABLED_WARN) ||
			port_id == (portid_t)RTE_PORT_ALL) {
			print_valid_ports();
			return -1;
		}
		socket_id = (uint8_t)int_fld[FLD_SOCKET];
		if (new_socket_id(socket_id)) {
			if (num_sockets >= RTE_MAX_NUMA_NODES) {
				print_invalid_socket_id_error();
				return -1;
			}
			socket_ids[num_sockets++] = socket_id;
		}
		ring_flag = (uint8_t)int_fld[FLD_FLAG];
		if ((ring_flag < RX_RING_ONLY) || (ring_flag > RXTX_RING)) {
			fprintf(stderr,
				"Invalid ring-flag=%d config for port =%d\n",
				ring_flag,port_id);
			return -1;
		}

		switch (ring_flag & RXTX_RING) {
		case RX_RING_ONLY:
			rxring_numa[port_id] = socket_id;
			break;
		case TX_RING_ONLY:
			txring_numa[port_id] = socket_id;
			break;
		case RXTX_RING:
			rxring_numa[port_id] = socket_id;
			txring_numa[port_id] = socket_id;
			break;
		default:
			fprintf(stderr,
				"Invalid ring-flag=%d config for port=%d\n",
				ring_flag,port_id);
			break;
		}
	}

	return 0;
}

static int
parse_event_printing_config(const char *optarg, int enable)
{
	uint32_t mask = 0;

	if (!strcmp(optarg, "unknown"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_UNKNOWN;
	else if (!strcmp(optarg, "intr_lsc"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_INTR_LSC;
	else if (!strcmp(optarg, "queue_state"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_QUEUE_STATE;
	else if (!strcmp(optarg, "intr_reset"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_INTR_RESET;
	else if (!strcmp(optarg, "vf_mbox"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_VF_MBOX;
	else if (!strcmp(optarg, "ipsec"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_IPSEC;
	else if (!strcmp(optarg, "macsec"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_MACSEC;
	else if (!strcmp(optarg, "intr_rmv"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_INTR_RMV;
	else if (!strcmp(optarg, "dev_probed"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_NEW;
	else if (!strcmp(optarg, "dev_released"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_DESTROY;
	else if (!strcmp(optarg, "flow_aged"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_FLOW_AGED;
	else if (!strcmp(optarg, "err_recovering"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_ERR_RECOVERING;
	else if (!strcmp(optarg, "recovery_success"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_RECOVERY_SUCCESS;
	else if (!strcmp(optarg, "recovery_failed"))
		mask = UINT32_C(1) << RTE_ETH_EVENT_RECOVERY_FAILED;
	else if (!strcmp(optarg, "all"))
		mask = ~UINT32_C(0);
	else {
		fprintf(stderr, "Invalid event: %s\n", optarg);
		return -1;
	}
	if (enable)
		event_print_mask |= mask;
	else
		event_print_mask &= ~mask;
	return 0;
}

static int
parse_xstats_list(const char *in_str, struct rte_eth_xstat_name **xstats,
		  unsigned int *xstats_num)
{
	int max_names_nb, names_nb, nonempty_names_nb;
	int name, nonempty_name;
	int stringlen;
	char **names;
	char *str;
	int ret;
	int i;

	names = NULL;
	str = strdup(in_str);
	if (str == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	stringlen = strlen(str);

	for (i = 0, max_names_nb = 1; str[i] != '\0'; i++) {
		if (str[i] == ',')
			max_names_nb++;
	}

	names = calloc(max_names_nb, sizeof(*names));
	if (names == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	names_nb = rte_strsplit(str, stringlen, names, max_names_nb, ',');
	if (names_nb < 0) {
		ret = -EINVAL;
		goto out;
	}

	nonempty_names_nb = 0;
	for (i = 0; i < names_nb; i++) {
		if (names[i][0] == '\0')
			continue;
		nonempty_names_nb++;
	}
	*xstats = calloc(nonempty_names_nb, sizeof(**xstats));
	if (*xstats == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	for (name = nonempty_name = 0; name < names_nb; name++) {
		if (names[name][0] == '\0')
			continue;
		rte_strscpy((*xstats)[nonempty_name].name, names[name],
			    sizeof((*xstats)[nonempty_name].name));
		nonempty_name++;
	}

	*xstats_num = nonempty_names_nb;
	ret = 0;

out:
	free(names);
	free(str);
	return ret;
}

static int
parse_link_speed(int n)
{
	uint32_t speed = RTE_ETH_LINK_SPEED_FIXED;

	switch (n) {
	case 1000:
		speed |= RTE_ETH_LINK_SPEED_1G;
		break;
	case 2500:
		speed |= RTE_ETH_LINK_SPEED_2_5G;
		break;
	case 5000:
		speed |= RTE_ETH_LINK_SPEED_5G;
		break;
	case 10000:
		speed |= RTE_ETH_LINK_SPEED_10G;
		break;
	case 25000:
		speed |= RTE_ETH_LINK_SPEED_25G;
		break;
	case 40000:
		speed |= RTE_ETH_LINK_SPEED_40G;
		break;
	case 50000:
		speed |= RTE_ETH_LINK_SPEED_50G;
		break;
	case 100000:
		speed |= RTE_ETH_LINK_SPEED_100G;
		break;
	case 200000:
		speed |= RTE_ETH_LINK_SPEED_200G;
		break;
	case 400000:
		speed |= RTE_ETH_LINK_SPEED_400G;
		break;
	case 100:
	case 10:
	default:
		fprintf(stderr, "Unsupported fixed speed\n");
		return 0;
	}

	return speed;
}

void
launch_args_parse(int argc, char** argv)
{
#define PARAM_PROC_ID "proc-id"
#define PARAM_NUM_PROCS "num-procs"

	int n, opt;
	char **argvopt;
	int opt_idx;
	portid_t pid;
	enum { TX, RX };
	/* Default offloads for all ports. */
	uint64_t rx_offloads = rx_mode.offloads;
	uint64_t tx_offloads = tx_mode.offloads;
	struct rte_eth_dev_info dev_info;
	uint16_t rec_nb_pkts;
	int ret;

	static struct option lgopts[] = {
		{ "help",			0, 0, 0 },
#ifdef RTE_LIB_CMDLINE
		{ "interactive",		0, 0, 0 },
		{ "cmdline-file",		1, 0, 0 },
		{ "auto-start",			0, 0, 0 },
		{ "eth-peers-configfile",	1, 0, 0 },
		{ "eth-peer",			1, 0, 0 },
#endif
		{ "tx-first",			0, 0, 0 },
		{ "stats-period",		1, 0, 0 },
		{ "display-xstats",		1, 0, 0 },
		{ "nb-cores",			1, 0, 0 },
		{ "nb-ports",			1, 0, 0 },
		{ "coremask",			1, 0, 0 },
		{ "portmask",			1, 0, 0 },
		{ "portlist",			1, 0, 0 },
		{ "numa",			0, 0, 0 },
		{ "no-numa",			0, 0, 0 },
		{ "mp-anon",			0, 0, 0 }, /* deprecated */
		{ "port-numa-config",           1, 0, 0 },
		{ "ring-numa-config",           1, 0, 0 },
		{ "socket-num",			1, 0, 0 },
		{ "mbuf-size",			1, 0, 0 },
		{ "total-num-mbufs",		1, 0, 0 },
		{ "max-pkt-len",		1, 0, 0 },
		{ "max-lro-pkt-size",		1, 0, 0 },
#ifdef RTE_LIB_LATENCYSTATS
		{ "latencystats",               1, 0, 0 },
#endif
#ifdef RTE_LIB_BITRATESTATS
		{ "bitrate-stats",              1, 0, 0 },
#endif
		{ "disable-crc-strip",          0, 0, 0 },
		{ "enable-lro",                 0, 0, 0 },
		{ "enable-rx-cksum",            0, 0, 0 },
		{ "enable-rx-timestamp",        0, 0, 0 },
		{ "enable-scatter",             0, 0, 0 },
		{ "enable-hw-vlan",             0, 0, 0 },
		{ "enable-hw-vlan-filter",      0, 0, 0 },
		{ "enable-hw-vlan-strip",       0, 0, 0 },
		{ "enable-hw-vlan-extend",      0, 0, 0 },
		{ "enable-hw-qinq-strip",       0, 0, 0 },
		{ "enable-drop-en",            0, 0, 0 },
		{ "disable-rss",                0, 0, 0 },
		{ "port-topology",              1, 0, 0 },
		{ "forward-mode",               1, 0, 0 },
		{ "rss-ip",			0, 0, 0 },
		{ "rss-udp",			0, 0, 0 },
		{ "rss-level-outer",		0, 0, 0 },
		{ "rss-level-inner",		0, 0, 0 },
		{ "rxq",			1, 0, 0 },
		{ "txq",			1, 0, 0 },
		{ "rxd",			1, 0, 0 },
		{ "txd",			1, 0, 0 },
		{ "hairpinq",			1, 0, 0 },
		{ "hairpin-mode",		1, 0, 0 },
		{ "burst",			1, 0, 0 },
		{ "flowgen-clones",		1, 0, 0 },
		{ "flowgen-flows",		1, 0, 0 },
		{ "mbcache",			1, 0, 0 },
		{ "txpt",			1, 0, 0 },
		{ "txht",			1, 0, 0 },
		{ "txwt",			1, 0, 0 },
		{ "txfreet",			1, 0, 0 },
		{ "txrst",			1, 0, 0 },
		{ "rxpt",			1, 0, 0 },
		{ "rxht",			1, 0, 0 },
		{ "rxwt",			1, 0, 0 },
		{ "rxfreet",                    1, 0, 0 },
		{ "no-flush-rx",	0, 0, 0 },
		{ "flow-isolate-all",	        0, 0, 0 },
		{ "disable-flow-flush",         0, 0, 0 },
		{ "rxoffs",			1, 0, 0 },
		{ "rxpkts",			1, 0, 0 },
		{ "rxhdrs",			1, 0, 0 },
		{ "txpkts",			1, 0, 0 },
		{ "multi-rx-mempool",           0, 0, 0 },
		{ "txonly-multi-flow",		0, 0, 0 },
		{ "rxq-share",			2, 0, 0 },
		{ "eth-link-speed",		1, 0, 0 },
		{ "disable-link-check",		0, 0, 0 },
		{ "disable-device-start",	0, 0, 0 },
		{ "no-lsc-interrupt",		0, 0, 0 },
		{ "no-rmv-interrupt",		0, 0, 0 },
		{ "print-event",		1, 0, 0 },
		{ "mask-event",			1, 0, 0 },
		{ "tx-offloads",		1, 0, 0 },
		{ "rx-offloads",		1, 0, 0 },
		{ "hot-plug",			0, 0, 0 },
		{ "vxlan-gpe-port",		1, 0, 0 },
		{ "geneve-parsed-port",		1, 0, 0 },
#ifndef RTE_EXEC_ENV_WINDOWS
		{ "mlockall",			0, 0, 0 },
		{ "no-mlockall",		0, 0, 0 },
#endif
		{ "mp-alloc",			1, 0, 0 },
		{ "tx-ip",			1, 0, 0 },
		{ "tx-udp",			1, 0, 0 },
		{ "noisy-forward-mode",		1, 0, 0 },
		{ "noisy-tx-sw-buffer-size",	1, 0, 0 },
		{ "noisy-tx-sw-buffer-flushtime", 1, 0, 0 },
		{ "noisy-lkup-memory",		1, 0, 0 },
		{ "noisy-lkup-num-writes",	1, 0, 0 },
		{ "noisy-lkup-num-reads",	1, 0, 0 },
		{ "noisy-lkup-num-reads-writes", 1, 0, 0 },
		{ "no-iova-contig",             0, 0, 0 },
		{ "rx-mq-mode",                 1, 0, 0 },
		{ "record-core-cycles",         0, 0, 0 },
		{ "record-burst-stats",         0, 0, 0 },
		{ PARAM_NUM_PROCS,              1, 0, 0 },
		{ PARAM_PROC_ID,                1, 0, 0 },
		{ 0, 0, 0, 0 },
	};

	argvopt = argv;

#ifdef RTE_LIB_CMDLINE
#define SHORTOPTS "i"
#else
#define SHORTOPTS ""
#endif
	while ((opt = getopt_long(argc, argvopt, SHORTOPTS "ah",
				 lgopts, &opt_idx)) != EOF) {
		switch (opt) {
#ifdef RTE_LIB_CMDLINE
		case 'i':
			printf("Interactive-mode selected\n");
			interactive = 1;
			break;
#endif
		case 'a':
			printf("Auto-start selected\n");
			auto_start = 1;
			break;

		case 0: /*long options */
			if (!strcmp(lgopts[opt_idx].name, "help")) {
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			}
#ifdef RTE_LIB_CMDLINE
			if (!strcmp(lgopts[opt_idx].name, "interactive")) {
				printf("Interactive-mode selected\n");
				interactive = 1;
			}
			if (!strcmp(lgopts[opt_idx].name, "cmdline-file")) {
				printf("CLI commands to be read from %s\n",
				       optarg);
				strlcpy(cmdline_filename, optarg,
					sizeof(cmdline_filename));
			}
			if (!strcmp(lgopts[opt_idx].name, "auto-start")) {
				printf("Auto-start selected\n");
				auto_start = 1;
			}
			if (!strcmp(lgopts[opt_idx].name, "tx-first")) {
				printf("Ports to start sending a burst of "
						"packets first\n");
				tx_first = 1;
			}
			if (!strcmp(lgopts[opt_idx].name, "stats-period")) {
				char *end = NULL;
				unsigned int n;

				n = strtoul(optarg, &end, 10);
				if ((optarg[0] == '\0') || (end == NULL) ||
						(*end != '\0'))
					break;

				stats_period = n;
				break;
			}
			if (!strcmp(lgopts[opt_idx].name, "display-xstats")) {
				char rc;

				rc = parse_xstats_list(optarg, &xstats_display,
						       &xstats_display_num);
				if (rc != 0)
					rte_exit(EXIT_FAILURE,
						 "Failed to parse display-xstats argument: %d\n",
						 rc);
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "eth-peers-configfile")) {
				if (init_peer_eth_addrs(optarg) != 0)
					rte_exit(EXIT_FAILURE,
						 "Cannot open logfile\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "eth-peer")) {
				char *port_end;

				errno = 0;
				n = strtoul(optarg, &port_end, 10);
				if (errno != 0 || port_end == optarg || *port_end++ != ',')
					rte_exit(EXIT_FAILURE,
						 "Invalid eth-peer: %s", optarg);
				if (n >= RTE_MAX_ETHPORTS)
					rte_exit(EXIT_FAILURE,
						 "eth-peer: port %d >= RTE_MAX_ETHPORTS(%d)\n",
						 n, RTE_MAX_ETHPORTS);

				if (rte_ether_unformat_addr(port_end,
						&peer_eth_addrs[n]) < 0)
					rte_exit(EXIT_FAILURE,
						 "Invalid ethernet address: %s\n",
						 port_end);
				nb_peer_eth_addrs++;
			}
#endif
			if (!strcmp(lgopts[opt_idx].name, "tx-ip")) {
				struct in_addr in;
				char *end;

				end = strchr(optarg, ',');
				if (end == optarg || !end)
					rte_exit(EXIT_FAILURE,
						 "Invalid tx-ip: %s", optarg);

				*end++ = 0;
				if (inet_pton(AF_INET, optarg, &in) == 0)
					rte_exit(EXIT_FAILURE,
						 "Invalid source IP address: %s\n",
						 optarg);
				tx_ip_src_addr = rte_be_to_cpu_32(in.s_addr);

				if (inet_pton(AF_INET, end, &in) == 0)
					rte_exit(EXIT_FAILURE,
						 "Invalid destination IP address: %s\n",
						 optarg);
				tx_ip_dst_addr = rte_be_to_cpu_32(in.s_addr);
			}
			if (!strcmp(lgopts[opt_idx].name, "tx-udp")) {
				char *end = NULL;

				errno = 0;
				n = strtoul(optarg, &end, 10);
				if (errno != 0 || end == optarg ||
				    n > UINT16_MAX ||
				    !(*end == '\0' || *end == ','))
					rte_exit(EXIT_FAILURE,
						 "Invalid UDP port: %s\n",
						 optarg);
				tx_udp_src_port = n;
				if (*end == ',') {
					char *dst = end + 1;

					n = strtoul(dst, &end, 10);
					if (errno != 0 || end == dst ||
					    n > UINT16_MAX || *end)
						rte_exit(EXIT_FAILURE,
							 "Invalid destination UDP port: %s\n",
							 dst);
					tx_udp_dst_port = n;
				} else {
					tx_udp_dst_port = n;
				}

			}
			if (!strcmp(lgopts[opt_idx].name, "nb-ports")) {
				n = atoi(optarg);
				if (n > 0 && n <= nb_ports)
					nb_fwd_ports = n;
				else
					rte_exit(EXIT_FAILURE,
						 "Invalid port %d\n", n);
			}
			if (!strcmp(lgopts[opt_idx].name, "nb-cores")) {
				n = atoi(optarg);
				if (n > 0 && n <= nb_lcores)
					nb_fwd_lcores = (uint8_t) n;
				else
					rte_exit(EXIT_FAILURE,
						 "nb-cores should be > 0 and <= %d\n",
						 nb_lcores);
			}
			if (!strcmp(lgopts[opt_idx].name, "coremask"))
				parse_fwd_coremask(optarg);
			if (!strcmp(lgopts[opt_idx].name, "portmask"))
				parse_fwd_portmask(optarg);
			if (!strcmp(lgopts[opt_idx].name, "portlist"))
				parse_fwd_portlist(optarg);
			if (!strcmp(lgopts[opt_idx].name, "no-numa"))
				numa_support = 0;
			if (!strcmp(lgopts[opt_idx].name, "numa"))
				numa_support = 1;
			if (!strcmp(lgopts[opt_idx].name, "mp-anon")) {
				mp_alloc_type = MP_ALLOC_ANON;
			}
			if (!strcmp(lgopts[opt_idx].name, "mp-alloc")) {
				if (!strcmp(optarg, "native"))
					mp_alloc_type = MP_ALLOC_NATIVE;
				else if (!strcmp(optarg, "anon"))
					mp_alloc_type = MP_ALLOC_ANON;
				else if (!strcmp(optarg, "xmem"))
					mp_alloc_type = MP_ALLOC_XMEM;
				else if (!strcmp(optarg, "xmemhuge"))
					mp_alloc_type = MP_ALLOC_XMEM_HUGE;
				else if (!strcmp(optarg, "xbuf"))
					mp_alloc_type = MP_ALLOC_XBUF;
				else
					rte_exit(EXIT_FAILURE,
						"mp-alloc %s invalid - must be: "
						"native, anon, xmem or xmemhuge\n",
						 optarg);
			}
			if (!strcmp(lgopts[opt_idx].name, "port-numa-config")) {
				if (parse_portnuma_config(optarg))
					rte_exit(EXIT_FAILURE,
					   "invalid port-numa configuration\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "ring-numa-config"))
				if (parse_ringnuma_config(optarg))
					rte_exit(EXIT_FAILURE,
					   "invalid ring-numa configuration\n");
			if (!strcmp(lgopts[opt_idx].name, "socket-num")) {
				n = atoi(optarg);
				if (!new_socket_id((uint8_t)n)) {
					socket_num = (uint8_t)n;
				} else {
					print_invalid_socket_id_error();
					rte_exit(EXIT_FAILURE,
						"Invalid socket id");
				}
			}
			if (!strcmp(lgopts[opt_idx].name, "mbuf-size")) {
				unsigned int mb_sz[MAX_SEGS_BUFFER_SPLIT];
				unsigned int nb_segs, i;

				nb_segs = parse_item_list(optarg, "mbuf-size",
					MAX_SEGS_BUFFER_SPLIT, mb_sz, 0);
				if (nb_segs <= 0)
					rte_exit(EXIT_FAILURE,
						 "bad mbuf-size\n");
				for (i = 0; i < nb_segs; i++) {
					if (mb_sz[i] <= 0 || mb_sz[i] > 0xFFFF)
						rte_exit(EXIT_FAILURE,
							 "mbuf-size should be "
							 "> 0 and < 65536\n");
					mbuf_data_size[i] = (uint16_t) mb_sz[i];
				}
				mbuf_data_size_n = nb_segs;
			}
			if (!strcmp(lgopts[opt_idx].name, "total-num-mbufs")) {
				n = atoi(optarg);
				if (n > MIN_TOTAL_NUM_MBUFS)
					param_total_num_mbufs = (unsigned)n;
				else
					rte_exit(EXIT_FAILURE,
						 "total-num-mbufs should be > %d\n",
						 MIN_TOTAL_NUM_MBUFS);
			}
			if (!strcmp(lgopts[opt_idx].name, "max-pkt-len")) {
				n = atoi(optarg);
				if (n >= RTE_ETHER_MIN_LEN)
					max_rx_pkt_len = n;
				else
					rte_exit(EXIT_FAILURE,
						 "Invalid max-pkt-len=%d - should be > %d\n",
						 n, RTE_ETHER_MIN_LEN);
			}
			if (!strcmp(lgopts[opt_idx].name, "max-lro-pkt-size")) {
				n = atoi(optarg);
				rx_mode.max_lro_pkt_size = (uint32_t) n;
			}
#ifdef RTE_LIB_LATENCYSTATS
			if (!strcmp(lgopts[opt_idx].name,
				    "latencystats")) {
				n = atoi(optarg);
				if (n >= 0) {
					latencystats_lcore_id = (lcoreid_t) n;
					latencystats_enabled = 1;
				} else
					rte_exit(EXIT_FAILURE,
						 "invalid lcore id %d for latencystats"
						 " must be >= 0\n", n);
			}
#endif
#ifdef RTE_LIB_BITRATESTATS
			if (!strcmp(lgopts[opt_idx].name, "bitrate-stats")) {
				n = atoi(optarg);
				if (n >= 0) {
					bitrate_lcore_id = (lcoreid_t) n;
					bitrate_enabled = 1;
				} else
					rte_exit(EXIT_FAILURE,
						 "invalid lcore id %d for bitrate stats"
						 " must be >= 0\n", n);
			}
#endif
			if (!strcmp(lgopts[opt_idx].name, "disable-crc-strip"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_KEEP_CRC;
			if (!strcmp(lgopts[opt_idx].name, "enable-lro"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_TCP_LRO;
			if (!strcmp(lgopts[opt_idx].name, "enable-scatter"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
			if (!strcmp(lgopts[opt_idx].name, "enable-rx-cksum"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_CHECKSUM;
			if (!strcmp(lgopts[opt_idx].name,
					"enable-rx-timestamp"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
			if (!strcmp(lgopts[opt_idx].name, "enable-hw-vlan"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN;

			if (!strcmp(lgopts[opt_idx].name,
					"enable-hw-vlan-filter"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;

			if (!strcmp(lgopts[opt_idx].name,
					"enable-hw-vlan-strip"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

			if (!strcmp(lgopts[opt_idx].name,
					"enable-hw-vlan-extend"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;

			if (!strcmp(lgopts[opt_idx].name,
					"enable-hw-qinq-strip"))
				rx_offloads |= RTE_ETH_RX_OFFLOAD_QINQ_STRIP;

			if (!strcmp(lgopts[opt_idx].name, "enable-drop-en"))
				rx_drop_en = 1;

			if (!strcmp(lgopts[opt_idx].name, "disable-rss"))
				rss_hf = 0;
			if (!strcmp(lgopts[opt_idx].name, "port-topology")) {
				if (!strcmp(optarg, "paired"))
					port_topology = PORT_TOPOLOGY_PAIRED;
				else if (!strcmp(optarg, "chained"))
					port_topology = PORT_TOPOLOGY_CHAINED;
				else if (!strcmp(optarg, "loop"))
					port_topology = PORT_TOPOLOGY_LOOP;
				else
					rte_exit(EXIT_FAILURE, "port-topology %s invalid -"
						 " must be: paired, chained or loop\n",
						 optarg);
			}
			if (!strcmp(lgopts[opt_idx].name, "forward-mode"))
				set_pkt_forwarding_mode(optarg);
			if (!strcmp(lgopts[opt_idx].name, "rss-ip"))
				rss_hf = RTE_ETH_RSS_IP;
			if (!strcmp(lgopts[opt_idx].name, "rss-udp"))
				rss_hf = RTE_ETH_RSS_UDP;
			if (!strcmp(lgopts[opt_idx].name, "rss-level-inner"))
				rss_hf |= RTE_ETH_RSS_LEVEL_INNERMOST;
			if (!strcmp(lgopts[opt_idx].name, "rss-level-outer"))
				rss_hf |= RTE_ETH_RSS_LEVEL_OUTERMOST;
			if (!strcmp(lgopts[opt_idx].name, "rxq")) {
				n = atoi(optarg);
				if (n >= 0 && check_nb_rxq((queueid_t)n) == 0)
					nb_rxq = (queueid_t) n;
				else
					rte_exit(EXIT_FAILURE, "rxq %d invalid - must be"
						  " >= 0 && <= %u\n", n,
						  get_allowed_max_nb_rxq(&pid));
			}
			if (!strcmp(lgopts[opt_idx].name, "txq")) {
				n = atoi(optarg);
				if (n >= 0 && check_nb_txq((queueid_t)n) == 0)
					nb_txq = (queueid_t) n;
				else
					rte_exit(EXIT_FAILURE, "txq %d invalid - must be"
						  " >= 0 && <= %u\n", n,
						  get_allowed_max_nb_txq(&pid));
			}
			if (!strcmp(lgopts[opt_idx].name, "hairpinq")) {
				n = atoi(optarg);
				if (n >= 0 &&
				    check_nb_hairpinq((queueid_t)n) == 0)
					nb_hairpinq = (queueid_t) n;
				else
					rte_exit(EXIT_FAILURE, "txq %d invalid - must be"
						  " >= 0 && <= %u\n", n,
						  get_allowed_max_nb_hairpinq
						  (&pid));
				if ((n + nb_txq) < 0 ||
				    check_nb_txq((queueid_t)(n + nb_txq)) != 0)
					rte_exit(EXIT_FAILURE, "txq + hairpinq "
						 "%d invalid - must be"
						  " >= 0 && <= %u\n",
						  n + nb_txq,
						  get_allowed_max_nb_txq(&pid));
				if ((n + nb_rxq) < 0 ||
				    check_nb_rxq((queueid_t)(n + nb_rxq)) != 0)
					rte_exit(EXIT_FAILURE, "rxq + hairpinq "
						 "%d invalid - must be"
						  " >= 0 && <= %u\n",
						  n + nb_rxq,
						  get_allowed_max_nb_rxq(&pid));
			}
			if (!nb_rxq && !nb_txq) {
				rte_exit(EXIT_FAILURE, "Either rx or tx queues should "
						"be non-zero\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "hairpin-mode")) {
				char *end = NULL;
				unsigned int n;

				errno = 0;
				n = strtoul(optarg, &end, 0);
				if (errno != 0 || end == optarg)
					rte_exit(EXIT_FAILURE, "hairpin mode invalid\n");
				else
					hairpin_mode = (uint32_t)n;
			}
			if (!strcmp(lgopts[opt_idx].name, "burst")) {
				n = atoi(optarg);
				if (n == 0) {
					/* A burst size of zero means that the
					 * PMD should be queried for
					 * recommended Rx burst size. Since
					 * testpmd uses a single size for all
					 * ports, port 0 is queried for the
					 * value, on the assumption that all
					 * ports are of the same NIC model.
					 */
					ret = eth_dev_info_get_print_err(
								0,
								&dev_info);
					if (ret != 0)
						return;

					rec_nb_pkts = dev_info
						.default_rxportconf.burst_size;

					if (rec_nb_pkts == 0)
						rte_exit(EXIT_FAILURE,
							"PMD does not recommend a burst size. "
							"Provided value must be between "
							"1 and %d\n", MAX_PKT_BURST);
					else if (rec_nb_pkts > MAX_PKT_BURST)
						rte_exit(EXIT_FAILURE,
							"PMD recommended burst size of %d"
							" exceeds maximum value of %d\n",
							rec_nb_pkts, MAX_PKT_BURST);
					printf("Using PMD-provided burst value of %d\n",
						rec_nb_pkts);
					nb_pkt_per_burst = rec_nb_pkts;
				} else if (n > MAX_PKT_BURST)
					rte_exit(EXIT_FAILURE,
						"burst must be between1 and %d\n",
						MAX_PKT_BURST);
				else
					nb_pkt_per_burst = (uint16_t) n;
			}
			if (!strcmp(lgopts[opt_idx].name, "flowgen-clones")) {
				n = atoi(optarg);
				if (n >= 0)
					nb_pkt_flowgen_clones = (uint16_t) n;
				else
					rte_exit(EXIT_FAILURE,
						 "clones must be >= 0 and <= current burst\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "flowgen-flows")) {
				n = atoi(optarg);
				if (n > 0)
					nb_flows_flowgen = (int) n;
				else
					rte_exit(EXIT_FAILURE,
						 "flows must be >= 1\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "mbcache")) {
				n = atoi(optarg);
				if ((n >= 0) &&
				    (n <= RTE_MEMPOOL_CACHE_MAX_SIZE))
					mb_mempool_cache = (uint16_t) n;
				else
					rte_exit(EXIT_FAILURE,
						 "mbcache must be >= 0 and <= %d\n",
						 RTE_MEMPOOL_CACHE_MAX_SIZE);
			}
			if (!strcmp(lgopts[opt_idx].name, "txfreet")) {
				n = atoi(optarg);
				if (n >= 0)
					tx_free_thresh = (int16_t)n;
				else
					rte_exit(EXIT_FAILURE, "txfreet must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "txrst")) {
				n = atoi(optarg);
				if (n >= 0)
					tx_rs_thresh = (int16_t)n;
				else
					rte_exit(EXIT_FAILURE, "txrst must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxd")) {
				n = atoi(optarg);
				if (n > 0) {
					if (rx_free_thresh >= n)
						rte_exit(EXIT_FAILURE,
							 "rxd must be > "
							 "rx_free_thresh(%d)\n",
							 (int)rx_free_thresh);
					else
						nb_rxd = (uint16_t) n;
				} else
					rte_exit(EXIT_FAILURE,
						 "rxd(%d) invalid - must be > 0\n",
						 n);
			}
			if (!strcmp(lgopts[opt_idx].name, "txd")) {
				n = atoi(optarg);
				if (n > 0)
					nb_txd = (uint16_t) n;
				else
					rte_exit(EXIT_FAILURE, "txd must be in > 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "txpt")) {
				n = atoi(optarg);
				if (n >= 0)
					tx_pthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "txpt must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "txht")) {
				n = atoi(optarg);
				if (n >= 0)
					tx_hthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "txht must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "txwt")) {
				n = atoi(optarg);
				if (n >= 0)
					tx_wthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "txwt must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxpt")) {
				n = atoi(optarg);
				if (n >= 0)
					rx_pthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "rxpt must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxht")) {
				n = atoi(optarg);
				if (n >= 0)
					rx_hthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "rxht must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxwt")) {
				n = atoi(optarg);
				if (n >= 0)
					rx_wthresh = (int8_t)n;
				else
					rte_exit(EXIT_FAILURE, "rxwt must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxfreet")) {
				n = atoi(optarg);
				if (n >= 0)
					rx_free_thresh = (int16_t)n;
				else
					rte_exit(EXIT_FAILURE, "rxfreet must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxoffs")) {
				unsigned int seg_off[MAX_SEGS_BUFFER_SPLIT];
				unsigned int nb_offs;

				nb_offs = parse_item_list
						(optarg, "rxpkt offsets",
						 MAX_SEGS_BUFFER_SPLIT,
						 seg_off, 0);
				if (nb_offs > 0)
					set_rx_pkt_offsets(seg_off, nb_offs);
				else
					rte_exit(EXIT_FAILURE, "bad rxoffs\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxpkts")) {
				unsigned int seg_len[MAX_SEGS_BUFFER_SPLIT];
				unsigned int nb_segs;
				nb_segs = parse_item_list
						(optarg, "rxpkt segments",
						 MAX_SEGS_BUFFER_SPLIT,
						 seg_len, 0);
				if (nb_segs > 0)
					set_rx_pkt_segments(seg_len, nb_segs);
				else
					rte_exit(EXIT_FAILURE, "bad rxpkts\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "rxhdrs")) {
				unsigned int seg_hdrs[MAX_SEGS_BUFFER_SPLIT];
				unsigned int nb_segs;

				nb_segs = parse_hdrs_list
						(optarg, "rxpkt segments",
						MAX_SEGS_BUFFER_SPLIT,
						seg_hdrs);
				if (nb_segs > 0)
					set_rx_pkt_hdrs(seg_hdrs, nb_segs);
				else
					rte_exit(EXIT_FAILURE, "bad rxpkts\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "txpkts")) {
				unsigned seg_lengths[RTE_MAX_SEGS_PER_PKT];
				unsigned int nb_segs;

				nb_segs = parse_item_list(optarg, "txpkt segments",
						RTE_MAX_SEGS_PER_PKT, seg_lengths, 0);
				if (nb_segs > 0)
					set_tx_pkt_segments(seg_lengths, nb_segs);
				else
					rte_exit(EXIT_FAILURE, "bad txpkts\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "multi-rx-mempool"))
				multi_rx_mempool = 1;
			if (!strcmp(lgopts[opt_idx].name, "txonly-multi-flow"))
				txonly_multi_flow = 1;
			if (!strcmp(lgopts[opt_idx].name, "rxq-share")) {
				if (optarg == NULL) {
					rxq_share = UINT32_MAX;
				} else {
					n = atoi(optarg);
					if (n >= 0)
						rxq_share = (uint32_t)n;
					else
						rte_exit(EXIT_FAILURE, "rxq-share must be >= 0\n");
				}
			}
			if (!strcmp(lgopts[opt_idx].name, "no-flush-rx"))
				no_flush_rx = 1;
			if (!strcmp(lgopts[opt_idx].name, "eth-link-speed")) {
				n = atoi(optarg);
				if (n >= 0 && parse_link_speed(n) > 0)
					eth_link_speed = parse_link_speed(n);
			}
			if (!strcmp(lgopts[opt_idx].name, "disable-link-check"))
				no_link_check = 1;
			if (!strcmp(lgopts[opt_idx].name, "disable-device-start"))
				no_device_start = 1;
			if (!strcmp(lgopts[opt_idx].name, "no-lsc-interrupt"))
				lsc_interrupt = 0;
			if (!strcmp(lgopts[opt_idx].name, "no-rmv-interrupt"))
				rmv_interrupt = 0;
			if (!strcmp(lgopts[opt_idx].name, "flow-isolate-all"))
				flow_isolate_all = 1;
			if (!strcmp(lgopts[opt_idx].name, "disable-flow-flush"))
				no_flow_flush = 1;
			if (!strcmp(lgopts[opt_idx].name, "tx-offloads")) {
				char *end = NULL;
				n = strtoull(optarg, &end, 16);
				if (n >= 0)
					tx_offloads = (uint64_t)n;
				else
					rte_exit(EXIT_FAILURE,
						 "tx-offloads must be >= 0\n");
			}

			if (!strcmp(lgopts[opt_idx].name, "rx-offloads")) {
				char *end = NULL;
				n = strtoull(optarg, &end, 16);
				if (n >= 0)
					rx_offloads = (uint64_t)n;
				else
					rte_exit(EXIT_FAILURE,
						 "rx-offloads must be >= 0\n");
			}

			if (!strcmp(lgopts[opt_idx].name, "vxlan-gpe-port")) {
				n = atoi(optarg);
				if (n >= 0)
					vxlan_gpe_udp_port = (uint16_t)n;
				else
					rte_exit(EXIT_FAILURE,
						 "vxlan-gpe-port must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "geneve-parsed-port")) {
				n = atoi(optarg);
				if (n >= 0)
					geneve_udp_port = (uint16_t)n;
				else
					rte_exit(EXIT_FAILURE,
						 "geneve-parsed-port must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name, "print-event"))
				if (parse_event_printing_config(optarg, 1)) {
					rte_exit(EXIT_FAILURE,
						 "invalid print-event argument\n");
				}
			if (!strcmp(lgopts[opt_idx].name, "mask-event"))
				if (parse_event_printing_config(optarg, 0)) {
					rte_exit(EXIT_FAILURE,
						 "invalid mask-event argument\n");
				}
			if (!strcmp(lgopts[opt_idx].name, "hot-plug"))
				hot_plug = 1;
			if (!strcmp(lgopts[opt_idx].name, "mlockall"))
				do_mlockall = 1;
			if (!strcmp(lgopts[opt_idx].name, "no-mlockall"))
				do_mlockall = 0;
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-tx-sw-buffer-size")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_tx_sw_bufsz = n;
				else
					rte_exit(EXIT_FAILURE,
						"noisy-tx-sw-buffer-size must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-tx-sw-buffer-flushtime")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_tx_sw_buf_flush_time = n;
				else
					rte_exit(EXIT_FAILURE,
						 "noisy-tx-sw-buffer-flushtime must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-lkup-memory")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_lkup_mem_sz = n;
				else
					rte_exit(EXIT_FAILURE,
						 "noisy-lkup-memory must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-lkup-num-writes")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_lkup_num_writes = n;
				else
					rte_exit(EXIT_FAILURE,
						 "noisy-lkup-num-writes must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-lkup-num-reads")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_lkup_num_reads = n;
				else
					rte_exit(EXIT_FAILURE,
						 "noisy-lkup-num-reads must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-lkup-num-reads-writes")) {
				n = atoi(optarg);
				if (n >= 0)
					noisy_lkup_num_reads_writes = n;
				else
					rte_exit(EXIT_FAILURE,
						 "noisy-lkup-num-reads-writes must be >= 0\n");
			}
			if (!strcmp(lgopts[opt_idx].name,
				    "noisy-forward-mode")) {
				int i;
				for (i = 0; i < NOISY_FWD_MODE_MAX; i++)
					if (!strcmp(optarg, noisy_fwd_mode_desc[i])) {
						noisy_fwd_mode = i;
						break;
					}
				if (i == NOISY_FWD_MODE_MAX)
					rte_exit(EXIT_FAILURE, "noisy-forward-mode %s invalid,"
						 " must be a valid noisy-forward-mode value\n",
						 optarg);
			}
			if (!strcmp(lgopts[opt_idx].name, "no-iova-contig"))
				mempool_flags = RTE_MEMPOOL_F_NO_IOVA_CONTIG;

			if (!strcmp(lgopts[opt_idx].name, "rx-mq-mode")) {
				char *end = NULL;
				n = strtoul(optarg, &end, 16);
				if (n >= 0 && n <= RTE_ETH_MQ_RX_VMDQ_DCB_RSS)
					rx_mq_mode = (enum rte_eth_rx_mq_mode)n;
				else
					rte_exit(EXIT_FAILURE,
						 "rx-mq-mode must be >= 0 and <= %d\n",
						 RTE_ETH_MQ_RX_VMDQ_DCB_RSS);
			}
			if (!strcmp(lgopts[opt_idx].name, "record-core-cycles"))
				record_core_cycles = 1;
			if (!strcmp(lgopts[opt_idx].name, "record-burst-stats"))
				record_burst_stats = 1;
			if (!strcmp(lgopts[opt_idx].name, PARAM_NUM_PROCS))
				num_procs = atoi(optarg);
			if (!strcmp(lgopts[opt_idx].name, PARAM_PROC_ID))
				proc_id = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage(argv[0]);
			fprintf(stderr, "Invalid option: %s\n", argv[optind]);
			rte_exit(EXIT_FAILURE,
				 "Command line is incomplete or incorrect\n");
			break;
		}
	}

	if (optind != argc) {
		usage(argv[0]);
		fprintf(stderr, "Invalid parameter: %s\n", argv[optind]);
		rte_exit(EXIT_FAILURE, "Command line is incorrect\n");
	}

	if (proc_id >= (int)num_procs)
		rte_exit(EXIT_FAILURE,
			 "The multi-process option '%s(%d)' should be less than '%s(%u)'\n",
			 PARAM_PROC_ID, proc_id,
			 PARAM_NUM_PROCS, num_procs);

	/* Set offload configuration from command line parameters. */
	rx_mode.offloads = rx_offloads;
	tx_mode.offloads = tx_offloads;

	if (mempool_flags & RTE_MEMPOOL_F_NO_IOVA_CONTIG &&
	    mp_alloc_type != MP_ALLOC_ANON) {
		TESTPMD_LOG(WARNING, "cannot use no-iova-contig without "
				  "mp-alloc=anon. mempool no-iova-contig is "
				  "ignored\n");
		mempool_flags = 0;
	}
}
