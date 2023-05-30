/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef RTE_PDCP_HDR_H
#define RTE_PDCP_HDR_H

/**
 * @file
 *
 * PDCP-related defines
 *
 * Based on - ETSI TS 138 323 V17.1.0 (2022-08)
 * https://www.etsi.org/deliver/etsi_ts/138300_138399/138323/17.01.00_60/ts_138323v170100p.pdf
 */

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 4.3.1
 *
 * Indicate the maximum supported size of a PDCP Control PDU.
 */
#define RTE_PDCP_CTRL_PDU_SIZE_MAX 9000u

/**
 * 6.3.4 MAC-I
 *
 * Indicate the size of MAC-I in PDCP PDU.
 */
#define RTE_PDCP_MAC_I_LEN 4

/**
 * Indicate type of control information included in the corresponding PDCP
 * Control PDU.
 */
enum rte_pdcp_ctrl_pdu_type {
	RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT = 0,
	RTE_PDCP_CTRL_PDU_TYPE_ROHC_FEEDBACK = 1,
	RTE_PDCP_CTRL_PDU_TYPE_EHC_FEEDBACK = 2,
	RTE_PDCP_CRTL_PDU_TYPE_UDC_FEEDBACK = 3,
};

/**
 * 6.3.7 D/C
 *
 * This field indicates whether the corresponding PDCP PDU is a
 * PDCP Data PDU or a PDCP Control PDU.
 */
enum rte_pdcp_pdu_type {
	RTE_PDCP_PDU_TYPE_CTRL = 0,
	RTE_PDCP_PDU_TYPE_DATA = 1,
};

/**
 * 6.2.2.1 Data PDU for SRBs
 */
__extension__
struct rte_pdcp_cp_data_pdu_sn_12_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t sn_11_8 : 4;	/**< Sequence number bits 8-11 */
	uint8_t r : 4;		/**< Reserved */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t r : 4;		/**< Reserved */
	uint8_t sn_11_8 : 4;	/**< Sequence number bits 8-11 */
#endif
	uint8_t sn_7_0;		/**< Sequence number bits 0-7 */
} __rte_packed;

/**
 * 6.2.2.2 Data PDU for DRBs and MRBs with 12 bits PDCP SN
 */
__extension__
struct rte_pdcp_up_data_pdu_sn_12_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t sn_11_8 : 4;	/**< Sequence number bits 8-11 */
	uint8_t r : 3;		/**< Reserved */
	uint8_t d_c : 1;	/**< D/C bit */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t d_c : 1;	/**< D/C bit */
	uint8_t r : 3;		/**< Reserved */
	uint8_t sn_11_8 : 4;	/**< Sequence number bits 8-11 */
#endif
	uint8_t sn_7_0;		/**< Sequence number bits 0-7 */
} __rte_packed;

/**
 * 6.2.2.3 Data PDU for DRBs and MRBs with 18 bits PDCP SN
 */
__extension__
struct rte_pdcp_up_data_pdu_sn_18_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t sn_17_16 : 2;	/**< Sequence number bits 16-17 */
	uint8_t r : 5;		/**< Reserved */
	uint8_t d_c : 1;	/**< D/C bit */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t d_c : 1;	/**< D/C bit */
	uint8_t r : 5;		/**< Reserved */
	uint8_t sn_17_16 : 2;	/**< Sequence number bits 16-17 */
#endif
	uint8_t sn_15_8;	/**< Sequence number bits 8-15 */
	uint8_t sn_7_0;		/**< Sequence number bits 0-7 */
} __rte_packed;

/**
 * 6.2.3.1 Control PDU for PDCP status report
 */
__extension__
struct rte_pdcp_up_ctrl_pdu_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t r : 4;		/**< Reserved */
	uint8_t pdu_type : 3;	/**< Control PDU type */
	uint8_t d_c : 1;	/**< D/C bit */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t d_c : 1;	/**< D/C bit */
	uint8_t pdu_type : 3;	/**< Control PDU type */
	uint8_t r : 4;		/**< Reserved */
#endif
	/**
	 * 6.3.9 FMC
	 *
	 * First Missing COUNT. This field indicates the COUNT value of the
	 * first missing PDCP SDU within the reordering window, i.e. RX_DELIV.
	 */
	rte_be32_t fmc;
	/**
	 * 6.3.10 Bitmap
	 *
	 * Length: Variable. The length of the bitmap field can be 0.
	 *
	 * This field indicates which SDUs are missing and which SDUs are
	 * correctly received in the receiving PDCP entity. The bit position of
	 * Nth bit in the Bitmap is N, i.e., the bit position of the first bit
	 * in the Bitmap is 1.
	 */
	uint8_t bitmap[];
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* RTE_PDCP_HDR_H */
