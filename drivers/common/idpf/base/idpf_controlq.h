/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#ifndef _IDPF_CONTROLQ_H_
#define _IDPF_CONTROLQ_H_

#include "idpf_osdep.h"
#include "idpf_alloc.h"
#include "idpf_controlq_api.h"

/* Maximum buffer lengths for all control queue types */
#define IDPF_CTLQ_MAX_RING_SIZE 1024
#define IDPF_CTLQ_MAX_BUF_LEN	4096

#define IDPF_CTLQ_DESC(R, i) \
	(&(((struct idpf_ctlq_desc *)((R)->desc_ring.va))[i]))

#define IDPF_CTLQ_DESC_UNUSED(R)					\
	((u16)((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->ring_size) + \
	       (R)->next_to_clean - (R)->next_to_use - 1))

/* Data type manipulation macros. */
#define IDPF_HI_DWORD(x)	((u32)((((x) >> 16) >> 16) & 0xFFFFFFFF))
#define IDPF_LO_DWORD(x)	((u32)((x) & 0xFFFFFFFF))
#define IDPF_HI_WORD(x)		((u16)(((x) >> 16) & 0xFFFF))
#define IDPF_LO_WORD(x)		((u16)((x) & 0xFFFF))

/* Control Queue default settings */
#define IDPF_CTRL_SQ_CMD_TIMEOUT	250  /* msecs */

struct idpf_ctlq_desc {
	__le16	flags;
	__le16	opcode;
	__le16	datalen;	/* 0 for direct commands */
	union {
		__le16 ret_val;
		__le16 pfid_vfid;
#define IDPF_CTLQ_DESC_VF_ID_S	0
#ifdef SIMICS_BUILD
#define IDPF_CTLQ_DESC_VF_ID_M	(0x3FF << IDPF_CTLQ_DESC_VF_ID_S)
#define IDPF_CTLQ_DESC_PF_ID_S	10
#define IDPF_CTLQ_DESC_PF_ID_M	(0x3F << IDPF_CTLQ_DESC_PF_ID_S)
#else
#define IDPF_CTLQ_DESC_VF_ID_M	(0x7FF << IDPF_CTLQ_DESC_VF_ID_S)
#define IDPF_CTLQ_DESC_PF_ID_S	11
#define IDPF_CTLQ_DESC_PF_ID_M	(0x1F << IDPF_CTLQ_DESC_PF_ID_S)
#endif
	};
	__le32 cookie_high;
	__le32 cookie_low;
	union {
		struct {
			__le32 param0;
			__le32 param1;
			__le32 param2;
			__le32 param3;
		} direct;
		struct {
			__le32 param0;
			__le32 param1;
			__le32 addr_high;
			__le32 addr_low;
		} indirect;
		u8 raw[16];
	} params;
};

/* Flags sub-structure
 * |0  |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |
 * |DD |CMP|ERR|  * RSV *  |FTYPE  | *RSV* |RD |VFC|BUF|  HOST_ID  |
 */
/* command flags and offsets */
#define IDPF_CTLQ_FLAG_DD_S		0
#define IDPF_CTLQ_FLAG_CMP_S		1
#define IDPF_CTLQ_FLAG_ERR_S		2
#define IDPF_CTLQ_FLAG_FTYPE_S		6
#define IDPF_CTLQ_FLAG_RD_S		10
#define IDPF_CTLQ_FLAG_VFC_S		11
#define IDPF_CTLQ_FLAG_BUF_S		12
#define IDPF_CTLQ_FLAG_HOST_ID_S	13

#define IDPF_CTLQ_FLAG_DD	BIT(IDPF_CTLQ_FLAG_DD_S)	/* 0x1	  */
#define IDPF_CTLQ_FLAG_CMP	BIT(IDPF_CTLQ_FLAG_CMP_S)	/* 0x2	  */
#define IDPF_CTLQ_FLAG_ERR	BIT(IDPF_CTLQ_FLAG_ERR_S)	/* 0x4	  */
#define IDPF_CTLQ_FLAG_FTYPE_VM	BIT(IDPF_CTLQ_FLAG_FTYPE_S)	/* 0x40	  */
#define IDPF_CTLQ_FLAG_FTYPE_PF	BIT(IDPF_CTLQ_FLAG_FTYPE_S + 1)	/* 0x80   */
#define IDPF_CTLQ_FLAG_RD	BIT(IDPF_CTLQ_FLAG_RD_S)	/* 0x400  */
#define IDPF_CTLQ_FLAG_VFC	BIT(IDPF_CTLQ_FLAG_VFC_S)	/* 0x800  */
#define IDPF_CTLQ_FLAG_BUF	BIT(IDPF_CTLQ_FLAG_BUF_S)	/* 0x1000 */

struct idpf_mbxq_desc {
	u8 pad[8];		/* CTLQ flags/opcode/len/retval fields */
	u32 chnl_opcode;	/* avoid confusion with desc->opcode */
	u32 chnl_retval;	/* ditto for desc->retval */
	u32 pf_vf_id;		/* used by CP when sending to PF */
};

int idpf_ctlq_alloc_ring_res(struct idpf_hw *hw,
			     struct idpf_ctlq_info *cq);

void idpf_ctlq_dealloc_ring_res(struct idpf_hw *hw, struct idpf_ctlq_info *cq);

/* prototype for functions used for dynamic memory allocation */
void *idpf_alloc_dma_mem(struct idpf_hw *hw, struct idpf_dma_mem *mem,
			 u64 size);
void idpf_free_dma_mem(struct idpf_hw *hw, struct idpf_dma_mem *mem);
#endif /* _IDPF_CONTROLQ_H_ */
