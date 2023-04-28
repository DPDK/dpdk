/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef _ROC_CPT_SG_H_
#define _ROC_CPT_SG_H_

#define ROC_DMA_MODE_SG (1 << 7)

#define ROC_MAX_SG_IN_OUT_CNT 128
#define ROC_MAX_SG_CNT	      (ROC_MAX_SG_IN_OUT_CNT / 2)

#define ROC_SG_LIST_HDR_SIZE (8u)
#define ROC_SG_ENTRY_SIZE    sizeof(struct roc_sglist_comp)
#define ROC_SG_MAX_COMP	     25
#define ROC_SG_MAX_DLEN_SIZE (ROC_SG_LIST_HDR_SIZE + (ROC_SG_MAX_COMP * ROC_SG_ENTRY_SIZE))

struct roc_sglist_comp {
	union {
		uint64_t len;
		struct {
			uint16_t len[4];
		} s;
	} u;
	uint64_t ptr[4];
};

struct roc_sg2list_comp {
	union {
		uint64_t len;
		struct {
			uint16_t len[3];
			uint16_t valid_segs;
		} s;
	} u;
	uint64_t ptr[3];
};

#endif /* _ROC_CPT_SG_H_ */
