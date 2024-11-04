/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_MSG_H
#define ZXDH_MSG_H

#include <stdint.h>

#include <ethdev_driver.h>

#define ZXDH_BAR0_INDEX                 0
#define ZXDH_CTRLCH_OFFSET              (0x2000)
#define ZXDH_MSG_CHAN_PFVFSHARE_OFFSET  (ZXDH_CTRLCH_OFFSET + 0x1000)

#define ZXDH_MSIX_INTR_MSG_VEC_BASE   1
#define ZXDH_MSIX_INTR_MSG_VEC_NUM    3
#define ZXDH_MSIX_INTR_DTB_VEC        (ZXDH_MSIX_INTR_MSG_VEC_BASE + ZXDH_MSIX_INTR_MSG_VEC_NUM)
#define ZXDH_MSIX_INTR_DTB_VEC_NUM    1
#define ZXDH_INTR_NONQUE_NUM          (ZXDH_MSIX_INTR_MSG_VEC_NUM + ZXDH_MSIX_INTR_DTB_VEC_NUM + 1)
#define ZXDH_QUEUE_INTR_VEC_BASE      (ZXDH_MSIX_INTR_DTB_VEC + ZXDH_MSIX_INTR_DTB_VEC_NUM)
#define ZXDH_QUEUE_INTR_VEC_NUM       256

#define ZXDH_BAR_MSG_POLLING_SPAN     100
#define ZXDH_BAR_MSG_POLL_CNT_PER_MS  (1 * 1000 / ZXDH_BAR_MSG_POLLING_SPAN)
#define ZXDH_BAR_MSG_POLL_CNT_PER_S   (1 * 1000 * 1000 / ZXDH_BAR_MSG_POLLING_SPAN)
#define ZXDH_BAR_MSG_TIMEOUT_TH       (10 * 1000 * 1000 / ZXDH_BAR_MSG_POLLING_SPAN)

#define ZXDH_BAR_CHAN_MSG_SYNC         0

#define ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL  (2 * 1024) /* channel size */
#define ZXDH_BAR_MSG_PLAYLOAD_OFFSET     (sizeof(struct zxdh_bar_msg_header))
#define ZXDH_BAR_MSG_PAYLOAD_MAX_LEN     \
	(ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL - sizeof(struct zxdh_bar_msg_header))

enum ZXDH_DRIVER_TYPE {
	ZXDH_MSG_CHAN_END_MPF = 0,
	ZXDH_MSG_CHAN_END_PF,
	ZXDH_MSG_CHAN_END_VF,
	ZXDH_MSG_CHAN_END_RISC,
};

enum ZXDH_MSG_VEC {
	ZXDH_MSIX_FROM_PFVF = ZXDH_MSIX_INTR_MSG_VEC_BASE,
	ZXDH_MSIX_FROM_MPF,
	ZXDH_MSIX_FROM_RISCV,
	ZXDH_MSG_VEC_NUM,
};

enum ZXDH_BAR_MSG_RTN {
	ZXDH_BAR_MSG_OK = 0,
	ZXDH_BAR_MSG_ERR_MSGID,
	ZXDH_BAR_MSG_ERR_NULL,
	ZXDH_BAR_MSG_ERR_TYPE, /* Message type exception */
	ZXDH_BAR_MSG_ERR_MODULE, /* Module ID exception */
	ZXDH_BAR_MSG_ERR_BODY_NULL, /* Message body exception */
	ZXDH_BAR_MSG_ERR_LEN, /* Message length exception */
	ZXDH_BAR_MSG_ERR_TIME_OUT, /* Message sending length too long */
	ZXDH_BAR_MSG_ERR_NOT_READY, /* Abnormal message sending conditions*/
	ZXDH_BAR_MEG_ERR_NULL_FUNC, /* Empty receive processing function pointer*/
	ZXDH_BAR_MSG_ERR_REPEAT_REGISTER, /* Module duplicate registration*/
	ZXDH_BAR_MSG_ERR_UNGISTER, /* Repeated deregistration*/
	/*
	 * The sending interface parameter boundary structure pointer is empty
	 */
	ZXDH_BAR_MSG_ERR_NULL_PARA,
	ZXDH_BAR_MSG_ERR_REPSBUFF_LEN, /* The length of reps_buff is too short*/
	/*
	 * Unable to find the corresponding message processing function for this module
	 */
	ZXDH_BAR_MSG_ERR_MODULE_NOEXIST,
	/*
	 * The virtual address in the parameters passed in by the sending interface is empty
	 */
	ZXDH_BAR_MSG_ERR_VIRTADDR_NULL,
	ZXDH_BAR_MSG_ERR_REPLY, /* sync msg resp_error */
	ZXDH_BAR_MSG_ERR_MPF_NOT_SCANNED,
	ZXDH_BAR_MSG_ERR_KERNEL_READY,
	ZXDH_BAR_MSG_ERR_USR_RET_ERR,
	ZXDH_BAR_MSG_ERR_ERR_PCIEID,
	ZXDH_BAR_MSG_ERR_SOCKET, /* netlink sockte err */
};

enum ZXDH_BAR_MODULE_ID {
	ZXDH_BAR_MODULE_DBG = 0, /* 0:  debug */
	ZXDH_BAR_MODULE_TBL,     /* 1:  resource table */
	ZXDH_BAR_MODULE_MISX,    /* 2:  config msix */
	ZXDH_BAR_MODULE_SDA,     /* 3: */
	ZXDH_BAR_MODULE_RDMA,    /* 4: */
	ZXDH_BAR_MODULE_DEMO,    /* 5:  channel test */
	ZXDH_BAR_MODULE_SMMU,    /* 6: */
	ZXDH_BAR_MODULE_MAC,     /* 7:  mac rx/tx stats */
	ZXDH_BAR_MODULE_VDPA,    /* 8:  vdpa live migration */
	ZXDH_BAR_MODULE_VQM,     /* 9:  vqm live migration */
	ZXDH_BAR_MODULE_NP,      /* 10: vf msg callback np */
	ZXDH_BAR_MODULE_VPORT,   /* 11: get vport */
	ZXDH_BAR_MODULE_BDF,     /* 12: get bdf */
	ZXDH_BAR_MODULE_RISC_READY, /* 13: */
	ZXDH_BAR_MODULE_REVERSE,    /* 14: byte stream reverse */
	ZXDH_BAR_MDOULE_NVME,       /* 15: */
	ZXDH_BAR_MDOULE_NPSDK,      /* 16: */
	ZXDH_BAR_MODULE_NP_TODO,    /* 17: */
	ZXDH_MODULE_BAR_MSG_TO_PF,  /* 18: */
	ZXDH_MODULE_BAR_MSG_TO_VF,  /* 19: */

	ZXDH_MODULE_FLASH = 32,
	ZXDH_BAR_MODULE_OFFSET_GET = 33,
	ZXDH_BAR_EVENT_OVS_WITH_VCB = 36,

	ZXDH_BAR_MSG_MODULE_NUM = 100,
};

enum ZXDH_RES_TBL_FILED {
	ZXDH_TBL_FIELD_PCIEID     = 0,
	ZXDH_TBL_FIELD_BDF        = 1,
	ZXDH_TBL_FIELD_MSGCH      = 2,
	ZXDH_TBL_FIELD_DATACH     = 3,
	ZXDH_TBL_FIELD_VPORT      = 4,
	ZXDH_TBL_FIELD_PNLID      = 5,
	ZXDH_TBL_FIELD_PHYPORT    = 6,
	ZXDH_TBL_FIELD_SERDES_NUM = 7,
	ZXDH_TBL_FIELD_NP_PORT    = 8,
	ZXDH_TBL_FIELD_SPEED      = 9,
	ZXDH_TBL_FIELD_HASHID     = 10,
	ZXDH_TBL_FIELD_NON,
};

enum ZXDH_TBL_MSG_TYPE {
	ZXDH_TBL_TYPE_READ,
	ZXDH_TBL_TYPE_WRITE,
	ZXDH_TBL_TYPE_NON,
};

struct zxdh_msix_para {
	uint16_t pcie_id;
	uint16_t vector_risc;
	uint16_t vector_pfvf;
	uint16_t vector_mpf;
	uint64_t virt_addr;
	uint16_t driver_type; /* refer to DRIVER_TYPE */
};

struct zxdh_msix_msg {
	uint16_t pcie_id;
	uint16_t vector_risc;
	uint16_t vector_pfvf;
	uint16_t vector_mpf;
};

struct zxdh_pci_bar_msg {
	uint64_t virt_addr; /* bar addr */
	void    *payload_addr;
	uint16_t payload_len;
	uint16_t emec;
	uint16_t src; /* refer to BAR_DRIVER_TYPE */
	uint16_t dst; /* refer to BAR_DRIVER_TYPE */
	uint16_t module_id;
	uint16_t src_pcieid;
	uint16_t dst_pcieid;
	uint16_t usr;
};

struct zxdh_bar_msix_reps {
	uint16_t pcie_id;
	uint16_t check;
	uint16_t vport;
	uint16_t rsv;
} __rte_packed;

struct zxdh_bar_offset_reps {
	uint16_t check;
	uint16_t rsv;
	uint32_t offset;
	uint32_t length;
} __rte_packed;

struct zxdh_bar_recv_msg {
	uint8_t reps_ok;
	uint16_t reps_len;
	uint8_t rsv;
	union {
		struct zxdh_bar_msix_reps msix_reps;
		struct zxdh_bar_offset_reps offset_reps;
	} __rte_packed;
} __rte_packed;

struct zxdh_msg_recviver_mem {
	void *recv_buffer; /* first 4B is head, followed by payload */
	uint64_t buffer_len;
};

struct zxdh_bar_msg_header {
	uint8_t valid : 1; /* used by __bar_chan_msg_valid_set/get */
	uint8_t sync  : 1;
	uint8_t emec  : 1; /* emergency */
	uint8_t ack   : 1; /* ack msg */
	uint8_t poll  : 1;
	uint8_t usr   : 1;
	uint8_t rsv;
	uint16_t module_id;
	uint16_t len;
	uint16_t msg_id;
	uint16_t src_pcieid;
	uint16_t dst_pcieid; /* used in PF-->VF */
};

typedef int (*zxdh_bar_chan_msg_recv_callback)(void *pay_load, uint16_t len,
		void *reps_buffer, uint16_t *reps_len, void *dev);

int zxdh_msg_chan_init(void);
int zxdh_bar_msg_chan_exit(void);
int zxdh_msg_chan_hwlock_init(struct rte_eth_dev *dev);

int zxdh_msg_chan_enable(struct rte_eth_dev *dev);
int zxdh_bar_chan_sync_msg_send(struct zxdh_pci_bar_msg *in,
		struct zxdh_msg_recviver_mem *result);

int zxdh_bar_irq_recv(uint8_t src, uint8_t dst, uint64_t virt_addr, void *dev);

#endif /* ZXDH_MSG_H */
