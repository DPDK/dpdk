/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_dev.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_pmd_logs.h>

#include "dpaa2_ethdev.h"
#include "dpaa2_sparser.h"
#include "base/dpaa2_hw_dpni_annot.h"
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

uint8_t wriop_bytecode[] = {
	0x00, 0x04, 0x29, 0x42, 0x03, 0xe0, 0x12, 0x00, 0x29, 0x02,
	0x18, 0x00, 0x87, 0x3c, 0x00, 0x02, 0x18, 0x00, 0x00, 0x00
};

struct frame_attr frame_attr_arr[] = {
	/* Frame Attribute Flags 1 */
	/* 000 */ {"Routing header present in IPv6 header 2 ", 0, 0x80000000},
	/* 001 */ {"GTP Primed was detected                 ", 0, 0x40000000},
	/* 002 */ {"VLAN with VID = 0 was detected          ", 0, 0x20000000},
	/* 003 */ {"A PTP frame was detected                ", 0, 0x10000000},
	/* 004 */ {"VXLAN was parsed                        ", 0, 0x08000000},
	/* 005 */ {"A VXLAN HXS parsing error was detected  ", 0, 0x04000000},
	/* 006 */ {"Ethernet control protocol was detected  ", 0, 0x02000000},
	/* 007 */ {"IKE was detected at UDP port 4500       ", 0, 0x01000000},
	/* 008 */ {"Shim Shell Soft Parsing Error           ", 0, 0x00800000},
	/* 009 */ {"Parsing Error                           ", 0, 0x00400000},
	/* 010 */ {"Ethernet MAC Present                    ", 0, 0x00200000},
	/* 011 */ {"Ethernet Unicast                        ", 0, 0x00100000},
	/* 012 */ {"Ethernet Multicast                      ", 0, 0x00080000},
	/* 013 */ {"Ethernet Broadcast                      ", 0, 0x00040000},
	/* 014 */ {"BPDU frame (MAC DA is 01:80:C2:00:00:00)", 0, 0x00020000},
	/* 015 */ {"FCoE detected (Ether type is 0x8906)    ", 0, 0x00010000},
	/* 016 */ {"FIP detected (Ether type is 0x8914)     ", 0, 0x00008000},
	/* 017 */ {"Ethernet Parsing Error                  ", 0, 0x00004000},
	/* 018 */ {"LLC+SNAP Present                        ", 0, 0x00002000},
	/* 019 */ {"Unknown LLC/OUI                         ", 0, 0x00001000},
	/* 020 */ {"LLC+SNAP Error                          ", 0, 0x00000800},
	/* 021 */ {"VLAN 1 Present                          ", 0, 0x00000400},
	/* 022 */ {"VLAN n Present                          ", 0, 0x00000200},
	/* 023 */ {"CFI bit in a \"8100\" VLAN tag is set   ", 0, 0x00000100},
	/* 024 */ {"VLAN Parsing Error                      ", 0, 0x00000080},
	/* 025 */ {"PPPoE+PPP Present                       ", 0, 0x00000040},
	/* 026 */ {"PPPoE+PPP Parsing Error                 ", 0, 0x00000020},
	/* 027 */ {"MPLS 1 Present                          ", 0, 0x00000010},
	/* 028 */ {"MPLS n Present                          ", 0, 0x00000008},
	/* 029 */ {"MPLS Parsing Error                      ", 0, 0x00000004},
	/* 030 */ {"ARP frame Present (Ethertype 0x0806)    ", 0, 0x00000002},
	/* 031 */ {"ARP Parsing Error                       ", 0, 0x00000001},
	/* Frame Attribute Flags 2 */
	/* 032 */ {"L2 Unknown Protocol                     ", 1, 0x80000000},
	/* 033 */ {"L2 Soft Parsing Error                   ", 1, 0x40000000},
	/* 034 */ {"IPv4 1 Present                          ", 1, 0x20000000},
	/* 035 */ {"IPv4 1 Unicast                          ", 1, 0x10000000},
	/* 036 */ {"IPv4 1 Multicast                        ", 1, 0x08000000},
	/* 037 */ {"IPv4 1 Broadcast                        ", 1, 0x04000000},
	/* 038 */ {"IPv4 n Present                          ", 1, 0x02000000},
	/* 039 */ {"IPv4 n Unicast                          ", 1, 0x01000000},
	/* 040 */ {"IPv4 n Multicast                        ", 1, 0x00800000},
	/* 041 */ {"IPv4 n Broadcast                        ", 1, 0x00400000},
	/* 042 */ {"IPv6 1 Present                          ", 1, 0x00200000},
	/* 043 */ {"IPv6 1 Unicast                          ", 1, 0x00100000},
	/* 044 */ {"IPv6 1 Multicast                        ", 1, 0x00080000},
	/* 045 */ {"IPv6 n Present                          ", 1, 0x00040000},
	/* 046 */ {"IPv6 n Unicast                          ", 1, 0x00020000},
	/* 047 */ {"IPv6 n Multicast                        ", 1, 0x00010000},
	/* 048 */ {"IP 1 option present                     ", 1, 0x00008000},
	/* 049 */ {"IP 1 Unknown Protocol                   ", 1, 0x00004000},
	/* 050 */ {"IP 1 Packet is a fragment               ", 1, 0x00002000},
	/* 051 */ {"IP 1 Packet is an initial fragment      ", 1, 0x00001000},
	/* 052 */ {"IP 1 Parsing Error                      ", 1, 0x00000800},
	/* 053 */ {"IP n option present                     ", 1, 0x00000400},
	/* 054 */ {"IP n Unknown Protocol                   ", 1, 0x00000200},
	/* 055 */ {"IP n Packet is a fragment               ", 1, 0x00000100},
	/* 056 */ {"IP n Packet is an initial fragment      ", 1, 0x00000080},
	/* 057 */ {"ICMP detected (IP proto is 1)           ", 1, 0x00000040},
	/* 058 */ {"IGMP detected (IP proto is 2)           ", 1, 0x00000020},
	/* 059 */ {"ICMPv6 detected (IP proto is 3a)        ", 1, 0x00000010},
	/* 060 */ {"UDP Light detected (IP proto is 136)    ", 1, 0x00000008},
	/* 061 */ {"IP n Parsing Error                      ", 1, 0x00000004},
	/* 062 */ {"Min. Encap Present                      ", 1, 0x00000002},
	/* 063 */ {"Min. Encap S flag set                   ", 1, 0x00000001},
	/* Frame Attribute Flags 3 */
	/* 064 */ {"Min. Encap Parsing Error                ", 2, 0x80000000},
	/* 065 */ {"GRE Present                             ", 2, 0x40000000},
	/* 066 */ {"GRE R bit set                           ", 2, 0x20000000},
	/* 067 */ {"GRE Parsing Error                       ", 2, 0x10000000},
	/* 068 */ {"L3 Unknown Protocol                     ", 2, 0x08000000},
	/* 069 */ {"L3 Soft Parsing Error                   ", 2, 0x04000000},
	/* 070 */ {"UDP Present                             ", 2, 0x02000000},
	/* 071 */ {"UDP Parsing Error                       ", 2, 0x01000000},
	/* 072 */ {"TCP Present                             ", 2, 0x00800000},
	/* 073 */ {"TCP options present                     ", 2, 0x00400000},
	/* 074 */ {"TCP Control bits 6-11 set               ", 2, 0x00200000},
	/* 075 */ {"TCP Control bits 3-5 set                ", 2, 0x00100000},
	/* 076 */ {"TCP Parsing Error                       ", 2, 0x00080000},
	/* 077 */ {"IPSec Present                           ", 2, 0x00040000},
	/* 078 */ {"IPSec ESP found                         ", 2, 0x00020000},
	/* 079 */ {"IPSec AH found                          ", 2, 0x00010000},
	/* 080 */ {"IPSec Parsing Error                     ", 2, 0x00008000},
	/* 081 */ {"SCTP Present                            ", 2, 0x00004000},
	/* 082 */ {"SCTP Parsing Error                      ", 2, 0x00002000},
	/* 083 */ {"DCCP Present                            ", 2, 0x00001000},
	/* 084 */ {"DCCP Parsing Error                      ", 2, 0x00000800},
	/* 085 */ {"L4 Unknown Protocol                     ", 2, 0x00000400},
	/* 086 */ {"L4 Soft Parsing Error                   ", 2, 0x00000200},
	/* 087 */ {"GTP Present                             ", 2, 0x00000100},
	/* 088 */ {"GTP Parsing Error                       ", 2, 0x00000080},
	/* 089 */ {"ESP Present                             ", 2, 0x00000040},
	/* 090 */ {"ESP Parsing Error                       ", 2, 0x00000020},
	/* 091 */ {"iSCSI detected (Port# 860)              ", 2, 0x00000010},
	/* 092 */ {"Capwap-control detected (Port# 5246)    ", 2, 0x00000008},
	/* 093 */ {"Capwap-data detected (Port# 5247)       ", 2, 0x00000004},
	/* 094 */ {"L5 Soft Parsing Error                   ", 2, 0x00000002},
	/* 095 */ {"IPv6 Route hdr1 present                 ", 2, 0x00000001},
	/* 096 */ {NULL,                                       0, 0x00000000}
};

struct frame_attr_ext frame_attr_ext_arr[] = {
	/* Frame Attribute Flags Extension */
	/* 096 */ {"User defined soft parser bit #0         ", 0, 0x8000},
	/* 096 */ {"User defined soft parser bit #1         ", 0, 0x4000},
	/* 096 */ {"User defined soft parser bit #2         ", 0, 0x2000},
	/* 096 */ {"User defined soft parser bit #3         ", 0, 0x1000},
	/* 096 */ {"User defined soft parser bit #4         ", 0, 0x0800},
	/* 096 */ {"User defined soft parser bit #5         ", 0, 0x0400},
	/* 096 */ {"User defined soft parser bit #6         ", 0, 0x0200},
	/* 096 */ {"User defined soft parser bit #7         ", 0, 0x0100},
	/* 097 */ {"Reserved                                ", 0, 0x00ff},
	/* 112 */ {NULL,                                       0, 0x0000}
};

int dpaa2_eth_load_wriop_soft_parser(struct dpaa2_dev_priv *priv,
				     enum dpni_soft_sequence_dest dest)
{
	struct fsl_mc_io *dpni = priv->hw;
	struct dpni_load_ss_cfg     cfg;
	struct dpni_drv_sparser_param	sp_param;
	uint8_t *addr;
	int ret;

	memset(&sp_param, 0, sizeof(sp_param));
	sp_param.start_pc = priv->ss_offset;
	sp_param.byte_code = &wriop_bytecode[0];
	sp_param.size = sizeof(wriop_bytecode);

	cfg.dest = dest;
	cfg.ss_offset = sp_param.start_pc;
	cfg.ss_size = sp_param.size;

	addr = rte_malloc(NULL, sp_param.size, 64);
	if (!addr) {
		DPAA2_PMD_ERR("Memory unavailable for soft parser param\n");
		return -1;
	}

	memcpy(addr, sp_param.byte_code, sp_param.size);
	cfg.ss_iova = (uint64_t)(DPAA2_VADDR_TO_IOVA(addr));

	ret = dpni_load_sw_sequence(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("dpni_load_sw_sequence failed\n");
		rte_free(addr);
		return ret;
	}

	priv->ss_iova = (uint64_t)(DPAA2_VADDR_TO_IOVA(addr));
	priv->ss_offset += sp_param.size;
	RTE_LOG(INFO, PMD, "Soft parser loaded for dpni@%d\n", priv->hw_id);

	rte_free(addr);
	return 0;
}

int dpaa2_eth_enable_wriop_soft_parser(struct dpaa2_dev_priv *priv,
				       enum dpni_soft_sequence_dest dest)
{
	struct fsl_mc_io *dpni = priv->hw;
	struct dpni_enable_ss_cfg cfg;
	uint8_t pa[3];
	struct dpni_drv_sparser_param sp_param;
	uint8_t *param_addr = NULL;
	int ret;

	memset(&sp_param, 0, sizeof(sp_param));
	pa[0] = 32;	/* Custom Header Length in bytes */
	sp_param.custom_header_first = 1;
	sp_param.param_offset = 32;
	sp_param.param_size = 1;
	sp_param.start_pc = priv->ss_offset;
	sp_param.param_array = (uint8_t *)&pa[0];

	cfg.dest = dest;
	cfg.ss_offset = sp_param.start_pc;
	cfg.set_start = sp_param.custom_header_first;
	cfg.hxs = (uint16_t)sp_param.link_to_hard_hxs;
	cfg.param_offset = sp_param.param_offset;
	cfg.param_size = sp_param.param_size;
	if (cfg.param_size) {
		param_addr = rte_malloc(NULL, cfg.param_size, 64);
		if (!param_addr) {
			DPAA2_PMD_ERR("Memory unavailable for soft parser param\n");
			return -1;
		}

		memcpy(param_addr, sp_param.param_array, cfg.param_size);
		cfg.param_iova = (uint64_t)(DPAA2_VADDR_TO_IOVA(param_addr));
		priv->ss_param_iova = cfg.param_iova;
	} else {
		cfg.param_iova = 0;
	}

	ret = dpni_enable_sw_sequence(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("dpni_enable_sw_sequence failed for dpni%d\n",
			priv->hw_id);
		rte_free(param_addr);
		return ret;
	}

	rte_free(param_addr);
	RTE_LOG(INFO, PMD, "Soft parser enabled for dpni@%d\n", priv->hw_id);
	return 0;
}
