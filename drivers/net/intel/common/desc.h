/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

 #ifndef _COMMON_INTEL_DESC_H_
 #define _COMMON_INTEL_DESC_H_

#include <rte_byteorder.h>

/* HW desc structures, both 16-byte and 32-byte types are supported */
#ifdef RTE_NET_INTEL_USE_16BYTE_DESC
union ci_rx_desc {
	struct {
		rte_le64_t pkt_addr; /* Packet buffer address */
		rte_le64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			struct {
				union {
					rte_le16_t mirroring_status;
					rte_le16_t fcoe_ctx_id;
				} mirr_fcoe;
				rte_le16_t l2tag1;
			} lo_dword;
			union {
				rte_le32_t rss; /* RSS Hash */
				rte_le32_t fd_id; /* Flow director filter id */
				rte_le32_t fcoe_param; /* FCoE DDP Context id */
			} hi_dword;
		} qword0;
		struct {
			/* ext status/error/pktype/length */
			rte_le64_t status_error_len;
		} qword1;
	} wb;  /* writeback */
};
#else
union ci_rx_desc {
	struct {
		rte_le64_t  pkt_addr; /* Packet buffer address */
		rte_le64_t  hdr_addr; /* Header buffer address */
			/* bit 0 of hdr_buffer_addr is DD bit */
		rte_le64_t  rsvd1;
		rte_le64_t  rsvd2;
	} read;
	struct {
		struct {
			struct {
				union {
					rte_le16_t mirroring_status;
					rte_le16_t fcoe_ctx_id;
				} mirr_fcoe;
				rte_le16_t l2tag1;
			} lo_dword;
			union {
				rte_le32_t rss; /* RSS Hash */
				rte_le32_t fcoe_param; /* FCoE DDP Context id */
				/* Flow director filter id in case of
				 * Programming status desc WB
				 */
				rte_le32_t fd_id;
			} hi_dword;
		} qword0;
		struct {
			/* status/error/pktype/length */
			rte_le64_t status_error_len;
		} qword1;
		struct {
			rte_le16_t ext_status; /* extended status */
			rte_le16_t rsvd;
			rte_le16_t l2tag2_1;
			rte_le16_t l2tag2_2;
		} qword2;
		struct {
			union {
				rte_le32_t flex_bytes_lo;
				rte_le32_t pe_status;
			} lo_dword;
			union {
				rte_le32_t flex_bytes_hi;
				rte_le32_t fd_id;
			} hi_dword;
		} qword3;
	} wb;  /* writeback */
};
#endif

#endif /* _COMMON_INTEL_DESC_H_ */
