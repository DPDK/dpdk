/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
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

enum idpf_mac_type {
	IDPF_MAC_UNKNOWN = 0,
	IDPF_MAC_PF,
	IDPF_MAC_VF,
	IDPF_MAC_GENERIC
};

#define ETH_ALEN 6

struct idpf_mac_info {
	enum idpf_mac_type type;
	u8 addr[ETH_ALEN];
	u8 perm_addr[ETH_ALEN];
};

#define IDPF_AQ_LINK_UP 0x1

/* PCI bus types */
enum idpf_bus_type {
	idpf_bus_type_unknown = 0,
	idpf_bus_type_pci,
	idpf_bus_type_pcix,
	idpf_bus_type_pci_express,
	idpf_bus_type_reserved
};

/* PCI bus speeds */
enum idpf_bus_speed {
	idpf_bus_speed_unknown	= 0,
	idpf_bus_speed_33	= 33,
	idpf_bus_speed_66	= 66,
	idpf_bus_speed_100	= 100,
	idpf_bus_speed_120	= 120,
	idpf_bus_speed_133	= 133,
	idpf_bus_speed_2500	= 2500,
	idpf_bus_speed_5000	= 5000,
	idpf_bus_speed_8000	= 8000,
	idpf_bus_speed_reserved
};

/* PCI bus widths */
enum idpf_bus_width {
	idpf_bus_width_unknown	= 0,
	idpf_bus_width_pcie_x1	= 1,
	idpf_bus_width_pcie_x2	= 2,
	idpf_bus_width_pcie_x4	= 4,
	idpf_bus_width_pcie_x8	= 8,
	idpf_bus_width_32	= 32,
	idpf_bus_width_64	= 64,
	idpf_bus_width_reserved
};

/* Bus parameters */
struct idpf_bus_info {
	enum idpf_bus_speed speed;
	enum idpf_bus_width width;
	enum idpf_bus_type type;

	u16 func;
	u16 device;
	u16 lan_id;
	u16 bus_id;
};

/* Function specific capabilities */
struct idpf_hw_func_caps {
	u32 num_alloc_vfs;
	u32 vf_base_id;
};

/* Define the APF hardware struct to replace other control structs as needed
 * Align to ctlq_hw_info
 */
struct idpf_hw {
	/* Some part of BAR0 address space is not mapped by the LAN driver.
	 * This results in 2 regions of BAR0 to be mapped by LAN driver which
	 * will have its own base hardware address when mapped.
	 */
	u8 *hw_addr;
	u8 *hw_addr_region2;
	u64 hw_addr_len;
	u64 hw_addr_region2_len;

	void *back;

	/* control queue - send and receive */
	struct idpf_ctlq_info *asq;
	struct idpf_ctlq_info *arq;

	/* subsystem structs */
	struct idpf_mac_info mac;
	struct idpf_bus_info bus;
	struct idpf_hw_func_caps func_caps;

	/* pci info */
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	u8 revision_id;
	bool adapter_stopped;

	LIST_HEAD_TYPE(list_head, idpf_ctlq_info) cq_list_head;
};

int idpf_ctlq_alloc_ring_res(struct idpf_hw *hw,
			     struct idpf_ctlq_info *cq);

void idpf_ctlq_dealloc_ring_res(struct idpf_hw *hw, struct idpf_ctlq_info *cq);

/* prototype for functions used for dynamic memory allocation */
void *idpf_alloc_dma_mem(struct idpf_hw *hw, struct idpf_dma_mem *mem,
			 u64 size);
void idpf_free_dma_mem(struct idpf_hw *hw, struct idpf_dma_mem *mem);
#endif /* _IDPF_CONTROLQ_H_ */
