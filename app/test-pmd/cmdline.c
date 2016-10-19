/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#ifndef __FreeBSD__
#include <net/socket.h>
#else
#include <sys/socket.h>
#endif
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
#include <rte_malloc.h>
#include <rte_launch.h>
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
#include <rte_devargs.h>
#include <rte_eth_ctrl.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#ifdef RTE_LIBRTE_PMD_BOND
#include <rte_eth_bond.h>
#endif
#ifdef RTE_LIBRTE_IXGBE_PMD
#include <rte_pmd_ixgbe.h>
#endif
#include "testpmd.h"

static struct cmdline *testpmd_cl;

static void cmd_reconfig_device_queue(portid_t id, uint8_t dev, uint8_t queue);

/* *** Help command with introduction. *** */
struct cmd_help_brief_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_brief_parsed(__attribute__((unused)) void *parsed_result,
                                  struct cmdline *cl,
                                  __attribute__((unused)) void *data)
{
	cmdline_printf(
		cl,
		"\n"
		"Help is available for the following sections:\n\n"
		"    help control    : Start and stop forwarding.\n"
		"    help display    : Displaying port, stats and config "
		"information.\n"
		"    help config     : Configuration information.\n"
		"    help ports      : Configuring ports.\n"
		"    help registers  : Reading and setting port registers.\n"
		"    help filters    : Filters configuration help.\n"
		"    help all        : All of the above sections.\n\n"
	);

}

cmdline_parse_token_string_t cmd_help_brief_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_brief_result, help, "help");

cmdline_parse_inst_t cmd_help_brief = {
	.f = cmd_help_brief_parsed,
	.data = NULL,
	.help_str = "show help",
	.tokens = {
		(void *)&cmd_help_brief_help,
		NULL,
	},
};

/* *** Help command with help sections. *** */
struct cmd_help_long_result {
	cmdline_fixed_string_t help;
	cmdline_fixed_string_t section;
};

static void cmd_help_long_parsed(void *parsed_result,
                                 struct cmdline *cl,
                                 __attribute__((unused)) void *data)
{
	int show_all = 0;
	struct cmd_help_long_result *res = parsed_result;

	if (!strcmp(res->section, "all"))
		show_all = 1;

	if (show_all || !strcmp(res->section, "control")) {

		cmdline_printf(
			cl,
			"\n"
			"Control forwarding:\n"
			"-------------------\n\n"

			"start\n"
			"    Start packet forwarding with current configuration.\n\n"

			"start tx_first\n"
			"    Start packet forwarding with current config"
			" after sending one burst of packets.\n\n"

			"stop\n"
			"    Stop packet forwarding, and display accumulated"
			" statistics.\n\n"

			"quit\n"
			"    Quit to prompt.\n\n"
		);
	}

	if (show_all || !strcmp(res->section, "display")) {

		cmdline_printf(
			cl,
			"\n"
			"Display:\n"
			"--------\n\n"

			"show port (info|stats|xstats|fdir|stat_qmap|dcb_tc) (port_id|all)\n"
			"    Display information for port_id, or all.\n\n"

			"show port X rss reta (size) (mask0,mask1,...)\n"
			"    Display the rss redirection table entry indicated"
			" by masks on port X. size is used to indicate the"
			" hardware supported reta size\n\n"

			"show port rss-hash ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|"
			"ipv4-sctp|ipv4-other|ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|"
			"ipv6-other|l2-payload|ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex [key]\n"
			"    Display the RSS hash functions and RSS hash key"
			" of port X\n\n"

			"clear port (info|stats|xstats|fdir|stat_qmap) (port_id|all)\n"
			"    Clear information for port_id, or all.\n\n"

			"show (rxq|txq) info (port_id) (queue_id)\n"
			"    Display information for configured RX/TX queue.\n\n"

			"show config (rxtx|cores|fwd|txpkts)\n"
			"    Display the given configuration.\n\n"

			"read rxd (port_id) (queue_id) (rxd_id)\n"
			"    Display an RX descriptor of a port RX queue.\n\n"

			"read txd (port_id) (queue_id) (txd_id)\n"
			"    Display a TX descriptor of a port TX queue.\n\n"
		);
	}

	if (show_all || !strcmp(res->section, "config")) {
		cmdline_printf(
			cl,
			"\n"
			"Configuration:\n"
			"--------------\n"
			"Configuration changes only become active when"
			" forwarding is started/restarted.\n\n"

			"set default\n"
			"    Reset forwarding to the default configuration.\n\n"

			"set verbose (level)\n"
			"    Set the debug verbosity level X.\n\n"

			"set nbport (num)\n"
			"    Set number of ports.\n\n"

			"set nbcore (num)\n"
			"    Set number of cores.\n\n"

			"set coremask (mask)\n"
			"    Set the forwarding cores hexadecimal mask.\n\n"

			"set portmask (mask)\n"
			"    Set the forwarding ports hexadecimal mask.\n\n"

			"set burst (num)\n"
			"    Set number of packets per burst.\n\n"

			"set burst tx delay (microseconds) retry (num)\n"
			"    Set the transmit delay time and number of retries,"
			" effective when retry is enabled.\n\n"

			"set txpkts (x[,y]*)\n"
			"    Set the length of each segment of TXONLY"
			" and optionally CSUM packets.\n\n"

			"set txsplit (off|on|rand)\n"
			"    Set the split policy for the TX packets."
			" Right now only applicable for CSUM and TXONLY"
			" modes\n\n"

			"set corelist (x[,y]*)\n"
			"    Set the list of forwarding cores.\n\n"

			"set portlist (x[,y]*)\n"
			"    Set the list of forwarding ports.\n\n"

#ifdef RTE_LIBRTE_IXGBE_PMD
			"set tx loopback (port_id) (on|off)\n"
			"    Enable or disable tx loopback.\n\n"

			"set all queues drop (port_id) (on|off)\n"
			"    Set drop enable bit for all queues.\n\n"

			"set vf split drop (port_id) (vf_id) (on|off)\n"
			"    Set split drop enable bit for a VF from the PF.\n\n"

			"set vf mac antispoof (port_id) (vf_id) (on|off).\n"
			"    Set MAC antispoof for a VF from the PF.\n\n"
#endif

			"vlan set strip (on|off) (port_id)\n"
			"    Set the VLAN strip on a port.\n\n"

			"vlan set stripq (on|off) (port_id,queue_id)\n"
			"    Set the VLAN strip for a queue on a port.\n\n"

#ifdef RTE_LIBRTE_IXGBE_PMD
			"set vf vlan stripq (port_id) (vf_id) (on|off)\n"
			"    Set the VLAN strip for all queues in a pool for a VF from the PF.\n\n"

			"set vf vlan insert (port_id) (vf_id) (vlan_id)\n"
			"    Set VLAN insert for a VF from the PF.\n\n"

			"set vf vlan antispoof (port_id) (vf_id) (on|off)\n"
			"    Set VLAN antispoof for a VF from the PF.\n\n"
#endif

			"vlan set filter (on|off) (port_id)\n"
			"    Set the VLAN filter on a port.\n\n"

			"vlan set qinq (on|off) (port_id)\n"
			"    Set the VLAN QinQ (extended queue in queue)"
			" on a port.\n\n"

			"vlan set (inner|outer) tpid (value) (port_id)\n"
			"    Set the VLAN TPID for Packet Filtering on"
			" a port\n\n"

			"rx_vlan add (vlan_id|all) (port_id)\n"
			"    Add a vlan_id, or all identifiers, to the set"
			" of VLAN identifiers filtered by port_id.\n\n"

			"rx_vlan rm (vlan_id|all) (port_id)\n"
			"    Remove a vlan_id, or all identifiers, from the set"
			" of VLAN identifiers filtered by port_id.\n\n"

			"rx_vlan add (vlan_id) port (port_id) vf (vf_mask)\n"
			"    Add a vlan_id, to the set of VLAN identifiers"
			"filtered for VF(s) from port_id.\n\n"

			"rx_vlan rm (vlan_id) port (port_id) vf (vf_mask)\n"
			"    Remove a vlan_id, to the set of VLAN identifiers"
			"filtered for VF(s) from port_id.\n\n"

			"tunnel_filter add (port_id) (outer_mac) (inner_mac) (ip_addr) "
			"(inner_vlan) (vxlan|nvgre|ipingre) (imac-ivlan|imac-ivlan-tenid|"
			"imac-tenid|imac|omac-imac-tenid|oip|iip) (tenant_id) (queue_id)\n"
			"   add a tunnel filter of a port.\n\n"

			"tunnel_filter rm (port_id) (outer_mac) (inner_mac) (ip_addr) "
			"(inner_vlan) (vxlan|nvgre|ipingre) (imac-ivlan|imac-ivlan-tenid|"
			"imac-tenid|imac|omac-imac-tenid|oip|iip) (tenant_id) (queue_id)\n"
			"   remove a tunnel filter of a port.\n\n"

			"rx_vxlan_port add (udp_port) (port_id)\n"
			"    Add an UDP port for VXLAN packet filter on a port\n\n"

			"rx_vxlan_port rm (udp_port) (port_id)\n"
			"    Remove an UDP port for VXLAN packet filter on a port\n\n"

			"tx_vlan set (port_id) vlan_id[, vlan_id_outer]\n"
			"    Set hardware insertion of VLAN IDs (single or double VLAN "
			"depends on the number of VLAN IDs) in packets sent on a port.\n\n"

			"tx_vlan set pvid port_id vlan_id (on|off)\n"
			"    Set port based TX VLAN insertion.\n\n"

			"tx_vlan reset (port_id)\n"
			"    Disable hardware insertion of a VLAN header in"
			" packets sent on a port.\n\n"

			"csum set (ip|udp|tcp|sctp|outer-ip) (hw|sw) (port_id)\n"
			"    Select hardware or software calculation of the"
			" checksum when transmitting a packet using the"
			" csum forward engine.\n"
			"    ip|udp|tcp|sctp always concern the inner layer.\n"
			"    outer-ip concerns the outer IP layer in"
			" case the packet is recognized as a tunnel packet by"
			" the forward engine (vxlan, gre and ipip are supported)\n"
			"    Please check the NIC datasheet for HW limits.\n\n"

			"csum parse-tunnel (on|off) (tx_port_id)\n"
			"    If disabled, treat tunnel packets as non-tunneled"
			" packets (treat inner headers as payload). The port\n"
			"    argument is the port used for TX in csum forward"
			" engine.\n\n"

			"csum show (port_id)\n"
			"    Display tx checksum offload configuration\n\n"

			"tso set (segsize) (portid)\n"
			"    Enable TCP Segmentation Offload in csum forward"
			" engine.\n"
			"    Please check the NIC datasheet for HW limits.\n\n"

			"tso show (portid)"
			"    Display the status of TCP Segmentation Offload.\n\n"

			"set fwd (%s)\n"
			"    Set packet forwarding mode.\n\n"

			"mac_addr add (port_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Add a MAC address on port_id.\n\n"

			"mac_addr remove (port_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Remove a MAC address from port_id.\n\n"

			"mac_addr add port (port_id) vf (vf_id) (mac_address)\n"
			"    Add a MAC address for a VF on the port.\n\n"

#ifdef RTE_LIBRTE_IXGBE_PMD
			"set vf mac addr (port_id) (vf_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Set the MAC address for a VF from the PF.\n\n"
#endif

			"set port (port_id) uta (mac_address|all) (on|off)\n"
			"    Add/Remove a or all unicast hash filter(s)"
			"from port X.\n\n"

			"set promisc (port_id|all) (on|off)\n"
			"    Set the promiscuous mode on port_id, or all.\n\n"

			"set allmulti (port_id|all) (on|off)\n"
			"    Set the allmulti mode on port_id, or all.\n\n"

			"set flow_ctrl rx (on|off) tx (on|off) (high_water)"
			" (low_water) (pause_time) (send_xon) mac_ctrl_frame_fwd"
			" (on|off) autoneg (on|off) (port_id)\n"
			"set flow_ctrl rx (on|off) (portid)\n"
			"set flow_ctrl tx (on|off) (portid)\n"
			"set flow_ctrl high_water (high_water) (portid)\n"
			"set flow_ctrl low_water (low_water) (portid)\n"
			"set flow_ctrl pause_time (pause_time) (portid)\n"
			"set flow_ctrl send_xon (send_xon) (portid)\n"
			"set flow_ctrl mac_ctrl_frame_fwd (on|off) (portid)\n"
			"set flow_ctrl autoneg (on|off) (port_id)\n"
			"    Set the link flow control parameter on a port.\n\n"

			"set pfc_ctrl rx (on|off) tx (on|off) (high_water)"
			" (low_water) (pause_time) (priority) (port_id)\n"
			"    Set the priority flow control parameter on a"
			" port.\n\n"

			"set stat_qmap (tx|rx) (port_id) (queue_id) (qmapping)\n"
			"    Set statistics mapping (qmapping 0..15) for RX/TX"
			" queue on port.\n"
			"    e.g., 'set stat_qmap rx 0 2 5' sets rx queue 2"
			" on port 0 to mapping 5.\n\n"

			"set port (port_id) vf (vf_id) rx|tx on|off\n"
			"    Enable/Disable a VF receive/tranmit from a port\n\n"

			"set port (port_id) vf (vf_id) (mac_addr)"
			" (exact-mac#exact-mac-vlan#hashmac|hashmac-vlan) on|off\n"
			"   Add/Remove unicast or multicast MAC addr filter"
			" for a VF.\n\n"

			"set port (port_id) vf (vf_id) rxmode (AUPE|ROPE|BAM"
			"|MPE) (on|off)\n"
			"    AUPE:accepts untagged VLAN;"
			"ROPE:accept unicast hash\n\n"
			"    BAM:accepts broadcast packets;"
			"MPE:accepts all multicast packets\n\n"
			"    Enable/Disable a VF receive mode of a port\n\n"

			"set port (port_id) queue (queue_id) rate (rate_num)\n"
			"    Set rate limit for a queue of a port\n\n"

			"set port (port_id) vf (vf_id) rate (rate_num) "
			"queue_mask (queue_mask_value)\n"
			"    Set rate limit for queues in VF of a port\n\n"

			"set port (port_id) mirror-rule (rule_id)"
			" (pool-mirror-up|pool-mirror-down|vlan-mirror)"
			" (poolmask|vlanid[,vlanid]*) dst-pool (pool_id) (on|off)\n"
			"   Set pool or vlan type mirror rule on a port.\n"
			"   e.g., 'set port 0 mirror-rule 0 vlan-mirror 0,1"
			" dst-pool 0 on' enable mirror traffic with vlan 0,1"
			" to pool 0.\n\n"

			"set port (port_id) mirror-rule (rule_id)"
			" (uplink-mirror|downlink-mirror) dst-pool"
			" (pool_id) (on|off)\n"
			"   Set uplink or downlink type mirror rule on a port.\n"
			"   e.g., 'set port 0 mirror-rule 0 uplink-mirror dst-pool"
			" 0 on' enable mirror income traffic to pool 0.\n\n"

			"reset port (port_id) mirror-rule (rule_id)\n"
			"   Reset a mirror rule.\n\n"

			"set flush_rx (on|off)\n"
			"   Flush (default) or don't flush RX streams before"
			" forwarding. Mainly used with PCAP drivers.\n\n"

			#ifdef RTE_NIC_BYPASS
			"set bypass mode (normal|bypass|isolate) (port_id)\n"
			"   Set the bypass mode for the lowest port on bypass enabled"
			" NIC.\n\n"

			"set bypass event (timeout|os_on|os_off|power_on|power_off) "
			"mode (normal|bypass|isolate) (port_id)\n"
			"   Set the event required to initiate specified bypass mode for"
			" the lowest port on a bypass enabled NIC where:\n"
			"       timeout   = enable bypass after watchdog timeout.\n"
			"       os_on     = enable bypass when OS/board is powered on.\n"
			"       os_off    = enable bypass when OS/board is powered off.\n"
			"       power_on  = enable bypass when power supply is turned on.\n"
			"       power_off = enable bypass when power supply is turned off."
			"\n\n"

			"set bypass timeout (0|1.5|2|3|4|8|16|32)\n"
			"   Set the bypass watchdog timeout to 'n' seconds"
			" where 0 = instant.\n\n"

			"show bypass config (port_id)\n"
			"   Show the bypass configuration for a bypass enabled NIC"
			" using the lowest port on the NIC.\n\n"
#endif
#ifdef RTE_LIBRTE_PMD_BOND
			"create bonded device (mode) (socket)\n"
			"	Create a new bonded device with specific bonding mode and socket.\n\n"

			"add bonding slave (slave_id) (port_id)\n"
			"	Add a slave device to a bonded device.\n\n"

			"remove bonding slave (slave_id) (port_id)\n"
			"	Remove a slave device from a bonded device.\n\n"

			"set bonding mode (value) (port_id)\n"
			"	Set the bonding mode on a bonded device.\n\n"

			"set bonding primary (slave_id) (port_id)\n"
			"	Set the primary slave for a bonded device.\n\n"

			"show bonding config (port_id)\n"
			"	Show the bonding config for port_id.\n\n"

			"set bonding mac_addr (port_id) (address)\n"
			"	Set the MAC address of a bonded device.\n\n"

			"set bonding xmit_balance_policy (port_id) (l2|l23|l34)\n"
			"	Set the transmit balance policy for bonded device running in balance mode.\n\n"

			"set bonding mon_period (port_id) (value)\n"
			"	Set the bonding link status monitoring polling period in ms.\n\n"
#endif
			"set link-up port (port_id)\n"
			"	Set link up for a port.\n\n"

			"set link-down port (port_id)\n"
			"	Set link down for a port.\n\n"

			"E-tag set insertion on port-tag-id (value)"
			" port (port_id) vf (vf_id)\n"
			"    Enable E-tag insertion for a VF on a port\n\n"

			"E-tag set insertion off port (port_id) vf (vf_id)\n"
			"    Disable E-tag insertion for a VF on a port\n\n"

			"E-tag set stripping (on|off) port (port_id)\n"
			"    Enable/disable E-tag stripping on a port\n\n"

			"E-tag set forwarding (on|off) port (port_id)\n"
			"    Enable/disable E-tag based forwarding"
			" on a port\n\n"

			"E-tag set filter add e-tag-id (value) dst-pool"
			" (pool_id) port (port_id)\n"
			"    Add an E-tag forwarding filter on a port\n\n"

			"E-tag set filter del e-tag-id (value) port (port_id)\n"
			"    Delete an E-tag forwarding filter on a port\n\n"

			, list_pkt_forwarding_modes()
		);
	}

	if (show_all || !strcmp(res->section, "ports")) {

		cmdline_printf(
			cl,
			"\n"
			"Port Operations:\n"
			"----------------\n\n"

			"port start (port_id|all)\n"
			"    Start all ports or port_id.\n\n"

			"port stop (port_id|all)\n"
			"    Stop all ports or port_id.\n\n"

			"port close (port_id|all)\n"
			"    Close all ports or port_id.\n\n"

			"port attach (ident)\n"
			"    Attach physical or virtual dev by pci address or virtual device name\n\n"

			"port detach (port_id)\n"
			"    Detach physical or virtual dev by port_id\n\n"

			"port config (port_id|all)"
			" speed (10|100|1000|10000|25000|40000|50000|100000|auto)"
			" duplex (half|full|auto)\n"
			"    Set speed and duplex for all ports or port_id\n\n"

			"port config all (rxq|txq|rxd|txd) (value)\n"
			"    Set number for rxq/txq/rxd/txd.\n\n"

			"port config all max-pkt-len (value)\n"
			"    Set the max packet length.\n\n"

			"port config all (crc-strip|scatter|rx-cksum|hw-vlan|hw-vlan-filter|"
			"hw-vlan-strip|hw-vlan-extend|drop-en)"
			" (on|off)\n"
			"    Set crc-strip/scatter/rx-checksum/hardware-vlan/drop_en"
			" for ports.\n\n"

			"port config all rss (all|ip|tcp|udp|sctp|ether|port|vxlan|geneve|nvgre|none)\n"
			"    Set the RSS mode.\n\n"

			"port config port-id rss reta (hash,queue)[,(hash,queue)]\n"
			"    Set the RSS redirection table.\n\n"

			"port config (port_id) dcb vt (on|off) (traffic_class)"
			" pfc (on|off)\n"
			"    Set the DCB mode.\n\n"

			"port config all burst (value)\n"
			"    Set the number of packets per burst.\n\n"

			"port config all (txpt|txht|txwt|rxpt|rxht|rxwt)"
			" (value)\n"
			"    Set the ring prefetch/host/writeback threshold"
			" for tx/rx queue.\n\n"

			"port config all (txfreet|txrst|rxfreet) (value)\n"
			"    Set free threshold for rx/tx, or set"
			" tx rs bit threshold.\n\n"
			"port config mtu X value\n"
			"    Set the MTU of port X to a given value\n\n"

			"port (port_id) (rxq|txq) (queue_id) (start|stop)\n"
			"    Start/stop a rx/tx queue of port X. Only take effect"
			" when port X is started\n\n"

			"port config (port_id|all) l2-tunnel E-tag ether-type"
			" (value)\n"
			"    Set the value of E-tag ether-type.\n\n"

			"port config (port_id|all) l2-tunnel E-tag"
			" (enable|disable)\n"
			"    Enable/disable the E-tag support.\n\n"
		);
	}

	if (show_all || !strcmp(res->section, "registers")) {

		cmdline_printf(
			cl,
			"\n"
			"Registers:\n"
			"----------\n\n"

			"read reg (port_id) (address)\n"
			"    Display value of a port register.\n\n"

			"read regfield (port_id) (address) (bit_x) (bit_y)\n"
			"    Display a port register bit field.\n\n"

			"read regbit (port_id) (address) (bit_x)\n"
			"    Display a single port register bit.\n\n"

			"write reg (port_id) (address) (value)\n"
			"    Set value of a port register.\n\n"

			"write regfield (port_id) (address) (bit_x) (bit_y)"
			" (value)\n"
			"    Set bit field of a port register.\n\n"

			"write regbit (port_id) (address) (bit_x) (value)\n"
			"    Set single bit value of a port register.\n\n"
		);
	}
	if (show_all || !strcmp(res->section, "filters")) {

		cmdline_printf(
			cl,
			"\n"
			"filters:\n"
			"--------\n\n"

			"ethertype_filter (port_id) (add|del)"
			" (mac_addr|mac_ignr) (mac_address) ethertype"
			" (ether_type) (drop|fwd) queue (queue_id)\n"
			"    Add/Del an ethertype filter.\n\n"

			"2tuple_filter (port_id) (add|del)"
			" dst_port (dst_port_value) protocol (protocol_value)"
			" mask (mask_value) tcp_flags (tcp_flags_value)"
			" priority (prio_value) queue (queue_id)\n"
			"    Add/Del a 2tuple filter.\n\n"

			"5tuple_filter (port_id) (add|del)"
			" dst_ip (dst_address) src_ip (src_address)"
			" dst_port (dst_port_value) src_port (src_port_value)"
			" protocol (protocol_value)"
			" mask (mask_value) tcp_flags (tcp_flags_value)"
			" priority (prio_value) queue (queue_id)\n"
			"    Add/Del a 5tuple filter.\n\n"

			"syn_filter (port_id) (add|del) priority (high|low) queue (queue_id)"
			"    Add/Del syn filter.\n\n"

			"flex_filter (port_id) (add|del) len (len_value)"
			" bytes (bytes_value) mask (mask_value)"
			" priority (prio_value) queue (queue_id)\n"
			"    Add/Del a flex filter.\n\n"

			"flow_director_filter (port_id) mode IP (add|del|update)"
			" flow (ipv4-other|ipv4-frag|ipv6-other|ipv6-frag)"
			" src (src_ip_address) dst (dst_ip_address)"
			" tos (tos_value) proto (proto_value) ttl (ttl_value)"
			" vlan (vlan_value) flexbytes (flexbytes_value)"
			" (drop|fwd) pf|vf(vf_id) queue (queue_id)"
			" fd_id (fd_id_value)\n"
			"    Add/Del an IP type flow director filter.\n\n"

			"flow_director_filter (port_id) mode IP (add|del|update)"
			" flow (ipv4-tcp|ipv4-udp|ipv6-tcp|ipv6-udp)"
			" src (src_ip_address) (src_port)"
			" dst (dst_ip_address) (dst_port)"
			" tos (tos_value) ttl (ttl_value)"
			" vlan (vlan_value) flexbytes (flexbytes_value)"
			" (drop|fwd) pf|vf(vf_id) queue (queue_id)"
			" fd_id (fd_id_value)\n"
			"    Add/Del an UDP/TCP type flow director filter.\n\n"

			"flow_director_filter (port_id) mode IP (add|del|update)"
			" flow (ipv4-sctp|ipv6-sctp)"
			" src (src_ip_address) (src_port)"
			" dst (dst_ip_address) (dst_port)"
			" tag (verification_tag) "
			" tos (tos_value) ttl (ttl_value)"
			" vlan (vlan_value)"
			" flexbytes (flexbytes_value) (drop|fwd)"
			" pf|vf(vf_id) queue (queue_id) fd_id (fd_id_value)\n"
			"    Add/Del a SCTP type flow director filter.\n\n"

			"flow_director_filter (port_id) mode IP (add|del|update)"
			" flow l2_payload ether (ethertype)"
			" flexbytes (flexbytes_value) (drop|fwd)"
			" pf|vf(vf_id) queue (queue_id) fd_id (fd_id_value)\n"
			"    Add/Del a l2 payload type flow director filter.\n\n"

			"flow_director_filter (port_id) mode MAC-VLAN (add|del|update)"
			" mac (mac_address) vlan (vlan_value)"
			" flexbytes (flexbytes_value) (drop|fwd)"
			" queue (queue_id) fd_id (fd_id_value)\n"
			"    Add/Del a MAC-VLAN flow director filter.\n\n"

			"flow_director_filter (port_id) mode Tunnel (add|del|update)"
			" mac (mac_address) vlan (vlan_value)"
			" tunnel (NVGRE|VxLAN) tunnel-id (tunnel_id_value)"
			" flexbytes (flexbytes_value) (drop|fwd)"
			" queue (queue_id) fd_id (fd_id_value)\n"
			"    Add/Del a Tunnel flow director filter.\n\n"

			"flush_flow_director (port_id)\n"
			"    Flush all flow director entries of a device.\n\n"

			"flow_director_mask (port_id) mode IP vlan (vlan_value)"
			" src_mask (ipv4_src) (ipv6_src) (src_port)"
			" dst_mask (ipv4_dst) (ipv6_dst) (dst_port)\n"
			"    Set flow director IP mask.\n\n"

			"flow_director_mask (port_id) mode MAC-VLAN"
			" vlan (vlan_value)\n"
			"    Set flow director MAC-VLAN mask.\n\n"

			"flow_director_mask (port_id) mode Tunnel"
			" vlan (vlan_value) mac (mac_value)"
			" tunnel-type (tunnel_type_value)"
			" tunnel-id (tunnel_id_value)\n"
			"    Set flow director Tunnel mask.\n\n"

			"flow_director_flex_mask (port_id)"
			" flow (none|ipv4-other|ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|"
			"ipv6-other|ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|l2_payload|all)"
			" (mask)\n"
			"    Configure mask of flex payload.\n\n"

			"flow_director_flex_payload (port_id)"
			" (raw|l2|l3|l4) (config)\n"
			"    Configure flex payload selection.\n\n"

			"get_sym_hash_ena_per_port (port_id)\n"
			"    get symmetric hash enable configuration per port.\n\n"

			"set_sym_hash_ena_per_port (port_id) (enable|disable)\n"
			"    set symmetric hash enable configuration per port"
			" to enable or disable.\n\n"

			"get_hash_global_config (port_id)\n"
			"    Get the global configurations of hash filters.\n\n"

			"set_hash_global_config (port_id) (toeplitz|simple_xor|default)"
			" (ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|ipv6|"
			"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|l2_payload)"
			" (enable|disable)\n"
			"    Set the global configurations of hash filters.\n\n"

			"set_hash_input_set (port_id) (ipv4|ipv4-frag|"
			"ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|ipv6|"
			"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|"
			"l2_payload) (ovlan|ivlan|src-ipv4|dst-ipv4|src-ipv6|"
			"dst-ipv6|ipv4-tos|ipv4-proto|ipv6-tc|"
			"ipv6-next-header|udp-src-port|udp-dst-port|"
			"tcp-src-port|tcp-dst-port|sctp-src-port|"
			"sctp-dst-port|sctp-veri-tag|udp-key|gre-key|fld-1st|"
			"fld-2nd|fld-3rd|fld-4th|fld-5th|fld-6th|fld-7th|"
			"fld-8th|none) (select|add)\n"
			"    Set the input set for hash.\n\n"

			"set_fdir_input_set (port_id) "
			"(ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|"
			"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|"
			"l2_payload) (ivlan|ethertype|src-ipv4|dst-ipv4|src-ipv6|"
			"dst-ipv6|ipv4-tos|ipv4-proto|ipv4-ttl|ipv6-tc|"
			"ipv6-next-header|ipv6-hop-limits|udp-src-port|"
			"udp-dst-port|tcp-src-port|tcp-dst-port|"
			"sctp-src-port|sctp-dst-port|sctp-veri-tag|none)"
			" (select|add)\n"
			"    Set the input set for FDir.\n\n"
		);
	}
}

cmdline_parse_token_string_t cmd_help_long_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_long_result, help, "help");

cmdline_parse_token_string_t cmd_help_long_section =
	TOKEN_STRING_INITIALIZER(struct cmd_help_long_result, section,
			"all#control#display#config#"
			"ports#registers#filters");

cmdline_parse_inst_t cmd_help_long = {
	.f = cmd_help_long_parsed,
	.data = NULL,
	.help_str = "show help",
	.tokens = {
		(void *)&cmd_help_long_help,
		(void *)&cmd_help_long_section,
		NULL,
	},
};


/* *** start/stop/close all ports *** */
struct cmd_operate_port_result {
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t value;
};

static void cmd_operate_port_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_operate_port_result *res = parsed_result;

	if (!strcmp(res->name, "start"))
		start_port(RTE_PORT_ALL);
	else if (!strcmp(res->name, "stop"))
		stop_port(RTE_PORT_ALL);
	else if (!strcmp(res->name, "close"))
		close_port(RTE_PORT_ALL);
	else
		printf("Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_port_all_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, keyword,
								"port");
cmdline_parse_token_string_t cmd_operate_port_all_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, name,
						"start#stop#close");
cmdline_parse_token_string_t cmd_operate_port_all_all =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, value, "all");

cmdline_parse_inst_t cmd_operate_port = {
	.f = cmd_operate_port_parsed,
	.data = NULL,
	.help_str = "port start|stop|close all: start/stop/close all ports",
	.tokens = {
		(void *)&cmd_operate_port_all_cmd,
		(void *)&cmd_operate_port_all_port,
		(void *)&cmd_operate_port_all_all,
		NULL,
	},
};

/* *** start/stop/close specific port *** */
struct cmd_operate_specific_port_result {
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	uint8_t value;
};

static void cmd_operate_specific_port_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_operate_specific_port_result *res = parsed_result;

	if (!strcmp(res->name, "start"))
		start_port(res->value);
	else if (!strcmp(res->name, "stop"))
		stop_port(res->value);
	else if (!strcmp(res->name, "close"))
		close_port(res->value);
	else
		printf("Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_specific_port_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_specific_port_result,
							keyword, "port");
cmdline_parse_token_string_t cmd_operate_specific_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_specific_port_result,
						name, "start#stop#close");
cmdline_parse_token_num_t cmd_operate_specific_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_operate_specific_port_result,
							value, UINT8);

cmdline_parse_inst_t cmd_operate_specific_port = {
	.f = cmd_operate_specific_port_parsed,
	.data = NULL,
	.help_str = "port start|stop|close X: start/stop/close port X",
	.tokens = {
		(void *)&cmd_operate_specific_port_cmd,
		(void *)&cmd_operate_specific_port_port,
		(void *)&cmd_operate_specific_port_id,
		NULL,
	},
};

/* *** attach a specified port *** */
struct cmd_operate_attach_port_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t identifier;
};

static void cmd_operate_attach_port_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_operate_attach_port_result *res = parsed_result;

	if (!strcmp(res->keyword, "attach"))
		attach_port(res->identifier);
	else
		printf("Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_attach_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			port, "port");
cmdline_parse_token_string_t cmd_operate_attach_port_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			keyword, "attach");
cmdline_parse_token_string_t cmd_operate_attach_port_identifier =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			identifier, NULL);

cmdline_parse_inst_t cmd_operate_attach_port = {
	.f = cmd_operate_attach_port_parsed,
	.data = NULL,
	.help_str = "port attach identifier, "
		"identifier: pci address or virtual dev name",
	.tokens = {
		(void *)&cmd_operate_attach_port_port,
		(void *)&cmd_operate_attach_port_keyword,
		(void *)&cmd_operate_attach_port_identifier,
		NULL,
	},
};

/* *** detach a specified port *** */
struct cmd_operate_detach_port_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint8_t port_id;
};

static void cmd_operate_detach_port_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_operate_detach_port_result *res = parsed_result;

	if (!strcmp(res->keyword, "detach"))
		detach_port(res->port_id);
	else
		printf("Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_detach_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_port_result,
			port, "port");
cmdline_parse_token_string_t cmd_operate_detach_port_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_port_result,
			keyword, "detach");
cmdline_parse_token_num_t cmd_operate_detach_port_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_operate_detach_port_result,
			port_id, UINT8);

cmdline_parse_inst_t cmd_operate_detach_port = {
	.f = cmd_operate_detach_port_parsed,
	.data = NULL,
	.help_str = "port detach port_id",
	.tokens = {
		(void *)&cmd_operate_detach_port_port,
		(void *)&cmd_operate_detach_port_keyword,
		(void *)&cmd_operate_detach_port_port_id,
		NULL,
	},
};

/* *** configure speed for all ports *** */
struct cmd_config_speed_all {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t item1;
	cmdline_fixed_string_t item2;
	cmdline_fixed_string_t value1;
	cmdline_fixed_string_t value2;
};

static int
parse_and_check_speed_duplex(char *speedstr, char *duplexstr, uint32_t *speed)
{

	int duplex;

	if (!strcmp(duplexstr, "half")) {
		duplex = ETH_LINK_HALF_DUPLEX;
	} else if (!strcmp(duplexstr, "full")) {
		duplex = ETH_LINK_FULL_DUPLEX;
	} else if (!strcmp(duplexstr, "auto")) {
		duplex = ETH_LINK_FULL_DUPLEX;
	} else {
		printf("Unknown duplex parameter\n");
		return -1;
	}

	if (!strcmp(speedstr, "10")) {
		*speed = (duplex == ETH_LINK_HALF_DUPLEX) ?
				ETH_LINK_SPEED_10M_HD : ETH_LINK_SPEED_10M;
	} else if (!strcmp(speedstr, "100")) {
		*speed = (duplex == ETH_LINK_HALF_DUPLEX) ?
				ETH_LINK_SPEED_100M_HD : ETH_LINK_SPEED_100M;
	} else {
		if (duplex != ETH_LINK_FULL_DUPLEX) {
			printf("Invalid speed/duplex parameters\n");
			return -1;
		}
		if (!strcmp(speedstr, "1000")) {
			*speed = ETH_LINK_SPEED_1G;
		} else if (!strcmp(speedstr, "10000")) {
			*speed = ETH_LINK_SPEED_10G;
		} else if (!strcmp(speedstr, "25000")) {
			*speed = ETH_LINK_SPEED_25G;
		} else if (!strcmp(speedstr, "40000")) {
			*speed = ETH_LINK_SPEED_40G;
		} else if (!strcmp(speedstr, "50000")) {
			*speed = ETH_LINK_SPEED_50G;
		} else if (!strcmp(speedstr, "100000")) {
			*speed = ETH_LINK_SPEED_100G;
		} else if (!strcmp(speedstr, "auto")) {
			*speed = ETH_LINK_SPEED_AUTONEG;
		} else {
			printf("Unknown speed parameter\n");
			return -1;
		}
	}

	return 0;
}

static void
cmd_config_speed_all_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_speed_all *res = parsed_result;
	uint32_t link_speed;
	portid_t pid;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (parse_and_check_speed_duplex(res->value1, res->value2,
			&link_speed) < 0)
		return;

	FOREACH_PORT(pid, ports) {
		ports[pid].dev_conf.link_speeds = link_speed;
	}

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_speed_all_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, port, "port");
cmdline_parse_token_string_t cmd_config_speed_all_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, keyword,
							"config");
cmdline_parse_token_string_t cmd_config_speed_all_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, all, "all");
cmdline_parse_token_string_t cmd_config_speed_all_item1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, item1, "speed");
cmdline_parse_token_string_t cmd_config_speed_all_value1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, value1,
				"10#100#1000#10000#25000#40000#50000#100000#auto");
cmdline_parse_token_string_t cmd_config_speed_all_item2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, item2, "duplex");
cmdline_parse_token_string_t cmd_config_speed_all_value2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, value2,
						"half#full#auto");

cmdline_parse_inst_t cmd_config_speed_all = {
	.f = cmd_config_speed_all_parsed,
	.data = NULL,
	.help_str = "port config all speed "
		"10|100|1000|10000|25000|40000|50000|100000|auto duplex "
							"half|full|auto",
	.tokens = {
		(void *)&cmd_config_speed_all_port,
		(void *)&cmd_config_speed_all_keyword,
		(void *)&cmd_config_speed_all_all,
		(void *)&cmd_config_speed_all_item1,
		(void *)&cmd_config_speed_all_value1,
		(void *)&cmd_config_speed_all_item2,
		(void *)&cmd_config_speed_all_value2,
		NULL,
	},
};

/* *** configure speed for specific port *** */
struct cmd_config_speed_specific {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint8_t id;
	cmdline_fixed_string_t item1;
	cmdline_fixed_string_t item2;
	cmdline_fixed_string_t value1;
	cmdline_fixed_string_t value2;
};

static void
cmd_config_speed_specific_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_config_speed_specific *res = parsed_result;
	uint32_t link_speed;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (port_id_is_invalid(res->id, ENABLED_WARN))
		return;

	if (parse_and_check_speed_duplex(res->value1, res->value2,
			&link_speed) < 0)
		return;

	ports[res->id].dev_conf.link_speeds = link_speed;

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}


cmdline_parse_token_string_t cmd_config_speed_specific_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, port,
								"port");
cmdline_parse_token_string_t cmd_config_speed_specific_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, keyword,
								"config");
cmdline_parse_token_num_t cmd_config_speed_specific_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_speed_specific, id, UINT8);
cmdline_parse_token_string_t cmd_config_speed_specific_item1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, item1,
								"speed");
cmdline_parse_token_string_t cmd_config_speed_specific_value1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, value1,
				"10#100#1000#10000#25000#40000#50000#100000#auto");
cmdline_parse_token_string_t cmd_config_speed_specific_item2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, item2,
								"duplex");
cmdline_parse_token_string_t cmd_config_speed_specific_value2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, value2,
							"half#full#auto");

cmdline_parse_inst_t cmd_config_speed_specific = {
	.f = cmd_config_speed_specific_parsed,
	.data = NULL,
	.help_str = "port config X speed "
		"10|100|1000|10000|25000|40000|50000|100000|auto duplex "
							"half|full|auto",
	.tokens = {
		(void *)&cmd_config_speed_specific_port,
		(void *)&cmd_config_speed_specific_keyword,
		(void *)&cmd_config_speed_specific_id,
		(void *)&cmd_config_speed_specific_item1,
		(void *)&cmd_config_speed_specific_value1,
		(void *)&cmd_config_speed_specific_item2,
		(void *)&cmd_config_speed_specific_value2,
		NULL,
	},
};

/* *** configure txq/rxq, txd/rxd *** */
struct cmd_config_rx_tx {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint16_t value;
};

static void
cmd_config_rx_tx_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_rx_tx *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}
	if (!strcmp(res->name, "rxq")) {
		if (!res->value && !nb_txq) {
			printf("Warning: Either rx or tx queues should be non zero\n");
			return;
		}
		nb_rxq = res->value;
	}
	else if (!strcmp(res->name, "txq")) {
		if (!res->value && !nb_rxq) {
			printf("Warning: Either rx or tx queues should be non zero\n");
			return;
		}
		nb_txq = res->value;
	}
	else if (!strcmp(res->name, "rxd")) {
		if (res->value <= 0 || res->value > RTE_TEST_RX_DESC_MAX) {
			printf("rxd %d invalid - must be > 0 && <= %d\n",
					res->value, RTE_TEST_RX_DESC_MAX);
			return;
		}
		nb_rxd = res->value;
	} else if (!strcmp(res->name, "txd")) {
		if (res->value <= 0 || res->value > RTE_TEST_TX_DESC_MAX) {
			printf("txd %d invalid - must be > 0 && <= %d\n",
					res->value, RTE_TEST_TX_DESC_MAX);
			return;
		}
		nb_txd = res->value;
	} else {
		printf("Unknown parameter\n");
		return;
	}

	fwd_config_setup();

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_rx_tx_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_tx, port, "port");
cmdline_parse_token_string_t cmd_config_rx_tx_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_tx, keyword, "config");
cmdline_parse_token_string_t cmd_config_rx_tx_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_tx, all, "all");
cmdline_parse_token_string_t cmd_config_rx_tx_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_tx, name,
						"rxq#txq#rxd#txd");
cmdline_parse_token_num_t cmd_config_rx_tx_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rx_tx, value, UINT16);

cmdline_parse_inst_t cmd_config_rx_tx = {
	.f = cmd_config_rx_tx_parsed,
	.data = NULL,
	.help_str = "port config all rxq|txq|rxd|txd value",
	.tokens = {
		(void *)&cmd_config_rx_tx_port,
		(void *)&cmd_config_rx_tx_keyword,
		(void *)&cmd_config_rx_tx_all,
		(void *)&cmd_config_rx_tx_name,
		(void *)&cmd_config_rx_tx_value,
		NULL,
	},
};

/* *** config max packet length *** */
struct cmd_config_max_pkt_len_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint32_t value;
};

static void
cmd_config_max_pkt_len_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_config_max_pkt_len_result *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "max-pkt-len")) {
		if (res->value < ETHER_MIN_LEN) {
			printf("max-pkt-len can not be less than %d\n",
							ETHER_MIN_LEN);
			return;
		}
		if (res->value == rx_mode.max_rx_pkt_len)
			return;

		rx_mode.max_rx_pkt_len = res->value;
		if (res->value > ETHER_MAX_LEN)
			rx_mode.jumbo_frame = 1;
		else
			rx_mode.jumbo_frame = 0;
	} else {
		printf("Unknown parameter\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_max_pkt_len_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_pkt_len_result, port,
								"port");
cmdline_parse_token_string_t cmd_config_max_pkt_len_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_pkt_len_result, keyword,
								"config");
cmdline_parse_token_string_t cmd_config_max_pkt_len_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_pkt_len_result, all,
								"all");
cmdline_parse_token_string_t cmd_config_max_pkt_len_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_pkt_len_result, name,
								"max-pkt-len");
cmdline_parse_token_num_t cmd_config_max_pkt_len_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_max_pkt_len_result, value,
								UINT32);

cmdline_parse_inst_t cmd_config_max_pkt_len = {
	.f = cmd_config_max_pkt_len_parsed,
	.data = NULL,
	.help_str = "port config all max-pkt-len value",
	.tokens = {
		(void *)&cmd_config_max_pkt_len_port,
		(void *)&cmd_config_max_pkt_len_keyword,
		(void *)&cmd_config_max_pkt_len_all,
		(void *)&cmd_config_max_pkt_len_name,
		(void *)&cmd_config_max_pkt_len_value,
		NULL,
	},
};

/* *** configure port MTU *** */
struct cmd_config_mtu_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t mtu;
	uint8_t port_id;
	uint16_t value;
};

static void
cmd_config_mtu_parsed(void *parsed_result,
		      __attribute__((unused)) struct cmdline *cl,
		      __attribute__((unused)) void *data)
{
	struct cmd_config_mtu_result *res = parsed_result;

	if (res->value < ETHER_MIN_LEN) {
		printf("mtu cannot be less than %d\n", ETHER_MIN_LEN);
		return;
	}
	port_mtu_set(res->port_id, res->value);
}

cmdline_parse_token_string_t cmd_config_mtu_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_mtu_result, port,
				 "port");
cmdline_parse_token_string_t cmd_config_mtu_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_mtu_result, keyword,
				 "config");
cmdline_parse_token_string_t cmd_config_mtu_mtu =
	TOKEN_STRING_INITIALIZER(struct cmd_config_mtu_result, keyword,
				 "mtu");
cmdline_parse_token_num_t cmd_config_mtu_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_mtu_result, port_id, UINT8);
cmdline_parse_token_num_t cmd_config_mtu_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_mtu_result, value, UINT16);

cmdline_parse_inst_t cmd_config_mtu = {
	.f = cmd_config_mtu_parsed,
	.data = NULL,
	.help_str = "port config mtu port_id value",
	.tokens = {
		(void *)&cmd_config_mtu_port,
		(void *)&cmd_config_mtu_keyword,
		(void *)&cmd_config_mtu_mtu,
		(void *)&cmd_config_mtu_port_id,
		(void *)&cmd_config_mtu_value,
		NULL,
	},
};

/* *** configure rx mode *** */
struct cmd_config_rx_mode_flag {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t value;
};

static void
cmd_config_rx_mode_flag_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_config_rx_mode_flag *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "crc-strip")) {
		if (!strcmp(res->value, "on"))
			rx_mode.hw_strip_crc = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.hw_strip_crc = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "scatter")) {
		if (!strcmp(res->value, "on"))
			rx_mode.enable_scatter = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.enable_scatter = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "rx-cksum")) {
		if (!strcmp(res->value, "on"))
			rx_mode.hw_ip_checksum = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.hw_ip_checksum = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "hw-vlan")) {
		if (!strcmp(res->value, "on")) {
			rx_mode.hw_vlan_filter = 1;
			rx_mode.hw_vlan_strip  = 1;
		}
		else if (!strcmp(res->value, "off")) {
			rx_mode.hw_vlan_filter = 0;
			rx_mode.hw_vlan_strip  = 0;
		}
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "hw-vlan-filter")) {
		if (!strcmp(res->value, "on"))
			rx_mode.hw_vlan_filter = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.hw_vlan_filter = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "hw-vlan-strip")) {
		if (!strcmp(res->value, "on"))
			rx_mode.hw_vlan_strip  = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.hw_vlan_strip  = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "hw-vlan-extend")) {
		if (!strcmp(res->value, "on"))
			rx_mode.hw_vlan_extend = 1;
		else if (!strcmp(res->value, "off"))
			rx_mode.hw_vlan_extend = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else if (!strcmp(res->name, "drop-en")) {
		if (!strcmp(res->value, "on"))
			rx_drop_en = 1;
		else if (!strcmp(res->value, "off"))
			rx_drop_en = 0;
		else {
			printf("Unknown parameter\n");
			return;
		}
	} else {
		printf("Unknown parameter\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_rx_mode_flag_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, port, "port");
cmdline_parse_token_string_t cmd_config_rx_mode_flag_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, keyword,
								"config");
cmdline_parse_token_string_t cmd_config_rx_mode_flag_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, all, "all");
cmdline_parse_token_string_t cmd_config_rx_mode_flag_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, name,
					"crc-strip#scatter#rx-cksum#hw-vlan#"
					"hw-vlan-filter#hw-vlan-strip#hw-vlan-extend");
cmdline_parse_token_string_t cmd_config_rx_mode_flag_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, value,
							"on#off");

cmdline_parse_inst_t cmd_config_rx_mode_flag = {
	.f = cmd_config_rx_mode_flag_parsed,
	.data = NULL,
	.help_str = "port config all crc-strip|scatter|rx-cksum|hw-vlan|"
		"hw-vlan-filter|hw-vlan-strip|hw-vlan-extend on|off",
	.tokens = {
		(void *)&cmd_config_rx_mode_flag_port,
		(void *)&cmd_config_rx_mode_flag_keyword,
		(void *)&cmd_config_rx_mode_flag_all,
		(void *)&cmd_config_rx_mode_flag_name,
		(void *)&cmd_config_rx_mode_flag_value,
		NULL,
	},
};

/* *** configure rss *** */
struct cmd_config_rss {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t value;
};

static void
cmd_config_rss_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_rss *res = parsed_result;
	struct rte_eth_rss_conf rss_conf;
	int diag;
	uint8_t i;

	if (!strcmp(res->value, "all"))
		rss_conf.rss_hf = ETH_RSS_IP | ETH_RSS_TCP |
				ETH_RSS_UDP | ETH_RSS_SCTP |
					ETH_RSS_L2_PAYLOAD;
	else if (!strcmp(res->value, "ip"))
		rss_conf.rss_hf = ETH_RSS_IP;
	else if (!strcmp(res->value, "udp"))
		rss_conf.rss_hf = ETH_RSS_UDP;
	else if (!strcmp(res->value, "tcp"))
		rss_conf.rss_hf = ETH_RSS_TCP;
	else if (!strcmp(res->value, "sctp"))
		rss_conf.rss_hf = ETH_RSS_SCTP;
	else if (!strcmp(res->value, "ether"))
		rss_conf.rss_hf = ETH_RSS_L2_PAYLOAD;
	else if (!strcmp(res->value, "port"))
		rss_conf.rss_hf = ETH_RSS_PORT;
	else if (!strcmp(res->value, "vxlan"))
		rss_conf.rss_hf = ETH_RSS_VXLAN;
	else if (!strcmp(res->value, "geneve"))
		rss_conf.rss_hf = ETH_RSS_GENEVE;
	else if (!strcmp(res->value, "nvgre"))
		rss_conf.rss_hf = ETH_RSS_NVGRE;
	else if (!strcmp(res->value, "none"))
		rss_conf.rss_hf = 0;
	else {
		printf("Unknown parameter\n");
		return;
	}
	rss_conf.rss_key = NULL;
	for (i = 0; i < rte_eth_dev_count(); i++) {
		diag = rte_eth_dev_rss_hash_update(i, &rss_conf);
		if (diag < 0)
			printf("Configuration of RSS hash at ethernet port %d "
				"failed with error (%d): %s.\n",
				i, -diag, strerror(-diag));
	}
}

cmdline_parse_token_string_t cmd_config_rss_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, port, "port");
cmdline_parse_token_string_t cmd_config_rss_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, keyword, "config");
cmdline_parse_token_string_t cmd_config_rss_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, all, "all");
cmdline_parse_token_string_t cmd_config_rss_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, name, "rss");
cmdline_parse_token_string_t cmd_config_rss_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, value,
		"all#ip#tcp#udp#sctp#ether#port#vxlan#geneve#nvgre#none");

cmdline_parse_inst_t cmd_config_rss = {
	.f = cmd_config_rss_parsed,
	.data = NULL,
	.help_str = "port config all rss all|ip|tcp|udp|sctp|ether|port|vxlan|geneve|nvgre|none",
	.tokens = {
		(void *)&cmd_config_rss_port,
		(void *)&cmd_config_rss_keyword,
		(void *)&cmd_config_rss_all,
		(void *)&cmd_config_rss_name,
		(void *)&cmd_config_rss_value,
		NULL,
	},
};

/* *** configure rss hash key *** */
struct cmd_config_rss_hash_key {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	uint8_t port_id;
	cmdline_fixed_string_t rss_hash_key;
	cmdline_fixed_string_t rss_type;
	cmdline_fixed_string_t key;
};

static uint8_t
hexa_digit_to_value(char hexa_digit)
{
	if ((hexa_digit >= '0') && (hexa_digit <= '9'))
		return (uint8_t) (hexa_digit - '0');
	if ((hexa_digit >= 'a') && (hexa_digit <= 'f'))
		return (uint8_t) ((hexa_digit - 'a') + 10);
	if ((hexa_digit >= 'A') && (hexa_digit <= 'F'))
		return (uint8_t) ((hexa_digit - 'A') + 10);
	/* Invalid hexa digit */
	return 0xFF;
}

static uint8_t
parse_and_check_key_hexa_digit(char *key, int idx)
{
	uint8_t hexa_v;

	hexa_v = hexa_digit_to_value(key[idx]);
	if (hexa_v == 0xFF)
		printf("invalid key: character %c at position %d is not a "
		       "valid hexa digit\n", key[idx], idx);
	return hexa_v;
}

static void
cmd_config_rss_hash_key_parsed(void *parsed_result,
			       __attribute__((unused)) struct cmdline *cl,
			       __attribute__((unused)) void *data)
{
	struct cmd_config_rss_hash_key *res = parsed_result;
	uint8_t hash_key[RSS_HASH_KEY_LENGTH];
	uint8_t xdgt0;
	uint8_t xdgt1;
	int i;
	struct rte_eth_dev_info dev_info;
	uint8_t hash_key_size;
	uint32_t key_len;

	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(res->port_id, &dev_info);
	if (dev_info.hash_key_size > 0 &&
			dev_info.hash_key_size <= sizeof(hash_key))
		hash_key_size = dev_info.hash_key_size;
	else {
		printf("dev_info did not provide a valid hash key size\n");
		return;
	}
	/* Check the length of the RSS hash key */
	key_len = strlen(res->key);
	if (key_len != (hash_key_size * 2)) {
		printf("key length: %d invalid - key must be a string of %d"
			   " hexa-decimal numbers\n",
			   (int) key_len, hash_key_size * 2);
		return;
	}
	/* Translate RSS hash key into binary representation */
	for (i = 0; i < hash_key_size; i++) {
		xdgt0 = parse_and_check_key_hexa_digit(res->key, (i * 2));
		if (xdgt0 == 0xFF)
			return;
		xdgt1 = parse_and_check_key_hexa_digit(res->key, (i * 2) + 1);
		if (xdgt1 == 0xFF)
			return;
		hash_key[i] = (uint8_t) ((xdgt0 * 16) + xdgt1);
	}
	port_rss_hash_key_update(res->port_id, res->rss_type, hash_key,
			hash_key_size);
}

cmdline_parse_token_string_t cmd_config_rss_hash_key_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, port, "port");
cmdline_parse_token_string_t cmd_config_rss_hash_key_config =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, config,
				 "config");
cmdline_parse_token_num_t cmd_config_rss_hash_key_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rss_hash_key, port_id, UINT8);
cmdline_parse_token_string_t cmd_config_rss_hash_key_rss_hash_key =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key,
				 rss_hash_key, "rss-hash-key");
cmdline_parse_token_string_t cmd_config_rss_hash_key_rss_type =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, rss_type,
				 "ipv4#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#"
				 "ipv4-other#ipv6#ipv6-frag#ipv6-tcp#ipv6-udp#"
				 "ipv6-sctp#ipv6-other#l2-payload#ipv6-ex#"
				 "ipv6-tcp-ex#ipv6-udp-ex");
cmdline_parse_token_string_t cmd_config_rss_hash_key_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, key, NULL);

cmdline_parse_inst_t cmd_config_rss_hash_key = {
	.f = cmd_config_rss_hash_key_parsed,
	.data = NULL,
	.help_str =
		"port config X rss-hash-key ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|"
		"ipv4-sctp|ipv4-other|ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|"
		"ipv6-sctp|ipv6-other|l2-payload|"
		"ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex "
		"<string of hexa digits (variable length, NIC dependent)>\n",
	.tokens = {
		(void *)&cmd_config_rss_hash_key_port,
		(void *)&cmd_config_rss_hash_key_config,
		(void *)&cmd_config_rss_hash_key_port_id,
		(void *)&cmd_config_rss_hash_key_rss_hash_key,
		(void *)&cmd_config_rss_hash_key_rss_type,
		(void *)&cmd_config_rss_hash_key_value,
		NULL,
	},
};

/* *** configure port rxq/txq start/stop *** */
struct cmd_config_rxtx_queue {
	cmdline_fixed_string_t port;
	uint8_t portid;
	cmdline_fixed_string_t rxtxq;
	uint16_t qid;
	cmdline_fixed_string_t opname;
};

static void
cmd_config_rxtx_queue_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_rxtx_queue *res = parsed_result;
	uint8_t isrx;
	uint8_t isstart;
	int ret = 0;

	if (test_done == 0) {
		printf("Please stop forwarding first\n");
		return;
	}

	if (port_id_is_invalid(res->portid, ENABLED_WARN))
		return;

	if (port_is_started(res->portid) != 1) {
		printf("Please start port %u first\n", res->portid);
		return;
	}

	if (!strcmp(res->rxtxq, "rxq"))
		isrx = 1;
	else if (!strcmp(res->rxtxq, "txq"))
		isrx = 0;
	else {
		printf("Unknown parameter\n");
		return;
	}

	if (isrx && rx_queue_id_is_invalid(res->qid))
		return;
	else if (!isrx && tx_queue_id_is_invalid(res->qid))
		return;

	if (!strcmp(res->opname, "start"))
		isstart = 1;
	else if (!strcmp(res->opname, "stop"))
		isstart = 0;
	else {
		printf("Unknown parameter\n");
		return;
	}

	if (isstart && isrx)
		ret = rte_eth_dev_rx_queue_start(res->portid, res->qid);
	else if (!isstart && isrx)
		ret = rte_eth_dev_rx_queue_stop(res->portid, res->qid);
	else if (isstart && !isrx)
		ret = rte_eth_dev_tx_queue_start(res->portid, res->qid);
	else
		ret = rte_eth_dev_tx_queue_stop(res->portid, res->qid);

	if (ret == -ENOTSUP)
		printf("Function not supported in PMD driver\n");
}

cmdline_parse_token_string_t cmd_config_rxtx_queue_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, port, "port");
cmdline_parse_token_num_t cmd_config_rxtx_queue_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_queue, portid, UINT8);
cmdline_parse_token_string_t cmd_config_rxtx_queue_rxtxq =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, rxtxq, "rxq#txq");
cmdline_parse_token_num_t cmd_config_rxtx_queue_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_queue, qid, UINT16);
cmdline_parse_token_string_t cmd_config_rxtx_queue_opname =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, opname,
						"start#stop");

cmdline_parse_inst_t cmd_config_rxtx_queue = {
	.f = cmd_config_rxtx_queue_parsed,
	.data = NULL,
	.help_str = "port X rxq|txq ID start|stop",
	.tokens = {
		(void *)&cmd_config_speed_all_port,
		(void *)&cmd_config_rxtx_queue_portid,
		(void *)&cmd_config_rxtx_queue_rxtxq,
		(void *)&cmd_config_rxtx_queue_qid,
		(void *)&cmd_config_rxtx_queue_opname,
		NULL,
	},
};

/* *** Configure RSS RETA *** */
struct cmd_config_rss_reta {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint8_t port_id;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t list_name;
	cmdline_fixed_string_t list_of_items;
};

static int
parse_reta_config(const char *str,
		  struct rte_eth_rss_reta_entry64 *reta_conf,
		  uint16_t nb_entries)
{
	int i;
	unsigned size;
	uint16_t hash_index, idx, shift;
	uint16_t nb_queue;
	char s[256];
	const char *p, *p0 = str;
	char *end;
	enum fieldnames {
		FLD_HASH_INDEX = 0,
		FLD_QUEUE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];

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
			if (errno != 0 || end == str_fld[i] ||
					int_fld[i] > 65535)
				return -1;
		}

		hash_index = (uint16_t)int_fld[FLD_HASH_INDEX];
		nb_queue = (uint16_t)int_fld[FLD_QUEUE];

		if (hash_index >= nb_entries) {
			printf("Invalid RETA hash index=%d\n", hash_index);
			return -1;
		}

		idx = hash_index / RTE_RETA_GROUP_SIZE;
		shift = hash_index % RTE_RETA_GROUP_SIZE;
		reta_conf[idx].mask |= (1ULL << shift);
		reta_conf[idx].reta[shift] = nb_queue;
	}

	return 0;
}

static void
cmd_set_rss_reta_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	int ret;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rss_reta_entry64 reta_conf[8];
	struct cmd_config_rss_reta *res = parsed_result;

	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(res->port_id, &dev_info);
	if (dev_info.reta_size == 0) {
		printf("Redirection table size is 0 which is "
					"invalid for RSS\n");
		return;
	} else
		printf("The reta size of port %d is %u\n",
			res->port_id, dev_info.reta_size);
	if (dev_info.reta_size > ETH_RSS_RETA_SIZE_512) {
		printf("Currently do not support more than %u entries of "
			"redirection table\n", ETH_RSS_RETA_SIZE_512);
		return;
	}

	memset(reta_conf, 0, sizeof(reta_conf));
	if (!strcmp(res->list_name, "reta")) {
		if (parse_reta_config(res->list_of_items, reta_conf,
						dev_info.reta_size)) {
			printf("Invalid RSS Redirection Table "
					"config entered\n");
			return;
		}
		ret = rte_eth_dev_rss_reta_update(res->port_id,
				reta_conf, dev_info.reta_size);
		if (ret != 0)
			printf("Bad redirection table parameter, "
					"return code = %d \n", ret);
	}
}

cmdline_parse_token_string_t cmd_config_rss_reta_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, port, "port");
cmdline_parse_token_string_t cmd_config_rss_reta_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, keyword, "config");
cmdline_parse_token_num_t cmd_config_rss_reta_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rss_reta, port_id, UINT8);
cmdline_parse_token_string_t cmd_config_rss_reta_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, name, "rss");
cmdline_parse_token_string_t cmd_config_rss_reta_list_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, list_name, "reta");
cmdline_parse_token_string_t cmd_config_rss_reta_list_of_items =
        TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, list_of_items,
                                 NULL);
cmdline_parse_inst_t cmd_config_rss_reta = {
	.f = cmd_set_rss_reta_parsed,
	.data = NULL,
	.help_str = "port config X rss reta (hash,queue)[,(hash,queue)]",
	.tokens = {
		(void *)&cmd_config_rss_reta_port,
		(void *)&cmd_config_rss_reta_keyword,
		(void *)&cmd_config_rss_reta_port_id,
		(void *)&cmd_config_rss_reta_name,
		(void *)&cmd_config_rss_reta_list_name,
		(void *)&cmd_config_rss_reta_list_of_items,
		NULL,
	},
};

/* *** SHOW PORT RETA INFO *** */
struct cmd_showport_reta {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t rss;
	cmdline_fixed_string_t reta;
	uint16_t size;
	cmdline_fixed_string_t list_of_items;
};

static int
showport_parse_reta_config(struct rte_eth_rss_reta_entry64 *conf,
			   uint16_t nb_entries,
			   char *str)
{
	uint32_t size;
	const char *p, *p0 = str;
	char s[256];
	char *end;
	char *str_fld[8];
	uint16_t i, num = nb_entries / RTE_RETA_GROUP_SIZE;
	int ret;

	p = strchr(p0, '(');
	if (p == NULL)
		return -1;
	p++;
	p0 = strchr(p, ')');
	if (p0 == NULL)
		return -1;
	size = p0 - p;
	if (size >= sizeof(s)) {
		printf("The string size exceeds the internal buffer size\n");
		return -1;
	}
	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, num, ',');
	if (ret <= 0 || ret != num) {
		printf("The bits of masks do not match the number of "
					"reta entries: %u\n", num);
		return -1;
	}
	for (i = 0; i < ret; i++)
		conf[i].mask = (uint64_t)strtoul(str_fld[i], &end, 0);

	return 0;
}

static void
cmd_showport_reta_parsed(void *parsed_result,
			 __attribute__((unused)) struct cmdline *cl,
			 __attribute__((unused)) void *data)
{
	struct cmd_showport_reta *res = parsed_result;
	struct rte_eth_rss_reta_entry64 reta_conf[8];
	struct rte_eth_dev_info dev_info;

	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(res->port_id, &dev_info);
	if (dev_info.reta_size == 0 || res->size != dev_info.reta_size ||
				res->size > ETH_RSS_RETA_SIZE_512) {
		printf("Invalid redirection table size: %u\n", res->size);
		return;
	}

	memset(reta_conf, 0, sizeof(reta_conf));
	if (showport_parse_reta_config(reta_conf, res->size,
				res->list_of_items) < 0) {
		printf("Invalid string: %s for reta masks\n",
					res->list_of_items);
		return;
	}
	port_rss_reta_info(res->port_id, reta_conf, res->size);
}

cmdline_parse_token_string_t cmd_showport_reta_show =
	TOKEN_STRING_INITIALIZER(struct  cmd_showport_reta, show, "show");
cmdline_parse_token_string_t cmd_showport_reta_port =
	TOKEN_STRING_INITIALIZER(struct  cmd_showport_reta, port, "port");
cmdline_parse_token_num_t cmd_showport_reta_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_reta, port_id, UINT8);
cmdline_parse_token_string_t cmd_showport_reta_rss =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta, rss, "rss");
cmdline_parse_token_string_t cmd_showport_reta_reta =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta, reta, "reta");
cmdline_parse_token_num_t cmd_showport_reta_size =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_reta, size, UINT16);
cmdline_parse_token_string_t cmd_showport_reta_list_of_items =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta,
					list_of_items, NULL);

cmdline_parse_inst_t cmd_showport_reta = {
	.f = cmd_showport_reta_parsed,
	.data = NULL,
	.help_str = "show port X rss reta (size) (mask0,mask1,...)",
	.tokens = {
		(void *)&cmd_showport_reta_show,
		(void *)&cmd_showport_reta_port,
		(void *)&cmd_showport_reta_port_id,
		(void *)&cmd_showport_reta_rss,
		(void *)&cmd_showport_reta_reta,
		(void *)&cmd_showport_reta_size,
		(void *)&cmd_showport_reta_list_of_items,
		NULL,
	},
};

/* *** Show RSS hash configuration *** */
struct cmd_showport_rss_hash {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t rss_hash;
	cmdline_fixed_string_t rss_type;
	cmdline_fixed_string_t key; /* optional argument */
};

static void cmd_showport_rss_hash_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				void *show_rss_key)
{
	struct cmd_showport_rss_hash *res = parsed_result;

	port_rss_hash_conf_show(res->port_id, res->rss_type,
				show_rss_key != NULL);
}

cmdline_parse_token_string_t cmd_showport_rss_hash_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, show, "show");
cmdline_parse_token_string_t cmd_showport_rss_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, port, "port");
cmdline_parse_token_num_t cmd_showport_rss_hash_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_rss_hash, port_id, UINT8);
cmdline_parse_token_string_t cmd_showport_rss_hash_rss_hash =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, rss_hash,
				 "rss-hash");
cmdline_parse_token_string_t cmd_showport_rss_hash_rss_hash_info =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, rss_type,
				 "ipv4#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#"
				 "ipv4-other#ipv6#ipv6-frag#ipv6-tcp#ipv6-udp#"
				 "ipv6-sctp#ipv6-other#l2-payload#ipv6-ex#"
				 "ipv6-tcp-ex#ipv6-udp-ex");
cmdline_parse_token_string_t cmd_showport_rss_hash_rss_key =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, key, "key");

cmdline_parse_inst_t cmd_showport_rss_hash = {
	.f = cmd_showport_rss_hash_parsed,
	.data = NULL,
	.help_str =
		"show port X rss-hash ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|"
		"ipv4-sctp|ipv4-other|ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|"
		"ipv6-sctp|ipv6-other|l2-payload|"
		"ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex (X = port number)\n",
	.tokens = {
		(void *)&cmd_showport_rss_hash_show,
		(void *)&cmd_showport_rss_hash_port,
		(void *)&cmd_showport_rss_hash_port_id,
		(void *)&cmd_showport_rss_hash_rss_hash,
		(void *)&cmd_showport_rss_hash_rss_hash_info,
		NULL,
	},
};

cmdline_parse_inst_t cmd_showport_rss_hash_key = {
	.f = cmd_showport_rss_hash_parsed,
	.data = (void *)1,
	.help_str =
		"show port X rss-hash ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|"
		"ipv4-sctp|ipv4-other|ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|"
		"ipv6-sctp|ipv6-other|l2-payload|"
		"ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex key (X = port number)\n",
	.tokens = {
		(void *)&cmd_showport_rss_hash_show,
		(void *)&cmd_showport_rss_hash_port,
		(void *)&cmd_showport_rss_hash_port_id,
		(void *)&cmd_showport_rss_hash_rss_hash,
		(void *)&cmd_showport_rss_hash_rss_hash_info,
		(void *)&cmd_showport_rss_hash_rss_key,
		NULL,
	},
};

/* *** Configure DCB *** */
struct cmd_config_dcb {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	uint8_t port_id;
	cmdline_fixed_string_t dcb;
	cmdline_fixed_string_t vt;
	cmdline_fixed_string_t vt_en;
	uint8_t num_tcs;
	cmdline_fixed_string_t pfc;
	cmdline_fixed_string_t pfc_en;
};

static void
cmd_config_dcb_parsed(void *parsed_result,
                        __attribute__((unused)) struct cmdline *cl,
                        __attribute__((unused)) void *data)
{
	struct cmd_config_dcb *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_port *port;
	uint8_t pfc_en;
	int ret;

	port = &ports[port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		printf("Please stop port %d first\n", port_id);
		return;
	}

	if ((res->num_tcs != ETH_4_TCS) && (res->num_tcs != ETH_8_TCS)) {
		printf("The invalid number of traffic class,"
			" only 4 or 8 allowed.\n");
		return;
	}

	if (nb_fwd_lcores < res->num_tcs) {
		printf("nb_cores shouldn't be less than number of TCs.\n");
		return;
	}
	if (!strncmp(res->pfc_en, "on", 2))
		pfc_en = 1;
	else
		pfc_en = 0;

	/* DCB in VT mode */
	if (!strncmp(res->vt_en, "on", 2))
		ret = init_port_dcb_config(port_id, DCB_VT_ENABLED,
				(enum rte_eth_nb_tcs)res->num_tcs,
				pfc_en);
	else
		ret = init_port_dcb_config(port_id, DCB_ENABLED,
				(enum rte_eth_nb_tcs)res->num_tcs,
				pfc_en);


	if (ret != 0) {
		printf("Cannot initialize network ports.\n");
		return;
	}

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_config_dcb_port =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, port, "port");
cmdline_parse_token_string_t cmd_config_dcb_config =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, config, "config");
cmdline_parse_token_num_t cmd_config_dcb_port_id =
        TOKEN_NUM_INITIALIZER(struct cmd_config_dcb, port_id, UINT8);
cmdline_parse_token_string_t cmd_config_dcb_dcb =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, dcb, "dcb");
cmdline_parse_token_string_t cmd_config_dcb_vt =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, vt, "vt");
cmdline_parse_token_string_t cmd_config_dcb_vt_en =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, vt_en, "on#off");
cmdline_parse_token_num_t cmd_config_dcb_num_tcs =
        TOKEN_NUM_INITIALIZER(struct cmd_config_dcb, num_tcs, UINT8);
cmdline_parse_token_string_t cmd_config_dcb_pfc=
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, pfc, "pfc");
cmdline_parse_token_string_t cmd_config_dcb_pfc_en =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, pfc_en, "on#off");

cmdline_parse_inst_t cmd_config_dcb = {
        .f = cmd_config_dcb_parsed,
        .data = NULL,
        .help_str = "port config port-id dcb vt on|off nb-tcs pfc on|off",
        .tokens = {
		(void *)&cmd_config_dcb_port,
		(void *)&cmd_config_dcb_config,
		(void *)&cmd_config_dcb_port_id,
		(void *)&cmd_config_dcb_dcb,
		(void *)&cmd_config_dcb_vt,
		(void *)&cmd_config_dcb_vt_en,
		(void *)&cmd_config_dcb_num_tcs,
		(void *)&cmd_config_dcb_pfc,
		(void *)&cmd_config_dcb_pfc_en,
                NULL,
        },
};

/* *** configure number of packets per burst *** */
struct cmd_config_burst {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint16_t value;
};

static void
cmd_config_burst_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_burst *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "burst")) {
		if (res->value < 1 || res->value > MAX_PKT_BURST) {
			printf("burst must be >= 1 && <= %d\n", MAX_PKT_BURST);
			return;
		}
		nb_pkt_per_burst = res->value;
	} else {
		printf("Unknown parameter\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_burst_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_burst, port, "port");
cmdline_parse_token_string_t cmd_config_burst_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_burst, keyword, "config");
cmdline_parse_token_string_t cmd_config_burst_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_burst, all, "all");
cmdline_parse_token_string_t cmd_config_burst_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_burst, name, "burst");
cmdline_parse_token_num_t cmd_config_burst_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_burst, value, UINT16);

cmdline_parse_inst_t cmd_config_burst = {
	.f = cmd_config_burst_parsed,
	.data = NULL,
	.help_str = "port config all burst value",
	.tokens = {
		(void *)&cmd_config_burst_port,
		(void *)&cmd_config_burst_keyword,
		(void *)&cmd_config_burst_all,
		(void *)&cmd_config_burst_name,
		(void *)&cmd_config_burst_value,
		NULL,
	},
};

/* *** configure rx/tx queues *** */
struct cmd_config_thresh {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint8_t value;
};

static void
cmd_config_thresh_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_thresh *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "txpt"))
		tx_pthresh = res->value;
	else if(!strcmp(res->name, "txht"))
		tx_hthresh = res->value;
	else if(!strcmp(res->name, "txwt"))
		tx_wthresh = res->value;
	else if(!strcmp(res->name, "rxpt"))
		rx_pthresh = res->value;
	else if(!strcmp(res->name, "rxht"))
		rx_hthresh = res->value;
	else if(!strcmp(res->name, "rxwt"))
		rx_wthresh = res->value;
	else {
		printf("Unknown parameter\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_thresh_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_thresh, port, "port");
cmdline_parse_token_string_t cmd_config_thresh_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_thresh, keyword, "config");
cmdline_parse_token_string_t cmd_config_thresh_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_thresh, all, "all");
cmdline_parse_token_string_t cmd_config_thresh_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_thresh, name,
				"txpt#txht#txwt#rxpt#rxht#rxwt");
cmdline_parse_token_num_t cmd_config_thresh_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_thresh, value, UINT8);

cmdline_parse_inst_t cmd_config_thresh = {
	.f = cmd_config_thresh_parsed,
	.data = NULL,
	.help_str = "port config all txpt|txht|txwt|rxpt|rxht|rxwt value",
	.tokens = {
		(void *)&cmd_config_thresh_port,
		(void *)&cmd_config_thresh_keyword,
		(void *)&cmd_config_thresh_all,
		(void *)&cmd_config_thresh_name,
		(void *)&cmd_config_thresh_value,
		NULL,
	},
};

/* *** configure free/rs threshold *** */
struct cmd_config_threshold {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint16_t value;
};

static void
cmd_config_threshold_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_config_threshold *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "txfreet"))
		tx_free_thresh = res->value;
	else if (!strcmp(res->name, "txrst"))
		tx_rs_thresh = res->value;
	else if (!strcmp(res->name, "rxfreet"))
		rx_free_thresh = res->value;
	else {
		printf("Unknown parameter\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_threshold_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_threshold, port, "port");
cmdline_parse_token_string_t cmd_config_threshold_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_threshold, keyword,
								"config");
cmdline_parse_token_string_t cmd_config_threshold_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_threshold, all, "all");
cmdline_parse_token_string_t cmd_config_threshold_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_threshold, name,
						"txfreet#txrst#rxfreet");
cmdline_parse_token_num_t cmd_config_threshold_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_threshold, value, UINT16);

cmdline_parse_inst_t cmd_config_threshold = {
	.f = cmd_config_threshold_parsed,
	.data = NULL,
	.help_str = "port config all txfreet|txrst|rxfreet value",
	.tokens = {
		(void *)&cmd_config_threshold_port,
		(void *)&cmd_config_threshold_keyword,
		(void *)&cmd_config_threshold_all,
		(void *)&cmd_config_threshold_name,
		(void *)&cmd_config_threshold_value,
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

unsigned int
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
			return 0;
		}
		if (! value_ok) {
			printf("No valid value before comma\n");
			return 0;
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
		return 0;
	}
	parsed_items[nb_item++] = value;
	if (! check_unique_values)
		return nb_item;

	/*
	 * Then, check that all values in the list are differents.
	 * No optimization here...
	 */
	for (i = 0; i < nb_item; i++) {
		for (j = i + 1; j < nb_item; j++) {
			if (parsed_items[j] == parsed_items[i]) {
				printf("duplicated %s %u at index %u and %u\n",
				       item_name, parsed_items[i], i, j);
				return 0;
			}
		}
	}
	return nb_item;
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

	if (test_done == 0) {
		printf("Please stop forwarding first\n");
		return;
	}

	res = parsed_result;
	if (!strcmp(res->list_name, "corelist")) {
		nb_item = parse_item_list(res->list_of_items, "core",
					  RTE_MAX_LCORE,
					  parsed_items.lcorelist, 1);
		if (nb_item > 0) {
			set_fwd_lcores_list(parsed_items.lcorelist, nb_item);
			fwd_config_setup();
		}
		return;
	}
	if (!strcmp(res->list_name, "portlist")) {
		nb_item = parse_item_list(res->list_of_items, "port",
					  RTE_MAX_ETHPORTS,
					  parsed_items.portlist, 1);
		if (nb_item > 0) {
			set_fwd_ports_list(parsed_items.portlist, nb_item);
			fwd_config_setup();
		}
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

	if (test_done == 0) {
		printf("Please stop forwarding first\n");
		return;
	}
	if (!strcmp(res->mask, "coremask")) {
		set_fwd_lcores_mask(res->hexavalue);
		fwd_config_setup();
	} else if (!strcmp(res->mask, "portmask")) {
		set_fwd_ports_mask(res->hexavalue);
		fwd_config_setup();
	}
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
	if (!strcmp(res->what, "nbport")) {
		set_fwd_ports_number(res->value);
		fwd_config_setup();
	} else if (!strcmp(res->what, "nbcore")) {
		set_fwd_lcores_number(res->value);
		fwd_config_setup();
	} else if (!strcmp(res->what, "burst"))
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

/* *** SET COPY AND SPLIT POLICY ON TX PACKETS *** */

struct cmd_set_txsplit_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t txsplit;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_txsplit_parsed(void *parsed_result,
		      __attribute__((unused)) struct cmdline *cl,
		      __attribute__((unused)) void *data)
{
	struct cmd_set_txsplit_result *res;

	res = parsed_result;
	set_tx_pkt_split(res->mode);
}

cmdline_parse_token_string_t cmd_set_txsplit_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txsplit_result,
				 cmd_keyword, "set");
cmdline_parse_token_string_t cmd_set_txsplit_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txsplit_result,
				 txsplit, "txsplit");
cmdline_parse_token_string_t cmd_set_txsplit_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txsplit_result,
				 mode, NULL);

cmdline_parse_inst_t cmd_set_txsplit = {
	.f = cmd_set_txsplit_parsed,
	.data = NULL,
	.help_str = "set txsplit on|off|rand",
	.tokens = {
		(void *)&cmd_set_txsplit_keyword,
		(void *)&cmd_set_txsplit_name,
		(void *)&cmd_set_txsplit_mode,
		NULL,
	},
};

/* *** CONFIG TX QUEUE FLAGS *** */

struct cmd_config_txqflags_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t what;
	int32_t hexvalue;
};

static void cmd_config_txqflags_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_config_txqflags_result *res = parsed_result;

	if (!all_ports_stopped()) {
		printf("Please stop all ports first\n");
		return;
	}

	if (strcmp(res->what, "txqflags")) {
		printf("Unknown parameter\n");
		return;
	}

	if (res->hexvalue >= 0) {
		txq_flags = res->hexvalue;
	} else {
		printf("txqflags must be >= 0\n");
		return;
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_txqflags_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_txqflags_result, port,
				 "port");
cmdline_parse_token_string_t cmd_config_txqflags_config =
	TOKEN_STRING_INITIALIZER(struct cmd_config_txqflags_result, config,
				 "config");
cmdline_parse_token_string_t cmd_config_txqflags_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_txqflags_result, all,
				 "all");
cmdline_parse_token_string_t cmd_config_txqflags_what =
	TOKEN_STRING_INITIALIZER(struct cmd_config_txqflags_result, what,
				 "txqflags");
cmdline_parse_token_num_t cmd_config_txqflags_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_txqflags_result,
				hexvalue, INT32);

cmdline_parse_inst_t cmd_config_txqflags = {
	.f = cmd_config_txqflags_parsed,
	.data = NULL,
	.help_str = "port config all txqflags value",
	.tokens = {
		(void *)&cmd_config_txqflags_port,
		(void *)&cmd_config_txqflags_config,
		(void *)&cmd_config_txqflags_all,
		(void *)&cmd_config_txqflags_what,
		(void *)&cmd_config_txqflags_value,
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

/* *** VLAN OFFLOAD SET ON A PORT *** */
struct cmd_vlan_offload_result {
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vlan_type;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t on;
	cmdline_fixed_string_t port_id;
};

static void
cmd_vlan_offload_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	int on;
	struct cmd_vlan_offload_result *res = parsed_result;
	char *str;
	int i, len = 0;
	portid_t port_id = 0;
	unsigned int tmp;

	str = res->port_id;
	len = strnlen(str, STR_TOKEN_SIZE);
	i = 0;
	/* Get port_id first */
	while(i < len){
		if(str[i] == ',')
			break;

		i++;
	}
	str[i]='\0';
	tmp = strtoul(str, NULL, 0);
	/* If port_id greater that what portid_t can represent, return */
	if(tmp >= RTE_MAX_ETHPORTS)
		return;
	port_id = (portid_t)tmp;

	if (!strcmp(res->on, "on"))
		on = 1;
	else
		on = 0;

	if (!strcmp(res->what, "strip"))
		rx_vlan_strip_set(port_id,  on);
	else if(!strcmp(res->what, "stripq")){
		uint16_t queue_id = 0;

		/* No queue_id, return */
		if(i + 1 >= len) {
			printf("must specify (port,queue_id)\n");
			return;
		}
		tmp = strtoul(str + i + 1, NULL, 0);
		/* If queue_id greater that what 16-bits can represent, return */
		if(tmp > 0xffff)
			return;

		queue_id = (uint16_t)tmp;
		rx_vlan_strip_set_on_queue(port_id, queue_id, on);
	}
	else if (!strcmp(res->what, "filter"))
		rx_vlan_filter_set(port_id, on);
	else
		vlan_extend_set(port_id, on);

	return;
}

cmdline_parse_token_string_t cmd_vlan_offload_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
				 vlan, "vlan");
cmdline_parse_token_string_t cmd_vlan_offload_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
				 set, "set");
cmdline_parse_token_string_t cmd_vlan_offload_what =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
				 what, "strip#filter#qinq#stripq");
cmdline_parse_token_string_t cmd_vlan_offload_on =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
			      on, "on#off");
cmdline_parse_token_string_t cmd_vlan_offload_portid =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
			      port_id, NULL);

cmdline_parse_inst_t cmd_vlan_offload = {
	.f = cmd_vlan_offload_parsed,
	.data = NULL,
	.help_str = "set strip|filter|qinq|stripq on|off port_id[,queue_id], filter/strip for rx side"
	" qinq(extended) for both rx/tx sides ",
	.tokens = {
		(void *)&cmd_vlan_offload_vlan,
		(void *)&cmd_vlan_offload_set,
		(void *)&cmd_vlan_offload_what,
		(void *)&cmd_vlan_offload_on,
		(void *)&cmd_vlan_offload_portid,
		NULL,
	},
};

/* *** VLAN TPID SET ON A PORT *** */
struct cmd_vlan_tpid_result {
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vlan_type;
	cmdline_fixed_string_t what;
	uint16_t tp_id;
	uint8_t port_id;
};

static void
cmd_vlan_tpid_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_vlan_tpid_result *res = parsed_result;
	enum rte_vlan_type vlan_type;

	if (!strcmp(res->vlan_type, "inner"))
		vlan_type = ETH_VLAN_TYPE_INNER;
	else if (!strcmp(res->vlan_type, "outer"))
		vlan_type = ETH_VLAN_TYPE_OUTER;
	else {
		printf("Unknown vlan type\n");
		return;
	}
	vlan_tpid_set(res->port_id, vlan_type, res->tp_id);
}

cmdline_parse_token_string_t cmd_vlan_tpid_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_tpid_result,
				 vlan, "vlan");
cmdline_parse_token_string_t cmd_vlan_tpid_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_tpid_result,
				 set, "set");
cmdline_parse_token_string_t cmd_vlan_type =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_tpid_result,
				 vlan_type, "inner#outer");
cmdline_parse_token_string_t cmd_vlan_tpid_what =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_tpid_result,
				 what, "tpid");
cmdline_parse_token_num_t cmd_vlan_tpid_tpid =
	TOKEN_NUM_INITIALIZER(struct cmd_vlan_tpid_result,
			      tp_id, UINT16);
cmdline_parse_token_num_t cmd_vlan_tpid_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_vlan_tpid_result,
			      port_id, UINT8);

cmdline_parse_inst_t cmd_vlan_tpid = {
	.f = cmd_vlan_tpid_parsed,
	.data = NULL,
	.help_str = "set inner|outer tpid tp_id port_id, set the VLAN "
		    "Ether type",
	.tokens = {
		(void *)&cmd_vlan_tpid_vlan,
		(void *)&cmd_vlan_tpid_set,
		(void *)&cmd_vlan_type,
		(void *)&cmd_vlan_tpid_what,
		(void *)&cmd_vlan_tpid_tpid,
		(void *)&cmd_vlan_tpid_portid,
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
		rx_vft_set(res->port_id, res->vlan_id, 1);
	else
		rx_vft_set(res->port_id, res->vlan_id, 0);
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
	uint8_t port_id;
	uint16_t vlan_id;
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
cmdline_parse_token_num_t cmd_tx_vlan_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      port_id, UINT8);
cmdline_parse_token_num_t cmd_tx_vlan_set_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      vlan_id, UINT16);

cmdline_parse_inst_t cmd_tx_vlan_set = {
	.f = cmd_tx_vlan_set_parsed,
	.data = NULL,
	.help_str = "enable hardware insertion of a single VLAN header "
		"with a given TAG Identifier in packets sent on a port",
	.tokens = {
		(void *)&cmd_tx_vlan_set_tx_vlan,
		(void *)&cmd_tx_vlan_set_set,
		(void *)&cmd_tx_vlan_set_portid,
		(void *)&cmd_tx_vlan_set_vlanid,
		NULL,
	},
};

/* *** ENABLE HARDWARE INSERTION OF Double VLAN HEADER IN TX PACKETS *** */
struct cmd_tx_vlan_set_qinq_result {
	cmdline_fixed_string_t tx_vlan;
	cmdline_fixed_string_t set;
	uint8_t port_id;
	uint16_t vlan_id;
	uint16_t vlan_id_outer;
};

static void
cmd_tx_vlan_set_qinq_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_tx_vlan_set_qinq_result *res = parsed_result;

	tx_qinq_set(res->port_id, res->vlan_id, res->vlan_id_outer);
}

cmdline_parse_token_string_t cmd_tx_vlan_set_qinq_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_set_qinq_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		set, "set");
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		port_id, UINT8);
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		vlan_id, UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_vlanid_outer =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		vlan_id_outer, UINT16);

cmdline_parse_inst_t cmd_tx_vlan_set_qinq = {
	.f = cmd_tx_vlan_set_qinq_parsed,
	.data = NULL,
	.help_str = "enable hardware insertion of double VLAN header "
		"with given TAG Identifiers in packets sent on a port",
	.tokens = {
		(void *)&cmd_tx_vlan_set_qinq_tx_vlan,
		(void *)&cmd_tx_vlan_set_qinq_set,
		(void *)&cmd_tx_vlan_set_qinq_portid,
		(void *)&cmd_tx_vlan_set_qinq_vlanid,
		(void *)&cmd_tx_vlan_set_qinq_vlanid_outer,
		NULL,
	},
};

/* *** ENABLE/DISABLE PORT BASED TX VLAN INSERTION *** */
struct cmd_tx_vlan_set_pvid_result {
	cmdline_fixed_string_t tx_vlan;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t pvid;
	uint8_t port_id;
	uint16_t vlan_id;
	cmdline_fixed_string_t mode;
};

static void
cmd_tx_vlan_set_pvid_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_tx_vlan_set_pvid_result *res = parsed_result;

	if (strcmp(res->mode, "on") == 0)
		tx_vlan_pvid_set(res->port_id, res->vlan_id, 1);
	else
		tx_vlan_pvid_set(res->port_id, res->vlan_id, 0);
}

cmdline_parse_token_string_t cmd_tx_vlan_set_pvid_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
				 tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_set_pvid_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
				 set, "set");
cmdline_parse_token_string_t cmd_tx_vlan_set_pvid_pvid =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
				 pvid, "pvid");
cmdline_parse_token_num_t cmd_tx_vlan_set_pvid_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
			     port_id, UINT8);
cmdline_parse_token_num_t cmd_tx_vlan_set_pvid_vlan_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
			      vlan_id, UINT16);
cmdline_parse_token_string_t cmd_tx_vlan_set_pvid_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
				 mode, "on#off");

cmdline_parse_inst_t cmd_tx_vlan_set_pvid = {
	.f = cmd_tx_vlan_set_pvid_parsed,
	.data = NULL,
	.help_str = "tx_vlan set pvid port_id vlan_id (on|off)",
	.tokens = {
		(void *)&cmd_tx_vlan_set_pvid_tx_vlan,
		(void *)&cmd_tx_vlan_set_pvid_set,
		(void *)&cmd_tx_vlan_set_pvid_pvid,
		(void *)&cmd_tx_vlan_set_pvid_port_id,
		(void *)&cmd_tx_vlan_set_pvid_vlan_id,
		(void *)&cmd_tx_vlan_set_pvid_mode,
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
struct cmd_csum_result {
	cmdline_fixed_string_t csum;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t proto;
	cmdline_fixed_string_t hwsw;
	uint8_t port_id;
};

static void
csum_show(int port_id)
{
	struct rte_eth_dev_info dev_info;
	uint16_t ol_flags;

	ol_flags = ports[port_id].tx_ol_flags;
	printf("Parse tunnel is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_PARSE_TUNNEL) ? "on" : "off");
	printf("IP checksum offload is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_IP_CKSUM) ? "hw" : "sw");
	printf("UDP checksum offload is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_UDP_CKSUM) ? "hw" : "sw");
	printf("TCP checksum offload is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_TCP_CKSUM) ? "hw" : "sw");
	printf("SCTP checksum offload is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_SCTP_CKSUM) ? "hw" : "sw");
	printf("Outer-Ip checksum offload is %s\n",
		(ol_flags & TESTPMD_TX_OFFLOAD_OUTER_IP_CKSUM) ? "hw" : "sw");

	/* display warnings if configuration is not supported by the NIC */
	rte_eth_dev_info_get(port_id, &dev_info);
	if ((ol_flags & TESTPMD_TX_OFFLOAD_IP_CKSUM) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) == 0) {
		printf("Warning: hardware IP checksum enabled but not "
			"supported by port %d\n", port_id);
	}
	if ((ol_flags & TESTPMD_TX_OFFLOAD_UDP_CKSUM) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) == 0) {
		printf("Warning: hardware UDP checksum enabled but not "
			"supported by port %d\n", port_id);
	}
	if ((ol_flags & TESTPMD_TX_OFFLOAD_TCP_CKSUM) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0) {
		printf("Warning: hardware TCP checksum enabled but not "
			"supported by port %d\n", port_id);
	}
	if ((ol_flags & TESTPMD_TX_OFFLOAD_SCTP_CKSUM) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_SCTP_CKSUM) == 0) {
		printf("Warning: hardware SCTP checksum enabled but not "
			"supported by port %d\n", port_id);
	}
	if ((ol_flags & TESTPMD_TX_OFFLOAD_OUTER_IP_CKSUM) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) == 0) {
		printf("Warning: hardware outer IP checksum enabled but not "
			"supported by port %d\n", port_id);
	}
}

static void
cmd_csum_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_csum_result *res = parsed_result;
	int hw = 0;
	uint16_t mask = 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN)) {
		printf("invalid port %d\n", res->port_id);
		return;
	}

	if (!strcmp(res->mode, "set")) {

		if (!strcmp(res->hwsw, "hw"))
			hw = 1;

		if (!strcmp(res->proto, "ip")) {
			mask = TESTPMD_TX_OFFLOAD_IP_CKSUM;
		} else if (!strcmp(res->proto, "udp")) {
			mask = TESTPMD_TX_OFFLOAD_UDP_CKSUM;
		} else if (!strcmp(res->proto, "tcp")) {
			mask = TESTPMD_TX_OFFLOAD_TCP_CKSUM;
		} else if (!strcmp(res->proto, "sctp")) {
			mask = TESTPMD_TX_OFFLOAD_SCTP_CKSUM;
		} else if (!strcmp(res->proto, "outer-ip")) {
			mask = TESTPMD_TX_OFFLOAD_OUTER_IP_CKSUM;
		}

		if (hw)
			ports[res->port_id].tx_ol_flags |= mask;
		else
			ports[res->port_id].tx_ol_flags &= (~mask);
	}
	csum_show(res->port_id);
}

cmdline_parse_token_string_t cmd_csum_csum =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				csum, "csum");
cmdline_parse_token_string_t cmd_csum_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				mode, "set");
cmdline_parse_token_string_t cmd_csum_proto =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				proto, "ip#tcp#udp#sctp#outer-ip");
cmdline_parse_token_string_t cmd_csum_hwsw =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				hwsw, "hw#sw");
cmdline_parse_token_num_t cmd_csum_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_csum_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_csum_set = {
	.f = cmd_csum_parsed,
	.data = NULL,
	.help_str = "enable/disable hardware calculation of L3/L4 checksum when "
		"using csum forward engine: csum set ip|tcp|udp|sctp|outer-ip hw|sw <port>",
	.tokens = {
		(void *)&cmd_csum_csum,
		(void *)&cmd_csum_mode,
		(void *)&cmd_csum_proto,
		(void *)&cmd_csum_hwsw,
		(void *)&cmd_csum_portid,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_csum_mode_show =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				mode, "show");

cmdline_parse_inst_t cmd_csum_show = {
	.f = cmd_csum_parsed,
	.data = NULL,
	.help_str = "show checksum offload configuration: csum show <port>",
	.tokens = {
		(void *)&cmd_csum_csum,
		(void *)&cmd_csum_mode_show,
		(void *)&cmd_csum_portid,
		NULL,
	},
};

/* Enable/disable tunnel parsing */
struct cmd_csum_tunnel_result {
	cmdline_fixed_string_t csum;
	cmdline_fixed_string_t parse;
	cmdline_fixed_string_t onoff;
	uint8_t port_id;
};

static void
cmd_csum_tunnel_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_csum_tunnel_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!strcmp(res->onoff, "on"))
		ports[res->port_id].tx_ol_flags |=
			TESTPMD_TX_OFFLOAD_PARSE_TUNNEL;
	else
		ports[res->port_id].tx_ol_flags &=
			(~TESTPMD_TX_OFFLOAD_PARSE_TUNNEL);

	csum_show(res->port_id);
}

cmdline_parse_token_string_t cmd_csum_tunnel_csum =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				csum, "csum");
cmdline_parse_token_string_t cmd_csum_tunnel_parse =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				parse, "parse_tunnel");
cmdline_parse_token_string_t cmd_csum_tunnel_onoff =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				onoff, "on#off");
cmdline_parse_token_num_t cmd_csum_tunnel_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_csum_tunnel_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_csum_tunnel = {
	.f = cmd_csum_tunnel_parsed,
	.data = NULL,
	.help_str = "enable/disable parsing of tunnels for csum engine: "
	"csum parse_tunnel on|off <tx-port>",
	.tokens = {
		(void *)&cmd_csum_tunnel_csum,
		(void *)&cmd_csum_tunnel_parse,
		(void *)&cmd_csum_tunnel_onoff,
		(void *)&cmd_csum_tunnel_portid,
		NULL,
	},
};

/* *** ENABLE HARDWARE SEGMENTATION IN TX NON-TUNNELED PACKETS *** */
struct cmd_tso_set_result {
	cmdline_fixed_string_t tso;
	cmdline_fixed_string_t mode;
	uint16_t tso_segsz;
	uint8_t port_id;
};

static void
cmd_tso_set_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_tso_set_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!strcmp(res->mode, "set"))
		ports[res->port_id].tso_segsz = res->tso_segsz;

	if (ports[res->port_id].tso_segsz == 0)
		printf("TSO for non-tunneled packets is disabled\n");
	else
		printf("TSO segment size for non-tunneled packets is %d\n",
			ports[res->port_id].tso_segsz);

	/* display warnings if configuration is not supported by the NIC */
	rte_eth_dev_info_get(res->port_id, &dev_info);
	if ((ports[res->port_id].tso_segsz != 0) &&
		(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_TSO) == 0) {
		printf("Warning: TSO enabled but not "
			"supported by port %d\n", res->port_id);
	}
}

cmdline_parse_token_string_t cmd_tso_set_tso =
	TOKEN_STRING_INITIALIZER(struct cmd_tso_set_result,
				tso, "tso");
cmdline_parse_token_string_t cmd_tso_set_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tso_set_result,
				mode, "set");
cmdline_parse_token_num_t cmd_tso_set_tso_segsz =
	TOKEN_NUM_INITIALIZER(struct cmd_tso_set_result,
				tso_segsz, UINT16);
cmdline_parse_token_num_t cmd_tso_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tso_set_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_tso_set = {
	.f = cmd_tso_set_parsed,
	.data = NULL,
	.help_str = "Set TSO segment size of non-tunneled packets "
	"for csum engine (0 to disable): tso set <tso_segsz> <port>",
	.tokens = {
		(void *)&cmd_tso_set_tso,
		(void *)&cmd_tso_set_mode,
		(void *)&cmd_tso_set_tso_segsz,
		(void *)&cmd_tso_set_portid,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_tso_show_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tso_set_result,
				mode, "show");


cmdline_parse_inst_t cmd_tso_show = {
	.f = cmd_tso_set_parsed,
	.data = NULL,
	.help_str = "Show TSO segment size of non-tunneled packets "
	"for csum engine: tso show <port>",
	.tokens = {
		(void *)&cmd_tso_set_tso,
		(void *)&cmd_tso_show_mode,
		(void *)&cmd_tso_set_portid,
		NULL,
	},
};

/* *** ENABLE HARDWARE SEGMENTATION IN TX TUNNELED PACKETS *** */
struct cmd_tunnel_tso_set_result {
	cmdline_fixed_string_t tso;
	cmdline_fixed_string_t mode;
	uint16_t tso_segsz;
	uint8_t port_id;
};

static void
check_tunnel_tso_nic_support(uint8_t port_id)
{
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get(port_id, &dev_info);
	if (!(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_VXLAN_TNL_TSO))
		printf("Warning: TSO enabled but VXLAN TUNNEL TSO not "
		       "supported by port %d\n", port_id);
	if (!(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_GRE_TNL_TSO))
		printf("Warning: TSO enabled but GRE TUNNEL TSO not "
			"supported by port %d\n", port_id);
	if (!(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPIP_TNL_TSO))
		printf("Warning: TSO enabled but IPIP TUNNEL TSO not "
		       "supported by port %d\n", port_id);
	if (!(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_GENEVE_TNL_TSO))
		printf("Warning: TSO enabled but GENEVE TUNNEL TSO not "
		       "supported by port %d\n", port_id);
}

static void
cmd_tunnel_tso_set_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_tunnel_tso_set_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!strcmp(res->mode, "set"))
		ports[res->port_id].tunnel_tso_segsz = res->tso_segsz;

	if (ports[res->port_id].tunnel_tso_segsz == 0)
		printf("TSO for tunneled packets is disabled\n");
	else {
		printf("TSO segment size for tunneled packets is %d\n",
			ports[res->port_id].tunnel_tso_segsz);

		/* Below conditions are needed to make it work:
		 * (1) tunnel TSO is supported by the NIC;
		 * (2) "csum parse_tunnel" must be set so that tunneled pkts
		 * are recognized;
		 * (3) for tunneled pkts with outer L3 of IPv4,
		 * "csum set outer-ip" must be set to hw, because after tso,
		 * total_len of outer IP header is changed, and the checksum
		 * of outer IP header calculated by sw should be wrong; that
		 * is not necessary for IPv6 tunneled pkts because there's no
		 * checksum in IP header anymore.
		 */
		check_tunnel_tso_nic_support(res->port_id);

		if (!(ports[res->port_id].tx_ol_flags &
		      TESTPMD_TX_OFFLOAD_PARSE_TUNNEL))
			printf("Warning: csum parse_tunnel must be set "
				"so that tunneled packets are recognized\n");
		if (!(ports[res->port_id].tx_ol_flags &
		      TESTPMD_TX_OFFLOAD_OUTER_IP_CKSUM))
			printf("Warning: csum set outer-ip must be set to hw "
				"if outer L3 is IPv4; not necessary for IPv6\n");
	}
}

cmdline_parse_token_string_t cmd_tunnel_tso_set_tso =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_tso_set_result,
				tso, "tunnel_tso");
cmdline_parse_token_string_t cmd_tunnel_tso_set_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_tso_set_result,
				mode, "set");
cmdline_parse_token_num_t cmd_tunnel_tso_set_tso_segsz =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_tso_set_result,
				tso_segsz, UINT16);
cmdline_parse_token_num_t cmd_tunnel_tso_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_tso_set_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_tunnel_tso_set = {
	.f = cmd_tunnel_tso_set_parsed,
	.data = NULL,
	.help_str = "Set TSO segment size of tunneled packets for csum engine "
	"(0 to disable): tunnel_tso set <tso_segsz> <port>",
	.tokens = {
		(void *)&cmd_tunnel_tso_set_tso,
		(void *)&cmd_tunnel_tso_set_mode,
		(void *)&cmd_tunnel_tso_set_tso_segsz,
		(void *)&cmd_tunnel_tso_set_portid,
		NULL,
	},
};

cmdline_parse_token_string_t cmd_tunnel_tso_show_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_tso_set_result,
				mode, "show");


cmdline_parse_inst_t cmd_tunnel_tso_show = {
	.f = cmd_tunnel_tso_set_parsed,
	.data = NULL,
	.help_str = "Show TSO segment size of tunneled packets "
	"for csum engine: tunnel_tso show <port>",
	.tokens = {
		(void *)&cmd_tunnel_tso_set_tso,
		(void *)&cmd_tunnel_tso_show_mode,
		(void *)&cmd_tunnel_tso_set_portid,
		NULL,
	},
};

/* *** ENABLE/DISABLE FLUSH ON RX STREAMS *** */
struct cmd_set_flush_rx {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t flush_rx;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_flush_rx_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_flush_rx *res = parsed_result;
	no_flush_rx = (uint8_t)((strcmp(res->mode, "on") == 0) ? 0 : 1);
}

cmdline_parse_token_string_t cmd_setflushrx_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_flush_rx,
			set, "set");
cmdline_parse_token_string_t cmd_setflushrx_flush_rx =
	TOKEN_STRING_INITIALIZER(struct cmd_set_flush_rx,
			flush_rx, "flush_rx");
cmdline_parse_token_string_t cmd_setflushrx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_flush_rx,
			mode, "on#off");


cmdline_parse_inst_t cmd_set_flush_rx = {
	.f = cmd_set_flush_rx_parsed,
	.help_str = "set flush_rx on|off: enable/disable flush on rx streams",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setflushrx_set,
		(void *)&cmd_setflushrx_flush_rx,
		(void *)&cmd_setflushrx_mode,
		NULL,
	},
};

/* *** ENABLE/DISABLE LINK STATUS CHECK *** */
struct cmd_set_link_check {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t link_check;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_link_check_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_link_check *res = parsed_result;
	no_link_check = (uint8_t)((strcmp(res->mode, "on") == 0) ? 0 : 1);
}

cmdline_parse_token_string_t cmd_setlinkcheck_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_check,
			set, "set");
cmdline_parse_token_string_t cmd_setlinkcheck_link_check =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_check,
			link_check, "link_check");
cmdline_parse_token_string_t cmd_setlinkcheck_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_check,
			mode, "on#off");


cmdline_parse_inst_t cmd_set_link_check = {
	.f = cmd_set_link_check_parsed,
	.help_str = "set link_check on|off: enable/disable link status check "
	            "when starting/stopping a port",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setlinkcheck_set,
		(void *)&cmd_setlinkcheck_link_check,
		(void *)&cmd_setlinkcheck_mode,
		NULL,
	},
};

#ifdef RTE_NIC_BYPASS
/* *** SET NIC BYPASS MODE *** */
struct cmd_set_bypass_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bypass;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t value;
	uint8_t port_id;
};

static void
cmd_set_bypass_mode_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bypass_mode_result *res = parsed_result;
	portid_t port_id = res->port_id;
	uint32_t bypass_mode = RTE_BYPASS_MODE_NORMAL;

	if (!strcmp(res->value, "bypass"))
		bypass_mode = RTE_BYPASS_MODE_BYPASS;
	else if (!strcmp(res->value, "isolate"))
		bypass_mode = RTE_BYPASS_MODE_ISOLATE;
	else
		bypass_mode = RTE_BYPASS_MODE_NORMAL;

	/* Set the bypass mode for the relevant port. */
	if (0 != rte_eth_dev_bypass_state_set(port_id, &bypass_mode)) {
		printf("\t Failed to set bypass mode for port = %d.\n", port_id);
	}
}

cmdline_parse_token_string_t cmd_setbypass_mode_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_mode_result,
			set, "set");
cmdline_parse_token_string_t cmd_setbypass_mode_bypass =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_mode_result,
			bypass, "bypass");
cmdline_parse_token_string_t cmd_setbypass_mode_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_mode_result,
			mode, "mode");
cmdline_parse_token_string_t cmd_setbypass_mode_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_mode_result,
			value, "normal#bypass#isolate");
cmdline_parse_token_num_t cmd_setbypass_mode_port =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bypass_mode_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_set_bypass_mode = {
	.f = cmd_set_bypass_mode_parsed,
	.help_str = "set bypass mode (normal|bypass|isolate) (port_id): "
	            "Set the NIC bypass mode for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbypass_mode_set,
		(void *)&cmd_setbypass_mode_bypass,
		(void *)&cmd_setbypass_mode_mode,
		(void *)&cmd_setbypass_mode_value,
		(void *)&cmd_setbypass_mode_port,
		NULL,
	},
};

/* *** SET NIC BYPASS EVENT *** */
struct cmd_set_bypass_event_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bypass;
	cmdline_fixed_string_t event;
	cmdline_fixed_string_t event_value;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t mode_value;
	uint8_t port_id;
};

static void
cmd_set_bypass_event_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	int32_t rc;
	struct cmd_set_bypass_event_result *res = parsed_result;
	portid_t port_id = res->port_id;
	uint32_t bypass_event = RTE_BYPASS_EVENT_NONE;
	uint32_t bypass_mode = RTE_BYPASS_MODE_NORMAL;

	if (!strcmp(res->event_value, "timeout"))
		bypass_event = RTE_BYPASS_EVENT_TIMEOUT;
	else if (!strcmp(res->event_value, "os_on"))
		bypass_event = RTE_BYPASS_EVENT_OS_ON;
	else if (!strcmp(res->event_value, "os_off"))
		bypass_event = RTE_BYPASS_EVENT_OS_OFF;
	else if (!strcmp(res->event_value, "power_on"))
		bypass_event = RTE_BYPASS_EVENT_POWER_ON;
	else if (!strcmp(res->event_value, "power_off"))
		bypass_event = RTE_BYPASS_EVENT_POWER_OFF;
	else
		bypass_event = RTE_BYPASS_EVENT_NONE;

	if (!strcmp(res->mode_value, "bypass"))
		bypass_mode = RTE_BYPASS_MODE_BYPASS;
	else if (!strcmp(res->mode_value, "isolate"))
		bypass_mode = RTE_BYPASS_MODE_ISOLATE;
	else
		bypass_mode = RTE_BYPASS_MODE_NORMAL;

	/* Set the watchdog timeout. */
	if (bypass_event == RTE_BYPASS_EVENT_TIMEOUT) {

		rc = -EINVAL;
		if (!RTE_BYPASS_TMT_VALID(bypass_timeout) ||
				(rc = rte_eth_dev_wd_timeout_store(port_id,
				bypass_timeout)) != 0) {
			printf("Failed to set timeout value %u "
				"for port %d, errto code: %d.\n",
				bypass_timeout, port_id, rc);
		}
	}

	/* Set the bypass event to transition to bypass mode. */
	if (0 != rte_eth_dev_bypass_event_store(port_id,
			bypass_event, bypass_mode)) {
		printf("\t Failed to set bypass event for port = %d.\n", port_id);
	}

}

cmdline_parse_token_string_t cmd_setbypass_event_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			set, "set");
cmdline_parse_token_string_t cmd_setbypass_event_bypass =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			bypass, "bypass");
cmdline_parse_token_string_t cmd_setbypass_event_event =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			event, "event");
cmdline_parse_token_string_t cmd_setbypass_event_event_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			event_value, "none#timeout#os_off#os_on#power_on#power_off");
cmdline_parse_token_string_t cmd_setbypass_event_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			mode, "mode");
cmdline_parse_token_string_t cmd_setbypass_event_mode_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_event_result,
			mode_value, "normal#bypass#isolate");
cmdline_parse_token_num_t cmd_setbypass_event_port =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bypass_event_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_set_bypass_event = {
	.f = cmd_set_bypass_event_parsed,
	.help_str = "set bypass event (timeout|os_on|os_off|power_on|power_off) "
	            "mode (normal|bypass|isolate) (port_id): "
	            "Set the NIC bypass event mode for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbypass_event_set,
		(void *)&cmd_setbypass_event_bypass,
		(void *)&cmd_setbypass_event_event,
		(void *)&cmd_setbypass_event_event_value,
		(void *)&cmd_setbypass_event_mode,
		(void *)&cmd_setbypass_event_mode_value,
		(void *)&cmd_setbypass_event_port,
		NULL,
	},
};


/* *** SET NIC BYPASS TIMEOUT *** */
struct cmd_set_bypass_timeout_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bypass;
	cmdline_fixed_string_t timeout;
	cmdline_fixed_string_t value;
};

static void
cmd_set_bypass_timeout_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bypass_timeout_result *res = parsed_result;

	if (!strcmp(res->value, "1.5"))
		bypass_timeout = RTE_BYPASS_TMT_1_5_SEC;
	else if (!strcmp(res->value, "2"))
		bypass_timeout = RTE_BYPASS_TMT_2_SEC;
	else if (!strcmp(res->value, "3"))
		bypass_timeout = RTE_BYPASS_TMT_3_SEC;
	else if (!strcmp(res->value, "4"))
		bypass_timeout = RTE_BYPASS_TMT_4_SEC;
	else if (!strcmp(res->value, "8"))
		bypass_timeout = RTE_BYPASS_TMT_8_SEC;
	else if (!strcmp(res->value, "16"))
		bypass_timeout = RTE_BYPASS_TMT_16_SEC;
	else if (!strcmp(res->value, "32"))
		bypass_timeout = RTE_BYPASS_TMT_32_SEC;
	else
		bypass_timeout = RTE_BYPASS_TMT_OFF;
}

cmdline_parse_token_string_t cmd_setbypass_timeout_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_timeout_result,
			set, "set");
cmdline_parse_token_string_t cmd_setbypass_timeout_bypass =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_timeout_result,
			bypass, "bypass");
cmdline_parse_token_string_t cmd_setbypass_timeout_timeout =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_timeout_result,
			timeout, "timeout");
cmdline_parse_token_string_t cmd_setbypass_timeout_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bypass_timeout_result,
			value, "0#1.5#2#3#4#8#16#32");

cmdline_parse_inst_t cmd_set_bypass_timeout = {
	.f = cmd_set_bypass_timeout_parsed,
	.help_str = "set bypass timeout (0|1.5|2|3|4|8|16|32) seconds: "
	            "Set the NIC bypass watchdog timeout",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setbypass_timeout_set,
		(void *)&cmd_setbypass_timeout_bypass,
		(void *)&cmd_setbypass_timeout_timeout,
		(void *)&cmd_setbypass_timeout_value,
		NULL,
	},
};

/* *** SHOW NIC BYPASS MODE *** */
struct cmd_show_bypass_config_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bypass;
	cmdline_fixed_string_t config;
	uint8_t port_id;
};

static void
cmd_show_bypass_config_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_show_bypass_config_result *res = parsed_result;
	uint32_t event_mode;
	uint32_t bypass_mode;
	portid_t port_id = res->port_id;
	uint32_t timeout = bypass_timeout;
	int i;

	static const char * const timeouts[RTE_BYPASS_TMT_NUM] =
		{"off", "1.5", "2", "3", "4", "8", "16", "32"};
	static const char * const modes[RTE_BYPASS_MODE_NUM] =
		{"UNKNOWN", "normal", "bypass", "isolate"};
	static const char * const events[RTE_BYPASS_EVENT_NUM] = {
		"NONE",
		"OS/board on",
		"power supply on",
		"OS/board off",
		"power supply off",
		"timeout"};
	int num_events = (sizeof events) / (sizeof events[0]);

	/* Display the bypass mode.*/
	if (0 != rte_eth_dev_bypass_state_show(port_id, &bypass_mode)) {
		printf("\tFailed to get bypass mode for port = %d\n", port_id);
		return;
	}
	else {
		if (!RTE_BYPASS_MODE_VALID(bypass_mode))
			bypass_mode = RTE_BYPASS_MODE_NONE;

		printf("\tbypass mode    = %s\n",  modes[bypass_mode]);
	}

	/* Display the bypass timeout.*/
	if (!RTE_BYPASS_TMT_VALID(timeout))
		timeout = RTE_BYPASS_TMT_OFF;

	printf("\tbypass timeout = %s\n", timeouts[timeout]);

	/* Display the bypass events and associated modes. */
	for (i = RTE_BYPASS_EVENT_START; i < num_events; i++) {

		if (0 != rte_eth_dev_bypass_event_show(port_id, i, &event_mode)) {
			printf("\tFailed to get bypass mode for event = %s\n",
				events[i]);
		} else {
			if (!RTE_BYPASS_MODE_VALID(event_mode))
				event_mode = RTE_BYPASS_MODE_NONE;

			printf("\tbypass event: %-16s = %s\n", events[i],
				modes[event_mode]);
		}
	}
}

cmdline_parse_token_string_t cmd_showbypass_config_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bypass_config_result,
			show, "show");
cmdline_parse_token_string_t cmd_showbypass_config_bypass =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bypass_config_result,
			bypass, "bypass");
cmdline_parse_token_string_t cmd_showbypass_config_config =
	TOKEN_STRING_INITIALIZER(struct cmd_show_bypass_config_result,
			config, "config");
cmdline_parse_token_num_t cmd_showbypass_config_port =
	TOKEN_NUM_INITIALIZER(struct cmd_show_bypass_config_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_show_bypass_config = {
	.f = cmd_show_bypass_config_parsed,
	.help_str = "show bypass config (port_id): "
	            "Show the NIC bypass config for port_id",
	.data = NULL,
	.tokens = {
		(void *)&cmd_showbypass_config_show,
		(void *)&cmd_showbypass_config_bypass,
		(void *)&cmd_showbypass_config_config,
		(void *)&cmd_showbypass_config_port,
		NULL,
	},
};
#endif

#ifdef RTE_LIBRTE_PMD_BOND
/* *** SET BONDING MODE *** */
struct cmd_set_bonding_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mode;
	uint8_t value;
	uint8_t port_id;
};

static void cmd_set_bonding_mode_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bonding_mode_result *res = parsed_result;
	portid_t port_id = res->port_id;

	/* Set the bonding mode for the relevant port. */
	if (0 != rte_eth_bond_mode_set(port_id, res->value))
		printf("\t Failed to set bonding mode for port = %d.\n", port_id);
}

cmdline_parse_token_string_t cmd_setbonding_mode_set =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		set, "set");
cmdline_parse_token_string_t cmd_setbonding_mode_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_setbonding_mode_mode =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_mode_result,
		mode, "mode");
cmdline_parse_token_num_t cmd_setbonding_mode_value =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_mode_result,
		value, UINT8);
cmdline_parse_token_num_t cmd_setbonding_mode_port =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_mode_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_set_bonding_mode = {
		.f = cmd_set_bonding_mode_parsed,
		.help_str = "set bonding mode (mode_value) (port_id): Set the bonding mode for port_id",
		.data = NULL,
		.tokens = {
				(void *) &cmd_setbonding_mode_set,
				(void *) &cmd_setbonding_mode_bonding,
				(void *) &cmd_setbonding_mode_mode,
				(void *) &cmd_setbonding_mode_value,
				(void *) &cmd_setbonding_mode_port,
				NULL
		}
};

/* *** SET BALANCE XMIT POLICY *** */
struct cmd_set_bonding_balance_xmit_policy_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t balance_xmit_policy;
	uint8_t port_id;
	cmdline_fixed_string_t policy;
};

static void cmd_set_bonding_balance_xmit_policy_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bonding_balance_xmit_policy_result *res = parsed_result;
	portid_t port_id = res->port_id;
	uint8_t policy;

	if (!strcmp(res->policy, "l2")) {
		policy = BALANCE_XMIT_POLICY_LAYER2;
	} else if (!strcmp(res->policy, "l23")) {
		policy = BALANCE_XMIT_POLICY_LAYER23;
	} else if (!strcmp(res->policy, "l34")) {
		policy = BALANCE_XMIT_POLICY_LAYER34;
	} else {
		printf("\t Invalid xmit policy selection");
		return;
	}

	/* Set the bonding mode for the relevant port. */
	if (0 != rte_eth_bond_xmit_policy_set(port_id, policy)) {
		printf("\t Failed to set bonding balance xmit policy for port = %d.\n",
				port_id);
	}
}

cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_set =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		set, "set");
cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_balance_xmit_policy =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		balance_xmit_policy, "balance_xmit_policy");
cmdline_parse_token_num_t cmd_setbonding_balance_xmit_policy_port =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		port_id, UINT8);
cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_policy =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		policy, "l2#l23#l34");

cmdline_parse_inst_t cmd_set_balance_xmit_policy = {
		.f = cmd_set_bonding_balance_xmit_policy_parsed,
		.help_str = "set bonding balance_xmit_policy (port_id) (policy_value): Set the bonding balance_xmit_policy for port_id",
		.data = NULL,
		.tokens = {
				(void *)&cmd_setbonding_balance_xmit_policy_set,
				(void *)&cmd_setbonding_balance_xmit_policy_bonding,
				(void *)&cmd_setbonding_balance_xmit_policy_balance_xmit_policy,
				(void *)&cmd_setbonding_balance_xmit_policy_port,
				(void *)&cmd_setbonding_balance_xmit_policy_policy,
				NULL
		}
};

/* *** SHOW NIC BONDING CONFIGURATION *** */
struct cmd_show_bonding_config_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t config;
	uint8_t port_id;
};

static void cmd_show_bonding_config_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_show_bonding_config_result *res = parsed_result;
	int bonding_mode;
	uint8_t slaves[RTE_MAX_ETHPORTS];
	int num_slaves, num_active_slaves;
	int primary_id;
	int i;
	portid_t port_id = res->port_id;

	/* Display the bonding mode.*/
	bonding_mode = rte_eth_bond_mode_get(port_id);
	if (bonding_mode < 0) {
		printf("\tFailed to get bonding mode for port = %d\n", port_id);
		return;
	} else
		printf("\tBonding mode: %d\n", bonding_mode);

	if (bonding_mode == BONDING_MODE_BALANCE) {
		int balance_xmit_policy;

		balance_xmit_policy = rte_eth_bond_xmit_policy_get(port_id);
		if (balance_xmit_policy < 0) {
			printf("\tFailed to get balance xmit policy for port = %d\n",
					port_id);
			return;
		} else {
			printf("\tBalance Xmit Policy: ");

			switch (balance_xmit_policy) {
			case BALANCE_XMIT_POLICY_LAYER2:
				printf("BALANCE_XMIT_POLICY_LAYER2");
				break;
			case BALANCE_XMIT_POLICY_LAYER23:
				printf("BALANCE_XMIT_POLICY_LAYER23");
				break;
			case BALANCE_XMIT_POLICY_LAYER34:
				printf("BALANCE_XMIT_POLICY_LAYER34");
				break;
			}
			printf("\n");
		}
	}

	num_slaves = rte_eth_bond_slaves_get(port_id, slaves, RTE_MAX_ETHPORTS);

	if (num_slaves < 0) {
		printf("\tFailed to get slave list for port = %d\n", port_id);
		return;
	}
	if (num_slaves > 0) {
		printf("\tSlaves (%d): [", num_slaves);
		for (i = 0; i < num_slaves - 1; i++)
			printf("%d ", slaves[i]);

		printf("%d]\n", slaves[num_slaves - 1]);
	} else {
		printf("\tSlaves: []\n");

	}

	num_active_slaves = rte_eth_bond_active_slaves_get(port_id, slaves,
			RTE_MAX_ETHPORTS);

	if (num_active_slaves < 0) {
		printf("\tFailed to get active slave list for port = %d\n", port_id);
		return;
	}
	if (num_active_slaves > 0) {
		printf("\tActive Slaves (%d): [", num_active_slaves);
		for (i = 0; i < num_active_slaves - 1; i++)
			printf("%d ", slaves[i]);

		printf("%d]\n", slaves[num_active_slaves - 1]);

	} else {
		printf("\tActive Slaves: []\n");

	}

	primary_id = rte_eth_bond_primary_get(port_id);
	if (primary_id < 0) {
		printf("\tFailed to get primary slave for port = %d\n", port_id);
		return;
	} else
		printf("\tPrimary: [%d]\n", primary_id);

}

cmdline_parse_token_string_t cmd_showbonding_config_show =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		show, "show");
cmdline_parse_token_string_t cmd_showbonding_config_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_showbonding_config_config =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_config_result,
		config, "config");
cmdline_parse_token_num_t cmd_showbonding_config_port =
TOKEN_NUM_INITIALIZER(struct cmd_show_bonding_config_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_show_bonding_config = {
		.f = cmd_show_bonding_config_parsed,
		.help_str =	"show bonding config (port_id): Show the bonding config for port_id",
		.data = NULL,
		.tokens = {
				(void *)&cmd_showbonding_config_show,
				(void *)&cmd_showbonding_config_bonding,
				(void *)&cmd_showbonding_config_config,
				(void *)&cmd_showbonding_config_port,
				NULL
		}
};

/* *** SET BONDING PRIMARY *** */
struct cmd_set_bonding_primary_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t primary;
	uint8_t slave_id;
	uint8_t port_id;
};

static void cmd_set_bonding_primary_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bonding_primary_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* Set the primary slave for a bonded device. */
	if (0 != rte_eth_bond_primary_set(master_port_id, slave_port_id)) {
		printf("\t Failed to set primary slave for port = %d.\n",
				master_port_id);
		return;
	}
	init_port_config();
}

cmdline_parse_token_string_t cmd_setbonding_primary_set =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		set, "set");
cmdline_parse_token_string_t cmd_setbonding_primary_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_setbonding_primary_primary =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_primary_result,
		primary, "primary");
cmdline_parse_token_num_t cmd_setbonding_primary_slave =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_primary_result,
		slave_id, UINT8);
cmdline_parse_token_num_t cmd_setbonding_primary_port =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_primary_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_set_bonding_primary = {
		.f = cmd_set_bonding_primary_parsed,
		.help_str = "set bonding primary (slave_id) (port_id): Set the primary slave for port_id",
		.data = NULL,
		.tokens = {
				(void *)&cmd_setbonding_primary_set,
				(void *)&cmd_setbonding_primary_bonding,
				(void *)&cmd_setbonding_primary_primary,
				(void *)&cmd_setbonding_primary_slave,
				(void *)&cmd_setbonding_primary_port,
				NULL
		}
};

/* *** ADD SLAVE *** */
struct cmd_add_bonding_slave_result {
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t slave;
	uint8_t slave_id;
	uint8_t port_id;
};

static void cmd_add_bonding_slave_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_add_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* Set the primary slave for a bonded device. */
	if (0 != rte_eth_bond_slave_add(master_port_id, slave_port_id)) {
		printf("\t Failed to add slave %d to master port = %d.\n",
				slave_port_id, master_port_id);
		return;
	}
	init_port_config();
	set_port_slave_flag(slave_port_id);
}

cmdline_parse_token_string_t cmd_addbonding_slave_add =
TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		add, "add");
cmdline_parse_token_string_t cmd_addbonding_slave_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_addbonding_slave_slave =
TOKEN_STRING_INITIALIZER(struct cmd_add_bonding_slave_result,
		slave, "slave");
cmdline_parse_token_num_t cmd_addbonding_slave_slaveid =
TOKEN_NUM_INITIALIZER(struct cmd_add_bonding_slave_result,
		slave_id, UINT8);
cmdline_parse_token_num_t cmd_addbonding_slave_port =
TOKEN_NUM_INITIALIZER(struct cmd_add_bonding_slave_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_add_bonding_slave = {
		.f = cmd_add_bonding_slave_parsed,
		.help_str = "add bonding slave (slave_id) (port_id): Add a slave device to a bonded device",
		.data = NULL,
		.tokens = {
				(void *)&cmd_addbonding_slave_add,
				(void *)&cmd_addbonding_slave_bonding,
				(void *)&cmd_addbonding_slave_slave,
				(void *)&cmd_addbonding_slave_slaveid,
				(void *)&cmd_addbonding_slave_port,
				NULL
		}
};

/* *** REMOVE SLAVE *** */
struct cmd_remove_bonding_slave_result {
	cmdline_fixed_string_t remove;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t slave;
	uint8_t slave_id;
	uint8_t port_id;
};

static void cmd_remove_bonding_slave_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_remove_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* Set the primary slave for a bonded device. */
	if (0 != rte_eth_bond_slave_remove(master_port_id, slave_port_id)) {
		printf("\t Failed to remove slave %d from master port = %d.\n",
				slave_port_id, master_port_id);
		return;
	}
	init_port_config();
	clear_port_slave_flag(slave_port_id);
}

cmdline_parse_token_string_t cmd_removebonding_slave_remove =
		TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
				remove, "remove");
cmdline_parse_token_string_t cmd_removebonding_slave_bonding =
		TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
				bonding, "bonding");
cmdline_parse_token_string_t cmd_removebonding_slave_slave =
		TOKEN_STRING_INITIALIZER(struct cmd_remove_bonding_slave_result,
				slave, "slave");
cmdline_parse_token_num_t cmd_removebonding_slave_slaveid =
		TOKEN_NUM_INITIALIZER(struct cmd_remove_bonding_slave_result,
				slave_id, UINT8);
cmdline_parse_token_num_t cmd_removebonding_slave_port =
		TOKEN_NUM_INITIALIZER(struct cmd_remove_bonding_slave_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_remove_bonding_slave = {
		.f = cmd_remove_bonding_slave_parsed,
		.help_str = "remove bonding slave (slave_id) (port_id): Remove a slave device from a bonded device",
		.data = NULL,
		.tokens = {
				(void *)&cmd_removebonding_slave_remove,
				(void *)&cmd_removebonding_slave_bonding,
				(void *)&cmd_removebonding_slave_slave,
				(void *)&cmd_removebonding_slave_slaveid,
				(void *)&cmd_removebonding_slave_port,
				NULL
		}
};

/* *** CREATE BONDED DEVICE *** */
struct cmd_create_bonded_device_result {
	cmdline_fixed_string_t create;
	cmdline_fixed_string_t bonded;
	cmdline_fixed_string_t device;
	uint8_t mode;
	uint8_t socket;
};

static int bond_dev_num = 0;

static void cmd_create_bonded_device_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_create_bonded_device_result *res = parsed_result;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];
	int port_id;

	if (test_done == 0) {
		printf("Please stop forwarding first\n");
		return;
	}

	snprintf(ethdev_name, RTE_ETH_NAME_MAX_LEN, "net_bond_testpmd_%d",
			bond_dev_num++);

	/* Create a new bonded device. */
	port_id = rte_eth_bond_create(ethdev_name, res->mode, res->socket);
	if (port_id < 0) {
		printf("\t Failed to create bonded device.\n");
		return;
	} else {
		printf("Created new bonded device %s on (port %d).\n", ethdev_name,
				port_id);

		/* Update number of ports */
		nb_ports = rte_eth_dev_count();
		reconfig(port_id, res->socket);
		rte_eth_promiscuous_enable(port_id);
		ports[port_id].enabled = 1;
	}

}

cmdline_parse_token_string_t cmd_createbonded_device_create =
		TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
				create, "create");
cmdline_parse_token_string_t cmd_createbonded_device_bonded =
		TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
				bonded, "bonded");
cmdline_parse_token_string_t cmd_createbonded_device_device =
		TOKEN_STRING_INITIALIZER(struct cmd_create_bonded_device_result,
				device, "device");
cmdline_parse_token_num_t cmd_createbonded_device_mode =
		TOKEN_NUM_INITIALIZER(struct cmd_create_bonded_device_result,
				mode, UINT8);
cmdline_parse_token_num_t cmd_createbonded_device_socket =
		TOKEN_NUM_INITIALIZER(struct cmd_create_bonded_device_result,
				socket, UINT8);

cmdline_parse_inst_t cmd_create_bonded_device = {
		.f = cmd_create_bonded_device_parsed,
		.help_str = "create bonded device (mode) (socket): Create a new bonded device with specific bonding mode and socket",
		.data = NULL,
		.tokens = {
				(void *)&cmd_createbonded_device_create,
				(void *)&cmd_createbonded_device_bonded,
				(void *)&cmd_createbonded_device_device,
				(void *)&cmd_createbonded_device_mode,
				(void *)&cmd_createbonded_device_socket,
				NULL
		}
};

/* *** SET MAC ADDRESS IN BONDED DEVICE *** */
struct cmd_set_bond_mac_addr_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mac_addr;
	uint8_t port_num;
	struct ether_addr address;
};

static void cmd_set_bond_mac_addr_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bond_mac_addr_result *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_num, ENABLED_WARN))
		return;

	ret = rte_eth_bond_mac_address_set(res->port_num, &res->address);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		printf("set_bond_mac_addr error: (%s)\n", strerror(-ret));
}

cmdline_parse_token_string_t cmd_set_bond_mac_addr_set =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result, set, "set");
cmdline_parse_token_string_t cmd_set_bond_mac_addr_bonding =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result, bonding,
				"bonding");
cmdline_parse_token_string_t cmd_set_bond_mac_addr_mac =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mac_addr_result, mac_addr,
				"mac_addr");
cmdline_parse_token_num_t cmd_set_bond_mac_addr_portnum =
		TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mac_addr_result, port_num, UINT8);
cmdline_parse_token_etheraddr_t cmd_set_bond_mac_addr_addr =
		TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_bond_mac_addr_result, address);

cmdline_parse_inst_t cmd_set_bond_mac_addr = {
		.f = cmd_set_bond_mac_addr_parsed,
		.data = (void *) 0,
		.help_str = "set bonding mac_addr (port_id) (address): ",
		.tokens = {
				(void *)&cmd_set_bond_mac_addr_set,
				(void *)&cmd_set_bond_mac_addr_bonding,
				(void *)&cmd_set_bond_mac_addr_mac,
				(void *)&cmd_set_bond_mac_addr_portnum,
				(void *)&cmd_set_bond_mac_addr_addr,
				NULL
		}
};


/* *** SET LINK STATUS MONITORING POLLING PERIOD ON BONDED DEVICE *** */
struct cmd_set_bond_mon_period_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mon_period;
	uint8_t port_num;
	uint32_t period_ms;
};

static void cmd_set_bond_mon_period_parsed(void *parsed_result,
		__attribute__((unused))  struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_set_bond_mon_period_result *res = parsed_result;
	int ret;

	if (res->port_num >= nb_ports) {
		printf("Port id %d must be less than %d\n", res->port_num, nb_ports);
		return;
	}

	ret = rte_eth_bond_link_monitoring_set(res->port_num, res->period_ms);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		printf("set_bond_mac_addr error: (%s)\n", strerror(-ret));
}

cmdline_parse_token_string_t cmd_set_bond_mon_period_set =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
				set, "set");
cmdline_parse_token_string_t cmd_set_bond_mon_period_bonding =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
				bonding, "bonding");
cmdline_parse_token_string_t cmd_set_bond_mon_period_mon_period =
		TOKEN_STRING_INITIALIZER(struct cmd_set_bond_mon_period_result,
				mon_period,	"mon_period");
cmdline_parse_token_num_t cmd_set_bond_mon_period_portnum =
		TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mon_period_result,
				port_num, UINT8);
cmdline_parse_token_num_t cmd_set_bond_mon_period_period_ms =
		TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mon_period_result,
				period_ms, UINT32);

cmdline_parse_inst_t cmd_set_bond_mon_period = {
		.f = cmd_set_bond_mon_period_parsed,
		.data = (void *) 0,
		.help_str = "set bonding mon_period (port_id) (period_ms): ",
		.tokens = {
				(void *)&cmd_set_bond_mon_period_set,
				(void *)&cmd_set_bond_mon_period_bonding,
				(void *)&cmd_set_bond_mon_period_mon_period,
				(void *)&cmd_set_bond_mon_period_portnum,
				(void *)&cmd_set_bond_mon_period_period_ms,
				NULL
		}
};

#endif /* RTE_LIBRTE_PMD_BOND */

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

	retry_enabled = 0;
	set_pkt_forwarding_mode(res->mode);
}

cmdline_parse_token_string_t cmd_setfwd_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, set, "set");
cmdline_parse_token_string_t cmd_setfwd_fwd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, fwd, "fwd");
cmdline_parse_token_string_t cmd_setfwd_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_mode_result, mode,
		"" /* defined at init */);

cmdline_parse_inst_t cmd_set_fwd_mode = {
	.f = cmd_set_fwd_mode_parsed,
	.data = NULL,
	.help_str = NULL, /* defined at init */
	.tokens = {
		(void *)&cmd_setfwd_set,
		(void *)&cmd_setfwd_fwd,
		(void *)&cmd_setfwd_mode,
		NULL,
	},
};

static void cmd_set_fwd_mode_init(void)
{
	char *modes, *c;
	static char token[128];
	static char help[256];
	cmdline_parse_token_string_t *token_struct;

	modes = list_pkt_forwarding_modes();
	snprintf(help, sizeof help, "set fwd %s - "
		"set packet forwarding mode", modes);
	cmd_set_fwd_mode.help_str = help;

	/* string token separator is # */
	for (c = token; *modes != '\0'; modes++)
		if (*modes == '|')
			*c++ = '#';
		else
			*c++ = *modes;
	token_struct = (cmdline_parse_token_string_t*)cmd_set_fwd_mode.tokens[2];
	token_struct->string_data.str = token;
}

/* *** SET RETRY FORWARDING MODE *** */
struct cmd_set_fwd_retry_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t fwd;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t retry;
};

static void cmd_set_fwd_retry_mode_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_set_fwd_retry_mode_result *res = parsed_result;

	retry_enabled = 1;
	set_pkt_forwarding_mode(res->mode);
}

cmdline_parse_token_string_t cmd_setfwd_retry_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_retry_mode_result,
			set, "set");
cmdline_parse_token_string_t cmd_setfwd_retry_fwd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_retry_mode_result,
			fwd, "fwd");
cmdline_parse_token_string_t cmd_setfwd_retry_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_retry_mode_result,
			mode,
		"" /* defined at init */);
cmdline_parse_token_string_t cmd_setfwd_retry_retry =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fwd_retry_mode_result,
			retry, "retry");

cmdline_parse_inst_t cmd_set_fwd_retry_mode = {
	.f = cmd_set_fwd_retry_mode_parsed,
	.data = NULL,
	.help_str = NULL, /* defined at init */
	.tokens = {
		(void *)&cmd_setfwd_retry_set,
		(void *)&cmd_setfwd_retry_fwd,
		(void *)&cmd_setfwd_retry_mode,
		(void *)&cmd_setfwd_retry_retry,
		NULL,
	},
};

static void cmd_set_fwd_retry_mode_init(void)
{
	char *modes, *c;
	static char token[128];
	static char help[256];
	cmdline_parse_token_string_t *token_struct;

	modes = list_pkt_forwarding_retry_modes();
	snprintf(help, sizeof(help), "set fwd %s retry - "
		"set packet forwarding mode with retry", modes);
	cmd_set_fwd_retry_mode.help_str = help;

	/* string token separator is # */
	for (c = token; *modes != '\0'; modes++)
		if (*modes == '|')
			*c++ = '#';
		else
			*c++ = *modes;
	token_struct = (cmdline_parse_token_string_t *)
		cmd_set_fwd_retry_mode.tokens[2];
	token_struct->string_data.str = token;
}

/* *** SET BURST TX DELAY TIME RETRY NUMBER *** */
struct cmd_set_burst_tx_retry_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t burst;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t delay;
	uint32_t time;
	cmdline_fixed_string_t retry;
	uint32_t retry_num;
};

static void cmd_set_burst_tx_retry_parsed(void *parsed_result,
					__attribute__((unused)) struct cmdline *cl,
					__attribute__((unused)) void *data)
{
	struct cmd_set_burst_tx_retry_result *res = parsed_result;

	if (!strcmp(res->set, "set") && !strcmp(res->burst, "burst")
		&& !strcmp(res->tx, "tx")) {
		if (!strcmp(res->delay, "delay"))
			burst_tx_delay_time = res->time;
		if (!strcmp(res->retry, "retry"))
			burst_tx_retry_num = res->retry_num;
	}

}

cmdline_parse_token_string_t cmd_set_burst_tx_retry_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, set, "set");
cmdline_parse_token_string_t cmd_set_burst_tx_retry_burst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, burst,
				 "burst");
cmdline_parse_token_string_t cmd_set_burst_tx_retry_tx =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, tx, "tx");
cmdline_parse_token_string_t cmd_set_burst_tx_retry_delay =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, delay, "delay");
cmdline_parse_token_num_t cmd_set_burst_tx_retry_time =
	TOKEN_NUM_INITIALIZER(struct cmd_set_burst_tx_retry_result, time, UINT32);
cmdline_parse_token_string_t cmd_set_burst_tx_retry_retry =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, retry, "retry");
cmdline_parse_token_num_t cmd_set_burst_tx_retry_retry_num =
	TOKEN_NUM_INITIALIZER(struct cmd_set_burst_tx_retry_result, retry_num, UINT32);

cmdline_parse_inst_t cmd_set_burst_tx_retry = {
	.f = cmd_set_burst_tx_retry_parsed,
	.help_str = "set burst tx delay (time_by_useconds) retry (retry_num)",
	.tokens = {
		(void *)&cmd_set_burst_tx_retry_set,
		(void *)&cmd_set_burst_tx_retry_burst,
		(void *)&cmd_set_burst_tx_retry_tx,
		(void *)&cmd_set_burst_tx_retry_delay,
		(void *)&cmd_set_burst_tx_retry_time,
		(void *)&cmd_set_burst_tx_retry_retry,
		(void *)&cmd_set_burst_tx_retry_retry_num,
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
		FOREACH_PORT(i, ports) {
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
		FOREACH_PORT(i, ports) {
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

/* *** SETUP ETHERNET LINK FLOW CONTROL *** */
struct cmd_link_flow_ctrl_set_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t flow_ctrl;
	cmdline_fixed_string_t rx;
	cmdline_fixed_string_t rx_lfc_mode;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t tx_lfc_mode;
	cmdline_fixed_string_t mac_ctrl_frame_fwd;
	cmdline_fixed_string_t mac_ctrl_frame_fwd_mode;
	cmdline_fixed_string_t autoneg_str;
	cmdline_fixed_string_t autoneg;
	cmdline_fixed_string_t hw_str;
	uint32_t high_water;
	cmdline_fixed_string_t lw_str;
	uint32_t low_water;
	cmdline_fixed_string_t pt_str;
	uint16_t pause_time;
	cmdline_fixed_string_t xon_str;
	uint16_t send_xon;
	uint8_t  port_id;
};

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
cmdline_parse_token_string_t cmd_lfc_set_high_water_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				hw_str, "high_water");
cmdline_parse_token_num_t cmd_lfc_set_high_water =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				high_water, UINT32);
cmdline_parse_token_string_t cmd_lfc_set_low_water_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				lw_str, "low_water");
cmdline_parse_token_num_t cmd_lfc_set_low_water =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				low_water, UINT32);
cmdline_parse_token_string_t cmd_lfc_set_pause_time_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				pt_str, "pause_time");
cmdline_parse_token_num_t cmd_lfc_set_pause_time =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				pause_time, UINT16);
cmdline_parse_token_string_t cmd_lfc_set_send_xon_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				xon_str, "send_xon");
cmdline_parse_token_num_t cmd_lfc_set_send_xon =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				send_xon, UINT16);
cmdline_parse_token_string_t cmd_lfc_set_mac_ctrl_frame_fwd_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				mac_ctrl_frame_fwd, "mac_ctrl_frame_fwd");
cmdline_parse_token_string_t cmd_lfc_set_mac_ctrl_frame_fwd =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				mac_ctrl_frame_fwd_mode, "on#off");
cmdline_parse_token_string_t cmd_lfc_set_autoneg_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				autoneg_str, "autoneg");
cmdline_parse_token_string_t cmd_lfc_set_autoneg =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				autoneg, "on#off");
cmdline_parse_token_num_t cmd_lfc_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				port_id, UINT8);

/* forward declaration */
static void
cmd_link_flow_ctrl_set_parsed(void *parsed_result, struct cmdline *cl,
			      void *data);

cmdline_parse_inst_t cmd_link_flow_control_set = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = NULL,
	.help_str = "Configure the Ethernet flow control: set flow_ctrl rx on|off \
tx on|off high_water low_water pause_time send_xon mac_ctrl_frame_fwd on|off \
autoneg on|off port_id",
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
		(void *)&cmd_lfc_set_mac_ctrl_frame_fwd_mode,
		(void *)&cmd_lfc_set_mac_ctrl_frame_fwd,
		(void *)&cmd_lfc_set_autoneg_str,
		(void *)&cmd_lfc_set_autoneg,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_rx = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_rx,
	.help_str = "Change rx flow control parameter: set flow_ctrl "
		    "rx on|off port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_rx,
		(void *)&cmd_lfc_set_rx_mode,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_tx = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_tx,
	.help_str = "Change tx flow control parameter: set flow_ctrl "
		    "tx on|off port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_tx,
		(void *)&cmd_lfc_set_tx_mode,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_hw = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_hw,
	.help_str = "Change high water flow control parameter: set flow_ctrl "
		    "high_water value port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_high_water_str,
		(void *)&cmd_lfc_set_high_water,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_lw = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_lw,
	.help_str = "Change low water flow control parameter: set flow_ctrl "
		    "low_water value port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_low_water_str,
		(void *)&cmd_lfc_set_low_water,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_pt = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_pt,
	.help_str = "Change pause time flow control parameter: set flow_ctrl "
		    "pause_time value port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_pause_time_str,
		(void *)&cmd_lfc_set_pause_time,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_xon = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_xon,
	.help_str = "Change send_xon flow control parameter: set flow_ctrl "
		    "send_xon value port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_send_xon_str,
		(void *)&cmd_lfc_set_send_xon,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_macfwd = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_macfwd,
	.help_str = "Change mac ctrl fwd flow control parameter: set flow_ctrl "
		    "mac_ctrl_frame_fwd on|off port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_mac_ctrl_frame_fwd_mode,
		(void *)&cmd_lfc_set_mac_ctrl_frame_fwd,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

cmdline_parse_inst_t cmd_link_flow_control_set_autoneg = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = (void *)&cmd_link_flow_control_set_autoneg,
	.help_str = "Change autoneg flow control parameter: set flow_ctrl "
		    "autoneg on|off port_id",
	.tokens = {
		(void *)&cmd_lfc_set_set,
		(void *)&cmd_lfc_set_flow_ctrl,
		(void *)&cmd_lfc_set_autoneg_str,
		(void *)&cmd_lfc_set_autoneg,
		(void *)&cmd_lfc_set_portid,
		NULL,
	},
};

static void
cmd_link_flow_ctrl_set_parsed(void *parsed_result,
			      __attribute__((unused)) struct cmdline *cl,
			      void *data)
{
	struct cmd_link_flow_ctrl_set_result *res = parsed_result;
	cmdline_parse_inst_t *cmd = data;
	struct rte_eth_fc_conf fc_conf;
	int rx_fc_en = 0;
	int tx_fc_en = 0;
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

	/* Partial command line, retrieve current configuration */
	if (cmd) {
		ret = rte_eth_dev_flow_ctrl_get(res->port_id, &fc_conf);
		if (ret != 0) {
			printf("cannot get current flow ctrl parameters, return"
			       "code = %d\n", ret);
			return;
		}

		if ((fc_conf.mode == RTE_FC_RX_PAUSE) ||
		    (fc_conf.mode == RTE_FC_FULL))
			rx_fc_en = 1;
		if ((fc_conf.mode == RTE_FC_TX_PAUSE) ||
		    (fc_conf.mode == RTE_FC_FULL))
			tx_fc_en = 1;
	}

	if (!cmd || cmd == &cmd_link_flow_control_set_rx)
		rx_fc_en = (!strcmp(res->rx_lfc_mode, "on")) ? 1 : 0;

	if (!cmd || cmd == &cmd_link_flow_control_set_tx)
		tx_fc_en = (!strcmp(res->tx_lfc_mode, "on")) ? 1 : 0;

	fc_conf.mode = rx_tx_onoff_2_lfc_mode[rx_fc_en][tx_fc_en];

	if (!cmd || cmd == &cmd_link_flow_control_set_hw)
		fc_conf.high_water = res->high_water;

	if (!cmd || cmd == &cmd_link_flow_control_set_lw)
		fc_conf.low_water = res->low_water;

	if (!cmd || cmd == &cmd_link_flow_control_set_pt)
		fc_conf.pause_time = res->pause_time;

	if (!cmd || cmd == &cmd_link_flow_control_set_xon)
		fc_conf.send_xon = res->send_xon;

	if (!cmd || cmd == &cmd_link_flow_control_set_macfwd) {
		if (!strcmp(res->mac_ctrl_frame_fwd_mode, "on"))
			fc_conf.mac_ctrl_frame_fwd = 1;
		else
			fc_conf.mac_ctrl_frame_fwd = 0;
	}

	if (!cmd || cmd == &cmd_link_flow_control_set_autoneg)
		fc_conf.autoneg = (!strcmp(res->autoneg, "on")) ? 1 : 0;

	ret = rte_eth_dev_flow_ctrl_set(res->port_id, &fc_conf);
	if (ret != 0)
		printf("bad flow contrl parameter, return code = %d \n", ret);
}

/* *** SETUP ETHERNET PIRORITY FLOW CONTROL *** */
struct cmd_priority_flow_ctrl_set_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t pfc_ctrl;
	cmdline_fixed_string_t rx;
	cmdline_fixed_string_t rx_pfc_mode;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t tx_pfc_mode;
	uint32_t high_water;
	uint32_t low_water;
	uint16_t pause_time;
	uint8_t  priority;
	uint8_t  port_id;
};

static void
cmd_priority_flow_ctrl_set_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_priority_flow_ctrl_set_result *res = parsed_result;
	struct rte_eth_pfc_conf pfc_conf;
	int rx_fc_enable, tx_fc_enable;
	int ret;

	/*
	 * Rx on/off, flow control is enabled/disabled on RX side. This can indicate
	 * the RTE_FC_TX_PAUSE, Transmit pause frame at the Rx side.
	 * Tx on/off, flow control is enabled/disabled on TX side. This can indicate
	 * the RTE_FC_RX_PAUSE, Respond to the pause frame at the Tx side.
	 */
	static enum rte_eth_fc_mode rx_tx_onoff_2_pfc_mode[2][2] = {
			{RTE_FC_NONE, RTE_FC_RX_PAUSE}, {RTE_FC_TX_PAUSE, RTE_FC_FULL}
	};

	rx_fc_enable = (!strncmp(res->rx_pfc_mode, "on",2)) ? 1 : 0;
	tx_fc_enable = (!strncmp(res->tx_pfc_mode, "on",2)) ? 1 : 0;
	pfc_conf.fc.mode       = rx_tx_onoff_2_pfc_mode[rx_fc_enable][tx_fc_enable];
	pfc_conf.fc.high_water = res->high_water;
	pfc_conf.fc.low_water  = res->low_water;
	pfc_conf.fc.pause_time = res->pause_time;
	pfc_conf.priority      = res->priority;

	ret = rte_eth_dev_priority_flow_ctrl_set(res->port_id, &pfc_conf);
	if (ret != 0)
		printf("bad priority flow contrl parameter, return code = %d \n", ret);
}

cmdline_parse_token_string_t cmd_pfc_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				set, "set");
cmdline_parse_token_string_t cmd_pfc_set_flow_ctrl =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				pfc_ctrl, "pfc_ctrl");
cmdline_parse_token_string_t cmd_pfc_set_rx =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				rx, "rx");
cmdline_parse_token_string_t cmd_pfc_set_rx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				rx_pfc_mode, "on#off");
cmdline_parse_token_string_t cmd_pfc_set_tx =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				tx, "tx");
cmdline_parse_token_string_t cmd_pfc_set_tx_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				tx_pfc_mode, "on#off");
cmdline_parse_token_num_t cmd_pfc_set_high_water =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				high_water, UINT32);
cmdline_parse_token_num_t cmd_pfc_set_low_water =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				low_water, UINT32);
cmdline_parse_token_num_t cmd_pfc_set_pause_time =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				pause_time, UINT16);
cmdline_parse_token_num_t cmd_pfc_set_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				priority, UINT8);
cmdline_parse_token_num_t cmd_pfc_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				port_id, UINT8);

cmdline_parse_inst_t cmd_priority_flow_control_set = {
	.f = cmd_priority_flow_ctrl_set_parsed,
	.data = NULL,
	.help_str = "Configure the Ethernet priority flow control: set pfc_ctrl rx on|off\n\
			tx on|off high_water low_water pause_time priority port_id",
	.tokens = {
		(void *)&cmd_pfc_set_set,
		(void *)&cmd_pfc_set_flow_ctrl,
		(void *)&cmd_pfc_set_rx,
		(void *)&cmd_pfc_set_rx_mode,
		(void *)&cmd_pfc_set_tx,
		(void *)&cmd_pfc_set_tx_mode,
		(void *)&cmd_pfc_set_high_water,
		(void *)&cmd_pfc_set_low_water,
		(void *)&cmd_pfc_set_pause_time,
		(void *)&cmd_pfc_set_priority,
		(void *)&cmd_pfc_set_portid,
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

/* *** START FORWARDING WITH N TX BURST FIRST *** */
struct cmd_start_tx_first_n_result {
	cmdline_fixed_string_t start;
	cmdline_fixed_string_t tx_first;
	uint32_t tx_num;
};

static void
cmd_start_tx_first_n_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_start_tx_first_n_result *res = parsed_result;

	start_packet_forwarding(res->tx_num);
}

cmdline_parse_token_string_t cmd_start_tx_first_n_start =
	TOKEN_STRING_INITIALIZER(struct cmd_start_tx_first_n_result,
			start, "start");
cmdline_parse_token_string_t cmd_start_tx_first_n_tx_first =
	TOKEN_STRING_INITIALIZER(struct cmd_start_tx_first_n_result,
			tx_first, "tx_first");
cmdline_parse_token_num_t cmd_start_tx_first_n_tx_num =
	TOKEN_NUM_INITIALIZER(struct cmd_start_tx_first_n_result,
			tx_num, UINT32);

cmdline_parse_inst_t cmd_start_tx_first_n = {
	.f = cmd_start_tx_first_n_parsed,
	.data = NULL,
	.help_str = "start packet forwarding, after sending <num> "
		"bursts of packets",
	.tokens = {
		(void *)&cmd_start_tx_first_n_start,
		(void *)&cmd_start_tx_first_n_tx_first,
		(void *)&cmd_start_tx_first_n_tx_num,
		NULL,
	},
};

/* *** SET LINK UP *** */
struct cmd_set_link_up_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t link_up;
	cmdline_fixed_string_t port;
	uint8_t port_id;
};

cmdline_parse_token_string_t cmd_set_link_up_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, set, "set");
cmdline_parse_token_string_t cmd_set_link_up_link_up =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, link_up,
				"link-up");
cmdline_parse_token_string_t cmd_set_link_up_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, port, "port");
cmdline_parse_token_num_t cmd_set_link_up_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_link_up_result, port_id, UINT8);

static void cmd_set_link_up_parsed(__attribute__((unused)) void *parsed_result,
			     __attribute__((unused)) struct cmdline *cl,
			     __attribute__((unused)) void *data)
{
	struct cmd_set_link_up_result *res = parsed_result;
	dev_set_link_up(res->port_id);
}

cmdline_parse_inst_t cmd_set_link_up = {
	.f = cmd_set_link_up_parsed,
	.data = NULL,
	.help_str = "set link-up port (port id)",
	.tokens = {
		(void *)&cmd_set_link_up_set,
		(void *)&cmd_set_link_up_link_up,
		(void *)&cmd_set_link_up_port,
		(void *)&cmd_set_link_up_port_id,
		NULL,
	},
};

/* *** SET LINK DOWN *** */
struct cmd_set_link_down_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t link_down;
	cmdline_fixed_string_t port;
	uint8_t port_id;
};

cmdline_parse_token_string_t cmd_set_link_down_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, set, "set");
cmdline_parse_token_string_t cmd_set_link_down_link_down =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, link_down,
				"link-down");
cmdline_parse_token_string_t cmd_set_link_down_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, port, "port");
cmdline_parse_token_num_t cmd_set_link_down_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_link_down_result, port_id, UINT8);

static void cmd_set_link_down_parsed(
				__attribute__((unused)) void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_set_link_down_result *res = parsed_result;
	dev_set_link_down(res->port_id);
}

cmdline_parse_inst_t cmd_set_link_down = {
	.f = cmd_set_link_down_parsed,
	.data = NULL,
	.help_str = "set link-down port (port id)",
	.tokens = {
		(void *)&cmd_set_link_down_set,
		(void *)&cmd_set_link_down_link_down,
		(void *)&cmd_set_link_down_port,
		(void *)&cmd_set_link_down_port_id,
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
		pkt_fwd_config_display(&cur_fwd_config);
	else if (!strcmp(res->what, "txpkts"))
		show_tx_pkt_segments();
}

cmdline_parse_token_string_t cmd_showcfg_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, show, "show");
cmdline_parse_token_string_t cmd_showcfg_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, cfg, "config");
cmdline_parse_token_string_t cmd_showcfg_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, what,
				 "rxtx#cores#fwd#txpkts");

cmdline_parse_inst_t cmd_showcfg = {
	.f = cmd_showcfg_parsed,
	.data = NULL,
	.help_str = "show config rxtx|cores|fwd|txpkts",
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
			FOREACH_PORT(i, ports)
				nic_stats_clear(i);
		else if (!strcmp(res->what, "xstats"))
			FOREACH_PORT(i, ports)
				nic_xstats_clear(i);
	} else if (!strcmp(res->what, "info"))
		FOREACH_PORT(i, ports)
			port_infos_display(i);
	else if (!strcmp(res->what, "stats"))
		FOREACH_PORT(i, ports)
			nic_stats_display(i);
	else if (!strcmp(res->what, "xstats"))
		FOREACH_PORT(i, ports)
			nic_xstats_display(i);
	else if (!strcmp(res->what, "fdir"))
		FOREACH_PORT(i, ports)
			fdir_get_infos(i);
	else if (!strcmp(res->what, "stat_qmap"))
		FOREACH_PORT(i, ports)
			nic_stats_mapping_display(i);
	else if (!strcmp(res->what, "dcb_tc"))
		FOREACH_PORT(i, ports)
			port_dcb_info_display(i);
}

cmdline_parse_token_string_t cmd_showportall_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, show,
				 "show#clear");
cmdline_parse_token_string_t cmd_showportall_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, port, "port");
cmdline_parse_token_string_t cmd_showportall_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, what,
				 "info#stats#xstats#fdir#stat_qmap#dcb_tc");
cmdline_parse_token_string_t cmd_showportall_all =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, all, "all");
cmdline_parse_inst_t cmd_showportall = {
	.f = cmd_showportall_parsed,
	.data = NULL,
	.help_str = "show|clear port info|stats|xstats|fdir|stat_qmap|dcb_tc all",
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
		else if (!strcmp(res->what, "xstats"))
			nic_xstats_clear(res->portnum);
	} else if (!strcmp(res->what, "info"))
		port_infos_display(res->portnum);
	else if (!strcmp(res->what, "stats"))
		nic_stats_display(res->portnum);
	else if (!strcmp(res->what, "xstats"))
		nic_xstats_display(res->portnum);
	else if (!strcmp(res->what, "fdir"))
		 fdir_get_infos(res->portnum);
	else if (!strcmp(res->what, "stat_qmap"))
		nic_stats_mapping_display(res->portnum);
	else if (!strcmp(res->what, "dcb_tc"))
		port_dcb_info_display(res->portnum);
}

cmdline_parse_token_string_t cmd_showport_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, show,
				 "show#clear");
cmdline_parse_token_string_t cmd_showport_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, port, "port");
cmdline_parse_token_string_t cmd_showport_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_result, what,
				 "info#stats#xstats#fdir#stat_qmap#dcb_tc");
cmdline_parse_token_num_t cmd_showport_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_result, portnum, UINT8);

cmdline_parse_inst_t cmd_showport = {
	.f = cmd_showport_parsed,
	.data = NULL,
	.help_str = "show|clear port info|stats|xstats|fdir|stat_qmap|dcb_tc X (X = port number)",
	.tokens = {
		(void *)&cmd_showport_show,
		(void *)&cmd_showport_port,
		(void *)&cmd_showport_what,
		(void *)&cmd_showport_portnum,
		NULL,
	},
};

/* *** SHOW QUEUE INFO *** */
struct cmd_showqueue_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t type;
	cmdline_fixed_string_t what;
	uint8_t portnum;
	uint16_t queuenum;
};

static void
cmd_showqueue_parsed(void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_showqueue_result *res = parsed_result;

	if (!strcmp(res->type, "rxq"))
		rx_queue_infos_display(res->portnum, res->queuenum);
	else if (!strcmp(res->type, "txq"))
		tx_queue_infos_display(res->portnum, res->queuenum);
}

cmdline_parse_token_string_t cmd_showqueue_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showqueue_result, show, "show");
cmdline_parse_token_string_t cmd_showqueue_type =
	TOKEN_STRING_INITIALIZER(struct cmd_showqueue_result, type, "rxq#txq");
cmdline_parse_token_string_t cmd_showqueue_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showqueue_result, what, "info");
cmdline_parse_token_num_t cmd_showqueue_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_showqueue_result, portnum, UINT8);
cmdline_parse_token_num_t cmd_showqueue_queuenum =
	TOKEN_NUM_INITIALIZER(struct cmd_showqueue_result, queuenum, UINT16);

cmdline_parse_inst_t cmd_showqueue = {
	.f = cmd_showqueue_parsed,
	.data = NULL,
	.help_str = "show rxq|txq info <port number> <queue_number>",
	.tokens = {
		(void *)&cmd_showqueue_show,
		(void *)&cmd_showqueue_type,
		(void *)&cmd_showqueue_what,
		(void *)&cmd_showqueue_portnum,
		(void *)&cmd_showqueue_queuenum,
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
cmdline_parse_token_etheraddr_t cmd_mac_addr_addr =
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


/* *** CONFIGURE QUEUE STATS COUNTER MAPPINGS *** */
struct cmd_set_qmap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t qmap;
	cmdline_fixed_string_t what;
	uint8_t port_id;
	uint16_t queue_id;
	uint8_t map_value;
};

static void
cmd_set_qmap_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_set_qmap_result *res = parsed_result;
	int is_rx = (strcmp(res->what, "tx") == 0) ? 0 : 1;

	set_qmap(res->port_id, (uint8_t)is_rx, res->queue_id, res->map_value);
}

cmdline_parse_token_string_t cmd_setqmap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_qmap_result,
				 set, "set");
cmdline_parse_token_string_t cmd_setqmap_qmap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_qmap_result,
				 qmap, "stat_qmap");
cmdline_parse_token_string_t cmd_setqmap_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_qmap_result,
				 what, "tx#rx");
cmdline_parse_token_num_t cmd_setqmap_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_qmap_result,
			      port_id, UINT8);
cmdline_parse_token_num_t cmd_setqmap_queueid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_qmap_result,
			      queue_id, UINT16);
cmdline_parse_token_num_t cmd_setqmap_mapvalue =
	TOKEN_NUM_INITIALIZER(struct cmd_set_qmap_result,
			      map_value, UINT8);

cmdline_parse_inst_t cmd_set_qmap = {
	.f = cmd_set_qmap_parsed,
	.data = NULL,
	.help_str = "Set statistics mapping value on tx|rx queue_id of port_id",
	.tokens = {
		(void *)&cmd_setqmap_set,
		(void *)&cmd_setqmap_qmap,
		(void *)&cmd_setqmap_what,
		(void *)&cmd_setqmap_portid,
		(void *)&cmd_setqmap_queueid,
		(void *)&cmd_setqmap_mapvalue,
		NULL,
	},
};

/* *** CONFIGURE UNICAST HASH TABLE *** */
struct cmd_set_uc_hash_table {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t what;
	struct ether_addr address;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_uc_hash_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret=0;
	struct cmd_set_uc_hash_table *res = parsed_result;

	int is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	if (strcmp(res->what, "uta") == 0)
		ret = rte_eth_dev_uc_hash_table_set(res->port_id,
						&res->address,(uint8_t)is_on);
	if (ret < 0)
		printf("bad unicast hash table parameter, return code = %d \n", ret);

}

cmdline_parse_token_string_t cmd_set_uc_hash_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 set, "set");
cmdline_parse_token_string_t cmd_set_uc_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 port, "port");
cmdline_parse_token_num_t cmd_set_uc_hash_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_uc_hash_table,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_set_uc_hash_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 what, "uta");
cmdline_parse_token_etheraddr_t cmd_set_uc_hash_mac =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_uc_hash_table,
				address);
cmdline_parse_token_string_t cmd_set_uc_hash_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 mode, "on#off");

cmdline_parse_inst_t cmd_set_uc_hash_filter = {
	.f = cmd_set_uc_hash_parsed,
	.data = NULL,
	.help_str = "set port X uta Y on|off(X = port number,Y = MAC address)",
	.tokens = {
		(void *)&cmd_set_uc_hash_set,
		(void *)&cmd_set_uc_hash_port,
		(void *)&cmd_set_uc_hash_portid,
		(void *)&cmd_set_uc_hash_what,
		(void *)&cmd_set_uc_hash_mac,
		(void *)&cmd_set_uc_hash_mode,
		NULL,
	},
};

struct cmd_set_uc_all_hash_table {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t value;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_uc_all_hash_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret=0;
	struct cmd_set_uc_all_hash_table *res = parsed_result;

	int is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	if ((strcmp(res->what, "uta") == 0) &&
		(strcmp(res->value, "all") == 0))
		ret = rte_eth_dev_uc_all_hash_table_set(res->port_id,(uint8_t) is_on);
	if (ret < 0)
		printf("bad unicast hash table parameter,"
			"return code = %d \n", ret);
}

cmdline_parse_token_string_t cmd_set_uc_all_hash_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 set, "set");
cmdline_parse_token_string_t cmd_set_uc_all_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 port, "port");
cmdline_parse_token_num_t cmd_set_uc_all_hash_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_uc_all_hash_table,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_set_uc_all_hash_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 what, "uta");
cmdline_parse_token_string_t cmd_set_uc_all_hash_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				value,"all");
cmdline_parse_token_string_t cmd_set_uc_all_hash_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 mode, "on#off");

cmdline_parse_inst_t cmd_set_uc_all_hash_filter = {
	.f = cmd_set_uc_all_hash_parsed,
	.data = NULL,
	.help_str = "set port X uta all on|off (X = port number)",
	.tokens = {
		(void *)&cmd_set_uc_all_hash_set,
		(void *)&cmd_set_uc_all_hash_port,
		(void *)&cmd_set_uc_all_hash_portid,
		(void *)&cmd_set_uc_all_hash_what,
		(void *)&cmd_set_uc_all_hash_value,
		(void *)&cmd_set_uc_all_hash_mode,
		NULL,
	},
};

/* *** CONFIGURE MACVLAN FILTER FOR VF(s) *** */
struct cmd_set_vf_macvlan_filter {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
	struct ether_addr address;
	cmdline_fixed_string_t filter_type;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_vf_macvlan_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int is_on, ret = 0;
	struct cmd_set_vf_macvlan_filter *res = parsed_result;
	struct rte_eth_mac_filter filter;

	memset(&filter, 0, sizeof(struct rte_eth_mac_filter));

	(void)rte_memcpy(&filter.mac_addr, &res->address, ETHER_ADDR_LEN);

	/* set VF MAC filter */
	filter.is_vf = 1;

	/* set VF ID */
	filter.dst_id = res->vf_id;

	if (!strcmp(res->filter_type, "exact-mac"))
		filter.filter_type = RTE_MAC_PERFECT_MATCH;
	else if (!strcmp(res->filter_type, "exact-mac-vlan"))
		filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
	else if (!strcmp(res->filter_type, "hashmac"))
		filter.filter_type = RTE_MAC_HASH_MATCH;
	else if (!strcmp(res->filter_type, "hashmac-vlan"))
		filter.filter_type = RTE_MACVLAN_HASH_MATCH;

	is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	if (is_on)
		ret = rte_eth_dev_filter_ctrl(res->port_id,
					RTE_ETH_FILTER_MACVLAN,
					RTE_ETH_FILTER_ADD,
					 &filter);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
					RTE_ETH_FILTER_MACVLAN,
					RTE_ETH_FILTER_DELETE,
					&filter);

	if (ret < 0)
		printf("bad set MAC hash parameter, return code = %d\n", ret);

}

cmdline_parse_token_string_t cmd_set_vf_macvlan_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				 set, "set");
cmdline_parse_token_string_t cmd_set_vf_macvlan_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				 port, "port");
cmdline_parse_token_num_t cmd_set_vf_macvlan_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_macvlan_filter,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_set_vf_macvlan_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				 vf, "vf");
cmdline_parse_token_num_t cmd_set_vf_macvlan_vf_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				vf_id, UINT8);
cmdline_parse_token_etheraddr_t cmd_set_vf_macvlan_mac =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				address);
cmdline_parse_token_string_t cmd_set_vf_macvlan_filter_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				filter_type, "exact-mac#exact-mac-vlan"
				"#hashmac#hashmac-vlan");
cmdline_parse_token_string_t cmd_set_vf_macvlan_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_macvlan_filter,
				 mode, "on#off");

cmdline_parse_inst_t cmd_set_vf_macvlan_filter = {
	.f = cmd_set_vf_macvlan_parsed,
	.data = NULL,
	.help_str = "set port (portid) vf (vfid) (mac-addr) "
			"(exact-mac|exact-mac-vlan|hashmac|hashmac-vlan) "
			"on|off\n"
			"exact match rule:exact match of MAC or MAC and VLAN; "
			"hash match rule: hash match of MAC and exact match "
			"of VLAN",
	.tokens = {
		(void *)&cmd_set_vf_macvlan_set,
		(void *)&cmd_set_vf_macvlan_port,
		(void *)&cmd_set_vf_macvlan_portid,
		(void *)&cmd_set_vf_macvlan_vf,
		(void *)&cmd_set_vf_macvlan_vf_id,
		(void *)&cmd_set_vf_macvlan_mac,
		(void *)&cmd_set_vf_macvlan_filter_type,
		(void *)&cmd_set_vf_macvlan_mode,
		NULL,
	},
};

/* *** CONFIGURE VF TRAFFIC CONTROL *** */
struct cmd_set_vf_traffic {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_vf_traffic_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	struct cmd_set_vf_traffic *res = parsed_result;
	int is_rx = (strcmp(res->what, "rx") == 0) ? 1 : 0;
	int is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	set_vf_traffic(res->port_id, (uint8_t)is_rx, res->vf_id,(uint8_t) is_on);
}

cmdline_parse_token_string_t cmd_setvf_traffic_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 set, "set");
cmdline_parse_token_string_t cmd_setvf_traffic_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 port, "port");
cmdline_parse_token_num_t cmd_setvf_traffic_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_traffic,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_setvf_traffic_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 vf, "vf");
cmdline_parse_token_num_t cmd_setvf_traffic_vfid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_traffic,
			      vf_id, UINT8);
cmdline_parse_token_string_t cmd_setvf_traffic_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 what, "tx#rx");
cmdline_parse_token_string_t cmd_setvf_traffic_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 mode, "on#off");

cmdline_parse_inst_t cmd_set_vf_traffic = {
	.f = cmd_set_vf_traffic_parsed,
	.data = NULL,
	.help_str = "set port X vf Y rx|tx on|off"
			"(X = port number,Y = vf id)",
	.tokens = {
		(void *)&cmd_setvf_traffic_set,
		(void *)&cmd_setvf_traffic_port,
		(void *)&cmd_setvf_traffic_portid,
		(void *)&cmd_setvf_traffic_vf,
		(void *)&cmd_setvf_traffic_vfid,
		(void *)&cmd_setvf_traffic_what,
		(void *)&cmd_setvf_traffic_mode,
		NULL,
	},
};

/* *** CONFIGURE VF RECEIVE MODE *** */
struct cmd_set_vf_rxmode {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t on;
};

static void
cmd_set_vf_rxmode_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret;
	uint16_t rx_mode = 0;
	struct cmd_set_vf_rxmode *res = parsed_result;

	int is_on = (strcmp(res->on, "on") == 0) ? 1 : 0;
	if (!strcmp(res->what,"rxmode")) {
		if (!strcmp(res->mode, "AUPE"))
			rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;
		else if (!strcmp(res->mode, "ROPE"))
			rx_mode |= ETH_VMDQ_ACCEPT_HASH_UC;
		else if (!strcmp(res->mode, "BAM"))
			rx_mode |= ETH_VMDQ_ACCEPT_BROADCAST;
		else if (!strncmp(res->mode, "MPE",3))
			rx_mode |= ETH_VMDQ_ACCEPT_MULTICAST;
	}

	ret = rte_eth_dev_set_vf_rxmode(res->port_id,res->vf_id,rx_mode,(uint8_t)is_on);
	if (ret < 0)
		printf("bad VF receive mode parameter, return code = %d \n",
		ret);
}

cmdline_parse_token_string_t cmd_set_vf_rxmode_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 set, "set");
cmdline_parse_token_string_t cmd_set_vf_rxmode_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 port, "port");
cmdline_parse_token_num_t cmd_set_vf_rxmode_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_rxmode,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_set_vf_rxmode_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 vf, "vf");
cmdline_parse_token_num_t cmd_set_vf_rxmode_vfid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_rxmode,
			      vf_id, UINT8);
cmdline_parse_token_string_t cmd_set_vf_rxmode_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 what, "rxmode");
cmdline_parse_token_string_t cmd_set_vf_rxmode_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 mode, "AUPE#ROPE#BAM#MPE");
cmdline_parse_token_string_t cmd_set_vf_rxmode_on =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 on, "on#off");

cmdline_parse_inst_t cmd_set_vf_rxmode = {
	.f = cmd_set_vf_rxmode_parsed,
	.data = NULL,
	.help_str = "set port X vf Y rxmode AUPE|ROPE|BAM|MPE on|off",
	.tokens = {
		(void *)&cmd_set_vf_rxmode_set,
		(void *)&cmd_set_vf_rxmode_port,
		(void *)&cmd_set_vf_rxmode_portid,
		(void *)&cmd_set_vf_rxmode_vf,
		(void *)&cmd_set_vf_rxmode_vfid,
		(void *)&cmd_set_vf_rxmode_what,
		(void *)&cmd_set_vf_rxmode_mode,
		(void *)&cmd_set_vf_rxmode_on,
		NULL,
	},
};

/* *** ADD MAC ADDRESS FILTER FOR A VF OF A PORT *** */
struct cmd_vf_mac_addr_result {
	cmdline_fixed_string_t mac_addr_cmd;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t port;
	uint8_t port_num;
	cmdline_fixed_string_t vf;
	uint8_t vf_num;
	struct ether_addr address;
};

static void cmd_vf_mac_addr_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_vf_mac_addr_result *res = parsed_result;
	int ret = 0;

	if (strcmp(res->what, "add") == 0)
		ret = rte_eth_dev_mac_addr_add(res->port_num,
					&res->address, res->vf_num);
	if(ret < 0)
		printf("vf_mac_addr_cmd error: (%s)\n", strerror(-ret));

}

cmdline_parse_token_string_t cmd_vf_mac_addr_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_mac_addr_result,
				mac_addr_cmd,"mac_addr");
cmdline_parse_token_string_t cmd_vf_mac_addr_what =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_mac_addr_result,
				what,"add");
cmdline_parse_token_string_t cmd_vf_mac_addr_port =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_mac_addr_result,
				port,"port");
cmdline_parse_token_num_t cmd_vf_mac_addr_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_mac_addr_result,
				port_num, UINT8);
cmdline_parse_token_string_t cmd_vf_mac_addr_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_mac_addr_result,
				vf,"vf");
cmdline_parse_token_num_t cmd_vf_mac_addr_vfnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_mac_addr_result,
				vf_num, UINT8);
cmdline_parse_token_etheraddr_t cmd_vf_mac_addr_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_vf_mac_addr_result,
				address);

cmdline_parse_inst_t cmd_vf_mac_addr_filter = {
	.f = cmd_vf_mac_addr_parsed,
	.data = (void *)0,
	.help_str = "mac_addr add port X vf Y ethaddr:(X = port number,"
	"Y = VF number)add MAC address filtering for a VF on port X",
	.tokens = {
		(void *)&cmd_vf_mac_addr_cmd,
		(void *)&cmd_vf_mac_addr_what,
		(void *)&cmd_vf_mac_addr_port,
		(void *)&cmd_vf_mac_addr_portnum,
		(void *)&cmd_vf_mac_addr_vf,
		(void *)&cmd_vf_mac_addr_vfnum,
		(void *)&cmd_vf_mac_addr_addr,
		NULL,
	},
};

/* *** ADD/REMOVE A VLAN IDENTIFIER TO/FROM A PORT VLAN RX FILTER *** */
struct cmd_vf_rx_vlan_filter {
	cmdline_fixed_string_t rx_vlan;
	cmdline_fixed_string_t what;
	uint16_t vlan_id;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t vf;
	uint64_t vf_mask;
};

static void
cmd_vf_rx_vlan_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_vf_rx_vlan_filter *res = parsed_result;

	if (!strcmp(res->what, "add"))
		set_vf_rx_vlan(res->port_id, res->vlan_id,res->vf_mask, 1);
	else
		set_vf_rx_vlan(res->port_id, res->vlan_id,res->vf_mask, 0);
}

cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_rx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 rx_vlan, "rx_vlan");
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_what =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 what, "add#rm");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      vlan_id, UINT16);
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_port =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 port, "port");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 vf, "vf");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_vf_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      vf_mask, UINT64);

cmdline_parse_inst_t cmd_vf_rxvlan_filter = {
	.f = cmd_vf_rx_vlan_filter_parsed,
	.data = NULL,
	.help_str = "rx_vlan add|rm X port Y vf Z (X = VLAN ID,"
		"Y = port number,Z = hexadecimal VF mask)",
	.tokens = {
		(void *)&cmd_vf_rx_vlan_filter_rx_vlan,
		(void *)&cmd_vf_rx_vlan_filter_what,
		(void *)&cmd_vf_rx_vlan_filter_vlanid,
		(void *)&cmd_vf_rx_vlan_filter_port,
		(void *)&cmd_vf_rx_vlan_filter_portid,
		(void *)&cmd_vf_rx_vlan_filter_vf,
		(void *)&cmd_vf_rx_vlan_filter_vf_mask,
		NULL,
	},
};

/* *** SET RATE LIMIT FOR A QUEUE OF A PORT *** */
struct cmd_queue_rate_limit_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_num;
	cmdline_fixed_string_t queue;
	uint8_t queue_num;
	cmdline_fixed_string_t rate;
	uint16_t rate_num;
};

static void cmd_queue_rate_limit_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_queue_rate_limit_result *res = parsed_result;
	int ret = 0;

	if ((strcmp(res->set, "set") == 0) && (strcmp(res->port, "port") == 0)
		&& (strcmp(res->queue, "queue") == 0)
		&& (strcmp(res->rate, "rate") == 0))
		ret = set_queue_rate_limit(res->port_num, res->queue_num,
					res->rate_num);
	if (ret < 0)
		printf("queue_rate_limit_cmd error: (%s)\n", strerror(-ret));

}

cmdline_parse_token_string_t cmd_queue_rate_limit_set =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				set, "set");
cmdline_parse_token_string_t cmd_queue_rate_limit_port =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				port, "port");
cmdline_parse_token_num_t cmd_queue_rate_limit_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				port_num, UINT8);
cmdline_parse_token_string_t cmd_queue_rate_limit_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_queue_rate_limit_queuenum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				queue_num, UINT8);
cmdline_parse_token_string_t cmd_queue_rate_limit_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				rate, "rate");
cmdline_parse_token_num_t cmd_queue_rate_limit_ratenum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				rate_num, UINT16);

cmdline_parse_inst_t cmd_queue_rate_limit = {
	.f = cmd_queue_rate_limit_parsed,
	.data = (void *)0,
	.help_str = "set port X queue Y rate Z:(X = port number,"
	"Y = queue number,Z = rate number)set rate limit for a queue on port X",
	.tokens = {
		(void *)&cmd_queue_rate_limit_set,
		(void *)&cmd_queue_rate_limit_port,
		(void *)&cmd_queue_rate_limit_portnum,
		(void *)&cmd_queue_rate_limit_queue,
		(void *)&cmd_queue_rate_limit_queuenum,
		(void *)&cmd_queue_rate_limit_rate,
		(void *)&cmd_queue_rate_limit_ratenum,
		NULL,
	},
};

/* *** SET RATE LIMIT FOR A VF OF A PORT *** */
struct cmd_vf_rate_limit_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_num;
	cmdline_fixed_string_t vf;
	uint8_t vf_num;
	cmdline_fixed_string_t rate;
	uint16_t rate_num;
	cmdline_fixed_string_t q_msk;
	uint64_t q_msk_val;
};

static void cmd_vf_rate_limit_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_vf_rate_limit_result *res = parsed_result;
	int ret = 0;

	if ((strcmp(res->set, "set") == 0) && (strcmp(res->port, "port") == 0)
		&& (strcmp(res->vf, "vf") == 0)
		&& (strcmp(res->rate, "rate") == 0)
		&& (strcmp(res->q_msk, "queue_mask") == 0))
		ret = set_vf_rate_limit(res->port_num, res->vf_num,
					res->rate_num, res->q_msk_val);
	if (ret < 0)
		printf("vf_rate_limit_cmd error: (%s)\n", strerror(-ret));

}

cmdline_parse_token_string_t cmd_vf_rate_limit_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				set, "set");
cmdline_parse_token_string_t cmd_vf_rate_limit_port =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				port, "port");
cmdline_parse_token_num_t cmd_vf_rate_limit_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				port_num, UINT8);
cmdline_parse_token_string_t cmd_vf_rate_limit_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				vf, "vf");
cmdline_parse_token_num_t cmd_vf_rate_limit_vfnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				vf_num, UINT8);
cmdline_parse_token_string_t cmd_vf_rate_limit_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				rate, "rate");
cmdline_parse_token_num_t cmd_vf_rate_limit_ratenum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				rate_num, UINT16);
cmdline_parse_token_string_t cmd_vf_rate_limit_q_msk =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				q_msk, "queue_mask");
cmdline_parse_token_num_t cmd_vf_rate_limit_q_msk_val =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				q_msk_val, UINT64);

cmdline_parse_inst_t cmd_vf_rate_limit = {
	.f = cmd_vf_rate_limit_parsed,
	.data = (void *)0,
	.help_str = "set port X vf Y rate Z queue_mask V:(X = port number,"
	"Y = VF number,Z = rate number, V = queue mask value)set rate limit "
	"for queues of VF on port X",
	.tokens = {
		(void *)&cmd_vf_rate_limit_set,
		(void *)&cmd_vf_rate_limit_port,
		(void *)&cmd_vf_rate_limit_portnum,
		(void *)&cmd_vf_rate_limit_vf,
		(void *)&cmd_vf_rate_limit_vfnum,
		(void *)&cmd_vf_rate_limit_rate,
		(void *)&cmd_vf_rate_limit_ratenum,
		(void *)&cmd_vf_rate_limit_q_msk,
		(void *)&cmd_vf_rate_limit_q_msk_val,
		NULL,
	},
};

/* *** ADD TUNNEL FILTER OF A PORT *** */
struct cmd_tunnel_filter_result {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t what;
	uint8_t port_id;
	struct ether_addr outer_mac;
	struct ether_addr inner_mac;
	cmdline_ipaddr_t ip_value;
	uint16_t inner_vlan;
	cmdline_fixed_string_t tunnel_type;
	cmdline_fixed_string_t filter_type;
	uint32_t tenant_id;
	uint16_t queue_num;
};

static void
cmd_tunnel_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_tunnel_filter_result *res = parsed_result;
	struct rte_eth_tunnel_filter_conf tunnel_filter_conf;
	int ret = 0;

	memset(&tunnel_filter_conf, 0, sizeof(tunnel_filter_conf));

	ether_addr_copy(&res->outer_mac, &tunnel_filter_conf.outer_mac);
	ether_addr_copy(&res->inner_mac, &tunnel_filter_conf.inner_mac);
	tunnel_filter_conf.inner_vlan = res->inner_vlan;

	if (res->ip_value.family == AF_INET) {
		tunnel_filter_conf.ip_addr.ipv4_addr =
			res->ip_value.addr.ipv4.s_addr;
		tunnel_filter_conf.ip_type = RTE_TUNNEL_IPTYPE_IPV4;
	} else {
		memcpy(&(tunnel_filter_conf.ip_addr.ipv6_addr),
			&(res->ip_value.addr.ipv6),
			sizeof(struct in6_addr));
		tunnel_filter_conf.ip_type = RTE_TUNNEL_IPTYPE_IPV6;
	}

	if (!strcmp(res->filter_type, "imac-ivlan"))
		tunnel_filter_conf.filter_type = RTE_TUNNEL_FILTER_IMAC_IVLAN;
	else if (!strcmp(res->filter_type, "imac-ivlan-tenid"))
		tunnel_filter_conf.filter_type =
			RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID;
	else if (!strcmp(res->filter_type, "imac-tenid"))
		tunnel_filter_conf.filter_type = RTE_TUNNEL_FILTER_IMAC_TENID;
	else if (!strcmp(res->filter_type, "imac"))
		tunnel_filter_conf.filter_type = ETH_TUNNEL_FILTER_IMAC;
	else if (!strcmp(res->filter_type, "omac-imac-tenid"))
		tunnel_filter_conf.filter_type =
			RTE_TUNNEL_FILTER_OMAC_TENID_IMAC;
	else if (!strcmp(res->filter_type, "oip"))
		tunnel_filter_conf.filter_type = ETH_TUNNEL_FILTER_OIP;
	else if (!strcmp(res->filter_type, "iip"))
		tunnel_filter_conf.filter_type = ETH_TUNNEL_FILTER_IIP;
	else {
		printf("The filter type is not supported");
		return;
	}

	if (!strcmp(res->tunnel_type, "vxlan"))
		tunnel_filter_conf.tunnel_type = RTE_TUNNEL_TYPE_VXLAN;
	else if (!strcmp(res->tunnel_type, "nvgre"))
		tunnel_filter_conf.tunnel_type = RTE_TUNNEL_TYPE_NVGRE;
	else if (!strcmp(res->tunnel_type, "ipingre"))
		tunnel_filter_conf.tunnel_type = RTE_TUNNEL_TYPE_IP_IN_GRE;
	else {
		printf("The tunnel type %s not supported.\n", res->tunnel_type);
		return;
	}

	tunnel_filter_conf.tenant_id = res->tenant_id;
	tunnel_filter_conf.queue_id = res->queue_num;
	if (!strcmp(res->what, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id,
					RTE_ETH_FILTER_TUNNEL,
					RTE_ETH_FILTER_ADD,
					&tunnel_filter_conf);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
					RTE_ETH_FILTER_TUNNEL,
					RTE_ETH_FILTER_DELETE,
					&tunnel_filter_conf);
	if (ret < 0)
		printf("cmd_tunnel_filter_parsed error: (%s)\n",
				strerror(-ret));

}
cmdline_parse_token_string_t cmd_tunnel_filter_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_filter_result,
	cmd, "tunnel_filter");
cmdline_parse_token_string_t cmd_tunnel_filter_what =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_filter_result,
	what, "add#rm");
cmdline_parse_token_num_t cmd_tunnel_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_filter_result,
	port_id, UINT8);
cmdline_parse_token_etheraddr_t cmd_tunnel_filter_outer_mac =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_tunnel_filter_result,
	outer_mac);
cmdline_parse_token_etheraddr_t cmd_tunnel_filter_inner_mac =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_tunnel_filter_result,
	inner_mac);
cmdline_parse_token_num_t cmd_tunnel_filter_innner_vlan =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_filter_result,
	inner_vlan, UINT16);
cmdline_parse_token_ipaddr_t cmd_tunnel_filter_ip_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_tunnel_filter_result,
	ip_value);
cmdline_parse_token_string_t cmd_tunnel_filter_tunnel_type =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_filter_result,
	tunnel_type, "vxlan#nvgre#ipingre");

cmdline_parse_token_string_t cmd_tunnel_filter_filter_type =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_filter_result,
	filter_type, "oip#iip#imac-ivlan#imac-ivlan-tenid#imac-tenid#"
		"imac#omac-imac-tenid");
cmdline_parse_token_num_t cmd_tunnel_filter_tenant_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_filter_result,
	tenant_id, UINT32);
cmdline_parse_token_num_t cmd_tunnel_filter_queue_num =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_filter_result,
	queue_num, UINT16);

cmdline_parse_inst_t cmd_tunnel_filter = {
	.f = cmd_tunnel_filter_parsed,
	.data = (void *)0,
	.help_str = "add/rm tunnel filter of a port: "
			"tunnel_filter add port_id outer_mac inner_mac ip "
			"inner_vlan tunnel_type(vxlan|nvgre|ipingre) filter_type "
			"(oip|iip|imac-ivlan|imac-ivlan-tenid|imac-tenid|"
			"imac|omac-imac-tenid) "
			"tenant_id queue_num",
	.tokens = {
		(void *)&cmd_tunnel_filter_cmd,
		(void *)&cmd_tunnel_filter_what,
		(void *)&cmd_tunnel_filter_port_id,
		(void *)&cmd_tunnel_filter_outer_mac,
		(void *)&cmd_tunnel_filter_inner_mac,
		(void *)&cmd_tunnel_filter_ip_value,
		(void *)&cmd_tunnel_filter_innner_vlan,
		(void *)&cmd_tunnel_filter_tunnel_type,
		(void *)&cmd_tunnel_filter_filter_type,
		(void *)&cmd_tunnel_filter_tenant_id,
		(void *)&cmd_tunnel_filter_queue_num,
		NULL,
	},
};

/* *** CONFIGURE TUNNEL UDP PORT *** */
struct cmd_tunnel_udp_config {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t what;
	uint16_t udp_port;
	uint8_t port_id;
};

static void
cmd_tunnel_udp_config_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_tunnel_udp_config *res = parsed_result;
	struct rte_eth_udp_tunnel tunnel_udp;
	int ret;

	tunnel_udp.udp_port = res->udp_port;

	if (!strcmp(res->cmd, "rx_vxlan_port"))
		tunnel_udp.prot_type = RTE_TUNNEL_TYPE_VXLAN;

	if (!strcmp(res->what, "add"))
		ret = rte_eth_dev_udp_tunnel_port_add(res->port_id,
						      &tunnel_udp);
	else
		ret = rte_eth_dev_udp_tunnel_port_delete(res->port_id,
							 &tunnel_udp);

	if (ret < 0)
		printf("udp tunneling add error: (%s)\n", strerror(-ret));
}

cmdline_parse_token_string_t cmd_tunnel_udp_config_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_udp_config,
				cmd, "rx_vxlan_port");
cmdline_parse_token_string_t cmd_tunnel_udp_config_what =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_udp_config,
				what, "add#rm");
cmdline_parse_token_num_t cmd_tunnel_udp_config_udp_port =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_udp_config,
				udp_port, UINT16);
cmdline_parse_token_num_t cmd_tunnel_udp_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_udp_config,
				port_id, UINT8);

cmdline_parse_inst_t cmd_tunnel_udp_config = {
	.f = cmd_tunnel_udp_config_parsed,
	.data = (void *)0,
	.help_str = "add/rm an tunneling UDP port filter: "
			"rx_vxlan_port add udp_port port_id",
	.tokens = {
		(void *)&cmd_tunnel_udp_config_cmd,
		(void *)&cmd_tunnel_udp_config_what,
		(void *)&cmd_tunnel_udp_config_udp_port,
		(void *)&cmd_tunnel_udp_config_port_id,
		NULL,
	},
};

/* *** GLOBAL CONFIG *** */
struct cmd_global_config_result {
	cmdline_fixed_string_t cmd;
	uint8_t port_id;
	cmdline_fixed_string_t cfg_type;
	uint8_t len;
};

static void
cmd_global_config_parsed(void *parsed_result,
			 __attribute__((unused)) struct cmdline *cl,
			 __attribute__((unused)) void *data)
{
	struct cmd_global_config_result *res = parsed_result;
	struct rte_eth_global_cfg conf;
	int ret;

	memset(&conf, 0, sizeof(conf));
	conf.cfg_type = RTE_ETH_GLOBAL_CFG_TYPE_GRE_KEY_LEN;
	conf.cfg.gre_key_len = res->len;
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_NONE,
				      RTE_ETH_FILTER_SET, &conf);
	if (ret != 0)
		printf("Global config error\n");
}

cmdline_parse_token_string_t cmd_global_config_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_global_config_result, cmd,
		"global_config");
cmdline_parse_token_num_t cmd_global_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_global_config_result, port_id, UINT8);
cmdline_parse_token_string_t cmd_global_config_type =
	TOKEN_STRING_INITIALIZER(struct cmd_global_config_result,
		cfg_type, "gre-key-len");
cmdline_parse_token_num_t cmd_global_config_gre_key_len =
	TOKEN_NUM_INITIALIZER(struct cmd_global_config_result,
		len, UINT8);

cmdline_parse_inst_t cmd_global_config = {
	.f = cmd_global_config_parsed,
	.data = (void *)NULL,
	.help_str = "global_config <port_id> gre-key-len <number>",
	.tokens = {
		(void *)&cmd_global_config_cmd,
		(void *)&cmd_global_config_port_id,
		(void *)&cmd_global_config_type,
		(void *)&cmd_global_config_gre_key_len,
		NULL,
	},
};

/* *** CONFIGURE VM MIRROR VLAN/POOL RULE *** */
struct cmd_set_mirror_mask_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t mirror;
	uint8_t rule_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t value;
	cmdline_fixed_string_t dstpool;
	uint8_t dstpool_id;
	cmdline_fixed_string_t on;
};

cmdline_parse_token_string_t cmd_mirror_mask_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				set, "set");
cmdline_parse_token_string_t cmd_mirror_mask_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				port, "port");
cmdline_parse_token_num_t cmd_mirror_mask_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_mask_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_mask_mirror =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				mirror, "mirror-rule");
cmdline_parse_token_num_t cmd_mirror_mask_ruleid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_mask_result,
				rule_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_mask_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				what, "pool-mirror-up#pool-mirror-down"
				      "#vlan-mirror");
cmdline_parse_token_string_t cmd_mirror_mask_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				value, NULL);
cmdline_parse_token_string_t cmd_mirror_mask_dstpool =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				dstpool, "dst-pool");
cmdline_parse_token_num_t cmd_mirror_mask_poolid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_mask_result,
				dstpool_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_mask_on =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_mask_result,
				on, "on#off");

static void
cmd_set_mirror_mask_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret,nb_item,i;
	struct cmd_set_mirror_mask_result *res = parsed_result;
	struct rte_eth_mirror_conf mr_conf;

	memset(&mr_conf, 0, sizeof(struct rte_eth_mirror_conf));

	unsigned int vlan_list[ETH_MIRROR_MAX_VLANS];

	mr_conf.dst_pool = res->dstpool_id;

	if (!strcmp(res->what, "pool-mirror-up")) {
		mr_conf.pool_mask = strtoull(res->value, NULL, 16);
		mr_conf.rule_type = ETH_MIRROR_VIRTUAL_POOL_UP;
	} else if (!strcmp(res->what, "pool-mirror-down")) {
		mr_conf.pool_mask = strtoull(res->value, NULL, 16);
		mr_conf.rule_type = ETH_MIRROR_VIRTUAL_POOL_DOWN;
	} else if (!strcmp(res->what, "vlan-mirror")) {
		mr_conf.rule_type = ETH_MIRROR_VLAN;
		nb_item = parse_item_list(res->value, "vlan",
				ETH_MIRROR_MAX_VLANS, vlan_list, 1);
		if (nb_item <= 0)
			return;

		for (i = 0; i < nb_item; i++) {
			if (vlan_list[i] > ETHER_MAX_VLAN_ID) {
				printf("Invalid vlan_id: must be < 4096\n");
				return;
			}

			mr_conf.vlan.vlan_id[i] = (uint16_t)vlan_list[i];
			mr_conf.vlan.vlan_mask |= 1ULL << i;
		}
	}

	if (!strcmp(res->on, "on"))
		ret = rte_eth_mirror_rule_set(res->port_id, &mr_conf,
						res->rule_id, 1);
	else
		ret = rte_eth_mirror_rule_set(res->port_id, &mr_conf,
						res->rule_id, 0);
	if (ret < 0)
		printf("mirror rule add error: (%s)\n", strerror(-ret));
}

cmdline_parse_inst_t cmd_set_mirror_mask = {
		.f = cmd_set_mirror_mask_parsed,
		.data = NULL,
		.help_str = "set port X mirror-rule Y pool-mirror-up|pool-mirror-down|vlan-mirror"
			    " pool_mask|vlan_id[,vlan_id]* dst-pool Z on|off",
		.tokens = {
			(void *)&cmd_mirror_mask_set,
			(void *)&cmd_mirror_mask_port,
			(void *)&cmd_mirror_mask_portid,
			(void *)&cmd_mirror_mask_mirror,
			(void *)&cmd_mirror_mask_ruleid,
			(void *)&cmd_mirror_mask_what,
			(void *)&cmd_mirror_mask_value,
			(void *)&cmd_mirror_mask_dstpool,
			(void *)&cmd_mirror_mask_poolid,
			(void *)&cmd_mirror_mask_on,
			NULL,
		},
};

/* *** CONFIGURE VM MIRROR UDLINK/DOWNLINK RULE *** */
struct cmd_set_mirror_link_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t mirror;
	uint8_t rule_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t dstpool;
	uint8_t dstpool_id;
	cmdline_fixed_string_t on;
};

cmdline_parse_token_string_t cmd_mirror_link_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				 set, "set");
cmdline_parse_token_string_t cmd_mirror_link_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				port, "port");
cmdline_parse_token_num_t cmd_mirror_link_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_link_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_link_mirror =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				mirror, "mirror-rule");
cmdline_parse_token_num_t cmd_mirror_link_ruleid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_link_result,
			    rule_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_link_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				what, "uplink-mirror#downlink-mirror");
cmdline_parse_token_string_t cmd_mirror_link_dstpool =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				dstpool, "dst-pool");
cmdline_parse_token_num_t cmd_mirror_link_poolid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mirror_link_result,
				dstpool_id, UINT8);
cmdline_parse_token_string_t cmd_mirror_link_on =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mirror_link_result,
				on, "on#off");

static void
cmd_set_mirror_link_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret;
	struct cmd_set_mirror_link_result *res = parsed_result;
	struct rte_eth_mirror_conf mr_conf;

	memset(&mr_conf, 0, sizeof(struct rte_eth_mirror_conf));
	if (!strcmp(res->what, "uplink-mirror"))
		mr_conf.rule_type = ETH_MIRROR_UPLINK_PORT;
	else
		mr_conf.rule_type = ETH_MIRROR_DOWNLINK_PORT;

	mr_conf.dst_pool = res->dstpool_id;

	if (!strcmp(res->on, "on"))
		ret = rte_eth_mirror_rule_set(res->port_id, &mr_conf,
						res->rule_id, 1);
	else
		ret = rte_eth_mirror_rule_set(res->port_id, &mr_conf,
						res->rule_id, 0);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		printf("mirror rule add error: (%s)\n", strerror(-ret));

}

cmdline_parse_inst_t cmd_set_mirror_link = {
		.f = cmd_set_mirror_link_parsed,
		.data = NULL,
		.help_str = "set port X mirror-rule Y uplink-mirror|"
			"downlink-mirror dst-pool Z on|off",
		.tokens = {
			(void *)&cmd_mirror_link_set,
			(void *)&cmd_mirror_link_port,
			(void *)&cmd_mirror_link_portid,
			(void *)&cmd_mirror_link_mirror,
			(void *)&cmd_mirror_link_ruleid,
			(void *)&cmd_mirror_link_what,
			(void *)&cmd_mirror_link_dstpool,
			(void *)&cmd_mirror_link_poolid,
			(void *)&cmd_mirror_link_on,
			NULL,
		},
};

/* *** RESET VM MIRROR RULE *** */
struct cmd_rm_mirror_rule_result {
	cmdline_fixed_string_t reset;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t mirror;
	uint8_t rule_id;
};

cmdline_parse_token_string_t cmd_rm_mirror_rule_reset =
	TOKEN_STRING_INITIALIZER(struct cmd_rm_mirror_rule_result,
				 reset, "reset");
cmdline_parse_token_string_t cmd_rm_mirror_rule_port =
	TOKEN_STRING_INITIALIZER(struct cmd_rm_mirror_rule_result,
				port, "port");
cmdline_parse_token_num_t cmd_rm_mirror_rule_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_rm_mirror_rule_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_rm_mirror_rule_mirror =
	TOKEN_STRING_INITIALIZER(struct cmd_rm_mirror_rule_result,
				mirror, "mirror-rule");
cmdline_parse_token_num_t cmd_rm_mirror_rule_ruleid =
	TOKEN_NUM_INITIALIZER(struct cmd_rm_mirror_rule_result,
				rule_id, UINT8);

static void
cmd_reset_mirror_rule_parsed(void *parsed_result,
		       __attribute__((unused)) struct cmdline *cl,
		       __attribute__((unused)) void *data)
{
	int ret;
	struct cmd_set_mirror_link_result *res = parsed_result;
        /* check rule_id */
	ret = rte_eth_mirror_rule_reset(res->port_id,res->rule_id);
	if(ret < 0)
		printf("mirror rule remove error: (%s)\n", strerror(-ret));
}

cmdline_parse_inst_t cmd_reset_mirror_rule = {
		.f = cmd_reset_mirror_rule_parsed,
		.data = NULL,
		.help_str = "reset port X mirror-rule Y",
		.tokens = {
			(void *)&cmd_rm_mirror_rule_reset,
			(void *)&cmd_rm_mirror_rule_port,
			(void *)&cmd_rm_mirror_rule_portid,
			(void *)&cmd_rm_mirror_rule_mirror,
			(void *)&cmd_rm_mirror_rule_ruleid,
			NULL,
		},
};

/* ******************************************************************************** */

struct cmd_dump_result {
	cmdline_fixed_string_t dump;
};

static void
dump_struct_sizes(void)
{
#define DUMP_SIZE(t) printf("sizeof(" #t ") = %u\n", (unsigned)sizeof(t));
	DUMP_SIZE(struct rte_mbuf);
	DUMP_SIZE(struct rte_mempool);
	DUMP_SIZE(struct rte_ring);
#undef DUMP_SIZE
}

static void cmd_dump_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_dump_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_physmem"))
		rte_dump_physmem_layout(stdout);
	else if (!strcmp(res->dump, "dump_memzone"))
		rte_memzone_dump(stdout);
	else if (!strcmp(res->dump, "dump_struct_sizes"))
		dump_struct_sizes();
	else if (!strcmp(res->dump, "dump_ring"))
		rte_ring_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_mempool"))
		rte_mempool_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_devargs"))
		rte_eal_devargs_dump(stdout);
}

cmdline_parse_token_string_t cmd_dump_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_result, dump,
		"dump_physmem#"
		"dump_memzone#"
		"dump_struct_sizes#"
		"dump_ring#"
		"dump_mempool#"
		"dump_devargs");

cmdline_parse_inst_t cmd_dump = {
	.f = cmd_dump_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump status",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_dump,
		NULL,
	},
};

/* ******************************************************************************** */

struct cmd_dump_one_result {
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t name;
};

static void cmd_dump_one_parsed(void *parsed_result, struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_dump_one_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_ring")) {
		struct rte_ring *r;
		r = rte_ring_lookup(res->name);
		if (r == NULL) {
			cmdline_printf(cl, "Cannot find ring\n");
			return;
		}
		rte_ring_dump(stdout, r);
	} else if (!strcmp(res->dump, "dump_mempool")) {
		struct rte_mempool *mp;
		mp = rte_mempool_lookup(res->name);
		if (mp == NULL) {
			cmdline_printf(cl, "Cannot find mempool\n");
			return;
		}
		rte_mempool_dump(stdout, mp);
	}
}

cmdline_parse_token_string_t cmd_dump_one_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, dump,
				 "dump_ring#dump_mempool");

cmdline_parse_token_string_t cmd_dump_one_name =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, name, NULL);

cmdline_parse_inst_t cmd_dump_one = {
	.f = cmd_dump_one_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump one ring/mempool: dump_ring|dump_mempool <name>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_one_dump,
		(void *)&cmd_dump_one_name,
		NULL,
	},
};

/* *** Add/Del syn filter *** */
struct cmd_syn_filter_result {
	cmdline_fixed_string_t filter;
	uint8_t port_id;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t priority;
	cmdline_fixed_string_t high;
	cmdline_fixed_string_t queue;
	uint16_t queue_id;
};

static void
cmd_syn_filter_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct cmd_syn_filter_result *res = parsed_result;
	struct rte_eth_syn_filter syn_filter;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id,
					RTE_ETH_FILTER_SYN);
	if (ret < 0) {
		printf("syn filter is not supported on port %u.\n",
				res->port_id);
		return;
	}

	memset(&syn_filter, 0, sizeof(syn_filter));

	if (!strcmp(res->ops, "add")) {
		if (!strcmp(res->high, "high"))
			syn_filter.hig_pri = 1;
		else
			syn_filter.hig_pri = 0;

		syn_filter.queue = res->queue_id;
		ret = rte_eth_dev_filter_ctrl(res->port_id,
						RTE_ETH_FILTER_SYN,
						RTE_ETH_FILTER_ADD,
						&syn_filter);
	} else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
						RTE_ETH_FILTER_SYN,
						RTE_ETH_FILTER_DELETE,
						&syn_filter);

	if (ret < 0)
		printf("syn filter programming error: (%s)\n",
				strerror(-ret));
}

cmdline_parse_token_string_t cmd_syn_filter_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_syn_filter_result,
	filter, "syn_filter");
cmdline_parse_token_num_t cmd_syn_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_syn_filter_result,
	port_id, UINT8);
cmdline_parse_token_string_t cmd_syn_filter_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_syn_filter_result,
	ops, "add#del");
cmdline_parse_token_string_t cmd_syn_filter_priority =
	TOKEN_STRING_INITIALIZER(struct cmd_syn_filter_result,
				priority, "priority");
cmdline_parse_token_string_t cmd_syn_filter_high =
	TOKEN_STRING_INITIALIZER(struct cmd_syn_filter_result,
				high, "high#low");
cmdline_parse_token_string_t cmd_syn_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_syn_filter_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_syn_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_syn_filter_result,
				queue_id, UINT16);

cmdline_parse_inst_t cmd_syn_filter = {
	.f = cmd_syn_filter_parsed,
	.data = NULL,
	.help_str = "add/delete syn filter",
	.tokens = {
		(void *)&cmd_syn_filter_filter,
		(void *)&cmd_syn_filter_port_id,
		(void *)&cmd_syn_filter_ops,
		(void *)&cmd_syn_filter_priority,
		(void *)&cmd_syn_filter_high,
		(void *)&cmd_syn_filter_queue,
		(void *)&cmd_syn_filter_queue_id,
		NULL,
	},
};

/* *** ADD/REMOVE A 2tuple FILTER *** */
struct cmd_2tuple_filter_result {
	cmdline_fixed_string_t filter;
	uint8_t  port_id;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t dst_port;
	uint16_t dst_port_value;
	cmdline_fixed_string_t protocol;
	uint8_t protocol_value;
	cmdline_fixed_string_t mask;
	uint8_t  mask_value;
	cmdline_fixed_string_t tcp_flags;
	uint8_t tcp_flags_value;
	cmdline_fixed_string_t priority;
	uint8_t  priority_value;
	cmdline_fixed_string_t queue;
	uint16_t  queue_id;
};

static void
cmd_2tuple_filter_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct rte_eth_ntuple_filter filter;
	struct cmd_2tuple_filter_result *res = parsed_result;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id, RTE_ETH_FILTER_NTUPLE);
	if (ret < 0) {
		printf("ntuple filter is not supported on port %u.\n",
			res->port_id);
		return;
	}

	memset(&filter, 0, sizeof(struct rte_eth_ntuple_filter));

	filter.flags = RTE_2TUPLE_FLAGS;
	filter.dst_port_mask = (res->mask_value & 0x02) ? UINT16_MAX : 0;
	filter.proto_mask = (res->mask_value & 0x01) ? UINT8_MAX : 0;
	filter.proto = res->protocol_value;
	filter.priority = res->priority_value;
	if (res->tcp_flags_value != 0 && filter.proto != IPPROTO_TCP) {
		printf("nonzero tcp_flags is only meaningful"
			" when protocol is TCP.\n");
		return;
	}
	if (res->tcp_flags_value > TCP_FLAG_ALL) {
		printf("invalid TCP flags.\n");
		return;
	}

	if (res->tcp_flags_value != 0) {
		filter.flags |= RTE_NTUPLE_FLAGS_TCP_FLAG;
		filter.tcp_flags = res->tcp_flags_value;
	}

	/* need convert to big endian. */
	filter.dst_port = rte_cpu_to_be_16(res->dst_port_value);
	filter.queue = res->queue_id;

	if (!strcmp(res->ops, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_NTUPLE,
				RTE_ETH_FILTER_ADD,
				&filter);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_NTUPLE,
				RTE_ETH_FILTER_DELETE,
				&filter);
	if (ret < 0)
		printf("2tuple filter programming error: (%s)\n",
			strerror(-ret));

}

cmdline_parse_token_string_t cmd_2tuple_filter_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				 filter, "2tuple_filter");
cmdline_parse_token_num_t cmd_2tuple_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_2tuple_filter_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				 ops, "add#del");
cmdline_parse_token_string_t cmd_2tuple_filter_dst_port =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				dst_port, "dst_port");
cmdline_parse_token_num_t cmd_2tuple_filter_dst_port_value =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				dst_port_value, UINT16);
cmdline_parse_token_string_t cmd_2tuple_filter_protocol =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				protocol, "protocol");
cmdline_parse_token_num_t cmd_2tuple_filter_protocol_value =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				protocol_value, UINT8);
cmdline_parse_token_string_t cmd_2tuple_filter_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				mask, "mask");
cmdline_parse_token_num_t cmd_2tuple_filter_mask_value =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				mask_value, INT8);
cmdline_parse_token_string_t cmd_2tuple_filter_tcp_flags =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				tcp_flags, "tcp_flags");
cmdline_parse_token_num_t cmd_2tuple_filter_tcp_flags_value =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				tcp_flags_value, UINT8);
cmdline_parse_token_string_t cmd_2tuple_filter_priority =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				priority, "priority");
cmdline_parse_token_num_t cmd_2tuple_filter_priority_value =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				priority_value, UINT8);
cmdline_parse_token_string_t cmd_2tuple_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_2tuple_filter_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_2tuple_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_2tuple_filter_result,
				queue_id, UINT16);

cmdline_parse_inst_t cmd_2tuple_filter = {
	.f = cmd_2tuple_filter_parsed,
	.data = NULL,
	.help_str = "add a 2tuple filter",
	.tokens = {
		(void *)&cmd_2tuple_filter_filter,
		(void *)&cmd_2tuple_filter_port_id,
		(void *)&cmd_2tuple_filter_ops,
		(void *)&cmd_2tuple_filter_dst_port,
		(void *)&cmd_2tuple_filter_dst_port_value,
		(void *)&cmd_2tuple_filter_protocol,
		(void *)&cmd_2tuple_filter_protocol_value,
		(void *)&cmd_2tuple_filter_mask,
		(void *)&cmd_2tuple_filter_mask_value,
		(void *)&cmd_2tuple_filter_tcp_flags,
		(void *)&cmd_2tuple_filter_tcp_flags_value,
		(void *)&cmd_2tuple_filter_priority,
		(void *)&cmd_2tuple_filter_priority_value,
		(void *)&cmd_2tuple_filter_queue,
		(void *)&cmd_2tuple_filter_queue_id,
		NULL,
	},
};

/* *** ADD/REMOVE A 5tuple FILTER *** */
struct cmd_5tuple_filter_result {
	cmdline_fixed_string_t filter;
	uint8_t  port_id;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t dst_ip;
	cmdline_ipaddr_t dst_ip_value;
	cmdline_fixed_string_t src_ip;
	cmdline_ipaddr_t src_ip_value;
	cmdline_fixed_string_t dst_port;
	uint16_t dst_port_value;
	cmdline_fixed_string_t src_port;
	uint16_t src_port_value;
	cmdline_fixed_string_t protocol;
	uint8_t protocol_value;
	cmdline_fixed_string_t mask;
	uint8_t  mask_value;
	cmdline_fixed_string_t tcp_flags;
	uint8_t tcp_flags_value;
	cmdline_fixed_string_t priority;
	uint8_t  priority_value;
	cmdline_fixed_string_t queue;
	uint16_t  queue_id;
};

static void
cmd_5tuple_filter_parsed(void *parsed_result,
			__attribute__((unused)) struct cmdline *cl,
			__attribute__((unused)) void *data)
{
	struct rte_eth_ntuple_filter filter;
	struct cmd_5tuple_filter_result *res = parsed_result;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id, RTE_ETH_FILTER_NTUPLE);
	if (ret < 0) {
		printf("ntuple filter is not supported on port %u.\n",
			res->port_id);
		return;
	}

	memset(&filter, 0, sizeof(struct rte_eth_ntuple_filter));

	filter.flags = RTE_5TUPLE_FLAGS;
	filter.dst_ip_mask = (res->mask_value & 0x10) ? UINT32_MAX : 0;
	filter.src_ip_mask = (res->mask_value & 0x08) ? UINT32_MAX : 0;
	filter.dst_port_mask = (res->mask_value & 0x04) ? UINT16_MAX : 0;
	filter.src_port_mask = (res->mask_value & 0x02) ? UINT16_MAX : 0;
	filter.proto_mask = (res->mask_value & 0x01) ? UINT8_MAX : 0;
	filter.proto = res->protocol_value;
	filter.priority = res->priority_value;
	if (res->tcp_flags_value != 0 && filter.proto != IPPROTO_TCP) {
		printf("nonzero tcp_flags is only meaningful"
			" when protocol is TCP.\n");
		return;
	}
	if (res->tcp_flags_value > TCP_FLAG_ALL) {
		printf("invalid TCP flags.\n");
		return;
	}

	if (res->tcp_flags_value != 0) {
		filter.flags |= RTE_NTUPLE_FLAGS_TCP_FLAG;
		filter.tcp_flags = res->tcp_flags_value;
	}

	if (res->dst_ip_value.family == AF_INET)
		/* no need to convert, already big endian. */
		filter.dst_ip = res->dst_ip_value.addr.ipv4.s_addr;
	else {
		if (filter.dst_ip_mask == 0) {
			printf("can not support ipv6 involved compare.\n");
			return;
		}
		filter.dst_ip = 0;
	}

	if (res->src_ip_value.family == AF_INET)
		/* no need to convert, already big endian. */
		filter.src_ip = res->src_ip_value.addr.ipv4.s_addr;
	else {
		if (filter.src_ip_mask == 0) {
			printf("can not support ipv6 involved compare.\n");
			return;
		}
		filter.src_ip = 0;
	}
	/* need convert to big endian. */
	filter.dst_port = rte_cpu_to_be_16(res->dst_port_value);
	filter.src_port = rte_cpu_to_be_16(res->src_port_value);
	filter.queue = res->queue_id;

	if (!strcmp(res->ops, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_NTUPLE,
				RTE_ETH_FILTER_ADD,
				&filter);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_NTUPLE,
				RTE_ETH_FILTER_DELETE,
				&filter);
	if (ret < 0)
		printf("5tuple filter programming error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_token_string_t cmd_5tuple_filter_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				 filter, "5tuple_filter");
cmdline_parse_token_num_t cmd_5tuple_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_5tuple_filter_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				 ops, "add#del");
cmdline_parse_token_string_t cmd_5tuple_filter_dst_ip =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				dst_ip, "dst_ip");
cmdline_parse_token_ipaddr_t cmd_5tuple_filter_dst_ip_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_5tuple_filter_result,
				dst_ip_value);
cmdline_parse_token_string_t cmd_5tuple_filter_src_ip =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				src_ip, "src_ip");
cmdline_parse_token_ipaddr_t cmd_5tuple_filter_src_ip_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_5tuple_filter_result,
				src_ip_value);
cmdline_parse_token_string_t cmd_5tuple_filter_dst_port =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				dst_port, "dst_port");
cmdline_parse_token_num_t cmd_5tuple_filter_dst_port_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				dst_port_value, UINT16);
cmdline_parse_token_string_t cmd_5tuple_filter_src_port =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				src_port, "src_port");
cmdline_parse_token_num_t cmd_5tuple_filter_src_port_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				src_port_value, UINT16);
cmdline_parse_token_string_t cmd_5tuple_filter_protocol =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				protocol, "protocol");
cmdline_parse_token_num_t cmd_5tuple_filter_protocol_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				protocol_value, UINT8);
cmdline_parse_token_string_t cmd_5tuple_filter_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				mask, "mask");
cmdline_parse_token_num_t cmd_5tuple_filter_mask_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				mask_value, INT8);
cmdline_parse_token_string_t cmd_5tuple_filter_tcp_flags =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				tcp_flags, "tcp_flags");
cmdline_parse_token_num_t cmd_5tuple_filter_tcp_flags_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				tcp_flags_value, UINT8);
cmdline_parse_token_string_t cmd_5tuple_filter_priority =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				priority, "priority");
cmdline_parse_token_num_t cmd_5tuple_filter_priority_value =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				priority_value, UINT8);
cmdline_parse_token_string_t cmd_5tuple_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_5tuple_filter_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_5tuple_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_5tuple_filter_result,
				queue_id, UINT16);

cmdline_parse_inst_t cmd_5tuple_filter = {
	.f = cmd_5tuple_filter_parsed,
	.data = NULL,
	.help_str = "add/del a 5tuple filter",
	.tokens = {
		(void *)&cmd_5tuple_filter_filter,
		(void *)&cmd_5tuple_filter_port_id,
		(void *)&cmd_5tuple_filter_ops,
		(void *)&cmd_5tuple_filter_dst_ip,
		(void *)&cmd_5tuple_filter_dst_ip_value,
		(void *)&cmd_5tuple_filter_src_ip,
		(void *)&cmd_5tuple_filter_src_ip_value,
		(void *)&cmd_5tuple_filter_dst_port,
		(void *)&cmd_5tuple_filter_dst_port_value,
		(void *)&cmd_5tuple_filter_src_port,
		(void *)&cmd_5tuple_filter_src_port_value,
		(void *)&cmd_5tuple_filter_protocol,
		(void *)&cmd_5tuple_filter_protocol_value,
		(void *)&cmd_5tuple_filter_mask,
		(void *)&cmd_5tuple_filter_mask_value,
		(void *)&cmd_5tuple_filter_tcp_flags,
		(void *)&cmd_5tuple_filter_tcp_flags_value,
		(void *)&cmd_5tuple_filter_priority,
		(void *)&cmd_5tuple_filter_priority_value,
		(void *)&cmd_5tuple_filter_queue,
		(void *)&cmd_5tuple_filter_queue_id,
		NULL,
	},
};

/* *** ADD/REMOVE A flex FILTER *** */
struct cmd_flex_filter_result {
	cmdline_fixed_string_t filter;
	cmdline_fixed_string_t ops;
	uint8_t port_id;
	cmdline_fixed_string_t len;
	uint8_t len_value;
	cmdline_fixed_string_t bytes;
	cmdline_fixed_string_t bytes_value;
	cmdline_fixed_string_t mask;
	cmdline_fixed_string_t mask_value;
	cmdline_fixed_string_t priority;
	uint8_t priority_value;
	cmdline_fixed_string_t queue;
	uint16_t queue_id;
};

static int xdigit2val(unsigned char c)
{
	int val;
	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;
	return val;
}

static void
cmd_flex_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	int ret = 0;
	struct rte_eth_flex_filter filter;
	struct cmd_flex_filter_result *res = parsed_result;
	char *bytes_ptr, *mask_ptr;
	uint16_t len, i, j = 0;
	char c;
	int val;
	uint8_t byte = 0;

	if (res->len_value > RTE_FLEX_FILTER_MAXLEN) {
		printf("the len exceed the max length 128\n");
		return;
	}
	memset(&filter, 0, sizeof(struct rte_eth_flex_filter));
	filter.len = res->len_value;
	filter.priority = res->priority_value;
	filter.queue = res->queue_id;
	bytes_ptr = res->bytes_value;
	mask_ptr = res->mask_value;

	 /* translate bytes string to array. */
	if (bytes_ptr[0] == '0' && ((bytes_ptr[1] == 'x') ||
		(bytes_ptr[1] == 'X')))
		bytes_ptr += 2;
	len = strnlen(bytes_ptr, res->len_value * 2);
	if (len == 0 || (len % 8 != 0)) {
		printf("please check len and bytes input\n");
		return;
	}
	for (i = 0; i < len; i++) {
		c = bytes_ptr[i];
		if (isxdigit(c) == 0) {
			/* invalid characters. */
			printf("invalid input\n");
			return;
		}
		val = xdigit2val(c);
		if (i % 2) {
			byte |= val;
			filter.bytes[j] = byte;
			printf("bytes[%d]:%02x ", j, filter.bytes[j]);
			j++;
			byte = 0;
		} else
			byte |= val << 4;
	}
	printf("\n");
	 /* translate mask string to uint8_t array. */
	if (mask_ptr[0] == '0' && ((mask_ptr[1] == 'x') ||
		(mask_ptr[1] == 'X')))
		mask_ptr += 2;
	len = strnlen(mask_ptr, (res->len_value + 3) / 4);
	if (len == 0) {
		printf("invalid input\n");
		return;
	}
	j = 0;
	byte = 0;
	for (i = 0; i < len; i++) {
		c = mask_ptr[i];
		if (isxdigit(c) == 0) {
			/* invalid characters. */
			printf("invalid input\n");
			return;
		}
		val = xdigit2val(c);
		if (i % 2) {
			byte |= val;
			filter.mask[j] = byte;
			printf("mask[%d]:%02x ", j, filter.mask[j]);
			j++;
			byte = 0;
		} else
			byte |= val << 4;
	}
	printf("\n");

	if (!strcmp(res->ops, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_FLEXIBLE,
				RTE_ETH_FILTER_ADD,
				&filter);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_FLEXIBLE,
				RTE_ETH_FILTER_DELETE,
				&filter);

	if (ret < 0)
		printf("flex filter setting error: (%s)\n", strerror(-ret));
}

cmdline_parse_token_string_t cmd_flex_filter_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				filter, "flex_filter");
cmdline_parse_token_num_t cmd_flex_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flex_filter_result,
				port_id, UINT8);
cmdline_parse_token_string_t cmd_flex_filter_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				ops, "add#del");
cmdline_parse_token_string_t cmd_flex_filter_len =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				len, "len");
cmdline_parse_token_num_t cmd_flex_filter_len_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flex_filter_result,
				len_value, UINT8);
cmdline_parse_token_string_t cmd_flex_filter_bytes =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				bytes, "bytes");
cmdline_parse_token_string_t cmd_flex_filter_bytes_value =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				bytes_value, NULL);
cmdline_parse_token_string_t cmd_flex_filter_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				mask, "mask");
cmdline_parse_token_string_t cmd_flex_filter_mask_value =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				mask_value, NULL);
cmdline_parse_token_string_t cmd_flex_filter_priority =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				priority, "priority");
cmdline_parse_token_num_t cmd_flex_filter_priority_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flex_filter_result,
				priority_value, UINT8);
cmdline_parse_token_string_t cmd_flex_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_flex_filter_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_flex_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flex_filter_result,
				queue_id, UINT16);
cmdline_parse_inst_t cmd_flex_filter = {
	.f = cmd_flex_filter_parsed,
	.data = NULL,
	.help_str = "add/del a flex filter",
	.tokens = {
		(void *)&cmd_flex_filter_filter,
		(void *)&cmd_flex_filter_port_id,
		(void *)&cmd_flex_filter_ops,
		(void *)&cmd_flex_filter_len,
		(void *)&cmd_flex_filter_len_value,
		(void *)&cmd_flex_filter_bytes,
		(void *)&cmd_flex_filter_bytes_value,
		(void *)&cmd_flex_filter_mask,
		(void *)&cmd_flex_filter_mask_value,
		(void *)&cmd_flex_filter_priority,
		(void *)&cmd_flex_filter_priority_value,
		(void *)&cmd_flex_filter_queue,
		(void *)&cmd_flex_filter_queue_id,
		NULL,
	},
};

/* *** Filters Control *** */

/* *** deal with ethertype filter *** */
struct cmd_ethertype_filter_result {
	cmdline_fixed_string_t filter;
	uint8_t port_id;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t mac;
	struct ether_addr mac_addr;
	cmdline_fixed_string_t ethertype;
	uint16_t ethertype_value;
	cmdline_fixed_string_t drop;
	cmdline_fixed_string_t queue;
	uint16_t  queue_id;
};

cmdline_parse_token_string_t cmd_ethertype_filter_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 filter, "ethertype_filter");
cmdline_parse_token_num_t cmd_ethertype_filter_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ethertype_filter_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_ethertype_filter_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 ops, "add#del");
cmdline_parse_token_string_t cmd_ethertype_filter_mac =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 mac, "mac_addr#mac_ignr");
cmdline_parse_token_etheraddr_t cmd_ethertype_filter_mac_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_ethertype_filter_result,
				     mac_addr);
cmdline_parse_token_string_t cmd_ethertype_filter_ethertype =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 ethertype, "ethertype");
cmdline_parse_token_num_t cmd_ethertype_filter_ethertype_value =
	TOKEN_NUM_INITIALIZER(struct cmd_ethertype_filter_result,
			      ethertype_value, UINT16);
cmdline_parse_token_string_t cmd_ethertype_filter_drop =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 drop, "drop#fwd");
cmdline_parse_token_string_t cmd_ethertype_filter_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_ethertype_filter_result,
				 queue, "queue");
cmdline_parse_token_num_t cmd_ethertype_filter_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ethertype_filter_result,
			      queue_id, UINT16);

static void
cmd_ethertype_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_ethertype_filter_result *res = parsed_result;
	struct rte_eth_ethertype_filter filter;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id,
			RTE_ETH_FILTER_ETHERTYPE);
	if (ret < 0) {
		printf("ethertype filter is not supported on port %u.\n",
			res->port_id);
		return;
	}

	memset(&filter, 0, sizeof(filter));
	if (!strcmp(res->mac, "mac_addr")) {
		filter.flags |= RTE_ETHTYPE_FLAGS_MAC;
		(void)rte_memcpy(&filter.mac_addr, &res->mac_addr,
			sizeof(struct ether_addr));
	}
	if (!strcmp(res->drop, "drop"))
		filter.flags |= RTE_ETHTYPE_FLAGS_DROP;
	filter.ether_type = res->ethertype_value;
	filter.queue = res->queue_id;

	if (!strcmp(res->ops, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_ETHERTYPE,
				RTE_ETH_FILTER_ADD,
				&filter);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id,
				RTE_ETH_FILTER_ETHERTYPE,
				RTE_ETH_FILTER_DELETE,
				&filter);
	if (ret < 0)
		printf("ethertype filter programming error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_inst_t cmd_ethertype_filter = {
	.f = cmd_ethertype_filter_parsed,
	.data = NULL,
	.help_str = "add or delete an ethertype filter entry",
	.tokens = {
		(void *)&cmd_ethertype_filter_filter,
		(void *)&cmd_ethertype_filter_port_id,
		(void *)&cmd_ethertype_filter_ops,
		(void *)&cmd_ethertype_filter_mac,
		(void *)&cmd_ethertype_filter_mac_addr,
		(void *)&cmd_ethertype_filter_ethertype,
		(void *)&cmd_ethertype_filter_ethertype_value,
		(void *)&cmd_ethertype_filter_drop,
		(void *)&cmd_ethertype_filter_queue,
		(void *)&cmd_ethertype_filter_queue_id,
		NULL,
	},
};

/* *** deal with flow director filter *** */
struct cmd_flow_director_result {
	cmdline_fixed_string_t flow_director_filter;
	uint8_t port_id;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t mode_value;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t flow;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t ether;
	uint16_t ether_type;
	cmdline_fixed_string_t src;
	cmdline_ipaddr_t ip_src;
	uint16_t port_src;
	cmdline_fixed_string_t dst;
	cmdline_ipaddr_t ip_dst;
	uint16_t port_dst;
	cmdline_fixed_string_t verify_tag;
	uint32_t verify_tag_value;
	cmdline_ipaddr_t tos;
	uint8_t tos_value;
	cmdline_ipaddr_t proto;
	uint8_t proto_value;
	cmdline_ipaddr_t ttl;
	uint8_t ttl_value;
	cmdline_fixed_string_t vlan;
	uint16_t vlan_value;
	cmdline_fixed_string_t flexbytes;
	cmdline_fixed_string_t flexbytes_value;
	cmdline_fixed_string_t pf_vf;
	cmdline_fixed_string_t drop;
	cmdline_fixed_string_t queue;
	uint16_t  queue_id;
	cmdline_fixed_string_t fd_id;
	uint32_t  fd_id_value;
	cmdline_fixed_string_t mac;
	struct ether_addr mac_addr;
	cmdline_fixed_string_t tunnel;
	cmdline_fixed_string_t tunnel_type;
	cmdline_fixed_string_t tunnel_id;
	uint32_t tunnel_id_value;
};

static inline int
parse_flexbytes(const char *q_arg, uint8_t *flexbytes, uint16_t max_num)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	unsigned long int_fld;
	char *str_fld[max_num];
	int i;
	unsigned size;
	int ret = -1;

	p = strchr(p0, '(');
	if (p == NULL)
		return -1;
	++p;
	p0 = strchr(p, ')');
	if (p0 == NULL)
		return -1;

	size = p0 - p;
	if (size >= sizeof(s))
		return -1;

	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, max_num, ',');
	if (ret < 0 || ret > max_num)
		return -1;
	for (i = 0; i < ret; i++) {
		errno = 0;
		int_fld = strtoul(str_fld[i], &end, 0);
		if (errno != 0 || *end != '\0' || int_fld > UINT8_MAX)
			return -1;
		flexbytes[i] = (uint8_t)int_fld;
	}
	return ret;
}

static uint16_t
str2flowtype(char *string)
{
	uint8_t i = 0;
	static const struct {
		char str[32];
		uint16_t type;
	} flowtype_str[] = {
		{"raw", RTE_ETH_FLOW_RAW},
		{"ipv4", RTE_ETH_FLOW_IPV4},
		{"ipv4-frag", RTE_ETH_FLOW_FRAG_IPV4},
		{"ipv4-tcp", RTE_ETH_FLOW_NONFRAG_IPV4_TCP},
		{"ipv4-udp", RTE_ETH_FLOW_NONFRAG_IPV4_UDP},
		{"ipv4-sctp", RTE_ETH_FLOW_NONFRAG_IPV4_SCTP},
		{"ipv4-other", RTE_ETH_FLOW_NONFRAG_IPV4_OTHER},
		{"ipv6", RTE_ETH_FLOW_IPV6},
		{"ipv6-frag", RTE_ETH_FLOW_FRAG_IPV6},
		{"ipv6-tcp", RTE_ETH_FLOW_NONFRAG_IPV6_TCP},
		{"ipv6-udp", RTE_ETH_FLOW_NONFRAG_IPV6_UDP},
		{"ipv6-sctp", RTE_ETH_FLOW_NONFRAG_IPV6_SCTP},
		{"ipv6-other", RTE_ETH_FLOW_NONFRAG_IPV6_OTHER},
		{"l2_payload", RTE_ETH_FLOW_L2_PAYLOAD},
	};

	for (i = 0; i < RTE_DIM(flowtype_str); i++) {
		if (!strcmp(flowtype_str[i].str, string))
			return flowtype_str[i].type;
	}
	return RTE_ETH_FLOW_UNKNOWN;
}

static enum rte_eth_fdir_tunnel_type
str2fdir_tunneltype(char *string)
{
	uint8_t i = 0;

	static const struct {
		char str[32];
		enum rte_eth_fdir_tunnel_type type;
	} tunneltype_str[] = {
		{"NVGRE", RTE_FDIR_TUNNEL_TYPE_NVGRE},
		{"VxLAN", RTE_FDIR_TUNNEL_TYPE_VXLAN},
	};

	for (i = 0; i < RTE_DIM(tunneltype_str); i++) {
		if (!strcmp(tunneltype_str[i].str, string))
			return tunneltype_str[i].type;
	}
	return RTE_FDIR_TUNNEL_TYPE_UNKNOWN;
}

#define IPV4_ADDR_TO_UINT(ip_addr, ip) \
do { \
	if ((ip_addr).family == AF_INET) \
		(ip) = (ip_addr).addr.ipv4.s_addr; \
	else { \
		printf("invalid parameter.\n"); \
		return; \
	} \
} while (0)

#define IPV6_ADDR_TO_ARRAY(ip_addr, ip) \
do { \
	if ((ip_addr).family == AF_INET6) \
		(void)rte_memcpy(&(ip), \
				 &((ip_addr).addr.ipv6), \
				 sizeof(struct in6_addr)); \
	else { \
		printf("invalid parameter.\n"); \
		return; \
	} \
} while (0)

static void
cmd_flow_director_filter_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_flow_director_result *res = parsed_result;
	struct rte_eth_fdir_filter entry;
	uint8_t flexbytes[RTE_ETH_FDIR_MAX_FLEXLEN];
	char *end;
	unsigned long vf_id;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id, RTE_ETH_FILTER_FDIR);
	if (ret < 0) {
		printf("flow director is not supported on port %u.\n",
			res->port_id);
		return;
	}
	memset(flexbytes, 0, sizeof(flexbytes));
	memset(&entry, 0, sizeof(struct rte_eth_fdir_filter));

	if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		if (strcmp(res->mode_value, "MAC-VLAN")) {
			printf("Please set mode to MAC-VLAN.\n");
			return;
		}
	} else if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_TUNNEL) {
		if (strcmp(res->mode_value, "Tunnel")) {
			printf("Please set mode to Tunnel.\n");
			return;
		}
	} else {
		if (strcmp(res->mode_value, "IP")) {
			printf("Please set mode to IP.\n");
			return;
		}
		entry.input.flow_type = str2flowtype(res->flow_type);
	}

	ret = parse_flexbytes(res->flexbytes_value,
					flexbytes,
					RTE_ETH_FDIR_MAX_FLEXLEN);
	if (ret < 0) {
		printf("error: Cannot parse flexbytes input.\n");
		return;
	}

	switch (entry.input.flow_type) {
	case RTE_ETH_FLOW_FRAG_IPV4:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		entry.input.flow.ip4_flow.proto = res->proto_value;
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		IPV4_ADDR_TO_UINT(res->ip_dst,
			entry.input.flow.ip4_flow.dst_ip);
		IPV4_ADDR_TO_UINT(res->ip_src,
			entry.input.flow.ip4_flow.src_ip);
		entry.input.flow.ip4_flow.tos = res->tos_value;
		entry.input.flow.ip4_flow.ttl = res->ttl_value;
		/* need convert to big endian. */
		entry.input.flow.udp4_flow.dst_port =
				rte_cpu_to_be_16(res->port_dst);
		entry.input.flow.udp4_flow.src_port =
				rte_cpu_to_be_16(res->port_src);
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
		IPV4_ADDR_TO_UINT(res->ip_dst,
			entry.input.flow.sctp4_flow.ip.dst_ip);
		IPV4_ADDR_TO_UINT(res->ip_src,
			entry.input.flow.sctp4_flow.ip.src_ip);
		entry.input.flow.ip4_flow.tos = res->tos_value;
		entry.input.flow.ip4_flow.ttl = res->ttl_value;
		/* need convert to big endian. */
		entry.input.flow.sctp4_flow.dst_port =
				rte_cpu_to_be_16(res->port_dst);
		entry.input.flow.sctp4_flow.src_port =
				rte_cpu_to_be_16(res->port_src);
		entry.input.flow.sctp4_flow.verify_tag =
				rte_cpu_to_be_32(res->verify_tag_value);
		break;
	case RTE_ETH_FLOW_FRAG_IPV6:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		entry.input.flow.ipv6_flow.proto = res->proto_value;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		IPV6_ADDR_TO_ARRAY(res->ip_dst,
			entry.input.flow.ipv6_flow.dst_ip);
		IPV6_ADDR_TO_ARRAY(res->ip_src,
			entry.input.flow.ipv6_flow.src_ip);
		entry.input.flow.ipv6_flow.tc = res->tos_value;
		entry.input.flow.ipv6_flow.hop_limits = res->ttl_value;
		/* need convert to big endian. */
		entry.input.flow.udp6_flow.dst_port =
				rte_cpu_to_be_16(res->port_dst);
		entry.input.flow.udp6_flow.src_port =
				rte_cpu_to_be_16(res->port_src);
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
		IPV6_ADDR_TO_ARRAY(res->ip_dst,
			entry.input.flow.sctp6_flow.ip.dst_ip);
		IPV6_ADDR_TO_ARRAY(res->ip_src,
			entry.input.flow.sctp6_flow.ip.src_ip);
		entry.input.flow.ipv6_flow.tc = res->tos_value;
		entry.input.flow.ipv6_flow.hop_limits = res->ttl_value;
		/* need convert to big endian. */
		entry.input.flow.sctp6_flow.dst_port =
				rte_cpu_to_be_16(res->port_dst);
		entry.input.flow.sctp6_flow.src_port =
				rte_cpu_to_be_16(res->port_src);
		entry.input.flow.sctp6_flow.verify_tag =
				rte_cpu_to_be_32(res->verify_tag_value);
		break;
	case RTE_ETH_FLOW_L2_PAYLOAD:
		entry.input.flow.l2_flow.ether_type =
			rte_cpu_to_be_16(res->ether_type);
		break;
	default:
		break;
	}

	if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_MAC_VLAN)
		(void)rte_memcpy(&entry.input.flow.mac_vlan_flow.mac_addr,
				 &res->mac_addr,
				 sizeof(struct ether_addr));

	if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_TUNNEL) {
		(void)rte_memcpy(&entry.input.flow.tunnel_flow.mac_addr,
				 &res->mac_addr,
				 sizeof(struct ether_addr));
		entry.input.flow.tunnel_flow.tunnel_type =
			str2fdir_tunneltype(res->tunnel_type);
		entry.input.flow.tunnel_flow.tunnel_id =
			rte_cpu_to_be_32(res->tunnel_id_value);
	}

	(void)rte_memcpy(entry.input.flow_ext.flexbytes,
		   flexbytes,
		   RTE_ETH_FDIR_MAX_FLEXLEN);

	entry.input.flow_ext.vlan_tci = rte_cpu_to_be_16(res->vlan_value);

	entry.action.flex_off = 0;  /*use 0 by default */
	if (!strcmp(res->drop, "drop"))
		entry.action.behavior = RTE_ETH_FDIR_REJECT;
	else
		entry.action.behavior = RTE_ETH_FDIR_ACCEPT;

	if (fdir_conf.mode !=  RTE_FDIR_MODE_PERFECT_MAC_VLAN &&
	    fdir_conf.mode !=  RTE_FDIR_MODE_PERFECT_TUNNEL) {
		if (!strcmp(res->pf_vf, "pf"))
			entry.input.flow_ext.is_vf = 0;
		else if (!strncmp(res->pf_vf, "vf", 2)) {
			struct rte_eth_dev_info dev_info;

			memset(&dev_info, 0, sizeof(dev_info));
			rte_eth_dev_info_get(res->port_id, &dev_info);
			errno = 0;
			vf_id = strtoul(res->pf_vf + 2, &end, 10);
			if (errno != 0 || *end != '\0' ||
			    vf_id >= dev_info.max_vfs) {
				printf("invalid parameter %s.\n", res->pf_vf);
				return;
			}
			entry.input.flow_ext.is_vf = 1;
			entry.input.flow_ext.dst_id = (uint16_t)vf_id;
		} else {
			printf("invalid parameter %s.\n", res->pf_vf);
			return;
		}
	}

	/* set to report FD ID by default */
	entry.action.report_status = RTE_ETH_FDIR_REPORT_ID;
	entry.action.rx_queue = res->queue_id;
	entry.soft_id = res->fd_id_value;
	if (!strcmp(res->ops, "add"))
		ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
					     RTE_ETH_FILTER_ADD, &entry);
	else if (!strcmp(res->ops, "del"))
		ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
					     RTE_ETH_FILTER_DELETE, &entry);
	else
		ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
					     RTE_ETH_FILTER_UPDATE, &entry);
	if (ret < 0)
		printf("flow director programming error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_token_string_t cmd_flow_director_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 flow_director_filter, "flow_director_filter");
cmdline_parse_token_num_t cmd_flow_director_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_flow_director_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 ops, "add#del#update");
cmdline_parse_token_string_t cmd_flow_director_flow =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 flow, "flow");
cmdline_parse_token_string_t cmd_flow_director_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		flow_type, "ipv4-other#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#"
		"ipv6-other#ipv6-frag#ipv6-tcp#ipv6-udp#ipv6-sctp#l2_payload");
cmdline_parse_token_string_t cmd_flow_director_ether =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 ether, "ether");
cmdline_parse_token_num_t cmd_flow_director_ether_type =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      ether_type, UINT16);
cmdline_parse_token_string_t cmd_flow_director_src =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 src, "src");
cmdline_parse_token_ipaddr_t cmd_flow_director_ip_src =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_result,
				 ip_src);
cmdline_parse_token_num_t cmd_flow_director_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      port_src, UINT16);
cmdline_parse_token_string_t cmd_flow_director_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 dst, "dst");
cmdline_parse_token_ipaddr_t cmd_flow_director_ip_dst =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_result,
				 ip_dst);
cmdline_parse_token_num_t cmd_flow_director_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      port_dst, UINT16);
cmdline_parse_token_string_t cmd_flow_director_verify_tag =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				  verify_tag, "verify_tag");
cmdline_parse_token_num_t cmd_flow_director_verify_tag_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      verify_tag_value, UINT32);
cmdline_parse_token_string_t cmd_flow_director_tos =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 tos, "tos");
cmdline_parse_token_num_t cmd_flow_director_tos_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      tos_value, UINT8);
cmdline_parse_token_string_t cmd_flow_director_proto =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 proto, "proto");
cmdline_parse_token_num_t cmd_flow_director_proto_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      proto_value, UINT8);
cmdline_parse_token_string_t cmd_flow_director_ttl =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 ttl, "ttl");
cmdline_parse_token_num_t cmd_flow_director_ttl_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      ttl_value, UINT8);
cmdline_parse_token_string_t cmd_flow_director_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 vlan, "vlan");
cmdline_parse_token_num_t cmd_flow_director_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      vlan_value, UINT16);
cmdline_parse_token_string_t cmd_flow_director_flexbytes =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 flexbytes, "flexbytes");
cmdline_parse_token_string_t cmd_flow_director_flexbytes_value =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
			      flexbytes_value, NULL);
cmdline_parse_token_string_t cmd_flow_director_drop =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 drop, "drop#fwd");
cmdline_parse_token_string_t cmd_flow_director_pf_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
			      pf_vf, NULL);
cmdline_parse_token_string_t cmd_flow_director_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 queue, "queue");
cmdline_parse_token_num_t cmd_flow_director_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      queue_id, UINT16);
cmdline_parse_token_string_t cmd_flow_director_fd_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 fd_id, "fd_id");
cmdline_parse_token_num_t cmd_flow_director_fd_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      fd_id_value, UINT32);

cmdline_parse_token_string_t cmd_flow_director_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode, "mode");
cmdline_parse_token_string_t cmd_flow_director_mode_ip =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode_value, "IP");
cmdline_parse_token_string_t cmd_flow_director_mode_mac_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode_value, "MAC-VLAN");
cmdline_parse_token_string_t cmd_flow_director_mode_tunnel =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode_value, "Tunnel");
cmdline_parse_token_string_t cmd_flow_director_mac =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mac, "mac");
cmdline_parse_token_etheraddr_t cmd_flow_director_mac_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_flow_director_result,
				    mac_addr);
cmdline_parse_token_string_t cmd_flow_director_tunnel =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 tunnel, "tunnel");
cmdline_parse_token_string_t cmd_flow_director_tunnel_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 tunnel_type, "NVGRE#VxLAN");
cmdline_parse_token_string_t cmd_flow_director_tunnel_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 tunnel_id, "tunnel-id");
cmdline_parse_token_num_t cmd_flow_director_tunnel_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      tunnel_id_value, UINT32);

cmdline_parse_inst_t cmd_add_del_ip_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete an ip flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_ip,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_src,
		(void *)&cmd_flow_director_ip_src,
		(void *)&cmd_flow_director_dst,
		(void *)&cmd_flow_director_ip_dst,
		(void *)&cmd_flow_director_tos,
		(void *)&cmd_flow_director_tos_value,
		(void *)&cmd_flow_director_proto,
		(void *)&cmd_flow_director_proto_value,
		(void *)&cmd_flow_director_ttl,
		(void *)&cmd_flow_director_ttl_value,
		(void *)&cmd_flow_director_vlan,
		(void *)&cmd_flow_director_vlan_value,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_pf_vf,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_add_del_udp_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete an udp/tcp flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_ip,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_src,
		(void *)&cmd_flow_director_ip_src,
		(void *)&cmd_flow_director_port_src,
		(void *)&cmd_flow_director_dst,
		(void *)&cmd_flow_director_ip_dst,
		(void *)&cmd_flow_director_port_dst,
		(void *)&cmd_flow_director_tos,
		(void *)&cmd_flow_director_tos_value,
		(void *)&cmd_flow_director_ttl,
		(void *)&cmd_flow_director_ttl_value,
		(void *)&cmd_flow_director_vlan,
		(void *)&cmd_flow_director_vlan_value,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_pf_vf,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_add_del_sctp_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete a sctp flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_ip,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_src,
		(void *)&cmd_flow_director_ip_src,
		(void *)&cmd_flow_director_port_dst,
		(void *)&cmd_flow_director_dst,
		(void *)&cmd_flow_director_ip_dst,
		(void *)&cmd_flow_director_port_dst,
		(void *)&cmd_flow_director_verify_tag,
		(void *)&cmd_flow_director_verify_tag_value,
		(void *)&cmd_flow_director_tos,
		(void *)&cmd_flow_director_tos_value,
		(void *)&cmd_flow_director_ttl,
		(void *)&cmd_flow_director_ttl_value,
		(void *)&cmd_flow_director_vlan,
		(void *)&cmd_flow_director_vlan_value,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_pf_vf,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_add_del_l2_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete a L2 flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_ip,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_ether,
		(void *)&cmd_flow_director_ether_type,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_pf_vf,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_add_del_mac_vlan_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete a MAC VLAN flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_mac_vlan,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_mac,
		(void *)&cmd_flow_director_mac_addr,
		(void *)&cmd_flow_director_vlan,
		(void *)&cmd_flow_director_vlan_value,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_add_del_tunnel_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "add or delete a tunnel flow director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_tunnel,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_mac,
		(void *)&cmd_flow_director_mac_addr,
		(void *)&cmd_flow_director_vlan,
		(void *)&cmd_flow_director_vlan_value,
		(void *)&cmd_flow_director_tunnel,
		(void *)&cmd_flow_director_tunnel_type,
		(void *)&cmd_flow_director_tunnel_id,
		(void *)&cmd_flow_director_tunnel_id_value,
		(void *)&cmd_flow_director_flexbytes,
		(void *)&cmd_flow_director_flexbytes_value,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		NULL,
	},
};

struct cmd_flush_flow_director_result {
	cmdline_fixed_string_t flush_flow_director;
	uint8_t port_id;
};

cmdline_parse_token_string_t cmd_flush_flow_director_flush =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_flow_director_result,
				 flush_flow_director, "flush_flow_director");
cmdline_parse_token_num_t cmd_flush_flow_director_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flush_flow_director_result,
			      port_id, UINT8);

static void
cmd_flush_flow_director_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_flow_director_result *res = parsed_result;
	int ret = 0;

	ret = rte_eth_dev_filter_supported(res->port_id, RTE_ETH_FILTER_FDIR);
	if (ret < 0) {
		printf("flow director is not supported on port %u.\n",
			res->port_id);
		return;
	}

	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
			RTE_ETH_FILTER_FLUSH, NULL);
	if (ret < 0)
		printf("flow director table flushing error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_inst_t cmd_flush_flow_director = {
	.f = cmd_flush_flow_director_parsed,
	.data = NULL,
	.help_str = "flush all flow director entries of a device on NIC",
	.tokens = {
		(void *)&cmd_flush_flow_director_flush,
		(void *)&cmd_flush_flow_director_port_id,
		NULL,
	},
};

/* *** deal with flow director mask *** */
struct cmd_flow_director_mask_result {
	cmdline_fixed_string_t flow_director_mask;
	uint8_t port_id;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t mode_value;
	cmdline_fixed_string_t vlan;
	uint16_t vlan_mask;
	cmdline_fixed_string_t src_mask;
	cmdline_ipaddr_t ipv4_src;
	cmdline_ipaddr_t ipv6_src;
	uint16_t port_src;
	cmdline_fixed_string_t dst_mask;
	cmdline_ipaddr_t ipv4_dst;
	cmdline_ipaddr_t ipv6_dst;
	uint16_t port_dst;
	cmdline_fixed_string_t mac;
	uint8_t mac_addr_byte_mask;
	cmdline_fixed_string_t tunnel_id;
	uint32_t tunnel_id_mask;
	cmdline_fixed_string_t tunnel_type;
	uint8_t tunnel_type_mask;
};

static void
cmd_flow_director_mask_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_flow_director_mask_result *res = parsed_result;
	struct rte_eth_fdir_masks *mask;
	struct rte_port *port;

	if (res->port_id > nb_ports) {
		printf("Invalid port, range is [0, %d]\n", nb_ports - 1);
		return;
	}

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		printf("Please stop port %d first\n", res->port_id);
		return;
	}

	mask = &port->dev_conf.fdir_conf.mask;

	if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		if (strcmp(res->mode_value, "MAC-VLAN")) {
			printf("Please set mode to MAC-VLAN.\n");
			return;
		}

		mask->vlan_tci_mask = rte_cpu_to_be_16(res->vlan_mask);
	} else if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_TUNNEL) {
		if (strcmp(res->mode_value, "Tunnel")) {
			printf("Please set mode to Tunnel.\n");
			return;
		}

		mask->vlan_tci_mask = rte_cpu_to_be_16(res->vlan_mask);
		mask->mac_addr_byte_mask = res->mac_addr_byte_mask;
		mask->tunnel_id_mask = rte_cpu_to_be_32(res->tunnel_id_mask);
		mask->tunnel_type_mask = res->tunnel_type_mask;
	} else {
		if (strcmp(res->mode_value, "IP")) {
			printf("Please set mode to IP.\n");
			return;
		}

		mask->vlan_tci_mask = rte_cpu_to_be_16(res->vlan_mask);
		IPV4_ADDR_TO_UINT(res->ipv4_src, mask->ipv4_mask.src_ip);
		IPV4_ADDR_TO_UINT(res->ipv4_dst, mask->ipv4_mask.dst_ip);
		IPV6_ADDR_TO_ARRAY(res->ipv6_src, mask->ipv6_mask.src_ip);
		IPV6_ADDR_TO_ARRAY(res->ipv6_dst, mask->ipv6_mask.dst_ip);
		mask->src_port_mask = rte_cpu_to_be_16(res->port_src);
		mask->dst_port_mask = rte_cpu_to_be_16(res->port_dst);
	}

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_flow_director_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 flow_director_mask, "flow_director_mask");
cmdline_parse_token_num_t cmd_flow_director_mask_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_flow_director_mask_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 vlan, "vlan");
cmdline_parse_token_num_t cmd_flow_director_mask_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      vlan_mask, UINT16);
cmdline_parse_token_string_t cmd_flow_director_mask_src =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 src_mask, "src_mask");
cmdline_parse_token_ipaddr_t cmd_flow_director_mask_ipv4_src =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_mask_result,
				 ipv4_src);
cmdline_parse_token_ipaddr_t cmd_flow_director_mask_ipv6_src =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_mask_result,
				 ipv6_src);
cmdline_parse_token_num_t cmd_flow_director_mask_port_src =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      port_src, UINT16);
cmdline_parse_token_string_t cmd_flow_director_mask_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 dst_mask, "dst_mask");
cmdline_parse_token_ipaddr_t cmd_flow_director_mask_ipv4_dst =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_mask_result,
				 ipv4_dst);
cmdline_parse_token_ipaddr_t cmd_flow_director_mask_ipv6_dst =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_director_mask_result,
				 ipv6_dst);
cmdline_parse_token_num_t cmd_flow_director_mask_port_dst =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      port_dst, UINT16);

cmdline_parse_token_string_t cmd_flow_director_mask_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 mode, "mode");
cmdline_parse_token_string_t cmd_flow_director_mask_mode_ip =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 mode_value, "IP");
cmdline_parse_token_string_t cmd_flow_director_mask_mode_mac_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 mode_value, "MAC-VLAN");
cmdline_parse_token_string_t cmd_flow_director_mask_mode_tunnel =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 mode_value, "Tunnel");
cmdline_parse_token_string_t cmd_flow_director_mask_mac =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 mac, "mac");
cmdline_parse_token_num_t cmd_flow_director_mask_mac_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      mac_addr_byte_mask, UINT8);
cmdline_parse_token_string_t cmd_flow_director_mask_tunnel_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 tunnel_type, "tunnel-type");
cmdline_parse_token_num_t cmd_flow_director_mask_tunnel_type_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      tunnel_type_mask, UINT8);
cmdline_parse_token_string_t cmd_flow_director_mask_tunnel_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 tunnel_id, "tunnel-id");
cmdline_parse_token_num_t cmd_flow_director_mask_tunnel_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      tunnel_id_mask, UINT32);

cmdline_parse_inst_t cmd_set_flow_director_ip_mask = {
	.f = cmd_flow_director_mask_parsed,
	.data = NULL,
	.help_str = "set IP mode flow director's mask on NIC",
	.tokens = {
		(void *)&cmd_flow_director_mask,
		(void *)&cmd_flow_director_mask_port_id,
		(void *)&cmd_flow_director_mask_mode,
		(void *)&cmd_flow_director_mask_mode_ip,
		(void *)&cmd_flow_director_mask_vlan,
		(void *)&cmd_flow_director_mask_vlan_value,
		(void *)&cmd_flow_director_mask_src,
		(void *)&cmd_flow_director_mask_ipv4_src,
		(void *)&cmd_flow_director_mask_ipv6_src,
		(void *)&cmd_flow_director_mask_port_src,
		(void *)&cmd_flow_director_mask_dst,
		(void *)&cmd_flow_director_mask_ipv4_dst,
		(void *)&cmd_flow_director_mask_ipv6_dst,
		(void *)&cmd_flow_director_mask_port_dst,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_flow_director_mac_vlan_mask = {
	.f = cmd_flow_director_mask_parsed,
	.data = NULL,
	.help_str = "set MAC VLAN mode flow director's mask on NIC",
	.tokens = {
		(void *)&cmd_flow_director_mask,
		(void *)&cmd_flow_director_mask_port_id,
		(void *)&cmd_flow_director_mask_mode,
		(void *)&cmd_flow_director_mask_mode_mac_vlan,
		(void *)&cmd_flow_director_mask_vlan,
		(void *)&cmd_flow_director_mask_vlan_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_flow_director_tunnel_mask = {
	.f = cmd_flow_director_mask_parsed,
	.data = NULL,
	.help_str = "set tunnel mode flow director's mask on NIC",
	.tokens = {
		(void *)&cmd_flow_director_mask,
		(void *)&cmd_flow_director_mask_port_id,
		(void *)&cmd_flow_director_mask_mode,
		(void *)&cmd_flow_director_mask_mode_tunnel,
		(void *)&cmd_flow_director_mask_vlan,
		(void *)&cmd_flow_director_mask_vlan_value,
		(void *)&cmd_flow_director_mask_mac,
		(void *)&cmd_flow_director_mask_mac_value,
		(void *)&cmd_flow_director_mask_tunnel_type,
		(void *)&cmd_flow_director_mask_tunnel_type_value,
		(void *)&cmd_flow_director_mask_tunnel_id,
		(void *)&cmd_flow_director_mask_tunnel_id_value,
		NULL,
	},
};

/* *** deal with flow director mask on flexible payload *** */
struct cmd_flow_director_flex_mask_result {
	cmdline_fixed_string_t flow_director_flexmask;
	uint8_t port_id;
	cmdline_fixed_string_t flow;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t mask;
};

static void
cmd_flow_director_flex_mask_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_flow_director_flex_mask_result *res = parsed_result;
	struct rte_eth_fdir_info fdir_info;
	struct rte_eth_fdir_flex_mask flex_mask;
	struct rte_port *port;
	uint32_t flow_type_mask;
	uint16_t i;
	int ret;

	if (res->port_id > nb_ports) {
		printf("Invalid port, range is [0, %d]\n", nb_ports - 1);
		return;
	}

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		printf("Please stop port %d first\n", res->port_id);
		return;
	}

	memset(&flex_mask, 0, sizeof(struct rte_eth_fdir_flex_mask));
	ret = parse_flexbytes(res->mask,
			flex_mask.mask,
			RTE_ETH_FDIR_MAX_FLEXLEN);
	if (ret < 0) {
		printf("error: Cannot parse mask input.\n");
		return;
	}

	memset(&fdir_info, 0, sizeof(fdir_info));
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
				RTE_ETH_FILTER_INFO, &fdir_info);
	if (ret < 0) {
		printf("Cannot get FDir filter info\n");
		return;
	}

	if (!strcmp(res->flow_type, "none")) {
		/* means don't specify the flow type */
		flex_mask.flow_type = RTE_ETH_FLOW_UNKNOWN;
		for (i = 0; i < RTE_ETH_FLOW_MAX; i++)
			memset(&port->dev_conf.fdir_conf.flex_conf.flex_mask[i],
			       0, sizeof(struct rte_eth_fdir_flex_mask));
		port->dev_conf.fdir_conf.flex_conf.nb_flexmasks = 1;
		(void)rte_memcpy(&port->dev_conf.fdir_conf.flex_conf.flex_mask[0],
				 &flex_mask,
				 sizeof(struct rte_eth_fdir_flex_mask));
		cmd_reconfig_device_queue(res->port_id, 1, 1);
		return;
	}
	flow_type_mask = fdir_info.flow_types_mask[0];
	if (!strcmp(res->flow_type, "all")) {
		if (!flow_type_mask) {
			printf("No flow type supported\n");
			return;
		}
		for (i = RTE_ETH_FLOW_UNKNOWN; i < RTE_ETH_FLOW_MAX; i++) {
			if (flow_type_mask & (1 << i)) {
				flex_mask.flow_type = i;
				fdir_set_flex_mask(res->port_id, &flex_mask);
			}
		}
		cmd_reconfig_device_queue(res->port_id, 1, 1);
		return;
	}
	flex_mask.flow_type = str2flowtype(res->flow_type);
	if (!(flow_type_mask & (1 << flex_mask.flow_type))) {
		printf("Flow type %s not supported on port %d\n",
				res->flow_type, res->port_id);
		return;
	}
	fdir_set_flex_mask(res->port_id, &flex_mask);
	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_flow_director_flexmask =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flex_mask_result,
				 flow_director_flexmask,
				 "flow_director_flex_mask");
cmdline_parse_token_num_t cmd_flow_director_flexmask_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_flex_mask_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_flow_director_flexmask_flow =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flex_mask_result,
				 flow, "flow");
cmdline_parse_token_string_t cmd_flow_director_flexmask_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flex_mask_result,
		flow_type, "none#ipv4-other#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#"
		"ipv6-other#ipv6-frag#ipv6-tcp#ipv6-udp#ipv6-sctp#l2_payload#all");
cmdline_parse_token_string_t cmd_flow_director_flexmask_mask =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flex_mask_result,
				 mask, NULL);

cmdline_parse_inst_t cmd_set_flow_director_flex_mask = {
	.f = cmd_flow_director_flex_mask_parsed,
	.data = NULL,
	.help_str = "set flow director's flex mask on NIC",
	.tokens = {
		(void *)&cmd_flow_director_flexmask,
		(void *)&cmd_flow_director_flexmask_port_id,
		(void *)&cmd_flow_director_flexmask_flow,
		(void *)&cmd_flow_director_flexmask_flow_type,
		(void *)&cmd_flow_director_flexmask_mask,
		NULL,
	},
};

/* *** deal with flow director flexible payload configuration *** */
struct cmd_flow_director_flexpayload_result {
	cmdline_fixed_string_t flow_director_flexpayload;
	uint8_t port_id;
	cmdline_fixed_string_t payload_layer;
	cmdline_fixed_string_t payload_cfg;
};

static inline int
parse_offsets(const char *q_arg, uint16_t *offsets, uint16_t max_num)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	unsigned long int_fld;
	char *str_fld[max_num];
	int i;
	unsigned size;
	int ret = -1;

	p = strchr(p0, '(');
	if (p == NULL)
		return -1;
	++p;
	p0 = strchr(p, ')');
	if (p0 == NULL)
		return -1;

	size = p0 - p;
	if (size >= sizeof(s))
		return -1;

	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, max_num, ',');
	if (ret < 0 || ret > max_num)
		return -1;
	for (i = 0; i < ret; i++) {
		errno = 0;
		int_fld = strtoul(str_fld[i], &end, 0);
		if (errno != 0 || *end != '\0' || int_fld > UINT16_MAX)
			return -1;
		offsets[i] = (uint16_t)int_fld;
	}
	return ret;
}

static void
cmd_flow_director_flxpld_parsed(void *parsed_result,
			  __attribute__((unused)) struct cmdline *cl,
			  __attribute__((unused)) void *data)
{
	struct cmd_flow_director_flexpayload_result *res = parsed_result;
	struct rte_eth_flex_payload_cfg flex_cfg;
	struct rte_port *port;
	int ret = 0;

	if (res->port_id > nb_ports) {
		printf("Invalid port, range is [0, %d]\n", nb_ports - 1);
		return;
	}

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		printf("Please stop port %d first\n", res->port_id);
		return;
	}

	memset(&flex_cfg, 0, sizeof(struct rte_eth_flex_payload_cfg));

	if (!strcmp(res->payload_layer, "raw"))
		flex_cfg.type = RTE_ETH_RAW_PAYLOAD;
	else if (!strcmp(res->payload_layer, "l2"))
		flex_cfg.type = RTE_ETH_L2_PAYLOAD;
	else if (!strcmp(res->payload_layer, "l3"))
		flex_cfg.type = RTE_ETH_L3_PAYLOAD;
	else if (!strcmp(res->payload_layer, "l4"))
		flex_cfg.type = RTE_ETH_L4_PAYLOAD;

	ret = parse_offsets(res->payload_cfg, flex_cfg.src_offset,
			    RTE_ETH_FDIR_MAX_FLEXLEN);
	if (ret < 0) {
		printf("error: Cannot parse flex payload input.\n");
		return;
	}

	fdir_set_flex_payload(res->port_id, &flex_cfg);
	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_flow_director_flexpayload =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flexpayload_result,
				 flow_director_flexpayload,
				 "flow_director_flex_payload");
cmdline_parse_token_num_t cmd_flow_director_flexpayload_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_flexpayload_result,
			      port_id, UINT8);
cmdline_parse_token_string_t cmd_flow_director_flexpayload_payload_layer =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flexpayload_result,
				 payload_layer, "raw#l2#l3#l4");
cmdline_parse_token_string_t cmd_flow_director_flexpayload_payload_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flexpayload_result,
				 payload_cfg, NULL);

cmdline_parse_inst_t cmd_set_flow_director_flex_payload = {
	.f = cmd_flow_director_flxpld_parsed,
	.data = NULL,
	.help_str = "set flow director's flex payload on NIC",
	.tokens = {
		(void *)&cmd_flow_director_flexpayload,
		(void *)&cmd_flow_director_flexpayload_port_id,
		(void *)&cmd_flow_director_flexpayload_payload_layer,
		(void *)&cmd_flow_director_flexpayload_payload_cfg,
		NULL,
	},
};

/* *** Classification Filters Control *** */
/* *** Get symmetric hash enable per port *** */
struct cmd_get_sym_hash_ena_per_port_result {
	cmdline_fixed_string_t get_sym_hash_ena_per_port;
	uint8_t port_id;
};

static void
cmd_get_sym_hash_per_port_parsed(void *parsed_result,
				 __rte_unused struct cmdline *cl,
				 __rte_unused void *data)
{
	struct cmd_get_sym_hash_ena_per_port_result *res = parsed_result;
	struct rte_eth_hash_filter_info info;
	int ret;

	if (rte_eth_dev_filter_supported(res->port_id,
				RTE_ETH_FILTER_HASH) < 0) {
		printf("RTE_ETH_FILTER_HASH not supported on port: %d\n",
							res->port_id);
		return;
	}

	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_HASH,
						RTE_ETH_FILTER_GET, &info);

	if (ret < 0) {
		printf("Cannot get symmetric hash enable per port "
					"on port %u\n", res->port_id);
		return;
	}

	printf("Symmetric hash is %s on port %u\n", info.info.enable ?
				"enabled" : "disabled", res->port_id);
}

cmdline_parse_token_string_t cmd_get_sym_hash_ena_per_port_all =
	TOKEN_STRING_INITIALIZER(struct cmd_get_sym_hash_ena_per_port_result,
		get_sym_hash_ena_per_port, "get_sym_hash_ena_per_port");
cmdline_parse_token_num_t cmd_get_sym_hash_ena_per_port_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_get_sym_hash_ena_per_port_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_get_sym_hash_ena_per_port = {
	.f = cmd_get_sym_hash_per_port_parsed,
	.data = NULL,
	.help_str = "get_sym_hash_ena_per_port port_id",
	.tokens = {
		(void *)&cmd_get_sym_hash_ena_per_port_all,
		(void *)&cmd_get_sym_hash_ena_per_port_port_id,
		NULL,
	},
};

/* *** Set symmetric hash enable per port *** */
struct cmd_set_sym_hash_ena_per_port_result {
	cmdline_fixed_string_t set_sym_hash_ena_per_port;
	cmdline_fixed_string_t enable;
	uint8_t port_id;
};

static void
cmd_set_sym_hash_per_port_parsed(void *parsed_result,
				 __rte_unused struct cmdline *cl,
				 __rte_unused void *data)
{
	struct cmd_set_sym_hash_ena_per_port_result *res = parsed_result;
	struct rte_eth_hash_filter_info info;
	int ret;

	if (rte_eth_dev_filter_supported(res->port_id,
				RTE_ETH_FILTER_HASH) < 0) {
		printf("RTE_ETH_FILTER_HASH not supported on port: %d\n",
							res->port_id);
		return;
	}

	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
	if (!strcmp(res->enable, "enable"))
		info.info.enable = 1;
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_HASH,
					RTE_ETH_FILTER_SET, &info);
	if (ret < 0) {
		printf("Cannot set symmetric hash enable per port on "
					"port %u\n", res->port_id);
		return;
	}
	printf("Symmetric hash has been set to %s on port %u\n",
					res->enable, res->port_id);
}

cmdline_parse_token_string_t cmd_set_sym_hash_ena_per_port_all =
	TOKEN_STRING_INITIALIZER(struct cmd_set_sym_hash_ena_per_port_result,
		set_sym_hash_ena_per_port, "set_sym_hash_ena_per_port");
cmdline_parse_token_num_t cmd_set_sym_hash_ena_per_port_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_sym_hash_ena_per_port_result,
		port_id, UINT8);
cmdline_parse_token_string_t cmd_set_sym_hash_ena_per_port_enable =
	TOKEN_STRING_INITIALIZER(struct cmd_set_sym_hash_ena_per_port_result,
		enable, "enable#disable");

cmdline_parse_inst_t cmd_set_sym_hash_ena_per_port = {
	.f = cmd_set_sym_hash_per_port_parsed,
	.data = NULL,
	.help_str = "set_sym_hash_ena_per_port port_id enable|disable",
	.tokens = {
		(void *)&cmd_set_sym_hash_ena_per_port_all,
		(void *)&cmd_set_sym_hash_ena_per_port_port_id,
		(void *)&cmd_set_sym_hash_ena_per_port_enable,
		NULL,
	},
};

/* Get global config of hash function */
struct cmd_get_hash_global_config_result {
	cmdline_fixed_string_t get_hash_global_config;
	uint8_t port_id;
};

static char *
flowtype_to_str(uint16_t ftype)
{
	uint16_t i;
	static struct {
		char str[16];
		uint16_t ftype;
	} ftype_table[] = {
		{"ipv4", RTE_ETH_FLOW_IPV4},
		{"ipv4-frag", RTE_ETH_FLOW_FRAG_IPV4},
		{"ipv4-tcp", RTE_ETH_FLOW_NONFRAG_IPV4_TCP},
		{"ipv4-udp", RTE_ETH_FLOW_NONFRAG_IPV4_UDP},
		{"ipv4-sctp", RTE_ETH_FLOW_NONFRAG_IPV4_SCTP},
		{"ipv4-other", RTE_ETH_FLOW_NONFRAG_IPV4_OTHER},
		{"ipv6", RTE_ETH_FLOW_IPV6},
		{"ipv6-frag", RTE_ETH_FLOW_FRAG_IPV6},
		{"ipv6-tcp", RTE_ETH_FLOW_NONFRAG_IPV6_TCP},
		{"ipv6-udp", RTE_ETH_FLOW_NONFRAG_IPV6_UDP},
		{"ipv6-sctp", RTE_ETH_FLOW_NONFRAG_IPV6_SCTP},
		{"ipv6-other", RTE_ETH_FLOW_NONFRAG_IPV6_OTHER},
		{"l2_payload", RTE_ETH_FLOW_L2_PAYLOAD},
		{"port", RTE_ETH_FLOW_PORT},
		{"vxlan", RTE_ETH_FLOW_VXLAN},
		{"geneve", RTE_ETH_FLOW_GENEVE},
		{"nvgre", RTE_ETH_FLOW_NVGRE},
	};

	for (i = 0; i < RTE_DIM(ftype_table); i++) {
		if (ftype_table[i].ftype == ftype)
			return ftype_table[i].str;
	}

	return NULL;
}

static void
cmd_get_hash_global_config_parsed(void *parsed_result,
				  __rte_unused struct cmdline *cl,
				  __rte_unused void *data)
{
	struct cmd_get_hash_global_config_result *res = parsed_result;
	struct rte_eth_hash_filter_info info;
	uint32_t idx, offset;
	uint16_t i;
	char *str;
	int ret;

	if (rte_eth_dev_filter_supported(res->port_id,
			RTE_ETH_FILTER_HASH) < 0) {
		printf("RTE_ETH_FILTER_HASH not supported on port %d\n",
							res->port_id);
		return;
	}

	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_GLOBAL_CONFIG;
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_HASH,
					RTE_ETH_FILTER_GET, &info);
	if (ret < 0) {
		printf("Cannot get hash global configurations by port %d\n",
							res->port_id);
		return;
	}

	switch (info.info.global_conf.hash_func) {
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		printf("Hash function is Toeplitz\n");
		break;
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		printf("Hash function is Simple XOR\n");
		break;
	default:
		printf("Unknown hash function\n");
		break;
	}

	for (i = 0; i < RTE_ETH_FLOW_MAX; i++) {
		idx = i / UINT32_BIT;
		offset = i % UINT32_BIT;
		if (!(info.info.global_conf.valid_bit_mask[idx] &
						(1UL << offset)))
			continue;
		str = flowtype_to_str(i);
		if (!str)
			continue;
		printf("Symmetric hash is %s globally for flow type %s "
							"by port %d\n",
			((info.info.global_conf.sym_hash_enable_mask[idx] &
			(1UL << offset)) ? "enabled" : "disabled"), str,
							res->port_id);
	}
}

cmdline_parse_token_string_t cmd_get_hash_global_config_all =
	TOKEN_STRING_INITIALIZER(struct cmd_get_hash_global_config_result,
		get_hash_global_config, "get_hash_global_config");
cmdline_parse_token_num_t cmd_get_hash_global_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_get_hash_global_config_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_get_hash_global_config = {
	.f = cmd_get_hash_global_config_parsed,
	.data = NULL,
	.help_str = "get_hash_global_config port_id",
	.tokens = {
		(void *)&cmd_get_hash_global_config_all,
		(void *)&cmd_get_hash_global_config_port_id,
		NULL,
	},
};

/* Set global config of hash function */
struct cmd_set_hash_global_config_result {
	cmdline_fixed_string_t set_hash_global_config;
	uint8_t port_id;
	cmdline_fixed_string_t hash_func;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t enable;
};

static void
cmd_set_hash_global_config_parsed(void *parsed_result,
				  __rte_unused struct cmdline *cl,
				  __rte_unused void *data)
{
	struct cmd_set_hash_global_config_result *res = parsed_result;
	struct rte_eth_hash_filter_info info;
	uint32_t ftype, idx, offset;
	int ret;

	if (rte_eth_dev_filter_supported(res->port_id,
				RTE_ETH_FILTER_HASH) < 0) {
		printf("RTE_ETH_FILTER_HASH not supported on port %d\n",
							res->port_id);
		return;
	}
	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_GLOBAL_CONFIG;
	if (!strcmp(res->hash_func, "toeplitz"))
		info.info.global_conf.hash_func =
			RTE_ETH_HASH_FUNCTION_TOEPLITZ;
	else if (!strcmp(res->hash_func, "simple_xor"))
		info.info.global_conf.hash_func =
			RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
	else if (!strcmp(res->hash_func, "default"))
		info.info.global_conf.hash_func =
			RTE_ETH_HASH_FUNCTION_DEFAULT;

	ftype = str2flowtype(res->flow_type);
	idx = ftype / (CHAR_BIT * sizeof(uint32_t));
	offset = ftype % (CHAR_BIT * sizeof(uint32_t));
	info.info.global_conf.valid_bit_mask[idx] |= (1UL << offset);
	if (!strcmp(res->enable, "enable"))
		info.info.global_conf.sym_hash_enable_mask[idx] |=
						(1UL << offset);
	ret = rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_HASH,
					RTE_ETH_FILTER_SET, &info);
	if (ret < 0)
		printf("Cannot set global hash configurations by port %d\n",
							res->port_id);
	else
		printf("Global hash configurations have been set "
			"succcessfully by port %d\n", res->port_id);
}

cmdline_parse_token_string_t cmd_set_hash_global_config_all =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_global_config_result,
		set_hash_global_config, "set_hash_global_config");
cmdline_parse_token_num_t cmd_set_hash_global_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_hash_global_config_result,
		port_id, UINT8);
cmdline_parse_token_string_t cmd_set_hash_global_config_hash_func =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_global_config_result,
		hash_func, "toeplitz#simple_xor#default");
cmdline_parse_token_string_t cmd_set_hash_global_config_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_global_config_result,
		flow_type,
		"ipv4#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#ipv4-other#ipv6#"
		"ipv6-frag#ipv6-tcp#ipv6-udp#ipv6-sctp#ipv6-other#l2_payload");
cmdline_parse_token_string_t cmd_set_hash_global_config_enable =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_global_config_result,
		enable, "enable#disable");

cmdline_parse_inst_t cmd_set_hash_global_config = {
	.f = cmd_set_hash_global_config_parsed,
	.data = NULL,
	.help_str = "set_hash_global_config port_id "
		"toeplitz|simple_xor|default "
		"ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|ipv6|"
		"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|l2_payload "
		"enable|disable",
	.tokens = {
		(void *)&cmd_set_hash_global_config_all,
		(void *)&cmd_set_hash_global_config_port_id,
		(void *)&cmd_set_hash_global_config_hash_func,
		(void *)&cmd_set_hash_global_config_flow_type,
		(void *)&cmd_set_hash_global_config_enable,
		NULL,
	},
};

/* Set hash input set */
struct cmd_set_hash_input_set_result {
	cmdline_fixed_string_t set_hash_input_set;
	uint8_t port_id;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t inset_field;
	cmdline_fixed_string_t select;
};

static enum rte_eth_input_set_field
str2inset(char *string)
{
	uint16_t i;

	static const struct {
		char str[32];
		enum rte_eth_input_set_field inset;
	} inset_table[] = {
		{"ethertype", RTE_ETH_INPUT_SET_L2_ETHERTYPE},
		{"ovlan", RTE_ETH_INPUT_SET_L2_OUTER_VLAN},
		{"ivlan", RTE_ETH_INPUT_SET_L2_INNER_VLAN},
		{"src-ipv4", RTE_ETH_INPUT_SET_L3_SRC_IP4},
		{"dst-ipv4", RTE_ETH_INPUT_SET_L3_DST_IP4},
		{"ipv4-tos", RTE_ETH_INPUT_SET_L3_IP4_TOS},
		{"ipv4-proto", RTE_ETH_INPUT_SET_L3_IP4_PROTO},
		{"ipv4-ttl", RTE_ETH_INPUT_SET_L3_IP4_TTL},
		{"src-ipv6", RTE_ETH_INPUT_SET_L3_SRC_IP6},
		{"dst-ipv6", RTE_ETH_INPUT_SET_L3_DST_IP6},
		{"ipv6-tc", RTE_ETH_INPUT_SET_L3_IP6_TC},
		{"ipv6-next-header", RTE_ETH_INPUT_SET_L3_IP6_NEXT_HEADER},
		{"ipv6-hop-limits", RTE_ETH_INPUT_SET_L3_IP6_HOP_LIMITS},
		{"udp-src-port", RTE_ETH_INPUT_SET_L4_UDP_SRC_PORT},
		{"udp-dst-port", RTE_ETH_INPUT_SET_L4_UDP_DST_PORT},
		{"tcp-src-port", RTE_ETH_INPUT_SET_L4_TCP_SRC_PORT},
		{"tcp-dst-port", RTE_ETH_INPUT_SET_L4_TCP_DST_PORT},
		{"sctp-src-port", RTE_ETH_INPUT_SET_L4_SCTP_SRC_PORT},
		{"sctp-dst-port", RTE_ETH_INPUT_SET_L4_SCTP_DST_PORT},
		{"sctp-veri-tag", RTE_ETH_INPUT_SET_L4_SCTP_VERIFICATION_TAG},
		{"udp-key", RTE_ETH_INPUT_SET_TUNNEL_L4_UDP_KEY},
		{"gre-key", RTE_ETH_INPUT_SET_TUNNEL_GRE_KEY},
		{"fld-1st", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_1ST_WORD},
		{"fld-2nd", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_2ND_WORD},
		{"fld-3rd", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_3RD_WORD},
		{"fld-4th", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_4TH_WORD},
		{"fld-5th", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_5TH_WORD},
		{"fld-6th", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_6TH_WORD},
		{"fld-7th", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_7TH_WORD},
		{"fld-8th", RTE_ETH_INPUT_SET_FLEX_PAYLOAD_8TH_WORD},
		{"none", RTE_ETH_INPUT_SET_NONE},
	};

	for (i = 0; i < RTE_DIM(inset_table); i++) {
		if (!strcmp(string, inset_table[i].str))
			return inset_table[i].inset;
	}

	return RTE_ETH_INPUT_SET_UNKNOWN;
}

static void
cmd_set_hash_input_set_parsed(void *parsed_result,
			      __rte_unused struct cmdline *cl,
			      __rte_unused void *data)
{
	struct cmd_set_hash_input_set_result *res = parsed_result;
	struct rte_eth_hash_filter_info info;

	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_INPUT_SET_SELECT;
	info.info.input_set_conf.flow_type = str2flowtype(res->flow_type);
	info.info.input_set_conf.field[0] = str2inset(res->inset_field);
	info.info.input_set_conf.inset_size = 1;
	if (!strcmp(res->select, "select"))
		info.info.input_set_conf.op = RTE_ETH_INPUT_SET_SELECT;
	else if (!strcmp(res->select, "add"))
		info.info.input_set_conf.op = RTE_ETH_INPUT_SET_ADD;
	rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_HASH,
				RTE_ETH_FILTER_SET, &info);
}

cmdline_parse_token_string_t cmd_set_hash_input_set_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_input_set_result,
		set_hash_input_set, "set_hash_input_set");
cmdline_parse_token_num_t cmd_set_hash_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_hash_input_set_result,
		port_id, UINT8);
cmdline_parse_token_string_t cmd_set_hash_input_set_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_input_set_result,
		flow_type,
		"ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#ipv4-other#"
		"ipv6-frag#ipv6-tcp#ipv6-udp#ipv6-sctp#ipv6-other#l2_payload");
cmdline_parse_token_string_t cmd_set_hash_input_set_field =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_input_set_result,
		inset_field,
		"ovlan#ivlan#src-ipv4#dst-ipv4#src-ipv6#dst-ipv6#"
		"ipv4-tos#ipv4-proto#ipv6-tc#ipv6-next-header#udp-src-port#"
		"udp-dst-port#tcp-src-port#tcp-dst-port#sctp-src-port#"
		"sctp-dst-port#sctp-veri-tag#udp-key#gre-key#fld-1st#"
		"fld-2nd#fld-3rd#fld-4th#fld-5th#fld-6th#fld-7th#"
		"fld-8th#none");
cmdline_parse_token_string_t cmd_set_hash_input_set_select =
	TOKEN_STRING_INITIALIZER(struct cmd_set_hash_input_set_result,
		select, "select#add");

cmdline_parse_inst_t cmd_set_hash_input_set = {
	.f = cmd_set_hash_input_set_parsed,
	.data = NULL,
	.help_str = "set_hash_input_set <port_id> "
	"ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|"
	"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|l2_payload "
	"ovlan|ivlan|src-ipv4|dst-ipv4|src-ipv6|dst-ipv6|ipv4-tos|ipv4-proto|"
	"ipv6-tc|ipv6-next-header|udp-src-port|udp-dst-port|tcp-src-port|"
	"tcp-dst-port|sctp-src-port|sctp-dst-port|sctp-veri-tag|udp-key|"
	"gre-key|fld-1st|fld-2nd|fld-3rd|fld-4th|fld-5th|fld-6th|"
	"fld-7th|fld-8th|none select|add",
	.tokens = {
		(void *)&cmd_set_hash_input_set_cmd,
		(void *)&cmd_set_hash_input_set_port_id,
		(void *)&cmd_set_hash_input_set_flow_type,
		(void *)&cmd_set_hash_input_set_field,
		(void *)&cmd_set_hash_input_set_select,
		NULL,
	},
};

/* Set flow director input set */
struct cmd_set_fdir_input_set_result {
	cmdline_fixed_string_t set_fdir_input_set;
	uint8_t port_id;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t inset_field;
	cmdline_fixed_string_t select;
};

static void
cmd_set_fdir_input_set_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_fdir_input_set_result *res = parsed_result;
	struct rte_eth_fdir_filter_info info;

	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_FDIR_FILTER_INPUT_SET_SELECT;
	info.info.input_set_conf.flow_type = str2flowtype(res->flow_type);
	info.info.input_set_conf.field[0] = str2inset(res->inset_field);
	info.info.input_set_conf.inset_size = 1;
	if (!strcmp(res->select, "select"))
		info.info.input_set_conf.op = RTE_ETH_INPUT_SET_SELECT;
	else if (!strcmp(res->select, "add"))
		info.info.input_set_conf.op = RTE_ETH_INPUT_SET_ADD;
	rte_eth_dev_filter_ctrl(res->port_id, RTE_ETH_FILTER_FDIR,
		RTE_ETH_FILTER_SET, &info);
}

cmdline_parse_token_string_t cmd_set_fdir_input_set_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fdir_input_set_result,
	set_fdir_input_set, "set_fdir_input_set");
cmdline_parse_token_num_t cmd_set_fdir_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_fdir_input_set_result,
	port_id, UINT8);
cmdline_parse_token_string_t cmd_set_fdir_input_set_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fdir_input_set_result,
	flow_type,
	"ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#ipv4-other#"
	"ipv6-frag#ipv6-tcp#ipv6-udp#ipv6-sctp#ipv6-other#l2_payload");
cmdline_parse_token_string_t cmd_set_fdir_input_set_field =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fdir_input_set_result,
	inset_field,
	"ivlan#ethertype#src-ipv4#dst-ipv4#src-ipv6#dst-ipv6#"
	"ipv4-tos#ipv4-proto#ipv4-ttl#ipv6-tc#ipv6-next-header#"
	"ipv6-hop-limits#udp-src-port#udp-dst-port#"
	"tcp-src-port#tcp-dst-port#sctp-src-port#sctp-dst-port#"
	"sctp-veri-tag#none");
cmdline_parse_token_string_t cmd_set_fdir_input_set_select =
	TOKEN_STRING_INITIALIZER(struct cmd_set_fdir_input_set_result,
	select, "select#add");

cmdline_parse_inst_t cmd_set_fdir_input_set = {
	.f = cmd_set_fdir_input_set_parsed,
	.data = NULL,
	.help_str = "set_fdir_input_set <port_id> "
	"ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|"
	"ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|l2_payload "
	"ivlan|ethertype|src-ipv4|dst-ipv4|src-ipv6|dst-ipv6|"
	"ipv4-tos|ipv4-proto|ipv4-ttl|ipv6-tc|ipv6-next-header|"
	"ipv6-hop-limits|udp-src-port|udp-dst-port|"
	"tcp-src-port|tcp-dst-port|sctp-src-port|sctp-dst-port|"
	"sctp-veri-tag|none select|add",
	.tokens = {
		(void *)&cmd_set_fdir_input_set_cmd,
		(void *)&cmd_set_fdir_input_set_port_id,
		(void *)&cmd_set_fdir_input_set_flow_type,
		(void *)&cmd_set_fdir_input_set_field,
		(void *)&cmd_set_fdir_input_set_select,
		NULL,
	},
};

/* *** ADD/REMOVE A MULTICAST MAC ADDRESS TO/FROM A PORT *** */
struct cmd_mcast_addr_result {
	cmdline_fixed_string_t mcast_addr_cmd;
	cmdline_fixed_string_t what;
	uint8_t port_num;
	struct ether_addr mc_addr;
};

static void cmd_mcast_addr_parsed(void *parsed_result,
		__attribute__((unused)) struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	struct cmd_mcast_addr_result *res = parsed_result;

	if (!is_multicast_ether_addr(&res->mc_addr)) {
		printf("Invalid multicast addr %02X:%02X:%02X:%02X:%02X:%02X\n",
		       res->mc_addr.addr_bytes[0], res->mc_addr.addr_bytes[1],
		       res->mc_addr.addr_bytes[2], res->mc_addr.addr_bytes[3],
		       res->mc_addr.addr_bytes[4], res->mc_addr.addr_bytes[5]);
		return;
	}
	if (strcmp(res->what, "add") == 0)
		mcast_addr_add(res->port_num, &res->mc_addr);
	else
		mcast_addr_remove(res->port_num, &res->mc_addr);
}

cmdline_parse_token_string_t cmd_mcast_addr_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_mcast_addr_result,
				 mcast_addr_cmd, "mcast_addr");
cmdline_parse_token_string_t cmd_mcast_addr_what =
	TOKEN_STRING_INITIALIZER(struct cmd_mcast_addr_result, what,
				 "add#remove");
cmdline_parse_token_num_t cmd_mcast_addr_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_mcast_addr_result, port_num, UINT8);
cmdline_parse_token_etheraddr_t cmd_mcast_addr_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_mac_addr_result, address);

cmdline_parse_inst_t cmd_mcast_addr = {
	.f = cmd_mcast_addr_parsed,
	.data = (void *)0,
	.help_str = "mcast_addr add|remove X <mcast_addr>: add/remove multicast MAC address on port X",
	.tokens = {
		(void *)&cmd_mcast_addr_cmd,
		(void *)&cmd_mcast_addr_what,
		(void *)&cmd_mcast_addr_portnum,
		(void *)&cmd_mcast_addr_addr,
		NULL,
	},
};

/* l2 tunnel config
 * only support E-tag now.
 */

/* Ether type config */
struct cmd_config_l2_tunnel_eth_type_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	cmdline_fixed_string_t all;
	uint8_t id;
	cmdline_fixed_string_t l2_tunnel;
	cmdline_fixed_string_t l2_tunnel_type;
	cmdline_fixed_string_t eth_type;
	uint16_t eth_type_val;
};

cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 port, "port");
cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 config, "config");
cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_all_str =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 all, "all");
cmdline_parse_token_num_t cmd_config_l2_tunnel_eth_type_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 id, UINT8);
cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_l2_tunnel =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 l2_tunnel, "l2-tunnel");
cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_l2_tunnel_type =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 l2_tunnel_type, "E-tag");
cmdline_parse_token_string_t cmd_config_l2_tunnel_eth_type_eth_type =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 eth_type, "ether-type");
cmdline_parse_token_num_t cmd_config_l2_tunnel_eth_type_eth_type_val =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_l2_tunnel_eth_type_result,
		 eth_type_val, UINT16);

static enum rte_eth_tunnel_type
str2fdir_l2_tunnel_type(char *string)
{
	uint32_t i = 0;

	static const struct {
		char str[32];
		enum rte_eth_tunnel_type type;
	} l2_tunnel_type_str[] = {
		{"E-tag", RTE_L2_TUNNEL_TYPE_E_TAG},
	};

	for (i = 0; i < RTE_DIM(l2_tunnel_type_str); i++) {
		if (!strcmp(l2_tunnel_type_str[i].str, string))
			return l2_tunnel_type_str[i].type;
	}
	return RTE_TUNNEL_TYPE_NONE;
}

/* ether type config for all ports */
static void
cmd_config_l2_tunnel_eth_type_all_parsed
	(void *parsed_result,
	 __attribute__((unused)) struct cmdline *cl,
	 __attribute__((unused)) void *data)
{
	struct cmd_config_l2_tunnel_eth_type_result *res = parsed_result;
	struct rte_eth_l2_tunnel_conf entry;
	portid_t pid;

	entry.l2_tunnel_type = str2fdir_l2_tunnel_type(res->l2_tunnel_type);
	entry.ether_type = res->eth_type_val;

	FOREACH_PORT(pid, ports) {
		rte_eth_dev_l2_tunnel_eth_type_conf(pid, &entry);
	}
}

cmdline_parse_inst_t cmd_config_l2_tunnel_eth_type_all = {
	.f = cmd_config_l2_tunnel_eth_type_all_parsed,
	.data = NULL,
	.help_str = "port config all l2-tunnel ether-type",
	.tokens = {
		(void *)&cmd_config_l2_tunnel_eth_type_port,
		(void *)&cmd_config_l2_tunnel_eth_type_config,
		(void *)&cmd_config_l2_tunnel_eth_type_all_str,
		(void *)&cmd_config_l2_tunnel_eth_type_l2_tunnel,
		(void *)&cmd_config_l2_tunnel_eth_type_l2_tunnel_type,
		(void *)&cmd_config_l2_tunnel_eth_type_eth_type,
		(void *)&cmd_config_l2_tunnel_eth_type_eth_type_val,
		NULL,
	},
};

/* ether type config for a specific port */
static void
cmd_config_l2_tunnel_eth_type_specific_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_l2_tunnel_eth_type_result *res =
		 parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = str2fdir_l2_tunnel_type(res->l2_tunnel_type);
	entry.ether_type = res->eth_type_val;

	rte_eth_dev_l2_tunnel_eth_type_conf(res->id, &entry);
}

cmdline_parse_inst_t cmd_config_l2_tunnel_eth_type_specific = {
	.f = cmd_config_l2_tunnel_eth_type_specific_parsed,
	.data = NULL,
	.help_str = "port config l2-tunnel ether-type",
	.tokens = {
		(void *)&cmd_config_l2_tunnel_eth_type_port,
		(void *)&cmd_config_l2_tunnel_eth_type_config,
		(void *)&cmd_config_l2_tunnel_eth_type_id,
		(void *)&cmd_config_l2_tunnel_eth_type_l2_tunnel,
		(void *)&cmd_config_l2_tunnel_eth_type_l2_tunnel_type,
		(void *)&cmd_config_l2_tunnel_eth_type_eth_type,
		(void *)&cmd_config_l2_tunnel_eth_type_eth_type_val,
		NULL,
	},
};

/* Enable/disable l2 tunnel */
struct cmd_config_l2_tunnel_en_dis_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	cmdline_fixed_string_t all;
	uint8_t id;
	cmdline_fixed_string_t l2_tunnel;
	cmdline_fixed_string_t l2_tunnel_type;
	cmdline_fixed_string_t en_dis;
};

cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 port, "port");
cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 config, "config");
cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_all_str =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 all, "all");
cmdline_parse_token_num_t cmd_config_l2_tunnel_en_dis_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 id, UINT8);
cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_l2_tunnel =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 l2_tunnel, "l2-tunnel");
cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_l2_tunnel_type =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 l2_tunnel_type, "E-tag");
cmdline_parse_token_string_t cmd_config_l2_tunnel_en_dis_en_dis =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_l2_tunnel_en_dis_result,
		 en_dis, "enable#disable");

/* enable/disable l2 tunnel for all ports */
static void
cmd_config_l2_tunnel_en_dis_all_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_l2_tunnel_en_dis_result *res = parsed_result;
	struct rte_eth_l2_tunnel_conf entry;
	portid_t pid;
	uint8_t en;

	entry.l2_tunnel_type = str2fdir_l2_tunnel_type(res->l2_tunnel_type);

	if (!strcmp("enable", res->en_dis))
		en = 1;
	else
		en = 0;

	FOREACH_PORT(pid, ports) {
		rte_eth_dev_l2_tunnel_offload_set(pid,
						  &entry,
						  ETH_L2_TUNNEL_ENABLE_MASK,
						  en);
	}
}

cmdline_parse_inst_t cmd_config_l2_tunnel_en_dis_all = {
	.f = cmd_config_l2_tunnel_en_dis_all_parsed,
	.data = NULL,
	.help_str = "port config all l2-tunnel enable/disable",
	.tokens = {
		(void *)&cmd_config_l2_tunnel_en_dis_port,
		(void *)&cmd_config_l2_tunnel_en_dis_config,
		(void *)&cmd_config_l2_tunnel_en_dis_all_str,
		(void *)&cmd_config_l2_tunnel_en_dis_l2_tunnel,
		(void *)&cmd_config_l2_tunnel_en_dis_l2_tunnel_type,
		(void *)&cmd_config_l2_tunnel_en_dis_en_dis,
		NULL,
	},
};

/* enable/disable l2 tunnel for a port */
static void
cmd_config_l2_tunnel_en_dis_specific_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_l2_tunnel_en_dis_result *res =
		parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = str2fdir_l2_tunnel_type(res->l2_tunnel_type);

	if (!strcmp("enable", res->en_dis))
		rte_eth_dev_l2_tunnel_offload_set(res->id,
						  &entry,
						  ETH_L2_TUNNEL_ENABLE_MASK,
						  1);
	else
		rte_eth_dev_l2_tunnel_offload_set(res->id,
						  &entry,
						  ETH_L2_TUNNEL_ENABLE_MASK,
						  0);
}

cmdline_parse_inst_t cmd_config_l2_tunnel_en_dis_specific = {
	.f = cmd_config_l2_tunnel_en_dis_specific_parsed,
	.data = NULL,
	.help_str = "port config l2-tunnel enable/disable",
	.tokens = {
		(void *)&cmd_config_l2_tunnel_en_dis_port,
		(void *)&cmd_config_l2_tunnel_en_dis_config,
		(void *)&cmd_config_l2_tunnel_en_dis_id,
		(void *)&cmd_config_l2_tunnel_en_dis_l2_tunnel,
		(void *)&cmd_config_l2_tunnel_en_dis_l2_tunnel_type,
		(void *)&cmd_config_l2_tunnel_en_dis_en_dis,
		NULL,
	},
};

/* E-tag configuration */

/* Common result structure for all E-tag configuration */
struct cmd_config_e_tag_result {
	cmdline_fixed_string_t e_tag;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t insertion;
	cmdline_fixed_string_t stripping;
	cmdline_fixed_string_t forwarding;
	cmdline_fixed_string_t filter;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t del;
	cmdline_fixed_string_t on;
	cmdline_fixed_string_t off;
	cmdline_fixed_string_t on_off;
	cmdline_fixed_string_t port_tag_id;
	uint32_t port_tag_id_val;
	cmdline_fixed_string_t e_tag_id;
	uint16_t e_tag_id_val;
	cmdline_fixed_string_t dst_pool;
	uint8_t dst_pool_val;
	cmdline_fixed_string_t port;
	uint8_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
};

/* Common CLI fields for all E-tag configuration */
cmdline_parse_token_string_t cmd_config_e_tag_e_tag =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 e_tag, "E-tag");
cmdline_parse_token_string_t cmd_config_e_tag_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 set, "set");
cmdline_parse_token_string_t cmd_config_e_tag_insertion =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 insertion, "insertion");
cmdline_parse_token_string_t cmd_config_e_tag_stripping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 stripping, "stripping");
cmdline_parse_token_string_t cmd_config_e_tag_forwarding =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 forwarding, "forwarding");
cmdline_parse_token_string_t cmd_config_e_tag_filter =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 filter, "filter");
cmdline_parse_token_string_t cmd_config_e_tag_add =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 add, "add");
cmdline_parse_token_string_t cmd_config_e_tag_del =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 del, "del");
cmdline_parse_token_string_t cmd_config_e_tag_on =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 on, "on");
cmdline_parse_token_string_t cmd_config_e_tag_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 off, "off");
cmdline_parse_token_string_t cmd_config_e_tag_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 on_off, "on#off");
cmdline_parse_token_string_t cmd_config_e_tag_port_tag_id =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 port_tag_id, "port-tag-id");
cmdline_parse_token_num_t cmd_config_e_tag_port_tag_id_val =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_e_tag_result,
		 port_tag_id_val, UINT32);
cmdline_parse_token_string_t cmd_config_e_tag_e_tag_id =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 e_tag_id, "e-tag-id");
cmdline_parse_token_num_t cmd_config_e_tag_e_tag_id_val =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_e_tag_result,
		 e_tag_id_val, UINT16);
cmdline_parse_token_string_t cmd_config_e_tag_dst_pool =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 dst_pool, "dst-pool");
cmdline_parse_token_num_t cmd_config_e_tag_dst_pool_val =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_e_tag_result,
		 dst_pool_val, UINT8);
cmdline_parse_token_string_t cmd_config_e_tag_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 port, "port");
cmdline_parse_token_num_t cmd_config_e_tag_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_e_tag_result,
		 port_id, UINT8);
cmdline_parse_token_string_t cmd_config_e_tag_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_e_tag_result,
		 vf, "vf");
cmdline_parse_token_num_t cmd_config_e_tag_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_e_tag_result,
		 vf_id, UINT8);

/* E-tag insertion configuration */
static void
cmd_config_e_tag_insertion_en_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res =
		parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;
	entry.tunnel_id = res->port_tag_id_val;
	entry.vf_id = res->vf_id;
	rte_eth_dev_l2_tunnel_offload_set(res->port_id,
					  &entry,
					  ETH_L2_TUNNEL_INSERTION_MASK,
					  1);
}

static void
cmd_config_e_tag_insertion_dis_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res =
		parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;
	entry.vf_id = res->vf_id;

	rte_eth_dev_l2_tunnel_offload_set(res->port_id,
					  &entry,
					  ETH_L2_TUNNEL_INSERTION_MASK,
					  0);
}

cmdline_parse_inst_t cmd_config_e_tag_insertion_en = {
	.f = cmd_config_e_tag_insertion_en_parsed,
	.data = NULL,
	.help_str = "E-tag insertion enable",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_insertion,
		(void *)&cmd_config_e_tag_on,
		(void *)&cmd_config_e_tag_port_tag_id,
		(void *)&cmd_config_e_tag_port_tag_id_val,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		(void *)&cmd_config_e_tag_vf,
		(void *)&cmd_config_e_tag_vf_id,
		NULL,
	},
};

cmdline_parse_inst_t cmd_config_e_tag_insertion_dis = {
	.f = cmd_config_e_tag_insertion_dis_parsed,
	.data = NULL,
	.help_str = "E-tag insertion disable",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_insertion,
		(void *)&cmd_config_e_tag_off,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		(void *)&cmd_config_e_tag_vf,
		(void *)&cmd_config_e_tag_vf_id,
		NULL,
	},
};

/* E-tag stripping configuration */
static void
cmd_config_e_tag_stripping_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res =
		parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;

	if (!strcmp(res->on_off, "on"))
		rte_eth_dev_l2_tunnel_offload_set
			(res->port_id,
			 &entry,
			 ETH_L2_TUNNEL_STRIPPING_MASK,
			 1);
	else
		rte_eth_dev_l2_tunnel_offload_set
			(res->port_id,
			 &entry,
			 ETH_L2_TUNNEL_STRIPPING_MASK,
			 0);
}

cmdline_parse_inst_t cmd_config_e_tag_stripping_en_dis = {
	.f = cmd_config_e_tag_stripping_parsed,
	.data = NULL,
	.help_str = "E-tag stripping enable/disable",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_stripping,
		(void *)&cmd_config_e_tag_on_off,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		NULL,
	},
};

/* E-tag forwarding configuration */
static void
cmd_config_e_tag_forwarding_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res = parsed_result;
	struct rte_eth_l2_tunnel_conf entry;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;

	if (!strcmp(res->on_off, "on"))
		rte_eth_dev_l2_tunnel_offload_set
			(res->port_id,
			 &entry,
			 ETH_L2_TUNNEL_FORWARDING_MASK,
			 1);
	else
		rte_eth_dev_l2_tunnel_offload_set
			(res->port_id,
			 &entry,
			 ETH_L2_TUNNEL_FORWARDING_MASK,
			 0);
}

cmdline_parse_inst_t cmd_config_e_tag_forwarding_en_dis = {
	.f = cmd_config_e_tag_forwarding_parsed,
	.data = NULL,
	.help_str = "E-tag forwarding enable/disable",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_forwarding,
		(void *)&cmd_config_e_tag_on_off,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		NULL,
	},
};

/* E-tag filter configuration */
static void
cmd_config_e_tag_filter_add_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res = parsed_result;
	struct rte_eth_l2_tunnel_conf entry;
	int ret = 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (res->e_tag_id_val > 0x3fff) {
		printf("e-tag-id must be equal or less than 0x3fff.\n");
		return;
	}

	ret = rte_eth_dev_filter_supported(res->port_id,
					   RTE_ETH_FILTER_L2_TUNNEL);
	if (ret < 0) {
		printf("E-tag filter is not supported on port %u.\n",
		       res->port_id);
		return;
	}

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;
	entry.tunnel_id = res->e_tag_id_val;
	entry.pool = res->dst_pool_val;

	ret = rte_eth_dev_filter_ctrl(res->port_id,
				      RTE_ETH_FILTER_L2_TUNNEL,
				      RTE_ETH_FILTER_ADD,
				      &entry);
	if (ret < 0)
		printf("E-tag filter programming error: (%s)\n",
		       strerror(-ret));
}

cmdline_parse_inst_t cmd_config_e_tag_filter_add = {
	.f = cmd_config_e_tag_filter_add_parsed,
	.data = NULL,
	.help_str = "E-tag filter add",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_filter,
		(void *)&cmd_config_e_tag_add,
		(void *)&cmd_config_e_tag_e_tag_id,
		(void *)&cmd_config_e_tag_e_tag_id_val,
		(void *)&cmd_config_e_tag_dst_pool,
		(void *)&cmd_config_e_tag_dst_pool_val,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		NULL,
	},
};

static void
cmd_config_e_tag_filter_del_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_config_e_tag_result *res = parsed_result;
	struct rte_eth_l2_tunnel_conf entry;
	int ret = 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (res->e_tag_id_val > 0x3fff) {
		printf("e-tag-id must be less than 0x3fff.\n");
		return;
	}

	ret = rte_eth_dev_filter_supported(res->port_id,
					   RTE_ETH_FILTER_L2_TUNNEL);
	if (ret < 0) {
		printf("E-tag filter is not supported on port %u.\n",
		       res->port_id);
		return;
	}

	entry.l2_tunnel_type = RTE_L2_TUNNEL_TYPE_E_TAG;
	entry.tunnel_id = res->e_tag_id_val;

	ret = rte_eth_dev_filter_ctrl(res->port_id,
				      RTE_ETH_FILTER_L2_TUNNEL,
				      RTE_ETH_FILTER_DELETE,
				      &entry);
	if (ret < 0)
		printf("E-tag filter programming error: (%s)\n",
		       strerror(-ret));
}

cmdline_parse_inst_t cmd_config_e_tag_filter_del = {
	.f = cmd_config_e_tag_filter_del_parsed,
	.data = NULL,
	.help_str = "E-tag filter delete",
	.tokens = {
		(void *)&cmd_config_e_tag_e_tag,
		(void *)&cmd_config_e_tag_set,
		(void *)&cmd_config_e_tag_filter,
		(void *)&cmd_config_e_tag_del,
		(void *)&cmd_config_e_tag_e_tag_id,
		(void *)&cmd_config_e_tag_e_tag_id_val,
		(void *)&cmd_config_e_tag_port,
		(void *)&cmd_config_e_tag_port_id,
		NULL,
	},
};
#ifdef RTE_LIBRTE_IXGBE_PMD

/* vf vlan anti spoof configuration */

/* Common result structure for vf vlan anti spoof */
struct cmd_vf_vlan_anti_spoof_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t antispoof;
	uint8_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf vlan anti spoof enable disable */
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_vlan =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 vlan, "vlan");
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_antispoof =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 antispoof, "antispoof");
cmdline_parse_token_num_t cmd_vf_vlan_anti_spoof_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_vf_vlan_anti_spoof_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 vf_id, UINT32);
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 on_off, "on#off");

static void
cmd_set_vf_vlan_anti_spoof_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_vf_vlan_anti_spoof_result *res = parsed_result;
	int ret = 0;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_vf_vlan_anti_spoof(res->port_id, res->vf_id,
			is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_anti_spoof = {
	.f = cmd_set_vf_vlan_anti_spoof_parsed,
	.data = NULL,
	.help_str = "set vf vlan antispoof port_id vf_id on|off",
	.tokens = {
		(void *)&cmd_vf_vlan_anti_spoof_set,
		(void *)&cmd_vf_vlan_anti_spoof_vf,
		(void *)&cmd_vf_vlan_anti_spoof_vlan,
		(void *)&cmd_vf_vlan_anti_spoof_antispoof,
		(void *)&cmd_vf_vlan_anti_spoof_port_id,
		(void *)&cmd_vf_vlan_anti_spoof_vf_id,
		(void *)&cmd_vf_vlan_anti_spoof_on_off,
		NULL,
	},
};

/* vf mac anti spoof configuration */

/* Common result structure for vf mac anti spoof */
struct cmd_vf_mac_anti_spoof_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t mac;
	cmdline_fixed_string_t antispoof;
	uint8_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf mac anti spoof enable disable */
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_mac =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 mac, "mac");
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_antispoof =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 antispoof, "antispoof");
cmdline_parse_token_num_t cmd_vf_mac_anti_spoof_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_vf_mac_anti_spoof_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 vf_id, UINT32);
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 on_off, "on#off");

static void
cmd_set_vf_mac_anti_spoof_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_vf_mac_anti_spoof_result *res = parsed_result;
	int ret;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_vf_mac_anti_spoof(res->port_id, res->vf_id,
			is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d or is_on %d\n", res->vf_id, is_on);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_mac_anti_spoof = {
	.f = cmd_set_vf_mac_anti_spoof_parsed,
	.data = NULL,
	.help_str = "set vf mac antispoof port_id vf_id on|off",
	.tokens = {
		(void *)&cmd_vf_mac_anti_spoof_set,
		(void *)&cmd_vf_mac_anti_spoof_vf,
		(void *)&cmd_vf_mac_anti_spoof_mac,
		(void *)&cmd_vf_mac_anti_spoof_antispoof,
		(void *)&cmd_vf_mac_anti_spoof_port_id,
		(void *)&cmd_vf_mac_anti_spoof_vf_id,
		(void *)&cmd_vf_mac_anti_spoof_on_off,
		NULL,
	},
};

/* vf vlan strip queue configuration */

/* Common result structure for vf mac anti spoof */
struct cmd_vf_vlan_stripq_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t stripq;
	uint8_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf vlan strip enable disable */
cmdline_parse_token_string_t cmd_vf_vlan_stripq_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_vlan_stripq_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_vlan_stripq_vlan =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 vlan, "vlan");
cmdline_parse_token_string_t cmd_vf_vlan_stripq_stripq =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 stripq, "stripq");
cmdline_parse_token_num_t cmd_vf_vlan_stripq_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_vf_vlan_stripq_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 vf_id, UINT16);
cmdline_parse_token_string_t cmd_vf_vlan_stripq_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 on_off, "on#off");

static void
cmd_set_vf_vlan_stripq_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_vf_vlan_stripq_result *res = parsed_result;
	int ret = 0;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_vf_vlan_stripq(res->port_id, res->vf_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d or is_on %d\n", res->vf_id, is_on);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_stripq = {
	.f = cmd_set_vf_vlan_stripq_parsed,
	.data = NULL,
	.help_str = "set vf vlan stripq port_id vf_id on|off",
	.tokens = {
		(void *)&cmd_vf_vlan_stripq_set,
		(void *)&cmd_vf_vlan_stripq_vf,
		(void *)&cmd_vf_vlan_stripq_vlan,
		(void *)&cmd_vf_vlan_stripq_stripq,
		(void *)&cmd_vf_vlan_stripq_port_id,
		(void *)&cmd_vf_vlan_stripq_vf_id,
		(void *)&cmd_vf_vlan_stripq_on_off,
		NULL,
	},
};

/* vf vlan insert configuration */

/* Common result structure for vf vlan insert */
struct cmd_vf_vlan_insert_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t insert;
	uint8_t port_id;
	uint16_t vf_id;
	uint16_t vlan_id;
};

/* Common CLI fields for vf vlan insert enable disable */
cmdline_parse_token_string_t cmd_vf_vlan_insert_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_vlan_insert_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_vlan_insert_vlan =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vlan, "vlan");
cmdline_parse_token_string_t cmd_vf_vlan_insert_insert =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 insert, "insert");
cmdline_parse_token_num_t cmd_vf_vlan_insert_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_vf_vlan_insert_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vf_id, UINT16);
cmdline_parse_token_num_t cmd_vf_vlan_insert_vlan_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vlan_id, UINT16);

static void
cmd_set_vf_vlan_insert_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_vf_vlan_insert_result *res = parsed_result;
	int ret;

	ret = rte_pmd_ixgbe_set_vf_vlan_insert(res->port_id, res->vf_id, res->vlan_id);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d or vlan_id %d\n", res->vf_id, res->vlan_id);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_insert = {
	.f = cmd_set_vf_vlan_insert_parsed,
	.data = NULL,
	.help_str = "set vf vlan insert port_id vf_id vlan_id",
	.tokens = {
		(void *)&cmd_vf_vlan_insert_set,
		(void *)&cmd_vf_vlan_insert_vf,
		(void *)&cmd_vf_vlan_insert_vlan,
		(void *)&cmd_vf_vlan_insert_insert,
		(void *)&cmd_vf_vlan_insert_port_id,
		(void *)&cmd_vf_vlan_insert_vf_id,
		(void *)&cmd_vf_vlan_insert_vlan_id,
		NULL,
	},
};

/* tx loopback configuration */

/* Common result structure for tx loopback */
struct cmd_tx_loopback_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t loopback;
	uint8_t port_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for tx loopback enable disable */
cmdline_parse_token_string_t cmd_tx_loopback_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_loopback_result,
		 set, "set");
cmdline_parse_token_string_t cmd_tx_loopback_tx =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_loopback_result,
		 tx, "tx");
cmdline_parse_token_string_t cmd_tx_loopback_loopback =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_loopback_result,
		 loopback, "loopback");
cmdline_parse_token_num_t cmd_tx_loopback_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_tx_loopback_result,
		 port_id, UINT8);
cmdline_parse_token_string_t cmd_tx_loopback_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_loopback_result,
		 on_off, "on#off");

static void
cmd_set_tx_loopback_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_tx_loopback_result *res = parsed_result;
	int ret;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_tx_loopback(res->port_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid is_on %d\n", is_on);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_tx_loopback = {
	.f = cmd_set_tx_loopback_parsed,
	.data = NULL,
	.help_str = "set tx loopback port_id on|off",
	.tokens = {
		(void *)&cmd_tx_loopback_set,
		(void *)&cmd_tx_loopback_tx,
		(void *)&cmd_tx_loopback_loopback,
		(void *)&cmd_tx_loopback_port_id,
		(void *)&cmd_tx_loopback_on_off,
		NULL,
	},
};

/* all queues drop enable configuration */

/* Common result structure for all queues drop enable */
struct cmd_all_queues_drop_en_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t queues;
	cmdline_fixed_string_t drop;
	uint8_t port_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for tx loopback enable disable */
cmdline_parse_token_string_t cmd_all_queues_drop_en_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 set, "set");
cmdline_parse_token_string_t cmd_all_queues_drop_en_all =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 all, "all");
cmdline_parse_token_string_t cmd_all_queues_drop_en_queues =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 queues, "queues");
cmdline_parse_token_string_t cmd_all_queues_drop_en_drop =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 drop, "drop");
cmdline_parse_token_num_t cmd_all_queues_drop_en_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 port_id, UINT8);
cmdline_parse_token_string_t cmd_all_queues_drop_en_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 on_off, "on#off");

static void
cmd_set_all_queues_drop_en_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_all_queues_drop_en_result *res = parsed_result;
	int ret = 0;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_all_queues_drop_en(res->port_id, is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid is_on %d\n", is_on);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_all_queues_drop_en = {
	.f = cmd_set_all_queues_drop_en_parsed,
	.data = NULL,
	.help_str = "set all queues drop port_id on|off",
	.tokens = {
		(void *)&cmd_all_queues_drop_en_set,
		(void *)&cmd_all_queues_drop_en_all,
		(void *)&cmd_all_queues_drop_en_queues,
		(void *)&cmd_all_queues_drop_en_drop,
		(void *)&cmd_all_queues_drop_en_port_id,
		(void *)&cmd_all_queues_drop_en_on_off,
		NULL,
	},
};

/* vf split drop enable configuration */

/* Common result structure for vf split drop enable */
struct cmd_vf_split_drop_en_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t split;
	cmdline_fixed_string_t drop;
	uint8_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf split drop enable disable */
cmdline_parse_token_string_t cmd_vf_split_drop_en_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_split_drop_en_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_split_drop_en_split =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 split, "split");
cmdline_parse_token_string_t cmd_vf_split_drop_en_drop =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 drop, "drop");
cmdline_parse_token_num_t cmd_vf_split_drop_en_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_vf_split_drop_en_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 vf_id, UINT16);
cmdline_parse_token_string_t cmd_vf_split_drop_en_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 on_off, "on#off");

static void
cmd_set_vf_split_drop_en_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_vf_split_drop_en_result *res = parsed_result;
	int ret;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	ret = rte_pmd_ixgbe_set_vf_split_drop_en(res->port_id, res->vf_id,
			is_on);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d or is_on %d\n", res->vf_id, is_on);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_split_drop_en = {
	.f = cmd_set_vf_split_drop_en_parsed,
	.data = NULL,
	.help_str = "set vf split drop port_id vf_id on|off",
	.tokens = {
		(void *)&cmd_vf_split_drop_en_set,
		(void *)&cmd_vf_split_drop_en_vf,
		(void *)&cmd_vf_split_drop_en_split,
		(void *)&cmd_vf_split_drop_en_drop,
		(void *)&cmd_vf_split_drop_en_port_id,
		(void *)&cmd_vf_split_drop_en_vf_id,
		(void *)&cmd_vf_split_drop_en_on_off,
		NULL,
	},
};

/* vf mac address configuration */

/* Common result structure for vf mac address */
struct cmd_set_vf_mac_addr_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t mac;
	cmdline_fixed_string_t addr;
	uint8_t port_id;
	uint16_t vf_id;
	struct ether_addr mac_addr;

};

/* Common CLI fields for vf split drop enable disable */
cmdline_parse_token_string_t cmd_set_vf_mac_addr_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 set, "set");
cmdline_parse_token_string_t cmd_set_vf_mac_addr_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_set_vf_mac_addr_mac =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 mac, "mac");
cmdline_parse_token_string_t cmd_set_vf_mac_addr_addr =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 addr, "addr");
cmdline_parse_token_num_t cmd_set_vf_mac_addr_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 port_id, UINT8);
cmdline_parse_token_num_t cmd_set_vf_mac_addr_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 vf_id, UINT16);
cmdline_parse_token_etheraddr_t cmd_set_vf_mac_addr_mac_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_vf_mac_addr_result,
		 mac_addr);

static void
cmd_set_vf_mac_addr_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_set_vf_mac_addr_result *res = parsed_result;
	int ret;

	ret = rte_pmd_ixgbe_set_vf_mac_addr(res->port_id, res->vf_id,
			&res->mac_addr);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		printf("invalid vf_id %d or mac_addr\n", res->vf_id);
		break;
	case -ENODEV:
		printf("invalid port_id %d\n", res->port_id);
		break;
	default:
		printf("programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_mac_addr = {
	.f = cmd_set_vf_mac_addr_parsed,
	.data = NULL,
	.help_str = "set vf mac addr port_id vf_id xx:xx:xx:xx:xx:xx",
	.tokens = {
		(void *)&cmd_set_vf_mac_addr_set,
		(void *)&cmd_set_vf_mac_addr_vf,
		(void *)&cmd_set_vf_mac_addr_mac,
		(void *)&cmd_set_vf_mac_addr_addr,
		(void *)&cmd_set_vf_mac_addr_port_id,
		(void *)&cmd_set_vf_mac_addr_vf_id,
		(void *)&cmd_set_vf_mac_addr_mac_addr,
		NULL,
	},
};
#endif

/* ******************************************************************************** */

/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help_brief,
	(cmdline_parse_inst_t *)&cmd_help_long,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_showport,
	(cmdline_parse_inst_t *)&cmd_showqueue,
	(cmdline_parse_inst_t *)&cmd_showportall,
	(cmdline_parse_inst_t *)&cmd_showcfg,
	(cmdline_parse_inst_t *)&cmd_start,
	(cmdline_parse_inst_t *)&cmd_start_tx_first,
	(cmdline_parse_inst_t *)&cmd_start_tx_first_n,
	(cmdline_parse_inst_t *)&cmd_set_link_up,
	(cmdline_parse_inst_t *)&cmd_set_link_down,
	(cmdline_parse_inst_t *)&cmd_reset,
	(cmdline_parse_inst_t *)&cmd_set_numbers,
	(cmdline_parse_inst_t *)&cmd_set_txpkts,
	(cmdline_parse_inst_t *)&cmd_set_txsplit,
	(cmdline_parse_inst_t *)&cmd_set_fwd_list,
	(cmdline_parse_inst_t *)&cmd_set_fwd_mask,
	(cmdline_parse_inst_t *)&cmd_set_fwd_mode,
	(cmdline_parse_inst_t *)&cmd_set_fwd_retry_mode,
	(cmdline_parse_inst_t *)&cmd_set_burst_tx_retry,
	(cmdline_parse_inst_t *)&cmd_set_promisc_mode_one,
	(cmdline_parse_inst_t *)&cmd_set_promisc_mode_all,
	(cmdline_parse_inst_t *)&cmd_set_allmulti_mode_one,
	(cmdline_parse_inst_t *)&cmd_set_allmulti_mode_all,
	(cmdline_parse_inst_t *)&cmd_set_flush_rx,
	(cmdline_parse_inst_t *)&cmd_set_link_check,
#ifdef RTE_NIC_BYPASS
	(cmdline_parse_inst_t *)&cmd_set_bypass_mode,
	(cmdline_parse_inst_t *)&cmd_set_bypass_event,
	(cmdline_parse_inst_t *)&cmd_set_bypass_timeout,
	(cmdline_parse_inst_t *)&cmd_show_bypass_config,
#endif
#ifdef RTE_LIBRTE_PMD_BOND
	(cmdline_parse_inst_t *) &cmd_set_bonding_mode,
	(cmdline_parse_inst_t *) &cmd_show_bonding_config,
	(cmdline_parse_inst_t *) &cmd_set_bonding_primary,
	(cmdline_parse_inst_t *) &cmd_add_bonding_slave,
	(cmdline_parse_inst_t *) &cmd_remove_bonding_slave,
	(cmdline_parse_inst_t *) &cmd_create_bonded_device,
	(cmdline_parse_inst_t *) &cmd_set_bond_mac_addr,
	(cmdline_parse_inst_t *) &cmd_set_balance_xmit_policy,
	(cmdline_parse_inst_t *) &cmd_set_bond_mon_period,
#endif
	(cmdline_parse_inst_t *)&cmd_vlan_offload,
	(cmdline_parse_inst_t *)&cmd_vlan_tpid,
	(cmdline_parse_inst_t *)&cmd_rx_vlan_filter_all,
	(cmdline_parse_inst_t *)&cmd_rx_vlan_filter,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_set,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_set_qinq,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_reset,
	(cmdline_parse_inst_t *)&cmd_tx_vlan_set_pvid,
	(cmdline_parse_inst_t *)&cmd_csum_set,
	(cmdline_parse_inst_t *)&cmd_csum_show,
	(cmdline_parse_inst_t *)&cmd_csum_tunnel,
	(cmdline_parse_inst_t *)&cmd_tso_set,
	(cmdline_parse_inst_t *)&cmd_tso_show,
	(cmdline_parse_inst_t *)&cmd_tunnel_tso_set,
	(cmdline_parse_inst_t *)&cmd_tunnel_tso_show,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_rx,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_tx,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_hw,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_lw,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_pt,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_xon,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_macfwd,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_autoneg,
	(cmdline_parse_inst_t *)&cmd_priority_flow_control_set,
	(cmdline_parse_inst_t *)&cmd_config_dcb,
	(cmdline_parse_inst_t *)&cmd_read_reg,
	(cmdline_parse_inst_t *)&cmd_read_reg_bit_field,
	(cmdline_parse_inst_t *)&cmd_read_reg_bit,
	(cmdline_parse_inst_t *)&cmd_write_reg,
	(cmdline_parse_inst_t *)&cmd_write_reg_bit_field,
	(cmdline_parse_inst_t *)&cmd_write_reg_bit,
	(cmdline_parse_inst_t *)&cmd_read_rxd_txd,
	(cmdline_parse_inst_t *)&cmd_stop,
	(cmdline_parse_inst_t *)&cmd_mac_addr,
	(cmdline_parse_inst_t *)&cmd_set_qmap,
	(cmdline_parse_inst_t *)&cmd_operate_port,
	(cmdline_parse_inst_t *)&cmd_operate_specific_port,
	(cmdline_parse_inst_t *)&cmd_operate_attach_port,
	(cmdline_parse_inst_t *)&cmd_operate_detach_port,
	(cmdline_parse_inst_t *)&cmd_config_speed_all,
	(cmdline_parse_inst_t *)&cmd_config_speed_specific,
	(cmdline_parse_inst_t *)&cmd_config_rx_tx,
	(cmdline_parse_inst_t *)&cmd_config_mtu,
	(cmdline_parse_inst_t *)&cmd_config_max_pkt_len,
	(cmdline_parse_inst_t *)&cmd_config_rx_mode_flag,
	(cmdline_parse_inst_t *)&cmd_config_rss,
	(cmdline_parse_inst_t *)&cmd_config_rxtx_queue,
	(cmdline_parse_inst_t *)&cmd_config_txqflags,
	(cmdline_parse_inst_t *)&cmd_config_rss_reta,
	(cmdline_parse_inst_t *)&cmd_showport_reta,
	(cmdline_parse_inst_t *)&cmd_config_burst,
	(cmdline_parse_inst_t *)&cmd_config_thresh,
	(cmdline_parse_inst_t *)&cmd_config_threshold,
	(cmdline_parse_inst_t *)&cmd_set_vf_rxmode,
	(cmdline_parse_inst_t *)&cmd_set_uc_hash_filter,
	(cmdline_parse_inst_t *)&cmd_set_uc_all_hash_filter,
	(cmdline_parse_inst_t *)&cmd_vf_mac_addr_filter,
	(cmdline_parse_inst_t *)&cmd_set_vf_macvlan_filter,
	(cmdline_parse_inst_t *)&cmd_set_vf_traffic,
	(cmdline_parse_inst_t *)&cmd_vf_rxvlan_filter,
	(cmdline_parse_inst_t *)&cmd_queue_rate_limit,
	(cmdline_parse_inst_t *)&cmd_vf_rate_limit,
	(cmdline_parse_inst_t *)&cmd_tunnel_filter,
	(cmdline_parse_inst_t *)&cmd_tunnel_udp_config,
	(cmdline_parse_inst_t *)&cmd_global_config,
	(cmdline_parse_inst_t *)&cmd_set_mirror_mask,
	(cmdline_parse_inst_t *)&cmd_set_mirror_link,
	(cmdline_parse_inst_t *)&cmd_reset_mirror_rule,
	(cmdline_parse_inst_t *)&cmd_showport_rss_hash,
	(cmdline_parse_inst_t *)&cmd_showport_rss_hash_key,
	(cmdline_parse_inst_t *)&cmd_config_rss_hash_key,
	(cmdline_parse_inst_t *)&cmd_dump,
	(cmdline_parse_inst_t *)&cmd_dump_one,
	(cmdline_parse_inst_t *)&cmd_ethertype_filter,
	(cmdline_parse_inst_t *)&cmd_syn_filter,
	(cmdline_parse_inst_t *)&cmd_2tuple_filter,
	(cmdline_parse_inst_t *)&cmd_5tuple_filter,
	(cmdline_parse_inst_t *)&cmd_flex_filter,
	(cmdline_parse_inst_t *)&cmd_add_del_ip_flow_director,
	(cmdline_parse_inst_t *)&cmd_add_del_udp_flow_director,
	(cmdline_parse_inst_t *)&cmd_add_del_sctp_flow_director,
	(cmdline_parse_inst_t *)&cmd_add_del_l2_flow_director,
	(cmdline_parse_inst_t *)&cmd_add_del_mac_vlan_flow_director,
	(cmdline_parse_inst_t *)&cmd_add_del_tunnel_flow_director,
	(cmdline_parse_inst_t *)&cmd_flush_flow_director,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_ip_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_mac_vlan_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_tunnel_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_flex_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_flex_payload,
	(cmdline_parse_inst_t *)&cmd_get_sym_hash_ena_per_port,
	(cmdline_parse_inst_t *)&cmd_set_sym_hash_ena_per_port,
	(cmdline_parse_inst_t *)&cmd_get_hash_global_config,
	(cmdline_parse_inst_t *)&cmd_set_hash_global_config,
	(cmdline_parse_inst_t *)&cmd_set_hash_input_set,
	(cmdline_parse_inst_t *)&cmd_set_fdir_input_set,
	(cmdline_parse_inst_t *)&cmd_mcast_addr,
	(cmdline_parse_inst_t *)&cmd_config_l2_tunnel_eth_type_all,
	(cmdline_parse_inst_t *)&cmd_config_l2_tunnel_eth_type_specific,
	(cmdline_parse_inst_t *)&cmd_config_l2_tunnel_en_dis_all,
	(cmdline_parse_inst_t *)&cmd_config_l2_tunnel_en_dis_specific,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_insertion_en,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_insertion_dis,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_stripping_en_dis,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_forwarding_en_dis,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_filter_add,
	(cmdline_parse_inst_t *)&cmd_config_e_tag_filter_del,
#ifdef RTE_LIBRTE_IXGBE_PMD
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_anti_spoof,
	(cmdline_parse_inst_t *)&cmd_set_vf_mac_anti_spoof,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_stripq,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_insert,
	(cmdline_parse_inst_t *)&cmd_set_tx_loopback,
	(cmdline_parse_inst_t *)&cmd_set_all_queues_drop_en,
	(cmdline_parse_inst_t *)&cmd_set_vf_split_drop_en,
	(cmdline_parse_inst_t *)&cmd_set_vf_mac_addr,
#endif
	NULL,
};

/* prompt function, called from main on MASTER lcore */
void
prompt(void)
{
	/* initialize non-constant commands */
	cmd_set_fwd_mode_init();
	cmd_set_fwd_retry_mode_init();

	testpmd_cl = cmdline_stdin_new(main_ctx, "testpmd> ");
	if (testpmd_cl == NULL)
		return;
	cmdline_interact(testpmd_cl);
	cmdline_stdin_exit(testpmd_cl);
}

void
prompt_exit(void)
{
	if (testpmd_cl != NULL)
		cmdline_quit(testpmd_cl);
}

static void
cmd_reconfig_device_queue(portid_t id, uint8_t dev, uint8_t queue)
{
	if (id == (portid_t)RTE_PORT_ALL) {
		portid_t pid;

		FOREACH_PORT(pid, ports) {
			/* check if need_reconfig has been set to 1 */
			if (ports[pid].need_reconfig == 0)
				ports[pid].need_reconfig = dev;
			/* check if need_reconfig_queues has been set to 1 */
			if (ports[pid].need_reconfig_queues == 0)
				ports[pid].need_reconfig_queues = queue;
		}
	} else if (!port_id_is_invalid(id, DISABLED_WARN)) {
		/* check if need_reconfig has been set to 1 */
		if (ports[id].need_reconfig == 0)
			ports[id].need_reconfig = dev;
		/* check if need_reconfig_queues has been set to 1 */
		if (ports[id].need_reconfig_queues == 0)
			ports[id].need_reconfig_queues = queue;
	}
}
