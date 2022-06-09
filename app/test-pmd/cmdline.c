/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
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
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_devargs.h>
#include <rte_flow.h>
#ifdef RTE_LIB_GRO
#include <rte_gro.h>
#endif
#include <rte_mbuf_dyn.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>
#ifdef RTE_NET_BOND
#include <rte_eth_bond.h>
#include <rte_eth_bond_8023ad.h>
#endif
#if defined RTE_BUS_DPAA && defined RTE_NET_DPAA
#include <rte_pmd_dpaa.h>
#endif
#ifdef RTE_NET_IXGBE
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_NET_I40E
#include <rte_pmd_i40e.h>
#endif
#ifdef RTE_NET_BNXT
#include <rte_pmd_bnxt.h>
#endif
#include "testpmd.h"
#include "cmdline_mtr.h"
#include "cmdline_tm.h"
#include "bpf_cmd.h"

static struct cmdline *testpmd_cl;

static void cmd_reconfig_device_queue(portid_t id, uint8_t dev, uint8_t queue);

/* *** Help command with introduction. *** */
struct cmd_help_brief_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_brief_parsed(__rte_unused void *parsed_result,
                                  struct cmdline *cl,
                                  __rte_unused void *data)
{
	cmdline_printf(
		cl,
		"\n"
		"Help is available for the following sections:\n\n"
		"    help control                    : Start and stop forwarding.\n"
		"    help display                    : Displaying port, stats and config "
		"information.\n"
		"    help config                     : Configuration information.\n"
		"    help ports                      : Configuring ports.\n"
		"    help registers                  : Reading and setting port registers.\n"
		"    help filters                    : Filters configuration help.\n"
		"    help traffic_management         : Traffic Management commands.\n"
		"    help devices                    : Device related cmds.\n"
		"    help all                        : All of the above sections.\n\n"
	);

}

cmdline_parse_token_string_t cmd_help_brief_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_brief_result, help, "help");

cmdline_parse_inst_t cmd_help_brief = {
	.f = cmd_help_brief_parsed,
	.data = NULL,
	.help_str = "help: Show help",
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
                                 __rte_unused void *data)
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

			"show port (info|stats|summary|xstats|fdir|dcb_tc) (port_id|all)\n"
			"    Display information for port_id, or all.\n\n"

			"show port info (port_id) representor\n"
			"    Show supported representors for a specific port\n\n"

			"show port port_id (module_eeprom|eeprom)\n"
			"    Display the module EEPROM or EEPROM information for port_id.\n\n"

			"show port X rss reta (size) (mask0,mask1,...)\n"
			"    Display the rss redirection table entry indicated"
			" by masks on port X. size is used to indicate the"
			" hardware supported reta size\n\n"

			"show port (port_id) rss-hash [key]\n"
			"    Display the RSS hash functions and RSS hash key of port\n\n"

			"clear port (info|stats|xstats|fdir) (port_id|all)\n"
			"    Clear information for port_id, or all.\n\n"

			"show (rxq|txq) info (port_id) (queue_id)\n"
			"    Display information for configured RX/TX queue.\n\n"

			"show config (rxtx|cores|fwd|rxoffs|rxpkts|txpkts)\n"
			"    Display the given configuration.\n\n"

			"read rxd (port_id) (queue_id) (rxd_id)\n"
			"    Display an RX descriptor of a port RX queue.\n\n"

			"read txd (port_id) (queue_id) (txd_id)\n"
			"    Display a TX descriptor of a port TX queue.\n\n"

			"ddp get list (port_id)\n"
			"    Get ddp profile info list\n\n"

			"ddp get info (profile_path)\n"
			"    Get ddp profile information.\n\n"

			"show vf stats (port_id) (vf_id)\n"
			"    Display a VF's statistics.\n\n"

			"clear vf stats (port_id) (vf_id)\n"
			"    Reset a VF's statistics.\n\n"

			"show port (port_id) pctype mapping\n"
			"    Get flow ptype to pctype mapping on a port\n\n"

			"show port meter stats (port_id) (meter_id) (clear)\n"
			"    Get meter stats on a port\n\n"

			"show fwd stats all\n"
			"    Display statistics for all fwd engines.\n\n"

			"clear fwd stats all\n"
			"    Clear statistics for all fwd engines.\n\n"

			"show port (port_id) rx_offload capabilities\n"
			"    List all per queue and per port Rx offloading"
			" capabilities of a port\n\n"

			"show port (port_id) rx_offload configuration\n"
			"    List port level and all queue level"
			" Rx offloading configuration\n\n"

			"show port (port_id) tx_offload capabilities\n"
			"    List all per queue and per port"
			" Tx offloading capabilities of a port\n\n"

			"show port (port_id) tx_offload configuration\n"
			"    List port level and all queue level"
			" Tx offloading configuration\n\n"

			"show port (port_id) tx_metadata\n"
			"    Show Tx metadata value set"
			" for a specific port\n\n"

			"show port (port_id) ptypes\n"
			"    Show port supported ptypes"
			" for a specific port\n\n"

			"show device info (<identifier>|all)"
			"       Show general information about devices probed.\n\n"

			"show port (port_id) rxq|txq (queue_id) desc (desc_id) status"
			"       Show status of rx|tx descriptor.\n\n"

			"show port (port_id) rxq (queue_id) desc used count\n"
			"    Show current number of filled receive"
			" packet descriptors.\n\n"

			"show port (port_id) macs|mcast_macs"
			"       Display list of mac addresses added to port.\n\n"

			"show port (port_id) flow transfer proxy\n"
			"	Display proxy port to manage transfer flows\n\n"

			"show port (port_id) fec capabilities"
			"	Show fec capabilities of a port.\n\n"

			"show port (port_id) fec_mode"
			"	Show fec mode of a port.\n\n"

			"show port (port_id) flow_ctrl"
			"	Show flow control info of a port.\n\n"
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

			"set log global|(type) (level)\n"
			"    Set the log level.\n\n"

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

			"set rxoffs (x[,y]*)\n"
			"    Set the offset of each packet segment on"
			" receiving if split feature is engaged."
			" Affects only the queues configured with split"
			" offloads.\n\n"

			"set rxpkts (x[,y]*)\n"
			"    Set the length of each segment to scatter"
			" packets on receiving if split feature is engaged."
			" Affects only the queues configured with split"
			" offloads.\n\n"

			"set txpkts (x[,y]*)\n"
			"    Set the length of each segment of TXONLY"
			" and optionally CSUM packets.\n\n"

			"set txsplit (off|on|rand)\n"
			"    Set the split policy for the TX packets."
			" Right now only applicable for CSUM and TXONLY"
			" modes\n\n"

			"set txtimes (x, y)\n"
			"    Set the scheduling on timestamps"
			" timings for the TXONLY mode\n\n"

			"set corelist (x[,y]*)\n"
			"    Set the list of forwarding cores.\n\n"

			"set portlist (x[,y]*)\n"
			"    Set the list of forwarding ports.\n\n"

			"set port setup on (iterator|event)\n"
			"    Select how attached port is retrieved for setup.\n\n"

			"set tx loopback (port_id) (on|off)\n"
			"    Enable or disable tx loopback.\n\n"

			"set all queues drop (port_id) (on|off)\n"
			"    Set drop enable bit for all queues.\n\n"

			"set vf split drop (port_id) (vf_id) (on|off)\n"
			"    Set split drop enable bit for a VF from the PF.\n\n"

			"set vf mac antispoof (port_id) (vf_id) (on|off).\n"
			"    Set MAC antispoof for a VF from the PF.\n\n"

			"set macsec offload (port_id) on encrypt (on|off) replay-protect (on|off)\n"
			"    Enable MACsec offload.\n\n"

			"set macsec offload (port_id) off\n"
			"    Disable MACsec offload.\n\n"

			"set macsec sc (tx|rx) (port_id) (mac) (pi)\n"
			"    Configure MACsec secure connection (SC).\n\n"

			"set macsec sa (tx|rx) (port_id) (idx) (an) (pn) (key)\n"
			"    Configure MACsec secure association (SA).\n\n"

			"set vf broadcast (port_id) (vf_id) (on|off)\n"
			"    Set VF broadcast for a VF from the PF.\n\n"

			"vlan set stripq (on|off) (port_id,queue_id)\n"
			"    Set the VLAN strip for a queue on a port.\n\n"

			"set vf vlan stripq (port_id) (vf_id) (on|off)\n"
			"    Set the VLAN strip for all queues in a pool for a VF from the PF.\n\n"

			"set vf vlan insert (port_id) (vf_id) (vlan_id)\n"
			"    Set VLAN insert for a VF from the PF.\n\n"

			"set vf vlan antispoof (port_id) (vf_id) (on|off)\n"
			"    Set VLAN antispoof for a VF from the PF.\n\n"

			"set vf vlan tag (port_id) (vf_id) (on|off)\n"
			"    Set VLAN tag for a VF from the PF.\n\n"

			"set vf tx max-bandwidth (port_id) (vf_id) (bandwidth)\n"
			"    Set a VF's max bandwidth(Mbps).\n\n"

			"set vf tc tx min-bandwidth (port_id) (vf_id) (bw1, bw2, ...)\n"
			"    Set all TCs' min bandwidth(%%) on a VF.\n\n"

			"set vf tc tx max-bandwidth (port_id) (vf_id) (tc_no) (bandwidth)\n"
			"    Set a TC's max bandwidth(Mbps) on a VF.\n\n"

			"set tx strict-link-priority (port_id) (tc_bitmap)\n"
			"    Set some TCs' strict link priority mode on a physical port.\n\n"

			"set tc tx min-bandwidth (port_id) (bw1, bw2, ...)\n"
			"    Set all TCs' min bandwidth(%%) for all PF and VFs.\n\n"

			"vlan set (strip|filter|qinq_strip|extend) (on|off) (port_id)\n"
			"    Set the VLAN strip or filter or qinq strip or extend\n\n"

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

			"csum set (ip|udp|tcp|sctp|outer-ip|outer-udp) (hw|sw) (port_id)\n"
			"    Select hardware or software calculation of the"
			" checksum when transmitting a packet using the"
			" csum forward engine.\n"
			"    ip|udp|tcp|sctp always concern the inner layer.\n"
			"    outer-ip concerns the outer IP layer in"
			"    outer-udp concerns the outer UDP layer in"
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

#ifdef RTE_LIB_GRO
			"set port (port_id) gro on|off\n"
			"    Enable or disable Generic Receive Offload in"
			" csum forwarding engine.\n\n"

			"show port (port_id) gro\n"
			"    Display GRO configuration.\n\n"

			"set gro flush (cycles)\n"
			"    Set the cycle to flush GROed packets from"
			" reassembly tables.\n\n"
#endif

#ifdef RTE_LIB_GSO
			"set port (port_id) gso (on|off)"
			"    Enable or disable Generic Segmentation Offload in"
			" csum forwarding engine.\n\n"

			"set gso segsz (length)\n"
			"    Set max packet length for output GSO segments,"
			" including packet header and payload.\n\n"

			"show port (port_id) gso\n"
			"    Show GSO configuration.\n\n"
#endif

			"set fwd (%s)\n"
			"    Set packet forwarding mode.\n\n"

			"mac_addr add (port_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Add a MAC address on port_id.\n\n"

			"mac_addr remove (port_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Remove a MAC address from port_id.\n\n"

			"mac_addr set (port_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Set the default MAC address for port_id.\n\n"

			"mac_addr add port (port_id) vf (vf_id) (mac_address)\n"
			"    Add a MAC address for a VF on the port.\n\n"

			"set vf mac addr (port_id) (vf_id) (XX:XX:XX:XX:XX:XX)\n"
			"    Set the MAC address for a VF from the PF.\n\n"

			"set eth-peer (port_id) (peer_addr)\n"
			"    set the peer address for certain port.\n\n"

			"set port (port_id) uta (mac_address|all) (on|off)\n"
			"    Add/Remove a or all unicast hash filter(s)"
			"from port X.\n\n"

			"set promisc (port_id|all) (on|off)\n"
			"    Set the promiscuous mode on port_id, or all.\n\n"

			"set allmulti (port_id|all) (on|off)\n"
			"    Set the allmulti mode on port_id, or all.\n\n"

			"set vf promisc (port_id) (vf_id) (on|off)\n"
			"    Set unicast promiscuous mode for a VF from the PF.\n\n"

			"set vf allmulti (port_id) (vf_id) (on|off)\n"
			"    Set multicast promiscuous mode for a VF from the PF.\n\n"

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

			"set xstats-hide-zero on|off\n"
			"    Set the option to hide the zero values"
			" for xstats display.\n"

			"set record-core-cycles on|off\n"
			"    Set the option to enable measurement of CPU cycles.\n"

			"set record-burst-stats on|off\n"
			"    Set the option to enable display of RX and TX bursts.\n"

			"set port (port_id) vf (vf_id) rx|tx on|off\n"
			"    Enable/Disable a VF receive/transmit from a port\n\n"

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

			"set flush_rx (on|off)\n"
			"   Flush (default) or don't flush RX streams before"
			" forwarding. Mainly used with PCAP drivers.\n\n"

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

#ifdef RTE_NET_BOND
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

			"show bonding lacp info (port_id)\n"
			"	Show the bonding lacp information for port_id.\n\n"

			"set bonding mac_addr (port_id) (address)\n"
			"	Set the MAC address of a bonded device.\n\n"

			"set bonding mode IEEE802.3AD aggregator policy (port_id) (agg_name)"
			"	Set Aggregation mode for IEEE802.3AD (mode 4)"

			"set bonding balance_xmit_policy (port_id) (l2|l23|l34)\n"
			"	Set the transmit balance policy for bonded device running in balance mode.\n\n"

			"set bonding mon_period (port_id) (value)\n"
			"	Set the bonding link status monitoring polling period in ms.\n\n"

			"set bonding lacp dedicated_queues <port_id> (enable|disable)\n"
			"	Enable/disable dedicated queues for LACP control traffic.\n\n"

#endif
			"set link-up port (port_id)\n"
			"	Set link up for a port.\n\n"

			"set link-down port (port_id)\n"
			"	Set link down for a port.\n\n"

			"ddp add (port_id) (profile_path[,backup_profile_path])\n"
			"    Load a profile package on a port\n\n"

			"ddp del (port_id) (backup_profile_path)\n"
			"    Delete a profile package from a port\n\n"

			"ptype mapping get (port_id) (valid_only)\n"
			"    Get ptype mapping on a port\n\n"

			"ptype mapping replace (port_id) (target) (mask) (pky_type)\n"
			"    Replace target with the pkt_type in ptype mapping\n\n"

			"ptype mapping reset (port_id)\n"
			"    Reset ptype mapping on a port\n\n"

			"ptype mapping update (port_id) (hw_ptype) (sw_ptype)\n"
			"    Update a ptype mapping item on a port\n\n"

			"set port (port_id) ptype_mask (ptype_mask)\n"
			"    set packet types classification for a specific port\n\n"

			"set port (port_id) queue-region region_id (value) "
			"queue_start_index (value) queue_num (value)\n"
			"    Set a queue region on a port\n\n"

			"set port (port_id) queue-region region_id (value) "
			"flowtype (value)\n"
			"    Set a flowtype region index on a port\n\n"

			"set port (port_id) queue-region UP (value) region_id (value)\n"
			"    Set the mapping of User Priority to "
			"queue region on a port\n\n"

			"set port (port_id) queue-region flush (on|off)\n"
			"    flush all queue region related configuration\n\n"

			"show port meter cap (port_id)\n"
			"    Show port meter capability information\n\n"

			"add port meter profile srtcm_rfc2697 (port_id) (profile_id) (cir) (cbs) (ebs) (packet_mode)\n"
			"    meter profile add - srtcm rfc 2697\n\n"

			"add port meter profile trtcm_rfc2698 (port_id) (profile_id) (cir) (pir) (cbs) (pbs) (packet_mode)\n"
			"    meter profile add - trtcm rfc 2698\n\n"

			"add port meter profile trtcm_rfc4115 (port_id) (profile_id) (cir) (eir) (cbs) (ebs) (packet_mode)\n"
			"    meter profile add - trtcm rfc 4115\n\n"

			"del port meter profile (port_id) (profile_id)\n"
			"    meter profile delete\n\n"

			"create port meter (port_id) (mtr_id) (profile_id) (policy_id) (meter_enable)\n"
			"(stats_mask) (shared) (use_pre_meter_color) [(dscp_tbl_entry0) (dscp_tbl_entry1)...\n"
			"(dscp_tbl_entry63)]\n"
			"    meter create\n\n"

			"enable port meter (port_id) (mtr_id)\n"
			"    meter enable\n\n"

			"disable port meter (port_id) (mtr_id)\n"
			"    meter disable\n\n"

			"del port meter (port_id) (mtr_id)\n"
			"    meter delete\n\n"

			"add port meter policy (port_id) (policy_id) g_actions (actions)\n"
			"y_actions (actions) r_actions (actions)\n"
			"    meter policy add\n\n"

			"del port meter policy (port_id) (policy_id)\n"
			"    meter policy delete\n\n"

			"set port meter profile (port_id) (mtr_id) (profile_id)\n"
			"    meter update meter profile\n\n"

			"set port meter dscp table (port_id) (mtr_id) [(dscp_tbl_entry0)\n"
			"(dscp_tbl_entry1)...(dscp_tbl_entry63)]\n"
			"    update meter dscp table entries\n\n"

			"set port meter policer action (port_id) (mtr_id) (action_mask)\n"
			"(action0) [(action1) (action2)]\n"
			"    meter update policer action\n\n"

			"set port meter stats mask (port_id) (mtr_id) (stats_mask)\n"
			"    meter update stats\n\n"

			"show port (port_id) queue-region\n"
			"    show all queue region related configuration info\n\n"

			"set port (port_id) fec_mode auto|off|rs|baser\n"
			"    set fec mode for a specific port\n\n"

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

			"port reset (port_id|all)\n"
			"    Reset all ports or port_id.\n\n"

			"port attach (ident)\n"
			"    Attach physical or virtual dev by pci address or virtual device name\n\n"

			"port detach (port_id)\n"
			"    Detach physical or virtual dev by port_id\n\n"

			"port config (port_id|all)"
			" speed (10|100|1000|10000|25000|40000|50000|100000|200000|auto)"
			" duplex (half|full|auto)\n"
			"    Set speed and duplex for all ports or port_id\n\n"

			"port config (port_id|all) loopback (mode)\n"
			"    Set loopback mode for all ports or port_id\n\n"

			"port config all (rxq|txq|rxd|txd) (value)\n"
			"    Set number for rxq/txq/rxd/txd.\n\n"

			"port config all max-pkt-len (value)\n"
			"    Set the max packet length.\n\n"

			"port config all max-lro-pkt-size (value)\n"
			"    Set the max LRO aggregated packet size.\n\n"

			"port config all drop-en (on|off)\n"
			"    Enable or disable packet drop on all RX queues of all ports when no "
			"receive buffers available.\n\n"

			"port config all rss (all|default|ip|tcp|udp|sctp|"
			"ether|port|vxlan|geneve|nvgre|vxlan-gpe|ecpri|mpls|none|level-default|"
			"level-outer|level-inner|<flowtype_id>)\n"
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

			"port config (port_id) (rxq|txq) (queue_id) ring_size (value)\n"
			"    Set a rx/tx queue's ring size configuration, the new"
			" value will take effect after command that (re-)start the port"
			" or command that setup the specific queue\n\n"

			"port (port_id) (rxq|txq) (queue_id) (start|stop)\n"
			"    Start/stop a rx/tx queue of port X. Only take effect"
			" when port X is started\n\n"

			"port (port_id) (rxq|txq) (queue_id) deferred_start (on|off)\n"
			"    Switch on/off a deferred start of port X rx/tx queue. Only"
			" take effect when port X is stopped.\n\n"

			"port (port_id) (rxq|txq) (queue_id) setup\n"
			"    Setup a rx/tx queue of port X.\n\n"

			"port config (port_id) pctype mapping reset\n"
			"    Reset flow type to pctype mapping on a port\n\n"

			"port config (port_id) pctype mapping update"
			" (pctype_id_0[,pctype_id_1]*) (flow_type_id)\n"
			"    Update a flow type to pctype mapping item on a port\n\n"

			"port config (port_id) pctype (pctype_id) hash_inset|"
			"fdir_inset|fdir_flx_inset get|set|clear field\n"
			" (field_idx)\n"
			"    Configure RSS|FDIR|FDIR_FLX input set for some pctype\n\n"

			"port config (port_id) pctype (pctype_id) hash_inset|"
			"fdir_inset|fdir_flx_inset clear all"
			"    Clear RSS|FDIR|FDIR_FLX input set completely for some pctype\n\n"

			"port config (port_id) udp_tunnel_port add|rm vxlan|geneve|ecpri (udp_port)\n\n"
			"    Add/remove UDP tunnel port for tunneling offload\n\n"

			"port config <port_id> rx_offload vlan_strip|"
			"ipv4_cksum|udp_cksum|tcp_cksum|tcp_lro|qinq_strip|"
			"outer_ipv4_cksum|macsec_strip|header_split|"
			"vlan_filter|vlan_extend|jumbo_frame|scatter|"
			"buffer_split|timestamp|security|keep_crc on|off\n"
			"     Enable or disable a per port Rx offloading"
			" on all Rx queues of a port\n\n"

			"port (port_id) rxq (queue_id) rx_offload vlan_strip|"
			"ipv4_cksum|udp_cksum|tcp_cksum|tcp_lro|qinq_strip|"
			"outer_ipv4_cksum|macsec_strip|header_split|"
			"vlan_filter|vlan_extend|jumbo_frame|scatter|"
			"buffer_split|timestamp|security|keep_crc on|off\n"
			"    Enable or disable a per queue Rx offloading"
			" only on a specific Rx queue\n\n"

			"port config (port_id) tx_offload vlan_insert|"
			"ipv4_cksum|udp_cksum|tcp_cksum|sctp_cksum|tcp_tso|"
			"udp_tso|outer_ipv4_cksum|qinq_insert|vxlan_tnl_tso|"
			"gre_tnl_tso|ipip_tnl_tso|geneve_tnl_tso|"
			"macsec_insert|mt_lockfree|multi_segs|mbuf_fast_free|"
			"security on|off\n"
			"    Enable or disable a per port Tx offloading"
			" on all Tx queues of a port\n\n"

			"port (port_id) txq (queue_id) tx_offload vlan_insert|"
			"ipv4_cksum|udp_cksum|tcp_cksum|sctp_cksum|tcp_tso|"
			"udp_tso|outer_ipv4_cksum|qinq_insert|vxlan_tnl_tso|"
			"gre_tnl_tso|ipip_tnl_tso|geneve_tnl_tso|macsec_insert"
			"|mt_lockfree|multi_segs|mbuf_fast_free|security"
			" on|off\n"
			"    Enable or disable a per queue Tx offloading"
			" only on a specific Tx queue\n\n"

			"bpf-load rx|tx (port) (queue) (J|M|B) (file_name)\n"
			"    Load an eBPF program as a callback"
			" for particular RX/TX queue\n\n"

			"bpf-unload rx|tx (port) (queue)\n"
			"    Unload previously loaded eBPF program"
			" for particular RX/TX queue\n\n"

			"port config (port_id) tx_metadata (value)\n"
			"    Set Tx metadata value per port. Testpmd will add this value"
			" to any Tx packet sent from this port\n\n"

			"port config (port_id) dynf (name) set|clear\n"
			"    Register a dynf and Set/clear this flag on Tx. "
			"Testpmd will set this value to any Tx packet "
			"sent from this port\n\n"

			"port cleanup (port_id) txq (queue_id) (free_cnt)\n"
			"    Cleanup txq mbufs for a specific Tx queue\n\n"
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

#ifdef RTE_NET_I40E
			"flow_director_filter (port_id) mode raw (add|del|update)"
			" flow (flow_id) (drop|fwd) queue (queue_id)"
			" fd_id (fd_id_value) packet (packet file name)\n"
			"    Add/Del a raw type flow director filter.\n\n"
#endif

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

			"flow_director_flex_payload (port_id)"
			" (raw|l2|l3|l4) (config)\n"
			"    Configure flex payload selection.\n\n"

			"flow validate {port_id}"
			" [group {group_id}] [priority {level}]"
			" [ingress] [egress]"
			" pattern {item} [/ {item} [...]] / end"
			" actions {action} [/ {action} [...]] / end\n"
			"    Check whether a flow rule can be created.\n\n"

			"flow create {port_id}"
			" [group {group_id}] [priority {level}]"
			" [ingress] [egress]"
			" pattern {item} [/ {item} [...]] / end"
			" actions {action} [/ {action} [...]] / end\n"
			"    Create a flow rule.\n\n"

			"flow destroy {port_id} rule {rule_id} [...]\n"
			"    Destroy specific flow rules.\n\n"

			"flow flush {port_id}\n"
			"    Destroy all flow rules.\n\n"

			"flow query {port_id} {rule_id} {action}\n"
			"    Query an existing flow rule.\n\n"

			"flow list {port_id} [group {group_id}] [...]\n"
			"    List existing flow rules sorted by priority,"
			" filtered by group identifiers.\n\n"

			"flow isolate {port_id} {boolean}\n"
			"    Restrict ingress traffic to the defined"
			" flow rules\n\n"

			"flow aged {port_id} [destroy]\n"
			"    List and destroy aged flows"
			" flow rules\n\n"

			"flow indirect_action {port_id} create"
			" [action_id {indirect_action_id}]"
			" [ingress] [egress]"
			" action {action} / end\n"
			"    Create indirect action.\n\n"

			"flow indirect_action {port_id} update"
			" {indirect_action_id} action {action} / end\n"
			"    Update indirect action.\n\n"

			"flow indirect_action {port_id} destroy"
			" action_id {indirect_action_id} [...]\n"
			"    Destroy specific indirect actions.\n\n"

			"flow indirect_action {port_id} query"
			" {indirect_action_id}\n"
			"    Query an existing indirect action.\n\n"

			"set vxlan ip-version (ipv4|ipv6) vni (vni) udp-src"
			" (udp-src) udp-dst (udp-dst) ip-src (ip-src) ip-dst"
			" (ip-dst) eth-src (eth-src) eth-dst (eth-dst)\n"
			"       Configure the VXLAN encapsulation for flows.\n\n"

			"set vxlan-with-vlan ip-version (ipv4|ipv6) vni (vni)"
			" udp-src (udp-src) udp-dst (udp-dst) ip-src (ip-src)"
			" ip-dst (ip-dst) vlan-tci (vlan-tci) eth-src (eth-src)"
			" eth-dst (eth-dst)\n"
			"       Configure the VXLAN encapsulation for flows.\n\n"

			"set vxlan-tos-ttl ip-version (ipv4|ipv6) vni (vni) udp-src"
			" (udp-src) udp-dst (udp-dst) ip-tos (ip-tos) ip-ttl (ip-ttl)"
			" ip-src (ip-src) ip-dst (ip-dst) eth-src (eth-src)"
			" eth-dst (eth-dst)\n"
			"       Configure the VXLAN encapsulation for flows.\n\n"

			"set nvgre ip-version (ipv4|ipv6) tni (tni) ip-src"
			" (ip-src) ip-dst (ip-dst) eth-src (eth-src) eth-dst"
			" (eth-dst)\n"
			"       Configure the NVGRE encapsulation for flows.\n\n"

			"set nvgre-with-vlan ip-version (ipv4|ipv6) tni (tni)"
			" ip-src (ip-src) ip-dst (ip-dst) vlan-tci (vlan-tci)"
			" eth-src (eth-src) eth-dst (eth-dst)\n"
			"       Configure the NVGRE encapsulation for flows.\n\n"

			"set raw_encap {flow items}\n"
			"	Configure the encapsulation with raw data.\n\n"

			"set raw_decap {flow items}\n"
			"	Configure the decapsulation with raw data.\n\n"

		);
	}

	if (show_all || !strcmp(res->section, "traffic_management")) {
		cmdline_printf(
			cl,
			"\n"
			"Traffic Management:\n"
			"--------------\n"
			"show port tm cap (port_id)\n"
			"       Display the port TM capability.\n\n"

			"show port tm level cap (port_id) (level_id)\n"
			"       Display the port TM hierarchical level capability.\n\n"

			"show port tm node cap (port_id) (node_id)\n"
			"       Display the port TM node capability.\n\n"

			"show port tm node type (port_id) (node_id)\n"
			"       Display the port TM node type.\n\n"

			"show port tm node stats (port_id) (node_id) (clear)\n"
			"       Display the port TM node stats.\n\n"

			"add port tm node shaper profile (port_id) (shaper_profile_id)"
			" (cmit_tb_rate) (cmit_tb_size) (peak_tb_rate) (peak_tb_size)"
			" (packet_length_adjust) (packet_mode)\n"
			"       Add port tm node private shaper profile.\n\n"

			"del port tm node shaper profile (port_id) (shaper_profile_id)\n"
			"       Delete port tm node private shaper profile.\n\n"

			"add port tm node shared shaper (port_id) (shared_shaper_id)"
			" (shaper_profile_id)\n"
			"       Add/update port tm node shared shaper.\n\n"

			"del port tm node shared shaper (port_id) (shared_shaper_id)\n"
			"       Delete port tm node shared shaper.\n\n"

			"set port tm node shaper profile (port_id) (node_id)"
			" (shaper_profile_id)\n"
			"       Set port tm node shaper profile.\n\n"

			"add port tm node wred profile (port_id) (wred_profile_id)"
			" (color_g) (min_th_g) (max_th_g) (maxp_inv_g) (wq_log2_g)"
			" (color_y) (min_th_y) (max_th_y) (maxp_inv_y) (wq_log2_y)"
			" (color_r) (min_th_r) (max_th_r) (maxp_inv_r) (wq_log2_r)\n"
			"       Add port tm node wred profile.\n\n"

			"del port tm node wred profile (port_id) (wred_profile_id)\n"
			"       Delete port tm node wred profile.\n\n"

			"add port tm nonleaf node (port_id) (node_id) (parent_node_id)"
			" (priority) (weight) (level_id) (shaper_profile_id)"
			" (n_sp_priorities) (stats_mask) (n_shared_shapers)"
			" [(shared_shaper_id_0) (shared_shaper_id_1)...]\n"
			"       Add port tm nonleaf node.\n\n"

			"add port tm nonleaf node pktmode (port_id) (node_id) (parent_node_id)"
			" (priority) (weight) (level_id) (shaper_profile_id)"
			" (n_sp_priorities) (stats_mask) (n_shared_shapers)"
			" [(shared_shaper_id_0) (shared_shaper_id_1)...]\n"
			"       Add port tm nonleaf node with pkt mode enabled.\n\n"

			"add port tm leaf node (port_id) (node_id) (parent_node_id)"
			" (priority) (weight) (level_id) (shaper_profile_id)"
			" (cman_mode) (wred_profile_id) (stats_mask) (n_shared_shapers)"
			" [(shared_shaper_id_0) (shared_shaper_id_1)...]\n"
			"       Add port tm leaf node.\n\n"

			"del port tm node (port_id) (node_id)\n"
			"       Delete port tm node.\n\n"

			"set port tm node parent (port_id) (node_id) (parent_node_id)"
			" (priority) (weight)\n"
			"       Set port tm node parent.\n\n"

			"suspend port tm node (port_id) (node_id)"
			"       Suspend tm node.\n\n"

			"resume port tm node (port_id) (node_id)"
			"       Resume tm node.\n\n"

			"port tm hierarchy commit (port_id) (clean_on_fail)\n"
			"       Commit tm hierarchy.\n\n"

			"set port tm mark ip_ecn (port) (green) (yellow)"
			" (red)\n"
			"    Enables/Disables the traffic management marking"
			" for IP ECN (Explicit Congestion Notification)"
			" packets on a given port\n\n"

			"set port tm mark ip_dscp (port) (green) (yellow)"
			" (red)\n"
			"    Enables/Disables the traffic management marking"
			" on the port for IP dscp packets\n\n"

			"set port tm mark vlan_dei (port) (green) (yellow)"
			" (red)\n"
			"    Enables/Disables the traffic management marking"
			" on the port for VLAN packets with DEI enabled\n\n"
		);
	}

	if (show_all || !strcmp(res->section, "devices")) {
		cmdline_printf(
			cl,
			"\n"
			"Device Operations:\n"
			"--------------\n"
			"device detach (identifier)\n"
			"       Detach device by identifier.\n\n"
		);
	}

}

cmdline_parse_token_string_t cmd_help_long_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_long_result, help, "help");

cmdline_parse_token_string_t cmd_help_long_section =
	TOKEN_STRING_INITIALIZER(struct cmd_help_long_result, section,
			"all#control#display#config#"
			"ports#registers#filters#traffic_management#devices");

cmdline_parse_inst_t cmd_help_long = {
	.f = cmd_help_long_parsed,
	.data = NULL,
	.help_str = "help all|control|display|config|ports|register|"
		"filters|traffic_management|devices: "
		"Show help",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_operate_port_result *res = parsed_result;

	if (!strcmp(res->name, "start"))
		start_port(RTE_PORT_ALL);
	else if (!strcmp(res->name, "stop"))
		stop_port(RTE_PORT_ALL);
	else if (!strcmp(res->name, "close"))
		close_port(RTE_PORT_ALL);
	else if (!strcmp(res->name, "reset"))
		reset_port(RTE_PORT_ALL);
	else
		fprintf(stderr, "Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_port_all_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, keyword,
								"port");
cmdline_parse_token_string_t cmd_operate_port_all_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, name,
						"start#stop#close#reset");
cmdline_parse_token_string_t cmd_operate_port_all_all =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_port_result, value, "all");

cmdline_parse_inst_t cmd_operate_port = {
	.f = cmd_operate_port_parsed,
	.data = NULL,
	.help_str = "port start|stop|close|reset all: Start/Stop/Close/Reset all ports",
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
			__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_operate_specific_port_result *res = parsed_result;

	if (!strcmp(res->name, "start"))
		start_port(res->value);
	else if (!strcmp(res->name, "stop"))
		stop_port(res->value);
	else if (!strcmp(res->name, "close"))
		close_port(res->value);
	else if (!strcmp(res->name, "reset"))
		reset_port(res->value);
	else
		fprintf(stderr, "Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_specific_port_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_specific_port_result,
							keyword, "port");
cmdline_parse_token_string_t cmd_operate_specific_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_specific_port_result,
						name, "start#stop#close#reset");
cmdline_parse_token_num_t cmd_operate_specific_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_operate_specific_port_result,
							value, RTE_UINT8);

cmdline_parse_inst_t cmd_operate_specific_port = {
	.f = cmd_operate_specific_port_parsed,
	.data = NULL,
	.help_str = "port start|stop|close|reset <port_id>: Start/Stop/Close/Reset port_id",
	.tokens = {
		(void *)&cmd_operate_specific_port_cmd,
		(void *)&cmd_operate_specific_port_port,
		(void *)&cmd_operate_specific_port_id,
		NULL,
	},
};

/* *** enable port setup (after attach) via iterator or event *** */
struct cmd_set_port_setup_on_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t setup;
	cmdline_fixed_string_t on;
	cmdline_fixed_string_t mode;
};

static void cmd_set_port_setup_on_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_set_port_setup_on_result *res = parsed_result;

	if (strcmp(res->mode, "event") == 0)
		setup_on_probe_event = true;
	else if (strcmp(res->mode, "iterator") == 0)
		setup_on_probe_event = false;
	else
		fprintf(stderr, "Unknown mode\n");
}

cmdline_parse_token_string_t cmd_set_port_setup_on_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_setup_on_result,
			set, "set");
cmdline_parse_token_string_t cmd_set_port_setup_on_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_setup_on_result,
			port, "port");
cmdline_parse_token_string_t cmd_set_port_setup_on_setup =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_setup_on_result,
			setup, "setup");
cmdline_parse_token_string_t cmd_set_port_setup_on_on =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_setup_on_result,
			on, "on");
cmdline_parse_token_string_t cmd_set_port_setup_on_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_port_setup_on_result,
			mode, "iterator#event");

cmdline_parse_inst_t cmd_set_port_setup_on = {
	.f = cmd_set_port_setup_on_parsed,
	.data = NULL,
	.help_str = "set port setup on iterator|event",
	.tokens = {
		(void *)&cmd_set_port_setup_on_set,
		(void *)&cmd_set_port_setup_on_port,
		(void *)&cmd_set_port_setup_on_setup,
		(void *)&cmd_set_port_setup_on_on,
		(void *)&cmd_set_port_setup_on_mode,
		NULL,
	},
};

/* *** attach a specified port *** */
struct cmd_operate_attach_port_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_multi_string_t identifier;
};

static void cmd_operate_attach_port_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_operate_attach_port_result *res = parsed_result;

	if (!strcmp(res->keyword, "attach"))
		attach_port(res->identifier);
	else
		fprintf(stderr, "Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_attach_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			port, "port");
cmdline_parse_token_string_t cmd_operate_attach_port_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			keyword, "attach");
cmdline_parse_token_string_t cmd_operate_attach_port_identifier =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_attach_port_result,
			identifier, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_operate_attach_port = {
	.f = cmd_operate_attach_port_parsed,
	.data = NULL,
	.help_str = "port attach <identifier>: "
		"(identifier: pci address or virtual dev name)",
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
	portid_t port_id;
};

static void cmd_operate_detach_port_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_operate_detach_port_result *res = parsed_result;

	if (!strcmp(res->keyword, "detach")) {
		RTE_ETH_VALID_PORTID_OR_RET(res->port_id);
		detach_port_device(res->port_id);
	} else {
		fprintf(stderr, "Unknown parameter\n");
	}
}

cmdline_parse_token_string_t cmd_operate_detach_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_port_result,
			port, "port");
cmdline_parse_token_string_t cmd_operate_detach_port_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_port_result,
			keyword, "detach");
cmdline_parse_token_num_t cmd_operate_detach_port_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_operate_detach_port_result,
			port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_operate_detach_port = {
	.f = cmd_operate_detach_port_parsed,
	.data = NULL,
	.help_str = "port detach <port_id>",
	.tokens = {
		(void *)&cmd_operate_detach_port_port,
		(void *)&cmd_operate_detach_port_keyword,
		(void *)&cmd_operate_detach_port_port_id,
		NULL,
	},
};

/* *** detach device by identifier *** */
struct cmd_operate_detach_device_result {
	cmdline_fixed_string_t device;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t identifier;
};

static void cmd_operate_detach_device_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_operate_detach_device_result *res = parsed_result;

	if (!strcmp(res->keyword, "detach"))
		detach_devargs(res->identifier);
	else
		fprintf(stderr, "Unknown parameter\n");
}

cmdline_parse_token_string_t cmd_operate_detach_device_device =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_device_result,
			device, "device");
cmdline_parse_token_string_t cmd_operate_detach_device_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_device_result,
			keyword, "detach");
cmdline_parse_token_string_t cmd_operate_detach_device_identifier =
	TOKEN_STRING_INITIALIZER(struct cmd_operate_detach_device_result,
			identifier, NULL);

cmdline_parse_inst_t cmd_operate_detach_device = {
	.f = cmd_operate_detach_device_parsed,
	.data = NULL,
	.help_str = "device detach <identifier>:"
		"(identifier: pci address or virtual dev name)",
	.tokens = {
		(void *)&cmd_operate_detach_device_device,
		(void *)&cmd_operate_detach_device_keyword,
		(void *)&cmd_operate_detach_device_identifier,
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
		duplex = RTE_ETH_LINK_HALF_DUPLEX;
	} else if (!strcmp(duplexstr, "full")) {
		duplex = RTE_ETH_LINK_FULL_DUPLEX;
	} else if (!strcmp(duplexstr, "auto")) {
		duplex = RTE_ETH_LINK_FULL_DUPLEX;
	} else {
		fprintf(stderr, "Unknown duplex parameter\n");
		return -1;
	}

	if (!strcmp(speedstr, "10")) {
		*speed = (duplex == RTE_ETH_LINK_HALF_DUPLEX) ?
				RTE_ETH_LINK_SPEED_10M_HD : RTE_ETH_LINK_SPEED_10M;
	} else if (!strcmp(speedstr, "100")) {
		*speed = (duplex == RTE_ETH_LINK_HALF_DUPLEX) ?
				RTE_ETH_LINK_SPEED_100M_HD : RTE_ETH_LINK_SPEED_100M;
	} else {
		if (duplex != RTE_ETH_LINK_FULL_DUPLEX) {
			fprintf(stderr, "Invalid speed/duplex parameters\n");
			return -1;
		}
		if (!strcmp(speedstr, "1000")) {
			*speed = RTE_ETH_LINK_SPEED_1G;
		} else if (!strcmp(speedstr, "10000")) {
			*speed = RTE_ETH_LINK_SPEED_10G;
		} else if (!strcmp(speedstr, "25000")) {
			*speed = RTE_ETH_LINK_SPEED_25G;
		} else if (!strcmp(speedstr, "40000")) {
			*speed = RTE_ETH_LINK_SPEED_40G;
		} else if (!strcmp(speedstr, "50000")) {
			*speed = RTE_ETH_LINK_SPEED_50G;
		} else if (!strcmp(speedstr, "100000")) {
			*speed = RTE_ETH_LINK_SPEED_100G;
		} else if (!strcmp(speedstr, "200000")) {
			*speed = RTE_ETH_LINK_SPEED_200G;
		} else if (!strcmp(speedstr, "auto")) {
			*speed = RTE_ETH_LINK_SPEED_AUTONEG;
		} else {
			fprintf(stderr, "Unknown speed parameter\n");
			return -1;
		}
	}

	if (*speed != RTE_ETH_LINK_SPEED_AUTONEG)
		*speed |= RTE_ETH_LINK_SPEED_FIXED;

	return 0;
}

static void
cmd_config_speed_all_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_speed_all *res = parsed_result;
	uint32_t link_speed;
	portid_t pid;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (parse_and_check_speed_duplex(res->value1, res->value2,
			&link_speed) < 0)
		return;

	RTE_ETH_FOREACH_DEV(pid) {
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
				"10#100#1000#10000#25000#40000#50000#100000#200000#auto");
cmdline_parse_token_string_t cmd_config_speed_all_item2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, item2, "duplex");
cmdline_parse_token_string_t cmd_config_speed_all_value2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_all, value2,
						"half#full#auto");

cmdline_parse_inst_t cmd_config_speed_all = {
	.f = cmd_config_speed_all_parsed,
	.data = NULL,
	.help_str = "port config all speed "
		"10|100|1000|10000|25000|40000|50000|100000|200000|auto duplex "
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
	portid_t id;
	cmdline_fixed_string_t item1;
	cmdline_fixed_string_t item2;
	cmdline_fixed_string_t value1;
	cmdline_fixed_string_t value2;
};

static void
cmd_config_speed_specific_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_speed_specific *res = parsed_result;
	uint32_t link_speed;

	if (port_id_is_invalid(res->id, ENABLED_WARN))
		return;

	if (!port_is_stopped(res->id)) {
		fprintf(stderr, "Please stop port %d first\n", res->id);
		return;
	}

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
	TOKEN_NUM_INITIALIZER(struct cmd_config_speed_specific, id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_speed_specific_item1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, item1,
								"speed");
cmdline_parse_token_string_t cmd_config_speed_specific_value1 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, value1,
				"10#100#1000#10000#25000#40000#50000#100000#200000#auto");
cmdline_parse_token_string_t cmd_config_speed_specific_item2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, item2,
								"duplex");
cmdline_parse_token_string_t cmd_config_speed_specific_value2 =
	TOKEN_STRING_INITIALIZER(struct cmd_config_speed_specific, value2,
							"half#full#auto");

cmdline_parse_inst_t cmd_config_speed_specific = {
	.f = cmd_config_speed_specific_parsed,
	.data = NULL,
	.help_str = "port config <port_id> speed "
		"10|100|1000|10000|25000|40000|50000|100000|200000|auto duplex "
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

/* *** configure loopback for all ports *** */
struct cmd_config_loopback_all {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t item;
	uint32_t mode;
};

static void
cmd_config_loopback_all_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_loopback_all *res = parsed_result;
	portid_t pid;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	RTE_ETH_FOREACH_DEV(pid) {
		ports[pid].dev_conf.lpbk_mode = res->mode;
	}

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_loopback_all_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_all, port, "port");
cmdline_parse_token_string_t cmd_config_loopback_all_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_all, keyword,
							"config");
cmdline_parse_token_string_t cmd_config_loopback_all_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_all, all, "all");
cmdline_parse_token_string_t cmd_config_loopback_all_item =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_all, item,
							"loopback");
cmdline_parse_token_num_t cmd_config_loopback_all_mode =
	TOKEN_NUM_INITIALIZER(struct cmd_config_loopback_all, mode, RTE_UINT32);

cmdline_parse_inst_t cmd_config_loopback_all = {
	.f = cmd_config_loopback_all_parsed,
	.data = NULL,
	.help_str = "port config all loopback <mode>",
	.tokens = {
		(void *)&cmd_config_loopback_all_port,
		(void *)&cmd_config_loopback_all_keyword,
		(void *)&cmd_config_loopback_all_all,
		(void *)&cmd_config_loopback_all_item,
		(void *)&cmd_config_loopback_all_mode,
		NULL,
	},
};

/* *** configure loopback for specific port *** */
struct cmd_config_loopback_specific {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint16_t port_id;
	cmdline_fixed_string_t item;
	uint32_t mode;
};

static void
cmd_config_loopback_specific_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_loopback_specific *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %u first\n", res->port_id);
		return;
	}

	ports[res->port_id].dev_conf.lpbk_mode = res->mode;

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}


cmdline_parse_token_string_t cmd_config_loopback_specific_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_specific, port,
								"port");
cmdline_parse_token_string_t cmd_config_loopback_specific_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_specific, keyword,
								"config");
cmdline_parse_token_num_t cmd_config_loopback_specific_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_loopback_specific, port_id,
								RTE_UINT16);
cmdline_parse_token_string_t cmd_config_loopback_specific_item =
	TOKEN_STRING_INITIALIZER(struct cmd_config_loopback_specific, item,
								"loopback");
cmdline_parse_token_num_t cmd_config_loopback_specific_mode =
	TOKEN_NUM_INITIALIZER(struct cmd_config_loopback_specific, mode,
			      RTE_UINT32);

cmdline_parse_inst_t cmd_config_loopback_specific = {
	.f = cmd_config_loopback_specific_parsed,
	.data = NULL,
	.help_str = "port config <port_id> loopback <mode>",
	.tokens = {
		(void *)&cmd_config_loopback_specific_port,
		(void *)&cmd_config_loopback_specific_keyword,
		(void *)&cmd_config_loopback_specific_id,
		(void *)&cmd_config_loopback_specific_item,
		(void *)&cmd_config_loopback_specific_mode,
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
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_rx_tx *res = parsed_result;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}
	if (!strcmp(res->name, "rxq")) {
		if (!res->value && !nb_txq) {
			fprintf(stderr, "Warning: Either rx or tx queues should be non zero\n");
			return;
		}
		if (check_nb_rxq(res->value) != 0)
			return;
		nb_rxq = res->value;
	}
	else if (!strcmp(res->name, "txq")) {
		if (!res->value && !nb_rxq) {
			fprintf(stderr, "Warning: Either rx or tx queues should be non zero\n");
			return;
		}
		if (check_nb_txq(res->value) != 0)
			return;
		nb_txq = res->value;
	}
	else if (!strcmp(res->name, "rxd")) {
		if (check_nb_rxd(res->value) != 0)
			return;
		nb_rxd = res->value;
	} else if (!strcmp(res->name, "txd")) {
		if (check_nb_txd(res->value) != 0)
			return;

		nb_txd = res->value;
	} else {
		fprintf(stderr, "Unknown parameter\n");
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
	TOKEN_NUM_INITIALIZER(struct cmd_config_rx_tx, value, RTE_UINT16);

cmdline_parse_inst_t cmd_config_rx_tx = {
	.f = cmd_config_rx_tx_parsed,
	.data = NULL,
	.help_str = "port config all rxq|txq|rxd|txd <value>",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_max_pkt_len_result *res = parsed_result;
	portid_t port_id;
	int ret;

	if (strcmp(res->name, "max-pkt-len") != 0) {
		printf("Unknown parameter\n");
		return;
	}

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	RTE_ETH_FOREACH_DEV(port_id) {
		struct rte_port *port = &ports[port_id];

		if (res->value < RTE_ETHER_MIN_LEN) {
			fprintf(stderr,
				"max-pkt-len can not be less than %d\n",
				RTE_ETHER_MIN_LEN);
			return;
		}

		ret = eth_dev_info_get_print_err(port_id, &port->dev_info);
		if (ret != 0) {
			fprintf(stderr,
				"rte_eth_dev_info_get() failed for port %u\n",
				port_id);
			return;
		}

		update_mtu_from_frame_size(port_id, res->value);
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
								RTE_UINT32);

cmdline_parse_inst_t cmd_config_max_pkt_len = {
	.f = cmd_config_max_pkt_len_parsed,
	.data = NULL,
	.help_str = "port config all max-pkt-len <value>",
	.tokens = {
		(void *)&cmd_config_max_pkt_len_port,
		(void *)&cmd_config_max_pkt_len_keyword,
		(void *)&cmd_config_max_pkt_len_all,
		(void *)&cmd_config_max_pkt_len_name,
		(void *)&cmd_config_max_pkt_len_value,
		NULL,
	},
};

/* *** config max LRO aggregated packet size *** */
struct cmd_config_max_lro_pkt_size_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t all;
	cmdline_fixed_string_t name;
	uint32_t value;
};

static void
cmd_config_max_lro_pkt_size_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_max_lro_pkt_size_result *res = parsed_result;
	portid_t pid;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_port *port = &ports[pid];

		if (!strcmp(res->name, "max-lro-pkt-size")) {
			if (res->value ==
					port->dev_conf.rxmode.max_lro_pkt_size)
				return;

			port->dev_conf.rxmode.max_lro_pkt_size = res->value;
		} else {
			fprintf(stderr, "Unknown parameter\n");
			return;
		}
	}

	init_port_config();

	cmd_reconfig_device_queue(RTE_PORT_ALL, 1, 1);
}

cmdline_parse_token_string_t cmd_config_max_lro_pkt_size_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_lro_pkt_size_result,
				 port, "port");
cmdline_parse_token_string_t cmd_config_max_lro_pkt_size_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_lro_pkt_size_result,
				 keyword, "config");
cmdline_parse_token_string_t cmd_config_max_lro_pkt_size_all =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_lro_pkt_size_result,
				 all, "all");
cmdline_parse_token_string_t cmd_config_max_lro_pkt_size_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_max_lro_pkt_size_result,
				 name, "max-lro-pkt-size");
cmdline_parse_token_num_t cmd_config_max_lro_pkt_size_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_max_lro_pkt_size_result,
			      value, RTE_UINT32);

cmdline_parse_inst_t cmd_config_max_lro_pkt_size = {
	.f = cmd_config_max_lro_pkt_size_parsed,
	.data = NULL,
	.help_str = "port config all max-lro-pkt-size <value>",
	.tokens = {
		(void *)&cmd_config_max_lro_pkt_size_port,
		(void *)&cmd_config_max_lro_pkt_size_keyword,
		(void *)&cmd_config_max_lro_pkt_size_all,
		(void *)&cmd_config_max_lro_pkt_size_name,
		(void *)&cmd_config_max_lro_pkt_size_value,
		NULL,
	},
};

/* *** configure port MTU *** */
struct cmd_config_mtu_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t mtu;
	portid_t port_id;
	uint16_t value;
};

static void
cmd_config_mtu_parsed(void *parsed_result,
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
{
	struct cmd_config_mtu_result *res = parsed_result;

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
	TOKEN_NUM_INITIALIZER(struct cmd_config_mtu_result, port_id,
				 RTE_UINT16);
cmdline_parse_token_num_t cmd_config_mtu_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_mtu_result, value,
				 RTE_UINT16);

cmdline_parse_inst_t cmd_config_mtu = {
	.f = cmd_config_mtu_parsed,
	.data = NULL,
	.help_str = "port config mtu <port_id> <value>",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_rx_mode_flag *res = parsed_result;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "drop-en")) {
		if (!strcmp(res->value, "on"))
			rx_drop_en = 1;
		else if (!strcmp(res->value, "off"))
			rx_drop_en = 0;
		else {
			fprintf(stderr, "Unknown parameter\n");
			return;
		}
	} else {
		fprintf(stderr, "Unknown parameter\n");
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
					"drop-en");
cmdline_parse_token_string_t cmd_config_rx_mode_flag_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rx_mode_flag, value,
							"on#off");

cmdline_parse_inst_t cmd_config_rx_mode_flag = {
	.f = cmd_config_rx_mode_flag_parsed,
	.data = NULL,
	.help_str = "port config all drop-en on|off",
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
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_rss *res = parsed_result;
	struct rte_eth_rss_conf rss_conf = { .rss_key_len = 0, };
	struct rte_eth_dev_info dev_info = { .flow_type_rss_offloads = 0, };
	int use_default = 0;
	int all_updated = 1;
	int diag;
	uint16_t i;
	int ret;

	if (!strcmp(res->value, "all"))
		rss_conf.rss_hf = RTE_ETH_RSS_ETH | RTE_ETH_RSS_VLAN | RTE_ETH_RSS_IP |
			RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_SCTP |
			RTE_ETH_RSS_L2_PAYLOAD | RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_ESP |
			RTE_ETH_RSS_AH | RTE_ETH_RSS_PFCP | RTE_ETH_RSS_GTPU |
			RTE_ETH_RSS_ECPRI;
	else if (!strcmp(res->value, "eth"))
		rss_conf.rss_hf = RTE_ETH_RSS_ETH;
	else if (!strcmp(res->value, "vlan"))
		rss_conf.rss_hf = RTE_ETH_RSS_VLAN;
	else if (!strcmp(res->value, "ip"))
		rss_conf.rss_hf = RTE_ETH_RSS_IP;
	else if (!strcmp(res->value, "udp"))
		rss_conf.rss_hf = RTE_ETH_RSS_UDP;
	else if (!strcmp(res->value, "tcp"))
		rss_conf.rss_hf = RTE_ETH_RSS_TCP;
	else if (!strcmp(res->value, "sctp"))
		rss_conf.rss_hf = RTE_ETH_RSS_SCTP;
	else if (!strcmp(res->value, "ether"))
		rss_conf.rss_hf = RTE_ETH_RSS_L2_PAYLOAD;
	else if (!strcmp(res->value, "port"))
		rss_conf.rss_hf = RTE_ETH_RSS_PORT;
	else if (!strcmp(res->value, "vxlan"))
		rss_conf.rss_hf = RTE_ETH_RSS_VXLAN;
	else if (!strcmp(res->value, "geneve"))
		rss_conf.rss_hf = RTE_ETH_RSS_GENEVE;
	else if (!strcmp(res->value, "nvgre"))
		rss_conf.rss_hf = RTE_ETH_RSS_NVGRE;
	else if (!strcmp(res->value, "l3-pre32"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE32;
	else if (!strcmp(res->value, "l3-pre40"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE40;
	else if (!strcmp(res->value, "l3-pre48"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE48;
	else if (!strcmp(res->value, "l3-pre56"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE56;
	else if (!strcmp(res->value, "l3-pre64"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE64;
	else if (!strcmp(res->value, "l3-pre96"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_PRE96;
	else if (!strcmp(res->value, "l3-src-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_SRC_ONLY;
	else if (!strcmp(res->value, "l3-dst-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L3_DST_ONLY;
	else if (!strcmp(res->value, "l4-src-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L4_SRC_ONLY;
	else if (!strcmp(res->value, "l4-dst-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L4_DST_ONLY;
	else if (!strcmp(res->value, "l2-src-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L2_SRC_ONLY;
	else if (!strcmp(res->value, "l2-dst-only"))
		rss_conf.rss_hf = RTE_ETH_RSS_L2_DST_ONLY;
	else if (!strcmp(res->value, "l2tpv3"))
		rss_conf.rss_hf = RTE_ETH_RSS_L2TPV3;
	else if (!strcmp(res->value, "esp"))
		rss_conf.rss_hf = RTE_ETH_RSS_ESP;
	else if (!strcmp(res->value, "ah"))
		rss_conf.rss_hf = RTE_ETH_RSS_AH;
	else if (!strcmp(res->value, "pfcp"))
		rss_conf.rss_hf = RTE_ETH_RSS_PFCP;
	else if (!strcmp(res->value, "pppoe"))
		rss_conf.rss_hf = RTE_ETH_RSS_PPPOE;
	else if (!strcmp(res->value, "gtpu"))
		rss_conf.rss_hf = RTE_ETH_RSS_GTPU;
	else if (!strcmp(res->value, "ecpri"))
		rss_conf.rss_hf = RTE_ETH_RSS_ECPRI;
	else if (!strcmp(res->value, "mpls"))
		rss_conf.rss_hf = RTE_ETH_RSS_MPLS;
	else if (!strcmp(res->value, "ipv4-chksum"))
		rss_conf.rss_hf = RTE_ETH_RSS_IPV4_CHKSUM;
	else if (!strcmp(res->value, "none"))
		rss_conf.rss_hf = 0;
	else if (!strcmp(res->value, "level-default")) {
		rss_hf &= (~RTE_ETH_RSS_LEVEL_MASK);
		rss_conf.rss_hf = (rss_hf | RTE_ETH_RSS_LEVEL_PMD_DEFAULT);
	} else if (!strcmp(res->value, "level-outer")) {
		rss_hf &= (~RTE_ETH_RSS_LEVEL_MASK);
		rss_conf.rss_hf = (rss_hf | RTE_ETH_RSS_LEVEL_OUTERMOST);
	} else if (!strcmp(res->value, "level-inner")) {
		rss_hf &= (~RTE_ETH_RSS_LEVEL_MASK);
		rss_conf.rss_hf = (rss_hf | RTE_ETH_RSS_LEVEL_INNERMOST);
	} else if (!strcmp(res->value, "default"))
		use_default = 1;
	else if (isdigit(res->value[0]) && atoi(res->value) > 0 &&
						atoi(res->value) < 64)
		rss_conf.rss_hf = 1ULL << atoi(res->value);
	else {
		fprintf(stderr, "Unknown parameter\n");
		return;
	}
	rss_conf.rss_key = NULL;
	/* Update global configuration for RSS types. */
	RTE_ETH_FOREACH_DEV(i) {
		struct rte_eth_rss_conf local_rss_conf;

		ret = eth_dev_info_get_print_err(i, &dev_info);
		if (ret != 0)
			return;

		if (use_default)
			rss_conf.rss_hf = dev_info.flow_type_rss_offloads;

		local_rss_conf = rss_conf;
		local_rss_conf.rss_hf = rss_conf.rss_hf &
			dev_info.flow_type_rss_offloads;
		if (local_rss_conf.rss_hf != rss_conf.rss_hf) {
			printf("Port %u modified RSS hash function based on hardware support,"
				"requested:%#"PRIx64" configured:%#"PRIx64"\n",
				i, rss_conf.rss_hf, local_rss_conf.rss_hf);
		}
		diag = rte_eth_dev_rss_hash_update(i, &local_rss_conf);
		if (diag < 0) {
			all_updated = 0;
			fprintf(stderr,
				"Configuration of RSS hash at ethernet port %d failed with error (%d): %s.\n",
				i, -diag, strerror(-diag));
		}
	}
	if (all_updated && !use_default) {
		rss_hf = rss_conf.rss_hf;
		printf("rss_hf %#"PRIx64"\n", rss_hf);
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
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss, value, NULL);

cmdline_parse_inst_t cmd_config_rss = {
	.f = cmd_config_rss_parsed,
	.data = NULL,
	.help_str = "port config all rss "
		"all|default|eth|vlan|ip|tcp|udp|sctp|ether|port|vxlan|geneve|"
		"nvgre|vxlan-gpe|l2tpv3|esp|ah|pfcp|ecpri|mpls|none|level-default|"
		"level-outer|level-inner|ipv4-chksum|<flowtype_id>",
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
	portid_t port_id;
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
		fprintf(stderr,
			"invalid key: character %c at position %d is not a valid hexa digit\n",
			key[idx], idx);
	return hexa_v;
}

static void
cmd_config_rss_hash_key_parsed(void *parsed_result,
			       __rte_unused struct cmdline *cl,
			       __rte_unused void *data)
{
	struct cmd_config_rss_hash_key *res = parsed_result;
	uint8_t hash_key[RSS_HASH_KEY_LENGTH];
	uint8_t xdgt0;
	uint8_t xdgt1;
	int i;
	struct rte_eth_dev_info dev_info;
	uint8_t hash_key_size;
	uint32_t key_len;
	int ret;

	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	if (dev_info.hash_key_size > 0 &&
			dev_info.hash_key_size <= sizeof(hash_key))
		hash_key_size = dev_info.hash_key_size;
	else {
		fprintf(stderr,
			"dev_info did not provide a valid hash key size\n");
		return;
	}
	/* Check the length of the RSS hash key */
	key_len = strlen(res->key);
	if (key_len != (hash_key_size * 2)) {
		fprintf(stderr,
			"key length: %d invalid - key must be a string of %d hexa-decimal numbers\n",
			(int)key_len, hash_key_size * 2);
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
	TOKEN_NUM_INITIALIZER(struct cmd_config_rss_hash_key, port_id,
				 RTE_UINT16);
cmdline_parse_token_string_t cmd_config_rss_hash_key_rss_hash_key =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key,
				 rss_hash_key, "rss-hash-key");
cmdline_parse_token_string_t cmd_config_rss_hash_key_rss_type =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, rss_type,
				 "ipv4#ipv4-frag#ipv4-tcp#ipv4-udp#ipv4-sctp#"
				 "ipv4-other#ipv6#ipv6-frag#ipv6-tcp#ipv6-udp#"
				 "ipv6-sctp#ipv6-other#l2-payload#ipv6-ex#"
				 "ipv6-tcp-ex#ipv6-udp-ex#"
				 "l3-src-only#l3-dst-only#l4-src-only#l4-dst-only#"
				 "l2-src-only#l2-dst-only#s-vlan#c-vlan#"
				 "l2tpv3#esp#ah#pfcp#pppoe#gtpu#ecpri#mpls");
cmdline_parse_token_string_t cmd_config_rss_hash_key_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_hash_key, key, NULL);

cmdline_parse_inst_t cmd_config_rss_hash_key = {
	.f = cmd_config_rss_hash_key_parsed,
	.data = NULL,
	.help_str = "port config <port_id> rss-hash-key "
		"ipv4|ipv4-frag|ipv4-tcp|ipv4-udp|ipv4-sctp|ipv4-other|"
		"ipv6|ipv6-frag|ipv6-tcp|ipv6-udp|ipv6-sctp|ipv6-other|"
		"l2-payload|ipv6-ex|ipv6-tcp-ex|ipv6-udp-ex|"
		"l3-src-only|l3-dst-only|l4-src-only|l4-dst-only|"
		"l2-src-only|l2-dst-only|s-vlan|c-vlan|"
		"l2tpv3|esp|ah|pfcp|pppoe|gtpu|ecpri|mpls "
		"<string of hex digits (variable length, NIC dependent)>",
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

/* *** cleanup txq mbufs *** */
struct cmd_cleanup_txq_mbufs_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	uint16_t port_id;
	uint16_t queue_id;
	uint32_t free_cnt;
};

static void
cmd_cleanup_txq_mbufs_parsed(void *parsed_result,
			     __rte_unused struct cmdline *cl,
			     __rte_unused void *data)
{
	struct cmd_cleanup_txq_mbufs_result *res = parsed_result;
	uint16_t port_id = res->port_id;
	uint16_t queue_id = res->queue_id;
	uint32_t free_cnt = res->free_cnt;
	struct rte_eth_txq_info qinfo;
	int ret;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
		return;
	}

	if (rte_eth_tx_queue_info_get(port_id, queue_id, &qinfo)) {
		fprintf(stderr, "Failed to get port %u Tx queue %u info\n",
			port_id, queue_id);
		return;
	}

	if (qinfo.queue_state != RTE_ETH_QUEUE_STATE_STARTED) {
		fprintf(stderr, "Tx queue %u not started\n", queue_id);
		return;
	}

	ret = rte_eth_tx_done_cleanup(port_id, queue_id, free_cnt);
	if (ret < 0) {
		fprintf(stderr,
			"Failed to cleanup mbuf for port %u Tx queue %u error desc: %s(%d)\n",
			port_id, queue_id, strerror(-ret), ret);
		return;
	}

	printf("Cleanup port %u Tx queue %u mbuf nums: %u\n",
	       port_id, queue_id, ret);
}

cmdline_parse_token_string_t cmd_cleanup_txq_mbufs_port =
	TOKEN_STRING_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, port,
				 "port");
cmdline_parse_token_string_t cmd_cleanup_txq_mbufs_cleanup =
	TOKEN_STRING_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, keyword,
				 "cleanup");
cmdline_parse_token_num_t cmd_cleanup_txq_mbufs_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_cleanup_txq_mbufs_txq =
	TOKEN_STRING_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, name,
				 "txq");
cmdline_parse_token_num_t cmd_cleanup_txq_mbufs_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, queue_id,
			      RTE_UINT16);
cmdline_parse_token_num_t cmd_cleanup_txq_mbufs_free_cnt =
	TOKEN_NUM_INITIALIZER(struct cmd_cleanup_txq_mbufs_result, free_cnt,
			      RTE_UINT32);

cmdline_parse_inst_t cmd_cleanup_txq_mbufs = {
	.f = cmd_cleanup_txq_mbufs_parsed,
	.data = NULL,
	.help_str = "port cleanup <port_id> txq <queue_id> <free_cnt>",
	.tokens = {
		(void *)&cmd_cleanup_txq_mbufs_port,
		(void *)&cmd_cleanup_txq_mbufs_cleanup,
		(void *)&cmd_cleanup_txq_mbufs_port_id,
		(void *)&cmd_cleanup_txq_mbufs_txq,
		(void *)&cmd_cleanup_txq_mbufs_queue_id,
		(void *)&cmd_cleanup_txq_mbufs_free_cnt,
		NULL,
	},
};

/* *** configure port rxq/txq ring size *** */
struct cmd_config_rxtx_ring_size {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t portid;
	cmdline_fixed_string_t rxtxq;
	uint16_t qid;
	cmdline_fixed_string_t rsize;
	uint16_t size;
};

static void
cmd_config_rxtx_ring_size_parsed(void *parsed_result,
				 __rte_unused struct cmdline *cl,
				 __rte_unused void *data)
{
	struct cmd_config_rxtx_ring_size *res = parsed_result;
	struct rte_port *port;
	uint8_t isrx;

	if (port_id_is_invalid(res->portid, ENABLED_WARN))
		return;

	if (res->portid == (portid_t)RTE_PORT_ALL) {
		fprintf(stderr, "Invalid port id\n");
		return;
	}

	port = &ports[res->portid];

	if (!strcmp(res->rxtxq, "rxq"))
		isrx = 1;
	else if (!strcmp(res->rxtxq, "txq"))
		isrx = 0;
	else {
		fprintf(stderr, "Unknown parameter\n");
		return;
	}

	if (isrx && rx_queue_id_is_invalid(res->qid))
		return;
	else if (!isrx && tx_queue_id_is_invalid(res->qid))
		return;

	if (isrx && res->size != 0 && res->size <= rx_free_thresh) {
		fprintf(stderr,
			"Invalid rx ring_size, must > rx_free_thresh: %d\n",
			rx_free_thresh);
		return;
	}

	if (isrx)
		port->nb_rx_desc[res->qid] = res->size;
	else
		port->nb_tx_desc[res->qid] = res->size;

	cmd_reconfig_device_queue(res->portid, 0, 1);
}

cmdline_parse_token_string_t cmd_config_rxtx_ring_size_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_ring_size,
				 port, "port");
cmdline_parse_token_string_t cmd_config_rxtx_ring_size_config =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_ring_size,
				 config, "config");
cmdline_parse_token_num_t cmd_config_rxtx_ring_size_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_ring_size,
				 portid, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_rxtx_ring_size_rxtxq =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_ring_size,
				 rxtxq, "rxq#txq");
cmdline_parse_token_num_t cmd_config_rxtx_ring_size_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_ring_size,
			      qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_rxtx_ring_size_rsize =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_ring_size,
				 rsize, "ring_size");
cmdline_parse_token_num_t cmd_config_rxtx_ring_size_size =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_ring_size,
			      size, RTE_UINT16);

cmdline_parse_inst_t cmd_config_rxtx_ring_size = {
	.f = cmd_config_rxtx_ring_size_parsed,
	.data = NULL,
	.help_str = "port config <port_id> rxq|txq <queue_id> ring_size <value>",
	.tokens = {
		(void *)&cmd_config_rxtx_ring_size_port,
		(void *)&cmd_config_rxtx_ring_size_config,
		(void *)&cmd_config_rxtx_ring_size_portid,
		(void *)&cmd_config_rxtx_ring_size_rxtxq,
		(void *)&cmd_config_rxtx_ring_size_qid,
		(void *)&cmd_config_rxtx_ring_size_rsize,
		(void *)&cmd_config_rxtx_ring_size_size,
		NULL,
	},
};

/* *** configure port rxq/txq start/stop *** */
struct cmd_config_rxtx_queue {
	cmdline_fixed_string_t port;
	portid_t portid;
	cmdline_fixed_string_t rxtxq;
	uint16_t qid;
	cmdline_fixed_string_t opname;
};

static void
cmd_config_rxtx_queue_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_rxtx_queue *res = parsed_result;
	struct rte_port *port;
	uint8_t isrx;
	uint8_t isstart;
	uint8_t *state;
	int ret = 0;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
		return;
	}

	if (port_id_is_invalid(res->portid, ENABLED_WARN))
		return;

	if (port_is_started(res->portid) != 1) {
		fprintf(stderr, "Please start port %u first\n", res->portid);
		return;
	}

	if (!strcmp(res->rxtxq, "rxq"))
		isrx = 1;
	else if (!strcmp(res->rxtxq, "txq"))
		isrx = 0;
	else {
		fprintf(stderr, "Unknown parameter\n");
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
		fprintf(stderr, "Unknown parameter\n");
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

	if (ret == -ENOTSUP) {
		fprintf(stderr, "Function not supported in PMD\n");
		return;
	}

	port = &ports[res->portid];
	state = isrx ? &port->rxq[res->qid].state : &port->txq[res->qid].state;
	*state = isstart ? RTE_ETH_QUEUE_STATE_STARTED :
			   RTE_ETH_QUEUE_STATE_STOPPED;
}

cmdline_parse_token_string_t cmd_config_rxtx_queue_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, port, "port");
cmdline_parse_token_num_t cmd_config_rxtx_queue_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_queue, portid, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_rxtx_queue_rxtxq =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, rxtxq, "rxq#txq");
cmdline_parse_token_num_t cmd_config_rxtx_queue_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rxtx_queue, qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_rxtx_queue_opname =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rxtx_queue, opname,
						"start#stop");

cmdline_parse_inst_t cmd_config_rxtx_queue = {
	.f = cmd_config_rxtx_queue_parsed,
	.data = NULL,
	.help_str = "port <port_id> rxq|txq <queue_id> start|stop",
	.tokens = {
		(void *)&cmd_config_rxtx_queue_port,
		(void *)&cmd_config_rxtx_queue_portid,
		(void *)&cmd_config_rxtx_queue_rxtxq,
		(void *)&cmd_config_rxtx_queue_qid,
		(void *)&cmd_config_rxtx_queue_opname,
		NULL,
	},
};

/* *** configure port rxq/txq deferred start on/off *** */
struct cmd_config_deferred_start_rxtx_queue {
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t rxtxq;
	uint16_t qid;
	cmdline_fixed_string_t opname;
	cmdline_fixed_string_t state;
};

static void
cmd_config_deferred_start_rxtx_queue_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_deferred_start_rxtx_queue *res = parsed_result;
	struct rte_port *port;
	uint8_t isrx;
	uint8_t ison;
	uint8_t needreconfig = 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (port_is_started(res->port_id) != 0) {
		fprintf(stderr, "Please stop port %u first\n", res->port_id);
		return;
	}

	port = &ports[res->port_id];

	isrx = !strcmp(res->rxtxq, "rxq");

	if (isrx && rx_queue_id_is_invalid(res->qid))
		return;
	else if (!isrx && tx_queue_id_is_invalid(res->qid))
		return;

	ison = !strcmp(res->state, "on");

	if (isrx && port->rxq[res->qid].conf.rx_deferred_start != ison) {
		port->rxq[res->qid].conf.rx_deferred_start = ison;
		needreconfig = 1;
	} else if (!isrx && port->txq[res->qid].conf.tx_deferred_start != ison) {
		port->txq[res->qid].conf.tx_deferred_start = ison;
		needreconfig = 1;
	}

	if (needreconfig)
		cmd_reconfig_device_queue(res->port_id, 0, 1);
}

cmdline_parse_token_string_t cmd_config_deferred_start_rxtx_queue_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						port, "port");
cmdline_parse_token_num_t cmd_config_deferred_start_rxtx_queue_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_deferred_start_rxtx_queue_rxtxq =
	TOKEN_STRING_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						rxtxq, "rxq#txq");
cmdline_parse_token_num_t cmd_config_deferred_start_rxtx_queue_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_deferred_start_rxtx_queue_opname =
	TOKEN_STRING_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						opname, "deferred_start");
cmdline_parse_token_string_t cmd_config_deferred_start_rxtx_queue_state =
	TOKEN_STRING_INITIALIZER(struct cmd_config_deferred_start_rxtx_queue,
						state, "on#off");

cmdline_parse_inst_t cmd_config_deferred_start_rxtx_queue = {
	.f = cmd_config_deferred_start_rxtx_queue_parsed,
	.data = NULL,
	.help_str = "port <port_id> rxq|txq <queue_id> deferred_start on|off",
	.tokens = {
		(void *)&cmd_config_deferred_start_rxtx_queue_port,
		(void *)&cmd_config_deferred_start_rxtx_queue_port_id,
		(void *)&cmd_config_deferred_start_rxtx_queue_rxtxq,
		(void *)&cmd_config_deferred_start_rxtx_queue_qid,
		(void *)&cmd_config_deferred_start_rxtx_queue_opname,
		(void *)&cmd_config_deferred_start_rxtx_queue_state,
		NULL,
	},
};

/* *** configure port rxq/txq setup *** */
struct cmd_setup_rxtx_queue {
	cmdline_fixed_string_t port;
	portid_t portid;
	cmdline_fixed_string_t rxtxq;
	uint16_t qid;
	cmdline_fixed_string_t setup;
};

/* Common CLI fields for queue setup */
cmdline_parse_token_string_t cmd_setup_rxtx_queue_port =
	TOKEN_STRING_INITIALIZER(struct cmd_setup_rxtx_queue, port, "port");
cmdline_parse_token_num_t cmd_setup_rxtx_queue_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_setup_rxtx_queue, portid, RTE_UINT16);
cmdline_parse_token_string_t cmd_setup_rxtx_queue_rxtxq =
	TOKEN_STRING_INITIALIZER(struct cmd_setup_rxtx_queue, rxtxq, "rxq#txq");
cmdline_parse_token_num_t cmd_setup_rxtx_queue_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_setup_rxtx_queue, qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_setup_rxtx_queue_setup =
	TOKEN_STRING_INITIALIZER(struct cmd_setup_rxtx_queue, setup, "setup");

static void
cmd_setup_rxtx_queue_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_setup_rxtx_queue *res = parsed_result;
	struct rte_port *port;
	struct rte_mempool *mp;
	unsigned int socket_id;
	uint8_t isrx = 0;
	int ret;

	if (port_id_is_invalid(res->portid, ENABLED_WARN))
		return;

	if (res->portid == (portid_t)RTE_PORT_ALL) {
		fprintf(stderr, "Invalid port id\n");
		return;
	}

	if (!strcmp(res->rxtxq, "rxq"))
		isrx = 1;
	else if (!strcmp(res->rxtxq, "txq"))
		isrx = 0;
	else {
		fprintf(stderr, "Unknown parameter\n");
		return;
	}

	if (isrx && rx_queue_id_is_invalid(res->qid)) {
		fprintf(stderr, "Invalid rx queue\n");
		return;
	} else if (!isrx && tx_queue_id_is_invalid(res->qid)) {
		fprintf(stderr, "Invalid tx queue\n");
		return;
	}

	port = &ports[res->portid];
	if (isrx) {
		socket_id = rxring_numa[res->portid];
		if (!numa_support || socket_id == NUMA_NO_CONFIG)
			socket_id = port->socket_id;

		mp = mbuf_pool_find(socket_id, 0);
		if (mp == NULL) {
			fprintf(stderr,
				"Failed to setup RX queue: No mempool allocation on the socket %d\n",
				rxring_numa[res->portid]);
			return;
		}
		ret = rx_queue_setup(res->portid,
				     res->qid,
				     port->nb_rx_desc[res->qid],
				     socket_id,
				     &port->rxq[res->qid].conf,
				     mp);
		if (ret)
			fprintf(stderr, "Failed to setup RX queue\n");
	} else {
		socket_id = txring_numa[res->portid];
		if (!numa_support || socket_id == NUMA_NO_CONFIG)
			socket_id = port->socket_id;

		if (port->nb_tx_desc[res->qid] < tx_pkt_nb_segs) {
			fprintf(stderr,
				"Failed to setup TX queue: not enough descriptors\n");
			return;
		}
		ret = rte_eth_tx_queue_setup(res->portid,
					     res->qid,
					     port->nb_tx_desc[res->qid],
					     socket_id,
					     &port->txq[res->qid].conf);
		if (ret)
			fprintf(stderr, "Failed to setup TX queue\n");
	}
}

cmdline_parse_inst_t cmd_setup_rxtx_queue = {
	.f = cmd_setup_rxtx_queue_parsed,
	.data = NULL,
	.help_str = "port <port_id> rxq|txq <queue_idx> setup",
	.tokens = {
		(void *)&cmd_setup_rxtx_queue_port,
		(void *)&cmd_setup_rxtx_queue_portid,
		(void *)&cmd_setup_rxtx_queue_rxtxq,
		(void *)&cmd_setup_rxtx_queue_qid,
		(void *)&cmd_setup_rxtx_queue_setup,
		NULL,
	},
};


/* *** Configure RSS RETA *** */
struct cmd_config_rss_reta {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	portid_t port_id;
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
			fprintf(stderr, "Invalid RETA hash index=%d\n",
				hash_index);
			return -1;
		}

		idx = hash_index / RTE_ETH_RETA_GROUP_SIZE;
		shift = hash_index % RTE_ETH_RETA_GROUP_SIZE;
		reta_conf[idx].mask |= (1ULL << shift);
		reta_conf[idx].reta[shift] = nb_queue;
	}

	return 0;
}

static void
cmd_set_rss_reta_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	int ret;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rss_reta_entry64 reta_conf[8];
	struct cmd_config_rss_reta *res = parsed_result;

	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	if (dev_info.reta_size == 0) {
		fprintf(stderr,
			"Redirection table size is 0 which is invalid for RSS\n");
		return;
	} else
		printf("The reta size of port %d is %u\n",
			res->port_id, dev_info.reta_size);
	if (dev_info.reta_size > RTE_ETH_RSS_RETA_SIZE_512) {
		fprintf(stderr,
			"Currently do not support more than %u entries of redirection table\n",
			RTE_ETH_RSS_RETA_SIZE_512);
		return;
	}

	memset(reta_conf, 0, sizeof(reta_conf));
	if (!strcmp(res->list_name, "reta")) {
		if (parse_reta_config(res->list_of_items, reta_conf,
						dev_info.reta_size)) {
			fprintf(stderr,
				"Invalid RSS Redirection Table config entered\n");
			return;
		}
		ret = rte_eth_dev_rss_reta_update(res->port_id,
				reta_conf, dev_info.reta_size);
		if (ret != 0)
			fprintf(stderr,
				"Bad redirection table parameter, return code = %d\n",
				ret);
	}
}

cmdline_parse_token_string_t cmd_config_rss_reta_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, port, "port");
cmdline_parse_token_string_t cmd_config_rss_reta_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_rss_reta, keyword, "config");
cmdline_parse_token_num_t cmd_config_rss_reta_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_rss_reta, port_id, RTE_UINT16);
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
	.help_str = "port config <port_id> rss reta <hash,queue[,hash,queue]*>",
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
	portid_t port_id;
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
	uint16_t i;
	uint16_t num = (nb_entries + RTE_ETH_RETA_GROUP_SIZE - 1) /
			RTE_ETH_RETA_GROUP_SIZE;
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
		fprintf(stderr,
			"The string size exceeds the internal buffer size\n");
		return -1;
	}
	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, num, ',');
	if (ret <= 0 || ret != num) {
		fprintf(stderr,
			"The bits of masks do not match the number of reta entries: %u\n",
			num);
		return -1;
	}
	for (i = 0; i < ret; i++)
		conf[i].mask = (uint64_t)strtoull(str_fld[i], &end, 0);

	return 0;
}

static void
cmd_showport_reta_parsed(void *parsed_result,
			 __rte_unused struct cmdline *cl,
			 __rte_unused void *data)
{
	struct cmd_showport_reta *res = parsed_result;
	struct rte_eth_rss_reta_entry64 reta_conf[8];
	struct rte_eth_dev_info dev_info;
	uint16_t max_reta_size;
	int ret;

	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	max_reta_size = RTE_MIN(dev_info.reta_size, RTE_ETH_RSS_RETA_SIZE_512);
	if (res->size == 0 || res->size > max_reta_size) {
		fprintf(stderr, "Invalid redirection table size: %u (1-%u)\n",
			res->size, max_reta_size);
		return;
	}

	memset(reta_conf, 0, sizeof(reta_conf));
	if (showport_parse_reta_config(reta_conf, res->size,
				res->list_of_items) < 0) {
		fprintf(stderr, "Invalid string: %s for reta masks\n",
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
	TOKEN_NUM_INITIALIZER(struct cmd_showport_reta, port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_showport_reta_rss =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta, rss, "rss");
cmdline_parse_token_string_t cmd_showport_reta_reta =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta, reta, "reta");
cmdline_parse_token_num_t cmd_showport_reta_size =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_reta, size, RTE_UINT16);
cmdline_parse_token_string_t cmd_showport_reta_list_of_items =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_reta,
					list_of_items, NULL);

cmdline_parse_inst_t cmd_showport_reta = {
	.f = cmd_showport_reta_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rss reta <size> <mask0[,mask1]*>",
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
	portid_t port_id;
	cmdline_fixed_string_t rss_hash;
	cmdline_fixed_string_t rss_type;
	cmdline_fixed_string_t key; /* optional argument */
};

static void cmd_showport_rss_hash_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				void *show_rss_key)
{
	struct cmd_showport_rss_hash *res = parsed_result;

	port_rss_hash_conf_show(res->port_id, show_rss_key != NULL);
}

cmdline_parse_token_string_t cmd_showport_rss_hash_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, show, "show");
cmdline_parse_token_string_t cmd_showport_rss_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, port, "port");
cmdline_parse_token_num_t cmd_showport_rss_hash_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_rss_hash, port_id,
				 RTE_UINT16);
cmdline_parse_token_string_t cmd_showport_rss_hash_rss_hash =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, rss_hash,
				 "rss-hash");
cmdline_parse_token_string_t cmd_showport_rss_hash_rss_key =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_rss_hash, key, "key");

cmdline_parse_inst_t cmd_showport_rss_hash = {
	.f = cmd_showport_rss_hash_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rss-hash",
	.tokens = {
		(void *)&cmd_showport_rss_hash_show,
		(void *)&cmd_showport_rss_hash_port,
		(void *)&cmd_showport_rss_hash_port_id,
		(void *)&cmd_showport_rss_hash_rss_hash,
		NULL,
	},
};

cmdline_parse_inst_t cmd_showport_rss_hash_key = {
	.f = cmd_showport_rss_hash_parsed,
	.data = (void *)1,
	.help_str = "show port <port_id> rss-hash key",
	.tokens = {
		(void *)&cmd_showport_rss_hash_show,
		(void *)&cmd_showport_rss_hash_port,
		(void *)&cmd_showport_rss_hash_port_id,
		(void *)&cmd_showport_rss_hash_rss_hash,
		(void *)&cmd_showport_rss_hash_rss_key,
		NULL,
	},
};

/* *** Configure DCB *** */
struct cmd_config_dcb {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t dcb;
	cmdline_fixed_string_t vt;
	cmdline_fixed_string_t vt_en;
	uint8_t num_tcs;
	cmdline_fixed_string_t pfc;
	cmdline_fixed_string_t pfc_en;
};

static void
cmd_config_dcb_parsed(void *parsed_result,
                        __rte_unused struct cmdline *cl,
                        __rte_unused void *data)
{
	struct cmd_config_dcb *res = parsed_result;
	struct rte_eth_dcb_info dcb_info;
	portid_t port_id = res->port_id;
	struct rte_port *port;
	uint8_t pfc_en;
	int ret;

	port = &ports[port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", port_id);
		return;
	}

	if ((res->num_tcs != RTE_ETH_4_TCS) && (res->num_tcs != RTE_ETH_8_TCS)) {
		fprintf(stderr,
			"The invalid number of traffic class, only 4 or 8 allowed.\n");
		return;
	}

	if (nb_fwd_lcores < res->num_tcs) {
		fprintf(stderr,
			"nb_cores shouldn't be less than number of TCs.\n");
		return;
	}

	/* Check whether the port supports the report of DCB info. */
	ret = rte_eth_dev_get_dcb_info(port_id, &dcb_info);
	if (ret == -ENOTSUP) {
		fprintf(stderr, "rte_eth_dev_get_dcb_info not supported.\n");
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
		fprintf(stderr, "Cannot initialize network ports.\n");
		return;
	}

	fwd_config_setup();

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_config_dcb_port =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, port, "port");
cmdline_parse_token_string_t cmd_config_dcb_config =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, config, "config");
cmdline_parse_token_num_t cmd_config_dcb_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_dcb, port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_dcb_dcb =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, dcb, "dcb");
cmdline_parse_token_string_t cmd_config_dcb_vt =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, vt, "vt");
cmdline_parse_token_string_t cmd_config_dcb_vt_en =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, vt_en, "on#off");
cmdline_parse_token_num_t cmd_config_dcb_num_tcs =
	TOKEN_NUM_INITIALIZER(struct cmd_config_dcb, num_tcs, RTE_UINT8);
cmdline_parse_token_string_t cmd_config_dcb_pfc=
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, pfc, "pfc");
cmdline_parse_token_string_t cmd_config_dcb_pfc_en =
        TOKEN_STRING_INITIALIZER(struct cmd_config_dcb, pfc_en, "on#off");

cmdline_parse_inst_t cmd_config_dcb = {
	.f = cmd_config_dcb_parsed,
	.data = NULL,
	.help_str = "port config <port-id> dcb vt on|off <num_tcs> pfc on|off",
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
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_burst *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	uint16_t rec_nb_pkts;
	int ret;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "burst")) {
		if (res->value == 0) {
			/* If user gives a value of zero, query the PMD for
			 * its recommended Rx burst size. Testpmd uses a single
			 * size for all ports, so assume all ports are the same
			 * NIC model and use the values from Port 0.
			 */
			ret = eth_dev_info_get_print_err(0, &dev_info);
			if (ret != 0)
				return;

			rec_nb_pkts = dev_info.default_rxportconf.burst_size;

			if (rec_nb_pkts == 0) {
				printf("PMD does not recommend a burst size.\n"
					"User provided value must be between"
					" 1 and %d\n", MAX_PKT_BURST);
				return;
			} else if (rec_nb_pkts > MAX_PKT_BURST) {
				printf("PMD recommended burst size of %d"
					" exceeds maximum value of %d\n",
					rec_nb_pkts, MAX_PKT_BURST);
				return;
			}
			printf("Using PMD-provided burst value of %d\n",
				rec_nb_pkts);
			nb_pkt_per_burst = rec_nb_pkts;
		} else if (res->value > MAX_PKT_BURST) {
			fprintf(stderr, "burst must be >= 1 && <= %d\n",
				MAX_PKT_BURST);
			return;
		} else
			nb_pkt_per_burst = res->value;
	} else {
		fprintf(stderr, "Unknown parameter\n");
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
	TOKEN_NUM_INITIALIZER(struct cmd_config_burst, value, RTE_UINT16);

cmdline_parse_inst_t cmd_config_burst = {
	.f = cmd_config_burst_parsed,
	.data = NULL,
	.help_str = "port config all burst <value>",
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
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_thresh *res = parsed_result;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
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
		fprintf(stderr, "Unknown parameter\n");
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
	TOKEN_NUM_INITIALIZER(struct cmd_config_thresh, value, RTE_UINT8);

cmdline_parse_inst_t cmd_config_thresh = {
	.f = cmd_config_thresh_parsed,
	.data = NULL,
	.help_str = "port config all txpt|txht|txwt|rxpt|rxht|rxwt <value>",
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
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_config_threshold *res = parsed_result;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	if (!strcmp(res->name, "txfreet"))
		tx_free_thresh = res->value;
	else if (!strcmp(res->name, "txrst"))
		tx_rs_thresh = res->value;
	else if (!strcmp(res->name, "rxfreet"))
		rx_free_thresh = res->value;
	else {
		fprintf(stderr, "Unknown parameter\n");
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
	TOKEN_NUM_INITIALIZER(struct cmd_config_threshold, value, RTE_UINT16);

cmdline_parse_inst_t cmd_config_threshold = {
	.f = cmd_config_threshold_parsed,
	.data = NULL,
	.help_str = "port config all txfreet|txrst|rxfreet <value>",
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

static void cmd_stop_parsed(__rte_unused void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	stop_packet_forwarding();
}

cmdline_parse_token_string_t cmd_stop_stop =
	TOKEN_STRING_INITIALIZER(struct cmd_stop_result, stop, "stop");

cmdline_parse_inst_t cmd_stop = {
	.f = cmd_stop_parsed,
	.data = NULL,
	.help_str = "stop: Stop packet forwarding",
	.tokens = {
		(void *)&cmd_stop_stop,
		NULL,
	},
};

/* *** SET CORELIST and PORTLIST CONFIGURATION *** */

unsigned int
parse_item_list(const char *str, const char *item_name, unsigned int max_items,
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
			fprintf(stderr, "character %c is not a decimal digit\n", c);
			return 0;
		}
		if (! value_ok) {
			fprintf(stderr, "No valid value before comma\n");
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
		fprintf(stderr, "Number of %s = %u > %u (maximum items)\n",
			item_name, nb_item + 1, max_items);
		return 0;
	}
	parsed_items[nb_item++] = value;
	if (! check_unique_values)
		return nb_item;

	/*
	 * Then, check that all values in the list are different.
	 * No optimization here...
	 */
	for (i = 0; i < nb_item; i++) {
		for (j = i + 1; j < nb_item; j++) {
			if (parsed_items[j] == parsed_items[i]) {
				fprintf(stderr,
					"duplicated %s %u at index %u and %u\n",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_set_list_result *res;
	union {
		unsigned int lcorelist[RTE_MAX_LCORE];
		unsigned int portlist[RTE_MAX_ETHPORTS];
	} parsed_items;
	unsigned int nb_item;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
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
	.help_str = "set corelist|portlist <list0[,list1]*>",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_setmask_result *res = parsed_result;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
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
	TOKEN_NUM_INITIALIZER(struct cmd_setmask_result, hexavalue, RTE_UINT64);

cmdline_parse_inst_t cmd_set_fwd_mask = {
	.f = cmd_set_mask_parsed,
	.data = NULL,
	.help_str = "set coremask|portmask <hexadecimal value>",
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
			   __rte_unused struct cmdline *cl,
			   __rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_set_result, value, RTE_UINT16);

cmdline_parse_inst_t cmd_set_numbers = {
	.f = cmd_set_parsed,
	.data = NULL,
	.help_str = "set nbport|nbcore|burst|verbose <value>",
	.tokens = {
		(void *)&cmd_set_set,
		(void *)&cmd_set_what,
		(void *)&cmd_set_value,
		NULL,
	},
};

/* *** SET LOG LEVEL CONFIGURATION *** */

struct cmd_set_log_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t log;
	cmdline_fixed_string_t type;
	uint32_t level;
};

static void
cmd_set_log_parsed(void *parsed_result,
		   __rte_unused struct cmdline *cl,
		   __rte_unused void *data)
{
	struct cmd_set_log_result *res;
	int ret;

	res = parsed_result;
	if (!strcmp(res->type, "global"))
		rte_log_set_global_level(res->level);
	else {
		ret = rte_log_set_level_regexp(res->type, res->level);
		if (ret < 0)
			fprintf(stderr, "Unable to set log level\n");
	}
}

cmdline_parse_token_string_t cmd_set_log_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_log_result, set, "set");
cmdline_parse_token_string_t cmd_set_log_log =
	TOKEN_STRING_INITIALIZER(struct cmd_set_log_result, log, "log");
cmdline_parse_token_string_t cmd_set_log_type =
	TOKEN_STRING_INITIALIZER(struct cmd_set_log_result, type, NULL);
cmdline_parse_token_num_t cmd_set_log_level =
	TOKEN_NUM_INITIALIZER(struct cmd_set_log_result, level, RTE_UINT32);

cmdline_parse_inst_t cmd_set_log = {
	.f = cmd_set_log_parsed,
	.data = NULL,
	.help_str = "set log global|<type> <level>",
	.tokens = {
		(void *)&cmd_set_log_set,
		(void *)&cmd_set_log_log,
		(void *)&cmd_set_log_type,
		(void *)&cmd_set_log_level,
		NULL,
	},
};

/* *** SET SEGMENT OFFSETS OF RX PACKETS SPLIT *** */

struct cmd_set_rxoffs_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t rxoffs;
	cmdline_fixed_string_t seg_offsets;
};

static void
cmd_set_rxoffs_parsed(void *parsed_result,
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
{
	struct cmd_set_rxoffs_result *res;
	unsigned int seg_offsets[MAX_SEGS_BUFFER_SPLIT];
	unsigned int nb_segs;

	res = parsed_result;
	nb_segs = parse_item_list(res->seg_offsets, "segment offsets",
				  MAX_SEGS_BUFFER_SPLIT, seg_offsets, 0);
	if (nb_segs > 0)
		set_rx_pkt_offsets(seg_offsets, nb_segs);
	cmd_reconfig_device_queue(RTE_PORT_ALL, 0, 1);
}

cmdline_parse_token_string_t cmd_set_rxoffs_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxoffs_result,
				 cmd_keyword, "set");
cmdline_parse_token_string_t cmd_set_rxoffs_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxoffs_result,
				 rxoffs, "rxoffs");
cmdline_parse_token_string_t cmd_set_rxoffs_offsets =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxoffs_result,
				 seg_offsets, NULL);

cmdline_parse_inst_t cmd_set_rxoffs = {
	.f = cmd_set_rxoffs_parsed,
	.data = NULL,
	.help_str = "set rxoffs <len0[,len1]*>",
	.tokens = {
		(void *)&cmd_set_rxoffs_keyword,
		(void *)&cmd_set_rxoffs_name,
		(void *)&cmd_set_rxoffs_offsets,
		NULL,
	},
};

/* *** SET SEGMENT LENGTHS OF RX PACKETS SPLIT *** */

struct cmd_set_rxpkts_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t rxpkts;
	cmdline_fixed_string_t seg_lengths;
};

static void
cmd_set_rxpkts_parsed(void *parsed_result,
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
{
	struct cmd_set_rxpkts_result *res;
	unsigned int seg_lengths[MAX_SEGS_BUFFER_SPLIT];
	unsigned int nb_segs;

	res = parsed_result;
	nb_segs = parse_item_list(res->seg_lengths, "segment lengths",
				  MAX_SEGS_BUFFER_SPLIT, seg_lengths, 0);
	if (nb_segs > 0)
		set_rx_pkt_segments(seg_lengths, nb_segs);
	cmd_reconfig_device_queue(RTE_PORT_ALL, 0, 1);
}

cmdline_parse_token_string_t cmd_set_rxpkts_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxpkts_result,
				 cmd_keyword, "set");
cmdline_parse_token_string_t cmd_set_rxpkts_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxpkts_result,
				 rxpkts, "rxpkts");
cmdline_parse_token_string_t cmd_set_rxpkts_lengths =
	TOKEN_STRING_INITIALIZER(struct cmd_set_rxpkts_result,
				 seg_lengths, NULL);

cmdline_parse_inst_t cmd_set_rxpkts = {
	.f = cmd_set_rxpkts_parsed,
	.data = NULL,
	.help_str = "set rxpkts <len0[,len1]*>",
	.tokens = {
		(void *)&cmd_set_rxpkts_keyword,
		(void *)&cmd_set_rxpkts_name,
		(void *)&cmd_set_rxpkts_lengths,
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
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
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
	.help_str = "set txpkts <len0[,len1]*>",
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
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
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

/* *** SET TIMES FOR TXONLY PACKETS SCHEDULING ON TIMESTAMPS *** */

struct cmd_set_txtimes_result {
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t txtimes;
	cmdline_fixed_string_t tx_times;
};

static void
cmd_set_txtimes_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_set_txtimes_result *res;
	unsigned int tx_times[2] = {0, 0};
	unsigned int n_times;

	res = parsed_result;
	n_times = parse_item_list(res->tx_times, "tx times",
				  2, tx_times, 0);
	if (n_times == 2)
		set_tx_pkt_times(tx_times);
}

cmdline_parse_token_string_t cmd_set_txtimes_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txtimes_result,
				 cmd_keyword, "set");
cmdline_parse_token_string_t cmd_set_txtimes_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txtimes_result,
				 txtimes, "txtimes");
cmdline_parse_token_string_t cmd_set_txtimes_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_txtimes_result,
				 tx_times, NULL);

cmdline_parse_inst_t cmd_set_txtimes = {
	.f = cmd_set_txtimes_parsed,
	.data = NULL,
	.help_str = "set txtimes <inter_burst>,<intra_burst>",
	.tokens = {
		(void *)&cmd_set_txtimes_keyword,
		(void *)&cmd_set_txtimes_name,
		(void *)&cmd_set_txtimes_value,
		NULL,
	},
};

/* *** ADD/REMOVE ALL VLAN IDENTIFIERS TO/FROM A PORT VLAN RX FILTER *** */
struct cmd_rx_vlan_filter_all_result {
	cmdline_fixed_string_t rx_vlan;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t all;
	portid_t port_id;
};

static void
cmd_rx_vlan_filter_all_parsed(void *parsed_result,
			      __rte_unused struct cmdline *cl,
			      __rte_unused void *data)
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
			      port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_rx_vlan_filter_all = {
	.f = cmd_rx_vlan_filter_all_parsed,
	.data = NULL,
	.help_str = "rx_vlan add|rm all <port_id>: "
		"Add/Remove all identifiers to/from the set of VLAN "
		"identifiers filtered by a port",
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
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
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
			fprintf(stderr, "must specify (port,queue_id)\n");
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
	else if (!strcmp(res->what, "qinq_strip"))
		rx_vlan_qinq_strip_set(port_id, on);
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
				what, "strip#filter#qinq_strip#extend#stripq");
cmdline_parse_token_string_t cmd_vlan_offload_on =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
			      on, "on#off");
cmdline_parse_token_string_t cmd_vlan_offload_portid =
	TOKEN_STRING_INITIALIZER(struct cmd_vlan_offload_result,
			      port_id, NULL);

cmdline_parse_inst_t cmd_vlan_offload = {
	.f = cmd_vlan_offload_parsed,
	.data = NULL,
	.help_str = "vlan set strip|filter|qinq_strip|extend|stripq on|off "
		"<port_id[,queue_id]>: "
		"Strip/Filter/QinQ for rx side Extend for both rx/tx sides",
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
	portid_t port_id;
};

static void
cmd_vlan_tpid_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_vlan_tpid_result *res = parsed_result;
	enum rte_vlan_type vlan_type;

	if (!strcmp(res->vlan_type, "inner"))
		vlan_type = RTE_ETH_VLAN_TYPE_INNER;
	else if (!strcmp(res->vlan_type, "outer"))
		vlan_type = RTE_ETH_VLAN_TYPE_OUTER;
	else {
		fprintf(stderr, "Unknown vlan type\n");
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
			      tp_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vlan_tpid_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_vlan_tpid_result,
			      port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_vlan_tpid = {
	.f = cmd_vlan_tpid_parsed,
	.data = NULL,
	.help_str = "vlan set inner|outer tpid <tp_id> <port_id>: "
		"Set the VLAN Ether type",
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
	portid_t port_id;
};

static void
cmd_rx_vlan_filter_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
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
			      vlan_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_rx_vlan_filter_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_rx_vlan_filter_result,
			      port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_rx_vlan_filter = {
	.f = cmd_rx_vlan_filter_parsed,
	.data = NULL,
	.help_str = "rx_vlan add|rm <vlan_id> <port_id>: "
		"Add/Remove a VLAN identifier to/from the set of VLAN "
		"identifiers filtered by a port",
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
	portid_t port_id;
	uint16_t vlan_id;
};

static void
cmd_tx_vlan_set_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_tx_vlan_set_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	tx_vlan_set(res->port_id, res->vlan_id);

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_tx_vlan_set_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_result,
				 tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_set_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_result,
				 set, "set");
cmdline_parse_token_num_t cmd_tx_vlan_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_result,
			      vlan_id, RTE_UINT16);

cmdline_parse_inst_t cmd_tx_vlan_set = {
	.f = cmd_tx_vlan_set_parsed,
	.data = NULL,
	.help_str = "tx_vlan set <port_id> <vlan_id>: "
		"Enable hardware insertion of a single VLAN header "
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
	portid_t port_id;
	uint16_t vlan_id;
	uint16_t vlan_id_outer;
};

static void
cmd_tx_vlan_set_qinq_parsed(void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	struct cmd_tx_vlan_set_qinq_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	tx_qinq_set(res->port_id, res->vlan_id, res->vlan_id_outer);

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_tx_vlan_set_qinq_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_set_qinq_set =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		set, "set");
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		vlan_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_qinq_vlanid_outer =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_qinq_result,
		vlan_id_outer, RTE_UINT16);

cmdline_parse_inst_t cmd_tx_vlan_set_qinq = {
	.f = cmd_tx_vlan_set_qinq_parsed,
	.data = NULL,
	.help_str = "tx_vlan set <port_id> <vlan_id> <outer_vlan_id>: "
		"Enable hardware insertion of double VLAN header "
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
	portid_t port_id;
	uint16_t vlan_id;
	cmdline_fixed_string_t mode;
};

static void
cmd_tx_vlan_set_pvid_parsed(void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
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
			     port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_tx_vlan_set_pvid_vlan_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
			      vlan_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_tx_vlan_set_pvid_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_set_pvid_result,
				 mode, "on#off");

cmdline_parse_inst_t cmd_tx_vlan_set_pvid = {
	.f = cmd_tx_vlan_set_pvid_parsed,
	.data = NULL,
	.help_str = "tx_vlan set pvid <port_id> <vlan_id> on|off",
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
	portid_t port_id;
};

static void
cmd_tx_vlan_reset_parsed(void *parsed_result,
			 __rte_unused struct cmdline *cl,
			 __rte_unused void *data)
{
	struct cmd_tx_vlan_reset_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	tx_vlan_reset(res->port_id);

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_tx_vlan_reset_tx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_reset_result,
				 tx_vlan, "tx_vlan");
cmdline_parse_token_string_t cmd_tx_vlan_reset_reset =
	TOKEN_STRING_INITIALIZER(struct cmd_tx_vlan_reset_result,
				 reset, "reset");
cmdline_parse_token_num_t cmd_tx_vlan_reset_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tx_vlan_reset_result,
			      port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_tx_vlan_reset = {
	.f = cmd_tx_vlan_reset_parsed,
	.data = NULL,
	.help_str = "tx_vlan reset <port_id>: Disable hardware insertion of a "
		"VLAN header in packets sent on a port",
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
	portid_t port_id;
};

static void
csum_show(int port_id)
{
	struct rte_eth_dev_info dev_info;
	uint64_t tx_offloads;
	int ret;

	tx_offloads = ports[port_id].dev_conf.txmode.offloads;
	printf("Parse tunnel is %s\n",
		(ports[port_id].parse_tunnel) ? "on" : "off");
	printf("IP checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) ? "hw" : "sw");
	printf("UDP checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) ? "hw" : "sw");
	printf("TCP checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) ? "hw" : "sw");
	printf("SCTP checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM) ? "hw" : "sw");
	printf("Outer-Ip checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM) ? "hw" : "sw");
	printf("Outer-Udp checksum offload is %s\n",
		(tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM) ? "hw" : "sw");

	/* display warnings if configuration is not supported by the NIC */
	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) == 0) {
		fprintf(stderr,
			"Warning: hardware IP checksum enabled but not supported by port %d\n",
			port_id);
	}
	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) == 0) {
		fprintf(stderr,
			"Warning: hardware UDP checksum enabled but not supported by port %d\n",
			port_id);
	}
	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) == 0) {
		fprintf(stderr,
			"Warning: hardware TCP checksum enabled but not supported by port %d\n",
			port_id);
	}
	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM) == 0) {
		fprintf(stderr,
			"Warning: hardware SCTP checksum enabled but not supported by port %d\n",
			port_id);
	}
	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM) == 0) {
		fprintf(stderr,
			"Warning: hardware outer IP checksum enabled but not supported by port %d\n",
			port_id);
	}
	if ((tx_offloads & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)
			== 0) {
		fprintf(stderr,
			"Warning: hardware outer UDP checksum enabled but not supported by port %d\n",
			port_id);
	}
}

static void
cmd_config_queue_tx_offloads(struct rte_port *port)
{
	int k;

	/* Apply queue tx offloads configuration */
	for (k = 0; k < port->dev_info.max_tx_queues; k++)
		port->txq[k].conf.offloads =
			port->dev_conf.txmode.offloads;
}

static void
cmd_csum_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_csum_result *res = parsed_result;
	int hw = 0;
	uint64_t csum_offloads = 0;
	struct rte_eth_dev_info dev_info;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN)) {
		fprintf(stderr, "invalid port %d\n", res->port_id);
		return;
	}
	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	if (!strcmp(res->mode, "set")) {

		if (!strcmp(res->hwsw, "hw"))
			hw = 1;

		if (!strcmp(res->proto, "ip")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
						RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)) {
				csum_offloads |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
			} else {
				fprintf(stderr,
					"IP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		} else if (!strcmp(res->proto, "udp")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
						RTE_ETH_TX_OFFLOAD_UDP_CKSUM)) {
				csum_offloads |= RTE_ETH_TX_OFFLOAD_UDP_CKSUM;
			} else {
				fprintf(stderr,
					"UDP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		} else if (!strcmp(res->proto, "tcp")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
						RTE_ETH_TX_OFFLOAD_TCP_CKSUM)) {
				csum_offloads |= RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
			} else {
				fprintf(stderr,
					"TCP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		} else if (!strcmp(res->proto, "sctp")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
						RTE_ETH_TX_OFFLOAD_SCTP_CKSUM)) {
				csum_offloads |= RTE_ETH_TX_OFFLOAD_SCTP_CKSUM;
			} else {
				fprintf(stderr,
					"SCTP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		} else if (!strcmp(res->proto, "outer-ip")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
					RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM)) {
				csum_offloads |=
						RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;
			} else {
				fprintf(stderr,
					"Outer IP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		} else if (!strcmp(res->proto, "outer-udp")) {
			if (hw == 0 || (dev_info.tx_offload_capa &
					RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)) {
				csum_offloads |=
						RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM;
			} else {
				fprintf(stderr,
					"Outer UDP checksum offload is not supported by port %u\n",
					res->port_id);
			}
		}

		if (hw) {
			ports[res->port_id].dev_conf.txmode.offloads |=
							csum_offloads;
		} else {
			ports[res->port_id].dev_conf.txmode.offloads &=
							(~csum_offloads);
		}
		cmd_config_queue_tx_offloads(&ports[res->port_id]);
	}
	csum_show(res->port_id);

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_csum_csum =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				csum, "csum");
cmdline_parse_token_string_t cmd_csum_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				mode, "set");
cmdline_parse_token_string_t cmd_csum_proto =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				proto, "ip#tcp#udp#sctp#outer-ip#outer-udp");
cmdline_parse_token_string_t cmd_csum_hwsw =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_result,
				hwsw, "hw#sw");
cmdline_parse_token_num_t cmd_csum_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_csum_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_csum_set = {
	.f = cmd_csum_parsed,
	.data = NULL,
	.help_str = "csum set ip|tcp|udp|sctp|outer-ip|outer-udp hw|sw <port_id>: "
		"Enable/Disable hardware calculation of L3/L4 checksum when "
		"using csum forward engine",
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
	.help_str = "csum show <port_id>: Show checksum offload configuration",
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
	portid_t port_id;
};

static void
cmd_csum_tunnel_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_csum_tunnel_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (!strcmp(res->onoff, "on"))
		ports[res->port_id].parse_tunnel = 1;
	else
		ports[res->port_id].parse_tunnel = 0;

	csum_show(res->port_id);
}

cmdline_parse_token_string_t cmd_csum_tunnel_csum =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				csum, "csum");
cmdline_parse_token_string_t cmd_csum_tunnel_parse =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				parse, "parse-tunnel");
cmdline_parse_token_string_t cmd_csum_tunnel_onoff =
	TOKEN_STRING_INITIALIZER(struct cmd_csum_tunnel_result,
				onoff, "on#off");
cmdline_parse_token_num_t cmd_csum_tunnel_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_csum_tunnel_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_csum_tunnel = {
	.f = cmd_csum_tunnel_parsed,
	.data = NULL,
	.help_str = "csum parse-tunnel on|off <port_id>: "
		"Enable/Disable parsing of tunnels for csum engine",
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
	portid_t port_id;
};

static void
cmd_tso_set_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_tso_set_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	if (!strcmp(res->mode, "set"))
		ports[res->port_id].tso_segsz = res->tso_segsz;

	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	if ((ports[res->port_id].tso_segsz != 0) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_TCP_TSO) == 0) {
		fprintf(stderr, "Error: TSO is not supported by port %d\n",
			res->port_id);
		return;
	}

	if (ports[res->port_id].tso_segsz == 0) {
		ports[res->port_id].dev_conf.txmode.offloads &=
						~RTE_ETH_TX_OFFLOAD_TCP_TSO;
		printf("TSO for non-tunneled packets is disabled\n");
	} else {
		ports[res->port_id].dev_conf.txmode.offloads |=
						RTE_ETH_TX_OFFLOAD_TCP_TSO;
		printf("TSO segment size for non-tunneled packets is %d\n",
			ports[res->port_id].tso_segsz);
	}
	cmd_config_queue_tx_offloads(&ports[res->port_id]);

	/* display warnings if configuration is not supported by the NIC */
	ret = eth_dev_info_get_print_err(res->port_id, &dev_info);
	if (ret != 0)
		return;

	if ((ports[res->port_id].tso_segsz != 0) &&
		(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_TCP_TSO) == 0) {
		fprintf(stderr,
			"Warning: TSO enabled but not supported by port %d\n",
			res->port_id);
	}

	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_tso_set_tso =
	TOKEN_STRING_INITIALIZER(struct cmd_tso_set_result,
				tso, "tso");
cmdline_parse_token_string_t cmd_tso_set_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tso_set_result,
				mode, "set");
cmdline_parse_token_num_t cmd_tso_set_tso_segsz =
	TOKEN_NUM_INITIALIZER(struct cmd_tso_set_result,
				tso_segsz, RTE_UINT16);
cmdline_parse_token_num_t cmd_tso_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tso_set_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_tso_set = {
	.f = cmd_tso_set_parsed,
	.data = NULL,
	.help_str = "tso set <tso_segsz> <port_id>: "
		"Set TSO segment size of non-tunneled packets for csum engine "
		"(0 to disable)",
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
	.help_str = "tso show <port_id>: "
		"Show TSO segment size of non-tunneled packets for csum engine",
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
	portid_t port_id;
};

static struct rte_eth_dev_info
check_tunnel_tso_nic_support(portid_t port_id)
{
	struct rte_eth_dev_info dev_info;

	if (eth_dev_info_get_print_err(port_id, &dev_info) != 0)
		return dev_info;

	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO))
		fprintf(stderr,
			"Warning: VXLAN TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO))
		fprintf(stderr,
			"Warning: GRE TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO))
		fprintf(stderr,
			"Warning: IPIP TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO))
		fprintf(stderr,
			"Warning: GENEVE TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_IP_TNL_TSO))
		fprintf(stderr,
			"Warning: IP TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO))
		fprintf(stderr,
			"Warning: UDP TUNNEL TSO not supported therefore not enabled for port %d\n",
			port_id);
	return dev_info;
}

static void
cmd_tunnel_tso_set_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_tunnel_tso_set_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	if (!port_is_stopped(res->port_id)) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	if (!strcmp(res->mode, "set"))
		ports[res->port_id].tunnel_tso_segsz = res->tso_segsz;

	dev_info = check_tunnel_tso_nic_support(res->port_id);
	if (ports[res->port_id].tunnel_tso_segsz == 0) {
		ports[res->port_id].dev_conf.txmode.offloads &=
			~(RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_IP_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO);
		printf("TSO for tunneled packets is disabled\n");
	} else {
		uint64_t tso_offloads = (RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
					 RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
					 RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
					 RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
					 RTE_ETH_TX_OFFLOAD_IP_TNL_TSO |
					 RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO);

		ports[res->port_id].dev_conf.txmode.offloads |=
			(tso_offloads & dev_info.tx_offload_capa);
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

		if (!ports[res->port_id].parse_tunnel)
			fprintf(stderr,
				"Warning: csum parse_tunnel must be set so that tunneled packets are recognized\n");
		if (!(ports[res->port_id].dev_conf.txmode.offloads &
		      RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM))
			fprintf(stderr,
				"Warning: csum set outer-ip must be set to hw if outer L3 is IPv4; not necessary for IPv6\n");
	}

	cmd_config_queue_tx_offloads(&ports[res->port_id]);
	cmd_reconfig_device_queue(res->port_id, 1, 1);
}

cmdline_parse_token_string_t cmd_tunnel_tso_set_tso =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_tso_set_result,
				tso, "tunnel_tso");
cmdline_parse_token_string_t cmd_tunnel_tso_set_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_tso_set_result,
				mode, "set");
cmdline_parse_token_num_t cmd_tunnel_tso_set_tso_segsz =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_tso_set_result,
				tso_segsz, RTE_UINT16);
cmdline_parse_token_num_t cmd_tunnel_tso_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_tso_set_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_tunnel_tso_set = {
	.f = cmd_tunnel_tso_set_parsed,
	.data = NULL,
	.help_str = "tunnel_tso set <tso_segsz> <port_id>: "
		"Set TSO segment size of tunneled packets for csum engine "
		"(0 to disable)",
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
	.help_str = "tunnel_tso show <port_id> "
		"Show TSO segment size of tunneled packets for csum engine",
	.tokens = {
		(void *)&cmd_tunnel_tso_set_tso,
		(void *)&cmd_tunnel_tso_show_mode,
		(void *)&cmd_tunnel_tso_set_portid,
		NULL,
	},
};

#ifdef RTE_LIB_GRO
/* *** SET GRO FOR A PORT *** */
struct cmd_gro_enable_result {
	cmdline_fixed_string_t cmd_set;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t cmd_onoff;
	portid_t cmd_pid;
};

static void
cmd_gro_enable_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_gro_enable_result *res;

	res = parsed_result;
	if (!strcmp(res->cmd_keyword, "gro"))
		setup_gro(res->cmd_onoff, res->cmd_pid);
}

cmdline_parse_token_string_t cmd_gro_enable_set =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_enable_result,
			cmd_set, "set");
cmdline_parse_token_string_t cmd_gro_enable_port =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_enable_result,
			cmd_keyword, "port");
cmdline_parse_token_num_t cmd_gro_enable_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_gro_enable_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_gro_enable_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_enable_result,
			cmd_keyword, "gro");
cmdline_parse_token_string_t cmd_gro_enable_onoff =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_enable_result,
			cmd_onoff, "on#off");

cmdline_parse_inst_t cmd_gro_enable = {
	.f = cmd_gro_enable_parsed,
	.data = NULL,
	.help_str = "set port <port_id> gro on|off",
	.tokens = {
		(void *)&cmd_gro_enable_set,
		(void *)&cmd_gro_enable_port,
		(void *)&cmd_gro_enable_pid,
		(void *)&cmd_gro_enable_keyword,
		(void *)&cmd_gro_enable_onoff,
		NULL,
	},
};

/* *** DISPLAY GRO CONFIGURATION *** */
struct cmd_gro_show_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_gro_show_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_gro_show_result *res;

	res = parsed_result;
	if (!strcmp(res->cmd_keyword, "gro"))
		show_gro(res->cmd_pid);
}

cmdline_parse_token_string_t cmd_gro_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_show_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_gro_show_port =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_show_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_gro_show_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_gro_show_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_gro_show_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_show_result,
			cmd_keyword, "gro");

cmdline_parse_inst_t cmd_gro_show = {
	.f = cmd_gro_show_parsed,
	.data = NULL,
	.help_str = "show port <port_id> gro",
	.tokens = {
		(void *)&cmd_gro_show_show,
		(void *)&cmd_gro_show_port,
		(void *)&cmd_gro_show_pid,
		(void *)&cmd_gro_show_keyword,
		NULL,
	},
};

/* *** SET FLUSH CYCLES FOR GRO *** */
struct cmd_gro_flush_result {
	cmdline_fixed_string_t cmd_set;
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t cmd_flush;
	uint8_t cmd_cycles;
};

static void
cmd_gro_flush_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_gro_flush_result *res;

	res = parsed_result;
	if ((!strcmp(res->cmd_keyword, "gro")) &&
			(!strcmp(res->cmd_flush, "flush")))
		setup_gro_flush_cycles(res->cmd_cycles);
}

cmdline_parse_token_string_t cmd_gro_flush_set =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_flush_result,
			cmd_set, "set");
cmdline_parse_token_string_t cmd_gro_flush_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_flush_result,
			cmd_keyword, "gro");
cmdline_parse_token_string_t cmd_gro_flush_flush =
	TOKEN_STRING_INITIALIZER(struct cmd_gro_flush_result,
			cmd_flush, "flush");
cmdline_parse_token_num_t cmd_gro_flush_cycles =
	TOKEN_NUM_INITIALIZER(struct cmd_gro_flush_result,
			cmd_cycles, RTE_UINT8);

cmdline_parse_inst_t cmd_gro_flush = {
	.f = cmd_gro_flush_parsed,
	.data = NULL,
	.help_str = "set gro flush <cycles>",
	.tokens = {
		(void *)&cmd_gro_flush_set,
		(void *)&cmd_gro_flush_keyword,
		(void *)&cmd_gro_flush_flush,
		(void *)&cmd_gro_flush_cycles,
		NULL,
	},
};
#endif /* RTE_LIB_GRO */

#ifdef RTE_LIB_GSO
/* *** ENABLE/DISABLE GSO *** */
struct cmd_gso_enable_result {
	cmdline_fixed_string_t cmd_set;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t cmd_mode;
	portid_t cmd_pid;
};

static void
cmd_gso_enable_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_gso_enable_result *res;

	res = parsed_result;
	if (!strcmp(res->cmd_keyword, "gso"))
		setup_gso(res->cmd_mode, res->cmd_pid);
}

cmdline_parse_token_string_t cmd_gso_enable_set =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_enable_result,
			cmd_set, "set");
cmdline_parse_token_string_t cmd_gso_enable_port =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_enable_result,
			cmd_port, "port");
cmdline_parse_token_string_t cmd_gso_enable_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_enable_result,
			cmd_keyword, "gso");
cmdline_parse_token_string_t cmd_gso_enable_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_enable_result,
			cmd_mode, "on#off");
cmdline_parse_token_num_t cmd_gso_enable_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_gso_enable_result,
			cmd_pid, RTE_UINT16);

cmdline_parse_inst_t cmd_gso_enable = {
	.f = cmd_gso_enable_parsed,
	.data = NULL,
	.help_str = "set port <port_id> gso on|off",
	.tokens = {
		(void *)&cmd_gso_enable_set,
		(void *)&cmd_gso_enable_port,
		(void *)&cmd_gso_enable_pid,
		(void *)&cmd_gso_enable_keyword,
		(void *)&cmd_gso_enable_mode,
		NULL,
	},
};

/* *** SET MAX PACKET LENGTH FOR GSO SEGMENTS *** */
struct cmd_gso_size_result {
	cmdline_fixed_string_t cmd_set;
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t cmd_segsz;
	uint16_t cmd_size;
};

static void
cmd_gso_size_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_gso_size_result *res = parsed_result;

	if (test_done == 0) {
		fprintf(stderr,
			"Before setting GSO segsz, please first stop forwarding\n");
		return;
	}

	if (!strcmp(res->cmd_keyword, "gso") &&
			!strcmp(res->cmd_segsz, "segsz")) {
		if (res->cmd_size < RTE_GSO_SEG_SIZE_MIN)
			fprintf(stderr,
				"gso_size should be larger than %zu. Please input a legal value\n",
				RTE_GSO_SEG_SIZE_MIN);
		else
			gso_max_segment_size = res->cmd_size;
	}
}

cmdline_parse_token_string_t cmd_gso_size_set =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_size_result,
				cmd_set, "set");
cmdline_parse_token_string_t cmd_gso_size_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_size_result,
				cmd_keyword, "gso");
cmdline_parse_token_string_t cmd_gso_size_segsz =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_size_result,
				cmd_segsz, "segsz");
cmdline_parse_token_num_t cmd_gso_size_size =
	TOKEN_NUM_INITIALIZER(struct cmd_gso_size_result,
				cmd_size, RTE_UINT16);

cmdline_parse_inst_t cmd_gso_size = {
	.f = cmd_gso_size_parsed,
	.data = NULL,
	.help_str = "set gso segsz <length>",
	.tokens = {
		(void *)&cmd_gso_size_set,
		(void *)&cmd_gso_size_keyword,
		(void *)&cmd_gso_size_segsz,
		(void *)&cmd_gso_size_size,
		NULL,
	},
};

/* *** SHOW GSO CONFIGURATION *** */
struct cmd_gso_show_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_gso_show_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_gso_show_result *res = parsed_result;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "invalid port id %u\n", res->cmd_pid);
		return;
	}
	if (!strcmp(res->cmd_keyword, "gso")) {
		if (gso_ports[res->cmd_pid].enable) {
			printf("Max GSO'd packet size: %uB\n"
					"Supported GSO types: TCP/IPv4, "
					"UDP/IPv4, VxLAN with inner "
					"TCP/IPv4 packet, GRE with inner "
					"TCP/IPv4 packet\n",
					gso_max_segment_size);
		} else
			printf("GSO is not enabled on Port %u\n", res->cmd_pid);
	}
}

cmdline_parse_token_string_t cmd_gso_show_show =
TOKEN_STRING_INITIALIZER(struct cmd_gso_show_result,
		cmd_show, "show");
cmdline_parse_token_string_t cmd_gso_show_port =
TOKEN_STRING_INITIALIZER(struct cmd_gso_show_result,
		cmd_port, "port");
cmdline_parse_token_string_t cmd_gso_show_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_gso_show_result,
				cmd_keyword, "gso");
cmdline_parse_token_num_t cmd_gso_show_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_gso_show_result,
				cmd_pid, RTE_UINT16);

cmdline_parse_inst_t cmd_gso_show = {
	.f = cmd_gso_show_parsed,
	.data = NULL,
	.help_str = "show port <port_id> gso",
	.tokens = {
		(void *)&cmd_gso_show_show,
		(void *)&cmd_gso_show_port,
		(void *)&cmd_gso_show_pid,
		(void *)&cmd_gso_show_keyword,
		NULL,
	},
};
#endif /* RTE_LIB_GSO */

/* *** ENABLE/DISABLE FLUSH ON RX STREAMS *** */
struct cmd_set_flush_rx {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t flush_rx;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_flush_rx_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_flush_rx *res = parsed_result;

	if (num_procs > 1 && (strcmp(res->mode, "on") == 0)) {
		printf("multi-process doesn't support to flush Rx queues.\n");
		return;
	}

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
	.help_str = "set flush_rx on|off: Enable/Disable flush on rx streams",
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
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
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
	.help_str = "set link_check on|off: Enable/Disable link status check "
	            "when starting/stopping a port",
	.data = NULL,
	.tokens = {
		(void *)&cmd_setlinkcheck_set,
		(void *)&cmd_setlinkcheck_link_check,
		(void *)&cmd_setlinkcheck_mode,
		NULL,
	},
};

/* *** SET NIC BYPASS MODE *** */
struct cmd_set_bypass_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bypass;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t value;
	portid_t port_id;
};

static void
cmd_set_bypass_mode_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bypass_mode_result *res = parsed_result;
	portid_t port_id = res->port_id;
	int32_t rc = -EINVAL;

#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
	uint32_t bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_NORMAL;

	if (!strcmp(res->value, "bypass"))
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_BYPASS;
	else if (!strcmp(res->value, "isolate"))
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_ISOLATE;
	else
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_NORMAL;

	/* Set the bypass mode for the relevant port. */
	rc = rte_pmd_ixgbe_bypass_state_set(port_id, &bypass_mode);
#endif
	if (rc != 0)
		fprintf(stderr, "\t Failed to set bypass mode for port = %d.\n",
			port_id);
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
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_set_bypass_mode = {
	.f = cmd_set_bypass_mode_parsed,
	.help_str = "set bypass mode normal|bypass|isolate <port_id>: "
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
	portid_t port_id;
};

static void
cmd_set_bypass_event_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	int32_t rc = -EINVAL;
	struct cmd_set_bypass_event_result *res = parsed_result;
	portid_t port_id = res->port_id;

#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
	uint32_t bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_NONE;
	uint32_t bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_NORMAL;

	if (!strcmp(res->event_value, "timeout"))
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_TIMEOUT;
	else if (!strcmp(res->event_value, "os_on"))
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_OS_ON;
	else if (!strcmp(res->event_value, "os_off"))
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_OS_OFF;
	else if (!strcmp(res->event_value, "power_on"))
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_POWER_ON;
	else if (!strcmp(res->event_value, "power_off"))
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_POWER_OFF;
	else
		bypass_event = RTE_PMD_IXGBE_BYPASS_EVENT_NONE;

	if (!strcmp(res->mode_value, "bypass"))
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_BYPASS;
	else if (!strcmp(res->mode_value, "isolate"))
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_ISOLATE;
	else
		bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_NORMAL;

	/* Set the watchdog timeout. */
	if (bypass_event == RTE_PMD_IXGBE_BYPASS_EVENT_TIMEOUT) {

		rc = -EINVAL;
		if (RTE_PMD_IXGBE_BYPASS_TMT_VALID(bypass_timeout)) {
			rc = rte_pmd_ixgbe_bypass_wd_timeout_store(port_id,
							   bypass_timeout);
		}
		if (rc != 0) {
			fprintf(stderr,
				"Failed to set timeout value %u for port %d, errto code: %d.\n",
				bypass_timeout, port_id, rc);
		}
	}

	/* Set the bypass event to transition to bypass mode. */
	rc = rte_pmd_ixgbe_bypass_event_store(port_id, bypass_event,
					      bypass_mode);
#endif

	if (rc != 0)
		fprintf(stderr, "\t Failed to set bypass event for port = %d.\n",
			port_id);
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
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_set_bypass_event = {
	.f = cmd_set_bypass_event_parsed,
	.help_str = "set bypass event none|timeout|os_on|os_off|power_on|"
		"power_off mode normal|bypass|isolate <port_id>: "
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
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	__rte_unused struct cmd_set_bypass_timeout_result *res = parsed_result;

#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
	if (!strcmp(res->value, "1.5"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_1_5_SEC;
	else if (!strcmp(res->value, "2"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_2_SEC;
	else if (!strcmp(res->value, "3"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_3_SEC;
	else if (!strcmp(res->value, "4"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_4_SEC;
	else if (!strcmp(res->value, "8"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_8_SEC;
	else if (!strcmp(res->value, "16"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_16_SEC;
	else if (!strcmp(res->value, "32"))
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_32_SEC;
	else
		bypass_timeout = RTE_PMD_IXGBE_BYPASS_TMT_OFF;
#endif
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
	.help_str = "set bypass timeout 0|1.5|2|3|4|8|16|32: "
		"Set the NIC bypass watchdog timeout in seconds",
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
	portid_t port_id;
};

static void
cmd_show_bypass_config_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_bypass_config_result *res = parsed_result;
	portid_t port_id = res->port_id;
	int rc = -EINVAL;
#if defined RTE_NET_IXGBE && defined RTE_LIBRTE_IXGBE_BYPASS
	uint32_t event_mode;
	uint32_t bypass_mode;
	uint32_t timeout = bypass_timeout;
	unsigned int i;

	static const char * const timeouts[RTE_PMD_IXGBE_BYPASS_TMT_NUM] =
		{"off", "1.5", "2", "3", "4", "8", "16", "32"};
	static const char * const modes[RTE_PMD_IXGBE_BYPASS_MODE_NUM] =
		{"UNKNOWN", "normal", "bypass", "isolate"};
	static const char * const events[RTE_PMD_IXGBE_BYPASS_EVENT_NUM] = {
		"NONE",
		"OS/board on",
		"power supply on",
		"OS/board off",
		"power supply off",
		"timeout"};

	/* Display the bypass mode.*/
	if (rte_pmd_ixgbe_bypass_state_show(port_id, &bypass_mode) != 0) {
		fprintf(stderr, "\tFailed to get bypass mode for port = %d\n",
			port_id);
		return;
	}
	else {
		if (!RTE_PMD_IXGBE_BYPASS_MODE_VALID(bypass_mode))
			bypass_mode = RTE_PMD_IXGBE_BYPASS_MODE_NONE;

		printf("\tbypass mode    = %s\n",  modes[bypass_mode]);
	}

	/* Display the bypass timeout.*/
	if (!RTE_PMD_IXGBE_BYPASS_TMT_VALID(timeout))
		timeout = RTE_PMD_IXGBE_BYPASS_TMT_OFF;

	printf("\tbypass timeout = %s\n", timeouts[timeout]);

	/* Display the bypass events and associated modes. */
	for (i = RTE_PMD_IXGBE_BYPASS_EVENT_START; i < RTE_DIM(events); i++) {

		if (rte_pmd_ixgbe_bypass_event_show(port_id, i, &event_mode)) {
			fprintf(stderr,
				"\tFailed to get bypass mode for event = %s\n",
				events[i]);
		} else {
			if (!RTE_PMD_IXGBE_BYPASS_MODE_VALID(event_mode))
				event_mode = RTE_PMD_IXGBE_BYPASS_MODE_NONE;

			printf("\tbypass event: %-16s = %s\n", events[i],
				modes[event_mode]);
		}
	}
#endif
	if (rc != 0)
		fprintf(stderr,
			"\tFailed to get bypass configuration for port = %d\n",
		       port_id);
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
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_show_bypass_config = {
	.f = cmd_show_bypass_config_parsed,
	.help_str = "show bypass config <port_id>: "
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

#ifdef RTE_NET_BOND
/* *** SET BONDING MODE *** */
struct cmd_set_bonding_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t mode;
	uint8_t value;
	portid_t port_id;
};

static void cmd_set_bonding_mode_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bonding_mode_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_port *port = &ports[port_id];

	/*
	 * Bonding mode changed means resources of device changed, like whether
	 * started rte timer or not. Device should be restarted when resources
	 * of device changed.
	 */
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"\t Error: Can't set bonding mode when port %d is not stopped\n",
			port_id);
		return;
	}

	/* Set the bonding mode for the relevant port. */
	if (0 != rte_eth_bond_mode_set(port_id, res->value))
		fprintf(stderr, "\t Failed to set bonding mode for port = %d.\n",
			port_id);
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
		value, RTE_UINT8);
cmdline_parse_token_num_t cmd_setbonding_mode_port =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_mode_result,
		port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_set_bonding_mode = {
		.f = cmd_set_bonding_mode_parsed,
		.help_str = "set bonding mode <mode_value> <port_id>: "
			"Set the bonding mode for port_id",
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

/* *** SET BONDING SLOW_QUEUE SW/HW *** */
struct cmd_set_bonding_lacp_dedicated_queues_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t lacp;
	cmdline_fixed_string_t dedicated_queues;
	portid_t port_id;
	cmdline_fixed_string_t mode;
};

static void cmd_set_bonding_lacp_dedicated_queues_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bonding_lacp_dedicated_queues_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_port *port;

	port = &ports[port_id];

	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", port_id);
		return;
	}

	if (!strcmp(res->mode, "enable")) {
		if (rte_eth_bond_8023ad_dedicated_queues_enable(port_id) == 0)
			printf("Dedicate queues for LACP control packets"
					" enabled\n");
		else
			printf("Enabling dedicate queues for LACP control "
					"packets on port %d failed\n", port_id);
	} else if (!strcmp(res->mode, "disable")) {
		if (rte_eth_bond_8023ad_dedicated_queues_disable(port_id) == 0)
			printf("Dedicated queues for LACP control packets "
					"disabled\n");
		else
			printf("Disabling dedicated queues for LACP control "
					"traffic on port %d failed\n", port_id);
	}
}

cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_set =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		set, "set");
cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_lacp =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		lacp, "lacp");
cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_dedicated_queues =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		dedicated_queues, "dedicated_queues");
cmdline_parse_token_num_t cmd_setbonding_lacp_dedicated_queues_port_id =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_setbonding_lacp_dedicated_queues_mode =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_lacp_dedicated_queues_result,
		mode, "enable#disable");

cmdline_parse_inst_t cmd_set_lacp_dedicated_queues = {
		.f = cmd_set_bonding_lacp_dedicated_queues_parsed,
		.help_str = "set bonding lacp dedicated_queues <port_id> "
			"enable|disable: "
			"Enable/disable dedicated queues for LACP control traffic for port_id",
		.data = NULL,
		.tokens = {
			(void *)&cmd_setbonding_lacp_dedicated_queues_set,
			(void *)&cmd_setbonding_lacp_dedicated_queues_bonding,
			(void *)&cmd_setbonding_lacp_dedicated_queues_lacp,
			(void *)&cmd_setbonding_lacp_dedicated_queues_dedicated_queues,
			(void *)&cmd_setbonding_lacp_dedicated_queues_port_id,
			(void *)&cmd_setbonding_lacp_dedicated_queues_mode,
			NULL
		}
};

/* *** SET BALANCE XMIT POLICY *** */
struct cmd_set_bonding_balance_xmit_policy_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t balance_xmit_policy;
	portid_t port_id;
	cmdline_fixed_string_t policy;
};

static void cmd_set_bonding_balance_xmit_policy_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
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
		fprintf(stderr, "\t Invalid xmit policy selection");
		return;
	}

	/* Set the bonding mode for the relevant port. */
	if (0 != rte_eth_bond_xmit_policy_set(port_id, policy)) {
		fprintf(stderr,
			"\t Failed to set bonding balance xmit policy for port = %d.\n",
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
		port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_setbonding_balance_xmit_policy_policy =
TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_balance_xmit_policy_result,
		policy, "l2#l23#l34");

cmdline_parse_inst_t cmd_set_balance_xmit_policy = {
		.f = cmd_set_bonding_balance_xmit_policy_parsed,
		.help_str = "set bonding balance_xmit_policy <port_id> "
			"l2|l23|l34: "
			"Set the bonding balance_xmit_policy for port_id",
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

/* *** SHOW IEEE802.3 BONDING INFORMATION *** */
struct cmd_show_bonding_lacp_info_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t lacp;
	cmdline_fixed_string_t info;
	portid_t port_id;
};

static void port_param_show(struct port_params *params)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	printf("\t\tsystem priority: %u\n", params->system_priority);
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &params->system);
	printf("\t\tsystem mac address: %s\n", buf);
	printf("\t\tport key: %u\n", params->key);
	printf("\t\tport priority: %u\n", params->port_priority);
	printf("\t\tport number: %u\n", params->port_number);
}

static void lacp_slave_info_show(struct rte_eth_bond_8023ad_slave_info *info)
{
	char a_state[256] = { 0 };
	char p_state[256] = { 0 };
	int a_len = 0;
	int p_len = 0;
	uint32_t i;

	static const char * const state[] = {
		"ACTIVE",
		"TIMEOUT",
		"AGGREGATION",
		"SYNCHRONIZATION",
		"COLLECTING",
		"DISTRIBUTING",
		"DEFAULTED",
		"EXPIRED"
	};
	static const char * const selection[] = {
		"UNSELECTED",
		"STANDBY",
		"SELECTED"
	};

	for (i = 0; i < RTE_DIM(state); i++) {
		if ((info->actor_state >> i) & 1)
			a_len += snprintf(&a_state[a_len],
						RTE_DIM(a_state) - a_len, "%s ",
						state[i]);

		if ((info->partner_state >> i) & 1)
			p_len += snprintf(&p_state[p_len],
						RTE_DIM(p_state) - p_len, "%s ",
						state[i]);
	}
	printf("\tAggregator port id: %u\n", info->agg_port_id);
	printf("\tselection: %s\n", selection[info->selected]);
	printf("\tActor detail info:\n");
	port_param_show(&info->actor);
	printf("\t\tport state: %s\n", a_state);
	printf("\tPartner detail info:\n");
	port_param_show(&info->partner);
	printf("\t\tport state: %s\n", p_state);
	printf("\n");
}

static void lacp_conf_show(struct rte_eth_bond_8023ad_conf *conf)
{
	printf("\tfast period: %u ms\n", conf->fast_periodic_ms);
	printf("\tslow period: %u ms\n", conf->slow_periodic_ms);
	printf("\tshort timeout: %u ms\n", conf->short_timeout_ms);
	printf("\tlong timeout: %u ms\n", conf->long_timeout_ms);
	printf("\taggregate wait timeout: %u ms\n",
			conf->aggregate_wait_timeout_ms);
	printf("\ttx period: %u ms\n", conf->tx_period_ms);
	printf("\trx marker period: %u ms\n", conf->rx_marker_period_ms);
	printf("\tupdate timeout: %u ms\n", conf->update_timeout_ms);
	switch (conf->agg_selection) {
	case AGG_BANDWIDTH:
		printf("\taggregation mode: bandwidth\n");
		break;
	case AGG_STABLE:
		printf("\taggregation mode: stable\n");
		break;
	case AGG_COUNT:
		printf("\taggregation mode: count\n");
		break;
	default:
		printf("\taggregation mode: invalid\n");
		break;
	}

	printf("\n");
}

static void cmd_show_bonding_lacp_info_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_bonding_lacp_info_result *res = parsed_result;
	struct rte_eth_bond_8023ad_slave_info slave_info;
	struct rte_eth_bond_8023ad_conf port_conf;
	portid_t slaves[RTE_MAX_ETHPORTS];
	portid_t port_id = res->port_id;
	int num_active_slaves;
	int bonding_mode;
	int i;
	int ret;

	bonding_mode = rte_eth_bond_mode_get(port_id);
	if (bonding_mode != BONDING_MODE_8023AD) {
		fprintf(stderr, "\tBonding mode is not mode 4\n");
		return;
	}

	num_active_slaves = rte_eth_bond_active_slaves_get(port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (num_active_slaves < 0) {
		fprintf(stderr, "\tFailed to get active slave list for port = %u\n",
				port_id);
		return;
	}
	if (num_active_slaves == 0)
		fprintf(stderr, "\tIEEE802.3 port %u has no active slave\n",
			port_id);

	printf("\tIEEE802.3 port: %u\n", port_id);
	ret = rte_eth_bond_8023ad_conf_get(port_id, &port_conf);
	if (ret) {
		fprintf(stderr, "\tGet bonded device %u info failed\n",
			port_id);
		return;
	}
	lacp_conf_show(&port_conf);

	for (i = 0; i < num_active_slaves; i++) {
		ret = rte_eth_bond_8023ad_slave_info(port_id, slaves[i],
				&slave_info);
		if (ret) {
			fprintf(stderr, "\tGet slave device %u info failed\n",
				slaves[i]);
			return;
		}
		printf("\tSlave Port: %u\n", slaves[i]);
		lacp_slave_info_show(&slave_info);
	}
}

cmdline_parse_token_string_t cmd_show_bonding_lacp_info_show =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		show, "show");
cmdline_parse_token_string_t cmd_show_bonding_lacp_info_bonding =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		bonding, "bonding");
cmdline_parse_token_string_t cmd_show_bonding_lacp_info_lacp =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		bonding, "lacp");
cmdline_parse_token_string_t cmd_show_bonding_lacp_info_info =
TOKEN_STRING_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		info, "info");
cmdline_parse_token_num_t cmd_show_bonding_lacp_info_port_id =
TOKEN_NUM_INITIALIZER(struct cmd_show_bonding_lacp_info_result,
		port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_show_bonding_lacp_info = {
		.f = cmd_show_bonding_lacp_info_parsed,
		.help_str = "show bonding lacp info <port_id> : "
			"Show bonding IEEE802.3 information for port_id",
		.data = NULL,
		.tokens = {
			(void *)&cmd_show_bonding_lacp_info_show,
			(void *)&cmd_show_bonding_lacp_info_bonding,
			(void *)&cmd_show_bonding_lacp_info_lacp,
			(void *)&cmd_show_bonding_lacp_info_info,
			(void *)&cmd_show_bonding_lacp_info_port_id,
			NULL
		}
};

/* *** SHOW NIC BONDING CONFIGURATION *** */
struct cmd_show_bonding_config_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t config;
	portid_t port_id;
};

static void cmd_show_bonding_config_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_bonding_config_result *res = parsed_result;
	int bonding_mode, agg_mode;
	portid_t slaves[RTE_MAX_ETHPORTS];
	int num_slaves, num_active_slaves;
	int primary_id;
	int i;
	portid_t port_id = res->port_id;

	/* Display the bonding mode.*/
	bonding_mode = rte_eth_bond_mode_get(port_id);
	if (bonding_mode < 0) {
		fprintf(stderr, "\tFailed to get bonding mode for port = %d\n",
			port_id);
		return;
	} else
		printf("\tBonding mode: %d\n", bonding_mode);

	if (bonding_mode == BONDING_MODE_BALANCE ||
		bonding_mode == BONDING_MODE_8023AD) {
		int balance_xmit_policy;

		balance_xmit_policy = rte_eth_bond_xmit_policy_get(port_id);
		if (balance_xmit_policy < 0) {
			fprintf(stderr,
				"\tFailed to get balance xmit policy for port = %d\n",
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

	if (bonding_mode == BONDING_MODE_8023AD) {
		agg_mode = rte_eth_bond_8023ad_agg_selection_get(port_id);
		printf("\tIEEE802.3AD Aggregator Mode: ");
		switch (agg_mode) {
		case AGG_BANDWIDTH:
			printf("bandwidth");
			break;
		case AGG_STABLE:
			printf("stable");
			break;
		case AGG_COUNT:
			printf("count");
			break;
		}
		printf("\n");
	}

	num_slaves = rte_eth_bond_slaves_get(port_id, slaves, RTE_MAX_ETHPORTS);

	if (num_slaves < 0) {
		fprintf(stderr, "\tFailed to get slave list for port = %d\n",
			port_id);
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
		fprintf(stderr,
			"\tFailed to get active slave list for port = %d\n",
			port_id);
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
		fprintf(stderr, "\tFailed to get primary slave for port = %d\n",
			port_id);
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
		port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_show_bonding_config = {
		.f = cmd_show_bonding_config_parsed,
		.help_str = "show bonding config <port_id>: "
			"Show the bonding config for port_id",
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
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_set_bonding_primary_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bonding_primary_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* Set the primary slave for a bonded device. */
	if (0 != rte_eth_bond_primary_set(master_port_id, slave_port_id)) {
		fprintf(stderr, "\t Failed to set primary slave for port = %d.\n",
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
		slave_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_setbonding_primary_port =
TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_primary_result,
		port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_set_bonding_primary = {
		.f = cmd_set_bonding_primary_parsed,
		.help_str = "set bonding primary <slave_id> <port_id>: "
			"Set the primary slave for port_id",
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
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_add_bonding_slave_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_add_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* add the slave for a bonded device. */
	if (0 != rte_eth_bond_slave_add(master_port_id, slave_port_id)) {
		fprintf(stderr,
			"\t Failed to add slave %d to master port = %d.\n",
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
		slave_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_addbonding_slave_port =
TOKEN_NUM_INITIALIZER(struct cmd_add_bonding_slave_result,
		port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_add_bonding_slave = {
		.f = cmd_add_bonding_slave_parsed,
		.help_str = "add bonding slave <slave_id> <port_id>: "
			"Add a slave device to a bonded device",
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
	portid_t slave_id;
	portid_t port_id;
};

static void cmd_remove_bonding_slave_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_remove_bonding_slave_result *res = parsed_result;
	portid_t master_port_id = res->port_id;
	portid_t slave_port_id = res->slave_id;

	/* remove the slave from a bonded device. */
	if (0 != rte_eth_bond_slave_remove(master_port_id, slave_port_id)) {
		fprintf(stderr,
			"\t Failed to remove slave %d from master port = %d.\n",
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
				slave_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_removebonding_slave_port =
		TOKEN_NUM_INITIALIZER(struct cmd_remove_bonding_slave_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_remove_bonding_slave = {
		.f = cmd_remove_bonding_slave_parsed,
		.help_str = "remove bonding slave <slave_id> <port_id>: "
			"Remove a slave device from a bonded device",
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
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_create_bonded_device_result *res = parsed_result;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];
	int port_id;
	int ret;

	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
		return;
	}

	snprintf(ethdev_name, RTE_ETH_NAME_MAX_LEN, "net_bonding_testpmd_%d",
			bond_dev_num++);

	/* Create a new bonded device. */
	port_id = rte_eth_bond_create(ethdev_name, res->mode, res->socket);
	if (port_id < 0) {
		fprintf(stderr, "\t Failed to create bonded device.\n");
		return;
	} else {
		printf("Created new bonded device %s on (port %d).\n", ethdev_name,
				port_id);

		/* Update number of ports */
		nb_ports = rte_eth_dev_count_avail();
		reconfig(port_id, res->socket);
		ret = rte_eth_promiscuous_enable(port_id);
		if (ret != 0)
			fprintf(stderr,
				"Failed to enable promiscuous mode for port %u: %s - ignore\n",
				port_id, rte_strerror(-ret));

		ports[port_id].bond_flag = 1;
		ports[port_id].need_setup = 0;
		ports[port_id].port_status = RTE_PORT_STOPPED;
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
				mode, RTE_UINT8);
cmdline_parse_token_num_t cmd_createbonded_device_socket =
		TOKEN_NUM_INITIALIZER(struct cmd_create_bonded_device_result,
				socket, RTE_UINT8);

cmdline_parse_inst_t cmd_create_bonded_device = {
		.f = cmd_create_bonded_device_parsed,
		.help_str = "create bonded device <mode> <socket>: "
			"Create a new bonded device with specific bonding mode and socket",
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
	uint16_t port_num;
	struct rte_ether_addr address;
};

static void cmd_set_bond_mac_addr_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bond_mac_addr_result *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_num, ENABLED_WARN))
		return;

	ret = rte_eth_bond_mac_address_set(res->port_num, &res->address);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		fprintf(stderr, "set_bond_mac_addr error: (%s)\n",
			strerror(-ret));
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
		TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mac_addr_result,
				port_num, RTE_UINT16);
cmdline_parse_token_etheraddr_t cmd_set_bond_mac_addr_addr =
		TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_bond_mac_addr_result, address);

cmdline_parse_inst_t cmd_set_bond_mac_addr = {
		.f = cmd_set_bond_mac_addr_parsed,
		.data = (void *) 0,
		.help_str = "set bonding mac_addr <port_id> <mac_addr>",
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
	uint16_t port_num;
	uint32_t period_ms;
};

static void cmd_set_bond_mon_period_parsed(void *parsed_result,
		__rte_unused  struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bond_mon_period_result *res = parsed_result;
	int ret;

	ret = rte_eth_bond_link_monitoring_set(res->port_num, res->period_ms);

	/* check the return value and print it if is < 0 */
	if (ret < 0)
		fprintf(stderr, "set_bond_mac_addr error: (%s)\n",
			strerror(-ret));
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
				port_num, RTE_UINT16);
cmdline_parse_token_num_t cmd_set_bond_mon_period_period_ms =
		TOKEN_NUM_INITIALIZER(struct cmd_set_bond_mon_period_result,
				period_ms, RTE_UINT32);

cmdline_parse_inst_t cmd_set_bond_mon_period = {
		.f = cmd_set_bond_mon_period_parsed,
		.data = (void *) 0,
		.help_str = "set bonding mon_period <port_id> <period_ms>",
		.tokens = {
				(void *)&cmd_set_bond_mon_period_set,
				(void *)&cmd_set_bond_mon_period_bonding,
				(void *)&cmd_set_bond_mon_period_mon_period,
				(void *)&cmd_set_bond_mon_period_portnum,
				(void *)&cmd_set_bond_mon_period_period_ms,
				NULL
		}
};



struct cmd_set_bonding_agg_mode_policy_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t bonding;
	cmdline_fixed_string_t agg_mode;
	uint16_t port_num;
	cmdline_fixed_string_t policy;
};


static void
cmd_set_bonding_agg_mode(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_set_bonding_agg_mode_policy_result *res = parsed_result;
	uint8_t policy = AGG_BANDWIDTH;

	if (!strcmp(res->policy, "bandwidth"))
		policy = AGG_BANDWIDTH;
	else if (!strcmp(res->policy, "stable"))
		policy = AGG_STABLE;
	else if (!strcmp(res->policy, "count"))
		policy = AGG_COUNT;

	rte_eth_bond_8023ad_agg_selection_set(res->port_num, policy);
}


cmdline_parse_token_string_t cmd_set_bonding_agg_mode_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
				set, "set");
cmdline_parse_token_string_t cmd_set_bonding_agg_mode_bonding =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
				bonding, "bonding");

cmdline_parse_token_string_t cmd_set_bonding_agg_mode_agg_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
				agg_mode, "agg_mode");

cmdline_parse_token_num_t cmd_set_bonding_agg_mode_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_set_bonding_agg_mode_policy_result,
				port_num, RTE_UINT16);

cmdline_parse_token_string_t cmd_set_bonding_agg_mode_policy_string =
	TOKEN_STRING_INITIALIZER(
			struct cmd_set_bonding_balance_xmit_policy_result,
		policy, "stable#bandwidth#count");

cmdline_parse_inst_t cmd_set_bonding_agg_mode_policy = {
	.f = cmd_set_bonding_agg_mode,
	.data = (void *) 0,
	.help_str = "set bonding mode IEEE802.3AD aggregator policy <port_id> <agg_name>",
	.tokens = {
			(void *)&cmd_set_bonding_agg_mode_set,
			(void *)&cmd_set_bonding_agg_mode_bonding,
			(void *)&cmd_set_bonding_agg_mode_agg_mode,
			(void *)&cmd_set_bonding_agg_mode_portnum,
			(void *)&cmd_set_bonding_agg_mode_policy_string,
			NULL
		}
};


#endif /* RTE_NET_BOND */

/* *** SET FORWARDING MODE *** */
struct cmd_set_fwd_mode_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t fwd;
	cmdline_fixed_string_t mode;
};

static void cmd_set_fwd_mode_parsed(void *parsed_result,
				    __rte_unused struct cmdline *cl,
				    __rte_unused void *data)
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
	snprintf(help, sizeof(help), "set fwd %s: "
		"Set packet forwarding mode", modes);
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
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
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
	snprintf(help, sizeof(help), "set fwd %s retry: "
		"Set packet forwarding mode with retry", modes);
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
					__rte_unused struct cmdline *cl,
					__rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_set_burst_tx_retry_result, time,
				 RTE_UINT32);
cmdline_parse_token_string_t cmd_set_burst_tx_retry_retry =
	TOKEN_STRING_INITIALIZER(struct cmd_set_burst_tx_retry_result, retry, "retry");
cmdline_parse_token_num_t cmd_set_burst_tx_retry_retry_num =
	TOKEN_NUM_INITIALIZER(struct cmd_set_burst_tx_retry_result, retry_num,
				 RTE_UINT32);

cmdline_parse_inst_t cmd_set_burst_tx_retry = {
	.f = cmd_set_burst_tx_retry_parsed,
	.help_str = "set burst tx delay <delay_usec> retry <num_retry>",
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
	uint16_t port_num;               /* valid if "allports" argument == 0 */
	cmdline_fixed_string_t mode;
};

static void cmd_set_promisc_mode_parsed(void *parsed_result,
					__rte_unused struct cmdline *cl,
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
		RTE_ETH_FOREACH_DEV(i)
			eth_set_promisc_mode(i, enable);
	} else {
		eth_set_promisc_mode(res->port_num, enable);
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
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_setpromisc_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_promisc_mode_result, mode,
				 "on#off");

cmdline_parse_inst_t cmd_set_promisc_mode_all = {
	.f = cmd_set_promisc_mode_parsed,
	.data = (void *)1,
	.help_str = "set promisc all on|off: Set promisc mode for all ports",
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
	.help_str = "set promisc <port_id> on|off: Set promisc mode on port_id",
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
	uint16_t port_num;               /* valid if "allports" argument == 0 */
	cmdline_fixed_string_t mode;
};

static void cmd_set_allmulti_mode_parsed(void *parsed_result,
					__rte_unused struct cmdline *cl,
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
		RTE_ETH_FOREACH_DEV(i) {
			eth_set_allmulticast_mode(i, enable);
		}
	}
	else {
		eth_set_allmulticast_mode(res->port_num, enable);
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
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_setallmulti_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_allmulti_mode_result, mode,
				 "on#off");

cmdline_parse_inst_t cmd_set_allmulti_mode_all = {
	.f = cmd_set_allmulti_mode_parsed,
	.data = (void *)1,
	.help_str = "set allmulti all on|off: Set allmulti mode for all ports",
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
	.help_str = "set allmulti <port_id> on|off: "
		"Set allmulti mode on port_id",
	.tokens = {
		(void *)&cmd_setallmulti_set,
		(void *)&cmd_setallmulti_allmulti,
		(void *)&cmd_setallmulti_portnum,
		(void *)&cmd_setallmulti_mode,
		NULL,
	},
};

/* *** GET CURRENT ETHERNET LINK FLOW CONTROL *** */
struct cmd_link_flow_ctrl_show {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t flow_ctrl;
};

cmdline_parse_token_string_t cmd_lfc_show_show =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_show,
				show, "show");
cmdline_parse_token_string_t cmd_lfc_show_port =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_show,
				port, "port");
cmdline_parse_token_num_t cmd_lfc_show_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_show,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_lfc_show_flow_ctrl =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_show,
				flow_ctrl, "flow_ctrl");

static void
cmd_link_flow_ctrl_show_parsed(void *parsed_result,
			      __rte_unused struct cmdline *cl,
			      __rte_unused void *data)
{
	struct cmd_link_flow_ctrl_show *res = parsed_result;
	static const char *info_border = "*********************";
	struct rte_eth_fc_conf fc_conf;
	bool rx_fc_en = false;
	bool tx_fc_en = false;
	int ret;

	ret = rte_eth_dev_flow_ctrl_get(res->port_id, &fc_conf);
	if (ret != 0) {
		fprintf(stderr,
			"Failed to get current flow ctrl information: err = %d\n",
			ret);
		return;
	}

	if (fc_conf.mode == RTE_ETH_FC_RX_PAUSE || fc_conf.mode == RTE_ETH_FC_FULL)
		rx_fc_en = true;
	if (fc_conf.mode == RTE_ETH_FC_TX_PAUSE || fc_conf.mode == RTE_ETH_FC_FULL)
		tx_fc_en = true;

	printf("\n%s Flow control infos for port %-2d %s\n",
		info_border, res->port_id, info_border);
	printf("FC mode:\n");
	printf("   Rx pause: %s\n", rx_fc_en ? "on" : "off");
	printf("   Tx pause: %s\n", tx_fc_en ? "on" : "off");
	printf("Autoneg: %s\n", fc_conf.autoneg ? "on" : "off");
	printf("Pause time: 0x%x\n", fc_conf.pause_time);
	printf("High waterline: 0x%x\n", fc_conf.high_water);
	printf("Low waterline: 0x%x\n", fc_conf.low_water);
	printf("Send XON: %s\n", fc_conf.send_xon ? "on" : "off");
	printf("Forward MAC control frames: %s\n",
		fc_conf.mac_ctrl_frame_fwd ? "on" : "off");
	printf("\n%s**************   End  ***********%s\n",
		info_border, info_border);
}

cmdline_parse_inst_t cmd_link_flow_control_show = {
	.f = cmd_link_flow_ctrl_show_parsed,
	.data = NULL,
	.help_str = "show port <port_id> flow_ctrl",
	.tokens = {
		(void *)&cmd_lfc_show_show,
		(void *)&cmd_lfc_show_port,
		(void *)&cmd_lfc_show_portid,
		(void *)&cmd_lfc_show_flow_ctrl,
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
	portid_t port_id;
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
				high_water, RTE_UINT32);
cmdline_parse_token_string_t cmd_lfc_set_low_water_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				lw_str, "low_water");
cmdline_parse_token_num_t cmd_lfc_set_low_water =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				low_water, RTE_UINT32);
cmdline_parse_token_string_t cmd_lfc_set_pause_time_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				pt_str, "pause_time");
cmdline_parse_token_num_t cmd_lfc_set_pause_time =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				pause_time, RTE_UINT16);
cmdline_parse_token_string_t cmd_lfc_set_send_xon_str =
	TOKEN_STRING_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				xon_str, "send_xon");
cmdline_parse_token_num_t cmd_lfc_set_send_xon =
	TOKEN_NUM_INITIALIZER(struct cmd_link_flow_ctrl_set_result,
				send_xon, RTE_UINT16);
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
				port_id, RTE_UINT16);

/* forward declaration */
static void
cmd_link_flow_ctrl_set_parsed(void *parsed_result, struct cmdline *cl,
			      void *data);

cmdline_parse_inst_t cmd_link_flow_control_set = {
	.f = cmd_link_flow_ctrl_set_parsed,
	.data = NULL,
	.help_str = "set flow_ctrl rx on|off tx on|off <high_water> "
		"<low_water> <pause_time> <send_xon> mac_ctrl_frame_fwd on|off "
		"autoneg on|off <port_id>: Configure the Ethernet flow control",
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
	.help_str = "set flow_ctrl rx on|off <port_id>: "
		"Change rx flow control parameter",
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
	.help_str = "set flow_ctrl tx on|off <port_id>: "
		"Change tx flow control parameter",
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
	.help_str = "set flow_ctrl high_water <value> <port_id>: "
		"Change high water flow control parameter",
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
	.help_str = "set flow_ctrl low_water <value> <port_id>: "
		"Change low water flow control parameter",
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
	.help_str = "set flow_ctrl pause_time <value> <port_id>: "
		"Change pause time flow control parameter",
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
	.help_str = "set flow_ctrl send_xon <value> <port_id>: "
		"Change send_xon flow control parameter",
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
	.help_str = "set flow_ctrl mac_ctrl_frame_fwd on|off <port_id>: "
		"Change mac ctrl fwd flow control parameter",
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
	.help_str = "set flow_ctrl autoneg on|off <port_id>: "
		"Change autoneg flow control parameter",
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
			      __rte_unused struct cmdline *cl,
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
	 * the RTE_ETH_FC_TX_PAUSE, Transmit pause frame at the Rx side.
	 * Tx on/off, flow control is enabled/disabled on TX side. This can indicate
	 * the RTE_ETH_FC_RX_PAUSE, Respond to the pause frame at the Tx side.
	 */
	static enum rte_eth_fc_mode rx_tx_onoff_2_lfc_mode[2][2] = {
			{RTE_ETH_FC_NONE, RTE_ETH_FC_TX_PAUSE}, {RTE_ETH_FC_RX_PAUSE, RTE_ETH_FC_FULL}
	};

	/* Partial command line, retrieve current configuration */
	if (cmd) {
		ret = rte_eth_dev_flow_ctrl_get(res->port_id, &fc_conf);
		if (ret != 0) {
			fprintf(stderr,
				"cannot get current flow ctrl parameters, return code = %d\n",
				ret);
			return;
		}

		if ((fc_conf.mode == RTE_ETH_FC_RX_PAUSE) ||
		    (fc_conf.mode == RTE_ETH_FC_FULL))
			rx_fc_en = 1;
		if ((fc_conf.mode == RTE_ETH_FC_TX_PAUSE) ||
		    (fc_conf.mode == RTE_ETH_FC_FULL))
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
		fprintf(stderr,
			"bad flow control parameter, return code = %d\n",
			ret);
}

/* *** SETUP ETHERNET PRIORITY FLOW CONTROL *** */
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
	portid_t port_id;
};

static void
cmd_priority_flow_ctrl_set_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	struct cmd_priority_flow_ctrl_set_result *res = parsed_result;
	struct rte_eth_pfc_conf pfc_conf;
	int rx_fc_enable, tx_fc_enable;
	int ret;

	/*
	 * Rx on/off, flow control is enabled/disabled on RX side. This can indicate
	 * the RTE_ETH_FC_TX_PAUSE, Transmit pause frame at the Rx side.
	 * Tx on/off, flow control is enabled/disabled on TX side. This can indicate
	 * the RTE_ETH_FC_RX_PAUSE, Respond to the pause frame at the Tx side.
	 */
	static enum rte_eth_fc_mode rx_tx_onoff_2_pfc_mode[2][2] = {
		{RTE_ETH_FC_NONE, RTE_ETH_FC_TX_PAUSE}, {RTE_ETH_FC_RX_PAUSE, RTE_ETH_FC_FULL}
	};

	memset(&pfc_conf, 0, sizeof(struct rte_eth_pfc_conf));
	rx_fc_enable = (!strncmp(res->rx_pfc_mode, "on",2)) ? 1 : 0;
	tx_fc_enable = (!strncmp(res->tx_pfc_mode, "on",2)) ? 1 : 0;
	pfc_conf.fc.mode       = rx_tx_onoff_2_pfc_mode[rx_fc_enable][tx_fc_enable];
	pfc_conf.fc.high_water = res->high_water;
	pfc_conf.fc.low_water  = res->low_water;
	pfc_conf.fc.pause_time = res->pause_time;
	pfc_conf.priority      = res->priority;

	ret = rte_eth_dev_priority_flow_ctrl_set(res->port_id, &pfc_conf);
	if (ret != 0)
		fprintf(stderr,
			"bad priority flow control parameter, return code = %d\n",
			ret);
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
				high_water, RTE_UINT32);
cmdline_parse_token_num_t cmd_pfc_set_low_water =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				low_water, RTE_UINT32);
cmdline_parse_token_num_t cmd_pfc_set_pause_time =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				pause_time, RTE_UINT16);
cmdline_parse_token_num_t cmd_pfc_set_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				priority, RTE_UINT8);
cmdline_parse_token_num_t cmd_pfc_set_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_priority_flow_ctrl_set_result,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_priority_flow_control_set = {
	.f = cmd_priority_flow_ctrl_set_parsed,
	.data = NULL,
	.help_str = "set pfc_ctrl rx on|off tx on|off <high_water> <low_water> "
		"<pause_time> <priority> <port_id>: "
		"Configure the Ethernet priority flow control",
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

static void cmd_reset_parsed(__rte_unused void *parsed_result,
			     struct cmdline *cl,
			     __rte_unused void *data)
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
	.help_str = "set default: Reset default forwarding configuration",
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

static void cmd_start_parsed(__rte_unused void *parsed_result,
			     __rte_unused struct cmdline *cl,
			     __rte_unused void *data)
{
	start_packet_forwarding(0);
}

cmdline_parse_inst_t cmd_start = {
	.f = cmd_start_parsed,
	.data = NULL,
	.help_str = "start: Start packet forwarding",
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
cmd_start_tx_first_parsed(__rte_unused void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
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
	.help_str = "start tx_first: Start packet forwarding, "
		"after sending 1 burst of packets",
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
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
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
			tx_num, RTE_UINT32);

cmdline_parse_inst_t cmd_start_tx_first_n = {
	.f = cmd_start_tx_first_n_parsed,
	.data = NULL,
	.help_str = "start tx_first <num>: "
		"packet forwarding, after sending <num> bursts of packets",
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
	portid_t port_id;
};

cmdline_parse_token_string_t cmd_set_link_up_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, set, "set");
cmdline_parse_token_string_t cmd_set_link_up_link_up =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, link_up,
				"link-up");
cmdline_parse_token_string_t cmd_set_link_up_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_up_result, port, "port");
cmdline_parse_token_num_t cmd_set_link_up_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_link_up_result, port_id,
				RTE_UINT16);

static void cmd_set_link_up_parsed(__rte_unused void *parsed_result,
			     __rte_unused struct cmdline *cl,
			     __rte_unused void *data)
{
	struct cmd_set_link_up_result *res = parsed_result;
	dev_set_link_up(res->port_id);
}

cmdline_parse_inst_t cmd_set_link_up = {
	.f = cmd_set_link_up_parsed,
	.data = NULL,
	.help_str = "set link-up port <port id>",
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
	portid_t port_id;
};

cmdline_parse_token_string_t cmd_set_link_down_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, set, "set");
cmdline_parse_token_string_t cmd_set_link_down_link_down =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, link_down,
				"link-down");
cmdline_parse_token_string_t cmd_set_link_down_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_link_down_result, port, "port");
cmdline_parse_token_num_t cmd_set_link_down_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_set_link_down_result, port_id,
				RTE_UINT16);

static void cmd_set_link_down_parsed(
				__rte_unused void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_set_link_down_result *res = parsed_result;
	dev_set_link_down(res->port_id);
}

cmdline_parse_inst_t cmd_set_link_down = {
	.f = cmd_set_link_down_parsed,
	.data = NULL,
	.help_str = "set link-down port <port id>",
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
			       __rte_unused struct cmdline *cl,
			       __rte_unused void *data)
{
	struct cmd_showcfg_result *res = parsed_result;
	if (!strcmp(res->what, "rxtx"))
		rxtx_config_display();
	else if (!strcmp(res->what, "cores"))
		fwd_lcores_config_display();
	else if (!strcmp(res->what, "fwd"))
		pkt_fwd_config_display(&cur_fwd_config);
	else if (!strcmp(res->what, "rxoffs"))
		show_rx_pkt_offsets();
	else if (!strcmp(res->what, "rxpkts"))
		show_rx_pkt_segments();
	else if (!strcmp(res->what, "txpkts"))
		show_tx_pkt_segments();
	else if (!strcmp(res->what, "txtimes"))
		show_tx_pkt_times();
}

cmdline_parse_token_string_t cmd_showcfg_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, show, "show");
cmdline_parse_token_string_t cmd_showcfg_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, cfg, "config");
cmdline_parse_token_string_t cmd_showcfg_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showcfg_result, what,
				 "rxtx#cores#fwd#rxoffs#rxpkts#txpkts#txtimes");

cmdline_parse_inst_t cmd_showcfg = {
	.f = cmd_showcfg_parsed,
	.data = NULL,
	.help_str = "show config rxtx|cores|fwd|rxoffs|rxpkts|txpkts|txtimes",
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
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	portid_t i;

	struct cmd_showportall_result *res = parsed_result;
	if (!strcmp(res->show, "clear")) {
		if (!strcmp(res->what, "stats"))
			RTE_ETH_FOREACH_DEV(i)
				nic_stats_clear(i);
		else if (!strcmp(res->what, "xstats"))
			RTE_ETH_FOREACH_DEV(i)
				nic_xstats_clear(i);
	} else if (!strcmp(res->what, "info"))
		RTE_ETH_FOREACH_DEV(i)
			port_infos_display(i);
	else if (!strcmp(res->what, "summary")) {
		port_summary_header_display();
		RTE_ETH_FOREACH_DEV(i)
			port_summary_display(i);
	}
	else if (!strcmp(res->what, "stats"))
		RTE_ETH_FOREACH_DEV(i)
			nic_stats_display(i);
	else if (!strcmp(res->what, "xstats"))
		RTE_ETH_FOREACH_DEV(i)
			nic_xstats_display(i);
#if defined(RTE_NET_I40E) || defined(RTE_NET_IXGBE)
	else if (!strcmp(res->what, "fdir"))
		RTE_ETH_FOREACH_DEV(i)
			fdir_get_infos(i);
#endif
	else if (!strcmp(res->what, "dcb_tc"))
		RTE_ETH_FOREACH_DEV(i)
			port_dcb_info_display(i);
}

cmdline_parse_token_string_t cmd_showportall_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, show,
				 "show#clear");
cmdline_parse_token_string_t cmd_showportall_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, port, "port");
cmdline_parse_token_string_t cmd_showportall_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, what,
				 "info#summary#stats#xstats#fdir#dcb_tc");
cmdline_parse_token_string_t cmd_showportall_all =
	TOKEN_STRING_INITIALIZER(struct cmd_showportall_result, all, "all");
cmdline_parse_inst_t cmd_showportall = {
	.f = cmd_showportall_parsed,
	.data = NULL,
	.help_str = "show|clear port "
		"info|summary|stats|xstats|fdir|dcb_tc all",
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
	uint16_t portnum;
};

static void cmd_showport_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_showport_result *res = parsed_result;
	if (!strcmp(res->show, "clear")) {
		if (!strcmp(res->what, "stats"))
			nic_stats_clear(res->portnum);
		else if (!strcmp(res->what, "xstats"))
			nic_xstats_clear(res->portnum);
	} else if (!strcmp(res->what, "info"))
		port_infos_display(res->portnum);
	else if (!strcmp(res->what, "summary")) {
		port_summary_header_display();
		port_summary_display(res->portnum);
	}
	else if (!strcmp(res->what, "stats"))
		nic_stats_display(res->portnum);
	else if (!strcmp(res->what, "xstats"))
		nic_xstats_display(res->portnum);
#if defined(RTE_NET_I40E) || defined(RTE_NET_IXGBE)
	else if (!strcmp(res->what, "fdir"))
		 fdir_get_infos(res->portnum);
#endif
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
				 "info#summary#stats#xstats#fdir#dcb_tc");
cmdline_parse_token_num_t cmd_showport_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_result, portnum, RTE_UINT16);

cmdline_parse_inst_t cmd_showport = {
	.f = cmd_showport_parsed,
	.data = NULL,
	.help_str = "show|clear port "
		"info|summary|stats|xstats|fdir|dcb_tc "
		"<port_id>",
	.tokens = {
		(void *)&cmd_showport_show,
		(void *)&cmd_showport_port,
		(void *)&cmd_showport_what,
		(void *)&cmd_showport_portnum,
		NULL,
	},
};

/* *** show port representors information *** */
struct cmd_representor_info_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_info;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_representor_info_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_representor_info_result *res = parsed_result;
	struct rte_eth_representor_info *info;
	struct rte_eth_representor_range *range;
	uint32_t range_diff;
	uint32_t i;
	int ret;
	int num;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "Invalid port id %u\n", res->cmd_pid);
		return;
	}

	ret = rte_eth_representor_info_get(res->cmd_pid, NULL);
	if (ret < 0) {
		fprintf(stderr,
			"Failed to get the number of representor info ranges for port %hu: %s\n",
			res->cmd_pid, rte_strerror(-ret));
		return;
	}
	num = ret;

	info = calloc(1, sizeof(*info) + num * sizeof(info->ranges[0]));
	if (info == NULL) {
		fprintf(stderr,
			"Failed to allocate memory for representor info for port %hu\n",
			res->cmd_pid);
		return;
	}
	info->nb_ranges_alloc = num;

	ret = rte_eth_representor_info_get(res->cmd_pid, info);
	if (ret < 0) {
		fprintf(stderr,
			"Failed to get the representor info for port %hu: %s\n",
			res->cmd_pid, rte_strerror(-ret));
		free(info);
		return;
	}

	printf("Port controller: %hu\n", info->controller);
	printf("Port PF: %hu\n", info->pf);

	printf("Ranges: %u\n", info->nb_ranges);
	for (i = 0; i < info->nb_ranges; i++) {
		range = &info->ranges[i];
		range_diff = range->id_end - range->id_base;

		printf("%u. ", i + 1);
		printf("'%s' ", range->name);
		if (range_diff > 0)
			printf("[%u-%u]: ", range->id_base, range->id_end);
		else
			printf("[%u]: ", range->id_base);

		printf("Controller %d, PF %d", range->controller, range->pf);

		switch (range->type) {
		case RTE_ETH_REPRESENTOR_NONE:
			printf(", NONE\n");
			break;
		case RTE_ETH_REPRESENTOR_VF:
			if (range_diff > 0)
				printf(", VF %d..%d\n", range->vf,
				       range->vf + range_diff);
			else
				printf(", VF %d\n", range->vf);
			break;
		case RTE_ETH_REPRESENTOR_SF:
			printf(", SF %d\n", range->sf);
			break;
		case RTE_ETH_REPRESENTOR_PF:
			if (range_diff > 0)
				printf("..%d\n", range->pf + range_diff);
			else
				printf("\n");
			break;
		default:
			printf(", UNKNOWN TYPE %d\n", range->type);
			break;
		}
	}

	free(info);
}

cmdline_parse_token_string_t cmd_representor_info_show =
	TOKEN_STRING_INITIALIZER(struct cmd_representor_info_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_representor_info_port =
	TOKEN_STRING_INITIALIZER(struct cmd_representor_info_result,
			cmd_port, "port");
cmdline_parse_token_string_t cmd_representor_info_info =
	TOKEN_STRING_INITIALIZER(struct cmd_representor_info_result,
			cmd_info, "info");
cmdline_parse_token_num_t cmd_representor_info_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_representor_info_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_representor_info_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_representor_info_result,
			cmd_keyword, "representor");

cmdline_parse_inst_t cmd_representor_info = {
	.f = cmd_representor_info_parsed,
	.data = NULL,
	.help_str = "show port info <port_id> representor",
	.tokens = {
		(void *)&cmd_representor_info_show,
		(void *)&cmd_representor_info_port,
		(void *)&cmd_representor_info_info,
		(void *)&cmd_representor_info_pid,
		(void *)&cmd_representor_info_keyword,
		NULL,
	},
};


/* *** SHOW DEVICE INFO *** */
struct cmd_showdevice_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t device;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t identifier;
};

static void cmd_showdevice_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_showdevice_result *res = parsed_result;
	if (!strcmp(res->what, "info")) {
		if (!strcmp(res->identifier, "all"))
			device_infos_display(NULL);
		else
			device_infos_display(res->identifier);
	}
}

cmdline_parse_token_string_t cmd_showdevice_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showdevice_result, show,
				 "show");
cmdline_parse_token_string_t cmd_showdevice_device =
	TOKEN_STRING_INITIALIZER(struct cmd_showdevice_result, device, "device");
cmdline_parse_token_string_t cmd_showdevice_what =
	TOKEN_STRING_INITIALIZER(struct cmd_showdevice_result, what,
				 "info");
cmdline_parse_token_string_t cmd_showdevice_identifier =
	TOKEN_STRING_INITIALIZER(struct cmd_showdevice_result,
			identifier, NULL);

cmdline_parse_inst_t cmd_showdevice = {
	.f = cmd_showdevice_parsed,
	.data = NULL,
	.help_str = "show device info <identifier>|all",
	.tokens = {
		(void *)&cmd_showdevice_show,
		(void *)&cmd_showdevice_device,
		(void *)&cmd_showdevice_what,
		(void *)&cmd_showdevice_identifier,
		NULL,
	},
};

/* *** SHOW MODULE EEPROM/EEPROM port INFO *** */
struct cmd_showeeprom_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	uint16_t portnum;
	cmdline_fixed_string_t type;
};

static void cmd_showeeprom_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_showeeprom_result *res = parsed_result;

	if (!strcmp(res->type, "eeprom"))
		port_eeprom_display(res->portnum);
	else if (!strcmp(res->type, "module_eeprom"))
		port_module_eeprom_display(res->portnum);
	else
		fprintf(stderr, "Unknown argument\n");
}

cmdline_parse_token_string_t cmd_showeeprom_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showeeprom_result, show, "show");
cmdline_parse_token_string_t cmd_showeeprom_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showeeprom_result, port, "port");
cmdline_parse_token_num_t cmd_showeeprom_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_showeeprom_result, portnum,
			RTE_UINT16);
cmdline_parse_token_string_t cmd_showeeprom_type =
	TOKEN_STRING_INITIALIZER(struct cmd_showeeprom_result, type, "module_eeprom#eeprom");

cmdline_parse_inst_t cmd_showeeprom = {
	.f = cmd_showeeprom_parsed,
	.data = NULL,
	.help_str = "show port <port_id> module_eeprom|eeprom",
	.tokens = {
		(void *)&cmd_showeeprom_show,
		(void *)&cmd_showeeprom_port,
		(void *)&cmd_showeeprom_portnum,
		(void *)&cmd_showeeprom_type,
		NULL,
	},
};

/* *** SHOW QUEUE INFO *** */
struct cmd_showqueue_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t type;
	cmdline_fixed_string_t what;
	uint16_t portnum;
	uint16_t queuenum;
};

static void
cmd_showqueue_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_showqueue_result, portnum,
		RTE_UINT16);
cmdline_parse_token_num_t cmd_showqueue_queuenum =
	TOKEN_NUM_INITIALIZER(struct cmd_showqueue_result, queuenum,
		RTE_UINT16);

cmdline_parse_inst_t cmd_showqueue = {
	.f = cmd_showqueue_parsed,
	.data = NULL,
	.help_str = "show rxq|txq info <port_id> <queue_id>",
	.tokens = {
		(void *)&cmd_showqueue_show,
		(void *)&cmd_showqueue_type,
		(void *)&cmd_showqueue_what,
		(void *)&cmd_showqueue_portnum,
		(void *)&cmd_showqueue_queuenum,
		NULL,
	},
};

/* show/clear fwd engine statistics */
struct fwd_result {
	cmdline_fixed_string_t action;
	cmdline_fixed_string_t fwd;
	cmdline_fixed_string_t stats;
	cmdline_fixed_string_t all;
};

cmdline_parse_token_string_t cmd_fwd_action =
	TOKEN_STRING_INITIALIZER(struct fwd_result, action, "show#clear");
cmdline_parse_token_string_t cmd_fwd_fwd =
	TOKEN_STRING_INITIALIZER(struct fwd_result, fwd, "fwd");
cmdline_parse_token_string_t cmd_fwd_stats =
	TOKEN_STRING_INITIALIZER(struct fwd_result, stats, "stats");
cmdline_parse_token_string_t cmd_fwd_all =
	TOKEN_STRING_INITIALIZER(struct fwd_result, all, "all");

static void
cmd_showfwdall_parsed(void *parsed_result,
		      __rte_unused struct cmdline *cl,
		      __rte_unused void *data)
{
	struct fwd_result *res = parsed_result;

	if (!strcmp(res->action, "show"))
		fwd_stats_display();
	else
		fwd_stats_reset();
}

static cmdline_parse_inst_t cmd_showfwdall = {
	.f = cmd_showfwdall_parsed,
	.data = NULL,
	.help_str = "show|clear fwd stats all",
	.tokens = {
		(void *)&cmd_fwd_action,
		(void *)&cmd_fwd_fwd,
		(void *)&cmd_fwd_stats,
		(void *)&cmd_fwd_all,
		NULL,
	},
};

/* *** READ PORT REGISTER *** */
struct cmd_read_reg_result {
	cmdline_fixed_string_t read;
	cmdline_fixed_string_t reg;
	portid_t port_id;
	uint32_t reg_off;
};

static void
cmd_read_reg_parsed(void *parsed_result,
		    __rte_unused struct cmdline *cl,
		    __rte_unused void *data)
{
	struct cmd_read_reg_result *res = parsed_result;
	port_reg_display(res->port_id, res->reg_off);
}

cmdline_parse_token_string_t cmd_read_reg_read =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_result, read, "read");
cmdline_parse_token_string_t cmd_read_reg_reg =
	TOKEN_STRING_INITIALIZER(struct cmd_read_reg_result, reg, "reg");
cmdline_parse_token_num_t cmd_read_reg_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_result, port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_read_reg_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_result, reg_off, RTE_UINT32);

cmdline_parse_inst_t cmd_read_reg = {
	.f = cmd_read_reg_parsed,
	.data = NULL,
	.help_str = "read reg <port_id> <reg_off>",
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
	portid_t port_id;
	uint32_t reg_off;
	uint8_t bit1_pos;
	uint8_t bit2_pos;
};

static void
cmd_read_reg_bit_field_parsed(void *parsed_result,
			      __rte_unused struct cmdline *cl,
			      __rte_unused void *data)
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
			      RTE_UINT16);
cmdline_parse_token_num_t cmd_read_reg_bit_field_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, reg_off,
			      RTE_UINT32);
cmdline_parse_token_num_t cmd_read_reg_bit_field_bit1_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, bit1_pos,
			      RTE_UINT8);
cmdline_parse_token_num_t cmd_read_reg_bit_field_bit2_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_field_result, bit2_pos,
			      RTE_UINT8);

cmdline_parse_inst_t cmd_read_reg_bit_field = {
	.f = cmd_read_reg_bit_field_parsed,
	.data = NULL,
	.help_str = "read regfield <port_id> <reg_off> <bit_x> <bit_y>: "
	"Read register bit field between bit_x and bit_y included",
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
	portid_t port_id;
	uint32_t reg_off;
	uint8_t bit_pos;
};

static void
cmd_read_reg_bit_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, port_id,
				 RTE_UINT16);
cmdline_parse_token_num_t cmd_read_reg_bit_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, reg_off,
				 RTE_UINT32);
cmdline_parse_token_num_t cmd_read_reg_bit_bit_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_read_reg_bit_result, bit_pos,
				 RTE_UINT8);

cmdline_parse_inst_t cmd_read_reg_bit = {
	.f = cmd_read_reg_bit_parsed,
	.data = NULL,
	.help_str = "read regbit <port_id> <reg_off> <bit_x>: 0 <= bit_x <= 31",
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
	portid_t port_id;
	uint32_t reg_off;
	uint32_t value;
};

static void
cmd_write_reg_parsed(void *parsed_result,
		     __rte_unused struct cmdline *cl,
		     __rte_unused void *data)
{
	struct cmd_write_reg_result *res = parsed_result;
	port_reg_set(res->port_id, res->reg_off, res->value);
}

cmdline_parse_token_string_t cmd_write_reg_write =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_result, write, "write");
cmdline_parse_token_string_t cmd_write_reg_reg =
	TOKEN_STRING_INITIALIZER(struct cmd_write_reg_result, reg, "reg");
cmdline_parse_token_num_t cmd_write_reg_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_write_reg_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, reg_off, RTE_UINT32);
cmdline_parse_token_num_t cmd_write_reg_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_result, value, RTE_UINT32);

cmdline_parse_inst_t cmd_write_reg = {
	.f = cmd_write_reg_parsed,
	.data = NULL,
	.help_str = "write reg <port_id> <reg_off> <reg_value>",
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
	portid_t port_id;
	uint32_t reg_off;
	uint8_t bit1_pos;
	uint8_t bit2_pos;
	uint32_t value;
};

static void
cmd_write_reg_bit_field_parsed(void *parsed_result,
			       __rte_unused struct cmdline *cl,
			       __rte_unused void *data)
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
			      RTE_UINT16);
cmdline_parse_token_num_t cmd_write_reg_bit_field_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, reg_off,
			      RTE_UINT32);
cmdline_parse_token_num_t cmd_write_reg_bit_field_bit1_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, bit1_pos,
			      RTE_UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_field_bit2_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, bit2_pos,
			      RTE_UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_field_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_field_result, value,
			      RTE_UINT32);

cmdline_parse_inst_t cmd_write_reg_bit_field = {
	.f = cmd_write_reg_bit_field_parsed,
	.data = NULL,
	.help_str = "write regfield <port_id> <reg_off> <bit_x> <bit_y> "
		"<reg_value>: "
		"Set register bit field between bit_x and bit_y included",
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
	portid_t port_id;
	uint32_t reg_off;
	uint8_t bit_pos;
	uint8_t value;
};

static void
cmd_write_reg_bit_parsed(void *parsed_result,
			 __rte_unused struct cmdline *cl,
			 __rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, port_id,
				 RTE_UINT16);
cmdline_parse_token_num_t cmd_write_reg_bit_reg_off =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, reg_off,
				 RTE_UINT32);
cmdline_parse_token_num_t cmd_write_reg_bit_bit_pos =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, bit_pos,
				 RTE_UINT8);
cmdline_parse_token_num_t cmd_write_reg_bit_value =
	TOKEN_NUM_INITIALIZER(struct cmd_write_reg_bit_result, value,
				 RTE_UINT8);

cmdline_parse_inst_t cmd_write_reg_bit = {
	.f = cmd_write_reg_bit_parsed,
	.data = NULL,
	.help_str = "write regbit <port_id> <reg_off> <bit_x> 0|1: "
		"0 <= bit_x <= 31",
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
	portid_t port_id;
	uint16_t queue_id;
	uint16_t desc_id;
};

static void
cmd_read_rxd_txd_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
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
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, port_id,
				 RTE_UINT16);
cmdline_parse_token_num_t cmd_read_rxd_txd_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, queue_id,
				 RTE_UINT16);
cmdline_parse_token_num_t cmd_read_rxd_txd_desc_id =
	TOKEN_NUM_INITIALIZER(struct cmd_read_rxd_txd_result, desc_id,
				 RTE_UINT16);

cmdline_parse_inst_t cmd_read_rxd_txd = {
	.f = cmd_read_rxd_txd_parsed,
	.data = NULL,
	.help_str = "read rxd|txd <port_id> <queue_id> <desc_id>",
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

static void cmd_quit_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_quit(cl);
	cl_quit = 1;
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "quit: Exit application",
	.tokens = {
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* *** ADD/REMOVE MAC ADDRESS FROM A PORT *** */
struct cmd_mac_addr_result {
	cmdline_fixed_string_t mac_addr_cmd;
	cmdline_fixed_string_t what;
	uint16_t port_num;
	struct rte_ether_addr address;
};

static void cmd_mac_addr_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_mac_addr_result *res = parsed_result;
	int ret;

	if (strcmp(res->what, "add") == 0)
		ret = rte_eth_dev_mac_addr_add(res->port_num, &res->address, 0);
	else if (strcmp(res->what, "set") == 0)
		ret = rte_eth_dev_default_mac_addr_set(res->port_num,
						       &res->address);
	else
		ret = rte_eth_dev_mac_addr_remove(res->port_num, &res->address);

	/* check the return value and print it if is < 0 */
	if(ret < 0)
		fprintf(stderr, "mac_addr_cmd error: (%s)\n", strerror(-ret));

}

cmdline_parse_token_string_t cmd_mac_addr_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_mac_addr_result, mac_addr_cmd,
				"mac_addr");
cmdline_parse_token_string_t cmd_mac_addr_what =
	TOKEN_STRING_INITIALIZER(struct cmd_mac_addr_result, what,
				"add#remove#set");
cmdline_parse_token_num_t cmd_mac_addr_portnum =
		TOKEN_NUM_INITIALIZER(struct cmd_mac_addr_result, port_num,
					RTE_UINT16);
cmdline_parse_token_etheraddr_t cmd_mac_addr_addr =
		TOKEN_ETHERADDR_INITIALIZER(struct cmd_mac_addr_result, address);

cmdline_parse_inst_t cmd_mac_addr = {
	.f = cmd_mac_addr_parsed,
	.data = (void *)0,
	.help_str = "mac_addr add|remove|set <port_id> <mac_addr>: "
			"Add/Remove/Set MAC address on port_id",
	.tokens = {
		(void *)&cmd_mac_addr_cmd,
		(void *)&cmd_mac_addr_what,
		(void *)&cmd_mac_addr_portnum,
		(void *)&cmd_mac_addr_addr,
		NULL,
	},
};

/* *** SET THE PEER ADDRESS FOR CERTAIN PORT *** */
struct cmd_eth_peer_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t eth_peer;
	portid_t port_id;
	cmdline_fixed_string_t peer_addr;
};

static void cmd_set_eth_peer_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
		struct cmd_eth_peer_result *res = parsed_result;

		if (test_done == 0) {
			fprintf(stderr, "Please stop forwarding first\n");
			return;
		}
		if (!strcmp(res->eth_peer, "eth-peer")) {
			set_fwd_eth_peer(res->port_id, res->peer_addr);
			fwd_config_setup();
		}
}
cmdline_parse_token_string_t cmd_eth_peer_set =
	TOKEN_STRING_INITIALIZER(struct cmd_eth_peer_result, set, "set");
cmdline_parse_token_string_t cmd_eth_peer =
	TOKEN_STRING_INITIALIZER(struct cmd_eth_peer_result, eth_peer, "eth-peer");
cmdline_parse_token_num_t cmd_eth_peer_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_eth_peer_result, port_id,
		RTE_UINT16);
cmdline_parse_token_string_t cmd_eth_peer_addr =
	TOKEN_STRING_INITIALIZER(struct cmd_eth_peer_result, peer_addr, NULL);

cmdline_parse_inst_t cmd_set_fwd_eth_peer = {
	.f = cmd_set_eth_peer_parsed,
	.data = NULL,
	.help_str = "set eth-peer <port_id> <peer_mac>",
	.tokens = {
		(void *)&cmd_eth_peer_set,
		(void *)&cmd_eth_peer,
		(void *)&cmd_eth_peer_port_id,
		(void *)&cmd_eth_peer_addr,
		NULL,
	},
};

/* *** CONFIGURE QUEUE STATS COUNTER MAPPINGS *** */
struct cmd_set_qmap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t qmap;
	cmdline_fixed_string_t what;
	portid_t port_id;
	uint16_t queue_id;
	uint8_t map_value;
};

static void
cmd_set_qmap_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
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
			      port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_setqmap_queueid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_qmap_result,
			      queue_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_setqmap_mapvalue =
	TOKEN_NUM_INITIALIZER(struct cmd_set_qmap_result,
			      map_value, RTE_UINT8);

cmdline_parse_inst_t cmd_set_qmap = {
	.f = cmd_set_qmap_parsed,
	.data = NULL,
	.help_str = "set stat_qmap rx|tx <port_id> <queue_id> <map_value>: "
		"Set statistics mapping value on tx|rx queue_id of port_id",
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

/* *** SET OPTION TO HIDE ZERO VALUES FOR XSTATS  DISPLAY *** */
struct cmd_set_xstats_hide_zero_result {
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t on_off;
};

static void
cmd_set_xstats_hide_zero_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_set_xstats_hide_zero_result *res;
	uint16_t on_off = 0;

	res = parsed_result;
	on_off = !strcmp(res->on_off, "on") ? 1 : 0;
	set_xstats_hide_zero(on_off);
}

cmdline_parse_token_string_t cmd_set_xstats_hide_zero_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_xstats_hide_zero_result,
				 keyword, "set");
cmdline_parse_token_string_t cmd_set_xstats_hide_zero_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_xstats_hide_zero_result,
				 name, "xstats-hide-zero");
cmdline_parse_token_string_t cmd_set_xstats_hide_zero_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_set_xstats_hide_zero_result,
				 on_off, "on#off");

cmdline_parse_inst_t cmd_set_xstats_hide_zero = {
	.f = cmd_set_xstats_hide_zero_parsed,
	.data = NULL,
	.help_str = "set xstats-hide-zero on|off",
	.tokens = {
		(void *)&cmd_set_xstats_hide_zero_keyword,
		(void *)&cmd_set_xstats_hide_zero_name,
		(void *)&cmd_set_xstats_hide_zero_on_off,
		NULL,
	},
};

/* *** SET OPTION TO ENABLE MEASUREMENT OF CPU CYCLES *** */
struct cmd_set_record_core_cycles_result {
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t on_off;
};

static void
cmd_set_record_core_cycles_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_set_record_core_cycles_result *res;
	uint16_t on_off = 0;

	res = parsed_result;
	on_off = !strcmp(res->on_off, "on") ? 1 : 0;
	set_record_core_cycles(on_off);
}

cmdline_parse_token_string_t cmd_set_record_core_cycles_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_core_cycles_result,
				 keyword, "set");
cmdline_parse_token_string_t cmd_set_record_core_cycles_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_core_cycles_result,
				 name, "record-core-cycles");
cmdline_parse_token_string_t cmd_set_record_core_cycles_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_core_cycles_result,
				 on_off, "on#off");

cmdline_parse_inst_t cmd_set_record_core_cycles = {
	.f = cmd_set_record_core_cycles_parsed,
	.data = NULL,
	.help_str = "set record-core-cycles on|off",
	.tokens = {
		(void *)&cmd_set_record_core_cycles_keyword,
		(void *)&cmd_set_record_core_cycles_name,
		(void *)&cmd_set_record_core_cycles_on_off,
		NULL,
	},
};

/* *** SET OPTION TO ENABLE DISPLAY OF RX AND TX BURSTS *** */
struct cmd_set_record_burst_stats_result {
	cmdline_fixed_string_t keyword;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t on_off;
};

static void
cmd_set_record_burst_stats_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_set_record_burst_stats_result *res;
	uint16_t on_off = 0;

	res = parsed_result;
	on_off = !strcmp(res->on_off, "on") ? 1 : 0;
	set_record_burst_stats(on_off);
}

cmdline_parse_token_string_t cmd_set_record_burst_stats_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_burst_stats_result,
				 keyword, "set");
cmdline_parse_token_string_t cmd_set_record_burst_stats_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_burst_stats_result,
				 name, "record-burst-stats");
cmdline_parse_token_string_t cmd_set_record_burst_stats_on_off =
	TOKEN_STRING_INITIALIZER(struct cmd_set_record_burst_stats_result,
				 on_off, "on#off");

cmdline_parse_inst_t cmd_set_record_burst_stats = {
	.f = cmd_set_record_burst_stats_parsed,
	.data = NULL,
	.help_str = "set record-burst-stats on|off",
	.tokens = {
		(void *)&cmd_set_record_burst_stats_keyword,
		(void *)&cmd_set_record_burst_stats_name,
		(void *)&cmd_set_record_burst_stats_on_off,
		NULL,
	},
};

/* *** CONFIGURE UNICAST HASH TABLE *** */
struct cmd_set_uc_hash_table {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t what;
	struct rte_ether_addr address;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_uc_hash_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	int ret=0;
	struct cmd_set_uc_hash_table *res = parsed_result;

	int is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	if (strcmp(res->what, "uta") == 0)
		ret = rte_eth_dev_uc_hash_table_set(res->port_id,
						&res->address,(uint8_t)is_on);
	if (ret < 0)
		fprintf(stderr,
			"bad unicast hash table parameter, return code = %d\n",
			ret);

}

cmdline_parse_token_string_t cmd_set_uc_hash_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 set, "set");
cmdline_parse_token_string_t cmd_set_uc_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_hash_table,
				 port, "port");
cmdline_parse_token_num_t cmd_set_uc_hash_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_uc_hash_table,
			      port_id, RTE_UINT16);
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
	.help_str = "set port <port_id> uta <mac_addr> on|off)",
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
	portid_t port_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t value;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_uc_all_hash_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	int ret=0;
	struct cmd_set_uc_all_hash_table *res = parsed_result;

	int is_on = (strcmp(res->mode, "on") == 0) ? 1 : 0;

	if ((strcmp(res->what, "uta") == 0) &&
		(strcmp(res->value, "all") == 0))
		ret = rte_eth_dev_uc_all_hash_table_set(res->port_id,(uint8_t) is_on);
	if (ret < 0)
		fprintf(stderr,
			"bad unicast hash table parameter, return code = %d\n",
			ret);
}

cmdline_parse_token_string_t cmd_set_uc_all_hash_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 set, "set");
cmdline_parse_token_string_t cmd_set_uc_all_hash_port =
	TOKEN_STRING_INITIALIZER(struct cmd_set_uc_all_hash_table,
				 port, "port");
cmdline_parse_token_num_t cmd_set_uc_all_hash_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_uc_all_hash_table,
			      port_id, RTE_UINT16);
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
	.help_str = "set port <port_id> uta all on|off",
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

/* *** CONFIGURE VF TRAFFIC CONTROL *** */
struct cmd_set_vf_traffic {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t mode;
};

static void
cmd_set_vf_traffic_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
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
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_setvf_traffic_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 vf, "vf");
cmdline_parse_token_num_t cmd_setvf_traffic_vfid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_traffic,
			      vf_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_setvf_traffic_what =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 what, "tx#rx");
cmdline_parse_token_string_t cmd_setvf_traffic_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_traffic,
				 mode, "on#off");

cmdline_parse_inst_t cmd_set_vf_traffic = {
	.f = cmd_set_vf_traffic_parsed,
	.data = NULL,
	.help_str = "set port <port_id> vf <vf_id> rx|tx on|off",
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
	portid_t port_id;
	cmdline_fixed_string_t vf;
	uint8_t vf_id;
	cmdline_fixed_string_t what;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t on;
};

static void
cmd_set_vf_rxmode_parsed(void *parsed_result,
		       __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	int ret = -ENOTSUP;
	uint16_t vf_rxmode = 0;
	struct cmd_set_vf_rxmode *res = parsed_result;

	int is_on = (strcmp(res->on, "on") == 0) ? 1 : 0;
	if (!strcmp(res->what,"rxmode")) {
		if (!strcmp(res->mode, "AUPE"))
			vf_rxmode |= RTE_ETH_VMDQ_ACCEPT_UNTAG;
		else if (!strcmp(res->mode, "ROPE"))
			vf_rxmode |= RTE_ETH_VMDQ_ACCEPT_HASH_UC;
		else if (!strcmp(res->mode, "BAM"))
			vf_rxmode |= RTE_ETH_VMDQ_ACCEPT_BROADCAST;
		else if (!strncmp(res->mode, "MPE",3))
			vf_rxmode |= RTE_ETH_VMDQ_ACCEPT_MULTICAST;
	}

	RTE_SET_USED(is_on);
	RTE_SET_USED(vf_rxmode);

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_rxmode(res->port_id, res->vf_id,
						  vf_rxmode, (uint8_t)is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_rxmode(res->port_id, res->vf_id,
						 vf_rxmode, (uint8_t)is_on);
#endif
	if (ret < 0)
		fprintf(stderr,
			"bad VF receive mode parameter, return code = %d\n",
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
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vf_rxmode_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vf_rxmode,
				 vf, "vf");
cmdline_parse_token_num_t cmd_set_vf_rxmode_vfid =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vf_rxmode,
			      vf_id, RTE_UINT8);
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
	.help_str = "set port <port_id> vf <vf_id> rxmode "
		"AUPE|ROPE|BAM|MPE on|off",
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
	uint16_t port_num;
	cmdline_fixed_string_t vf;
	uint8_t vf_num;
	struct rte_ether_addr address;
};

static void cmd_vf_mac_addr_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_vf_mac_addr_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (strcmp(res->what, "add") != 0)
		return;

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_add_vf_mac_addr(res->port_num, res->vf_num,
						   &res->address);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_mac_addr_add(res->port_num, &res->address,
						res->vf_num);
#endif

	if(ret < 0)
		fprintf(stderr, "vf_mac_addr_cmd error: (%s)\n", strerror(-ret));

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
				port_num, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_mac_addr_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_mac_addr_result,
				vf,"vf");
cmdline_parse_token_num_t cmd_vf_mac_addr_vfnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_mac_addr_result,
				vf_num, RTE_UINT8);
cmdline_parse_token_etheraddr_t cmd_vf_mac_addr_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_vf_mac_addr_result,
				address);

cmdline_parse_inst_t cmd_vf_mac_addr_filter = {
	.f = cmd_vf_mac_addr_parsed,
	.data = (void *)0,
	.help_str = "mac_addr add port <port_id> vf <vf_id> <mac_addr>: "
		"Add MAC address filtering for a VF on port_id",
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
	portid_t port_id;
	cmdline_fixed_string_t vf;
	uint64_t vf_mask;
};

static void
cmd_vf_rx_vlan_filter_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_vf_rx_vlan_filter *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_add = (strcmp(res->what, "add") == 0) ? 1 : 0;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_vlan_filter(res->port_id,
				res->vlan_id, res->vf_mask, is_add);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_vlan_filter(res->port_id,
				res->vlan_id, res->vf_mask, is_add);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_vlan_filter(res->port_id,
				res->vlan_id, res->vf_mask, is_add);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vlan_id %d or vf_mask %"PRIu64"\n",
			res->vlan_id, res->vf_mask);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_rx_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 rx_vlan, "rx_vlan");
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_what =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 what, "add#rm");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_vlanid =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      vlan_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_port =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 port, "port");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_portid =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_rx_vlan_filter_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rx_vlan_filter,
				 vf, "vf");
cmdline_parse_token_num_t cmd_vf_rx_vlan_filter_vf_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rx_vlan_filter,
			      vf_mask, RTE_UINT64);

cmdline_parse_inst_t cmd_vf_rxvlan_filter = {
	.f = cmd_vf_rx_vlan_filter_parsed,
	.data = NULL,
	.help_str = "rx_vlan add|rm <vlan_id> port <port_id> vf <vf_mask>: "
		"(vf_mask = hexadecimal VF mask)",
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
	uint16_t port_num;
	cmdline_fixed_string_t queue;
	uint8_t queue_num;
	cmdline_fixed_string_t rate;
	uint16_t rate_num;
};

static void cmd_queue_rate_limit_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_queue_rate_limit_result *res = parsed_result;
	int ret = 0;

	if ((strcmp(res->set, "set") == 0) && (strcmp(res->port, "port") == 0)
		&& (strcmp(res->queue, "queue") == 0)
		&& (strcmp(res->rate, "rate") == 0))
		ret = set_queue_rate_limit(res->port_num, res->queue_num,
					res->rate_num);
	if (ret < 0)
		fprintf(stderr, "queue_rate_limit_cmd error: (%s)\n",
			strerror(-ret));

}

cmdline_parse_token_string_t cmd_queue_rate_limit_set =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				set, "set");
cmdline_parse_token_string_t cmd_queue_rate_limit_port =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				port, "port");
cmdline_parse_token_num_t cmd_queue_rate_limit_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				port_num, RTE_UINT16);
cmdline_parse_token_string_t cmd_queue_rate_limit_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				queue, "queue");
cmdline_parse_token_num_t cmd_queue_rate_limit_queuenum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				queue_num, RTE_UINT8);
cmdline_parse_token_string_t cmd_queue_rate_limit_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_rate_limit_result,
				rate, "rate");
cmdline_parse_token_num_t cmd_queue_rate_limit_ratenum =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_rate_limit_result,
				rate_num, RTE_UINT16);

cmdline_parse_inst_t cmd_queue_rate_limit = {
	.f = cmd_queue_rate_limit_parsed,
	.data = (void *)0,
	.help_str = "set port <port_id> queue <queue_id> rate <rate_value>: "
		"Set rate limit for a queue on port_id",
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
	uint16_t port_num;
	cmdline_fixed_string_t vf;
	uint8_t vf_num;
	cmdline_fixed_string_t rate;
	uint16_t rate_num;
	cmdline_fixed_string_t q_msk;
	uint64_t q_msk_val;
};

static void cmd_vf_rate_limit_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
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
		fprintf(stderr, "vf_rate_limit_cmd error: (%s)\n",
			strerror(-ret));

}

cmdline_parse_token_string_t cmd_vf_rate_limit_set =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				set, "set");
cmdline_parse_token_string_t cmd_vf_rate_limit_port =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				port, "port");
cmdline_parse_token_num_t cmd_vf_rate_limit_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				port_num, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_rate_limit_vf =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				vf, "vf");
cmdline_parse_token_num_t cmd_vf_rate_limit_vfnum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				vf_num, RTE_UINT8);
cmdline_parse_token_string_t cmd_vf_rate_limit_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				rate, "rate");
cmdline_parse_token_num_t cmd_vf_rate_limit_ratenum =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				rate_num, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_rate_limit_q_msk =
	TOKEN_STRING_INITIALIZER(struct cmd_vf_rate_limit_result,
				q_msk, "queue_mask");
cmdline_parse_token_num_t cmd_vf_rate_limit_q_msk_val =
	TOKEN_NUM_INITIALIZER(struct cmd_vf_rate_limit_result,
				q_msk_val, RTE_UINT64);

cmdline_parse_inst_t cmd_vf_rate_limit = {
	.f = cmd_vf_rate_limit_parsed,
	.data = (void *)0,
	.help_str = "set port <port_id> vf <vf_id> rate <rate_value> "
		"queue_mask <queue_mask_value>: "
		"Set rate limit for queues of VF on port_id",
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

/* *** CONFIGURE TUNNEL UDP PORT *** */
struct cmd_tunnel_udp_config {
	cmdline_fixed_string_t rx_vxlan_port;
	cmdline_fixed_string_t what;
	uint16_t udp_port;
	portid_t port_id;
};

static void
cmd_tunnel_udp_config_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_tunnel_udp_config *res = parsed_result;
	struct rte_eth_udp_tunnel tunnel_udp;
	int ret;

	tunnel_udp.udp_port = res->udp_port;
	tunnel_udp.prot_type = RTE_ETH_TUNNEL_TYPE_VXLAN;

	if (!strcmp(res->what, "add"))
		ret = rte_eth_dev_udp_tunnel_port_add(res->port_id,
						      &tunnel_udp);
	else
		ret = rte_eth_dev_udp_tunnel_port_delete(res->port_id,
							 &tunnel_udp);

	if (ret < 0)
		fprintf(stderr, "udp tunneling add error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_token_string_t cmd_tunnel_udp_config_rx_vxlan_port =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_udp_config,
				rx_vxlan_port, "rx_vxlan_port");
cmdline_parse_token_string_t cmd_tunnel_udp_config_what =
	TOKEN_STRING_INITIALIZER(struct cmd_tunnel_udp_config,
				what, "add#rm");
cmdline_parse_token_num_t cmd_tunnel_udp_config_udp_port =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_udp_config,
				udp_port, RTE_UINT16);
cmdline_parse_token_num_t cmd_tunnel_udp_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_tunnel_udp_config,
				port_id, RTE_UINT16);

cmdline_parse_inst_t cmd_tunnel_udp_config = {
	.f = cmd_tunnel_udp_config_parsed,
	.data = (void *)0,
	.help_str = "rx_vxlan_port add|rm <udp_port> <port_id>: "
		"Add/Remove a tunneling UDP port filter",
	.tokens = {
		(void *)&cmd_tunnel_udp_config_rx_vxlan_port,
		(void *)&cmd_tunnel_udp_config_what,
		(void *)&cmd_tunnel_udp_config_udp_port,
		(void *)&cmd_tunnel_udp_config_port_id,
		NULL,
	},
};

struct cmd_config_tunnel_udp_port {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t udp_tunnel_port;
	cmdline_fixed_string_t action;
	cmdline_fixed_string_t tunnel_type;
	uint16_t udp_port;
};

static void
cmd_cfg_tunnel_udp_port_parsed(void *parsed_result,
			       __rte_unused struct cmdline *cl,
			       __rte_unused void *data)
{
	struct cmd_config_tunnel_udp_port *res = parsed_result;
	struct rte_eth_udp_tunnel tunnel_udp;
	int ret = 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	tunnel_udp.udp_port = res->udp_port;

	if (!strcmp(res->tunnel_type, "vxlan")) {
		tunnel_udp.prot_type = RTE_ETH_TUNNEL_TYPE_VXLAN;
	} else if (!strcmp(res->tunnel_type, "geneve")) {
		tunnel_udp.prot_type = RTE_ETH_TUNNEL_TYPE_GENEVE;
	} else if (!strcmp(res->tunnel_type, "vxlan-gpe")) {
		tunnel_udp.prot_type = RTE_ETH_TUNNEL_TYPE_VXLAN_GPE;
	} else if (!strcmp(res->tunnel_type, "ecpri")) {
		tunnel_udp.prot_type = RTE_ETH_TUNNEL_TYPE_ECPRI;
	} else {
		fprintf(stderr, "Invalid tunnel type\n");
		return;
	}

	if (!strcmp(res->action, "add"))
		ret = rte_eth_dev_udp_tunnel_port_add(res->port_id,
						      &tunnel_udp);
	else
		ret = rte_eth_dev_udp_tunnel_port_delete(res->port_id,
							 &tunnel_udp);

	if (ret < 0)
		fprintf(stderr, "udp tunneling port add error: (%s)\n",
			strerror(-ret));
}

cmdline_parse_token_string_t cmd_config_tunnel_udp_port_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tunnel_udp_port, port,
				 "port");
cmdline_parse_token_string_t cmd_config_tunnel_udp_port_config =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tunnel_udp_port, config,
				 "config");
cmdline_parse_token_num_t cmd_config_tunnel_udp_port_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_tunnel_udp_port, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_config_tunnel_udp_port_tunnel_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tunnel_udp_port,
				 udp_tunnel_port,
				 "udp_tunnel_port");
cmdline_parse_token_string_t cmd_config_tunnel_udp_port_action =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tunnel_udp_port, action,
				 "add#rm");
cmdline_parse_token_string_t cmd_config_tunnel_udp_port_tunnel_type =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tunnel_udp_port, tunnel_type,
				 "vxlan#geneve#vxlan-gpe#ecpri");
cmdline_parse_token_num_t cmd_config_tunnel_udp_port_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_tunnel_udp_port, udp_port,
			      RTE_UINT16);

cmdline_parse_inst_t cmd_cfg_tunnel_udp_port = {
	.f = cmd_cfg_tunnel_udp_port_parsed,
	.data = NULL,
	.help_str = "port config <port_id> udp_tunnel_port add|rm vxlan|"
		"geneve|vxlan-gpe|ecpri <udp_port>",
	.tokens = {
		(void *)&cmd_config_tunnel_udp_port_port,
		(void *)&cmd_config_tunnel_udp_port_config,
		(void *)&cmd_config_tunnel_udp_port_port_id,
		(void *)&cmd_config_tunnel_udp_port_tunnel_port,
		(void *)&cmd_config_tunnel_udp_port_action,
		(void *)&cmd_config_tunnel_udp_port_tunnel_type,
		(void *)&cmd_config_tunnel_udp_port_value,
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


/* Dump the socket memory statistics on console */
static void
dump_socket_mem(FILE *f)
{
	struct rte_malloc_socket_stats socket_stats;
	unsigned int i;
	size_t total = 0;
	size_t alloc = 0;
	size_t free = 0;
	unsigned int n_alloc = 0;
	unsigned int n_free = 0;
	static size_t last_allocs;
	static size_t last_total;


	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if (rte_malloc_get_socket_stats(i, &socket_stats) ||
		    !socket_stats.heap_totalsz_bytes)
			continue;
		total += socket_stats.heap_totalsz_bytes;
		alloc += socket_stats.heap_allocsz_bytes;
		free += socket_stats.heap_freesz_bytes;
		n_alloc += socket_stats.alloc_count;
		n_free += socket_stats.free_count;
		fprintf(f,
			"Socket %u: size(M) total: %.6lf alloc: %.6lf(%.3lf%%) free: %.6lf \tcount alloc: %-4u free: %u\n",
			i,
			(double)socket_stats.heap_totalsz_bytes / (1024 * 1024),
			(double)socket_stats.heap_allocsz_bytes / (1024 * 1024),
			(double)socket_stats.heap_allocsz_bytes * 100 /
			(double)socket_stats.heap_totalsz_bytes,
			(double)socket_stats.heap_freesz_bytes / (1024 * 1024),
			socket_stats.alloc_count,
			socket_stats.free_count);
	}
	fprintf(f,
		"Total   : size(M) total: %.6lf alloc: %.6lf(%.3lf%%) free: %.6lf \tcount alloc: %-4u free: %u\n",
		(double)total / (1024 * 1024), (double)alloc / (1024 * 1024),
		total ? ((double)alloc * 100 / (double)total) : 0,
		(double)free / (1024 * 1024),
		n_alloc, n_free);
	if (last_allocs)
		fprintf(stdout, "Memory total change: %.6lf(M), allocation change: %.6lf(M)\n",
			((double)total - (double)last_total) / (1024 * 1024),
			(double)(alloc - (double)last_allocs) / 1024 / 1024);
	last_allocs = alloc;
	last_total = total;
}

static void cmd_dump_parsed(void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	struct cmd_dump_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_physmem"))
		rte_dump_physmem_layout(stdout);
	else if (!strcmp(res->dump, "dump_socket_mem"))
		dump_socket_mem(stdout);
	else if (!strcmp(res->dump, "dump_memzone"))
		rte_memzone_dump(stdout);
	else if (!strcmp(res->dump, "dump_struct_sizes"))
		dump_struct_sizes();
	else if (!strcmp(res->dump, "dump_ring"))
		rte_ring_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_mempool"))
		rte_mempool_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_devargs"))
		rte_devargs_dump(stdout);
	else if (!strcmp(res->dump, "dump_log_types"))
		rte_log_dump(stdout);
}

cmdline_parse_token_string_t cmd_dump_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_result, dump,
		"dump_physmem#"
		"dump_memzone#"
		"dump_socket_mem#"
		"dump_struct_sizes#"
		"dump_ring#"
		"dump_mempool#"
		"dump_devargs#"
		"dump_log_types");

cmdline_parse_inst_t cmd_dump = {
	.f = cmd_dump_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "Dump status",
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
				__rte_unused void *data)
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
	.help_str = "dump_ring|dump_mempool <name>: Dump one ring/mempool",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_one_dump,
		(void *)&cmd_dump_one_name,
		NULL,
	},
};

/* *** queue region set *** */
struct cmd_queue_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t region;
	uint8_t  region_id;
	cmdline_fixed_string_t queue_start_index;
	uint8_t  queue_id;
	cmdline_fixed_string_t queue_num;
	uint8_t  queue_num_value;
};

static void
cmd_queue_region_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_queue_region_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	memset(&region_conf, 0, sizeof(region_conf));
	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_SET;
	region_conf.region_id = res->region_id;
	region_conf.queue_num = res->queue_num_value;
	region_conf.queue_start_index = res->queue_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config error: (%s)\n",
			strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_queue_region_set =
TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
		set, "set");
cmdline_parse_token_string_t cmd_queue_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result, port, "port");
cmdline_parse_token_num_t cmd_queue_region_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_queue_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
				 cmd, "queue-region");
cmdline_parse_token_string_t cmd_queue_region_id =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
				region, "region_id");
cmdline_parse_token_num_t cmd_queue_region_index =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
				region_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_queue_region_queue_start_index =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
				queue_start_index, "queue_start_index");
cmdline_parse_token_num_t cmd_queue_region_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
				queue_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_queue_region_queue_num =
	TOKEN_STRING_INITIALIZER(struct cmd_queue_region_result,
				queue_num, "queue_num");
cmdline_parse_token_num_t cmd_queue_region_queue_num_value =
	TOKEN_NUM_INITIALIZER(struct cmd_queue_region_result,
				queue_num_value, RTE_UINT8);

cmdline_parse_inst_t cmd_queue_region = {
	.f = cmd_queue_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region region_id <value> "
		"queue_start_index <value> queue_num <value>: Set a queue region",
	.tokens = {
		(void *)&cmd_queue_region_set,
		(void *)&cmd_queue_region_port,
		(void *)&cmd_queue_region_port_id,
		(void *)&cmd_queue_region_cmd,
		(void *)&cmd_queue_region_id,
		(void *)&cmd_queue_region_index,
		(void *)&cmd_queue_region_queue_start_index,
		(void *)&cmd_queue_region_queue_id,
		(void *)&cmd_queue_region_queue_num,
		(void *)&cmd_queue_region_queue_num_value,
		NULL,
	},
};

/* *** queue region and flowtype set *** */
struct cmd_region_flowtype_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t region;
	uint8_t  region_id;
	cmdline_fixed_string_t flowtype;
	uint8_t  flowtype_id;
};

static void
cmd_region_flowtype_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_region_flowtype_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	memset(&region_conf, 0, sizeof(region_conf));

	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_FLOWTYPE_SET;
	region_conf.region_id = res->region_id;
	region_conf.hw_flowtype = res->flowtype_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
			op_type, &region_conf);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "region flowtype config error: (%s)\n",
			strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_region_flowtype_set =
TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
				set, "set");
cmdline_parse_token_string_t cmd_region_flowtype_port =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
				port, "port");
cmdline_parse_token_num_t cmd_region_flowtype_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_region_flowtype_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
				cmd, "queue-region");
cmdline_parse_token_string_t cmd_region_flowtype_index =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
				region, "region_id");
cmdline_parse_token_num_t cmd_region_flowtype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
				region_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_region_flowtype_flow_index =
	TOKEN_STRING_INITIALIZER(struct cmd_region_flowtype_result,
				flowtype, "flowtype");
cmdline_parse_token_num_t cmd_region_flowtype_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_region_flowtype_result,
				flowtype_id, RTE_UINT8);
cmdline_parse_inst_t cmd_region_flowtype = {
	.f = cmd_region_flowtype_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region region_id <value> "
		"flowtype <value>: Set a flowtype region index",
	.tokens = {
		(void *)&cmd_region_flowtype_set,
		(void *)&cmd_region_flowtype_port,
		(void *)&cmd_region_flowtype_port_index,
		(void *)&cmd_region_flowtype_cmd,
		(void *)&cmd_region_flowtype_index,
		(void *)&cmd_region_flowtype_id,
		(void *)&cmd_region_flowtype_flow_index,
		(void *)&cmd_region_flowtype_flow_id,
		NULL,
	},
};

/* *** User Priority (UP) to queue region (region_id) set *** */
struct cmd_user_priority_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t user_priority;
	uint8_t  user_priority_id;
	cmdline_fixed_string_t region;
	uint8_t  region_id;
};

static void
cmd_user_priority_region_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_user_priority_region_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	memset(&region_conf, 0, sizeof(region_conf));
	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_USER_PRIORITY_SET;
	region_conf.user_priority = res->user_priority_id;
	region_conf.region_id = res->region_id;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "user_priority region config error: (%s)\n",
			strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_user_priority_region_set =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
				set, "set");
cmdline_parse_token_string_t cmd_user_priority_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
				port, "port");
cmdline_parse_token_num_t cmd_user_priority_region_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_user_priority_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
				cmd, "queue-region");
cmdline_parse_token_string_t cmd_user_priority_region_UP =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
				user_priority, "UP");
cmdline_parse_token_num_t cmd_user_priority_region_UP_id =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
				user_priority_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_user_priority_region_region =
	TOKEN_STRING_INITIALIZER(struct cmd_user_priority_region_result,
				region, "region_id");
cmdline_parse_token_num_t cmd_user_priority_region_region_id =
	TOKEN_NUM_INITIALIZER(struct cmd_user_priority_region_result,
				region_id, RTE_UINT8);

cmdline_parse_inst_t cmd_user_priority_region = {
	.f = cmd_user_priority_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region UP <value> "
		"region_id <value>: Set the mapping of User Priority (UP) "
		"to queue region (region_id) ",
	.tokens = {
		(void *)&cmd_user_priority_region_set,
		(void *)&cmd_user_priority_region_port,
		(void *)&cmd_user_priority_region_port_index,
		(void *)&cmd_user_priority_region_cmd,
		(void *)&cmd_user_priority_region_UP,
		(void *)&cmd_user_priority_region_UP_id,
		(void *)&cmd_user_priority_region_region,
		(void *)&cmd_user_priority_region_region_id,
		NULL,
	},
};

/* *** flush all queue region related configuration *** */
struct cmd_flush_queue_region_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t flush;
	cmdline_fixed_string_t what;
};

static void
cmd_flush_queue_region_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_flush_queue_region_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_queue_region_conf region_conf;
	enum rte_pmd_i40e_queue_region_op op_type;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	memset(&region_conf, 0, sizeof(region_conf));

	if (strcmp(res->what, "on") == 0)
		op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_ON;
	else
		op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_OFF;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
				op_type, &region_conf);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config flush error: (%s)\n",
			strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_flush_queue_region_set =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
				set, "set");
cmdline_parse_token_string_t cmd_flush_queue_region_port =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
				port, "port");
cmdline_parse_token_num_t cmd_flush_queue_region_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_flush_queue_region_result,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_flush_queue_region_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
				cmd, "queue-region");
cmdline_parse_token_string_t cmd_flush_queue_region_flush =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
				flush, "flush");
cmdline_parse_token_string_t cmd_flush_queue_region_what =
	TOKEN_STRING_INITIALIZER(struct cmd_flush_queue_region_result,
				what, "on#off");

cmdline_parse_inst_t cmd_flush_queue_region = {
	.f = cmd_flush_queue_region_parsed,
	.data = NULL,
	.help_str = "set port <port_id> queue-region flush on|off"
		": flush all queue region related configuration",
	.tokens = {
		(void *)&cmd_flush_queue_region_set,
		(void *)&cmd_flush_queue_region_port,
		(void *)&cmd_flush_queue_region_port_index,
		(void *)&cmd_flush_queue_region_cmd,
		(void *)&cmd_flush_queue_region_flush,
		(void *)&cmd_flush_queue_region_what,
		NULL,
	},
};

/* *** get all queue region related configuration info *** */
struct cmd_show_queue_region_info {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t cmd;
};

static void
cmd_show_queue_region_info_parsed(void *parsed_result,
			__rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	struct cmd_show_queue_region_info *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_queue_regions rte_pmd_regions;
	enum rte_pmd_i40e_queue_region_op op_type;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	memset(&rte_pmd_regions, 0, sizeof(rte_pmd_regions));

	op_type = RTE_PMD_I40E_RSS_QUEUE_REGION_INFO_GET;

	ret = rte_pmd_i40e_rss_queue_region_conf(res->port_id,
					op_type, &rte_pmd_regions);

	port_queue_region_info_display(res->port_id, &rte_pmd_regions);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "queue region config info show error: (%s)\n",
			strerror(-ret));
	}
}

cmdline_parse_token_string_t cmd_show_queue_region_info_get =
TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
				show, "show");
cmdline_parse_token_string_t cmd_show_queue_region_info_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
				port, "port");
cmdline_parse_token_num_t cmd_show_queue_region_info_port_index =
	TOKEN_NUM_INITIALIZER(struct cmd_show_queue_region_info,
				port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_queue_region_info_cmd =
	TOKEN_STRING_INITIALIZER(struct cmd_show_queue_region_info,
				cmd, "queue-region");

cmdline_parse_inst_t cmd_show_queue_region_info_all = {
	.f = cmd_show_queue_region_info_parsed,
	.data = NULL,
	.help_str = "show port <port_id> queue-region"
		": show all queue region related configuration info",
	.tokens = {
		(void *)&cmd_show_queue_region_info_get,
		(void *)&cmd_show_queue_region_info_port,
		(void *)&cmd_show_queue_region_info_port_index,
		(void *)&cmd_show_queue_region_info_cmd,
		NULL,
	},
};

/* *** Filters Control *** */

#define IPV4_ADDR_TO_UINT(ip_addr, ip) \
do { \
	if ((ip_addr).family == AF_INET) \
		(ip) = (ip_addr).addr.ipv4.s_addr; \
	else { \
		fprintf(stderr, "invalid parameter.\n"); \
		return; \
	} \
} while (0)

#define IPV6_ADDR_TO_ARRAY(ip_addr, ip) \
do { \
	if ((ip_addr).family == AF_INET6) \
		rte_memcpy(&(ip), \
				 &((ip_addr).addr.ipv6), \
				 sizeof(struct in6_addr)); \
	else { \
		fprintf(stderr, "invalid parameter.\n"); \
		return; \
	} \
} while (0)

#ifdef RTE_NET_I40E

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
		{"ipv6-ex", RTE_ETH_FLOW_IPV6_EX},
		{"ipv6-tcp-ex", RTE_ETH_FLOW_IPV6_TCP_EX},
		{"ipv6-udp-ex", RTE_ETH_FLOW_IPV6_UDP_EX},
		{"gtpu", RTE_ETH_FLOW_GTPU},
	};

	for (i = 0; i < RTE_DIM(flowtype_str); i++) {
		if (!strcmp(flowtype_str[i].str, string))
			return flowtype_str[i].type;
	}

	if (isdigit(string[0]) && atoi(string) > 0 && atoi(string) < 64)
		return (uint16_t)atoi(string);

	return RTE_ETH_FLOW_UNKNOWN;
}

/* *** deal with flow director filter *** */
struct cmd_flow_director_result {
	cmdline_fixed_string_t flow_director_filter;
	portid_t port_id;
	cmdline_fixed_string_t mode;
	cmdline_fixed_string_t mode_value;
	cmdline_fixed_string_t ops;
	cmdline_fixed_string_t flow;
	cmdline_fixed_string_t flow_type;
	cmdline_fixed_string_t drop;
	cmdline_fixed_string_t queue;
	uint16_t  queue_id;
	cmdline_fixed_string_t fd_id;
	uint32_t  fd_id_value;
	cmdline_fixed_string_t packet;
	char filepath[];
};

static void
cmd_flow_director_filter_parsed(void *parsed_result,
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_flow_director_result *res = parsed_result;
	int ret = 0;
	struct rte_pmd_i40e_flow_type_mapping
			mapping[RTE_PMD_I40E_FLOW_TYPE_MAX];
	struct rte_pmd_i40e_pkt_template_conf conf;
	uint16_t flow_type = str2flowtype(res->flow_type);
	uint16_t i, port = res->port_id;
	uint8_t add;

	memset(&conf, 0, sizeof(conf));

	if (flow_type == RTE_ETH_FLOW_UNKNOWN) {
		fprintf(stderr, "Invalid flow type specified.\n");
		return;
	}
	ret = rte_pmd_i40e_flow_type_mapping_get(res->port_id,
						 mapping);
	if (ret)
		return;
	if (mapping[flow_type].pctype == 0ULL) {
		fprintf(stderr, "Invalid flow type specified.\n");
		return;
	}
	for (i = 0; i < RTE_PMD_I40E_PCTYPE_MAX; i++) {
		if (mapping[flow_type].pctype & (1ULL << i)) {
			conf.input.pctype = i;
			break;
		}
	}

	conf.input.packet = open_file(res->filepath,
				&conf.input.length);
	if (!conf.input.packet)
		return;
	if (!strcmp(res->drop, "drop"))
		conf.action.behavior =
			RTE_PMD_I40E_PKT_TEMPLATE_REJECT;
	else
		conf.action.behavior =
			RTE_PMD_I40E_PKT_TEMPLATE_ACCEPT;
	conf.action.report_status =
			RTE_PMD_I40E_PKT_TEMPLATE_REPORT_ID;
	conf.action.rx_queue = res->queue_id;
	conf.soft_id = res->fd_id_value;
	add  = strcmp(res->ops, "del") ? 1 : 0;
	ret = rte_pmd_i40e_flow_add_del_packet_template(port,
							&conf,
							add);
	if (ret < 0)
		fprintf(stderr, "flow director config error: (%s)\n",
			strerror(-ret));
	close_file(conf.input.packet);
}

cmdline_parse_token_string_t cmd_flow_director_filter =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 flow_director_filter, "flow_director_filter");
cmdline_parse_token_num_t cmd_flow_director_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_flow_director_ops =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 ops, "add#del#update");
cmdline_parse_token_string_t cmd_flow_director_flow =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 flow, "flow");
cmdline_parse_token_string_t cmd_flow_director_flow_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
		flow_type, NULL);
cmdline_parse_token_string_t cmd_flow_director_drop =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 drop, "drop#fwd");
cmdline_parse_token_string_t cmd_flow_director_queue =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 queue, "queue");
cmdline_parse_token_num_t cmd_flow_director_queue_id =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      queue_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_flow_director_fd_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 fd_id, "fd_id");
cmdline_parse_token_num_t cmd_flow_director_fd_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_result,
			      fd_id_value, RTE_UINT32);

cmdline_parse_token_string_t cmd_flow_director_mode =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode, "mode");
cmdline_parse_token_string_t cmd_flow_director_mode_raw =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 mode_value, "raw");
cmdline_parse_token_string_t cmd_flow_director_packet =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 packet, "packet");
cmdline_parse_token_string_t cmd_flow_director_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_result,
				 filepath, NULL);

cmdline_parse_inst_t cmd_add_del_raw_flow_director = {
	.f = cmd_flow_director_filter_parsed,
	.data = NULL,
	.help_str = "flow_director_filter ... : Add or delete a raw flow "
		"director entry on NIC",
	.tokens = {
		(void *)&cmd_flow_director_filter,
		(void *)&cmd_flow_director_port_id,
		(void *)&cmd_flow_director_mode,
		(void *)&cmd_flow_director_mode_raw,
		(void *)&cmd_flow_director_ops,
		(void *)&cmd_flow_director_flow,
		(void *)&cmd_flow_director_flow_type,
		(void *)&cmd_flow_director_drop,
		(void *)&cmd_flow_director_queue,
		(void *)&cmd_flow_director_queue_id,
		(void *)&cmd_flow_director_fd_id,
		(void *)&cmd_flow_director_fd_id_value,
		(void *)&cmd_flow_director_packet,
		(void *)&cmd_flow_director_filepath,
		NULL,
	},
};

#endif /* RTE_NET_I40E */

/* *** deal with flow director mask *** */
struct cmd_flow_director_mask_result {
	cmdline_fixed_string_t flow_director_mask;
	portid_t port_id;
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
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_flow_director_mask_result *res = parsed_result;
	struct rte_eth_fdir_masks *mask;
	struct rte_port *port;

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	mask = &port->dev_conf.fdir_conf.mask;

	if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		if (strcmp(res->mode_value, "MAC-VLAN")) {
			fprintf(stderr, "Please set mode to MAC-VLAN.\n");
			return;
		}

		mask->vlan_tci_mask = rte_cpu_to_be_16(res->vlan_mask);
	} else if (fdir_conf.mode ==  RTE_FDIR_MODE_PERFECT_TUNNEL) {
		if (strcmp(res->mode_value, "Tunnel")) {
			fprintf(stderr, "Please set mode to Tunnel.\n");
			return;
		}

		mask->vlan_tci_mask = rte_cpu_to_be_16(res->vlan_mask);
		mask->mac_addr_byte_mask = res->mac_addr_byte_mask;
		mask->tunnel_id_mask = rte_cpu_to_be_32(res->tunnel_id_mask);
		mask->tunnel_type_mask = res->tunnel_type_mask;
	} else {
		if (strcmp(res->mode_value, "IP")) {
			fprintf(stderr, "Please set mode to IP.\n");
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
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_flow_director_mask_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 vlan, "vlan");
cmdline_parse_token_num_t cmd_flow_director_mask_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      vlan_mask, RTE_UINT16);
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
			      port_src, RTE_UINT16);
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
			      port_dst, RTE_UINT16);

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
			      mac_addr_byte_mask, RTE_UINT8);
cmdline_parse_token_string_t cmd_flow_director_mask_tunnel_type =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 tunnel_type, "tunnel-type");
cmdline_parse_token_num_t cmd_flow_director_mask_tunnel_type_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      tunnel_type_mask, RTE_UINT8);
cmdline_parse_token_string_t cmd_flow_director_mask_tunnel_id =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_mask_result,
				 tunnel_id, "tunnel-id");
cmdline_parse_token_num_t cmd_flow_director_mask_tunnel_id_value =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_director_mask_result,
			      tunnel_id_mask, RTE_UINT32);

cmdline_parse_inst_t cmd_set_flow_director_ip_mask = {
	.f = cmd_flow_director_mask_parsed,
	.data = NULL,
	.help_str = "flow_director_mask ... : "
		"Set IP mode flow director's mask on NIC",
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
	.help_str = "flow_director_mask ... : Set MAC VLAN mode "
		"flow director's mask on NIC",
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
	.help_str = "flow_director_mask ... : Set tunnel mode "
		"flow director's mask on NIC",
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

/* *** deal with flow director flexible payload configuration *** */
struct cmd_flow_director_flexpayload_result {
	cmdline_fixed_string_t flow_director_flexpayload;
	portid_t port_id;
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
			  __rte_unused struct cmdline *cl,
			  __rte_unused void *data)
{
	struct cmd_flow_director_flexpayload_result *res = parsed_result;
	struct rte_eth_flex_payload_cfg flex_cfg;
	struct rte_port *port;
	int ret = 0;

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
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
		fprintf(stderr, "error: Cannot parse flex payload input.\n");
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
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_flow_director_flexpayload_payload_layer =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flexpayload_result,
				 payload_layer, "raw#l2#l3#l4");
cmdline_parse_token_string_t cmd_flow_director_flexpayload_payload_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_director_flexpayload_result,
				 payload_cfg, NULL);

cmdline_parse_inst_t cmd_set_flow_director_flex_payload = {
	.f = cmd_flow_director_flxpld_parsed,
	.data = NULL,
	.help_str = "flow_director_flexpayload ... : "
		"Set flow director's flex payload on NIC",
	.tokens = {
		(void *)&cmd_flow_director_flexpayload,
		(void *)&cmd_flow_director_flexpayload_port_id,
		(void *)&cmd_flow_director_flexpayload_payload_layer,
		(void *)&cmd_flow_director_flexpayload_payload_cfg,
		NULL,
	},
};

/* Generic flow interface command. */
extern cmdline_parse_inst_t cmd_flow;

/* *** ADD/REMOVE A MULTICAST MAC ADDRESS TO/FROM A PORT *** */
struct cmd_mcast_addr_result {
	cmdline_fixed_string_t mcast_addr_cmd;
	cmdline_fixed_string_t what;
	uint16_t port_num;
	struct rte_ether_addr mc_addr;
};

static void cmd_mcast_addr_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_mcast_addr_result *res = parsed_result;

	if (!rte_is_multicast_ether_addr(&res->mc_addr)) {
		fprintf(stderr,
			"Invalid multicast addr " RTE_ETHER_ADDR_PRT_FMT "\n",
			RTE_ETHER_ADDR_BYTES(&res->mc_addr));
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
	TOKEN_NUM_INITIALIZER(struct cmd_mcast_addr_result, port_num,
				 RTE_UINT16);
cmdline_parse_token_etheraddr_t cmd_mcast_addr_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_mac_addr_result, address);

cmdline_parse_inst_t cmd_mcast_addr = {
	.f = cmd_mcast_addr_parsed,
	.data = (void *)0,
	.help_str = "mcast_addr add|remove <port_id> <mcast_addr>: "
		"Add/Remove multicast MAC address on port_id",
	.tokens = {
		(void *)&cmd_mcast_addr_cmd,
		(void *)&cmd_mcast_addr_what,
		(void *)&cmd_mcast_addr_portnum,
		(void *)&cmd_mcast_addr_addr,
		NULL,
	},
};

/* vf vlan anti spoof configuration */

/* Common result structure for vf vlan anti spoof */
struct cmd_vf_vlan_anti_spoof_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t antispoof;
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_vlan_anti_spoof_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 vf_id, RTE_UINT32);
cmdline_parse_token_string_t cmd_vf_vlan_anti_spoof_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_anti_spoof_result,
		 on_off, "on#off");

static void
cmd_set_vf_vlan_anti_spoof_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_vlan_anti_spoof_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_vlan_anti_spoof(res->port_id,
				res->vf_id, is_on);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_vlan_anti_spoof(res->port_id,
				res->vf_id, is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_vlan_anti_spoof(res->port_id,
				res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_anti_spoof = {
	.f = cmd_set_vf_vlan_anti_spoof_parsed,
	.data = NULL,
	.help_str = "set vf vlan antispoof <port_id> <vf_id> on|off",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_mac_anti_spoof_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 vf_id, RTE_UINT32);
cmdline_parse_token_string_t cmd_vf_mac_anti_spoof_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_mac_anti_spoof_result,
		 on_off, "on#off");

static void
cmd_set_vf_mac_anti_spoof_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_mac_anti_spoof_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_mac_anti_spoof(res->port_id,
			res->vf_id, is_on);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_mac_anti_spoof(res->port_id,
			res->vf_id, is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_mac_anti_spoof(res->port_id,
			res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_mac_anti_spoof = {
	.f = cmd_set_vf_mac_anti_spoof_parsed,
	.data = NULL,
	.help_str = "set vf mac antispoof <port_id> <vf_id> on|off",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_vlan_stripq_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_vlan_stripq_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_vlan_stripq_result,
		 on_off, "on#off");

static void
cmd_set_vf_vlan_stripq_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_vlan_stripq_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_vlan_stripq(res->port_id,
			res->vf_id, is_on);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_vlan_stripq(res->port_id,
			res->vf_id, is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_vlan_stripq(res->port_id,
			res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_stripq = {
	.f = cmd_set_vf_vlan_stripq_parsed,
	.data = NULL,
	.help_str = "set vf vlan stripq <port_id> <vf_id> on|off",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_vlan_insert_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_vlan_insert_vlan_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_vlan_insert_result,
		 vlan_id, RTE_UINT16);

static void
cmd_set_vf_vlan_insert_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_vlan_insert_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_vlan_insert(res->port_id, res->vf_id,
			res->vlan_id);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_vlan_insert(res->port_id, res->vf_id,
			res->vlan_id);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_vlan_insert(res->port_id, res->vf_id,
			res->vlan_id);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or vlan_id %d\n",
			res->vf_id, res->vlan_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_insert = {
	.f = cmd_set_vf_vlan_insert_parsed,
	.data = NULL,
	.help_str = "set vf vlan insert <port_id> <vf_id> <vlan_id>",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_tx_loopback_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_loopback_result,
		 on_off, "on#off");

static void
cmd_set_tx_loopback_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_tx_loopback_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_tx_loopback(res->port_id, is_on);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_tx_loopback(res->port_id, is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_tx_loopback(res->port_id, is_on);
#endif
#if defined RTE_BUS_DPAA && defined RTE_NET_DPAA
	if (ret == -ENOTSUP)
		ret = rte_pmd_dpaa_set_tx_loopback(res->port_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid is_on %d\n", is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_tx_loopback = {
	.f = cmd_set_tx_loopback_parsed,
	.data = NULL,
	.help_str = "set tx loopback <port_id> on|off",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_all_queues_drop_en_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_all_queues_drop_en_result,
		 on_off, "on#off");

static void
cmd_set_all_queues_drop_en_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_all_queues_drop_en_result *res = parsed_result;
	int ret = -ENOTSUP;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_all_queues_drop_en(res->port_id, is_on);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_all_queues_drop_en(res->port_id, is_on);
#endif
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid is_on %d\n", is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_all_queues_drop_en = {
	.f = cmd_set_all_queues_drop_en_parsed,
	.data = NULL,
	.help_str = "set all queues drop <port_id> on|off",
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
	portid_t port_id;
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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_split_drop_en_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_vf_split_drop_en_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_split_drop_en_result,
		 on_off, "on#off");

static void
cmd_set_vf_split_drop_en_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_split_drop_en_result *res = parsed_result;
	int ret = -ENOTSUP;
	int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	ret = rte_pmd_ixgbe_set_vf_split_drop_en(res->port_id, res->vf_id,
			is_on);
#endif
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "not supported on port %d\n", res->port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_split_drop_en = {
	.f = cmd_set_vf_split_drop_en_parsed,
	.data = NULL,
	.help_str = "set vf split drop <port_id> <vf_id> on|off",
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
	portid_t port_id;
	uint16_t vf_id;
	struct rte_ether_addr mac_addr;

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
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_set_vf_mac_addr_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_mac_addr_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_etheraddr_t cmd_set_vf_mac_addr_mac_addr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_vf_mac_addr_result,
		 mac_addr);

static void
cmd_set_vf_mac_addr_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vf_mac_addr_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP)
		ret = rte_pmd_ixgbe_set_vf_mac_addr(res->port_id, res->vf_id,
				&res->mac_addr);
#endif
#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_set_vf_mac_addr(res->port_id, res->vf_id,
				&res->mac_addr);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_set_vf_mac_addr(res->port_id, res->vf_id,
				&res->mac_addr);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or mac_addr\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_mac_addr = {
	.f = cmd_set_vf_mac_addr_parsed,
	.data = NULL,
	.help_str = "set vf mac addr <port_id> <vf_id> <mac_addr>",
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

/* MACsec configuration */

/* Common result structure for MACsec offload enable */
struct cmd_macsec_offload_on_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t macsec;
	cmdline_fixed_string_t offload;
	portid_t port_id;
	cmdline_fixed_string_t on;
	cmdline_fixed_string_t encrypt;
	cmdline_fixed_string_t en_on_off;
	cmdline_fixed_string_t replay_protect;
	cmdline_fixed_string_t rp_on_off;
};

/* Common CLI fields for MACsec offload disable */
cmdline_parse_token_string_t cmd_macsec_offload_on_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 set, "set");
cmdline_parse_token_string_t cmd_macsec_offload_on_macsec =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 macsec, "macsec");
cmdline_parse_token_string_t cmd_macsec_offload_on_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 offload, "offload");
cmdline_parse_token_num_t cmd_macsec_offload_on_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_macsec_offload_on_on =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 on, "on");
cmdline_parse_token_string_t cmd_macsec_offload_on_encrypt =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 encrypt, "encrypt");
cmdline_parse_token_string_t cmd_macsec_offload_on_en_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 en_on_off, "on#off");
cmdline_parse_token_string_t cmd_macsec_offload_on_replay_protect =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 replay_protect, "replay-protect");
cmdline_parse_token_string_t cmd_macsec_offload_on_rp_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_on_result,
		 rp_on_off, "on#off");

static void
cmd_set_macsec_offload_on_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_macsec_offload_on_result *res = parsed_result;
	int ret = -ENOTSUP;
	portid_t port_id = res->port_id;
	int en = (strcmp(res->en_on_off, "on") == 0) ? 1 : 0;
	int rp = (strcmp(res->rp_on_off, "on") == 0) ? 1 : 0;
	struct rte_eth_dev_info dev_info;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (!port_is_stopped(port_id)) {
		fprintf(stderr, "Please stop port %d first\n", port_id);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MACSEC_INSERT) {
#ifdef RTE_NET_IXGBE
		ret = rte_pmd_ixgbe_macsec_enable(port_id, en, rp);
#endif
	}
	RTE_SET_USED(en);
	RTE_SET_USED(rp);

	switch (ret) {
	case 0:
		ports[port_id].dev_conf.txmode.offloads |=
						RTE_ETH_TX_OFFLOAD_MACSEC_INSERT;
		cmd_reconfig_device_queue(port_id, 1, 1);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "not supported on port %d\n", port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_macsec_offload_on = {
	.f = cmd_set_macsec_offload_on_parsed,
	.data = NULL,
	.help_str = "set macsec offload <port_id> on "
		"encrypt on|off replay-protect on|off",
	.tokens = {
		(void *)&cmd_macsec_offload_on_set,
		(void *)&cmd_macsec_offload_on_macsec,
		(void *)&cmd_macsec_offload_on_offload,
		(void *)&cmd_macsec_offload_on_port_id,
		(void *)&cmd_macsec_offload_on_on,
		(void *)&cmd_macsec_offload_on_encrypt,
		(void *)&cmd_macsec_offload_on_en_on_off,
		(void *)&cmd_macsec_offload_on_replay_protect,
		(void *)&cmd_macsec_offload_on_rp_on_off,
		NULL,
	},
};

/* Common result structure for MACsec offload disable */
struct cmd_macsec_offload_off_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t macsec;
	cmdline_fixed_string_t offload;
	portid_t port_id;
	cmdline_fixed_string_t off;
};

/* Common CLI fields for MACsec offload disable */
cmdline_parse_token_string_t cmd_macsec_offload_off_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_off_result,
		 set, "set");
cmdline_parse_token_string_t cmd_macsec_offload_off_macsec =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_off_result,
		 macsec, "macsec");
cmdline_parse_token_string_t cmd_macsec_offload_off_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_off_result,
		 offload, "offload");
cmdline_parse_token_num_t cmd_macsec_offload_off_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_offload_off_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_macsec_offload_off_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_offload_off_result,
		 off, "off");

static void
cmd_set_macsec_offload_off_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_macsec_offload_off_result *res = parsed_result;
	int ret = -ENOTSUP;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (!port_is_stopped(port_id)) {
		fprintf(stderr, "Please stop port %d first\n", port_id);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MACSEC_INSERT) {
#ifdef RTE_NET_IXGBE
		ret = rte_pmd_ixgbe_macsec_disable(port_id);
#endif
	}
	switch (ret) {
	case 0:
		ports[port_id].dev_conf.txmode.offloads &=
						~RTE_ETH_TX_OFFLOAD_MACSEC_INSERT;
		cmd_reconfig_device_queue(port_id, 1, 1);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "not supported on port %d\n", port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_macsec_offload_off = {
	.f = cmd_set_macsec_offload_off_parsed,
	.data = NULL,
	.help_str = "set macsec offload <port_id> off",
	.tokens = {
		(void *)&cmd_macsec_offload_off_set,
		(void *)&cmd_macsec_offload_off_macsec,
		(void *)&cmd_macsec_offload_off_offload,
		(void *)&cmd_macsec_offload_off_port_id,
		(void *)&cmd_macsec_offload_off_off,
		NULL,
	},
};

/* Common result structure for MACsec secure connection configure */
struct cmd_macsec_sc_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t macsec;
	cmdline_fixed_string_t sc;
	cmdline_fixed_string_t tx_rx;
	portid_t port_id;
	struct rte_ether_addr mac;
	uint16_t pi;
};

/* Common CLI fields for MACsec secure connection configure */
cmdline_parse_token_string_t cmd_macsec_sc_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sc_result,
		 set, "set");
cmdline_parse_token_string_t cmd_macsec_sc_macsec =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sc_result,
		 macsec, "macsec");
cmdline_parse_token_string_t cmd_macsec_sc_sc =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sc_result,
		 sc, "sc");
cmdline_parse_token_string_t cmd_macsec_sc_tx_rx =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sc_result,
		 tx_rx, "tx#rx");
cmdline_parse_token_num_t cmd_macsec_sc_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sc_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_etheraddr_t cmd_macsec_sc_mac =
	TOKEN_ETHERADDR_INITIALIZER
		(struct cmd_macsec_sc_result,
		 mac);
cmdline_parse_token_num_t cmd_macsec_sc_pi =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sc_result,
		 pi, RTE_UINT16);

static void
cmd_set_macsec_sc_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_macsec_sc_result *res = parsed_result;
	int ret = -ENOTSUP;
	int is_tx = (strcmp(res->tx_rx, "tx") == 0) ? 1 : 0;

#ifdef RTE_NET_IXGBE
	ret = is_tx ?
		rte_pmd_ixgbe_macsec_config_txsc(res->port_id,
				res->mac.addr_bytes) :
		rte_pmd_ixgbe_macsec_config_rxsc(res->port_id,
				res->mac.addr_bytes, res->pi);
#endif
	RTE_SET_USED(is_tx);

	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "not supported on port %d\n", res->port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_macsec_sc = {
	.f = cmd_set_macsec_sc_parsed,
	.data = NULL,
	.help_str = "set macsec sc tx|rx <port_id> <mac> <pi>",
	.tokens = {
		(void *)&cmd_macsec_sc_set,
		(void *)&cmd_macsec_sc_macsec,
		(void *)&cmd_macsec_sc_sc,
		(void *)&cmd_macsec_sc_tx_rx,
		(void *)&cmd_macsec_sc_port_id,
		(void *)&cmd_macsec_sc_mac,
		(void *)&cmd_macsec_sc_pi,
		NULL,
	},
};

/* Common result structure for MACsec secure connection configure */
struct cmd_macsec_sa_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t macsec;
	cmdline_fixed_string_t sa;
	cmdline_fixed_string_t tx_rx;
	portid_t port_id;
	uint8_t idx;
	uint8_t an;
	uint32_t pn;
	cmdline_fixed_string_t key;
};

/* Common CLI fields for MACsec secure connection configure */
cmdline_parse_token_string_t cmd_macsec_sa_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sa_result,
		 set, "set");
cmdline_parse_token_string_t cmd_macsec_sa_macsec =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sa_result,
		 macsec, "macsec");
cmdline_parse_token_string_t cmd_macsec_sa_sa =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sa_result,
		 sa, "sa");
cmdline_parse_token_string_t cmd_macsec_sa_tx_rx =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sa_result,
		 tx_rx, "tx#rx");
cmdline_parse_token_num_t cmd_macsec_sa_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sa_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_macsec_sa_idx =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sa_result,
		 idx, RTE_UINT8);
cmdline_parse_token_num_t cmd_macsec_sa_an =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sa_result,
		 an, RTE_UINT8);
cmdline_parse_token_num_t cmd_macsec_sa_pn =
	TOKEN_NUM_INITIALIZER
		(struct cmd_macsec_sa_result,
		 pn, RTE_UINT32);
cmdline_parse_token_string_t cmd_macsec_sa_key =
	TOKEN_STRING_INITIALIZER
		(struct cmd_macsec_sa_result,
		 key, NULL);

static void
cmd_set_macsec_sa_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_macsec_sa_result *res = parsed_result;
	int ret = -ENOTSUP;
	int is_tx = (strcmp(res->tx_rx, "tx") == 0) ? 1 : 0;
	uint8_t key[16] = { 0 };
	uint8_t xdgt0;
	uint8_t xdgt1;
	int key_len;
	int i;

	key_len = strlen(res->key) / 2;
	if (key_len > 16)
		key_len = 16;

	for (i = 0; i < key_len; i++) {
		xdgt0 = parse_and_check_key_hexa_digit(res->key, (i * 2));
		if (xdgt0 == 0xFF)
			return;
		xdgt1 = parse_and_check_key_hexa_digit(res->key, (i * 2) + 1);
		if (xdgt1 == 0xFF)
			return;
		key[i] = (uint8_t) ((xdgt0 * 16) + xdgt1);
	}

#ifdef RTE_NET_IXGBE
	ret = is_tx ?
		rte_pmd_ixgbe_macsec_select_txsa(res->port_id,
			res->idx, res->an, res->pn, key) :
		rte_pmd_ixgbe_macsec_select_rxsa(res->port_id,
			res->idx, res->an, res->pn, key);
#endif
	RTE_SET_USED(is_tx);
	RTE_SET_USED(key);

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid idx %d or an %d\n", res->idx, res->an);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "not supported on port %d\n", res->port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_macsec_sa = {
	.f = cmd_set_macsec_sa_parsed,
	.data = NULL,
	.help_str = "set macsec sa tx|rx <port_id> <idx> <an> <pn> <key>",
	.tokens = {
		(void *)&cmd_macsec_sa_set,
		(void *)&cmd_macsec_sa_macsec,
		(void *)&cmd_macsec_sa_sa,
		(void *)&cmd_macsec_sa_tx_rx,
		(void *)&cmd_macsec_sa_port_id,
		(void *)&cmd_macsec_sa_idx,
		(void *)&cmd_macsec_sa_an,
		(void *)&cmd_macsec_sa_pn,
		(void *)&cmd_macsec_sa_key,
		NULL,
	},
};

/* VF unicast promiscuous mode configuration */

/* Common result structure for VF unicast promiscuous mode */
struct cmd_vf_promisc_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t promisc;
	portid_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for VF unicast promiscuous mode enable disable */
cmdline_parse_token_string_t cmd_vf_promisc_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_promisc_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_promisc_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_promisc_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_promisc_promisc =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_promisc_result,
		 promisc, "promisc");
cmdline_parse_token_num_t cmd_vf_promisc_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_promisc_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_promisc_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_promisc_result,
		 vf_id, RTE_UINT32);
cmdline_parse_token_string_t cmd_vf_promisc_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_promisc_result,
		 on_off, "on#off");

static void
cmd_set_vf_promisc_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_promisc_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_unicast_promisc(res->port_id,
						  res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_promisc = {
	.f = cmd_set_vf_promisc_parsed,
	.data = NULL,
	.help_str = "set vf promisc <port_id> <vf_id> on|off: "
		"Set unicast promiscuous mode for a VF from the PF",
	.tokens = {
		(void *)&cmd_vf_promisc_set,
		(void *)&cmd_vf_promisc_vf,
		(void *)&cmd_vf_promisc_promisc,
		(void *)&cmd_vf_promisc_port_id,
		(void *)&cmd_vf_promisc_vf_id,
		(void *)&cmd_vf_promisc_on_off,
		NULL,
	},
};

/* VF multicast promiscuous mode configuration */

/* Common result structure for VF multicast promiscuous mode */
struct cmd_vf_allmulti_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t allmulti;
	portid_t port_id;
	uint32_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for VF multicast promiscuous mode enable disable */
cmdline_parse_token_string_t cmd_vf_allmulti_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_allmulti_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_allmulti_allmulti =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 allmulti, "allmulti");
cmdline_parse_token_num_t cmd_vf_allmulti_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_allmulti_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 vf_id, RTE_UINT32);
cmdline_parse_token_string_t cmd_vf_allmulti_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_allmulti_result,
		 on_off, "on#off");

static void
cmd_set_vf_allmulti_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_allmulti_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_multicast_promisc(res->port_id,
						    res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_allmulti = {
	.f = cmd_set_vf_allmulti_parsed,
	.data = NULL,
	.help_str = "set vf allmulti <port_id> <vf_id> on|off: "
		"Set multicast promiscuous mode for a VF from the PF",
	.tokens = {
		(void *)&cmd_vf_allmulti_set,
		(void *)&cmd_vf_allmulti_vf,
		(void *)&cmd_vf_allmulti_allmulti,
		(void *)&cmd_vf_allmulti_port_id,
		(void *)&cmd_vf_allmulti_vf_id,
		(void *)&cmd_vf_allmulti_on_off,
		NULL,
	},
};

/* vf broadcast mode configuration */

/* Common result structure for vf broadcast */
struct cmd_set_vf_broadcast_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t broadcast;
	portid_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf broadcast enable disable */
cmdline_parse_token_string_t cmd_set_vf_broadcast_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 set, "set");
cmdline_parse_token_string_t cmd_set_vf_broadcast_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_set_vf_broadcast_broadcast =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 broadcast, "broadcast");
cmdline_parse_token_num_t cmd_set_vf_broadcast_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_set_vf_broadcast_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vf_broadcast_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_broadcast_result,
		 on_off, "on#off");

static void
cmd_set_vf_broadcast_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vf_broadcast_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_broadcast(res->port_id,
					    res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_broadcast = {
	.f = cmd_set_vf_broadcast_parsed,
	.data = NULL,
	.help_str = "set vf broadcast <port_id> <vf_id> on|off",
	.tokens = {
		(void *)&cmd_set_vf_broadcast_set,
		(void *)&cmd_set_vf_broadcast_vf,
		(void *)&cmd_set_vf_broadcast_broadcast,
		(void *)&cmd_set_vf_broadcast_port_id,
		(void *)&cmd_set_vf_broadcast_vf_id,
		(void *)&cmd_set_vf_broadcast_on_off,
		NULL,
	},
};

/* vf vlan tag configuration */

/* Common result structure for vf vlan tag */
struct cmd_set_vf_vlan_tag_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t vlan;
	cmdline_fixed_string_t tag;
	portid_t port_id;
	uint16_t vf_id;
	cmdline_fixed_string_t on_off;
};

/* Common CLI fields for vf vlan tag enable disable */
cmdline_parse_token_string_t cmd_set_vf_vlan_tag_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 set, "set");
cmdline_parse_token_string_t cmd_set_vf_vlan_tag_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_set_vf_vlan_tag_vlan =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 vlan, "vlan");
cmdline_parse_token_string_t cmd_set_vf_vlan_tag_tag =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 tag, "tag");
cmdline_parse_token_num_t cmd_set_vf_vlan_tag_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_set_vf_vlan_tag_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vf_vlan_tag_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_vf_vlan_tag_result,
		 on_off, "on#off");

static void
cmd_set_vf_vlan_tag_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vf_vlan_tag_result *res = parsed_result;
	int ret = -ENOTSUP;

	__rte_unused int is_on = (strcmp(res->on_off, "on") == 0) ? 1 : 0;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_vlan_tag(res->port_id,
					   res->vf_id, is_on);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or is_on %d\n",
			res->vf_id, is_on);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_set_vf_vlan_tag = {
	.f = cmd_set_vf_vlan_tag_parsed,
	.data = NULL,
	.help_str = "set vf vlan tag <port_id> <vf_id> on|off",
	.tokens = {
		(void *)&cmd_set_vf_vlan_tag_set,
		(void *)&cmd_set_vf_vlan_tag_vf,
		(void *)&cmd_set_vf_vlan_tag_vlan,
		(void *)&cmd_set_vf_vlan_tag_tag,
		(void *)&cmd_set_vf_vlan_tag_port_id,
		(void *)&cmd_set_vf_vlan_tag_vf_id,
		(void *)&cmd_set_vf_vlan_tag_on_off,
		NULL,
	},
};

/* Common definition of VF and TC TX bandwidth configuration */
struct cmd_vf_tc_bw_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t tc;
	cmdline_fixed_string_t tx;
	cmdline_fixed_string_t min_bw;
	cmdline_fixed_string_t max_bw;
	cmdline_fixed_string_t strict_link_prio;
	portid_t port_id;
	uint16_t vf_id;
	uint8_t tc_no;
	uint32_t bw;
	cmdline_fixed_string_t bw_list;
	uint8_t tc_map;
};

cmdline_parse_token_string_t cmd_vf_tc_bw_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 set, "set");
cmdline_parse_token_string_t cmd_vf_tc_bw_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_vf_tc_bw_tc =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 tc, "tc");
cmdline_parse_token_string_t cmd_vf_tc_bw_tx =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 tx, "tx");
cmdline_parse_token_string_t cmd_vf_tc_bw_strict_link_prio =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 strict_link_prio, "strict-link-priority");
cmdline_parse_token_string_t cmd_vf_tc_bw_min_bw =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 min_bw, "min-bandwidth");
cmdline_parse_token_string_t cmd_vf_tc_bw_max_bw =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 max_bw, "max-bandwidth");
cmdline_parse_token_num_t cmd_vf_tc_bw_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_tc_bw_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 vf_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_vf_tc_bw_tc_no =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 tc_no, RTE_UINT8);
cmdline_parse_token_num_t cmd_vf_tc_bw_bw =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 bw, RTE_UINT32);
cmdline_parse_token_string_t cmd_vf_tc_bw_bw_list =
	TOKEN_STRING_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 bw_list, NULL);
cmdline_parse_token_num_t cmd_vf_tc_bw_tc_map =
	TOKEN_NUM_INITIALIZER
		(struct cmd_vf_tc_bw_result,
		 tc_map, RTE_UINT8);

/* VF max bandwidth setting */
static void
cmd_vf_max_bw_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_max_bw(res->port_id,
					 res->vf_id, res->bw);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or bandwidth %d\n",
			res->vf_id, res->bw);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_vf_max_bw = {
	.f = cmd_vf_max_bw_parsed,
	.data = NULL,
	.help_str = "set vf tx max-bandwidth <port_id> <vf_id> <bandwidth>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_max_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_bw,
		NULL,
	},
};

static int
vf_tc_min_bw_parse_bw_list(uint8_t *bw_list,
			   uint8_t *tc_num,
			   char *str)
{
	uint32_t size;
	const char *p, *p0 = str;
	char s[256];
	char *end;
	char *str_fld[16];
	uint16_t i;
	int ret;

	p = strchr(p0, '(');
	if (p == NULL) {
		fprintf(stderr,
			"The bandwidth-list should be '(bw1, bw2, ...)'\n");
		return -1;
	}
	p++;
	p0 = strchr(p, ')');
	if (p0 == NULL) {
		fprintf(stderr,
			"The bandwidth-list should be '(bw1, bw2, ...)'\n");
		return -1;
	}
	size = p0 - p;
	if (size >= sizeof(s)) {
		fprintf(stderr,
			"The string size exceeds the internal buffer size\n");
		return -1;
	}
	snprintf(s, sizeof(s), "%.*s", size, p);
	ret = rte_strsplit(s, sizeof(s), str_fld, 16, ',');
	if (ret <= 0) {
		fprintf(stderr, "Failed to get the bandwidth list.\n");
		return -1;
	}
	*tc_num = ret;
	for (i = 0; i < ret; i++)
		bw_list[i] = (uint8_t)strtoul(str_fld[i], &end, 0);

	return 0;
}

/* TC min bandwidth setting */
static void
cmd_vf_tc_min_bw_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	uint8_t tc_num;
	uint8_t bw[16];
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	ret = vf_tc_min_bw_parse_bw_list(bw, &tc_num, res->bw_list);
	if (ret)
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_tc_bw_alloc(res->port_id, res->vf_id,
					      tc_num, bw);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d or bandwidth\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_vf_tc_min_bw = {
	.f = cmd_vf_tc_min_bw_parsed,
	.data = NULL,
	.help_str = "set vf tc tx min-bandwidth <port_id> <vf_id>"
		    " <bw1, bw2, ...>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tc,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_min_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_bw_list,
		NULL,
	},
};

static void
cmd_tc_min_bw_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	struct rte_port *port;
	uint8_t tc_num;
	uint8_t bw[16];
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	port = &ports[res->port_id];
	/** Check if the port is not started **/
	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr, "Please stop port %d first\n", res->port_id);
		return;
	}

	ret = vf_tc_min_bw_parse_bw_list(bw, &tc_num, res->bw_list);
	if (ret)
		return;

#ifdef RTE_NET_IXGBE
	ret = rte_pmd_ixgbe_set_tc_bw_alloc(res->port_id, tc_num, bw);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid bandwidth\n");
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_tc_min_bw = {
	.f = cmd_tc_min_bw_parsed,
	.data = NULL,
	.help_str = "set tc tx min-bandwidth <port_id> <bw1, bw2, ...>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_tc,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_min_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_bw_list,
		NULL,
	},
};

/* TC max bandwidth setting */
static void
cmd_vf_tc_max_bw_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_vf_tc_max_bw(res->port_id, res->vf_id,
					    res->tc_no, res->bw);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr,
			"invalid vf_id %d, tc_no %d or bandwidth %d\n",
			res->vf_id, res->tc_no, res->bw);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_vf_tc_max_bw = {
	.f = cmd_vf_tc_max_bw_parsed,
	.data = NULL,
	.help_str = "set vf tc tx max-bandwidth <port_id> <vf_id> <tc_no>"
		    " <bandwidth>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_vf,
		(void *)&cmd_vf_tc_bw_tc,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_max_bw,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_vf_id,
		(void *)&cmd_vf_tc_bw_tc_no,
		(void *)&cmd_vf_tc_bw_bw,
		NULL,
	},
};

/** Set VXLAN encapsulation details */
struct cmd_set_vxlan_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t vxlan;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
	uint32_t vni;
	uint16_t udp_src;
	uint16_t udp_dst;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t tci;
	uint8_t tos;
	uint8_t ttl;
	struct rte_ether_addr eth_src;
	struct rte_ether_addr eth_dst;
};

cmdline_parse_token_string_t cmd_set_vxlan_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, set, "set");
cmdline_parse_token_string_t cmd_set_vxlan_vxlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, vxlan, "vxlan");
cmdline_parse_token_string_t cmd_set_vxlan_vxlan_tos_ttl =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, vxlan,
				 "vxlan-tos-ttl");
cmdline_parse_token_string_t cmd_set_vxlan_vxlan_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, vxlan,
				 "vxlan-with-vlan");
cmdline_parse_token_string_t cmd_set_vxlan_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "ip-version");
cmdline_parse_token_string_t cmd_set_vxlan_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, ip_version,
				 "ipv4#ipv6");
cmdline_parse_token_string_t cmd_set_vxlan_vni =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "vni");
cmdline_parse_token_num_t cmd_set_vxlan_vni_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, vni, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_vxlan_udp_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "udp-src");
cmdline_parse_token_num_t cmd_set_vxlan_udp_src_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, udp_src, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vxlan_udp_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "udp-dst");
cmdline_parse_token_num_t cmd_set_vxlan_udp_dst_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, udp_dst, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vxlan_ip_tos =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "ip-tos");
cmdline_parse_token_num_t cmd_set_vxlan_ip_tos_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, tos, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_vxlan_ip_ttl =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "ip-ttl");
cmdline_parse_token_num_t cmd_set_vxlan_ip_ttl_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, ttl, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_vxlan_ip_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "ip-src");
cmdline_parse_token_ipaddr_t cmd_set_vxlan_ip_src_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_vxlan_result, ip_src);
cmdline_parse_token_string_t cmd_set_vxlan_ip_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "ip-dst");
cmdline_parse_token_ipaddr_t cmd_set_vxlan_ip_dst_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_vxlan_result, ip_dst);
cmdline_parse_token_string_t cmd_set_vxlan_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "vlan-tci");
cmdline_parse_token_num_t cmd_set_vxlan_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_vxlan_result, tci, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_vxlan_eth_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "eth-src");
cmdline_parse_token_etheraddr_t cmd_set_vxlan_eth_src_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_vxlan_result, eth_src);
cmdline_parse_token_string_t cmd_set_vxlan_eth_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_vxlan_result, pos_token,
				 "eth-dst");
cmdline_parse_token_etheraddr_t cmd_set_vxlan_eth_dst_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_vxlan_result, eth_dst);

static void cmd_set_vxlan_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_vxlan_result *res = parsed_result;
	union {
		uint32_t vxlan_id;
		uint8_t vni[4];
	} id = {
		.vxlan_id = rte_cpu_to_be_32(res->vni) & RTE_BE32(0x00ffffff),
	};

	vxlan_encap_conf.select_tos_ttl = 0;
	if (strcmp(res->vxlan, "vxlan") == 0)
		vxlan_encap_conf.select_vlan = 0;
	else if (strcmp(res->vxlan, "vxlan-with-vlan") == 0)
		vxlan_encap_conf.select_vlan = 1;
	else if (strcmp(res->vxlan, "vxlan-tos-ttl") == 0) {
		vxlan_encap_conf.select_vlan = 0;
		vxlan_encap_conf.select_tos_ttl = 1;
	}
	if (strcmp(res->ip_version, "ipv4") == 0)
		vxlan_encap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		vxlan_encap_conf.select_ipv4 = 0;
	else
		return;
	rte_memcpy(vxlan_encap_conf.vni, &id.vni[1], 3);
	vxlan_encap_conf.udp_src = rte_cpu_to_be_16(res->udp_src);
	vxlan_encap_conf.udp_dst = rte_cpu_to_be_16(res->udp_dst);
	vxlan_encap_conf.ip_tos = res->tos;
	vxlan_encap_conf.ip_ttl = res->ttl;
	if (vxlan_encap_conf.select_ipv4) {
		IPV4_ADDR_TO_UINT(res->ip_src, vxlan_encap_conf.ipv4_src);
		IPV4_ADDR_TO_UINT(res->ip_dst, vxlan_encap_conf.ipv4_dst);
	} else {
		IPV6_ADDR_TO_ARRAY(res->ip_src, vxlan_encap_conf.ipv6_src);
		IPV6_ADDR_TO_ARRAY(res->ip_dst, vxlan_encap_conf.ipv6_dst);
	}
	if (vxlan_encap_conf.select_vlan)
		vxlan_encap_conf.vlan_tci = rte_cpu_to_be_16(res->tci);
	rte_memcpy(vxlan_encap_conf.eth_src, res->eth_src.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
	rte_memcpy(vxlan_encap_conf.eth_dst, res->eth_dst.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
}

cmdline_parse_inst_t cmd_set_vxlan = {
	.f = cmd_set_vxlan_parsed,
	.data = NULL,
	.help_str = "set vxlan ip-version ipv4|ipv6 vni <vni> udp-src"
		" <udp-src> udp-dst <udp-dst> ip-src <ip-src> ip-dst <ip-dst>"
		" eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_vxlan_set,
		(void *)&cmd_set_vxlan_vxlan,
		(void *)&cmd_set_vxlan_ip_version,
		(void *)&cmd_set_vxlan_ip_version_value,
		(void *)&cmd_set_vxlan_vni,
		(void *)&cmd_set_vxlan_vni_value,
		(void *)&cmd_set_vxlan_udp_src,
		(void *)&cmd_set_vxlan_udp_src_value,
		(void *)&cmd_set_vxlan_udp_dst,
		(void *)&cmd_set_vxlan_udp_dst_value,
		(void *)&cmd_set_vxlan_ip_src,
		(void *)&cmd_set_vxlan_ip_src_value,
		(void *)&cmd_set_vxlan_ip_dst,
		(void *)&cmd_set_vxlan_ip_dst_value,
		(void *)&cmd_set_vxlan_eth_src,
		(void *)&cmd_set_vxlan_eth_src_value,
		(void *)&cmd_set_vxlan_eth_dst,
		(void *)&cmd_set_vxlan_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_vxlan_tos_ttl = {
	.f = cmd_set_vxlan_parsed,
	.data = NULL,
	.help_str = "set vxlan-tos-ttl ip-version ipv4|ipv6 vni <vni> udp-src"
		" <udp-src> udp-dst <udp-dst> ip-tos <ip-tos> ip-ttl <ip-ttl>"
		" ip-src <ip-src> ip-dst <ip-dst> eth-src <eth-src>"
		" eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_vxlan_set,
		(void *)&cmd_set_vxlan_vxlan_tos_ttl,
		(void *)&cmd_set_vxlan_ip_version,
		(void *)&cmd_set_vxlan_ip_version_value,
		(void *)&cmd_set_vxlan_vni,
		(void *)&cmd_set_vxlan_vni_value,
		(void *)&cmd_set_vxlan_udp_src,
		(void *)&cmd_set_vxlan_udp_src_value,
		(void *)&cmd_set_vxlan_udp_dst,
		(void *)&cmd_set_vxlan_udp_dst_value,
		(void *)&cmd_set_vxlan_ip_tos,
		(void *)&cmd_set_vxlan_ip_tos_value,
		(void *)&cmd_set_vxlan_ip_ttl,
		(void *)&cmd_set_vxlan_ip_ttl_value,
		(void *)&cmd_set_vxlan_ip_src,
		(void *)&cmd_set_vxlan_ip_src_value,
		(void *)&cmd_set_vxlan_ip_dst,
		(void *)&cmd_set_vxlan_ip_dst_value,
		(void *)&cmd_set_vxlan_eth_src,
		(void *)&cmd_set_vxlan_eth_src_value,
		(void *)&cmd_set_vxlan_eth_dst,
		(void *)&cmd_set_vxlan_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_vxlan_with_vlan = {
	.f = cmd_set_vxlan_parsed,
	.data = NULL,
	.help_str = "set vxlan-with-vlan ip-version ipv4|ipv6 vni <vni>"
		" udp-src <udp-src> udp-dst <udp-dst> ip-src <ip-src> ip-dst"
		" <ip-dst> vlan-tci <vlan-tci> eth-src <eth-src> eth-dst"
		" <eth-dst>",
	.tokens = {
		(void *)&cmd_set_vxlan_set,
		(void *)&cmd_set_vxlan_vxlan_with_vlan,
		(void *)&cmd_set_vxlan_ip_version,
		(void *)&cmd_set_vxlan_ip_version_value,
		(void *)&cmd_set_vxlan_vni,
		(void *)&cmd_set_vxlan_vni_value,
		(void *)&cmd_set_vxlan_udp_src,
		(void *)&cmd_set_vxlan_udp_src_value,
		(void *)&cmd_set_vxlan_udp_dst,
		(void *)&cmd_set_vxlan_udp_dst_value,
		(void *)&cmd_set_vxlan_ip_src,
		(void *)&cmd_set_vxlan_ip_src_value,
		(void *)&cmd_set_vxlan_ip_dst,
		(void *)&cmd_set_vxlan_ip_dst_value,
		(void *)&cmd_set_vxlan_vlan,
		(void *)&cmd_set_vxlan_vlan_value,
		(void *)&cmd_set_vxlan_eth_src,
		(void *)&cmd_set_vxlan_eth_src_value,
		(void *)&cmd_set_vxlan_eth_dst,
		(void *)&cmd_set_vxlan_eth_dst_value,
		NULL,
	},
};

/** Set NVGRE encapsulation details */
struct cmd_set_nvgre_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t nvgre;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t tni;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t tci;
	struct rte_ether_addr eth_src;
	struct rte_ether_addr eth_dst;
};

cmdline_parse_token_string_t cmd_set_nvgre_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, set, "set");
cmdline_parse_token_string_t cmd_set_nvgre_nvgre =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, nvgre, "nvgre");
cmdline_parse_token_string_t cmd_set_nvgre_nvgre_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, nvgre,
				 "nvgre-with-vlan");
cmdline_parse_token_string_t cmd_set_nvgre_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "ip-version");
cmdline_parse_token_string_t cmd_set_nvgre_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, ip_version,
				 "ipv4#ipv6");
cmdline_parse_token_string_t cmd_set_nvgre_tni =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "tni");
cmdline_parse_token_num_t cmd_set_nvgre_tni_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_nvgre_result, tni, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_nvgre_ip_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "ip-src");
cmdline_parse_token_num_t cmd_set_nvgre_ip_src_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_nvgre_result, ip_src);
cmdline_parse_token_string_t cmd_set_nvgre_ip_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "ip-dst");
cmdline_parse_token_ipaddr_t cmd_set_nvgre_ip_dst_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_nvgre_result, ip_dst);
cmdline_parse_token_string_t cmd_set_nvgre_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "vlan-tci");
cmdline_parse_token_num_t cmd_set_nvgre_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_nvgre_result, tci, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_nvgre_eth_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "eth-src");
cmdline_parse_token_etheraddr_t cmd_set_nvgre_eth_src_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_nvgre_result, eth_src);
cmdline_parse_token_string_t cmd_set_nvgre_eth_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_nvgre_result, pos_token,
				 "eth-dst");
cmdline_parse_token_etheraddr_t cmd_set_nvgre_eth_dst_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_nvgre_result, eth_dst);

static void cmd_set_nvgre_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_nvgre_result *res = parsed_result;
	union {
		uint32_t nvgre_tni;
		uint8_t tni[4];
	} id = {
		.nvgre_tni = rte_cpu_to_be_32(res->tni) & RTE_BE32(0x00ffffff),
	};

	if (strcmp(res->nvgre, "nvgre") == 0)
		nvgre_encap_conf.select_vlan = 0;
	else if (strcmp(res->nvgre, "nvgre-with-vlan") == 0)
		nvgre_encap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		nvgre_encap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		nvgre_encap_conf.select_ipv4 = 0;
	else
		return;
	rte_memcpy(nvgre_encap_conf.tni, &id.tni[1], 3);
	if (nvgre_encap_conf.select_ipv4) {
		IPV4_ADDR_TO_UINT(res->ip_src, nvgre_encap_conf.ipv4_src);
		IPV4_ADDR_TO_UINT(res->ip_dst, nvgre_encap_conf.ipv4_dst);
	} else {
		IPV6_ADDR_TO_ARRAY(res->ip_src, nvgre_encap_conf.ipv6_src);
		IPV6_ADDR_TO_ARRAY(res->ip_dst, nvgre_encap_conf.ipv6_dst);
	}
	if (nvgre_encap_conf.select_vlan)
		nvgre_encap_conf.vlan_tci = rte_cpu_to_be_16(res->tci);
	rte_memcpy(nvgre_encap_conf.eth_src, res->eth_src.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
	rte_memcpy(nvgre_encap_conf.eth_dst, res->eth_dst.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
}

cmdline_parse_inst_t cmd_set_nvgre = {
	.f = cmd_set_nvgre_parsed,
	.data = NULL,
	.help_str = "set nvgre ip-version <ipv4|ipv6> tni <tni> ip-src"
		" <ip-src> ip-dst <ip-dst> eth-src <eth-src>"
		" eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_nvgre_set,
		(void *)&cmd_set_nvgre_nvgre,
		(void *)&cmd_set_nvgre_ip_version,
		(void *)&cmd_set_nvgre_ip_version_value,
		(void *)&cmd_set_nvgre_tni,
		(void *)&cmd_set_nvgre_tni_value,
		(void *)&cmd_set_nvgre_ip_src,
		(void *)&cmd_set_nvgre_ip_src_value,
		(void *)&cmd_set_nvgre_ip_dst,
		(void *)&cmd_set_nvgre_ip_dst_value,
		(void *)&cmd_set_nvgre_eth_src,
		(void *)&cmd_set_nvgre_eth_src_value,
		(void *)&cmd_set_nvgre_eth_dst,
		(void *)&cmd_set_nvgre_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_nvgre_with_vlan = {
	.f = cmd_set_nvgre_parsed,
	.data = NULL,
	.help_str = "set nvgre-with-vlan ip-version <ipv4|ipv6> tni <tni>"
		" ip-src <ip-src> ip-dst <ip-dst> vlan-tci <vlan-tci>"
		" eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_nvgre_set,
		(void *)&cmd_set_nvgre_nvgre_with_vlan,
		(void *)&cmd_set_nvgre_ip_version,
		(void *)&cmd_set_nvgre_ip_version_value,
		(void *)&cmd_set_nvgre_tni,
		(void *)&cmd_set_nvgre_tni_value,
		(void *)&cmd_set_nvgre_ip_src,
		(void *)&cmd_set_nvgre_ip_src_value,
		(void *)&cmd_set_nvgre_ip_dst,
		(void *)&cmd_set_nvgre_ip_dst_value,
		(void *)&cmd_set_nvgre_vlan,
		(void *)&cmd_set_nvgre_vlan_value,
		(void *)&cmd_set_nvgre_eth_src,
		(void *)&cmd_set_nvgre_eth_src_value,
		(void *)&cmd_set_nvgre_eth_dst,
		(void *)&cmd_set_nvgre_eth_dst_value,
		NULL,
	},
};

/** Set L2 encapsulation details */
struct cmd_set_l2_encap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t l2_encap;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
	uint16_t tci;
	struct rte_ether_addr eth_src;
	struct rte_ether_addr eth_dst;
};

cmdline_parse_token_string_t cmd_set_l2_encap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, set, "set");
cmdline_parse_token_string_t cmd_set_l2_encap_l2_encap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, l2_encap, "l2_encap");
cmdline_parse_token_string_t cmd_set_l2_encap_l2_encap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, l2_encap,
				 "l2_encap-with-vlan");
cmdline_parse_token_string_t cmd_set_l2_encap_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, pos_token,
				 "ip-version");
cmdline_parse_token_string_t cmd_set_l2_encap_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, ip_version,
				 "ipv4#ipv6");
cmdline_parse_token_string_t cmd_set_l2_encap_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, pos_token,
				 "vlan-tci");
cmdline_parse_token_num_t cmd_set_l2_encap_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_l2_encap_result, tci, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_l2_encap_eth_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, pos_token,
				 "eth-src");
cmdline_parse_token_etheraddr_t cmd_set_l2_encap_eth_src_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_l2_encap_result, eth_src);
cmdline_parse_token_string_t cmd_set_l2_encap_eth_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_encap_result, pos_token,
				 "eth-dst");
cmdline_parse_token_etheraddr_t cmd_set_l2_encap_eth_dst_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_l2_encap_result, eth_dst);

static void cmd_set_l2_encap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_l2_encap_result *res = parsed_result;

	if (strcmp(res->l2_encap, "l2_encap") == 0)
		l2_encap_conf.select_vlan = 0;
	else if (strcmp(res->l2_encap, "l2_encap-with-vlan") == 0)
		l2_encap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		l2_encap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		l2_encap_conf.select_ipv4 = 0;
	else
		return;
	if (l2_encap_conf.select_vlan)
		l2_encap_conf.vlan_tci = rte_cpu_to_be_16(res->tci);
	rte_memcpy(l2_encap_conf.eth_src, res->eth_src.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
	rte_memcpy(l2_encap_conf.eth_dst, res->eth_dst.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
}

cmdline_parse_inst_t cmd_set_l2_encap = {
	.f = cmd_set_l2_encap_parsed,
	.data = NULL,
	.help_str = "set l2_encap ip-version ipv4|ipv6"
		" eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_l2_encap_set,
		(void *)&cmd_set_l2_encap_l2_encap,
		(void *)&cmd_set_l2_encap_ip_version,
		(void *)&cmd_set_l2_encap_ip_version_value,
		(void *)&cmd_set_l2_encap_eth_src,
		(void *)&cmd_set_l2_encap_eth_src_value,
		(void *)&cmd_set_l2_encap_eth_dst,
		(void *)&cmd_set_l2_encap_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_l2_encap_with_vlan = {
	.f = cmd_set_l2_encap_parsed,
	.data = NULL,
	.help_str = "set l2_encap-with-vlan ip-version ipv4|ipv6"
		" vlan-tci <vlan-tci> eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_l2_encap_set,
		(void *)&cmd_set_l2_encap_l2_encap_with_vlan,
		(void *)&cmd_set_l2_encap_ip_version,
		(void *)&cmd_set_l2_encap_ip_version_value,
		(void *)&cmd_set_l2_encap_vlan,
		(void *)&cmd_set_l2_encap_vlan_value,
		(void *)&cmd_set_l2_encap_eth_src,
		(void *)&cmd_set_l2_encap_eth_src_value,
		(void *)&cmd_set_l2_encap_eth_dst,
		(void *)&cmd_set_l2_encap_eth_dst_value,
		NULL,
	},
};

/** Set L2 decapsulation details */
struct cmd_set_l2_decap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t l2_decap;
	cmdline_fixed_string_t pos_token;
	uint32_t vlan_present:1;
};

cmdline_parse_token_string_t cmd_set_l2_decap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_decap_result, set, "set");
cmdline_parse_token_string_t cmd_set_l2_decap_l2_decap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_decap_result, l2_decap,
				 "l2_decap");
cmdline_parse_token_string_t cmd_set_l2_decap_l2_decap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_l2_decap_result, l2_decap,
				 "l2_decap-with-vlan");

static void cmd_set_l2_decap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_l2_decap_result *res = parsed_result;

	if (strcmp(res->l2_decap, "l2_decap") == 0)
		l2_decap_conf.select_vlan = 0;
	else if (strcmp(res->l2_decap, "l2_decap-with-vlan") == 0)
		l2_decap_conf.select_vlan = 1;
}

cmdline_parse_inst_t cmd_set_l2_decap = {
	.f = cmd_set_l2_decap_parsed,
	.data = NULL,
	.help_str = "set l2_decap",
	.tokens = {
		(void *)&cmd_set_l2_decap_set,
		(void *)&cmd_set_l2_decap_l2_decap,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_l2_decap_with_vlan = {
	.f = cmd_set_l2_decap_parsed,
	.data = NULL,
	.help_str = "set l2_decap-with-vlan",
	.tokens = {
		(void *)&cmd_set_l2_decap_set,
		(void *)&cmd_set_l2_decap_l2_decap_with_vlan,
		NULL,
	},
};

/** Set MPLSoGRE encapsulation details */
struct cmd_set_mplsogre_encap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mplsogre;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
	uint32_t label;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t tci;
	struct rte_ether_addr eth_src;
	struct rte_ether_addr eth_dst;
};

cmdline_parse_token_string_t cmd_set_mplsogre_encap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result, set,
				 "set");
cmdline_parse_token_string_t cmd_set_mplsogre_encap_mplsogre_encap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result, mplsogre,
				 "mplsogre_encap");
cmdline_parse_token_string_t cmd_set_mplsogre_encap_mplsogre_encap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 mplsogre, "mplsogre_encap-with-vlan");
cmdline_parse_token_string_t cmd_set_mplsogre_encap_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "ip-version");
cmdline_parse_token_string_t cmd_set_mplsogre_encap_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 ip_version, "ipv4#ipv6");
cmdline_parse_token_string_t cmd_set_mplsogre_encap_label =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "label");
cmdline_parse_token_num_t cmd_set_mplsogre_encap_label_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsogre_encap_result, label,
			      RTE_UINT32);
cmdline_parse_token_string_t cmd_set_mplsogre_encap_ip_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "ip-src");
cmdline_parse_token_ipaddr_t cmd_set_mplsogre_encap_ip_src_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_mplsogre_encap_result, ip_src);
cmdline_parse_token_string_t cmd_set_mplsogre_encap_ip_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "ip-dst");
cmdline_parse_token_ipaddr_t cmd_set_mplsogre_encap_ip_dst_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_mplsogre_encap_result, ip_dst);
cmdline_parse_token_string_t cmd_set_mplsogre_encap_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "vlan-tci");
cmdline_parse_token_num_t cmd_set_mplsogre_encap_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsogre_encap_result, tci,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_set_mplsogre_encap_eth_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "eth-src");
cmdline_parse_token_etheraddr_t cmd_set_mplsogre_encap_eth_src_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				    eth_src);
cmdline_parse_token_string_t cmd_set_mplsogre_encap_eth_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				 pos_token, "eth-dst");
cmdline_parse_token_etheraddr_t cmd_set_mplsogre_encap_eth_dst_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_mplsogre_encap_result,
				    eth_dst);

static void cmd_set_mplsogre_encap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_mplsogre_encap_result *res = parsed_result;
	union {
		uint32_t mplsogre_label;
		uint8_t label[4];
	} id = {
		.mplsogre_label = rte_cpu_to_be_32(res->label<<12),
	};

	if (strcmp(res->mplsogre, "mplsogre_encap") == 0)
		mplsogre_encap_conf.select_vlan = 0;
	else if (strcmp(res->mplsogre, "mplsogre_encap-with-vlan") == 0)
		mplsogre_encap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		mplsogre_encap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		mplsogre_encap_conf.select_ipv4 = 0;
	else
		return;
	rte_memcpy(mplsogre_encap_conf.label, &id.label, 3);
	if (mplsogre_encap_conf.select_ipv4) {
		IPV4_ADDR_TO_UINT(res->ip_src, mplsogre_encap_conf.ipv4_src);
		IPV4_ADDR_TO_UINT(res->ip_dst, mplsogre_encap_conf.ipv4_dst);
	} else {
		IPV6_ADDR_TO_ARRAY(res->ip_src, mplsogre_encap_conf.ipv6_src);
		IPV6_ADDR_TO_ARRAY(res->ip_dst, mplsogre_encap_conf.ipv6_dst);
	}
	if (mplsogre_encap_conf.select_vlan)
		mplsogre_encap_conf.vlan_tci = rte_cpu_to_be_16(res->tci);
	rte_memcpy(mplsogre_encap_conf.eth_src, res->eth_src.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
	rte_memcpy(mplsogre_encap_conf.eth_dst, res->eth_dst.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
}

cmdline_parse_inst_t cmd_set_mplsogre_encap = {
	.f = cmd_set_mplsogre_encap_parsed,
	.data = NULL,
	.help_str = "set mplsogre_encap ip-version ipv4|ipv6 label <label>"
		" ip-src <ip-src> ip-dst <ip-dst> eth-src <eth-src>"
		" eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_mplsogre_encap_set,
		(void *)&cmd_set_mplsogre_encap_mplsogre_encap,
		(void *)&cmd_set_mplsogre_encap_ip_version,
		(void *)&cmd_set_mplsogre_encap_ip_version_value,
		(void *)&cmd_set_mplsogre_encap_label,
		(void *)&cmd_set_mplsogre_encap_label_value,
		(void *)&cmd_set_mplsogre_encap_ip_src,
		(void *)&cmd_set_mplsogre_encap_ip_src_value,
		(void *)&cmd_set_mplsogre_encap_ip_dst,
		(void *)&cmd_set_mplsogre_encap_ip_dst_value,
		(void *)&cmd_set_mplsogre_encap_eth_src,
		(void *)&cmd_set_mplsogre_encap_eth_src_value,
		(void *)&cmd_set_mplsogre_encap_eth_dst,
		(void *)&cmd_set_mplsogre_encap_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_mplsogre_encap_with_vlan = {
	.f = cmd_set_mplsogre_encap_parsed,
	.data = NULL,
	.help_str = "set mplsogre_encap-with-vlan ip-version ipv4|ipv6"
		" label <label> ip-src <ip-src> ip-dst <ip-dst>"
		" vlan-tci <vlan-tci> eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_mplsogre_encap_set,
		(void *)&cmd_set_mplsogre_encap_mplsogre_encap_with_vlan,
		(void *)&cmd_set_mplsogre_encap_ip_version,
		(void *)&cmd_set_mplsogre_encap_ip_version_value,
		(void *)&cmd_set_mplsogre_encap_label,
		(void *)&cmd_set_mplsogre_encap_label_value,
		(void *)&cmd_set_mplsogre_encap_ip_src,
		(void *)&cmd_set_mplsogre_encap_ip_src_value,
		(void *)&cmd_set_mplsogre_encap_ip_dst,
		(void *)&cmd_set_mplsogre_encap_ip_dst_value,
		(void *)&cmd_set_mplsogre_encap_vlan,
		(void *)&cmd_set_mplsogre_encap_vlan_value,
		(void *)&cmd_set_mplsogre_encap_eth_src,
		(void *)&cmd_set_mplsogre_encap_eth_src_value,
		(void *)&cmd_set_mplsogre_encap_eth_dst,
		(void *)&cmd_set_mplsogre_encap_eth_dst_value,
		NULL,
	},
};

/** Set MPLSoGRE decapsulation details */
struct cmd_set_mplsogre_decap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mplsogre;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
};

cmdline_parse_token_string_t cmd_set_mplsogre_decap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_decap_result, set,
				 "set");
cmdline_parse_token_string_t cmd_set_mplsogre_decap_mplsogre_decap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_decap_result, mplsogre,
				 "mplsogre_decap");
cmdline_parse_token_string_t cmd_set_mplsogre_decap_mplsogre_decap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_decap_result,
				 mplsogre, "mplsogre_decap-with-vlan");
cmdline_parse_token_string_t cmd_set_mplsogre_decap_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_decap_result,
				 pos_token, "ip-version");
cmdline_parse_token_string_t cmd_set_mplsogre_decap_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsogre_decap_result,
				 ip_version, "ipv4#ipv6");

static void cmd_set_mplsogre_decap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_mplsogre_decap_result *res = parsed_result;

	if (strcmp(res->mplsogre, "mplsogre_decap") == 0)
		mplsogre_decap_conf.select_vlan = 0;
	else if (strcmp(res->mplsogre, "mplsogre_decap-with-vlan") == 0)
		mplsogre_decap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		mplsogre_decap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		mplsogre_decap_conf.select_ipv4 = 0;
}

cmdline_parse_inst_t cmd_set_mplsogre_decap = {
	.f = cmd_set_mplsogre_decap_parsed,
	.data = NULL,
	.help_str = "set mplsogre_decap ip-version ipv4|ipv6",
	.tokens = {
		(void *)&cmd_set_mplsogre_decap_set,
		(void *)&cmd_set_mplsogre_decap_mplsogre_decap,
		(void *)&cmd_set_mplsogre_decap_ip_version,
		(void *)&cmd_set_mplsogre_decap_ip_version_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_mplsogre_decap_with_vlan = {
	.f = cmd_set_mplsogre_decap_parsed,
	.data = NULL,
	.help_str = "set mplsogre_decap-with-vlan ip-version ipv4|ipv6",
	.tokens = {
		(void *)&cmd_set_mplsogre_decap_set,
		(void *)&cmd_set_mplsogre_decap_mplsogre_decap_with_vlan,
		(void *)&cmd_set_mplsogre_decap_ip_version,
		(void *)&cmd_set_mplsogre_decap_ip_version_value,
		NULL,
	},
};

/** Set MPLSoUDP encapsulation details */
struct cmd_set_mplsoudp_encap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mplsoudp;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
	uint32_t label;
	uint16_t udp_src;
	uint16_t udp_dst;
	cmdline_ipaddr_t ip_src;
	cmdline_ipaddr_t ip_dst;
	uint16_t tci;
	struct rte_ether_addr eth_src;
	struct rte_ether_addr eth_dst;
};

cmdline_parse_token_string_t cmd_set_mplsoudp_encap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result, set,
				 "set");
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_mplsoudp_encap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result, mplsoudp,
				 "mplsoudp_encap");
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_mplsoudp_encap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 mplsoudp, "mplsoudp_encap-with-vlan");
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "ip-version");
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 ip_version, "ipv4#ipv6");
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_label =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "label");
cmdline_parse_token_num_t cmd_set_mplsoudp_encap_label_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsoudp_encap_result, label,
			      RTE_UINT32);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_udp_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "udp-src");
cmdline_parse_token_num_t cmd_set_mplsoudp_encap_udp_src_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsoudp_encap_result, udp_src,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_udp_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "udp-dst");
cmdline_parse_token_num_t cmd_set_mplsoudp_encap_udp_dst_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsoudp_encap_result, udp_dst,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_ip_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "ip-src");
cmdline_parse_token_ipaddr_t cmd_set_mplsoudp_encap_ip_src_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_mplsoudp_encap_result, ip_src);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_ip_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "ip-dst");
cmdline_parse_token_ipaddr_t cmd_set_mplsoudp_encap_ip_dst_value =
	TOKEN_IPADDR_INITIALIZER(struct cmd_set_mplsoudp_encap_result, ip_dst);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "vlan-tci");
cmdline_parse_token_num_t cmd_set_mplsoudp_encap_vlan_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_mplsoudp_encap_result, tci,
			      RTE_UINT16);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_eth_src =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "eth-src");
cmdline_parse_token_etheraddr_t cmd_set_mplsoudp_encap_eth_src_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				    eth_src);
cmdline_parse_token_string_t cmd_set_mplsoudp_encap_eth_dst =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				 pos_token, "eth-dst");
cmdline_parse_token_etheraddr_t cmd_set_mplsoudp_encap_eth_dst_value =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_set_mplsoudp_encap_result,
				    eth_dst);

static void cmd_set_mplsoudp_encap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_mplsoudp_encap_result *res = parsed_result;
	union {
		uint32_t mplsoudp_label;
		uint8_t label[4];
	} id = {
		.mplsoudp_label = rte_cpu_to_be_32(res->label<<12),
	};

	if (strcmp(res->mplsoudp, "mplsoudp_encap") == 0)
		mplsoudp_encap_conf.select_vlan = 0;
	else if (strcmp(res->mplsoudp, "mplsoudp_encap-with-vlan") == 0)
		mplsoudp_encap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		mplsoudp_encap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		mplsoudp_encap_conf.select_ipv4 = 0;
	else
		return;
	rte_memcpy(mplsoudp_encap_conf.label, &id.label, 3);
	mplsoudp_encap_conf.udp_src = rte_cpu_to_be_16(res->udp_src);
	mplsoudp_encap_conf.udp_dst = rte_cpu_to_be_16(res->udp_dst);
	if (mplsoudp_encap_conf.select_ipv4) {
		IPV4_ADDR_TO_UINT(res->ip_src, mplsoudp_encap_conf.ipv4_src);
		IPV4_ADDR_TO_UINT(res->ip_dst, mplsoudp_encap_conf.ipv4_dst);
	} else {
		IPV6_ADDR_TO_ARRAY(res->ip_src, mplsoudp_encap_conf.ipv6_src);
		IPV6_ADDR_TO_ARRAY(res->ip_dst, mplsoudp_encap_conf.ipv6_dst);
	}
	if (mplsoudp_encap_conf.select_vlan)
		mplsoudp_encap_conf.vlan_tci = rte_cpu_to_be_16(res->tci);
	rte_memcpy(mplsoudp_encap_conf.eth_src, res->eth_src.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
	rte_memcpy(mplsoudp_encap_conf.eth_dst, res->eth_dst.addr_bytes,
		   RTE_ETHER_ADDR_LEN);
}

cmdline_parse_inst_t cmd_set_mplsoudp_encap = {
	.f = cmd_set_mplsoudp_encap_parsed,
	.data = NULL,
	.help_str = "set mplsoudp_encap ip-version ipv4|ipv6 label <label>"
		" udp-src <udp-src> udp-dst <udp-dst> ip-src <ip-src>"
		" ip-dst <ip-dst> eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_mplsoudp_encap_set,
		(void *)&cmd_set_mplsoudp_encap_mplsoudp_encap,
		(void *)&cmd_set_mplsoudp_encap_ip_version,
		(void *)&cmd_set_mplsoudp_encap_ip_version_value,
		(void *)&cmd_set_mplsoudp_encap_label,
		(void *)&cmd_set_mplsoudp_encap_label_value,
		(void *)&cmd_set_mplsoudp_encap_udp_src,
		(void *)&cmd_set_mplsoudp_encap_udp_src_value,
		(void *)&cmd_set_mplsoudp_encap_udp_dst,
		(void *)&cmd_set_mplsoudp_encap_udp_dst_value,
		(void *)&cmd_set_mplsoudp_encap_ip_src,
		(void *)&cmd_set_mplsoudp_encap_ip_src_value,
		(void *)&cmd_set_mplsoudp_encap_ip_dst,
		(void *)&cmd_set_mplsoudp_encap_ip_dst_value,
		(void *)&cmd_set_mplsoudp_encap_eth_src,
		(void *)&cmd_set_mplsoudp_encap_eth_src_value,
		(void *)&cmd_set_mplsoudp_encap_eth_dst,
		(void *)&cmd_set_mplsoudp_encap_eth_dst_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_mplsoudp_encap_with_vlan = {
	.f = cmd_set_mplsoudp_encap_parsed,
	.data = NULL,
	.help_str = "set mplsoudp_encap-with-vlan ip-version ipv4|ipv6"
		" label <label> udp-src <udp-src> udp-dst <udp-dst>"
		" ip-src <ip-src> ip-dst <ip-dst> vlan-tci <vlan-tci>"
		" eth-src <eth-src> eth-dst <eth-dst>",
	.tokens = {
		(void *)&cmd_set_mplsoudp_encap_set,
		(void *)&cmd_set_mplsoudp_encap_mplsoudp_encap_with_vlan,
		(void *)&cmd_set_mplsoudp_encap_ip_version,
		(void *)&cmd_set_mplsoudp_encap_ip_version_value,
		(void *)&cmd_set_mplsoudp_encap_label,
		(void *)&cmd_set_mplsoudp_encap_label_value,
		(void *)&cmd_set_mplsoudp_encap_udp_src,
		(void *)&cmd_set_mplsoudp_encap_udp_src_value,
		(void *)&cmd_set_mplsoudp_encap_udp_dst,
		(void *)&cmd_set_mplsoudp_encap_udp_dst_value,
		(void *)&cmd_set_mplsoudp_encap_ip_src,
		(void *)&cmd_set_mplsoudp_encap_ip_src_value,
		(void *)&cmd_set_mplsoudp_encap_ip_dst,
		(void *)&cmd_set_mplsoudp_encap_ip_dst_value,
		(void *)&cmd_set_mplsoudp_encap_vlan,
		(void *)&cmd_set_mplsoudp_encap_vlan_value,
		(void *)&cmd_set_mplsoudp_encap_eth_src,
		(void *)&cmd_set_mplsoudp_encap_eth_src_value,
		(void *)&cmd_set_mplsoudp_encap_eth_dst,
		(void *)&cmd_set_mplsoudp_encap_eth_dst_value,
		NULL,
	},
};

/** Set MPLSoUDP decapsulation details */
struct cmd_set_mplsoudp_decap_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t mplsoudp;
	cmdline_fixed_string_t pos_token;
	cmdline_fixed_string_t ip_version;
	uint32_t vlan_present:1;
};

cmdline_parse_token_string_t cmd_set_mplsoudp_decap_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_decap_result, set,
				 "set");
cmdline_parse_token_string_t cmd_set_mplsoudp_decap_mplsoudp_decap =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_decap_result, mplsoudp,
				 "mplsoudp_decap");
cmdline_parse_token_string_t cmd_set_mplsoudp_decap_mplsoudp_decap_with_vlan =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_decap_result,
				 mplsoudp, "mplsoudp_decap-with-vlan");
cmdline_parse_token_string_t cmd_set_mplsoudp_decap_ip_version =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_decap_result,
				 pos_token, "ip-version");
cmdline_parse_token_string_t cmd_set_mplsoudp_decap_ip_version_value =
	TOKEN_STRING_INITIALIZER(struct cmd_set_mplsoudp_decap_result,
				 ip_version, "ipv4#ipv6");

static void cmd_set_mplsoudp_decap_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_mplsoudp_decap_result *res = parsed_result;

	if (strcmp(res->mplsoudp, "mplsoudp_decap") == 0)
		mplsoudp_decap_conf.select_vlan = 0;
	else if (strcmp(res->mplsoudp, "mplsoudp_decap-with-vlan") == 0)
		mplsoudp_decap_conf.select_vlan = 1;
	if (strcmp(res->ip_version, "ipv4") == 0)
		mplsoudp_decap_conf.select_ipv4 = 1;
	else if (strcmp(res->ip_version, "ipv6") == 0)
		mplsoudp_decap_conf.select_ipv4 = 0;
}

cmdline_parse_inst_t cmd_set_mplsoudp_decap = {
	.f = cmd_set_mplsoudp_decap_parsed,
	.data = NULL,
	.help_str = "set mplsoudp_decap ip-version ipv4|ipv6",
	.tokens = {
		(void *)&cmd_set_mplsoudp_decap_set,
		(void *)&cmd_set_mplsoudp_decap_mplsoudp_decap,
		(void *)&cmd_set_mplsoudp_decap_ip_version,
		(void *)&cmd_set_mplsoudp_decap_ip_version_value,
		NULL,
	},
};

cmdline_parse_inst_t cmd_set_mplsoudp_decap_with_vlan = {
	.f = cmd_set_mplsoudp_decap_parsed,
	.data = NULL,
	.help_str = "set mplsoudp_decap-with-vlan ip-version ipv4|ipv6",
	.tokens = {
		(void *)&cmd_set_mplsoudp_decap_set,
		(void *)&cmd_set_mplsoudp_decap_mplsoudp_decap_with_vlan,
		(void *)&cmd_set_mplsoudp_decap_ip_version,
		(void *)&cmd_set_mplsoudp_decap_ip_version_value,
		NULL,
	},
};

/** Set connection tracking object common details */
struct cmd_set_conntrack_common_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t conntrack;
	cmdline_fixed_string_t common;
	cmdline_fixed_string_t peer;
	cmdline_fixed_string_t is_orig;
	cmdline_fixed_string_t enable;
	cmdline_fixed_string_t live;
	cmdline_fixed_string_t sack;
	cmdline_fixed_string_t cack;
	cmdline_fixed_string_t last_dir;
	cmdline_fixed_string_t liberal;
	cmdline_fixed_string_t state;
	cmdline_fixed_string_t max_ack_win;
	cmdline_fixed_string_t retrans;
	cmdline_fixed_string_t last_win;
	cmdline_fixed_string_t last_seq;
	cmdline_fixed_string_t last_ack;
	cmdline_fixed_string_t last_end;
	cmdline_fixed_string_t last_index;
	uint8_t stat;
	uint8_t factor;
	uint16_t peer_port;
	uint32_t is_original;
	uint32_t en;
	uint32_t is_live;
	uint32_t s_ack;
	uint32_t c_ack;
	uint32_t ld;
	uint32_t lb;
	uint8_t re_num;
	uint8_t li;
	uint16_t lw;
	uint32_t ls;
	uint32_t la;
	uint32_t le;
};

cmdline_parse_token_string_t cmd_set_conntrack_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 set, "set");
cmdline_parse_token_string_t cmd_set_conntrack_conntrack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 conntrack, "conntrack");
cmdline_parse_token_string_t cmd_set_conntrack_common_com =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 common, "com");
cmdline_parse_token_string_t cmd_set_conntrack_common_peer =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 peer, "peer");
cmdline_parse_token_num_t cmd_set_conntrack_common_peer_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      peer_port, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_conntrack_common_is_orig =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 is_orig, "is_orig");
cmdline_parse_token_num_t cmd_set_conntrack_common_is_orig_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      is_original, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_enable =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 enable, "enable");
cmdline_parse_token_num_t cmd_set_conntrack_common_enable_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      en, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_live =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 live, "live");
cmdline_parse_token_num_t cmd_set_conntrack_common_live_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      is_live, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_sack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 sack, "sack");
cmdline_parse_token_num_t cmd_set_conntrack_common_sack_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      s_ack, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_cack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 cack, "cack");
cmdline_parse_token_num_t cmd_set_conntrack_common_cack_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      c_ack, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_dir =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_dir, "last_dir");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_dir_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      ld, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_liberal =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 liberal, "liberal");
cmdline_parse_token_num_t cmd_set_conntrack_common_liberal_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      lb, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_state =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 state, "state");
cmdline_parse_token_num_t cmd_set_conntrack_common_state_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      stat, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_conntrack_common_max_ackwin =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 max_ack_win, "max_ack_win");
cmdline_parse_token_num_t cmd_set_conntrack_common_max_ackwin_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      factor, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_conntrack_common_retrans =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 retrans, "r_lim");
cmdline_parse_token_num_t cmd_set_conntrack_common_retrans_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      re_num, RTE_UINT8);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_win =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_win, "last_win");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_win_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      lw, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_seq =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_seq, "last_seq");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_seq_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      ls, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_ack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_ack, "last_ack");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_ack_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      la, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_end =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_end, "last_end");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_end_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      le, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_common_last_index =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_common_result,
				 last_index, "last_index");
cmdline_parse_token_num_t cmd_set_conntrack_common_last_index_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_common_result,
			      li, RTE_UINT8);

static void cmd_set_conntrack_common_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_conntrack_common_result *res = parsed_result;

	/* No need to swap to big endian. */
	conntrack_context.peer_port = res->peer_port;
	conntrack_context.is_original_dir = res->is_original;
	conntrack_context.enable = res->en;
	conntrack_context.live_connection = res->is_live;
	conntrack_context.selective_ack = res->s_ack;
	conntrack_context.challenge_ack_passed = res->c_ack;
	conntrack_context.last_direction = res->ld;
	conntrack_context.liberal_mode = res->lb;
	conntrack_context.state = (enum rte_flow_conntrack_state)res->stat;
	conntrack_context.max_ack_window = res->factor;
	conntrack_context.retransmission_limit = res->re_num;
	conntrack_context.last_window = res->lw;
	conntrack_context.last_index =
		(enum rte_flow_conntrack_tcp_last_index)res->li;
	conntrack_context.last_seq = res->ls;
	conntrack_context.last_ack = res->la;
	conntrack_context.last_end = res->le;
}

cmdline_parse_inst_t cmd_set_conntrack_common = {
	.f = cmd_set_conntrack_common_parsed,
	.data = NULL,
	.help_str = "set conntrack com peer <port_id> is_orig <dir> enable <en>"
		" live <ack_seen> sack <en> cack <passed> last_dir <dir>"
		" liberal <en> state <s> max_ack_win <factor> r_lim <num>"
		" last_win <win> last_seq <seq> last_ack <ack> last_end <end>"
		" last_index <flag>",
	.tokens = {
		(void *)&cmd_set_conntrack_set,
		(void *)&cmd_set_conntrack_conntrack,
		(void *)&cmd_set_conntrack_common_com,
		(void *)&cmd_set_conntrack_common_peer,
		(void *)&cmd_set_conntrack_common_peer_value,
		(void *)&cmd_set_conntrack_common_is_orig,
		(void *)&cmd_set_conntrack_common_is_orig_value,
		(void *)&cmd_set_conntrack_common_enable,
		(void *)&cmd_set_conntrack_common_enable_value,
		(void *)&cmd_set_conntrack_common_live,
		(void *)&cmd_set_conntrack_common_live_value,
		(void *)&cmd_set_conntrack_common_sack,
		(void *)&cmd_set_conntrack_common_sack_value,
		(void *)&cmd_set_conntrack_common_cack,
		(void *)&cmd_set_conntrack_common_cack_value,
		(void *)&cmd_set_conntrack_common_last_dir,
		(void *)&cmd_set_conntrack_common_last_dir_value,
		(void *)&cmd_set_conntrack_common_liberal,
		(void *)&cmd_set_conntrack_common_liberal_value,
		(void *)&cmd_set_conntrack_common_state,
		(void *)&cmd_set_conntrack_common_state_value,
		(void *)&cmd_set_conntrack_common_max_ackwin,
		(void *)&cmd_set_conntrack_common_max_ackwin_value,
		(void *)&cmd_set_conntrack_common_retrans,
		(void *)&cmd_set_conntrack_common_retrans_value,
		(void *)&cmd_set_conntrack_common_last_win,
		(void *)&cmd_set_conntrack_common_last_win_value,
		(void *)&cmd_set_conntrack_common_last_seq,
		(void *)&cmd_set_conntrack_common_last_seq_value,
		(void *)&cmd_set_conntrack_common_last_ack,
		(void *)&cmd_set_conntrack_common_last_ack_value,
		(void *)&cmd_set_conntrack_common_last_end,
		(void *)&cmd_set_conntrack_common_last_end_value,
		(void *)&cmd_set_conntrack_common_last_index,
		(void *)&cmd_set_conntrack_common_last_index_value,
		NULL,
	},
};

/** Set connection tracking object both directions' details */
struct cmd_set_conntrack_dir_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t conntrack;
	cmdline_fixed_string_t dir;
	cmdline_fixed_string_t scale;
	cmdline_fixed_string_t fin;
	cmdline_fixed_string_t ack_seen;
	cmdline_fixed_string_t unack;
	cmdline_fixed_string_t sent_end;
	cmdline_fixed_string_t reply_end;
	cmdline_fixed_string_t max_win;
	cmdline_fixed_string_t max_ack;
	uint32_t factor;
	uint32_t f;
	uint32_t as;
	uint32_t un;
	uint32_t se;
	uint32_t re;
	uint32_t mw;
	uint32_t ma;
};

cmdline_parse_token_string_t cmd_set_conntrack_dir_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 set, "set");
cmdline_parse_token_string_t cmd_set_conntrack_dir_conntrack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 conntrack, "conntrack");
cmdline_parse_token_string_t cmd_set_conntrack_dir_dir =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 dir, "orig#rply");
cmdline_parse_token_string_t cmd_set_conntrack_dir_scale =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 scale, "scale");
cmdline_parse_token_num_t cmd_set_conntrack_dir_scale_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      factor, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_fin =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 fin, "fin");
cmdline_parse_token_num_t cmd_set_conntrack_dir_fin_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      f, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_ack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 ack_seen, "acked");
cmdline_parse_token_num_t cmd_set_conntrack_dir_ack_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      as, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_unack_data =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 unack, "unack_data");
cmdline_parse_token_num_t cmd_set_conntrack_dir_unack_data_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      un, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_sent_end =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 sent_end, "sent_end");
cmdline_parse_token_num_t cmd_set_conntrack_dir_sent_end_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      se, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_reply_end =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 reply_end, "reply_end");
cmdline_parse_token_num_t cmd_set_conntrack_dir_reply_end_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      re, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_max_win =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 max_win, "max_win");
cmdline_parse_token_num_t cmd_set_conntrack_dir_max_win_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      mw, RTE_UINT32);
cmdline_parse_token_string_t cmd_set_conntrack_dir_max_ack =
	TOKEN_STRING_INITIALIZER(struct cmd_set_conntrack_dir_result,
				 max_ack, "max_ack");
cmdline_parse_token_num_t cmd_set_conntrack_dir_max_ack_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_conntrack_dir_result,
			      ma, RTE_UINT32);

static void cmd_set_conntrack_dir_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_conntrack_dir_result *res = parsed_result;
	struct rte_flow_tcp_dir_param *dir = NULL;

	if (strcmp(res->dir, "orig") == 0)
		dir = &conntrack_context.original_dir;
	else if (strcmp(res->dir, "rply") == 0)
		dir = &conntrack_context.reply_dir;
	else
		return;
	dir->scale = res->factor;
	dir->close_initiated = res->f;
	dir->last_ack_seen = res->as;
	dir->data_unacked = res->un;
	dir->sent_end = res->se;
	dir->reply_end = res->re;
	dir->max_ack = res->ma;
	dir->max_win = res->mw;
}

cmdline_parse_inst_t cmd_set_conntrack_dir = {
	.f = cmd_set_conntrack_dir_parsed,
	.data = NULL,
	.help_str = "set conntrack orig|rply scale <factor> fin <sent>"
		    " acked <seen> unack_data <unack> sent_end <sent>"
		    " reply_end <reply> max_win <win> max_ack <ack>",
	.tokens = {
		(void *)&cmd_set_conntrack_set,
		(void *)&cmd_set_conntrack_conntrack,
		(void *)&cmd_set_conntrack_dir_dir,
		(void *)&cmd_set_conntrack_dir_scale,
		(void *)&cmd_set_conntrack_dir_scale_value,
		(void *)&cmd_set_conntrack_dir_fin,
		(void *)&cmd_set_conntrack_dir_fin_value,
		(void *)&cmd_set_conntrack_dir_ack,
		(void *)&cmd_set_conntrack_dir_ack_value,
		(void *)&cmd_set_conntrack_dir_unack_data,
		(void *)&cmd_set_conntrack_dir_unack_data_value,
		(void *)&cmd_set_conntrack_dir_sent_end,
		(void *)&cmd_set_conntrack_dir_sent_end_value,
		(void *)&cmd_set_conntrack_dir_reply_end,
		(void *)&cmd_set_conntrack_dir_reply_end_value,
		(void *)&cmd_set_conntrack_dir_max_win,
		(void *)&cmd_set_conntrack_dir_max_win_value,
		(void *)&cmd_set_conntrack_dir_max_ack,
		(void *)&cmd_set_conntrack_dir_max_ack_value,
		NULL,
	},
};

/* Strict link priority scheduling mode setting */
static void
cmd_strict_link_prio_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_vf_tc_bw_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_set_tc_strict_prio(res->port_id, res->tc_map);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid tc_bitmap 0x%x\n", res->tc_map);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_strict_link_prio = {
	.f = cmd_strict_link_prio_parsed,
	.data = NULL,
	.help_str = "set tx strict-link-priority <port_id> <tc_bitmap>",
	.tokens = {
		(void *)&cmd_vf_tc_bw_set,
		(void *)&cmd_vf_tc_bw_tx,
		(void *)&cmd_vf_tc_bw_strict_link_prio,
		(void *)&cmd_vf_tc_bw_port_id,
		(void *)&cmd_vf_tc_bw_tc_map,
		NULL,
	},
};

/* Load dynamic device personalization*/
struct cmd_ddp_add_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t add;
	portid_t port_id;
	char filepath[];
};

cmdline_parse_token_string_t cmd_ddp_add_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_add_add =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, add, "add");
cmdline_parse_token_num_t cmd_ddp_add_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_add_result, port_id,
		RTE_UINT16);
cmdline_parse_token_string_t cmd_ddp_add_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_add_result, filepath, NULL);

static void
cmd_ddp_add_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_add_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	char *filepath;
	char *file_fld[2];
	int file_num;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	filepath = strdup(res->filepath);
	if (filepath == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		return;
	}
	file_num = rte_strsplit(filepath, strlen(filepath), file_fld, 2, ',');

	buff = open_file(file_fld[0], &size);
	if (!buff) {
		free((void *)filepath);
		return;
	}

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_process_ddp_package(res->port_id,
					       buff, size,
					       RTE_PMD_I40E_PKG_OP_WR_ADD);
#endif

	if (ret == -EEXIST)
		fprintf(stderr, "Profile has already existed.\n");
	else if (ret < 0)
		fprintf(stderr, "Failed to load profile.\n");
	else if (file_num == 2)
		save_file(file_fld[1], buff, size);

	close_file(buff);
	free((void *)filepath);
}

cmdline_parse_inst_t cmd_ddp_add = {
	.f = cmd_ddp_add_parsed,
	.data = NULL,
	.help_str = "ddp add <port_id> <profile_path[,backup_profile_path]>",
	.tokens = {
		(void *)&cmd_ddp_add_ddp,
		(void *)&cmd_ddp_add_add,
		(void *)&cmd_ddp_add_port_id,
		(void *)&cmd_ddp_add_filepath,
		NULL,
	},
};

/* Delete dynamic device personalization*/
struct cmd_ddp_del_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t del;
	portid_t port_id;
	char filepath[];
};

cmdline_parse_token_string_t cmd_ddp_del_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_del_del =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, del, "del");
cmdline_parse_token_num_t cmd_ddp_del_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_del_result, port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_ddp_del_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_del_result, filepath, NULL);

static void
cmd_ddp_del_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_del_result *res = parsed_result;
	uint8_t *buff;
	uint32_t size;
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

	buff = open_file(res->filepath, &size);
	if (!buff)
		return;

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_process_ddp_package(res->port_id,
					       buff, size,
					       RTE_PMD_I40E_PKG_OP_WR_DEL);
#endif

	if (ret == -EACCES)
		fprintf(stderr, "Profile does not exist.\n");
	else if (ret < 0)
		fprintf(stderr, "Failed to delete profile.\n");

	close_file(buff);
}

cmdline_parse_inst_t cmd_ddp_del = {
	.f = cmd_ddp_del_parsed,
	.data = NULL,
	.help_str = "ddp del <port_id> <backup_profile_path>",
	.tokens = {
		(void *)&cmd_ddp_del_ddp,
		(void *)&cmd_ddp_del_del,
		(void *)&cmd_ddp_del_port_id,
		(void *)&cmd_ddp_del_filepath,
		NULL,
	},
};

/* Get dynamic device personalization profile info */
struct cmd_ddp_info_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t get;
	cmdline_fixed_string_t info;
	char filepath[];
};

cmdline_parse_token_string_t cmd_ddp_info_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_info_get =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, get, "get");
cmdline_parse_token_string_t cmd_ddp_info_info =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, info, "info");
cmdline_parse_token_string_t cmd_ddp_info_filepath =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_info_result, filepath, NULL);

static void
cmd_ddp_info_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ddp_info_result *res = parsed_result;
	uint8_t *pkg;
	uint32_t pkg_size;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	uint32_t i, j, n;
	uint8_t *buff;
	uint32_t buff_size = 0;
	struct rte_pmd_i40e_profile_info info;
	uint32_t dev_num = 0;
	struct rte_pmd_i40e_ddp_device_id *devs;
	uint32_t proto_num = 0;
	struct rte_pmd_i40e_proto_info *proto = NULL;
	uint32_t pctype_num = 0;
	struct rte_pmd_i40e_ptype_info *pctype;
	uint32_t ptype_num = 0;
	struct rte_pmd_i40e_ptype_info *ptype;
	uint8_t proto_id;

#endif

	pkg = open_file(res->filepath, &pkg_size);
	if (!pkg)
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&info, sizeof(info),
				RTE_PMD_I40E_PKG_INFO_GLOBAL_HEADER);
	if (!ret) {
		printf("Global Track id:       0x%x\n", info.track_id);
		printf("Global Version:        %d.%d.%d.%d\n",
			info.version.major,
			info.version.minor,
			info.version.update,
			info.version.draft);
		printf("Global Package name:   %s\n\n", info.name);
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&info, sizeof(info),
				RTE_PMD_I40E_PKG_INFO_HEADER);
	if (!ret) {
		printf("i40e Profile Track id: 0x%x\n", info.track_id);
		printf("i40e Profile Version:  %d.%d.%d.%d\n",
			info.version.major,
			info.version.minor,
			info.version.update,
			info.version.draft);
		printf("i40e Profile name:     %s\n\n", info.name);
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&buff_size, sizeof(buff_size),
				RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES_SIZE);
	if (!ret && buff_size) {
		buff = (uint8_t *)malloc(buff_size);
		if (buff) {
			ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
						buff, buff_size,
						RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES);
			if (!ret)
				printf("Package Notes:\n%s\n\n", buff);
			free(buff);
		}
	}

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
				(uint8_t *)&dev_num, sizeof(dev_num),
				RTE_PMD_I40E_PKG_INFO_DEVID_NUM);
	if (!ret && dev_num) {
		buff_size = dev_num * sizeof(struct rte_pmd_i40e_ddp_device_id);
		devs = (struct rte_pmd_i40e_ddp_device_id *)malloc(buff_size);
		if (devs) {
			ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
						(uint8_t *)devs, buff_size,
						RTE_PMD_I40E_PKG_INFO_DEVID_LIST);
			if (!ret) {
				printf("List of supported devices:\n");
				for (i = 0; i < dev_num; i++) {
					printf("  %04X:%04X %04X:%04X\n",
						devs[i].vendor_dev_id >> 16,
						devs[i].vendor_dev_id & 0xFFFF,
						devs[i].sub_vendor_dev_id >> 16,
						devs[i].sub_vendor_dev_id & 0xFFFF);
				}
				printf("\n");
			}
			free(devs);
		}
	}

	/* get information about protocols and packet types */
	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
		(uint8_t *)&proto_num, sizeof(proto_num),
		RTE_PMD_I40E_PKG_INFO_PROTOCOL_NUM);
	if (ret || !proto_num)
		goto no_print_return;

	buff_size = proto_num * sizeof(struct rte_pmd_i40e_proto_info);
	proto = (struct rte_pmd_i40e_proto_info *)malloc(buff_size);
	if (!proto)
		goto no_print_return;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)proto,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PROTOCOL_LIST);
	if (!ret) {
		printf("List of used protocols:\n");
		for (i = 0; i < proto_num; i++)
			printf("  %2u: %s\n", proto[i].proto_id,
			       proto[i].name);
		printf("\n");
	}
	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size,
		(uint8_t *)&pctype_num, sizeof(pctype_num),
		RTE_PMD_I40E_PKG_INFO_PCTYPE_NUM);
	if (ret || !pctype_num)
		goto no_print_pctypes;

	buff_size = pctype_num * sizeof(struct rte_pmd_i40e_ptype_info);
	pctype = (struct rte_pmd_i40e_ptype_info *)malloc(buff_size);
	if (!pctype)
		goto no_print_pctypes;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)pctype,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PCTYPE_LIST);
	if (ret) {
		free(pctype);
		goto no_print_pctypes;
	}

	printf("List of defined packet classification types:\n");
	for (i = 0; i < pctype_num; i++) {
		printf("  %2u:", pctype[i].ptype_id);
		for (j = 0; j < RTE_PMD_I40E_PROTO_NUM; j++) {
			proto_id = pctype[i].protocols[j];
			if (proto_id != RTE_PMD_I40E_PROTO_UNUSED) {
				for (n = 0; n < proto_num; n++) {
					if (proto[n].proto_id == proto_id) {
						printf(" %s", proto[n].name);
						break;
					}
				}
			}
		}
		printf("\n");
	}
	printf("\n");
	free(pctype);

no_print_pctypes:

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)&ptype_num,
					sizeof(ptype_num),
					RTE_PMD_I40E_PKG_INFO_PTYPE_NUM);
	if (ret || !ptype_num)
		goto no_print_return;

	buff_size = ptype_num * sizeof(struct rte_pmd_i40e_ptype_info);
	ptype = (struct rte_pmd_i40e_ptype_info *)malloc(buff_size);
	if (!ptype)
		goto no_print_return;

	ret = rte_pmd_i40e_get_ddp_info(pkg, pkg_size, (uint8_t *)ptype,
					buff_size,
					RTE_PMD_I40E_PKG_INFO_PTYPE_LIST);
	if (ret) {
		free(ptype);
		goto no_print_return;
	}
	printf("List of defined packet types:\n");
	for (i = 0; i < ptype_num; i++) {
		printf("  %2u:", ptype[i].ptype_id);
		for (j = 0; j < RTE_PMD_I40E_PROTO_NUM; j++) {
			proto_id = ptype[i].protocols[j];
			if (proto_id != RTE_PMD_I40E_PROTO_UNUSED) {
				for (n = 0; n < proto_num; n++) {
					if (proto[n].proto_id == proto_id) {
						printf(" %s", proto[n].name);
						break;
					}
				}
			}
		}
		printf("\n");
	}
	free(ptype);
	printf("\n");

	ret = 0;
no_print_return:
	if (proto)
		free(proto);
#endif
	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported in PMD\n");
	close_file(pkg);
}

cmdline_parse_inst_t cmd_ddp_get_info = {
	.f = cmd_ddp_info_parsed,
	.data = NULL,
	.help_str = "ddp get info <profile_path>",
	.tokens = {
		(void *)&cmd_ddp_info_ddp,
		(void *)&cmd_ddp_info_get,
		(void *)&cmd_ddp_info_info,
		(void *)&cmd_ddp_info_filepath,
		NULL,
	},
};

/* Get dynamic device personalization profile info list*/
#define PROFILE_INFO_SIZE 48
#define MAX_PROFILE_NUM 16

struct cmd_ddp_get_list_result {
	cmdline_fixed_string_t ddp;
	cmdline_fixed_string_t get;
	cmdline_fixed_string_t list;
	portid_t port_id;
};

cmdline_parse_token_string_t cmd_ddp_get_list_ddp =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, ddp, "ddp");
cmdline_parse_token_string_t cmd_ddp_get_list_get =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, get, "get");
cmdline_parse_token_string_t cmd_ddp_get_list_list =
	TOKEN_STRING_INITIALIZER(struct cmd_ddp_get_list_result, list, "list");
cmdline_parse_token_num_t cmd_ddp_get_list_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ddp_get_list_result, port_id,
		RTE_UINT16);

static void
cmd_ddp_get_list_parsed(
	__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
#ifdef RTE_NET_I40E
	struct cmd_ddp_get_list_result *res = parsed_result;
	struct rte_pmd_i40e_profile_list *p_list;
	struct rte_pmd_i40e_profile_info *p_info;
	uint32_t p_num;
	uint32_t size;
	uint32_t i;
#endif
	int ret = -ENOTSUP;

#ifdef RTE_NET_I40E
	size = PROFILE_INFO_SIZE * MAX_PROFILE_NUM + 4;
	p_list = (struct rte_pmd_i40e_profile_list *)malloc(size);
	if (!p_list) {
		fprintf(stderr, "%s: Failed to malloc buffer\n", __func__);
		return;
	}

	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_get_ddp_list(res->port_id,
						(uint8_t *)p_list, size);

	if (!ret) {
		p_num = p_list->p_count;
		printf("Profile number is: %d\n\n", p_num);

		for (i = 0; i < p_num; i++) {
			p_info = &p_list->p_info[i];
			printf("Profile %d:\n", i);
			printf("Track id:     0x%x\n", p_info->track_id);
			printf("Version:      %d.%d.%d.%d\n",
			       p_info->version.major,
			       p_info->version.minor,
			       p_info->version.update,
			       p_info->version.draft);
			printf("Profile name: %s\n\n", p_info->name);
		}
	}

	free(p_list);
#endif

	if (ret < 0)
		fprintf(stderr, "Failed to get ddp list\n");
}

cmdline_parse_inst_t cmd_ddp_get_list = {
	.f = cmd_ddp_get_list_parsed,
	.data = NULL,
	.help_str = "ddp get list <port_id>",
	.tokens = {
		(void *)&cmd_ddp_get_list_ddp,
		(void *)&cmd_ddp_get_list_get,
		(void *)&cmd_ddp_get_list_list,
		(void *)&cmd_ddp_get_list_port_id,
		NULL,
	},
};

/* Configure input set */
struct cmd_cfg_input_set_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cfg;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	uint8_t pctype_id;
	cmdline_fixed_string_t inset_type;
	cmdline_fixed_string_t opt;
	cmdline_fixed_string_t field;
	uint8_t field_idx;
};

static void
cmd_cfg_input_set_parsed(
	__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
#ifdef RTE_NET_I40E
	struct cmd_cfg_input_set_result *res = parsed_result;
	enum rte_pmd_i40e_inset_type inset_type = INSET_NONE;
	struct rte_pmd_i40e_inset inset;
#endif
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

#ifdef RTE_NET_I40E
	if (!strcmp(res->inset_type, "hash_inset"))
		inset_type = INSET_HASH;
	else if (!strcmp(res->inset_type, "fdir_inset"))
		inset_type = INSET_FDIR;
	else if (!strcmp(res->inset_type, "fdir_flx_inset"))
		inset_type = INSET_FDIR_FLX;
	ret = rte_pmd_i40e_inset_get(res->port_id, res->pctype_id,
				     &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to get input set.\n");
		return;
	}

	if (!strcmp(res->opt, "get")) {
		ret = rte_pmd_i40e_inset_field_get(inset.inset,
						   res->field_idx);
		if (ret)
			printf("Field index %d is enabled.\n", res->field_idx);
		else
			printf("Field index %d is disabled.\n", res->field_idx);
		return;
	} else if (!strcmp(res->opt, "set"))
		ret = rte_pmd_i40e_inset_field_set(&inset.inset,
						   res->field_idx);
	else if (!strcmp(res->opt, "clear"))
		ret = rte_pmd_i40e_inset_field_clear(&inset.inset,
						     res->field_idx);
	if (ret) {
		fprintf(stderr, "Failed to configure input set field.\n");
		return;
	}

	ret = rte_pmd_i40e_inset_set(res->port_id, res->pctype_id,
				     &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to set input set.\n");
		return;
	}
#endif

	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported\n");
}

cmdline_parse_token_string_t cmd_cfg_input_set_port =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 port, "port");
cmdline_parse_token_string_t cmd_cfg_input_set_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 cfg, "config");
cmdline_parse_token_num_t cmd_cfg_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_cfg_input_set_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 pctype, "pctype");
cmdline_parse_token_num_t cmd_cfg_input_set_pctype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
			      pctype_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_cfg_input_set_inset_type =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 inset_type,
				 "hash_inset#fdir_inset#fdir_flx_inset");
cmdline_parse_token_string_t cmd_cfg_input_set_opt =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 opt, "get#set#clear");
cmdline_parse_token_string_t cmd_cfg_input_set_field =
	TOKEN_STRING_INITIALIZER(struct cmd_cfg_input_set_result,
				 field, "field");
cmdline_parse_token_num_t cmd_cfg_input_set_field_idx =
	TOKEN_NUM_INITIALIZER(struct cmd_cfg_input_set_result,
			      field_idx, RTE_UINT8);

cmdline_parse_inst_t cmd_cfg_input_set = {
	.f = cmd_cfg_input_set_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype <pctype_id> hash_inset|"
		    "fdir_inset|fdir_flx_inset get|set|clear field <field_idx>",
	.tokens = {
		(void *)&cmd_cfg_input_set_port,
		(void *)&cmd_cfg_input_set_cfg,
		(void *)&cmd_cfg_input_set_port_id,
		(void *)&cmd_cfg_input_set_pctype,
		(void *)&cmd_cfg_input_set_pctype_id,
		(void *)&cmd_cfg_input_set_inset_type,
		(void *)&cmd_cfg_input_set_opt,
		(void *)&cmd_cfg_input_set_field,
		(void *)&cmd_cfg_input_set_field_idx,
		NULL,
	},
};

/* Clear input set */
struct cmd_clear_input_set_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t cfg;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	uint8_t pctype_id;
	cmdline_fixed_string_t inset_type;
	cmdline_fixed_string_t clear;
	cmdline_fixed_string_t all;
};

static void
cmd_clear_input_set_parsed(
	__rte_unused void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
#ifdef RTE_NET_I40E
	struct cmd_clear_input_set_result *res = parsed_result;
	enum rte_pmd_i40e_inset_type inset_type = INSET_NONE;
	struct rte_pmd_i40e_inset inset;
#endif
	int ret = -ENOTSUP;

	if (!all_ports_stopped()) {
		fprintf(stderr, "Please stop all ports first\n");
		return;
	}

#ifdef RTE_NET_I40E
	if (!strcmp(res->inset_type, "hash_inset"))
		inset_type = INSET_HASH;
	else if (!strcmp(res->inset_type, "fdir_inset"))
		inset_type = INSET_FDIR;
	else if (!strcmp(res->inset_type, "fdir_flx_inset"))
		inset_type = INSET_FDIR_FLX;

	memset(&inset, 0, sizeof(inset));

	ret = rte_pmd_i40e_inset_set(res->port_id, res->pctype_id,
				     &inset, inset_type);
	if (ret) {
		fprintf(stderr, "Failed to clear input set.\n");
		return;
	}

#endif

	if (ret == -ENOTSUP)
		fprintf(stderr, "Function not supported\n");
}

cmdline_parse_token_string_t cmd_clear_input_set_port =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 port, "port");
cmdline_parse_token_string_t cmd_clear_input_set_cfg =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 cfg, "config");
cmdline_parse_token_num_t cmd_clear_input_set_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_clear_input_set_result,
			      port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_clear_input_set_pctype =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 pctype, "pctype");
cmdline_parse_token_num_t cmd_clear_input_set_pctype_id =
	TOKEN_NUM_INITIALIZER(struct cmd_clear_input_set_result,
			      pctype_id, RTE_UINT8);
cmdline_parse_token_string_t cmd_clear_input_set_inset_type =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 inset_type,
				 "hash_inset#fdir_inset#fdir_flx_inset");
cmdline_parse_token_string_t cmd_clear_input_set_clear =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 clear, "clear");
cmdline_parse_token_string_t cmd_clear_input_set_all =
	TOKEN_STRING_INITIALIZER(struct cmd_clear_input_set_result,
				 all, "all");

cmdline_parse_inst_t cmd_clear_input_set = {
	.f = cmd_clear_input_set_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype <pctype_id> hash_inset|"
		    "fdir_inset|fdir_flx_inset clear all",
	.tokens = {
		(void *)&cmd_clear_input_set_port,
		(void *)&cmd_clear_input_set_cfg,
		(void *)&cmd_clear_input_set_port_id,
		(void *)&cmd_clear_input_set_pctype,
		(void *)&cmd_clear_input_set_pctype_id,
		(void *)&cmd_clear_input_set_inset_type,
		(void *)&cmd_clear_input_set_clear,
		(void *)&cmd_clear_input_set_all,
		NULL,
	},
};

/* show vf stats */

/* Common result structure for show vf stats */
struct cmd_show_vf_stats_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t stats;
	portid_t port_id;
	uint16_t vf_id;
};

/* Common CLI fields show vf stats*/
cmdline_parse_token_string_t cmd_show_vf_stats_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_vf_stats_result,
		 show, "show");
cmdline_parse_token_string_t cmd_show_vf_stats_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_vf_stats_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_show_vf_stats_stats =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_vf_stats_result,
		 stats, "stats");
cmdline_parse_token_num_t cmd_show_vf_stats_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_vf_stats_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_show_vf_stats_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_vf_stats_result,
		 vf_id, RTE_UINT16);

static void
cmd_show_vf_stats_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_show_vf_stats_result *res = parsed_result;
	struct rte_eth_stats stats;
	int ret = -ENOTSUP;
	static const char *nic_stats_border = "########################";

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	memset(&stats, 0, sizeof(stats));

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_get_vf_stats(res->port_id,
						res->vf_id,
						&stats);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_get_vf_stats(res->port_id,
						res->vf_id,
						&stats);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}

	printf("\n  %s NIC statistics for port %-2d vf %-2d %s\n",
		nic_stats_border, res->port_id, res->vf_id, nic_stats_border);

	printf("  RX-packets: %-10"PRIu64" RX-missed: %-10"PRIu64" RX-bytes:  "
	       "%-"PRIu64"\n",
	       stats.ipackets, stats.imissed, stats.ibytes);
	printf("  RX-errors: %-"PRIu64"\n", stats.ierrors);
	printf("  RX-nombuf:  %-10"PRIu64"\n",
	       stats.rx_nombuf);
	printf("  TX-packets: %-10"PRIu64" TX-errors: %-10"PRIu64" TX-bytes:  "
	       "%-"PRIu64"\n",
	       stats.opackets, stats.oerrors, stats.obytes);

	printf("  %s############################%s\n",
			       nic_stats_border, nic_stats_border);
}

cmdline_parse_inst_t cmd_show_vf_stats = {
	.f = cmd_show_vf_stats_parsed,
	.data = NULL,
	.help_str = "show vf stats <port_id> <vf_id>",
	.tokens = {
		(void *)&cmd_show_vf_stats_show,
		(void *)&cmd_show_vf_stats_vf,
		(void *)&cmd_show_vf_stats_stats,
		(void *)&cmd_show_vf_stats_port_id,
		(void *)&cmd_show_vf_stats_vf_id,
		NULL,
	},
};

/* clear vf stats */

/* Common result structure for clear vf stats */
struct cmd_clear_vf_stats_result {
	cmdline_fixed_string_t clear;
	cmdline_fixed_string_t vf;
	cmdline_fixed_string_t stats;
	portid_t port_id;
	uint16_t vf_id;
};

/* Common CLI fields clear vf stats*/
cmdline_parse_token_string_t cmd_clear_vf_stats_clear =
	TOKEN_STRING_INITIALIZER
		(struct cmd_clear_vf_stats_result,
		 clear, "clear");
cmdline_parse_token_string_t cmd_clear_vf_stats_vf =
	TOKEN_STRING_INITIALIZER
		(struct cmd_clear_vf_stats_result,
		 vf, "vf");
cmdline_parse_token_string_t cmd_clear_vf_stats_stats =
	TOKEN_STRING_INITIALIZER
		(struct cmd_clear_vf_stats_result,
		 stats, "stats");
cmdline_parse_token_num_t cmd_clear_vf_stats_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_clear_vf_stats_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_clear_vf_stats_vf_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_clear_vf_stats_result,
		 vf_id, RTE_UINT16);

static void
cmd_clear_vf_stats_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_clear_vf_stats_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP)
		ret = rte_pmd_i40e_reset_vf_stats(res->port_id,
						  res->vf_id);
#endif
#ifdef RTE_NET_BNXT
	if (ret == -ENOTSUP)
		ret = rte_pmd_bnxt_reset_vf_stats(res->port_id,
						  res->vf_id);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid vf_id %d\n", res->vf_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_clear_vf_stats = {
	.f = cmd_clear_vf_stats_parsed,
	.data = NULL,
	.help_str = "clear vf stats <port_id> <vf_id>",
	.tokens = {
		(void *)&cmd_clear_vf_stats_clear,
		(void *)&cmd_clear_vf_stats_vf,
		(void *)&cmd_clear_vf_stats_stats,
		(void *)&cmd_clear_vf_stats_port_id,
		(void *)&cmd_clear_vf_stats_vf_id,
		NULL,
	},
};

/* port config pctype mapping reset */

/* Common result structure for port config pctype mapping reset */
struct cmd_pctype_mapping_reset_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
};

/* Common CLI fields for port config pctype mapping reset*/
cmdline_parse_token_string_t cmd_pctype_mapping_reset_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 port, "port");
cmdline_parse_token_string_t cmd_pctype_mapping_reset_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 config, "config");
cmdline_parse_token_num_t cmd_pctype_mapping_reset_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_pctype_mapping_reset_pctype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 pctype, "pctype");
cmdline_parse_token_string_t cmd_pctype_mapping_reset_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_pctype_mapping_reset_reset =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_reset_result,
		 reset, "reset");

static void
cmd_pctype_mapping_reset_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_reset_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_flow_type_mapping_reset(res->port_id);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_pctype_mapping_reset = {
	.f = cmd_pctype_mapping_reset_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype mapping reset",
	.tokens = {
		(void *)&cmd_pctype_mapping_reset_port,
		(void *)&cmd_pctype_mapping_reset_config,
		(void *)&cmd_pctype_mapping_reset_port_id,
		(void *)&cmd_pctype_mapping_reset_pctype,
		(void *)&cmd_pctype_mapping_reset_mapping,
		(void *)&cmd_pctype_mapping_reset_reset,
		NULL,
	},
};

/* show port pctype mapping */

/* Common result structure for show port pctype mapping */
struct cmd_pctype_mapping_get_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
};

/* Common CLI fields for pctype mapping get */
cmdline_parse_token_string_t cmd_pctype_mapping_get_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_get_result,
		 show, "show");
cmdline_parse_token_string_t cmd_pctype_mapping_get_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_get_result,
		 port, "port");
cmdline_parse_token_num_t cmd_pctype_mapping_get_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_pctype_mapping_get_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_pctype_mapping_get_pctype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_get_result,
		 pctype, "pctype");
cmdline_parse_token_string_t cmd_pctype_mapping_get_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_get_result,
		 mapping, "mapping");

static void
cmd_pctype_mapping_get_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_get_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_flow_type_mapping
				mapping[RTE_PMD_I40E_FLOW_TYPE_MAX];
	int i, j, first_pctype;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_flow_type_mapping_get(res->port_id, mapping);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		return;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		return;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
		return;
	}

#ifdef RTE_NET_I40E
	for (i = 0; i < RTE_PMD_I40E_FLOW_TYPE_MAX; i++) {
		if (mapping[i].pctype != 0ULL) {
			first_pctype = 1;

			printf("pctype: ");
			for (j = 0; j < RTE_PMD_I40E_PCTYPE_MAX; j++) {
				if (mapping[i].pctype & (1ULL << j)) {
					printf(first_pctype ?
					       "%02d" : ",%02d", j);
					first_pctype = 0;
				}
			}
			printf("  ->  flowtype: %02d\n", mapping[i].flow_type);
		}
	}
#endif
}

cmdline_parse_inst_t cmd_pctype_mapping_get = {
	.f = cmd_pctype_mapping_get_parsed,
	.data = NULL,
	.help_str = "show port <port_id> pctype mapping",
	.tokens = {
		(void *)&cmd_pctype_mapping_get_show,
		(void *)&cmd_pctype_mapping_get_port,
		(void *)&cmd_pctype_mapping_get_port_id,
		(void *)&cmd_pctype_mapping_get_pctype,
		(void *)&cmd_pctype_mapping_get_mapping,
		NULL,
	},
};

/* port config pctype mapping update */

/* Common result structure for port config pctype mapping update */
struct cmd_pctype_mapping_update_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t pctype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t update;
	cmdline_fixed_string_t pctype_list;
	uint16_t flow_type;
};

/* Common CLI fields for pctype mapping update*/
cmdline_parse_token_string_t cmd_pctype_mapping_update_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 port, "port");
cmdline_parse_token_string_t cmd_pctype_mapping_update_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 config, "config");
cmdline_parse_token_num_t cmd_pctype_mapping_update_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_pctype_mapping_update_pctype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 pctype, "pctype");
cmdline_parse_token_string_t cmd_pctype_mapping_update_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_pctype_mapping_update_update =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 update, "update");
cmdline_parse_token_string_t cmd_pctype_mapping_update_pc_type =
	TOKEN_STRING_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 pctype_list, NULL);
cmdline_parse_token_num_t cmd_pctype_mapping_update_flow_type =
	TOKEN_NUM_INITIALIZER
		(struct cmd_pctype_mapping_update_result,
		 flow_type, RTE_UINT16);

static void
cmd_pctype_mapping_update_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_pctype_mapping_update_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_flow_type_mapping mapping;
	unsigned int i;
	unsigned int nb_item;
	unsigned int pctype_list[RTE_PMD_I40E_PCTYPE_MAX];
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	nb_item = parse_item_list(res->pctype_list, "pctypes",
				  RTE_PMD_I40E_PCTYPE_MAX, pctype_list, 1);
	mapping.flow_type = res->flow_type;
	for (i = 0, mapping.pctype = 0ULL; i < nb_item; i++)
		mapping.pctype |= (1ULL << pctype_list[i]);
	ret = rte_pmd_i40e_flow_type_mapping_update(res->port_id,
						&mapping,
						1,
						0);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid pctype or flow type\n");
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_pctype_mapping_update = {
	.f = cmd_pctype_mapping_update_parsed,
	.data = NULL,
	.help_str = "port config <port_id> pctype mapping update"
	" <pctype_id_0,[pctype_id_1]*> <flowtype_id>",
	.tokens = {
		(void *)&cmd_pctype_mapping_update_port,
		(void *)&cmd_pctype_mapping_update_config,
		(void *)&cmd_pctype_mapping_update_port_id,
		(void *)&cmd_pctype_mapping_update_pctype,
		(void *)&cmd_pctype_mapping_update_mapping,
		(void *)&cmd_pctype_mapping_update_update,
		(void *)&cmd_pctype_mapping_update_pc_type,
		(void *)&cmd_pctype_mapping_update_flow_type,
		NULL,
	},
};

/* ptype mapping get */

/* Common result structure for ptype mapping get */
struct cmd_ptype_mapping_get_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t get;
	portid_t port_id;
	uint8_t valid_only;
};

/* Common CLI fields for ptype mapping get */
cmdline_parse_token_string_t cmd_ptype_mapping_get_ptype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_get_result,
		 ptype, "ptype");
cmdline_parse_token_string_t cmd_ptype_mapping_get_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_get_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_ptype_mapping_get_get =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_get_result,
		 get, "get");
cmdline_parse_token_num_t cmd_ptype_mapping_get_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_get_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_ptype_mapping_get_valid_only =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_get_result,
		 valid_only, RTE_UINT8);

static void
cmd_ptype_mapping_get_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_get_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	int max_ptype_num = 256;
	struct rte_pmd_i40e_ptype_mapping mapping[max_ptype_num];
	uint16_t count;
	int i;
#endif

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_ptype_mapping_get(res->port_id,
					mapping,
					max_ptype_num,
					&count,
					res->valid_only);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}

#ifdef RTE_NET_I40E
	if (!ret) {
		for (i = 0; i < count; i++)
			printf("%3d\t0x%08x\n",
				mapping[i].hw_ptype, mapping[i].sw_ptype);
	}
#endif
}

cmdline_parse_inst_t cmd_ptype_mapping_get = {
	.f = cmd_ptype_mapping_get_parsed,
	.data = NULL,
	.help_str = "ptype mapping get <port_id> <valid_only>",
	.tokens = {
		(void *)&cmd_ptype_mapping_get_ptype,
		(void *)&cmd_ptype_mapping_get_mapping,
		(void *)&cmd_ptype_mapping_get_get,
		(void *)&cmd_ptype_mapping_get_port_id,
		(void *)&cmd_ptype_mapping_get_valid_only,
		NULL,
	},
};

/* ptype mapping replace */

/* Common result structure for ptype mapping replace */
struct cmd_ptype_mapping_replace_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t replace;
	portid_t port_id;
	uint32_t target;
	uint8_t mask;
	uint32_t pkt_type;
};

/* Common CLI fields for ptype mapping replace */
cmdline_parse_token_string_t cmd_ptype_mapping_replace_ptype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 ptype, "ptype");
cmdline_parse_token_string_t cmd_ptype_mapping_replace_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_ptype_mapping_replace_replace =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 replace, "replace");
cmdline_parse_token_num_t cmd_ptype_mapping_replace_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_ptype_mapping_replace_target =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 target, RTE_UINT32);
cmdline_parse_token_num_t cmd_ptype_mapping_replace_mask =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 mask, RTE_UINT8);
cmdline_parse_token_num_t cmd_ptype_mapping_replace_pkt_type =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_replace_result,
		 pkt_type, RTE_UINT32);

static void
cmd_ptype_mapping_replace_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_replace_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_ptype_mapping_replace(res->port_id,
					res->target,
					res->mask,
					res->pkt_type);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid ptype 0x%8x or 0x%8x\n",
			res->target, res->pkt_type);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_ptype_mapping_replace = {
	.f = cmd_ptype_mapping_replace_parsed,
	.data = NULL,
	.help_str =
		"ptype mapping replace <port_id> <target> <mask> <pkt_type>",
	.tokens = {
		(void *)&cmd_ptype_mapping_replace_ptype,
		(void *)&cmd_ptype_mapping_replace_mapping,
		(void *)&cmd_ptype_mapping_replace_replace,
		(void *)&cmd_ptype_mapping_replace_port_id,
		(void *)&cmd_ptype_mapping_replace_target,
		(void *)&cmd_ptype_mapping_replace_mask,
		(void *)&cmd_ptype_mapping_replace_pkt_type,
		NULL,
	},
};

/* ptype mapping reset */

/* Common result structure for ptype mapping reset */
struct cmd_ptype_mapping_reset_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
	portid_t port_id;
};

/* Common CLI fields for ptype mapping reset*/
cmdline_parse_token_string_t cmd_ptype_mapping_reset_ptype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_reset_result,
		 ptype, "ptype");
cmdline_parse_token_string_t cmd_ptype_mapping_reset_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_reset_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_ptype_mapping_reset_reset =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_reset_result,
		 reset, "reset");
cmdline_parse_token_num_t cmd_ptype_mapping_reset_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_reset_result,
		 port_id, RTE_UINT16);

static void
cmd_ptype_mapping_reset_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_reset_result *res = parsed_result;
	int ret = -ENOTSUP;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	ret = rte_pmd_i40e_ptype_mapping_reset(res->port_id);
#endif

	switch (ret) {
	case 0:
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_ptype_mapping_reset = {
	.f = cmd_ptype_mapping_reset_parsed,
	.data = NULL,
	.help_str = "ptype mapping reset <port_id>",
	.tokens = {
		(void *)&cmd_ptype_mapping_reset_ptype,
		(void *)&cmd_ptype_mapping_reset_mapping,
		(void *)&cmd_ptype_mapping_reset_reset,
		(void *)&cmd_ptype_mapping_reset_port_id,
		NULL,
	},
};

/* ptype mapping update */

/* Common result structure for ptype mapping update */
struct cmd_ptype_mapping_update_result {
	cmdline_fixed_string_t ptype;
	cmdline_fixed_string_t mapping;
	cmdline_fixed_string_t reset;
	portid_t port_id;
	uint8_t hw_ptype;
	uint32_t sw_ptype;
};

/* Common CLI fields for ptype mapping update*/
cmdline_parse_token_string_t cmd_ptype_mapping_update_ptype =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 ptype, "ptype");
cmdline_parse_token_string_t cmd_ptype_mapping_update_mapping =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 mapping, "mapping");
cmdline_parse_token_string_t cmd_ptype_mapping_update_update =
	TOKEN_STRING_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 reset, "update");
cmdline_parse_token_num_t cmd_ptype_mapping_update_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_num_t cmd_ptype_mapping_update_hw_ptype =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 hw_ptype, RTE_UINT8);
cmdline_parse_token_num_t cmd_ptype_mapping_update_sw_ptype =
	TOKEN_NUM_INITIALIZER
		(struct cmd_ptype_mapping_update_result,
		 sw_ptype, RTE_UINT32);

static void
cmd_ptype_mapping_update_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_ptype_mapping_update_result *res = parsed_result;
	int ret = -ENOTSUP;
#ifdef RTE_NET_I40E
	struct rte_pmd_i40e_ptype_mapping mapping;
#endif
	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

#ifdef RTE_NET_I40E
	mapping.hw_ptype = res->hw_ptype;
	mapping.sw_ptype = res->sw_ptype;
	ret = rte_pmd_i40e_ptype_mapping_update(res->port_id,
						&mapping,
						1,
						0);
#endif

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid ptype 0x%8x\n", res->sw_ptype);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %d\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t cmd_ptype_mapping_update = {
	.f = cmd_ptype_mapping_update_parsed,
	.data = NULL,
	.help_str = "ptype mapping update <port_id> <hw_ptype> <sw_ptype>",
	.tokens = {
		(void *)&cmd_ptype_mapping_update_ptype,
		(void *)&cmd_ptype_mapping_update_mapping,
		(void *)&cmd_ptype_mapping_update_update,
		(void *)&cmd_ptype_mapping_update_port_id,
		(void *)&cmd_ptype_mapping_update_hw_ptype,
		(void *)&cmd_ptype_mapping_update_sw_ptype,
		NULL,
	},
};

/* Common result structure for file commands */
struct cmd_cmdfile_result {
	cmdline_fixed_string_t load;
	cmdline_fixed_string_t filename;
};

/* Common CLI fields for file commands */
cmdline_parse_token_string_t cmd_load_cmdfile =
	TOKEN_STRING_INITIALIZER(struct cmd_cmdfile_result, load, "load");
cmdline_parse_token_string_t cmd_load_cmdfile_filename =
	TOKEN_STRING_INITIALIZER(struct cmd_cmdfile_result, filename, NULL);

static void
cmd_load_from_file_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_cmdfile_result *res = parsed_result;

	cmdline_read_from_file(res->filename);
}

cmdline_parse_inst_t cmd_load_from_file = {
	.f = cmd_load_from_file_parsed,
	.data = NULL,
	.help_str = "load <filename>",
	.tokens = {
		(void *)&cmd_load_cmdfile,
		(void *)&cmd_load_cmdfile_filename,
		NULL,
	},
};

/* Get Rx offloads capabilities */
struct cmd_rx_offload_get_capa_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t rx_offload;
	cmdline_fixed_string_t capabilities;
};

cmdline_parse_token_string_t cmd_rx_offload_get_capa_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_capa_result,
		 show, "show");
cmdline_parse_token_string_t cmd_rx_offload_get_capa_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_capa_result,
		 port, "port");
cmdline_parse_token_num_t cmd_rx_offload_get_capa_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_rx_offload_get_capa_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_rx_offload_get_capa_rx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_capa_result,
		 rx_offload, "rx_offload");
cmdline_parse_token_string_t cmd_rx_offload_get_capa_capabilities =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_capa_result,
		 capabilities, "capabilities");

static void
print_rx_offloads(uint64_t offloads)
{
	uint64_t single_offload;
	int begin;
	int end;
	int bit;

	if (offloads == 0)
		return;

	begin = __builtin_ctzll(offloads);
	end = sizeof(offloads) * CHAR_BIT - __builtin_clzll(offloads);

	single_offload = 1ULL << begin;
	for (bit = begin; bit < end; bit++) {
		if (offloads & single_offload)
			printf(" %s",
			       rte_eth_dev_rx_offload_name(single_offload));
		single_offload <<= 1;
	}
}

static void
cmd_rx_offload_get_capa_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_rx_offload_get_capa_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	uint64_t queue_offloads;
	uint64_t port_offloads;
	int ret;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	queue_offloads = dev_info.rx_queue_offload_capa;
	port_offloads = dev_info.rx_offload_capa ^ queue_offloads;

	printf("Rx Offloading Capabilities of port %d :\n", port_id);
	printf("  Per Queue :");
	print_rx_offloads(queue_offloads);

	printf("\n");
	printf("  Per Port  :");
	print_rx_offloads(port_offloads);
	printf("\n\n");
}

cmdline_parse_inst_t cmd_rx_offload_get_capa = {
	.f = cmd_rx_offload_get_capa_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rx_offload capabilities",
	.tokens = {
		(void *)&cmd_rx_offload_get_capa_show,
		(void *)&cmd_rx_offload_get_capa_port,
		(void *)&cmd_rx_offload_get_capa_port_id,
		(void *)&cmd_rx_offload_get_capa_rx_offload,
		(void *)&cmd_rx_offload_get_capa_capabilities,
		NULL,
	}
};

/* Get Rx offloads configuration */
struct cmd_rx_offload_get_configuration_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t rx_offload;
	cmdline_fixed_string_t configuration;
};

cmdline_parse_token_string_t cmd_rx_offload_get_configuration_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_configuration_result,
		 show, "show");
cmdline_parse_token_string_t cmd_rx_offload_get_configuration_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_configuration_result,
		 port, "port");
cmdline_parse_token_num_t cmd_rx_offload_get_configuration_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_rx_offload_get_configuration_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_rx_offload_get_configuration_rx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_configuration_result,
		 rx_offload, "rx_offload");
cmdline_parse_token_string_t cmd_rx_offload_get_configuration_configuration =
	TOKEN_STRING_INITIALIZER
		(struct cmd_rx_offload_get_configuration_result,
		 configuration, "configuration");

static void
cmd_rx_offload_get_configuration_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_rx_offload_get_configuration_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	struct rte_port *port = &ports[port_id];
	struct rte_eth_conf dev_conf;
	uint64_t port_offloads;
	uint64_t queue_offloads;
	uint16_t nb_rx_queues;
	int q;
	int ret;

	printf("Rx Offloading Configuration of port %d :\n", port_id);

	ret = eth_dev_conf_get_print_err(port_id, &dev_conf);
	if (ret != 0)
		return;

	port_offloads = dev_conf.rxmode.offloads;
	printf("  Port :");
	print_rx_offloads(port_offloads);
	printf("\n");

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	nb_rx_queues = dev_info.nb_rx_queues;
	for (q = 0; q < nb_rx_queues; q++) {
		queue_offloads = port->rxq[q].conf.offloads;
		printf("  Queue[%2d] :", q);
		print_rx_offloads(queue_offloads);
		printf("\n");
	}
	printf("\n");
}

cmdline_parse_inst_t cmd_rx_offload_get_configuration = {
	.f = cmd_rx_offload_get_configuration_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rx_offload configuration",
	.tokens = {
		(void *)&cmd_rx_offload_get_configuration_show,
		(void *)&cmd_rx_offload_get_configuration_port,
		(void *)&cmd_rx_offload_get_configuration_port_id,
		(void *)&cmd_rx_offload_get_configuration_rx_offload,
		(void *)&cmd_rx_offload_get_configuration_configuration,
		NULL,
	}
};

/* Enable/Disable a per port offloading */
struct cmd_config_per_port_rx_offload_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t rx_offload;
	cmdline_fixed_string_t offload;
	cmdline_fixed_string_t on_off;
};

cmdline_parse_token_string_t cmd_config_per_port_rx_offload_result_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 port, "port");
cmdline_parse_token_string_t cmd_config_per_port_rx_offload_result_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 config, "config");
cmdline_parse_token_num_t cmd_config_per_port_rx_offload_result_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_port_rx_offload_result_rx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 rx_offload, "rx_offload");
cmdline_parse_token_string_t cmd_config_per_port_rx_offload_result_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 offload, "vlan_strip#ipv4_cksum#udp_cksum#tcp_cksum#tcp_lro#"
			   "qinq_strip#outer_ipv4_cksum#macsec_strip#"
			   "header_split#vlan_filter#vlan_extend#jumbo_frame#"
			   "scatter#buffer_split#timestamp#security#"
			   "keep_crc#rss_hash");
cmdline_parse_token_string_t cmd_config_per_port_rx_offload_result_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_rx_offload_result,
		 on_off, "on#off");

static uint64_t
search_rx_offload(const char *name)
{
	uint64_t single_offload;
	const char *single_name;
	int found = 0;
	unsigned int bit;

	single_offload = 1;
	for (bit = 0; bit < sizeof(single_offload) * CHAR_BIT; bit++) {
		single_name = rte_eth_dev_rx_offload_name(single_offload);
		if (!strcasecmp(single_name, name)) {
			found = 1;
			break;
		}
		single_offload <<= 1;
	}

	if (found)
		return single_offload;

	return 0;
}

static void
cmd_config_per_port_rx_offload_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_per_port_rx_offload_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_eth_dev_info dev_info;
	struct rte_port *port = &ports[port_id];
	uint64_t single_offload;
	uint16_t nb_rx_queues;
	int q;
	int ret;

	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"Error: Can't config offload when Port %d is not stopped\n",
			port_id);
		return;
	}

	single_offload = search_rx_offload(res->offload);
	if (single_offload == 0) {
		fprintf(stderr, "Unknown offload name: %s\n", res->offload);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	nb_rx_queues = dev_info.nb_rx_queues;
	if (!strcmp(res->on_off, "on")) {
		port->dev_conf.rxmode.offloads |= single_offload;
		for (q = 0; q < nb_rx_queues; q++)
			port->rxq[q].conf.offloads |= single_offload;
	} else {
		port->dev_conf.rxmode.offloads &= ~single_offload;
		for (q = 0; q < nb_rx_queues; q++)
			port->rxq[q].conf.offloads &= ~single_offload;
	}

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_inst_t cmd_config_per_port_rx_offload = {
	.f = cmd_config_per_port_rx_offload_parsed,
	.data = NULL,
	.help_str = "port config <port_id> rx_offload vlan_strip|ipv4_cksum|"
		    "udp_cksum|tcp_cksum|tcp_lro|qinq_strip|outer_ipv4_cksum|"
		    "macsec_strip|header_split|vlan_filter|vlan_extend|"
		    "jumbo_frame|scatter|buffer_split|timestamp|security|"
		    "keep_crc|rss_hash on|off",
	.tokens = {
		(void *)&cmd_config_per_port_rx_offload_result_port,
		(void *)&cmd_config_per_port_rx_offload_result_config,
		(void *)&cmd_config_per_port_rx_offload_result_port_id,
		(void *)&cmd_config_per_port_rx_offload_result_rx_offload,
		(void *)&cmd_config_per_port_rx_offload_result_offload,
		(void *)&cmd_config_per_port_rx_offload_result_on_off,
		NULL,
	}
};

/* Enable/Disable a per queue offloading */
struct cmd_config_per_queue_rx_offload_result {
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t rxq;
	uint16_t queue_id;
	cmdline_fixed_string_t rx_offload;
	cmdline_fixed_string_t offload;
	cmdline_fixed_string_t on_off;
};

cmdline_parse_token_string_t cmd_config_per_queue_rx_offload_result_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 port, "port");
cmdline_parse_token_num_t cmd_config_per_queue_rx_offload_result_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_queue_rx_offload_result_rxq =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 rxq, "rxq");
cmdline_parse_token_num_t cmd_config_per_queue_rx_offload_result_queue_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 queue_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_queue_rx_offload_result_rxoffload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 rx_offload, "rx_offload");
cmdline_parse_token_string_t cmd_config_per_queue_rx_offload_result_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 offload, "vlan_strip#ipv4_cksum#udp_cksum#tcp_cksum#tcp_lro#"
			   "qinq_strip#outer_ipv4_cksum#macsec_strip#"
			   "header_split#vlan_filter#vlan_extend#jumbo_frame#"
			   "scatter#buffer_split#timestamp#security#keep_crc");
cmdline_parse_token_string_t cmd_config_per_queue_rx_offload_result_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_rx_offload_result,
		 on_off, "on#off");

static void
cmd_config_per_queue_rx_offload_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_per_queue_rx_offload_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	uint16_t queue_id = res->queue_id;
	struct rte_port *port = &ports[port_id];
	uint64_t single_offload;
	int ret;

	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"Error: Can't config offload when Port %d is not stopped\n",
			port_id);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (queue_id >= dev_info.nb_rx_queues) {
		fprintf(stderr,
			"Error: input queue_id should be 0 ... %d\n",
			dev_info.nb_rx_queues - 1);
		return;
	}

	single_offload = search_rx_offload(res->offload);
	if (single_offload == 0) {
		fprintf(stderr, "Unknown offload name: %s\n", res->offload);
		return;
	}

	if (!strcmp(res->on_off, "on"))
		port->rxq[queue_id].conf.offloads |= single_offload;
	else
		port->rxq[queue_id].conf.offloads &= ~single_offload;

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_inst_t cmd_config_per_queue_rx_offload = {
	.f = cmd_config_per_queue_rx_offload_parsed,
	.data = NULL,
	.help_str = "port <port_id> rxq <queue_id> rx_offload "
		    "vlan_strip|ipv4_cksum|"
		    "udp_cksum|tcp_cksum|tcp_lro|qinq_strip|outer_ipv4_cksum|"
		    "macsec_strip|header_split|vlan_filter|vlan_extend|"
		    "jumbo_frame|scatter|buffer_split|timestamp|security|"
		    "keep_crc on|off",
	.tokens = {
		(void *)&cmd_config_per_queue_rx_offload_result_port,
		(void *)&cmd_config_per_queue_rx_offload_result_port_id,
		(void *)&cmd_config_per_queue_rx_offload_result_rxq,
		(void *)&cmd_config_per_queue_rx_offload_result_queue_id,
		(void *)&cmd_config_per_queue_rx_offload_result_rxoffload,
		(void *)&cmd_config_per_queue_rx_offload_result_offload,
		(void *)&cmd_config_per_queue_rx_offload_result_on_off,
		NULL,
	}
};

/* Get Tx offloads capabilities */
struct cmd_tx_offload_get_capa_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t tx_offload;
	cmdline_fixed_string_t capabilities;
};

cmdline_parse_token_string_t cmd_tx_offload_get_capa_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_capa_result,
		 show, "show");
cmdline_parse_token_string_t cmd_tx_offload_get_capa_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_capa_result,
		 port, "port");
cmdline_parse_token_num_t cmd_tx_offload_get_capa_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_tx_offload_get_capa_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_tx_offload_get_capa_tx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_capa_result,
		 tx_offload, "tx_offload");
cmdline_parse_token_string_t cmd_tx_offload_get_capa_capabilities =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_capa_result,
		 capabilities, "capabilities");

static void
print_tx_offloads(uint64_t offloads)
{
	uint64_t single_offload;
	int begin;
	int end;
	int bit;

	if (offloads == 0)
		return;

	begin = __builtin_ctzll(offloads);
	end = sizeof(offloads) * CHAR_BIT - __builtin_clzll(offloads);

	single_offload = 1ULL << begin;
	for (bit = begin; bit < end; bit++) {
		if (offloads & single_offload)
			printf(" %s",
			       rte_eth_dev_tx_offload_name(single_offload));
		single_offload <<= 1;
	}
}

static void
cmd_tx_offload_get_capa_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_tx_offload_get_capa_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	uint64_t queue_offloads;
	uint64_t port_offloads;
	int ret;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	queue_offloads = dev_info.tx_queue_offload_capa;
	port_offloads = dev_info.tx_offload_capa ^ queue_offloads;

	printf("Tx Offloading Capabilities of port %d :\n", port_id);
	printf("  Per Queue :");
	print_tx_offloads(queue_offloads);

	printf("\n");
	printf("  Per Port  :");
	print_tx_offloads(port_offloads);
	printf("\n\n");
}

cmdline_parse_inst_t cmd_tx_offload_get_capa = {
	.f = cmd_tx_offload_get_capa_parsed,
	.data = NULL,
	.help_str = "show port <port_id> tx_offload capabilities",
	.tokens = {
		(void *)&cmd_tx_offload_get_capa_show,
		(void *)&cmd_tx_offload_get_capa_port,
		(void *)&cmd_tx_offload_get_capa_port_id,
		(void *)&cmd_tx_offload_get_capa_tx_offload,
		(void *)&cmd_tx_offload_get_capa_capabilities,
		NULL,
	}
};

/* Get Tx offloads configuration */
struct cmd_tx_offload_get_configuration_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t tx_offload;
	cmdline_fixed_string_t configuration;
};

cmdline_parse_token_string_t cmd_tx_offload_get_configuration_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_configuration_result,
		 show, "show");
cmdline_parse_token_string_t cmd_tx_offload_get_configuration_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_configuration_result,
		 port, "port");
cmdline_parse_token_num_t cmd_tx_offload_get_configuration_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_tx_offload_get_configuration_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_tx_offload_get_configuration_tx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_configuration_result,
		 tx_offload, "tx_offload");
cmdline_parse_token_string_t cmd_tx_offload_get_configuration_configuration =
	TOKEN_STRING_INITIALIZER
		(struct cmd_tx_offload_get_configuration_result,
		 configuration, "configuration");

static void
cmd_tx_offload_get_configuration_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_tx_offload_get_configuration_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	struct rte_port *port = &ports[port_id];
	struct rte_eth_conf dev_conf;
	uint64_t port_offloads;
	uint64_t queue_offloads;
	uint16_t nb_tx_queues;
	int q;
	int ret;

	printf("Tx Offloading Configuration of port %d :\n", port_id);

	ret = eth_dev_conf_get_print_err(port_id, &dev_conf);
	if (ret != 0)
		return;

	port_offloads = dev_conf.txmode.offloads;
	printf("  Port :");
	print_tx_offloads(port_offloads);
	printf("\n");

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	nb_tx_queues = dev_info.nb_tx_queues;
	for (q = 0; q < nb_tx_queues; q++) {
		queue_offloads = port->txq[q].conf.offloads;
		printf("  Queue[%2d] :", q);
		print_tx_offloads(queue_offloads);
		printf("\n");
	}
	printf("\n");
}

cmdline_parse_inst_t cmd_tx_offload_get_configuration = {
	.f = cmd_tx_offload_get_configuration_parsed,
	.data = NULL,
	.help_str = "show port <port_id> tx_offload configuration",
	.tokens = {
		(void *)&cmd_tx_offload_get_configuration_show,
		(void *)&cmd_tx_offload_get_configuration_port,
		(void *)&cmd_tx_offload_get_configuration_port_id,
		(void *)&cmd_tx_offload_get_configuration_tx_offload,
		(void *)&cmd_tx_offload_get_configuration_configuration,
		NULL,
	}
};

/* Enable/Disable a per port offloading */
struct cmd_config_per_port_tx_offload_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t config;
	portid_t port_id;
	cmdline_fixed_string_t tx_offload;
	cmdline_fixed_string_t offload;
	cmdline_fixed_string_t on_off;
};

cmdline_parse_token_string_t cmd_config_per_port_tx_offload_result_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 port, "port");
cmdline_parse_token_string_t cmd_config_per_port_tx_offload_result_config =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 config, "config");
cmdline_parse_token_num_t cmd_config_per_port_tx_offload_result_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_port_tx_offload_result_tx_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 tx_offload, "tx_offload");
cmdline_parse_token_string_t cmd_config_per_port_tx_offload_result_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 offload, "vlan_insert#ipv4_cksum#udp_cksum#tcp_cksum#"
			  "sctp_cksum#tcp_tso#udp_tso#outer_ipv4_cksum#"
			  "qinq_insert#vxlan_tnl_tso#gre_tnl_tso#"
			  "ipip_tnl_tso#geneve_tnl_tso#macsec_insert#"
			  "mt_lockfree#multi_segs#mbuf_fast_free#security#"
			  "send_on_timestamp");
cmdline_parse_token_string_t cmd_config_per_port_tx_offload_result_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_port_tx_offload_result,
		 on_off, "on#off");

static uint64_t
search_tx_offload(const char *name)
{
	uint64_t single_offload;
	const char *single_name;
	int found = 0;
	unsigned int bit;

	single_offload = 1;
	for (bit = 0; bit < sizeof(single_offload) * CHAR_BIT; bit++) {
		single_name = rte_eth_dev_tx_offload_name(single_offload);
		if (single_name == NULL)
			break;
		if (!strcasecmp(single_name, name)) {
			found = 1;
			break;
		} else if (!strcasecmp(single_name, "UNKNOWN"))
			break;
		single_offload <<= 1;
	}

	if (found)
		return single_offload;

	return 0;
}

static void
cmd_config_per_port_tx_offload_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_per_port_tx_offload_result *res = parsed_result;
	portid_t port_id = res->port_id;
	struct rte_eth_dev_info dev_info;
	struct rte_port *port = &ports[port_id];
	uint64_t single_offload;
	uint16_t nb_tx_queues;
	int q;
	int ret;

	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"Error: Can't config offload when Port %d is not stopped\n",
			port_id);
		return;
	}

	single_offload = search_tx_offload(res->offload);
	if (single_offload == 0) {
		fprintf(stderr, "Unknown offload name: %s\n", res->offload);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	nb_tx_queues = dev_info.nb_tx_queues;
	if (!strcmp(res->on_off, "on")) {
		port->dev_conf.txmode.offloads |= single_offload;
		for (q = 0; q < nb_tx_queues; q++)
			port->txq[q].conf.offloads |= single_offload;
	} else {
		port->dev_conf.txmode.offloads &= ~single_offload;
		for (q = 0; q < nb_tx_queues; q++)
			port->txq[q].conf.offloads &= ~single_offload;
	}

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_inst_t cmd_config_per_port_tx_offload = {
	.f = cmd_config_per_port_tx_offload_parsed,
	.data = NULL,
	.help_str = "port config <port_id> tx_offload "
		    "vlan_insert|ipv4_cksum|udp_cksum|tcp_cksum|"
		    "sctp_cksum|tcp_tso|udp_tso|outer_ipv4_cksum|"
		    "qinq_insert|vxlan_tnl_tso|gre_tnl_tso|"
		    "ipip_tnl_tso|geneve_tnl_tso|macsec_insert|"
		    "mt_lockfree|multi_segs|mbuf_fast_free|security|"
		    "send_on_timestamp on|off",
	.tokens = {
		(void *)&cmd_config_per_port_tx_offload_result_port,
		(void *)&cmd_config_per_port_tx_offload_result_config,
		(void *)&cmd_config_per_port_tx_offload_result_port_id,
		(void *)&cmd_config_per_port_tx_offload_result_tx_offload,
		(void *)&cmd_config_per_port_tx_offload_result_offload,
		(void *)&cmd_config_per_port_tx_offload_result_on_off,
		NULL,
	}
};

/* Enable/Disable a per queue offloading */
struct cmd_config_per_queue_tx_offload_result {
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t txq;
	uint16_t queue_id;
	cmdline_fixed_string_t tx_offload;
	cmdline_fixed_string_t offload;
	cmdline_fixed_string_t on_off;
};

cmdline_parse_token_string_t cmd_config_per_queue_tx_offload_result_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 port, "port");
cmdline_parse_token_num_t cmd_config_per_queue_tx_offload_result_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_queue_tx_offload_result_txq =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 txq, "txq");
cmdline_parse_token_num_t cmd_config_per_queue_tx_offload_result_queue_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 queue_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_per_queue_tx_offload_result_txoffload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 tx_offload, "tx_offload");
cmdline_parse_token_string_t cmd_config_per_queue_tx_offload_result_offload =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 offload, "vlan_insert#ipv4_cksum#udp_cksum#tcp_cksum#"
			  "sctp_cksum#tcp_tso#udp_tso#outer_ipv4_cksum#"
			  "qinq_insert#vxlan_tnl_tso#gre_tnl_tso#"
			  "ipip_tnl_tso#geneve_tnl_tso#macsec_insert#"
			  "mt_lockfree#multi_segs#mbuf_fast_free#security");
cmdline_parse_token_string_t cmd_config_per_queue_tx_offload_result_on_off =
	TOKEN_STRING_INITIALIZER
		(struct cmd_config_per_queue_tx_offload_result,
		 on_off, "on#off");

static void
cmd_config_per_queue_tx_offload_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_per_queue_tx_offload_result *res = parsed_result;
	struct rte_eth_dev_info dev_info;
	portid_t port_id = res->port_id;
	uint16_t queue_id = res->queue_id;
	struct rte_port *port = &ports[port_id];
	uint64_t single_offload;
	int ret;

	if (port->port_status != RTE_PORT_STOPPED) {
		fprintf(stderr,
			"Error: Can't config offload when Port %d is not stopped\n",
			port_id);
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (queue_id >= dev_info.nb_tx_queues) {
		fprintf(stderr,
			"Error: input queue_id should be 0 ... %d\n",
			dev_info.nb_tx_queues - 1);
		return;
	}

	single_offload = search_tx_offload(res->offload);
	if (single_offload == 0) {
		fprintf(stderr, "Unknown offload name: %s\n", res->offload);
		return;
	}

	if (!strcmp(res->on_off, "on"))
		port->txq[queue_id].conf.offloads |= single_offload;
	else
		port->txq[queue_id].conf.offloads &= ~single_offload;

	cmd_reconfig_device_queue(port_id, 1, 1);
}

cmdline_parse_inst_t cmd_config_per_queue_tx_offload = {
	.f = cmd_config_per_queue_tx_offload_parsed,
	.data = NULL,
	.help_str = "port <port_id> txq <queue_id> tx_offload "
		    "vlan_insert|ipv4_cksum|udp_cksum|tcp_cksum|"
		    "sctp_cksum|tcp_tso|udp_tso|outer_ipv4_cksum|"
		    "qinq_insert|vxlan_tnl_tso|gre_tnl_tso|"
		    "ipip_tnl_tso|geneve_tnl_tso|macsec_insert|"
		    "mt_lockfree|multi_segs|mbuf_fast_free|security "
		    "on|off",
	.tokens = {
		(void *)&cmd_config_per_queue_tx_offload_result_port,
		(void *)&cmd_config_per_queue_tx_offload_result_port_id,
		(void *)&cmd_config_per_queue_tx_offload_result_txq,
		(void *)&cmd_config_per_queue_tx_offload_result_queue_id,
		(void *)&cmd_config_per_queue_tx_offload_result_txoffload,
		(void *)&cmd_config_per_queue_tx_offload_result_offload,
		(void *)&cmd_config_per_queue_tx_offload_result_on_off,
		NULL,
	}
};

/* *** configure tx_metadata for specific port *** */
struct cmd_config_tx_metadata_specific_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint16_t port_id;
	cmdline_fixed_string_t item;
	uint32_t value;
};

static void
cmd_config_tx_metadata_specific_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_tx_metadata_specific_result *res = parsed_result;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	ports[res->port_id].tx_metadata = res->value;
	/* Add/remove callback to insert valid metadata in every Tx packet. */
	if (ports[res->port_id].tx_metadata)
		add_tx_md_callback(res->port_id);
	else
		remove_tx_md_callback(res->port_id);
	rte_flow_dynf_metadata_register();
}

cmdline_parse_token_string_t cmd_config_tx_metadata_specific_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_metadata_specific_result,
			port, "port");
cmdline_parse_token_string_t cmd_config_tx_metadata_specific_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_metadata_specific_result,
			keyword, "config");
cmdline_parse_token_num_t cmd_config_tx_metadata_specific_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_tx_metadata_specific_result,
			port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_tx_metadata_specific_item =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_metadata_specific_result,
			item, "tx_metadata");
cmdline_parse_token_num_t cmd_config_tx_metadata_specific_value =
	TOKEN_NUM_INITIALIZER(struct cmd_config_tx_metadata_specific_result,
			value, RTE_UINT32);

cmdline_parse_inst_t cmd_config_tx_metadata_specific = {
	.f = cmd_config_tx_metadata_specific_parsed,
	.data = NULL,
	.help_str = "port config <port_id> tx_metadata <value>",
	.tokens = {
		(void *)&cmd_config_tx_metadata_specific_port,
		(void *)&cmd_config_tx_metadata_specific_keyword,
		(void *)&cmd_config_tx_metadata_specific_id,
		(void *)&cmd_config_tx_metadata_specific_item,
		(void *)&cmd_config_tx_metadata_specific_value,
		NULL,
	},
};

/* *** set dynf *** */
struct cmd_config_tx_dynf_specific_result {
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	uint16_t port_id;
	cmdline_fixed_string_t item;
	cmdline_fixed_string_t name;
	cmdline_fixed_string_t value;
};

static void
cmd_config_dynf_specific_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_config_tx_dynf_specific_result *res = parsed_result;
	struct rte_mbuf_dynflag desc_flag;
	int flag;
	uint64_t old_port_flags;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	flag = rte_mbuf_dynflag_lookup(res->name, NULL);
	if (flag <= 0) {
		if (strlcpy(desc_flag.name, res->name,
			    RTE_MBUF_DYN_NAMESIZE) >= RTE_MBUF_DYN_NAMESIZE) {
			fprintf(stderr, "Flag name too long\n");
			return;
		}
		desc_flag.flags = 0;
		flag = rte_mbuf_dynflag_register(&desc_flag);
		if (flag < 0) {
			fprintf(stderr, "Can't register flag\n");
			return;
		}
		strcpy(dynf_names[flag], desc_flag.name);
	}
	old_port_flags = ports[res->port_id].mbuf_dynf;
	if (!strcmp(res->value, "set")) {
		ports[res->port_id].mbuf_dynf |= 1UL << flag;
		if (old_port_flags == 0)
			add_tx_dynf_callback(res->port_id);
	} else {
		ports[res->port_id].mbuf_dynf &= ~(1UL << flag);
		if (ports[res->port_id].mbuf_dynf == 0)
			remove_tx_dynf_callback(res->port_id);
	}
}

cmdline_parse_token_string_t cmd_config_tx_dynf_specific_port =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			keyword, "port");
cmdline_parse_token_string_t cmd_config_tx_dynf_specific_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			keyword, "config");
cmdline_parse_token_num_t cmd_config_tx_dynf_specific_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_config_tx_dynf_specific_item =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			item, "dynf");
cmdline_parse_token_string_t cmd_config_tx_dynf_specific_name =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			name, NULL);
cmdline_parse_token_string_t cmd_config_tx_dynf_specific_value =
	TOKEN_STRING_INITIALIZER(struct cmd_config_tx_dynf_specific_result,
			value, "set#clear");

cmdline_parse_inst_t cmd_config_tx_dynf_specific = {
	.f = cmd_config_dynf_specific_parsed,
	.data = NULL,
	.help_str = "port config <port id> dynf <name> set|clear",
	.tokens = {
		(void *)&cmd_config_tx_dynf_specific_port,
		(void *)&cmd_config_tx_dynf_specific_keyword,
		(void *)&cmd_config_tx_dynf_specific_port_id,
		(void *)&cmd_config_tx_dynf_specific_item,
		(void *)&cmd_config_tx_dynf_specific_name,
		(void *)&cmd_config_tx_dynf_specific_value,
		NULL,
	},
};

/* *** display tx_metadata per port configuration *** */
struct cmd_show_tx_metadata_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_show_tx_metadata_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_tx_metadata_result *res = parsed_result;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "invalid port id %u\n", res->cmd_pid);
		return;
	}
	if (!strcmp(res->cmd_keyword, "tx_metadata")) {
		printf("Port %u tx_metadata: %u\n", res->cmd_pid,
		       ports[res->cmd_pid].tx_metadata);
	}
}

cmdline_parse_token_string_t cmd_show_tx_metadata_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_tx_metadata_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_show_tx_metadata_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_tx_metadata_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_show_tx_metadata_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_show_tx_metadata_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_tx_metadata_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_show_tx_metadata_result,
			cmd_keyword, "tx_metadata");

cmdline_parse_inst_t cmd_show_tx_metadata = {
	.f = cmd_show_tx_metadata_parsed,
	.data = NULL,
	.help_str = "show port <port_id> tx_metadata",
	.tokens = {
		(void *)&cmd_show_tx_metadata_show,
		(void *)&cmd_show_tx_metadata_port,
		(void *)&cmd_show_tx_metadata_pid,
		(void *)&cmd_show_tx_metadata_keyword,
		NULL,
	},
};

/* *** show fec capability per port configuration *** */
struct cmd_show_fec_capability_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_fec;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_show_fec_capability_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_fec_capability_result *res = parsed_result;
	struct rte_eth_fec_capa *speed_fec_capa;
	unsigned int num;
	int ret;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "Invalid port id %u\n", res->cmd_pid);
		return;
	}

	ret = rte_eth_fec_get_capability(res->cmd_pid, NULL, 0);
	if (ret == -ENOTSUP) {
		fprintf(stderr, "Function not implemented\n");
		return;
	} else if (ret < 0) {
		fprintf(stderr, "Get FEC capability failed: %d\n", ret);
		return;
	}

	num = (unsigned int)ret;
	speed_fec_capa = calloc(num, sizeof(*speed_fec_capa));
	if (speed_fec_capa == NULL) {
		fprintf(stderr, "Failed to alloc FEC capability buffer\n");
		return;
	}

	ret = rte_eth_fec_get_capability(res->cmd_pid, speed_fec_capa, num);
	if (ret < 0) {
		fprintf(stderr, "Error getting FEC capability: %d\n", ret);
		goto out;
	}

	show_fec_capability(num, speed_fec_capa);
out:
	free(speed_fec_capa);
}

cmdline_parse_token_string_t cmd_show_fec_capability_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_capability_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_show_fec_capability_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_capability_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_show_fec_capability_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_show_fec_capability_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_fec_capability_fec =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_capability_result,
			cmd_fec, "fec");
cmdline_parse_token_string_t cmd_show_fec_capability_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_capability_result,
			cmd_keyword, "capabilities");

cmdline_parse_inst_t cmd_show_capability = {
	.f = cmd_show_fec_capability_parsed,
	.data = NULL,
	.help_str = "show port <port_id> fec capabilities",
	.tokens = {
		(void *)&cmd_show_fec_capability_show,
		(void *)&cmd_show_fec_capability_port,
		(void *)&cmd_show_fec_capability_pid,
		(void *)&cmd_show_fec_capability_fec,
		(void *)&cmd_show_fec_capability_keyword,
		NULL,
	},
};

/* *** show fec mode per port configuration *** */
struct cmd_show_fec_metadata_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_show_fec_mode_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
#define FEC_NAME_SIZE 16
	struct cmd_show_fec_metadata_result *res = parsed_result;
	uint32_t mode;
	char buf[FEC_NAME_SIZE];
	int ret;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "Invalid port id %u\n", res->cmd_pid);
		return;
	}
	ret = rte_eth_fec_get(res->cmd_pid, &mode);
	if (ret == -ENOTSUP) {
		fprintf(stderr, "Function not implemented\n");
		return;
	} else if (ret < 0) {
		fprintf(stderr, "Get FEC mode failed\n");
		return;
	}

	switch (mode) {
	case RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC):
		strlcpy(buf, "off", sizeof(buf));
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(AUTO):
		strlcpy(buf, "auto", sizeof(buf));
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(BASER):
		strlcpy(buf, "baser", sizeof(buf));
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(RS):
		strlcpy(buf, "rs", sizeof(buf));
		break;
	default:
		return;
	}

	printf("%s\n", buf);
}

cmdline_parse_token_string_t cmd_show_fec_mode_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_metadata_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_show_fec_mode_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_metadata_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_show_fec_mode_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_show_fec_metadata_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_fec_mode_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_show_fec_metadata_result,
			cmd_keyword, "fec_mode");

cmdline_parse_inst_t cmd_show_fec_mode = {
	.f = cmd_show_fec_mode_parsed,
	.data = NULL,
	.help_str = "show port <port_id> fec_mode",
	.tokens = {
		(void *)&cmd_show_fec_mode_show,
		(void *)&cmd_show_fec_mode_port,
		(void *)&cmd_show_fec_mode_pid,
		(void *)&cmd_show_fec_mode_keyword,
		NULL,
	},
};

/* *** set fec mode per port configuration *** */
struct cmd_set_port_fec_mode {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t fec_mode;
	cmdline_fixed_string_t fec_value;
};

/* Common CLI fields for set fec mode */
cmdline_parse_token_string_t cmd_set_port_fec_mode_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_fec_mode,
		 set, "set");
cmdline_parse_token_string_t cmd_set_port_fec_mode_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_fec_mode,
		 port, "port");
cmdline_parse_token_num_t cmd_set_port_fec_mode_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_port_fec_mode,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_port_fec_mode_str =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_fec_mode,
		 fec_mode, "fec_mode");
cmdline_parse_token_string_t cmd_set_port_fec_mode_value =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_fec_mode,
		 fec_value, NULL);

static void
cmd_set_port_fec_mode_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_port_fec_mode *res = parsed_result;
	uint16_t port_id = res->port_id;
	uint32_t fec_capa;
	int ret;

	ret = parse_fec_mode(res->fec_value, &fec_capa);
	if (ret < 0) {
		fprintf(stderr, "Unknown fec mode: %s for port %d\n",
				res->fec_value,	port_id);
		return;
	}

	ret = rte_eth_fec_set(port_id, fec_capa);
	if (ret == -ENOTSUP) {
		fprintf(stderr, "Function not implemented\n");
		return;
	} else if (ret < 0) {
		fprintf(stderr, "Set FEC mode failed\n");
		return;
	}
}

cmdline_parse_inst_t cmd_set_fec_mode = {
	.f = cmd_set_port_fec_mode_parsed,
	.data = NULL,
	.help_str = "set port <port_id> fec_mode auto|off|rs|baser",
	.tokens = {
		(void *)&cmd_set_port_fec_mode_set,
		(void *)&cmd_set_port_fec_mode_port,
		(void *)&cmd_set_port_fec_mode_port_id,
		(void *)&cmd_set_port_fec_mode_str,
		(void *)&cmd_set_port_fec_mode_value,
		NULL,
	},
};

/* show port supported ptypes */

/* Common result structure for show port ptypes */
struct cmd_show_port_supported_ptypes_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t ptypes;
};

/* Common CLI fields for show port ptypes */
cmdline_parse_token_string_t cmd_show_port_supported_ptypes_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_supported_ptypes_result,
		 show, "show");
cmdline_parse_token_string_t cmd_show_port_supported_ptypes_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_supported_ptypes_result,
		 port, "port");
cmdline_parse_token_num_t cmd_show_port_supported_ptypes_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_port_supported_ptypes_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_port_supported_ptypes_ptypes =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_supported_ptypes_result,
		 ptypes, "ptypes");

static void
cmd_show_port_supported_ptypes_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
#define RSVD_PTYPE_MASK       0xf0000000
#define MAX_PTYPES_PER_LAYER  16
#define LTYPE_NAMESIZE        32
#define PTYPE_NAMESIZE        256
	struct cmd_show_port_supported_ptypes_result *res = parsed_result;
	char buf[PTYPE_NAMESIZE], ltype[LTYPE_NAMESIZE];
	uint32_t ptype_mask = RTE_PTYPE_L2_MASK;
	uint32_t ptypes[MAX_PTYPES_PER_LAYER];
	uint16_t port_id = res->port_id;
	int ret, i;

	ret = rte_eth_dev_get_supported_ptypes(port_id, ptype_mask, NULL, 0);
	if (ret < 0)
		return;

	while (ptype_mask != RSVD_PTYPE_MASK) {

		switch (ptype_mask) {
		case RTE_PTYPE_L2_MASK:
			strlcpy(ltype, "L2", sizeof(ltype));
			break;
		case RTE_PTYPE_L3_MASK:
			strlcpy(ltype, "L3", sizeof(ltype));
			break;
		case RTE_PTYPE_L4_MASK:
			strlcpy(ltype, "L4", sizeof(ltype));
			break;
		case RTE_PTYPE_TUNNEL_MASK:
			strlcpy(ltype, "Tunnel", sizeof(ltype));
			break;
		case RTE_PTYPE_INNER_L2_MASK:
			strlcpy(ltype, "Inner L2", sizeof(ltype));
			break;
		case RTE_PTYPE_INNER_L3_MASK:
			strlcpy(ltype, "Inner L3", sizeof(ltype));
			break;
		case RTE_PTYPE_INNER_L4_MASK:
			strlcpy(ltype, "Inner L4", sizeof(ltype));
			break;
		default:
			return;
		}

		ret = rte_eth_dev_get_supported_ptypes(res->port_id,
						       ptype_mask, ptypes,
						       MAX_PTYPES_PER_LAYER);

		if (ret > 0)
			printf("Supported %s ptypes:\n", ltype);
		else
			printf("%s ptypes unsupported\n", ltype);

		for (i = 0; i < ret; ++i) {
			rte_get_ptype_name(ptypes[i], buf, sizeof(buf));
			printf("%s\n", buf);
		}

		ptype_mask <<= 4;
	}
}

cmdline_parse_inst_t cmd_show_port_supported_ptypes = {
	.f = cmd_show_port_supported_ptypes_parsed,
	.data = NULL,
	.help_str = "show port <port_id> ptypes",
	.tokens = {
		(void *)&cmd_show_port_supported_ptypes_show,
		(void *)&cmd_show_port_supported_ptypes_port,
		(void *)&cmd_show_port_supported_ptypes_port_id,
		(void *)&cmd_show_port_supported_ptypes_ptypes,
		NULL,
	},
};

/* *** display rx/tx descriptor status *** */
struct cmd_show_rx_tx_desc_status_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	cmdline_fixed_string_t cmd_desc;
	cmdline_fixed_string_t cmd_status;
	portid_t cmd_pid;
	portid_t cmd_qid;
	portid_t cmd_did;
};

static void
cmd_show_rx_tx_desc_status_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_rx_tx_desc_status_result *res = parsed_result;
	int rc;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "invalid port id %u\n", res->cmd_pid);
		return;
	}

	if (!strcmp(res->cmd_keyword, "rxq")) {
		rc = rte_eth_rx_descriptor_status(res->cmd_pid, res->cmd_qid,
					     res->cmd_did);
		if (rc < 0) {
			fprintf(stderr,
				"Invalid input: queue id = %d, desc id = %d\n",
				res->cmd_qid, res->cmd_did);
			return;
		}
		if (rc == RTE_ETH_RX_DESC_AVAIL)
			printf("Desc status = AVAILABLE\n");
		else if (rc == RTE_ETH_RX_DESC_DONE)
			printf("Desc status = DONE\n");
		else
			printf("Desc status = UNAVAILABLE\n");
	} else if (!strcmp(res->cmd_keyword, "txq")) {
		rc = rte_eth_tx_descriptor_status(res->cmd_pid, res->cmd_qid,
					     res->cmd_did);
		if (rc < 0) {
			fprintf(stderr,
				"Invalid input: queue id = %d, desc id = %d\n",
				res->cmd_qid, res->cmd_did);
			return;
		}
		if (rc == RTE_ETH_TX_DESC_FULL)
			printf("Desc status = FULL\n");
		else if (rc == RTE_ETH_TX_DESC_DONE)
			printf("Desc status = DONE\n");
		else
			printf("Desc status = UNAVAILABLE\n");
	}
}

cmdline_parse_token_string_t cmd_show_rx_tx_desc_status_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_show_rx_tx_desc_status_port =
	TOKEN_STRING_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_show_rx_tx_desc_status_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_rx_tx_desc_status_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_keyword, "rxq#txq");
cmdline_parse_token_num_t cmd_show_rx_tx_desc_status_qid =
	TOKEN_NUM_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_rx_tx_desc_status_desc =
	TOKEN_STRING_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_desc, "desc");
cmdline_parse_token_num_t cmd_show_rx_tx_desc_status_did =
	TOKEN_NUM_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_did, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_rx_tx_desc_status_status =
	TOKEN_STRING_INITIALIZER(struct cmd_show_rx_tx_desc_status_result,
			cmd_status, "status");
cmdline_parse_inst_t cmd_show_rx_tx_desc_status = {
	.f = cmd_show_rx_tx_desc_status_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rxq|txq <queue_id> desc <desc_id> "
		"status",
	.tokens = {
		(void *)&cmd_show_rx_tx_desc_status_show,
		(void *)&cmd_show_rx_tx_desc_status_port,
		(void *)&cmd_show_rx_tx_desc_status_pid,
		(void *)&cmd_show_rx_tx_desc_status_keyword,
		(void *)&cmd_show_rx_tx_desc_status_qid,
		(void *)&cmd_show_rx_tx_desc_status_desc,
		(void *)&cmd_show_rx_tx_desc_status_did,
		(void *)&cmd_show_rx_tx_desc_status_status,
		NULL,
	},
};

/* *** display rx queue desc used count *** */
struct cmd_show_rx_queue_desc_used_count_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_rxq;
	cmdline_fixed_string_t cmd_desc;
	cmdline_fixed_string_t cmd_used;
	cmdline_fixed_string_t cmd_count;
	portid_t cmd_pid;
	portid_t cmd_qid;
};

static void
cmd_show_rx_queue_desc_used_count_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_show_rx_queue_desc_used_count_result *res = parsed_result;
	int rc;

	if (!rte_eth_dev_is_valid_port(res->cmd_pid)) {
		fprintf(stderr, "invalid port id %u\n", res->cmd_pid);
		return;
	}

	rc = rte_eth_rx_queue_count(res->cmd_pid, res->cmd_qid);
	if (rc < 0) {
		fprintf(stderr, "Invalid queueid = %d\n", res->cmd_qid);
		return;
	}
	printf("Used desc count = %d\n", rc);
}

cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_show, "show");
cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_port, "port");
cmdline_parse_token_num_t cmd_show_rx_queue_desc_used_count_pid =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_rxq =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_rxq, "rxq");
cmdline_parse_token_num_t cmd_show_rx_queue_desc_used_count_qid =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_qid, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_desc =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_count, "desc");
cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_used =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_count, "used");
cmdline_parse_token_string_t cmd_show_rx_queue_desc_used_count_count =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_rx_queue_desc_used_count_result,
		 cmd_count, "count");
cmdline_parse_inst_t cmd_show_rx_queue_desc_used_count = {
	.f = cmd_show_rx_queue_desc_used_count_parsed,
	.data = NULL,
	.help_str = "show port <port_id> rxq <queue_id> desc used count",
	.tokens = {
		(void *)&cmd_show_rx_queue_desc_used_count_show,
		(void *)&cmd_show_rx_queue_desc_used_count_port,
		(void *)&cmd_show_rx_queue_desc_used_count_pid,
		(void *)&cmd_show_rx_queue_desc_used_count_rxq,
		(void *)&cmd_show_rx_queue_desc_used_count_qid,
		(void *)&cmd_show_rx_queue_desc_used_count_desc,
		(void *)&cmd_show_rx_queue_desc_used_count_used,
		(void *)&cmd_show_rx_queue_desc_used_count_count,
		NULL,
	},
};

/* Common result structure for set port ptypes */
struct cmd_set_port_ptypes_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t ptype_mask;
	uint32_t mask;
};

/* Common CLI fields for set port ptypes */
cmdline_parse_token_string_t cmd_set_port_ptypes_set =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_ptypes_result,
		 set, "set");
cmdline_parse_token_string_t cmd_set_port_ptypes_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_ptypes_result,
		 port, "port");
cmdline_parse_token_num_t cmd_set_port_ptypes_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_port_ptypes_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_set_port_ptypes_mask_str =
	TOKEN_STRING_INITIALIZER
		(struct cmd_set_port_ptypes_result,
		 ptype_mask, "ptype_mask");
cmdline_parse_token_num_t cmd_set_port_ptypes_mask_u32 =
	TOKEN_NUM_INITIALIZER
		(struct cmd_set_port_ptypes_result,
		 mask, RTE_UINT32);

static void
cmd_set_port_ptypes_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	__rte_unused void *data)
{
	struct cmd_set_port_ptypes_result *res = parsed_result;
#define PTYPE_NAMESIZE        256
	char ptype_name[PTYPE_NAMESIZE];
	uint16_t port_id = res->port_id;
	uint32_t ptype_mask = res->mask;
	int ret, i;

	ret = rte_eth_dev_get_supported_ptypes(port_id, RTE_PTYPE_ALL_MASK,
					       NULL, 0);
	if (ret <= 0) {
		fprintf(stderr, "Port %d doesn't support any ptypes.\n",
			port_id);
		return;
	}

	uint32_t ptypes[ret];

	ret = rte_eth_dev_set_ptypes(port_id, ptype_mask, ptypes, ret);
	if (ret < 0) {
		fprintf(stderr, "Unable to set requested ptypes for Port %d\n",
			port_id);
		return;
	}

	printf("Successfully set following ptypes for Port %d\n", port_id);
	for (i = 0; i < ret && ptypes[i] != RTE_PTYPE_UNKNOWN; i++) {
		rte_get_ptype_name(ptypes[i], ptype_name, sizeof(ptype_name));
		printf("%s\n", ptype_name);
	}

	clear_ptypes = false;
}

cmdline_parse_inst_t cmd_set_port_ptypes = {
	.f = cmd_set_port_ptypes_parsed,
	.data = NULL,
	.help_str = "set port <port_id> ptype_mask <mask>",
	.tokens = {
		(void *)&cmd_set_port_ptypes_set,
		(void *)&cmd_set_port_ptypes_port,
		(void *)&cmd_set_port_ptypes_port_id,
		(void *)&cmd_set_port_ptypes_mask_str,
		(void *)&cmd_set_port_ptypes_mask_u32,
		NULL,
	},
};

/* *** display mac addresses added to a port *** */
struct cmd_showport_macs_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_port;
	cmdline_fixed_string_t cmd_keyword;
	portid_t cmd_pid;
};

static void
cmd_showport_macs_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_showport_macs_result *res = parsed_result;

	if (port_id_is_invalid(res->cmd_pid, ENABLED_WARN))
		return;

	if (!strcmp(res->cmd_keyword, "macs"))
		show_macs(res->cmd_pid);
	else if (!strcmp(res->cmd_keyword, "mcast_macs"))
		show_mcast_macs(res->cmd_pid);
}

cmdline_parse_token_string_t cmd_showport_macs_show =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_macs_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_showport_macs_port =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_macs_result,
			cmd_port, "port");
cmdline_parse_token_num_t cmd_showport_macs_pid =
	TOKEN_NUM_INITIALIZER(struct cmd_showport_macs_result,
			cmd_pid, RTE_UINT16);
cmdline_parse_token_string_t cmd_showport_macs_keyword =
	TOKEN_STRING_INITIALIZER(struct cmd_showport_macs_result,
			cmd_keyword, "macs#mcast_macs");

cmdline_parse_inst_t cmd_showport_macs = {
	.f = cmd_showport_macs_parsed,
	.data = NULL,
	.help_str = "show port <port_id> macs|mcast_macs",
	.tokens = {
		(void *)&cmd_showport_macs_show,
		(void *)&cmd_showport_macs_port,
		(void *)&cmd_showport_macs_pid,
		(void *)&cmd_showport_macs_keyword,
		NULL,
	},
};

/* *** show flow transfer proxy port ID for the given port *** */
struct cmd_show_port_flow_transfer_proxy_result {
	cmdline_fixed_string_t show;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t flow;
	cmdline_fixed_string_t transfer;
	cmdline_fixed_string_t proxy;
};

cmdline_parse_token_string_t cmd_show_port_flow_transfer_proxy_show =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 show, "show");
cmdline_parse_token_string_t cmd_show_port_flow_transfer_proxy_port =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 port, "port");
cmdline_parse_token_num_t cmd_show_port_flow_transfer_proxy_port_id =
	TOKEN_NUM_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 port_id, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_port_flow_transfer_proxy_flow =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 flow, "flow");
cmdline_parse_token_string_t cmd_show_port_flow_transfer_proxy_transfer =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 transfer, "transfer");
cmdline_parse_token_string_t cmd_show_port_flow_transfer_proxy_proxy =
	TOKEN_STRING_INITIALIZER
		(struct cmd_show_port_flow_transfer_proxy_result,
		 proxy, "proxy");

static void
cmd_show_port_flow_transfer_proxy_parsed(void *parsed_result,
					 __rte_unused struct cmdline *cl,
					 __rte_unused void *data)
{
	struct cmd_show_port_flow_transfer_proxy_result *res = parsed_result;
	portid_t proxy_port_id;
	int ret;

	printf("\n");

	ret = rte_flow_pick_transfer_proxy(res->port_id, &proxy_port_id, NULL);
	if (ret != 0) {
		fprintf(stderr, "Failed to pick transfer proxy: %s\n",
			rte_strerror(-ret));
		return;
	}

	printf("Transfer proxy port ID: %u\n\n", proxy_port_id);
}

cmdline_parse_inst_t cmd_show_port_flow_transfer_proxy = {
	.f = cmd_show_port_flow_transfer_proxy_parsed,
	.data = NULL,
	.help_str = "show port <port_id> flow transfer proxy",
	.tokens = {
		(void *)&cmd_show_port_flow_transfer_proxy_show,
		(void *)&cmd_show_port_flow_transfer_proxy_port,
		(void *)&cmd_show_port_flow_transfer_proxy_port_id,
		(void *)&cmd_show_port_flow_transfer_proxy_flow,
		(void *)&cmd_show_port_flow_transfer_proxy_transfer,
		(void *)&cmd_show_port_flow_transfer_proxy_proxy,
		NULL,
	}
};

/* ******************************************************************************** */

/* list of instructions */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_help_brief,
	(cmdline_parse_inst_t *)&cmd_help_long,
	(cmdline_parse_inst_t *)&cmd_quit,
	(cmdline_parse_inst_t *)&cmd_load_from_file,
	(cmdline_parse_inst_t *)&cmd_showport,
	(cmdline_parse_inst_t *)&cmd_showqueue,
	(cmdline_parse_inst_t *)&cmd_showeeprom,
	(cmdline_parse_inst_t *)&cmd_showportall,
	(cmdline_parse_inst_t *)&cmd_representor_info,
	(cmdline_parse_inst_t *)&cmd_showdevice,
	(cmdline_parse_inst_t *)&cmd_showcfg,
	(cmdline_parse_inst_t *)&cmd_showfwdall,
	(cmdline_parse_inst_t *)&cmd_start,
	(cmdline_parse_inst_t *)&cmd_start_tx_first,
	(cmdline_parse_inst_t *)&cmd_start_tx_first_n,
	(cmdline_parse_inst_t *)&cmd_set_link_up,
	(cmdline_parse_inst_t *)&cmd_set_link_down,
	(cmdline_parse_inst_t *)&cmd_reset,
	(cmdline_parse_inst_t *)&cmd_set_numbers,
	(cmdline_parse_inst_t *)&cmd_set_log,
	(cmdline_parse_inst_t *)&cmd_set_rxoffs,
	(cmdline_parse_inst_t *)&cmd_set_rxpkts,
	(cmdline_parse_inst_t *)&cmd_set_txpkts,
	(cmdline_parse_inst_t *)&cmd_set_txsplit,
	(cmdline_parse_inst_t *)&cmd_set_txtimes,
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
	(cmdline_parse_inst_t *)&cmd_set_bypass_mode,
	(cmdline_parse_inst_t *)&cmd_set_bypass_event,
	(cmdline_parse_inst_t *)&cmd_set_bypass_timeout,
	(cmdline_parse_inst_t *)&cmd_show_bypass_config,
#ifdef RTE_NET_BOND
	(cmdline_parse_inst_t *) &cmd_set_bonding_mode,
	(cmdline_parse_inst_t *) &cmd_show_bonding_config,
	(cmdline_parse_inst_t *) &cmd_show_bonding_lacp_info,
	(cmdline_parse_inst_t *) &cmd_set_bonding_primary,
	(cmdline_parse_inst_t *) &cmd_add_bonding_slave,
	(cmdline_parse_inst_t *) &cmd_remove_bonding_slave,
	(cmdline_parse_inst_t *) &cmd_create_bonded_device,
	(cmdline_parse_inst_t *) &cmd_set_bond_mac_addr,
	(cmdline_parse_inst_t *) &cmd_set_balance_xmit_policy,
	(cmdline_parse_inst_t *) &cmd_set_bond_mon_period,
	(cmdline_parse_inst_t *) &cmd_set_lacp_dedicated_queues,
	(cmdline_parse_inst_t *) &cmd_set_bonding_agg_mode_policy,
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
#ifdef RTE_LIB_GRO
	(cmdline_parse_inst_t *)&cmd_gro_enable,
	(cmdline_parse_inst_t *)&cmd_gro_flush,
	(cmdline_parse_inst_t *)&cmd_gro_show,
#endif
#ifdef RTE_LIB_GSO
	(cmdline_parse_inst_t *)&cmd_gso_enable,
	(cmdline_parse_inst_t *)&cmd_gso_size,
	(cmdline_parse_inst_t *)&cmd_gso_show,
#endif
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_rx,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_tx,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_hw,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_lw,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_pt,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_xon,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_macfwd,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_set_autoneg,
	(cmdline_parse_inst_t *)&cmd_link_flow_control_show,
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
	(cmdline_parse_inst_t *)&cmd_set_fwd_eth_peer,
	(cmdline_parse_inst_t *)&cmd_set_qmap,
	(cmdline_parse_inst_t *)&cmd_set_xstats_hide_zero,
	(cmdline_parse_inst_t *)&cmd_set_record_core_cycles,
	(cmdline_parse_inst_t *)&cmd_set_record_burst_stats,
	(cmdline_parse_inst_t *)&cmd_operate_port,
	(cmdline_parse_inst_t *)&cmd_operate_specific_port,
	(cmdline_parse_inst_t *)&cmd_operate_attach_port,
	(cmdline_parse_inst_t *)&cmd_operate_detach_port,
	(cmdline_parse_inst_t *)&cmd_operate_detach_device,
	(cmdline_parse_inst_t *)&cmd_set_port_setup_on,
	(cmdline_parse_inst_t *)&cmd_config_speed_all,
	(cmdline_parse_inst_t *)&cmd_config_speed_specific,
	(cmdline_parse_inst_t *)&cmd_config_loopback_all,
	(cmdline_parse_inst_t *)&cmd_config_loopback_specific,
	(cmdline_parse_inst_t *)&cmd_config_rx_tx,
	(cmdline_parse_inst_t *)&cmd_config_mtu,
	(cmdline_parse_inst_t *)&cmd_config_max_pkt_len,
	(cmdline_parse_inst_t *)&cmd_config_max_lro_pkt_size,
	(cmdline_parse_inst_t *)&cmd_config_rx_mode_flag,
	(cmdline_parse_inst_t *)&cmd_config_rss,
	(cmdline_parse_inst_t *)&cmd_config_rxtx_ring_size,
	(cmdline_parse_inst_t *)&cmd_config_rxtx_queue,
	(cmdline_parse_inst_t *)&cmd_config_deferred_start_rxtx_queue,
	(cmdline_parse_inst_t *)&cmd_setup_rxtx_queue,
	(cmdline_parse_inst_t *)&cmd_config_rss_reta,
	(cmdline_parse_inst_t *)&cmd_showport_reta,
	(cmdline_parse_inst_t *)&cmd_showport_macs,
	(cmdline_parse_inst_t *)&cmd_show_port_flow_transfer_proxy,
	(cmdline_parse_inst_t *)&cmd_config_burst,
	(cmdline_parse_inst_t *)&cmd_config_thresh,
	(cmdline_parse_inst_t *)&cmd_config_threshold,
	(cmdline_parse_inst_t *)&cmd_set_uc_hash_filter,
	(cmdline_parse_inst_t *)&cmd_set_uc_all_hash_filter,
	(cmdline_parse_inst_t *)&cmd_vf_mac_addr_filter,
	(cmdline_parse_inst_t *)&cmd_queue_rate_limit,
	(cmdline_parse_inst_t *)&cmd_tunnel_udp_config,
	(cmdline_parse_inst_t *)&cmd_showport_rss_hash,
	(cmdline_parse_inst_t *)&cmd_showport_rss_hash_key,
	(cmdline_parse_inst_t *)&cmd_config_rss_hash_key,
	(cmdline_parse_inst_t *)&cmd_cleanup_txq_mbufs,
	(cmdline_parse_inst_t *)&cmd_dump,
	(cmdline_parse_inst_t *)&cmd_dump_one,
#ifdef RTE_NET_I40E
	(cmdline_parse_inst_t *)&cmd_add_del_raw_flow_director,
#endif
	(cmdline_parse_inst_t *)&cmd_set_flow_director_ip_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_mac_vlan_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_tunnel_mask,
	(cmdline_parse_inst_t *)&cmd_set_flow_director_flex_payload,
	(cmdline_parse_inst_t *)&cmd_flow,
	(cmdline_parse_inst_t *)&cmd_show_port_meter_cap,
	(cmdline_parse_inst_t *)&cmd_add_port_meter_profile_srtcm,
	(cmdline_parse_inst_t *)&cmd_add_port_meter_profile_trtcm,
	(cmdline_parse_inst_t *)&cmd_add_port_meter_profile_trtcm_rfc4115,
	(cmdline_parse_inst_t *)&cmd_del_port_meter_profile,
	(cmdline_parse_inst_t *)&cmd_create_port_meter,
	(cmdline_parse_inst_t *)&cmd_enable_port_meter,
	(cmdline_parse_inst_t *)&cmd_disable_port_meter,
	(cmdline_parse_inst_t *)&cmd_del_port_meter,
	(cmdline_parse_inst_t *)&cmd_del_port_meter_policy,
	(cmdline_parse_inst_t *)&cmd_set_port_meter_profile,
	(cmdline_parse_inst_t *)&cmd_set_port_meter_dscp_table,
	(cmdline_parse_inst_t *)&cmd_set_port_meter_stats_mask,
	(cmdline_parse_inst_t *)&cmd_show_port_meter_stats,
	(cmdline_parse_inst_t *)&cmd_mcast_addr,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_anti_spoof,
	(cmdline_parse_inst_t *)&cmd_set_vf_mac_anti_spoof,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_stripq,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_insert,
	(cmdline_parse_inst_t *)&cmd_set_tx_loopback,
	(cmdline_parse_inst_t *)&cmd_set_all_queues_drop_en,
	(cmdline_parse_inst_t *)&cmd_set_vf_split_drop_en,
	(cmdline_parse_inst_t *)&cmd_set_macsec_offload_on,
	(cmdline_parse_inst_t *)&cmd_set_macsec_offload_off,
	(cmdline_parse_inst_t *)&cmd_set_macsec_sc,
	(cmdline_parse_inst_t *)&cmd_set_macsec_sa,
	(cmdline_parse_inst_t *)&cmd_set_vf_traffic,
	(cmdline_parse_inst_t *)&cmd_set_vf_rxmode,
	(cmdline_parse_inst_t *)&cmd_vf_rate_limit,
	(cmdline_parse_inst_t *)&cmd_vf_rxvlan_filter,
	(cmdline_parse_inst_t *)&cmd_set_vf_mac_addr,
	(cmdline_parse_inst_t *)&cmd_set_vf_promisc,
	(cmdline_parse_inst_t *)&cmd_set_vf_allmulti,
	(cmdline_parse_inst_t *)&cmd_set_vf_broadcast,
	(cmdline_parse_inst_t *)&cmd_set_vf_vlan_tag,
	(cmdline_parse_inst_t *)&cmd_vf_max_bw,
	(cmdline_parse_inst_t *)&cmd_vf_tc_min_bw,
	(cmdline_parse_inst_t *)&cmd_vf_tc_max_bw,
	(cmdline_parse_inst_t *)&cmd_strict_link_prio,
	(cmdline_parse_inst_t *)&cmd_tc_min_bw,
	(cmdline_parse_inst_t *)&cmd_set_vxlan,
	(cmdline_parse_inst_t *)&cmd_set_vxlan_tos_ttl,
	(cmdline_parse_inst_t *)&cmd_set_vxlan_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_nvgre,
	(cmdline_parse_inst_t *)&cmd_set_nvgre_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_l2_encap,
	(cmdline_parse_inst_t *)&cmd_set_l2_encap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_l2_decap,
	(cmdline_parse_inst_t *)&cmd_set_l2_decap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_mplsogre_encap,
	(cmdline_parse_inst_t *)&cmd_set_mplsogre_encap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_mplsogre_decap,
	(cmdline_parse_inst_t *)&cmd_set_mplsogre_decap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_mplsoudp_encap,
	(cmdline_parse_inst_t *)&cmd_set_mplsoudp_encap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_mplsoudp_decap,
	(cmdline_parse_inst_t *)&cmd_set_mplsoudp_decap_with_vlan,
	(cmdline_parse_inst_t *)&cmd_set_conntrack_common,
	(cmdline_parse_inst_t *)&cmd_set_conntrack_dir,
	(cmdline_parse_inst_t *)&cmd_ddp_add,
	(cmdline_parse_inst_t *)&cmd_ddp_del,
	(cmdline_parse_inst_t *)&cmd_ddp_get_list,
	(cmdline_parse_inst_t *)&cmd_ddp_get_info,
	(cmdline_parse_inst_t *)&cmd_cfg_input_set,
	(cmdline_parse_inst_t *)&cmd_clear_input_set,
	(cmdline_parse_inst_t *)&cmd_show_vf_stats,
	(cmdline_parse_inst_t *)&cmd_clear_vf_stats,
	(cmdline_parse_inst_t *)&cmd_show_port_supported_ptypes,
	(cmdline_parse_inst_t *)&cmd_set_port_ptypes,
	(cmdline_parse_inst_t *)&cmd_ptype_mapping_get,
	(cmdline_parse_inst_t *)&cmd_ptype_mapping_replace,
	(cmdline_parse_inst_t *)&cmd_ptype_mapping_reset,
	(cmdline_parse_inst_t *)&cmd_ptype_mapping_update,

	(cmdline_parse_inst_t *)&cmd_pctype_mapping_get,
	(cmdline_parse_inst_t *)&cmd_pctype_mapping_reset,
	(cmdline_parse_inst_t *)&cmd_pctype_mapping_update,
	(cmdline_parse_inst_t *)&cmd_queue_region,
	(cmdline_parse_inst_t *)&cmd_region_flowtype,
	(cmdline_parse_inst_t *)&cmd_user_priority_region,
	(cmdline_parse_inst_t *)&cmd_flush_queue_region,
	(cmdline_parse_inst_t *)&cmd_show_queue_region_info_all,
	(cmdline_parse_inst_t *)&cmd_show_port_tm_cap,
	(cmdline_parse_inst_t *)&cmd_show_port_tm_level_cap,
	(cmdline_parse_inst_t *)&cmd_show_port_tm_node_cap,
	(cmdline_parse_inst_t *)&cmd_show_port_tm_node_type,
	(cmdline_parse_inst_t *)&cmd_show_port_tm_node_stats,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_node_shaper_profile,
	(cmdline_parse_inst_t *)&cmd_del_port_tm_node_shaper_profile,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_node_shared_shaper,
	(cmdline_parse_inst_t *)&cmd_del_port_tm_node_shared_shaper,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_node_wred_profile,
	(cmdline_parse_inst_t *)&cmd_del_port_tm_node_wred_profile,
	(cmdline_parse_inst_t *)&cmd_set_port_tm_node_shaper_profile,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_nonleaf_node,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_nonleaf_node_pmode,
	(cmdline_parse_inst_t *)&cmd_add_port_tm_leaf_node,
	(cmdline_parse_inst_t *)&cmd_del_port_tm_node,
	(cmdline_parse_inst_t *)&cmd_set_port_tm_node_parent,
	(cmdline_parse_inst_t *)&cmd_suspend_port_tm_node,
	(cmdline_parse_inst_t *)&cmd_resume_port_tm_node,
	(cmdline_parse_inst_t *)&cmd_port_tm_hierarchy_commit,
	(cmdline_parse_inst_t *)&cmd_port_tm_mark_ip_ecn,
	(cmdline_parse_inst_t *)&cmd_port_tm_mark_ip_dscp,
	(cmdline_parse_inst_t *)&cmd_port_tm_mark_vlan_dei,
	(cmdline_parse_inst_t *)&cmd_cfg_tunnel_udp_port,
	(cmdline_parse_inst_t *)&cmd_rx_offload_get_capa,
	(cmdline_parse_inst_t *)&cmd_rx_offload_get_configuration,
	(cmdline_parse_inst_t *)&cmd_config_per_port_rx_offload,
	(cmdline_parse_inst_t *)&cmd_config_per_queue_rx_offload,
	(cmdline_parse_inst_t *)&cmd_tx_offload_get_capa,
	(cmdline_parse_inst_t *)&cmd_tx_offload_get_configuration,
	(cmdline_parse_inst_t *)&cmd_config_per_port_tx_offload,
	(cmdline_parse_inst_t *)&cmd_config_per_queue_tx_offload,
#ifdef RTE_LIB_BPF
	(cmdline_parse_inst_t *)&cmd_operate_bpf_ld_parse,
	(cmdline_parse_inst_t *)&cmd_operate_bpf_unld_parse,
#endif
	(cmdline_parse_inst_t *)&cmd_config_tx_metadata_specific,
	(cmdline_parse_inst_t *)&cmd_show_tx_metadata,
	(cmdline_parse_inst_t *)&cmd_show_rx_tx_desc_status,
	(cmdline_parse_inst_t *)&cmd_show_rx_queue_desc_used_count,
	(cmdline_parse_inst_t *)&cmd_set_raw,
	(cmdline_parse_inst_t *)&cmd_show_set_raw,
	(cmdline_parse_inst_t *)&cmd_show_set_raw_all,
	(cmdline_parse_inst_t *)&cmd_config_tx_dynf_specific,
	(cmdline_parse_inst_t *)&cmd_show_fec_mode,
	(cmdline_parse_inst_t *)&cmd_set_fec_mode,
	(cmdline_parse_inst_t *)&cmd_show_capability,
	(cmdline_parse_inst_t *)&cmd_set_flex_is_pattern,
	(cmdline_parse_inst_t *)&cmd_set_flex_spec_pattern,
	NULL,
};

/* read cmdline commands from file */
void
cmdline_read_from_file(const char *filename)
{
	struct cmdline *cl;

	cl = cmdline_file_new(main_ctx, "testpmd> ", filename);
	if (cl == NULL) {
		fprintf(stderr,
			"Failed to create file based cmdline context: %s\n",
			filename);
		return;
	}

	cmdline_interact(cl);
	cmdline_quit(cl);

	cmdline_free(cl);

	printf("Read CLI commands from %s\n", filename);
}

/* prompt function, called from main on MAIN lcore */
void
prompt(void)
{
	int ret;
	/* initialize non-constant commands */
	cmd_set_fwd_mode_init();
	cmd_set_fwd_retry_mode_init();

	testpmd_cl = cmdline_stdin_new(main_ctx, "testpmd> ");
	if (testpmd_cl == NULL)
		return;

	ret = atexit(prompt_exit);
	if (ret != 0)
		fprintf(stderr, "Cannot set exit function for cmdline\n");

	cmdline_interact(testpmd_cl);
	if (ret != 0)
		cmdline_stdin_exit(testpmd_cl);
}

void
prompt_exit(void)
{
	if (testpmd_cl != NULL) {
		cmdline_quit(testpmd_cl);
		cmdline_stdin_exit(testpmd_cl);
	}
}

static void
cmd_reconfig_device_queue(portid_t id, uint8_t dev, uint8_t queue)
{
	if (id == (portid_t)RTE_PORT_ALL) {
		portid_t pid;

		RTE_ETH_FOREACH_DEV(pid) {
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
