/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_COMMON_H_
#define _NBL_DEF_COMMON_H_

#include "nbl_include.h"

#define NBL_OPS_CALL(func, para)	\
	({ typeof(func) _func = (func);	\
	 (!_func) ? 0 : _func para; })

#define NBL_ONE_ETHERNET_PORT			(1)
#define NBL_TWO_ETHERNET_PORT			(2)
#define NBL_FOUR_ETHERNET_PORT			(4)

#define NBL_TWO_ETHERNET_MAX_MAC_NUM		(512)
#define NBL_FOUR_ETHERNET_MAX_MAC_NUM		(1024)

#define NBL_DEV_USER_TYPE	('n')
#define NBL_DEV_USER_DATA_LEN	(2044)

#define NBL_DEV_USER_PCI_OFFSET_SHIFT		40
#define NBL_DEV_USER_OFFSET_TO_INDEX(off)	((off) >> NBL_DEV_USER_PCI_OFFSET_SHIFT)
#define NBL_DEV_USER_INDEX_TO_OFFSET(index)	((u64)(index) << NBL_DEV_USER_PCI_OFFSET_SHIFT)
#define NBL_DEV_SHM_MSG_RING_INDEX		(6)

struct nbl_dev_user_channel_msg {
	u16 msg_type;
	u16 dst_id;
	u32 arg_len;
	u32 ack_err;
	u16 ack_length;
	u16 ack;
	u32 data[NBL_DEV_USER_DATA_LEN];
};

#define NBL_DEV_USER_CHANNEL		_IO(NBL_DEV_USER_TYPE, 0)

struct nbl_dev_user_dma_map {
	uint32_t	argsz;
	uint32_t	flags;
#define NBL_DEV_USER_DMA_MAP_FLAG_READ (RTE_BIT64(0))		/* readable from device */
#define NBL_DEV_USER_DMA_MAP_FLAG_WRITE (RTE_BIT64(1))	/* writable from device */
	uint64_t	vaddr;				/* Process virtual address */
	uint64_t	iova;				/* IO virtual address */
	uint64_t	size;				/* Size of mapping (bytes) */
};

#define NBL_DEV_USER_MAP_DMA		_IO(NBL_DEV_USER_TYPE, 1)

struct nbl_dev_user_dma_unmap {
	uint32_t	argsz;
	uint32_t	flags;
	uint64_t	vaddr;				/* Process virtual address */
	uint64_t	iova;				/* IO virtual address */
	uint64_t	size;				/* Size of mapping (bytes) */
};

#define NBL_DEV_USER_UNMAP_DMA		_IO(NBL_DEV_USER_TYPE, 2)

#define NBL_KERNEL_NETWORK			0
#define NBL_USER_NETWORK			1

#define NBL_DEV_USER_SWITCH_NETWORK	_IO(NBL_DEV_USER_TYPE, 3)
#define NBL_DEV_USER_GET_IFINDEX	_IO(NBL_DEV_USER_TYPE, 4)

struct nbl_dev_user_link_stat {
	u8 state;
	u8 flush;
};

#define NBL_DEV_USER_SET_EVENTFD	_IO(NBL_DEV_USER_TYPE, 5)
#define NBL_DEV_USER_CLEAR_EVENTFD	_IO(NBL_DEV_USER_TYPE, 6)
#define NBL_DEV_USER_SET_LISTENER	_IO(NBL_DEV_USER_TYPE, 7)
#define NBL_DEV_USER_GET_BAR_SIZE	_IO(NBL_DEV_USER_TYPE, 8)
#define NBL_DEV_USER_GET_DMA_LIMIT	_IO(NBL_DEV_USER_TYPE, 9)
#define NBL_DEV_USER_SET_PROMISC_MODE	_IO(NBL_DEV_USER_TYPE, 10)
#define NBL_DEV_USER_SET_MCAST_MODE	_IO(NBL_DEV_USER_TYPE, 11)

#define NBL_DMA_ADDRESS_FULL_TRANSLATE(hw, address)					\
	({ typeof(hw) _hw = (hw);							\
	((((u64)((_hw)->dma_set_msb)) << ((u64)((_hw)->dma_limit_msb))) | (address));	\
	})

struct nbl_dma_mem {
	void *va;
	uint64_t pa;
	uint32_t size;
	const struct rte_memzone *mz;
};

struct nbl_work {
	TAILQ_ENTRY(nbl_work) next;
	void *params;
	void (*handler)(void *priv);
	uint32_t tick;
	uint32_t random;
	bool run_once;
	bool no_run;
	uint8_t resv[2];
};

void *nbl_alloc_dma_mem(struct nbl_dma_mem *mem, uint32_t size);
void nbl_free_dma_mem(struct nbl_dma_mem *mem);

struct nbl_adapter;
int nbl_userdev_port_config(struct nbl_adapter *adapter, int start);
int nbl_userdev_port_isolate(struct nbl_adapter *adapter, int set, struct rte_flow_error *error);
int nbl_pci_map_device(struct nbl_adapter *adapter);
void nbl_pci_unmap_device(struct nbl_adapter *adapter);

#endif
