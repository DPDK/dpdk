/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _RTE_HIGIG_H_
#define _RTE_HIGIG_H_

/**
 * @file
 *
 * HIGIG2 headers definition.
 *
 * It is a layer 2.5 protocol and used in Broadcom switches.
 */

#include <stdint.h>
#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * higig2 frc header.
 */
struct rte_higig2_frc {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint32_t ksop:8;
	uint32_t tc:4;
	uint32_t mcst:1;
	uint32_t resv:3;
	uint32_t dst_modid:8;
	uint32_t dst_pid:8;
	uint32_t src_modid:8;
	uint32_t src_pid:8;
	uint32_t lbid:8;
	uint32_t ppd_type:3;
	uint32_t resv1:3;
	uint32_t dp:2;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint32_t ksop:8;
	uint32_t resv:3;
	uint32_t mcst:1;
	uint32_t tc:4;
	uint32_t dst_modid:8;
	uint32_t dst_pid:8;
	uint32_t src_modid:8;
	uint32_t src_pid:8;
	uint32_t lbid:8;
	uint32_t dp:2;
	uint32_t resv1:3;
	uint32_t ppd_type:3;
#endif
};


/**
 *
 * higig2 ppt type0 header
 */
struct rte_higig2_ppt_type0 {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint32_t mirror:1;
	uint32_t mirror_done:1;
	uint32_t mirror_only:1;
	uint32_t ingress_tagged:1;
	uint32_t dst_tgid:3;
	uint32_t dst_t:1;
	uint32_t vc_label2:4;
	uint32_t label_present:1;
	uint32_t l3:1;
	uint32_t res:2;
	uint32_t vc_label1:8;
	uint32_t vc_label0:8;
	uint32_t vid_high:8;
	uint32_t vid_low:8;
	uint32_t opc:3;
	uint32_t res1:2;
	uint32_t srce_t:1;
	uint32_t pf:2;
	uint32_t res2:5;
	uint32_t hdr_ext_length:3;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint32_t dst_t:1;
	uint32_t dst_tgid:3;
	uint32_t ingress_tagged:1;
	uint32_t mirror_only:1;
	uint32_t mirror_done:1;
	uint32_t mirror:1;
	uint32_t res:2;
	uint32_t l3:1;
	uint32_t label_present:1;
	uint32_t vc_label2:4;
	uint32_t vc_label1:8;
	uint32_t vc_label0:8;
	uint32_t vid_high:8;
	uint32_t vid_low:8;
	uint32_t pf:2;
	uint32_t srce_t:1;
	uint32_t res1:2;
	uint32_t opc:3;
	uint32_t hdr_ext_length:3;
	uint32_t res2:5;
#endif
};


/**
 *
 * higig2 ppt type1 header.
 */
__extension__
struct rte_higig2_ppt_type1 {
	uint16_t classification;
	uint16_t resv;
	uint16_t vid;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t opcode:3;
	uint16_t resv1:2;
	uint16_t src_t:1;
	uint16_t pfm:2;
	uint16_t resv2:5;
	uint16_t hdr_ext_len:3;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint16_t pfm:2;
	uint16_t src_t:1;
	uint16_t resv1:2;
	uint16_t opcode:3;
	uint16_t hdr_ext_len:3;
	uint16_t resv2:5;
#endif
};

/**
 *
 * higig2 header
 */
RTE_STD_C11
struct rte_higig2_hdr {
	struct rte_higig2_frc fcr;
	union {
		struct rte_higig2_ppt_type0 ppt0;
		struct rte_higig2_ppt_type1 ppt1;
	};
};

#ifdef __cplusplus
}
#endif

#endif /* RTE_HIGIG_H_ */
