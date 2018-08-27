/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 */

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mbuf_ptype.h>

/* get the name of the l2 packet type */
const char *rte_get_ptype_l2_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_L2_MASK) {
	case RTE_PTYPE_L2_ETHER: return "L2_ETHER";
	case RTE_PTYPE_L2_ETHER_TIMESYNC: return "L2_ETHER_TIMESYNC";
	case RTE_PTYPE_L2_ETHER_ARP: return "L2_ETHER_ARP";
	case RTE_PTYPE_L2_ETHER_LLDP: return "L2_ETHER_LLDP";
	case RTE_PTYPE_L2_ETHER_NSH: return "L2_ETHER_NSH";
	case RTE_PTYPE_L2_ETHER_VLAN: return "L2_ETHER_VLAN";
	case RTE_PTYPE_L2_ETHER_QINQ: return "L2_ETHER_QINQ";
	case RTE_PTYPE_L2_ETHER_PPPOE: return "L2_ETHER_PPPOE";
	case RTE_PTYPE_L2_ETHER_FCOE: return "L2_ETHER_FCOE";
	case RTE_PTYPE_L2_ETHER_MPLS: return "L2_ETHER_MPLS";
	default: return "L2_UNKNOWN";
	}
}

/* get the name of the l3 packet type */
const char *rte_get_ptype_l3_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_L3_MASK) {
	case RTE_PTYPE_L3_IPV4: return "L3_IPV4";
	case RTE_PTYPE_L3_IPV4_EXT: return "L3_IPV4_EXT";
	case RTE_PTYPE_L3_IPV6: return "L3_IPV6";
	case RTE_PTYPE_L3_IPV4_EXT_UNKNOWN: return "L3_IPV4_EXT_UNKNOWN";
	case RTE_PTYPE_L3_IPV6_EXT: return "L3_IPV6_EXT";
	case RTE_PTYPE_L3_IPV6_EXT_UNKNOWN: return "L3_IPV6_EXT_UNKNOWN";
	default: return "L3_UNKNOWN";
	}
}

/* get the name of the l4 packet type */
const char *rte_get_ptype_l4_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_L4_MASK) {
	case RTE_PTYPE_L4_TCP: return "L4_TCP";
	case RTE_PTYPE_L4_UDP: return "L4_UDP";
	case RTE_PTYPE_L4_FRAG: return "L4_FRAG";
	case RTE_PTYPE_L4_SCTP: return "L4_SCTP";
	case RTE_PTYPE_L4_ICMP: return "L4_ICMP";
	case RTE_PTYPE_L4_NONFRAG: return "L4_NONFRAG";
	case RTE_PTYPE_L4_IGMP: return "L4_IGMP";
	default: return "L4_UNKNOWN";
	}
}

/* get the name of the tunnel packet type */
const char *rte_get_ptype_tunnel_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_TUNNEL_MASK) {
	case RTE_PTYPE_TUNNEL_IP: return "TUNNEL_IP";
	case RTE_PTYPE_TUNNEL_GRE: return "TUNNEL_GRE";
	case RTE_PTYPE_TUNNEL_VXLAN: return "TUNNEL_VXLAN";
	case RTE_PTYPE_TUNNEL_NVGRE: return "TUNNEL_NVGRE";
	case RTE_PTYPE_TUNNEL_GENEVE: return "TUNNEL_GENEVE";
	case RTE_PTYPE_TUNNEL_GRENAT: return "TUNNEL_GRENAT";
	case RTE_PTYPE_TUNNEL_GTPC: return "TUNNEL_GTPC";
	case RTE_PTYPE_TUNNEL_GTPU: return "TUNNEL_GTPU";
	case RTE_PTYPE_TUNNEL_ESP: return "TUNNEL_ESP";
	case RTE_PTYPE_TUNNEL_L2TP: return "TUNNEL_L2TP";
	case RTE_PTYPE_TUNNEL_VXLAN_GPE: return "TUNNEL_VXLAN_GPE";
	case RTE_PTYPE_TUNNEL_MPLS_IN_UDP: return "TUNNEL_MPLS_IN_UDP";
	case RTE_PTYPE_TUNNEL_MPLS_IN_GRE: return "TUNNEL_MPLS_IN_GRE";
	default: return "TUNNEL_UNKNOWN";
	}
}

/* get the name of the inner_l2 packet type */
const char *rte_get_ptype_inner_l2_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_INNER_L2_MASK) {
	case RTE_PTYPE_INNER_L2_ETHER: return "INNER_L2_ETHER";
	case RTE_PTYPE_INNER_L2_ETHER_VLAN: return "INNER_L2_ETHER_VLAN";
	case RTE_PTYPE_INNER_L2_ETHER_QINQ: return "INNER_L2_ETHER_QINQ";
	default: return "INNER_L2_UNKNOWN";
	}
}

/* get the name of the inner_l3 packet type */
const char *rte_get_ptype_inner_l3_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_INNER_L3_MASK) {
	case RTE_PTYPE_INNER_L3_IPV4: return "INNER_L3_IPV4";
	case RTE_PTYPE_INNER_L3_IPV4_EXT: return "INNER_L3_IPV4_EXT";
	case RTE_PTYPE_INNER_L3_IPV6: return "INNER_L3_IPV6";
	case RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN:
		return "INNER_L3_IPV4_EXT_UNKNOWN";
	case RTE_PTYPE_INNER_L3_IPV6_EXT: return "INNER_L3_IPV6_EXT";
	case RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN:
		return "INNER_L3_IPV6_EXT_UNKNOWN";
	default: return "INNER_L3_UNKNOWN";
	}
}

/* get the name of the inner_l4 packet type */
const char *rte_get_ptype_inner_l4_name(uint32_t ptype)
{
	switch (ptype & RTE_PTYPE_INNER_L4_MASK) {
	case RTE_PTYPE_INNER_L4_TCP: return "INNER_L4_TCP";
	case RTE_PTYPE_INNER_L4_UDP: return "INNER_L4_UDP";
	case RTE_PTYPE_INNER_L4_FRAG: return "INNER_L4_FRAG";
	case RTE_PTYPE_INNER_L4_SCTP: return "INNER_L4_SCTP";
	case RTE_PTYPE_INNER_L4_ICMP: return "INNER_L4_ICMP";
	case RTE_PTYPE_INNER_L4_NONFRAG: return "INNER_L4_NONFRAG";
	default: return "INNER_L4_UNKNOWN";
	}
}

/* write the packet type name into the buffer */
int rte_get_ptype_name(uint32_t ptype, char *buf, size_t buflen)
{
	int ret;

	if (buflen == 0)
		return -1;

	buf[0] = '\0';
	if ((ptype & RTE_PTYPE_ALL_MASK) == RTE_PTYPE_UNKNOWN) {
		ret = snprintf(buf, buflen, "UNKNOWN");
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		return 0;
	}

	if ((ptype & RTE_PTYPE_L2_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_l2_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_L3_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_l3_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_L4_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_l4_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_TUNNEL_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_tunnel_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_INNER_L2_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_inner_l2_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_INNER_L3_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_inner_l3_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}
	if ((ptype & RTE_PTYPE_INNER_L4_MASK) != 0) {
		ret = snprintf(buf, buflen, "%s ",
			rte_get_ptype_inner_l4_name(ptype));
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}

	return 0;
}
