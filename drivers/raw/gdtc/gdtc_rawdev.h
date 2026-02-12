/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 ZTE Corporation
 */

#ifndef GDTC_RAWDEV_H
#define GDTC_RAWDEV_H

#include <stdint.h>
#include <rte_log.h>
#include <rte_common.h>
#include <generic/rte_spinlock.h>

extern int zxdh_gdma_rawdev_logtype;
#define RTE_LOGTYPE_ZXDH_GDMA                   zxdh_gdma_rawdev_logtype

#define ZXDH_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZXDH_GDMA, \
		"%s() line %u: ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#define ZXDH_GDMA_VENDORID                      0x1cf2
#define ZXDH_GDMA_DEVICEID                      0x8044

#define ZXDH_GDMA_TOTAL_CHAN_NUM                58
#define ZXDH_GDMA_QUEUE_SIZE                    16384
#define ZXDH_GDMA_RING_SIZE                     32768

/* States if the source addresses is physical. */
#define ZXDH_GDMA_JOB_SRC_PHY                   (1UL)

/* States if the destination addresses is physical. */
#define ZXDH_GDMA_JOB_DEST_PHY                  RTE_BIT32(1)

/* ZF->HOST */
#define ZXDH_GDMA_JOB_DIR_TX                    RTE_BIT32(2)

/* HOST->ZF */
#define ZXDH_GDMA_JOB_DIR_RX                    RTE_BIT32(3)

#define ZXDH_GDMA_JOB_DIR_MASK                  (ZXDH_GDMA_JOB_DIR_TX | ZXDH_GDMA_JOB_DIR_RX)

enum zxdh_gdma_device_state {
	ZXDH_GDMA_DEV_RUNNING,
	ZXDH_GDMA_DEV_STOPPED
};

struct zxdh_gdma_buff_desc {
	uint32_t SrcAddr_L;
	uint32_t DstAddr_L;
	uint32_t Xpara;
	uint32_t ZY_para;
	uint32_t ZY_SrcStep;
	uint32_t ZY_DstStep;
	uint32_t ExtAddr;
	uint32_t LLI_Addr_L;
	uint32_t LLI_Addr_H;
	uint32_t ChCont;
	uint32_t LLI_User;
	uint32_t ErrAddr;
	uint32_t Control;
	uint32_t SrcAddr_H;
	uint32_t DstAddr_H;
	uint32_t Reserved;
};

struct zxdh_gdma_job {
	uint64_t src;
	uint64_t dest;
	uint32_t len;
	uint32_t flags;
	uint64_t cnxt;
	uint16_t status;
	uint16_t vq_id;
	void *usr_elem;
	uint8_t ep_id;
	uint8_t pf_id;
	uint16_t vf_id;
};

struct zxdh_gdma_queue {
	uint8_t   enable;
	uint8_t   is_txq;
	uint16_t  vq_id;
	uint16_t  queue_size;
	/* 0:GDMA needs to be configured through the APB interface */
	uint16_t  flag;
	uint32_t  user;
	uint16_t  tc_cnt;
	rte_spinlock_t enqueue_lock;
	struct {
		uint16_t avail_idx;
		uint16_t last_avail_idx;
		rte_iova_t ring_mem;
		const struct rte_memzone *ring_mz;
		struct zxdh_gdma_buff_desc *desc;
	} ring;
	struct {
		uint16_t  free_cnt;
		uint16_t  deq_cnt;
		uint16_t  pend_cnt;
		uint16_t  enq_idx;
		uint16_t  deq_idx;
		uint16_t  used_idx;
		struct zxdh_gdma_job **job;
	} sw_ring;
};

struct zxdh_gdma_rawdev {
	struct rte_device *device;
	struct rte_rawdev *rawdev;
	uintptr_t base_addr;
	uint8_t queue_num; /* total queue num */
	uint8_t used_num;  /* used  queue num */
	enum zxdh_gdma_device_state device_state;
	struct zxdh_gdma_queue vqs[ZXDH_GDMA_TOTAL_CHAN_NUM];
};

struct zxdh_gdma_enqdeq {
	uint16_t vq_id;
	struct zxdh_gdma_job **job;
};

struct zxdh_gdma_config {
	uint16_t max_hw_queues_per_core;
	uint16_t max_vqs;
	int queue_pool_cnt;
};

struct zxdh_gdma_rbp {
	uint32_t use_ultrashort:1;
	uint32_t enable:1;
	uint32_t dportid:3;
	uint32_t dpfid:3;
	uint32_t dvfid:8; /* using route by port for destination */
	uint32_t drbp:1;
	uint32_t sportid:3;
	uint32_t spfid:3;
	uint32_t svfid:8;
	uint32_t srbp:1;
};

struct zxdh_gdma_queue_config {
	uint32_t lcore_id;
	uint32_t flags;
	struct zxdh_gdma_rbp *rbp;
};

struct zxdh_gdma_attr {
	uint16_t num_hw_queues;
};

#endif /* GDTC_RAWDEV_H */
