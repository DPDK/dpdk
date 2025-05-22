/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_QP_COMMON_H_
#define _ZSDA_QP_COMMON_H_

#include <rte_bus_pci.h>
#include <rte_io.h>
#include <rte_cycles.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "bus_pci_driver.h"
#include "zsda_logs.h"

#define ZSDA_MAX_DEV				RTE_PMD_ZSDA_MAX_PCI_DEVICES

#define ZSDA_SUCCESS			0
#define ZSDA_FAILED				(-1)

enum zsda_service_type {
	ZSDA_SERVICE_COMPRESSION = 0,
	ZSDA_SERVICE_DECOMPRESSION = 1,
	ZSDA_SERVICE_CRYPTO_ENCRY = 2,
	ZSDA_SERVICE_CRYPTO_DECRY = 3,
	ZSDA_SERVICE_HASH_ENCODE = 6,
	ZSDA_SERVICE_INVALID,
};
#define ZSDA_MAX_SERVICES (ZSDA_SERVICE_INVALID)

#define ZSDA_CSR_READ32(addr)	      rte_read32((addr))
#define ZSDA_CSR_WRITE32(addr, value) rte_write32((value), (addr))
#define ZSDA_CSR_READ8(addr)	      rte_read8((addr))
#define ZSDA_CSR_WRITE8(addr, value)  rte_write8_relaxed((value), (addr))

#define NB_DES					512
#define ZSDA_SGL_MAX_NUMBER		512
#define COMP_REMOVE_SPACE_LEN 16

#define ZSDA_MAX_DESC		512
#define ZSDA_MAX_CYCLE		256
#define ZSDA_MAX_DEV		RTE_PMD_ZSDA_MAX_PCI_DEVICES
#define MAX_NUM_OPS			0x1FF
#define ZSDA_SGL_FRAGMENT_SIZE	32

#define ZSDA_OPC_COMP_GZIP		0x10 /* Encomp deflate-Gzip */
#define ZSDA_OPC_COMP_ZLIB		0x11 /* Encomp deflate-Zlib */
#define ZSDA_OPC_DECOMP_GZIP	0x18 /* Decomp inflate-Gzip */
#define ZSDA_OPC_DECOMP_ZLIB	0x19 /* Decomp inflate-Zlib */
#define ZSDA_OPC_INVALID		0xff

#define CQE_VALID(value) (value & 0x8000)
#define CQE_ERR0(value) (value & 0xFFFF)
#define CQE_ERR1(value) (value & 0x7FFF)

enum wqe_element_type {
	WQE_ELM_TYPE_PHYS_ADDR = 1,
	WQE_ELM_TYPE_LIST,
	WQE_ELM_TYPE_LIST_ADDR,
	WQE_ELM_TYPE_LIST_SGL32,
};

enum sgl_element_type {
	SGL_TYPE_PHYS_ADDR = 0,
	SGL_TYPE_LAST_PHYS_ADDR,
	SGL_TYPE_NEXT_LIST,
	SGL_TYPE_EC_LEVEL1_SGL32,
};

struct __rte_packed_begin zsda_admin_req {
	uint16_t msg_type;
	uint8_t data[26];
} __rte_packed_end;

struct __rte_packed_begin zsda_admin_req_qcfg {
	uint16_t msg_type;
	uint8_t qid;
	uint8_t data[25];
} __rte_packed_end;

struct __rte_packed_begin qinfo {
	uint16_t q_type;
	uint16_t wq_tail;
	uint16_t wq_head;
	uint16_t cq_tail;
	uint16_t cq_head;
	uint16_t cycle;
} __rte_packed_end;

struct __rte_packed_begin zsda_admin_resp_qcfg {
	uint16_t msg_type;
	struct qinfo qcfg;
	uint8_t data[14];
} __rte_packed_end;

struct zsda_queue {
	char memz_name[RTE_MEMZONE_NAMESIZE];
	uint8_t *io_addr;
	uint8_t *base_addr;	   /* Base address */
	rte_iova_t base_phys_addr; /* Queue physical address */
	uint16_t head;		   /* Shadow copy of the head */
	uint16_t tail;		   /* Shadow copy of the tail */
	uint16_t modulo_mask;
	uint16_t msg_size;
	uint16_t queue_size;
	uint16_t cycle_size;
	uint16_t pushed_wqe;

	uint8_t hw_queue_number;
	uint32_t csr_head; /* last written head value */
	uint32_t csr_tail; /* last written tail value */

	uint8_t valid;
	uint16_t sid;
};

struct zsda_qp_stat {
	/**< Count of all operations enqueued */
	uint64_t enqueued_count;
	/**< Count of all operations dequeued */
	uint64_t dequeued_count;

	/**< Total error count on operations enqueued */
	uint64_t enqueue_err_count;
	/**< Total error count on operations dequeued */
	uint64_t dequeue_err_count;
};

struct __rte_packed_begin zsda_cqe {
	uint8_t valid; /* cqe_cycle */
	uint8_t op_code;
	uint16_t sid;
	uint8_t state;
	uint8_t result;
	uint16_t zsda_wq_id;
	uint32_t tx_real_length;
	uint16_t err0;
	uint16_t err1;
} __rte_packed_end;

typedef int (*rx_callback)(void *cookie_in, struct zsda_cqe *cqe);
typedef int (*tx_callback)(void *op_in, const struct zsda_queue *queue,
			   void **op_cookies, const uint16_t new_tail);
typedef int (*srv_match)(const void *op_in);

struct qp_srv {
	bool used;
	struct zsda_queue tx_q;
	struct zsda_queue rx_q;
	rx_callback rx_cb;
	tx_callback tx_cb;
	srv_match match;
	struct zsda_qp_stat stats;
	struct rte_mempool *op_cookie_pool;
	void **op_cookies;
	uint16_t nb_descriptors;
};

struct zsda_qp {
	struct qp_srv srv[ZSDA_MAX_SERVICES];
};

struct __rte_packed_begin zsda_buf {
	uint64_t addr;
	uint32_t len;
	uint8_t resrvd[3];
	uint8_t type;
} __rte_packed_end;

struct __rte_cache_aligned zsda_sgl {
	struct zsda_buf buffers[ZSDA_SGL_MAX_NUMBER];
};

struct comp_head_info {
	uint32_t head_len;
	phys_addr_t head_phys_addr;
};

void zsda_queue_delete(const struct zsda_queue *queue);
int zsda_queue_pair_release(struct zsda_qp **qp_addr);
void zsda_stats_get(void **queue_pairs, const uint32_t nb_queue_pairs,
			struct zsda_qp_stat *stats);
void zsda_stats_reset(void **queue_pairs, const uint32_t nb_queue_pairs);

int zsda_sgl_fill(const struct rte_mbuf *buf, uint32_t offset,
		  struct zsda_sgl *sgl, const phys_addr_t sgl_phy_addr,
		  uint32_t remain_len, struct comp_head_info *comp_head_info);

#endif /* _ZSDA_QP_COMMON_H_ */
