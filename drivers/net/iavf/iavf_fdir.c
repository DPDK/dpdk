/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "iavf.h"
#include "iavf_generic_flow.h"
#include "virtchnl.h"
#include "iavf_rxtx.h"

#define IAVF_FDIR_MAX_QREGION_SIZE 128

#define IAVF_FDIR_IPV6_TC_OFFSET 20
#define IAVF_IPV6_TC_MASK  (0xFF << IAVF_FDIR_IPV6_TC_OFFSET)

#define IAVF_GTPU_EH_DWLINK 0
#define IAVF_GTPU_EH_UPLINK 1

#define IAVF_FDIR_INSET_ETH (\
	IAVF_INSET_ETHERTYPE)

#define IAVF_FDIR_INSET_ETH_IPV4 (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_IPV4_PROTO | IAVF_INSET_IPV4_TOS | \
	IAVF_INSET_IPV4_TTL | IAVF_INSET_IPV4_ID)

#define IAVF_FDIR_INSET_ETH_IPV4_UDP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_IPV4_TOS | IAVF_INSET_IPV4_TTL | \
	IAVF_INSET_UDP_SRC_PORT | IAVF_INSET_UDP_DST_PORT)

#define IAVF_FDIR_INSET_ETH_IPV4_TCP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_IPV4_TOS | IAVF_INSET_IPV4_TTL | \
	IAVF_INSET_TCP_SRC_PORT | IAVF_INSET_TCP_DST_PORT)

#define IAVF_FDIR_INSET_ETH_IPV4_SCTP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_IPV4_TOS | IAVF_INSET_IPV4_TTL | \
	IAVF_INSET_SCTP_SRC_PORT | IAVF_INSET_SCTP_DST_PORT)

#define IAVF_FDIR_INSET_ETH_IPV6 (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_IPV6_NEXT_HDR | IAVF_INSET_IPV6_TC | \
	IAVF_INSET_IPV6_HOP_LIMIT)

#define IAVF_FDIR_INSET_ETH_IPV6_FRAG_EXT (\
	IAVF_FDIR_INSET_ETH_IPV6 | IAVF_INSET_IPV6_ID)

#define IAVF_FDIR_INSET_ETH_IPV6_UDP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_IPV6_TC | IAVF_INSET_IPV6_HOP_LIMIT | \
	IAVF_INSET_UDP_SRC_PORT | IAVF_INSET_UDP_DST_PORT)

#define IAVF_FDIR_INSET_ETH_IPV6_TCP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_IPV6_TC | IAVF_INSET_IPV6_HOP_LIMIT | \
	IAVF_INSET_TCP_SRC_PORT | IAVF_INSET_TCP_DST_PORT)

#define IAVF_FDIR_INSET_ETH_IPV6_SCTP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_IPV6_TC | IAVF_INSET_IPV6_HOP_LIMIT | \
	IAVF_INSET_SCTP_SRC_PORT | IAVF_INSET_SCTP_DST_PORT)

#define IAVF_FDIR_INSET_IPV4_GTPU (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_GTPU_TEID)

#define IAVF_FDIR_INSET_GTPU_IPV4 (\
	IAVF_INSET_TUN_IPV4_SRC | IAVF_INSET_TUN_IPV4_DST | \
	IAVF_INSET_TUN_IPV4_PROTO | IAVF_INSET_TUN_IPV4_TOS | \
	IAVF_INSET_TUN_IPV4_TTL)

#define IAVF_FDIR_INSET_GTPU_IPV4_UDP (\
	IAVF_FDIR_INSET_GTPU_IPV4 | \
	IAVF_INSET_TUN_UDP_SRC_PORT | IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_GTPU_IPV4_TCP (\
	IAVF_FDIR_INSET_GTPU_IPV4 | \
	IAVF_INSET_TUN_TCP_SRC_PORT | IAVF_INSET_TUN_TCP_DST_PORT)

#define IAVF_FDIR_INSET_IPV4_GTPU_EH (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_GTPU_TEID | IAVF_INSET_GTPU_QFI)

#define IAVF_FDIR_INSET_IPV6_GTPU (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_GTPU_TEID)

#define IAVF_FDIR_INSET_GTPU_IPV6 (\
	IAVF_INSET_TUN_IPV6_SRC | IAVF_INSET_TUN_IPV6_DST | \
	IAVF_INSET_TUN_IPV6_NEXT_HDR | IAVF_INSET_TUN_IPV6_TC | \
	IAVF_INSET_TUN_IPV6_HOP_LIMIT)

#define IAVF_FDIR_INSET_GTPU_IPV6_UDP (\
	IAVF_FDIR_INSET_GTPU_IPV6 | \
	IAVF_INSET_TUN_UDP_SRC_PORT | IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_GTPU_IPV6_TCP (\
	IAVF_FDIR_INSET_GTPU_IPV6 | \
	IAVF_INSET_TUN_TCP_SRC_PORT | IAVF_INSET_TUN_TCP_DST_PORT)

#define IAVF_FDIR_INSET_IPV6_GTPU_EH (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_GTPU_TEID | IAVF_INSET_GTPU_QFI)

#define IAVF_FDIR_INSET_L2TPV3OIP (\
	IAVF_L2TPV3OIP_SESSION_ID)

#define IAVF_FDIR_INSET_IPV4_ESP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_ESP_SPI)

#define IAVF_FDIR_INSET_IPV6_ESP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_ESP_SPI)

#define IAVF_FDIR_INSET_AH (\
	IAVF_INSET_AH_SPI)

#define IAVF_FDIR_INSET_IPV4_NATT_ESP (\
	IAVF_INSET_IPV4_SRC | IAVF_INSET_IPV4_DST | \
	IAVF_INSET_ESP_SPI)

#define IAVF_FDIR_INSET_IPV6_NATT_ESP (\
	IAVF_INSET_IPV6_SRC | IAVF_INSET_IPV6_DST | \
	IAVF_INSET_ESP_SPI)

#define IAVF_FDIR_INSET_PFCP (\
	IAVF_INSET_PFCP_S_FIELD)

#define IAVF_FDIR_INSET_ECPRI (\
	IAVF_INSET_ECPRI)

#define IAVF_FDIR_INSET_GRE_IPV4 (\
	IAVF_INSET_TUN_IPV4_SRC | IAVF_INSET_TUN_IPV4_DST | \
	IAVF_INSET_TUN_IPV4_TOS | IAVF_INSET_TUN_IPV4_PROTO)

#define IAVF_FDIR_INSET_GRE_IPV4_TCP (\
	IAVF_FDIR_INSET_GRE_IPV4 | IAVF_INSET_TUN_TCP_SRC_PORT | \
	IAVF_INSET_TUN_TCP_DST_PORT)

#define IAVF_FDIR_INSET_GRE_IPV4_UDP (\
	IAVF_FDIR_INSET_GRE_IPV4 | IAVF_INSET_TUN_UDP_SRC_PORT | \
	IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_GRE_IPV6 (\
	IAVF_INSET_TUN_IPV6_SRC | IAVF_INSET_TUN_IPV6_DST | \
	IAVF_INSET_TUN_IPV6_TC | IAVF_INSET_TUN_IPV6_NEXT_HDR)

#define IAVF_FDIR_INSET_GRE_IPV6_TCP (\
	IAVF_FDIR_INSET_GRE_IPV6 | IAVF_INSET_TUN_TCP_SRC_PORT | \
	IAVF_INSET_TUN_TCP_DST_PORT)

#define IAVF_FDIR_INSET_GRE_IPV6_UDP (\
	IAVF_FDIR_INSET_GRE_IPV6 | IAVF_INSET_TUN_UDP_SRC_PORT | \
	IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_L2TPV2 (\
	IAVF_INSET_SMAC | IAVF_INSET_DMAC | IAVF_INSET_L2TPV2)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV4 (\
	IAVF_INSET_TUN_IPV4_SRC | IAVF_INSET_TUN_IPV4_DST)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_UDP (\
	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4 | IAVF_INSET_TUN_UDP_SRC_PORT | \
	IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_TCP (\
	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4 | IAVF_INSET_TUN_TCP_SRC_PORT | \
	IAVF_INSET_TUN_TCP_DST_PORT)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV6 (\
	IAVF_INSET_TUN_IPV6_SRC | IAVF_INSET_TUN_IPV6_DST)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_UDP (\
	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6 | IAVF_INSET_TUN_UDP_SRC_PORT | \
	IAVF_INSET_TUN_UDP_DST_PORT)

#define IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_TCP (\
	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6 | IAVF_INSET_TUN_TCP_SRC_PORT | \
	IAVF_INSET_TUN_TCP_DST_PORT)

static struct iavf_pattern_match_item iavf_fdir_pattern[] = {
	{iavf_pattern_ethertype,		 IAVF_FDIR_INSET_ETH,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4,			 IAVF_FDIR_INSET_ETH_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp,		 IAVF_FDIR_INSET_ETH_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_tcp,		 IAVF_FDIR_INSET_ETH_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_sctp,		 IAVF_FDIR_INSET_ETH_IPV4_SCTP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6,			 IAVF_FDIR_INSET_ETH_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_frag_ext,	IAVF_FDIR_INSET_ETH_IPV6_FRAG_EXT,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp,		 IAVF_FDIR_INSET_ETH_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_tcp,		 IAVF_FDIR_INSET_ETH_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_sctp,		 IAVF_FDIR_INSET_ETH_IPV6_SCTP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu,		 IAVF_FDIR_INSET_IPV4_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh,		 IAVF_FDIR_INSET_IPV4_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_udp, IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_tcp, IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6_udp, IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6_tcp, IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu,		 IAVF_FDIR_INSET_IPV4_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu,		 IAVF_FDIR_INSET_IPV4_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu,		 IAVF_FDIR_INSET_IPV6_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu,		 IAVF_FDIR_INSET_IPV6_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4,	 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6,	 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh,		 IAVF_FDIR_INSET_IPV4_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4,		 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6,		 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh,		 IAVF_FDIR_INSET_IPV4_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4,		 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6,		 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh,		 IAVF_FDIR_INSET_IPV6_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4,		 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6,		 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh,		 IAVF_FDIR_INSET_IPV6_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4,		 IAVF_FDIR_INSET_GTPU_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4_udp,	 IAVF_FDIR_INSET_GTPU_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4_tcp,	 IAVF_FDIR_INSET_GTPU_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6,		 IAVF_FDIR_INSET_GTPU_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6_udp,	 IAVF_FDIR_INSET_GTPU_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6_tcp,	 IAVF_FDIR_INSET_GTPU_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gtpu,		 IAVF_FDIR_INSET_IPV6_GTPU,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gtpu_eh,		 IAVF_FDIR_INSET_IPV6_GTPU_EH,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_l2tpv3,		 IAVF_FDIR_INSET_L2TPV3OIP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_l2tpv3,		 IAVF_FDIR_INSET_L2TPV3OIP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_esp,		 IAVF_FDIR_INSET_IPV4_ESP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_esp,		 IAVF_FDIR_INSET_IPV6_ESP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_ah,		 IAVF_FDIR_INSET_AH,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_ah,		 IAVF_FDIR_INSET_AH,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_esp,		 IAVF_FDIR_INSET_IPV4_NATT_ESP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_esp,		 IAVF_FDIR_INSET_IPV6_NATT_ESP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_pfcp,		 IAVF_FDIR_INSET_PFCP,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_pfcp,		 IAVF_FDIR_INSET_PFCP,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ecpri,		 IAVF_FDIR_INSET_ECPRI,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_ecpri,		 IAVF_FDIR_INSET_ECPRI,		IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4,	IAVF_FDIR_INSET_GRE_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_tcp,	IAVF_FDIR_INSET_GRE_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv4_udp,	IAVF_FDIR_INSET_GRE_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6,	IAVF_FDIR_INSET_GRE_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_tcp,	IAVF_FDIR_INSET_GRE_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_gre_ipv6_udp,	IAVF_FDIR_INSET_GRE_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4,	IAVF_FDIR_INSET_GRE_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_tcp,	IAVF_FDIR_INSET_GRE_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv4_udp,	IAVF_FDIR_INSET_GRE_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6,	IAVF_FDIR_INSET_GRE_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_tcp,	IAVF_FDIR_INSET_GRE_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_gre_ipv6_udp,	IAVF_FDIR_INSET_GRE_IPV6_UDP,	IAVF_INSET_NONE},

	{iavf_pattern_eth_ipv4_udp_l2tpv2,		IAVF_FDIR_INSET_L2TPV2,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp,		IAVF_FDIR_INSET_L2TPV2,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2,		IAVF_FDIR_INSET_L2TPV2,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp,		IAVF_FDIR_INSET_L2TPV2,			IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4_udp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4_tcp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4_udp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4_tcp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV4_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6_udp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6_tcp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_TCP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6_udp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_UDP,	IAVF_INSET_NONE},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6_tcp,	IAVF_FDIR_INSET_L2TPV2_PPP_IPV6_TCP,	IAVF_INSET_NONE},
};

static struct iavf_flow_parser iavf_fdir_parser;

static int
iavf_fdir_init(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_parser *parser;

	if (!vf->vf_res)
		return -EINVAL;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_FDIR_PF)
		parser = &iavf_fdir_parser;
	else
		return -ENOTSUP;

	return iavf_register_parser(parser, ad);
}

static void
iavf_fdir_uninit(struct iavf_adapter *ad)
{
	iavf_unregister_parser(&iavf_fdir_parser, ad);
}

static int
iavf_fdir_create(struct iavf_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	struct iavf_fdir_conf *filter = meta;
	struct iavf_fdir_conf *rule;
	int ret;

	rule = rte_zmalloc("fdir_entry", sizeof(*rule), 0);
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to allocate memory for fdir rule");
		return -rte_errno;
	}

	ret = iavf_fdir_add(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to add filter rule.");
		goto free_entry;
	}

	if (filter->mark_flag == 1)
		iavf_fdir_rx_proc_enable(ad, 1);

	rte_memcpy(rule, filter, sizeof(*rule));
	flow->rule = rule;

	return 0;

free_entry:
	rte_free(rule);
	return -rte_errno;
}

static int
iavf_fdir_destroy(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct iavf_fdir_conf *filter;
	int ret;

	filter = (struct iavf_fdir_conf *)flow->rule;

	ret = iavf_fdir_del(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to delete filter rule.");
		return -rte_errno;
	}

	if (filter->mark_flag == 1)
		iavf_fdir_rx_proc_enable(ad, 0);

	flow->rule = NULL;
	rte_free(filter);

	return 0;
}

static int
iavf_fdir_validation(struct iavf_adapter *ad,
		__rte_unused struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	struct iavf_fdir_conf *filter = meta;
	int ret;

	ret = iavf_fdir_check(ad, filter);
	if (ret) {
		rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to validate filter rule.");
		return -rte_errno;
	}

	return 0;
};

static struct iavf_flow_engine iavf_fdir_engine = {
	.init = iavf_fdir_init,
	.uninit = iavf_fdir_uninit,
	.create = iavf_fdir_create,
	.destroy = iavf_fdir_destroy,
	.validation = iavf_fdir_validation,
	.type = IAVF_FLOW_ENGINE_FDIR,
};

static int
iavf_fdir_parse_action_qregion(struct iavf_adapter *ad,
			struct rte_flow_error *error,
			const struct rte_flow_action *act,
			struct virtchnl_filter_action *filter_action)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	const struct rte_flow_action_rss *rss = act->conf;
	uint32_t i;

	if (act->type != RTE_FLOW_ACTION_TYPE_RSS) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, act,
				"Invalid action.");
		return -rte_errno;
	}

	if (rss->queue_num <= 1) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, act,
				"Queue region size can't be 0 or 1.");
		return -rte_errno;
	}

	/* check if queue index for queue region is continuous */
	for (i = 0; i < rss->queue_num - 1; i++) {
		if (rss->queue[i + 1] != rss->queue[i] + 1) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, act,
					"Discontinuous queue region");
			return -rte_errno;
		}
	}

	if (rss->queue[rss->queue_num - 1] >= ad->dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, act,
				"Invalid queue region indexes.");
		return -rte_errno;
	}

	if (!(rte_is_power_of_2(rss->queue_num) &&
		rss->queue_num <= IAVF_FDIR_MAX_QREGION_SIZE)) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, act,
				"The region size should be any of the following values:"
				"1, 2, 4, 8, 16, 32, 64, 128 as long as the total number "
				"of queues do not exceed the VSI allocation.");
		return -rte_errno;
	}

	if (rss->queue_num > vf->max_rss_qregion) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, act,
				"The region size cannot be large than the supported max RSS queue region");
		return -rte_errno;
	}

	filter_action->act_conf.queue.index = rss->queue[0];
	filter_action->act_conf.queue.region = rte_fls_u32(rss->queue_num) - 1;

	return 0;
}

static int
iavf_fdir_parse_action(struct iavf_adapter *ad,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct iavf_fdir_conf *filter)
{
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark_spec = NULL;
	uint32_t dest_num = 0;
	uint32_t mark_num = 0;
	int ret;

	int number = 0;
	struct virtchnl_filter_action *filter_action;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		case RTE_FLOW_ACTION_TYPE_PASSTHRU:
			dest_num++;

			filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];

			filter_action->type = VIRTCHNL_ACTION_PASSTHRU;

			filter->add_fltr.rule_cfg.action_set.count = ++number;
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			dest_num++;

			filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];

			filter_action->type = VIRTCHNL_ACTION_DROP;

			filter->add_fltr.rule_cfg.action_set.count = ++number;
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_num++;

			act_q = actions->conf;
			filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];

			filter_action->type = VIRTCHNL_ACTION_QUEUE;
			filter_action->act_conf.queue.index = act_q->index;

			if (filter_action->act_conf.queue.index >=
				ad->dev_data->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions, "Invalid queue for FDIR.");
				return -rte_errno;
			}

			filter->add_fltr.rule_cfg.action_set.count = ++number;
			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			dest_num++;

			filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];

			filter_action->type = VIRTCHNL_ACTION_Q_REGION;

			ret = iavf_fdir_parse_action_qregion(ad,
						error, actions, filter_action);
			if (ret)
				return ret;

			filter->add_fltr.rule_cfg.action_set.count = ++number;
			break;

		case RTE_FLOW_ACTION_TYPE_MARK:
			mark_num++;

			filter->mark_flag = 1;
			mark_spec = actions->conf;
			filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];

			filter_action->type = VIRTCHNL_ACTION_MARK;
			filter_action->act_conf.mark_id = mark_spec->id;

			filter->add_fltr.rule_cfg.action_set.count = ++number;
			break;

		default:
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, actions,
					"Invalid action.");
			return -rte_errno;
		}
	}

	if (number > VIRTCHNL_MAX_NUM_ACTIONS) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, actions,
			"Action numbers exceed the maximum value");
		return -rte_errno;
	}

	if (dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, actions,
			"Unsupported action combination");
		return -rte_errno;
	}

	if (mark_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, actions,
			"Too many mark actions");
		return -rte_errno;
	}

	if (dest_num + mark_num == 0) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, actions,
			"Empty action");
		return -rte_errno;
	}

	/* Mark only is equal to mark + passthru. */
	if (dest_num == 0) {
		filter_action = &filter->add_fltr.rule_cfg.action_set.actions[number];
		filter_action->type = VIRTCHNL_ACTION_PASSTHRU;
		filter->add_fltr.rule_cfg.action_set.count = ++number;
	}

	return 0;
}

static bool
iavf_fdir_refine_input_set(const uint64_t input_set,
			   const uint64_t input_set_mask,
			   struct iavf_fdir_conf *filter)
{
	struct virtchnl_proto_hdr *hdr, *hdr_last;
	struct rte_flow_item_ipv4 ipv4_spec;
	struct rte_flow_item_ipv6 ipv6_spec;
	int last_layer;
	uint8_t proto_id;

	if (input_set & ~input_set_mask)
		return false;
	else if (input_set)
		return true;

	last_layer = filter->add_fltr.rule_cfg.proto_hdrs.count - 1;
	/* Last layer of TCP/UDP pattern isn't less than 2. */
	if (last_layer < 2)
		return false;
	hdr_last = &filter->add_fltr.rule_cfg.proto_hdrs.proto_hdr[last_layer];
	if (hdr_last->type == VIRTCHNL_PROTO_HDR_TCP)
		proto_id = 6;
	else if (hdr_last->type == VIRTCHNL_PROTO_HDR_UDP)
		proto_id = 17;
	else
		return false;

	hdr = &filter->add_fltr.rule_cfg.proto_hdrs.proto_hdr[last_layer - 1];
	switch (hdr->type) {
	case VIRTCHNL_PROTO_HDR_IPV4:
		VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4, PROT);
		memset(&ipv4_spec, 0, sizeof(ipv4_spec));
		ipv4_spec.hdr.next_proto_id = proto_id;
		rte_memcpy(hdr->buffer, &ipv4_spec.hdr,
			   sizeof(ipv4_spec.hdr));
		return true;
	case VIRTCHNL_PROTO_HDR_IPV6:
		VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6, PROT);
		memset(&ipv6_spec, 0, sizeof(ipv6_spec));
		ipv6_spec.hdr.proto = proto_id;
		rte_memcpy(hdr->buffer, &ipv6_spec.hdr,
			   sizeof(ipv6_spec.hdr));
		return true;
	default:
		return false;
	}
}

static void
iavf_fdir_add_fragment_hdr(struct virtchnl_proto_hdrs *hdrs, int layer)
{
	struct virtchnl_proto_hdr *hdr1;
	struct virtchnl_proto_hdr *hdr2;
	int i;

	if (layer < 0 || layer > hdrs->count)
		return;

	/* shift headers layer */
	for (i = hdrs->count; i >= layer; i--) {
		hdr1 = &hdrs->proto_hdr[i];
		hdr2 = &hdrs->proto_hdr[i - 1];
		*hdr1 = *hdr2;
	}

	/* adding dummy fragment header */
	hdr1 = &hdrs->proto_hdr[layer];
	VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, IPV4_FRAG);
	hdr1->field_selector = 0;
	hdrs->count = ++layer;
}

static int
iavf_fdir_parse_pattern(__rte_unused struct iavf_adapter *ad,
			const struct rte_flow_item pattern[],
			const uint64_t input_set_mask,
			struct rte_flow_error *error,
			struct iavf_fdir_conf *filter)
{
	struct virtchnl_proto_hdrs *hdrs =
			&filter->add_fltr.rule_cfg.proto_hdrs;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_last, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_ipv6_frag_ext *ipv6_frag_spec;
	const struct rte_flow_item_ipv6_frag_ext *ipv6_frag_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_gtp *gtp_spec, *gtp_mask;
	const struct rte_flow_item_gtp_psc *gtp_psc_spec, *gtp_psc_mask;
	const struct rte_flow_item_l2tpv3oip *l2tpv3oip_spec, *l2tpv3oip_mask;
	const struct rte_flow_item_esp *esp_spec, *esp_mask;
	const struct rte_flow_item_ah *ah_spec, *ah_mask;
	const struct rte_flow_item_pfcp *pfcp_spec, *pfcp_mask;
	const struct rte_flow_item_ecpri *ecpri_spec, *ecpri_mask;
	const struct rte_flow_item_gre *gre_spec, *gre_mask;
	const struct rte_flow_item_l2tpv2 *l2tpv2_spec, *l2tpv2_mask;
	const struct rte_flow_item_ppp *ppp_spec, *ppp_mask;
	const struct rte_flow_item *item = pattern;
	struct virtchnl_proto_hdr *hdr, *hdr1 = NULL;
	struct rte_ecpri_common_hdr ecpri_common;
	uint64_t input_set = IAVF_INSET_NONE;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type next_type;
	uint8_t tun_inner = 0;
	uint16_t ether_type, flags_version;
	int layer = 0;

	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		item_type = item->type;

		if (item->last && !(item_type == RTE_FLOW_ITEM_TYPE_IPV4 ||
				    item_type ==
				    RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Not support range");
		}

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			next_type = (item + 1)->type;

			hdr1 = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, ETH);

			if (next_type == RTE_FLOW_ITEM_TYPE_END &&
			    (!eth_spec || !eth_mask)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "NULL eth spec/mask.");
				return -rte_errno;
			}

			if (eth_spec && eth_mask) {
				if (!rte_is_zero_ether_addr(&eth_mask->dst)) {
					input_set |= IAVF_INSET_DMAC;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr1,
									ETH,
									DST);
				} else if (!rte_is_zero_ether_addr(&eth_mask->src)) {
					input_set |= IAVF_INSET_SMAC;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr1,
									ETH,
									SRC);
				}

				if (eth_mask->type) {
					if (eth_mask->type != RTE_BE16(0xffff)) {
						rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ITEM,
							item, "Invalid type mask.");
						return -rte_errno;
					}

					ether_type = rte_be_to_cpu_16(eth_spec->type);
					if (ether_type == RTE_ETHER_TYPE_IPV4 ||
						ether_type == RTE_ETHER_TYPE_IPV6) {
						rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ITEM,
							item,
							"Unsupported ether_type.");
						return -rte_errno;
					}

					input_set |= IAVF_INSET_ETHERTYPE;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr1, ETH,
									ETHERTYPE);
				}

				rte_memcpy(hdr1->buffer, eth_spec,
					   sizeof(struct rte_ether_hdr));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_last = item->last;
			ipv4_mask = item->mask;
			next_type = (item + 1)->type;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, IPV4);

			if (!(ipv4_spec && ipv4_mask)) {
				hdrs->count = ++layer;
				break;
			}

			if (ipv4_mask->hdr.version_ihl ||
			    ipv4_mask->hdr.total_length ||
			    ipv4_mask->hdr.hdr_checksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv4 mask.");
				return -rte_errno;
			}

			if (ipv4_last &&
			    (ipv4_last->hdr.version_ihl ||
			     ipv4_last->hdr.type_of_service ||
			     ipv4_last->hdr.time_to_live ||
			     ipv4_last->hdr.total_length |
			     ipv4_last->hdr.next_proto_id ||
			     ipv4_last->hdr.hdr_checksum ||
			     ipv4_last->hdr.src_addr ||
			     ipv4_last->hdr.dst_addr)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv4 last.");
				return -rte_errno;
			}

			if (ipv4_mask->hdr.type_of_service ==
			    UINT8_MAX) {
				input_set |= IAVF_INSET_IPV4_TOS;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4,
								 DSCP);
			}

			if (ipv4_mask->hdr.next_proto_id == UINT8_MAX) {
				input_set |= IAVF_INSET_IPV4_PROTO;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4,
								 PROT);
			}

			if (ipv4_mask->hdr.time_to_live == UINT8_MAX) {
				input_set |= IAVF_INSET_IPV4_TTL;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4,
								 TTL);
			}

			if (ipv4_mask->hdr.src_addr == UINT32_MAX) {
				input_set |= IAVF_INSET_IPV4_SRC;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4,
								 SRC);
			}

			if (ipv4_mask->hdr.dst_addr == UINT32_MAX) {
				input_set |= IAVF_INSET_IPV4_DST;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV4,
								 DST);
			}

			if (tun_inner) {
				input_set &= ~IAVF_PROT_IPV4_OUTER;
				input_set |= IAVF_PROT_IPV4_INNER;
			}

			rte_memcpy(hdr->buffer, &ipv4_spec->hdr,
				   sizeof(ipv4_spec->hdr));

			hdrs->count = ++layer;

			/* fragment Ipv4:
			 * spec is 0x2000, mask is 0x2000
			 */
			if (ipv4_spec->hdr.fragment_offset ==
			    rte_cpu_to_be_16(RTE_IPV4_HDR_MF_FLAG) &&
			    ipv4_mask->hdr.fragment_offset ==
			    rte_cpu_to_be_16(RTE_IPV4_HDR_MF_FLAG)) {
				/* all IPv4 fragment packet has the same
				 * ethertype, if the spec and mask is valid,
				 * set ethertype into input set.
				 */
				input_set |= IAVF_INSET_ETHERTYPE;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr1, ETH,
								 ETHERTYPE);

				/* add dummy header for IPv4 Fragment */
				iavf_fdir_add_fragment_hdr(hdrs, layer);
			} else if (ipv4_mask->hdr.packet_id == UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv4 mask.");
				return -rte_errno;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, IPV6);

			if (!(ipv6_spec && ipv6_mask)) {
				hdrs->count = ++layer;
				break;
			}

			if (ipv6_mask->hdr.payload_len) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv6 mask");
				return -rte_errno;
			}

			if ((ipv6_mask->hdr.vtc_flow &
			      rte_cpu_to_be_32(IAVF_IPV6_TC_MASK))
			     == rte_cpu_to_be_32(IAVF_IPV6_TC_MASK)) {
				input_set |= IAVF_INSET_IPV6_TC;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6,
								 TC);
			}

			if (ipv6_mask->hdr.proto == UINT8_MAX) {
				input_set |= IAVF_INSET_IPV6_NEXT_HDR;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6,
								 PROT);
			}

			if (ipv6_mask->hdr.hop_limits == UINT8_MAX) {
				input_set |= IAVF_INSET_IPV6_HOP_LIMIT;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6,
								 HOP_LIMIT);
			}

			if (!memcmp(ipv6_mask->hdr.src_addr, ipv6_addr_mask,
				    RTE_DIM(ipv6_mask->hdr.src_addr))) {
				input_set |= IAVF_INSET_IPV6_SRC;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6,
								 SRC);
			}
			if (!memcmp(ipv6_mask->hdr.dst_addr, ipv6_addr_mask,
				    RTE_DIM(ipv6_mask->hdr.dst_addr))) {
				input_set |= IAVF_INSET_IPV6_DST;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, IPV6,
								 DST);
			}

			if (tun_inner) {
				input_set &= ~IAVF_PROT_IPV6_OUTER;
				input_set |= IAVF_PROT_IPV6_INNER;
			}

			rte_memcpy(hdr->buffer, &ipv6_spec->hdr,
				   sizeof(ipv6_spec->hdr));

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
			ipv6_frag_spec = item->spec;
			ipv6_frag_mask = item->mask;
			next_type = (item + 1)->type;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, IPV6_EH_FRAG);

			if (!(ipv6_frag_spec && ipv6_frag_mask)) {
				hdrs->count = ++layer;
				break;
			}

			/* fragment Ipv6:
			 * spec is 0x1, mask is 0x1
			 */
			if (ipv6_frag_spec->hdr.frag_data ==
			    rte_cpu_to_be_16(1) &&
			    ipv6_frag_mask->hdr.frag_data ==
			    rte_cpu_to_be_16(1)) {
				/* all IPv6 fragment packet has the same
				 * ethertype, if the spec and mask is valid,
				 * set ethertype into input set.
				 */
				input_set |= IAVF_INSET_ETHERTYPE;
				VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr1, ETH,
								 ETHERTYPE);

				rte_memcpy(hdr->buffer, &ipv6_frag_spec->hdr,
					   sizeof(ipv6_frag_spec->hdr));
			} else if (ipv6_frag_mask->hdr.id == UINT32_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv6 mask.");
				return -rte_errno;
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, UDP);

			if (udp_spec && udp_mask) {
				if (udp_mask->hdr.dgram_len ||
					udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port == UINT16_MAX) {
					input_set |= IAVF_INSET_UDP_SRC_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, UDP, SRC_PORT);
				}
				if (udp_mask->hdr.dst_port == UINT16_MAX) {
					input_set |= IAVF_INSET_UDP_DST_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, UDP, DST_PORT);
				}

				if (tun_inner) {
					input_set &= ~IAVF_PROT_UDP_OUTER;
					input_set |= IAVF_PROT_UDP_INNER;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
					rte_memcpy(hdr->buffer,
						&udp_spec->hdr,
						sizeof(udp_spec->hdr));
				else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
					rte_memcpy(hdr->buffer,
						&udp_spec->hdr,
						sizeof(udp_spec->hdr));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, TCP);

			if (tcp_spec && tcp_mask) {
				if (tcp_mask->hdr.sent_seq ||
					tcp_mask->hdr.recv_ack ||
					tcp_mask->hdr.data_off ||
					tcp_mask->hdr.tcp_flags ||
					tcp_mask->hdr.rx_win ||
					tcp_mask->hdr.cksum ||
					tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port == UINT16_MAX) {
					input_set |= IAVF_INSET_TCP_SRC_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, TCP, SRC_PORT);
				}
				if (tcp_mask->hdr.dst_port == UINT16_MAX) {
					input_set |= IAVF_INSET_TCP_DST_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, TCP, DST_PORT);
				}

				if (tun_inner) {
					input_set &= ~IAVF_PROT_TCP_OUTER;
					input_set |= IAVF_PROT_TCP_INNER;
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
					rte_memcpy(hdr->buffer,
						&tcp_spec->hdr,
						sizeof(tcp_spec->hdr));
				else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
					rte_memcpy(hdr->buffer,
						&tcp_spec->hdr,
						sizeof(tcp_spec->hdr));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, SCTP);

			if (sctp_spec && sctp_mask) {
				if (sctp_mask->hdr.cksum) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"Invalid UDP mask");
					return -rte_errno;
				}

				if (sctp_mask->hdr.src_port == UINT16_MAX) {
					input_set |= IAVF_INSET_SCTP_SRC_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, SCTP, SRC_PORT);
				}
				if (sctp_mask->hdr.dst_port == UINT16_MAX) {
					input_set |= IAVF_INSET_SCTP_DST_PORT;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, SCTP, DST_PORT);
				}

				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
					rte_memcpy(hdr->buffer,
						&sctp_spec->hdr,
						sizeof(sctp_spec->hdr));
				else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
					rte_memcpy(hdr->buffer,
						&sctp_spec->hdr,
						sizeof(sctp_spec->hdr));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_GTPU:
			gtp_spec = item->spec;
			gtp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GTPU_IP);

			if (gtp_spec && gtp_mask) {
				if (gtp_mask->v_pt_rsv_flags ||
					gtp_mask->msg_type ||
					gtp_mask->msg_len) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item, "Invalid GTP mask");
					return -rte_errno;
				}

				if (gtp_mask->teid == UINT32_MAX) {
					input_set |= IAVF_INSET_GTPU_TEID;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, GTPU_IP, TEID);
				}

				rte_memcpy(hdr->buffer,
					gtp_spec, sizeof(*gtp_spec));
			}

			tun_inner = 1;

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			gtp_psc_spec = item->spec;
			gtp_psc_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			if (!gtp_psc_spec)
				VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GTPU_EH);
			else if ((gtp_psc_mask->hdr.qfi) &&
				!(gtp_psc_mask->hdr.type))
				VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GTPU_EH);
			else if (gtp_psc_spec->hdr.type == IAVF_GTPU_EH_UPLINK)
				VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GTPU_EH_PDU_UP);
			else if (gtp_psc_spec->hdr.type == IAVF_GTPU_EH_DWLINK)
				VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GTPU_EH_PDU_DWN);

			if (gtp_psc_spec && gtp_psc_mask) {
				if (gtp_psc_mask->hdr.qfi == 0x3F) {
					input_set |= IAVF_INSET_GTPU_QFI;
					if (!gtp_psc_mask->hdr.type)
						VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr,
										 GTPU_EH, QFI);
					else if (gtp_psc_spec->hdr.type ==
								IAVF_GTPU_EH_UPLINK)
						VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr,
										 GTPU_UP, QFI);
					else if (gtp_psc_spec->hdr.type ==
								IAVF_GTPU_EH_DWLINK)
						VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr,
										 GTPU_DWN, QFI);
				}

				rte_memcpy(hdr->buffer, gtp_psc_spec,
					sizeof(*gtp_psc_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
			l2tpv3oip_spec = item->spec;
			l2tpv3oip_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, L2TPV3);

			if (l2tpv3oip_spec && l2tpv3oip_mask) {
				if (l2tpv3oip_mask->session_id == UINT32_MAX) {
					input_set |= IAVF_L2TPV3OIP_SESSION_ID;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, L2TPV3, SESS_ID);
				}

				rte_memcpy(hdr->buffer, l2tpv3oip_spec,
					sizeof(*l2tpv3oip_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_ESP:
			esp_spec = item->spec;
			esp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, ESP);

			if (esp_spec && esp_mask) {
				if (esp_mask->hdr.spi == UINT32_MAX) {
					input_set |= IAVF_INSET_ESP_SPI;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, ESP, SPI);
				}

				rte_memcpy(hdr->buffer, &esp_spec->hdr,
					sizeof(esp_spec->hdr));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_AH:
			ah_spec = item->spec;
			ah_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, AH);

			if (ah_spec && ah_mask) {
				if (ah_mask->spi == UINT32_MAX) {
					input_set |= IAVF_INSET_AH_SPI;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, AH, SPI);
				}

				rte_memcpy(hdr->buffer, ah_spec,
					sizeof(*ah_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_PFCP:
			pfcp_spec = item->spec;
			pfcp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, PFCP);

			if (pfcp_spec && pfcp_mask) {
				if (pfcp_mask->s_field == UINT8_MAX) {
					input_set |= IAVF_INSET_PFCP_S_FIELD;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, PFCP, S_FIELD);
				}

				rte_memcpy(hdr->buffer, pfcp_spec,
					sizeof(*pfcp_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_ECPRI:
			ecpri_spec = item->spec;
			ecpri_mask = item->mask;

			ecpri_common.u32 = rte_be_to_cpu_32(ecpri_spec->hdr.common.u32);

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, ECPRI);

			if (ecpri_spec && ecpri_mask) {
				if (ecpri_common.type == RTE_ECPRI_MSG_TYPE_IQ_DATA &&
						ecpri_mask->hdr.type0.pc_id == UINT16_MAX) {
					input_set |= IAVF_ECPRI_PC_RTC_ID;
					VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, ECPRI,
									 PC_RTC_ID);
				}

				rte_memcpy(hdr->buffer, ecpri_spec,
					sizeof(*ecpri_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_GRE:
			gre_spec = item->spec;
			gre_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, GRE);

			if (gre_spec && gre_mask) {
				rte_memcpy(hdr->buffer, gre_spec,
					   sizeof(*gre_spec));
			}

			tun_inner = 1;

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_L2TPV2:
			l2tpv2_spec = item->spec;
			l2tpv2_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, L2TPV2);

			if (l2tpv2_spec && l2tpv2_mask) {
				flags_version =
					rte_be_to_cpu_16(l2tpv2_spec->hdr.common.flags_version);
				if ((flags_version == RTE_L2TPV2_MSG_TYPE_CONTROL &&
				     l2tpv2_mask->hdr.type3.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA &&
				     l2tpv2_mask->hdr.type7.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_L &&
				     l2tpv2_mask->hdr.type6.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_S &&
				     l2tpv2_mask->hdr.type5.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_O &&
				     l2tpv2_mask->hdr.type4.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_L_S &&
				     l2tpv2_mask->hdr.type3.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_L_O &&
				     l2tpv2_mask->hdr.type2.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_S_O &&
				     l2tpv2_mask->hdr.type1.session_id == UINT16_MAX) ||
				    (flags_version == RTE_L2TPV2_MSG_TYPE_DATA_L_S_O &&
				     l2tpv2_mask->hdr.type0.session_id == UINT16_MAX)) {
					input_set |= IAVF_L2TPV2_SESSION_ID;
					if (flags_version & IAVF_L2TPV2_FLAGS_LEN)
						VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr,
								L2TPV2,
								LEN_SESS_ID);
					else
						VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr,
								L2TPV2,
								SESS_ID);
				}

				rte_memcpy(hdr->buffer, l2tpv2_spec,
					   sizeof(*l2tpv2_spec));
			}

			tun_inner = 1;

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_PPP:
			ppp_spec = item->spec;
			ppp_mask = item->mask;

			hdr = &hdrs->proto_hdr[layer];

			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, PPP);

			if (ppp_spec && ppp_mask) {
				rte_memcpy(hdr->buffer, ppp_spec,
					   sizeof(*ppp_spec));
			}

			hdrs->count = ++layer;
			break;

		case RTE_FLOW_ITEM_TYPE_VOID:
			break;

		default:
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Invalid pattern item.");
			return -rte_errno;
		}
	}

	if (layer > VIRTCHNL_MAX_NUM_PROTO_HDRS) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Protocol header layers exceed the maximum value");
		return -rte_errno;
	}

	if (!iavf_fdir_refine_input_set(input_set,
					input_set_mask | IAVF_INSET_ETHERTYPE,
					filter)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC, pattern,
				   "Invalid input set");
		return -rte_errno;
	}

	filter->input_set = input_set;

	return 0;
}

static int
iavf_fdir_parse(struct iavf_adapter *ad,
		struct iavf_pattern_match_item *array,
		uint32_t array_len,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_fdir_conf *filter = &vf->fdir.conf;
	struct iavf_pattern_match_item *item = NULL;
	int ret;

	memset(filter, 0, sizeof(*filter));

	item = iavf_search_pattern_match_item(pattern, array, array_len, error);
	if (!item)
		return -rte_errno;

	ret = iavf_fdir_parse_pattern(ad, pattern, item->input_set_mask,
				      error, filter);
	if (ret)
		goto error;

	ret = iavf_fdir_parse_action(ad, actions, error, filter);
	if (ret)
		goto error;

	if (meta)
		*meta = filter;

error:
	rte_free(item);
	return ret;
}

static struct iavf_flow_parser iavf_fdir_parser = {
	.engine = &iavf_fdir_engine,
	.array = iavf_fdir_pattern,
	.array_len = RTE_DIM(iavf_fdir_pattern),
	.parse_pattern_action = iavf_fdir_parse,
	.stage = IAVF_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(iavf_fdir_engine_register)
{
	iavf_register_flow_engine(&iavf_fdir_engine);
}
