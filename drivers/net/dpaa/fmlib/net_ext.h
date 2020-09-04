/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2019 NXP
 */

#ifndef __NET_EXT_H
#define __NET_EXT_H

#include "ncsw_ext.h"

/*
 * @Description		This file contains common and general netcomm headers
 *			definitions.
 */

typedef uint8_t ioc_header_field_ppp_t;

#define IOC_NET_HF_PPP_PID		(1)
#define IOC_NET_HF_PPP_COMPRESSED	(IOC_NET_HF_PPP_PID << 1)
#define IOC_NET_HF_PPP_ALL_FIELDS	((IOC_NET_HF_PPP_PID << 2) - 1)

typedef uint8_t ioc_header_field_pppoe_t;

#define ioc_net_hf_pppo_e_ver		(1)
#define ioc_net_hf_pppo_e_type		(ioc_net_hf_pppo_e_ver << 1)
#define ioc_net_hf_pppo_e_code		(ioc_net_hf_pppo_e_ver << 2)
#define ioc_net_hf_pppo_e_sid		(ioc_net_hf_pppo_e_ver << 3)
#define ioc_net_hf_pppo_e_len		(ioc_net_hf_pppo_e_ver << 4)
#define ioc_net_hf_pppo_e_session	(ioc_net_hf_pppo_e_ver << 5)
#define ioc_net_hf_pppo_e_pid		(ioc_net_hf_pppo_e_ver << 6)
#define ioc_net_hf_pppo_e_all_fields	((ioc_net_hf_pppo_e_ver << 7) - 1)

#define IOC_NET_HF_PPPMUX_PID		(1)
#define IOC_NET_HF_PPPMUX_CKSUM		(IOC_NET_HF_PPPMUX_PID << 1)
#define IOC_NET_HF_PPPMUX_COMPRESSED	(IOC_NET_HF_PPPMUX_PID << 2)
#define IOC_NET_HF_PPPMUX_ALL_FIELDS	((IOC_NET_HF_PPPMUX_PID << 3) - 1)

#define IOC_NET_HF_PPPMUX_SUBFRAME_PFF	(1)
#define IOC_NET_HF_PPPMUX_SUBFRAME_LXT	(IOC_NET_HF_PPPMUX_SUBFRAME_PFF << 1)
#define IOC_NET_HF_PPPMUX_SUBFRAME_LEN	(IOC_NET_HF_PPPMUX_SUBFRAME_PFF << 2)
#define IOC_NET_HF_PPPMUX_SUBFRAME_PID	(IOC_NET_HF_PPPMUX_SUBFRAME_PFF << 3)
#define IOC_NET_HF_PPPMUX_SUBFRAME_USE_PID \
		(IOC_NET_HF_PPPMUX_SUBFRAME_PFF << 4)
#define IOC_NET_HF_PPPMUX_SUBFRAME_ALL_FIELDS \
		((IOC_NET_HF_PPPMUX_SUBFRAME_PFF << 5) - 1)

typedef uint8_t ioc_header_field_eth_t;

#define IOC_NET_HF_ETH_DA		(1)
#define IOC_NET_HF_ETH_SA		(IOC_NET_HF_ETH_DA << 1)
#define IOC_NET_HF_ETH_LENGTH		(IOC_NET_HF_ETH_DA << 2)
#define IOC_NET_HF_ETH_TYPE		(IOC_NET_HF_ETH_DA << 3)
#define IOC_NET_HF_ETH_FINAL_CKSUM	(IOC_NET_HF_ETH_DA << 4)
#define IOC_NET_HF_ETH_PADDING		(IOC_NET_HF_ETH_DA << 5)
#define IOC_NET_HF_ETH_ALL_FIELDS	((IOC_NET_HF_ETH_DA << 6) - 1)

#define IOC_NET_HF_ETH_ADDR_SIZE	6

typedef uint16_t ioc_header_field_ip_t;

#define IOC_NET_HF_IP_VER		(1)
#define IOC_NET_HF_IP_DSCP		(IOC_NET_HF_IP_VER << 2)
#define IOC_NET_HF_IP_ECN		(IOC_NET_HF_IP_VER << 3)
#define IOC_NET_HF_IP_PROTO		(IOC_NET_HF_IP_VER << 4)

#define IOC_NET_HF_IP_PROTO_SIZE	1

typedef uint16_t ioc_header_field_ipv4_t;

#define ioc_net_hf_ipv_4_ver		(1)
#define ioc_net_hf_ipv_4_hdr_len		(ioc_net_hf_ipv_4_ver << 1)
#define ioc_net_hf_ipv_4_tos		(ioc_net_hf_ipv_4_ver << 2)
#define ioc_net_hf_ipv_4_total_len	(ioc_net_hf_ipv_4_ver << 3)
#define ioc_net_hf_ipv_4_id		(ioc_net_hf_ipv_4_ver << 4)
#define ioc_net_hf_ipv_4_flag_d		(ioc_net_hf_ipv_4_ver << 5)
#define ioc_net_hf_ipv_4_flag_m		(ioc_net_hf_ipv_4_ver << 6)
#define ioc_net_hf_ipv_4_offset		(ioc_net_hf_ipv_4_ver << 7)
#define ioc_net_hf_ipv_4_ttl		(ioc_net_hf_ipv_4_ver << 8)
#define ioc_net_hf_ipv_4_proto		(ioc_net_hf_ipv_4_ver << 9)
#define ioc_net_hf_ipv_4_cksum		(ioc_net_hf_ipv_4_ver << 10)
#define ioc_net_hf_ipv_4_src_ip		(ioc_net_hf_ipv_4_ver << 11)
#define ioc_net_hf_ipv_4_dst_ip		(ioc_net_hf_ipv_4_ver << 12)
#define ioc_net_hf_ipv_4_opts		(ioc_net_hf_ipv_4_ver << 13)
#define ioc_net_hf_ipv_4_opts_COUNT	(ioc_net_hf_ipv_4_ver << 14)
#define ioc_net_hf_ipv_4_all_fields	((ioc_net_hf_ipv_4_ver << 15) - 1)

#define ioc_net_hf_ipv_4_addr_size	4
#define ioc_net_hf_ipv_4_proto_SIZE	1

typedef uint8_t ioc_header_field_ipv6_t;

#define ioc_net_hf_ipv_6_ver		(1)
#define ioc_net_hf_ipv_6_tc		(ioc_net_hf_ipv_6_ver << 1)
#define ioc_net_hf_ipv_6_src_ip		(ioc_net_hf_ipv_6_ver << 2)
#define ioc_net_hf_ipv_6_dst_ip		(ioc_net_hf_ipv_6_ver << 3)
#define ioc_net_hf_ipv_6_next_hdr	(ioc_net_hf_ipv_6_ver << 4)
#define ioc_net_hf_ipv_6_fl		(ioc_net_hf_ipv_6_ver << 5)
#define ioc_net_hf_ipv_6_hop_limit	(ioc_net_hf_ipv_6_ver << 6)
#define ioc_net_hf_ipv_6_all_fields	((ioc_net_hf_ipv_6_ver << 7) - 1)

#define ioc_net_hf_ipv6_addr_size	16
#define ioc_net_hf_ipv_6_next_hdr_SIZE	1

#define IOC_NET_HF_ICMP_TYPE		(1)
#define IOC_NET_HF_ICMP_CODE		(IOC_NET_HF_ICMP_TYPE << 1)
#define IOC_NET_HF_ICMP_CKSUM		(IOC_NET_HF_ICMP_TYPE << 2)
#define IOC_NET_HF_ICMP_ID		(IOC_NET_HF_ICMP_TYPE << 3)
#define IOC_NET_HF_ICMP_SQ_NUM		(IOC_NET_HF_ICMP_TYPE << 4)
#define IOC_NET_HF_ICMP_ALL_FIELDS	((IOC_NET_HF_ICMP_TYPE << 5) - 1)

#define IOC_NET_HF_ICMP_CODE_SIZE	1
#define IOC_NET_HF_ICMP_TYPE_SIZE	1

#define IOC_NET_HF_IGMP_VERSION		(1)
#define IOC_NET_HF_IGMP_TYPE		(IOC_NET_HF_IGMP_VERSION << 1)
#define IOC_NET_HF_IGMP_CKSUM		(IOC_NET_HF_IGMP_VERSION << 2)
#define IOC_NET_HF_IGMP_DATA		(IOC_NET_HF_IGMP_VERSION << 3)
#define IOC_NET_HF_IGMP_ALL_FIELDS	((IOC_NET_HF_IGMP_VERSION << 4) - 1)

typedef uint16_t ioc_header_field_tcp_t;

#define IOC_NET_HF_TCP_PORT_SRC		(1)
#define IOC_NET_HF_TCP_PORT_DST		(IOC_NET_HF_TCP_PORT_SRC << 1)
#define IOC_NET_HF_TCP_SEQ		(IOC_NET_HF_TCP_PORT_SRC << 2)
#define IOC_NET_HF_TCP_ACK		(IOC_NET_HF_TCP_PORT_SRC << 3)
#define IOC_NET_HF_TCP_OFFSET		(IOC_NET_HF_TCP_PORT_SRC << 4)
#define IOC_NET_HF_TCP_FLAGS		(IOC_NET_HF_TCP_PORT_SRC << 5)
#define IOC_NET_HF_TCP_WINDOW		(IOC_NET_HF_TCP_PORT_SRC << 6)
#define IOC_NET_HF_TCP_CKSUM		(IOC_NET_HF_TCP_PORT_SRC << 7)
#define IOC_NET_HF_TCP_URGPTR		(IOC_NET_HF_TCP_PORT_SRC << 8)
#define IOC_NET_HF_TCP_OPTS		(IOC_NET_HF_TCP_PORT_SRC << 9)
#define IOC_NET_HF_TCP_OPTS_COUNT	(IOC_NET_HF_TCP_PORT_SRC << 10)
#define IOC_NET_HF_TCP_ALL_FIELDS	((IOC_NET_HF_TCP_PORT_SRC << 11) - 1)

#define IOC_NET_HF_TCP_PORT_SIZE	2

typedef uint8_t ioc_header_field_sctp_t;

#define IOC_NET_HF_SCTP_PORT_SRC	(1)
#define IOC_NET_HF_SCTP_PORT_DST	(IOC_NET_HF_SCTP_PORT_SRC << 1)
#define IOC_NET_HF_SCTP_VER_TAG		(IOC_NET_HF_SCTP_PORT_SRC << 2)
#define IOC_NET_HF_SCTP_CKSUM		(IOC_NET_HF_SCTP_PORT_SRC << 3)
#define IOC_NET_HF_SCTP_ALL_FIELDS	((IOC_NET_HF_SCTP_PORT_SRC << 4) - 1)

#define IOC_NET_HF_SCTP_PORT_SIZE	2

typedef uint8_t ioc_header_field_dccp_t;

#define IOC_NET_HF_DCCP_PORT_SRC	(1)
#define IOC_NET_HF_DCCP_PORT_DST	(IOC_NET_HF_DCCP_PORT_SRC << 1)
#define IOC_NET_HF_DCCP_ALL_FIELDS	((IOC_NET_HF_DCCP_PORT_SRC << 2) - 1)

#define IOC_NET_HF_DCCP_PORT_SIZE	2

typedef uint8_t ioc_header_field_udp_t;

#define IOC_NET_HF_UDP_PORT_SRC		(1)
#define IOC_NET_HF_UDP_PORT_DST		(IOC_NET_HF_UDP_PORT_SRC << 1)
#define IOC_NET_HF_UDP_LEN		(IOC_NET_HF_UDP_PORT_SRC << 2)
#define IOC_NET_HF_UDP_CKSUM		(IOC_NET_HF_UDP_PORT_SRC << 3)
#define IOC_NET_HF_UDP_ALL_FIELDS	((IOC_NET_HF_UDP_PORT_SRC << 4) - 1)

#define IOC_NET_HF_UDP_PORT_SIZE	2

typedef uint8_t ioc_header_field_udp_lite_t;

#define IOC_NET_HF_UDP_LITE_PORT_SRC	(1)
#define IOC_NET_HF_UDP_LITE_PORT_DST	(IOC_NET_HF_UDP_LITE_PORT_SRC << 1)
#define IOC_NET_HF_UDP_LITE_ALL_FIELDS \
		((IOC_NET_HF_UDP_LITE_PORT_SRC << 2) - 1)

#define IOC_NET_HF_UDP_LITE_PORT_SIZE		2

typedef uint8_t ioc_header_field_udp_encap_esp_t;

#define IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC	(1)
#define IOC_NET_HF_UDP_ENCAP_ESP_PORT_DST \
		(IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 1)
#define IOC_NET_HF_UDP_ENCAP_ESP_LEN \
		(IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 2)
#define IOC_NET_HF_UDP_ENCAP_ESP_CKSUM \
		(IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 3)
#define IOC_NET_HF_UDP_ENCAP_ESP_SPI \
		(IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 4)
#define IOC_NET_HF_UDP_ENCAP_ESP_SEQUENCE_NUM \
		(IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 5)
#define IOC_NET_HF_UDP_ENCAP_ESP_ALL_FIELDS \
		((IOC_NET_HF_UDP_ENCAP_ESP_PORT_SRC << 6) - 1)

#define IOC_NET_HF_UDP_ENCAP_ESP_PORT_SIZE	2
#define IOC_NET_HF_UDP_ENCAP_ESP_SPI_SIZE	4

#define IOC_NET_HF_IPHC_CID		(1)
#define IOC_NET_HF_IPHC_CID_TYPE	(IOC_NET_HF_IPHC_CID << 1)
#define IOC_NET_HF_IPHC_HCINDEX		(IOC_NET_HF_IPHC_CID << 2)
#define IOC_NET_HF_IPHC_GEN		(IOC_NET_HF_IPHC_CID << 3)
#define IOC_NET_HF_IPHC_D_BIT		(IOC_NET_HF_IPHC_CID << 4)
#define IOC_NET_HF_IPHC_ALL_FIELDS	((IOC_NET_HF_IPHC_CID << 5) - 1)

#define IOC_NET_HF_SCTP_CHUNK_DATA_TYPE		(1)
#define IOC_NET_HF_SCTP_CHUNK_DATA_FLAGS \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 1)
#define IOC_NET_HF_SCTP_CHUNK_DATA_LENGTH \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 2)
#define IOC_NET_HF_SCTP_CHUNK_DATA_TSN \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 3)
#define IOC_NET_HF_SCTP_CHUNK_DATA_STREAM_ID \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 4)
#define IOC_NET_HF_SCTP_CHUNK_DATA_STREAM_SQN \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 5)
#define IOC_NET_HF_SCTP_CHUNK_DATA_PAYLOAD_PID \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 6)
#define IOC_NET_HF_SCTP_CHUNK_DATA_UNORDERED \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 7)
#define IOC_NET_HF_SCTP_CHUNK_DATA_BEGINNING \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 8)
#define IOC_NET_HF_SCTP_CHUNK_DATA_END \
		(IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 9)
#define IOC_NET_HF_SCTP_CHUNK_DATA_ALL_FIELDS \
		((IOC_NET_HF_SCTP_CHUNK_DATA_TYPE << 10) - 1)

#define ioc_net_hf_l2tpv_2_type_bit	(1)
#define ioc_net_hf_l2tpv_2_length_bit	(ioc_net_hf_l2tpv_2_type_bit << 1)
#define ioc_net_hf_l2tpv_2_sequence_bit	(ioc_net_hf_l2tpv_2_type_bit << 2)
#define ioc_net_hf_l2tpv_2_offset_bit	(ioc_net_hf_l2tpv_2_type_bit << 3)
#define ioc_net_hf_l2tpv_2_priority_bit	(ioc_net_hf_l2tpv_2_type_bit << 4)
#define ioc_net_hf_l2tpv_2_version	(ioc_net_hf_l2tpv_2_type_bit << 5)
#define ioc_net_hf_l2tpv_2_len		(ioc_net_hf_l2tpv_2_type_bit << 6)
#define ioc_net_hf_l2tpv_2_tunnel_id	(ioc_net_hf_l2tpv_2_type_bit << 7)
#define ioc_net_hf_l2tpv_2_session_id	(ioc_net_hf_l2tpv_2_type_bit << 8)
#define ioc_net_hf_l2tpv_2_ns		(ioc_net_hf_l2tpv_2_type_bit << 9)
#define ioc_net_hf_l2tpv_2_nr		(ioc_net_hf_l2tpv_2_type_bit << 10)
#define ioc_net_hf_l2tpv_2_offset_size	(ioc_net_hf_l2tpv_2_type_bit << 11)
#define ioc_net_hf_l2tpv_2_first_byte	(ioc_net_hf_l2tpv_2_type_bit << 12)
#define ioc_net_hf_l2tpv_2_all_fields \
		((ioc_net_hf_l2tpv_2_type_bit << 13) - 1)

#define ioc_net_hf_l2tpv_3_ctrl_type_bit	(1)
#define ioc_net_hf_l2tpv_3_ctrl_length_bit \
		(ioc_net_hf_l2tpv_3_ctrl_type_bit << 1)
#define ioc_net_hf_l2tpv_3_ctrl_sequence_bit \
		(ioc_net_hf_l2tpv_3_ctrl_type_bit << 2)
#define ioc_net_hf_l2tpv_3_ctrl_version	(ioc_net_hf_l2tpv_3_ctrl_type_bit << 3)
#define ioc_net_hf_l2tpv_3_ctrl_length	(ioc_net_hf_l2tpv_3_ctrl_type_bit << 4)
#define ioc_net_hf_l2tpv_3_ctrl_control	(ioc_net_hf_l2tpv_3_ctrl_type_bit << 5)
#define ioc_net_hf_l2tpv_3_ctrl_sent	(ioc_net_hf_l2tpv_3_ctrl_type_bit << 6)
#define ioc_net_hf_l2tpv_3_ctrl_recv	(ioc_net_hf_l2tpv_3_ctrl_type_bit << 7)
#define ioc_net_hf_l2tpv_3_ctrl_first_byte \
		(ioc_net_hf_l2tpv_3_ctrl_type_bit << 8)
#define ioc_net_hf_l2tpv_3_ctrl_all_fields \
		((ioc_net_hf_l2tpv_3_ctrl_type_bit << 9) - 1)

#define ioc_net_hf_l2tpv_3_sess_type_bit	(1)
#define ioc_net_hf_l2tpv_3_sess_version	(ioc_net_hf_l2tpv_3_sess_type_bit << 1)
#define ioc_net_hf_l2tpv_3_sess_id	(ioc_net_hf_l2tpv_3_sess_type_bit << 2)
#define ioc_net_hf_l2tpv_3_sess_cookie	(ioc_net_hf_l2tpv_3_sess_type_bit << 3)
#define ioc_net_hf_l2tpv_3_sess_all_fields \
		((ioc_net_hf_l2tpv_3_sess_type_bit << 4) - 1)

typedef uint8_t ioc_header_field_vlan_t;

#define IOC_NET_HF_VLAN_VPRI		(1)
#define IOC_NET_HF_VLAN_CFI		(IOC_NET_HF_VLAN_VPRI << 1)
#define IOC_NET_HF_VLAN_VID		(IOC_NET_HF_VLAN_VPRI << 2)
#define IOC_NET_HF_VLAN_LENGTH		(IOC_NET_HF_VLAN_VPRI << 3)
#define IOC_NET_HF_VLAN_TYPE		(IOC_NET_HF_VLAN_VPRI << 4)
#define IOC_NET_HF_VLAN_ALL_FIELDS	((IOC_NET_HF_VLAN_VPRI << 5) - 1)

#define IOC_NET_HF_VLAN_TCI		(IOC_NET_HF_VLAN_VPRI | \
				IOC_NET_HF_VLAN_CFI | \
				IOC_NET_HF_VLAN_VID)

typedef uint8_t ioc_header_field_llc_t;

#define IOC_NET_HF_LLC_DSAP		(1)
#define IOC_NET_HF_LLC_SSAP		(IOC_NET_HF_LLC_DSAP << 1)
#define IOC_NET_HF_LLC_CTRL		(IOC_NET_HF_LLC_DSAP << 2)
#define IOC_NET_HF_LLC_ALL_FIELDS	((IOC_NET_HF_LLC_DSAP << 3) - 1)

#define IOC_NET_HF_NLPID_NLPID	(1)
#define IOC_NET_HF_NLPID_ALL_FIELDS	((IOC_NET_HF_NLPID_NLPID << 1) - 1)

typedef uint8_t ioc_header_field_snap_t;

#define IOC_NET_HF_SNAP_OUI		(1)
#define IOC_NET_HF_SNAP_PID		(IOC_NET_HF_SNAP_OUI << 1)
#define IOC_NET_HF_SNAP_ALL_FIELDS	((IOC_NET_HF_SNAP_OUI << 2) - 1)

typedef uint8_t ioc_header_field_llc_snap_t;

#define IOC_NET_HF_LLC_SNAP_TYPE	(1)
#define IOC_NET_HF_LLC_SNAP_ALL_FIELDS	((IOC_NET_HF_LLC_SNAP_TYPE << 1) - 1)

#define IOC_NET_HF_ARP_HTYPE		(1)
#define IOC_NET_HF_ARP_PTYPE		(IOC_NET_HF_ARP_HTYPE << 1)
#define IOC_NET_HF_ARP_HLEN		(IOC_NET_HF_ARP_HTYPE << 2)
#define IOC_NET_HF_ARP_PLEN		(IOC_NET_HF_ARP_HTYPE << 3)
#define IOC_NET_HF_ARP_OPER		(IOC_NET_HF_ARP_HTYPE << 4)
#define IOC_NET_HF_ARP_SHA		(IOC_NET_HF_ARP_HTYPE << 5)
#define IOC_NET_HF_ARP_SPA		(IOC_NET_HF_ARP_HTYPE << 6)
#define IOC_NET_HF_ARP_TH		(IOC_NET_HF_ARP_HTYPE << 7)
#define IOC_NET_HF_ARP_TPA		(IOC_NET_HF_ARP_HTYPE << 8)
#define IOC_NET_HF_ARP_ALL_FIELDS	((IOC_NET_HF_ARP_HTYPE << 9) - 1)

#define IOC_NET_HF_RFC2684_LLC		(1)
#define IOC_NET_HF_RFC2684_NLPID	(IOC_NET_HF_RFC2684_LLC << 1)
#define IOC_NET_HF_RFC2684_OUI		(IOC_NET_HF_RFC2684_LLC << 2)
#define IOC_NET_HF_RFC2684_PID		(IOC_NET_HF_RFC2684_LLC << 3)
#define IOC_NET_HF_RFC2684_VPN_OUI	(IOC_NET_HF_RFC2684_LLC << 4)
#define IOC_NET_HF_RFC2684_VPN_IDX	(IOC_NET_HF_RFC2684_LLC << 5)
#define IOC_NET_HF_RFC2684_ALL_FIELDS	((IOC_NET_HF_RFC2684_LLC << 6) - 1)

#define IOC_NET_HF_USER_DEFINED_SRCPORT	(1)
#define IOC_NET_HF_USER_DEFINED_PCDID	(IOC_NET_HF_USER_DEFINED_SRCPORT << 1)
#define IOC_NET_HF_USER_DEFINED_ALL_FIELDS \
		((IOC_NET_HF_USER_DEFINED_SRCPORT << 2) - 1)

#define IOC_NET_HF_PAYLOAD_BUFFER	(1)
#define IOC_NET_HF_PAYLOAD_SIZE		(IOC_NET_HF_PAYLOAD_BUFFER << 1)
#define IOC_NET_HF_MAX_FRM_SIZE		(IOC_NET_HF_PAYLOAD_BUFFER << 2)
#define IOC_NET_HF_MIN_FRM_SIZE		(IOC_NET_HF_PAYLOAD_BUFFER << 3)
#define IOC_NET_HF_PAYLOAD_TYPE		(IOC_NET_HF_PAYLOAD_BUFFER << 4)
#define IOC_NET_HF_FRAME_SIZE		(IOC_NET_HF_PAYLOAD_BUFFER << 5)
#define IOC_NET_HF_PAYLOAD_ALL_FIELDS	((IOC_NET_HF_PAYLOAD_BUFFER << 6) - 1)

typedef uint8_t ioc_header_field_gre_t;

#define IOC_NET_HF_GRE_TYPE		(1)
#define IOC_NET_HF_GRE_ALL_FIELDS	((IOC_NET_HF_GRE_TYPE << 1) - 1)

typedef uint8_t ioc_header_field_minencap_t;

#define IOC_NET_HF_MINENCAP_SRC_IP	(1)
#define IOC_NET_HF_MINENCAP_DST_IP	(IOC_NET_HF_MINENCAP_SRC_IP << 1)
#define IOC_NET_HF_MINENCAP_TYPE	(IOC_NET_HF_MINENCAP_SRC_IP << 2)
#define IOC_NET_HF_MINENCAP_ALL_FIELDS	((IOC_NET_HF_MINENCAP_SRC_IP << 3) - 1)

typedef uint8_t ioc_header_field_ipsec_ah_t;

#define IOC_NET_HF_IPSEC_AH_SPI	(1)
#define IOC_NET_HF_IPSEC_AH_NH		(IOC_NET_HF_IPSEC_AH_SPI << 1)
#define IOC_NET_HF_IPSEC_AH_ALL_FIELDS	((IOC_NET_HF_IPSEC_AH_SPI << 2) - 1)

typedef uint8_t ioc_header_field_ipsec_esp_t;

#define IOC_NET_HF_IPSEC_ESP_SPI	(1)
#define IOC_NET_HF_IPSEC_ESP_SEQUENCE_NUM	(IOC_NET_HF_IPSEC_ESP_SPI << 1)
#define IOC_NET_HF_IPSEC_ESP_ALL_FIELDS	((IOC_NET_HF_IPSEC_ESP_SPI << 2) - 1)

#define IOC_NET_HF_IPSEC_ESP_SPI_SIZE		4


typedef uint8_t ioc_header_field_mpls_t;

#define IOC_NET_HF_MPLS_LABEL_STACK		(1)
#define IOC_NET_HF_MPLS_LABEL_STACK_ALL_FIELDS \
		((IOC_NET_HF_MPLS_LABEL_STACK << 1) - 1)

typedef uint8_t ioc_header_field_macsec_t;

#define IOC_NET_HF_MACSEC_SECTAG	(1)
#define IOC_NET_HF_MACSEC_ALL_FIELDS	((IOC_NET_HF_MACSEC_SECTAG << 1) - 1)

typedef enum {
	HEADER_TYPE_NONE = 0,
	HEADER_TYPE_PAYLOAD,
	HEADER_TYPE_ETH,
	HEADER_TYPE_VLAN,
	HEADER_TYPE_IPV4,
	HEADER_TYPE_IPV6,
	HEADER_TYPE_IP,
	HEADER_TYPE_TCP,
	HEADER_TYPE_UDP,
	HEADER_TYPE_UDP_LITE,
	HEADER_TYPE_IPHC,
	HEADER_TYPE_SCTP,
	HEADER_TYPE_SCTP_CHUNK_DATA,
	HEADER_TYPE_PPPOE,
	HEADER_TYPE_PPP,
	HEADER_TYPE_PPPMUX,
	HEADER_TYPE_PPPMUX_SUBFRAME,
	HEADER_TYPE_L2TPV2,
	HEADER_TYPE_L2TPV3_CTRL,
	HEADER_TYPE_L2TPV3_SESS,
	HEADER_TYPE_LLC,
	HEADER_TYPE_LLC_SNAP,
	HEADER_TYPE_NLPID,
	HEADER_TYPE_SNAP,
	HEADER_TYPE_MPLS,
	HEADER_TYPE_IPSEC_AH,
	HEADER_TYPE_IPSEC_ESP,
	HEADER_TYPE_UDP_ENCAP_ESP, /* RFC 3948 */
	HEADER_TYPE_MACSEC,
	HEADER_TYPE_GRE,
	HEADER_TYPE_MINENCAP,
	HEADER_TYPE_DCCP,
	HEADER_TYPE_ICMP,
	HEADER_TYPE_IGMP,
	HEADER_TYPE_ARP,
	HEADER_TYPE_CAPWAP,
	HEADER_TYPE_CAPWAP_DTLS,
	HEADER_TYPE_RFC2684,
	HEADER_TYPE_USER_DEFINED_L2,
	HEADER_TYPE_USER_DEFINED_L3,
	HEADER_TYPE_USER_DEFINED_L4,
	HEADER_TYPE_USER_DEFINED_SHIM1,
	HEADER_TYPE_USER_DEFINED_SHIM2,
	MAX_HEADER_TYPE_COUNT
} ioc_net_header_type;

#endif /* __NET_EXT_H */
