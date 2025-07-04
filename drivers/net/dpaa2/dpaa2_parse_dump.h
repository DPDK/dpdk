/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2022 NXP
 *
 */

#ifndef _DPAA2_PARSE_DUMP_H
#define _DPAA2_PARSE_DUMP_H

#include <rte_event_eth_rx_adapter.h>
#include <rte_pmd_dpaa2.h>

#include <dpaa2_hw_pvt.h>
#include "dpaa2_tm.h"

#include <mc/fsl_dpni.h>
#include <mc/fsl_mc_sys.h>

#include "base/dpaa2_hw_dpni_annot.h"

#define DPAA2_PR_PRINT printf

struct dpaa2_faf_bit_info {
	const char *name;
	int position;
};

struct dpaa2_fapr_field_info {
	const char *name;
	uint16_t value;
};

struct dpaa2_fapr_array {
	union {
		uint64_t pr_64[DPAA2_FAPR_SIZE / 8];
		uint8_t pr[DPAA2_FAPR_SIZE];
	};
};

#define NEXT_HEADER_NAME "Next Header"
#define ETH_OFF_NAME "ETH OFFSET"
#define VLAN_TCI_OFF_NAME "VLAN TCI OFFSET"
#define LAST_ENTRY_OFF_NAME "LAST ETYPE Offset"
#define L3_OFF_NAME "L3 Offset"
#define L4_OFF_NAME "L4 Offset"
#define L5_OFF_NAME "L5 Offset"
#define NEXT_HEADER_OFF_NAME "Next Header Offset"

static const
struct dpaa2_fapr_field_info support_dump_fields[] = {
	{
		.name = NEXT_HEADER_NAME,
	},
	{
		.name = ETH_OFF_NAME,
	},
	{
		.name = VLAN_TCI_OFF_NAME,
	},
	{
		.name = LAST_ENTRY_OFF_NAME,
	},
	{
		.name = L3_OFF_NAME,
	},
	{
		.name = L4_OFF_NAME,
	},
	{
		.name = L5_OFF_NAME,
	},
	{
		.name = NEXT_HEADER_OFF_NAME,
	}
};

static inline void
dpaa2_print_faf(struct dpaa2_fapr_array *fapr)
{
	const int faf_bit_len = DPAA2_FAF_TOTAL_SIZE * 8;
	struct dpaa2_faf_bit_info faf_bits[faf_bit_len];
	int i, byte_pos, bit_pos, vxlan = 0, vxlan_vlan = 0;
	struct rte_ether_hdr vxlan_in_eth;
	uint16_t vxlan_vlan_tci;

	for (i = 0; i < faf_bit_len; i++) {
		faf_bits[i].position = i;
		if (i == FAFE_VXLAN_IN_VLAN_FRAM)
			faf_bits[i].name = "VXLAN VLAN Present";
		else if (i == FAFE_VXLAN_IN_IPV4_FRAM)
			faf_bits[i].name = "VXLAN IPV4 Present";
		else if (i == FAFE_VXLAN_IN_IPV6_FRAM)
			faf_bits[i].name = "VXLAN IPV6 Present";
		else if (i == FAFE_VXLAN_IN_UDP_FRAM)
			faf_bits[i].name = "VXLAN UDP Present";
		else if (i == FAFE_VXLAN_IN_TCP_FRAM)
			faf_bits[i].name = "VXLAN TCP Present";
		else if (i == FAF_VXLAN_FRAM)
			faf_bits[i].name = "VXLAN Present";
		else if (i == FAF_ETH_FRAM)
			faf_bits[i].name = "Ethernet MAC Present";
		else if (i == FAF_VLAN_FRAM)
			faf_bits[i].name = "VLAN 1 Present";
		else if (i == FAF_IPV4_FRAM)
			faf_bits[i].name = "IPv4 1 Present";
		else if (i == FAF_IPV6_FRAM)
			faf_bits[i].name = "IPv6 1 Present";
		else if (i == FAF_IP_FRAG_FRAM)
			faf_bits[i].name = "IP fragment Present";
		else if (i == FAF_UDP_FRAM)
			faf_bits[i].name = "UDP Present";
		else if (i == FAF_TCP_FRAM)
			faf_bits[i].name = "TCP Present";
		else
			faf_bits[i].name = "Check RM for this unusual frame";
	}

	DPAA2_PR_PRINT("Frame Annotation Flags:\r\n");
	for (i = 0; i < faf_bit_len; i++) {
		byte_pos = i / 8 + DPAA2_FAFE_PSR_OFFSET;
		bit_pos = i % 8;
		if (fapr->pr[byte_pos] & (1 << (7 - bit_pos))) {
			DPAA2_PR_PRINT("FAF bit %d : %s\r\n",
				faf_bits[i].position, faf_bits[i].name);
			if (i == FAF_VXLAN_FRAM)
				vxlan = 1;
		}
	}

	if (vxlan) {
		vxlan_in_eth.dst_addr.addr_bytes[0] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR0_OFFSET];
		vxlan_in_eth.dst_addr.addr_bytes[1] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR1_OFFSET];
		vxlan_in_eth.dst_addr.addr_bytes[2] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR2_OFFSET];
		vxlan_in_eth.dst_addr.addr_bytes[3] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR3_OFFSET];
		vxlan_in_eth.dst_addr.addr_bytes[4] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR4_OFFSET];
		vxlan_in_eth.dst_addr.addr_bytes[5] =
			fapr->pr[DPAA2_VXLAN_IN_DADDR5_OFFSET];

		vxlan_in_eth.src_addr.addr_bytes[0] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR0_OFFSET];
		vxlan_in_eth.src_addr.addr_bytes[1] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR1_OFFSET];
		vxlan_in_eth.src_addr.addr_bytes[2] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR2_OFFSET];
		vxlan_in_eth.src_addr.addr_bytes[3] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR3_OFFSET];
		vxlan_in_eth.src_addr.addr_bytes[4] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR4_OFFSET];
		vxlan_in_eth.src_addr.addr_bytes[5] =
			fapr->pr[DPAA2_VXLAN_IN_SADDR5_OFFSET];

		vxlan_in_eth.ether_type =
			fapr->pr[DPAA2_VXLAN_IN_TYPE_OFFSET];
		vxlan_in_eth.ether_type =
			vxlan_in_eth.ether_type << 8;
		vxlan_in_eth.ether_type |=
			fapr->pr[DPAA2_VXLAN_IN_TYPE_OFFSET + 1];

		if (vxlan_in_eth.ether_type == RTE_ETHER_TYPE_VLAN)
			vxlan_vlan = 1;
		DPAA2_PR_PRINT("VXLAN inner eth:\r\n");
		DPAA2_PR_PRINT("dst addr: ");
		for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
			if (i != 0)
				DPAA2_PR_PRINT(":");
			DPAA2_PR_PRINT("%02x",
				vxlan_in_eth.dst_addr.addr_bytes[i]);
		}
		DPAA2_PR_PRINT("\r\n");
		DPAA2_PR_PRINT("src addr: ");
		for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
			if (i != 0)
				DPAA2_PR_PRINT(":");
			DPAA2_PR_PRINT("%02x",
				vxlan_in_eth.src_addr.addr_bytes[i]);
		}
		DPAA2_PR_PRINT("\r\n");
		DPAA2_PR_PRINT("type: 0x%04x\r\n",
			vxlan_in_eth.ether_type);
		if (vxlan_vlan) {
			vxlan_vlan_tci = fapr->pr[DPAA2_VXLAN_IN_TCI_OFFSET];
			vxlan_vlan_tci = vxlan_vlan_tci << 8;
			vxlan_vlan_tci |=
				fapr->pr[DPAA2_VXLAN_IN_TCI_OFFSET + 1];

			DPAA2_PR_PRINT("vlan tci: 0x%04x\r\n",
				vxlan_vlan_tci);
		}
	}
}

static inline void
dpaa2_print_parse_result(struct dpaa2_annot_hdr *annotation)
{
	struct dpaa2_fapr_array fapr;
	struct dpaa2_fapr_field_info
		fapr_fields[sizeof(support_dump_fields) /
		sizeof(struct dpaa2_fapr_field_info)];
	uint64_t len, i;

	memcpy(&fapr, &annotation->word3, DPAA2_FAPR_SIZE);
	for (i = 0; i < (DPAA2_FAPR_SIZE / 8); i++)
		fapr.pr_64[i] = rte_cpu_to_be_64(fapr.pr_64[i]);

	memcpy(fapr_fields, support_dump_fields,
		sizeof(support_dump_fields));

	for (i = 0;
		i < sizeof(fapr_fields) /
		sizeof(struct dpaa2_fapr_field_info);
		i++) {
		if (!strcmp(fapr_fields[i].name, NEXT_HEADER_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_NXTHDR_OFFSET];
			fapr_fields[i].value = fapr_fields[i].value << 8;
			fapr_fields[i].value |=
				fapr.pr[DPAA2_PR_NXTHDR_OFFSET + 1];
		} else if (!strcmp(fapr_fields[i].name, ETH_OFF_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_ETH_OFF_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, VLAN_TCI_OFF_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_TCI_OFF_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, LAST_ENTRY_OFF_NAME)) {
			fapr_fields[i].value =
				fapr.pr[DPAA2_PR_LAST_ETYPE_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, L3_OFF_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_L3_OFF_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, L4_OFF_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_L4_OFF_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, L5_OFF_NAME)) {
			fapr_fields[i].value = fapr.pr[DPAA2_PR_L5_OFF_OFFSET];
		} else if (!strcmp(fapr_fields[i].name, NEXT_HEADER_OFF_NAME)) {
			fapr_fields[i].value =
				fapr.pr[DPAA2_PR_NXTHDR_OFF_OFFSET];
		}
	}

	len = sizeof(fapr_fields) / sizeof(struct dpaa2_fapr_field_info);
	DPAA2_PR_PRINT("Parse Result:\r\n");
	for (i = 0; i < len; i++) {
		DPAA2_PR_PRINT("%21s : 0x%02x\r\n",
			fapr_fields[i].name, fapr_fields[i].value);
	}
	dpaa2_print_faf(&fapr);
}

#endif
