/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_QP_H_
#define _BCMFS_QP_H_

#include <rte_memzone.h>

/* Maximum number of h/w queues supported by device */
#define BCMFS_MAX_HW_QUEUES		32

/* H/W queue IO address space len */
#define BCMFS_HW_QUEUE_IO_ADDR_LEN	(64 * 1024)

/* Maximum size of device ops name */
#define BCMFS_HW_OPS_NAMESIZE		32

enum bcmfs_queue_type {
	/* TX or submission queue */
	BCMFS_RM_TXQ,
	 /* Completion or receive queue */
	BCMFS_RM_CPLQ
};

#define BCMFS_QP_IOBASE_XLATE(base, idx)	\
		((base) + ((idx) * BCMFS_HW_QUEUE_IO_ADDR_LEN))

/* Max pkts for preprocessing before submitting to h/w qp */
#define BCMFS_MAX_REQS_BUFF	64

/* qp stats */
struct bcmfs_qp_stats {
	/* Count of all operations enqueued */
	uint64_t enqueued_count;
	/* Count of all operations dequeued */
	uint64_t dequeued_count;
	/* Total error count on operations enqueued */
	uint64_t enqueue_err_count;
	/* Total error count on operations dequeued */
	uint64_t dequeue_err_count;
};

struct bcmfs_qp_config {
	/* Socket to allocate memory on */
	int socket_id;
	/* Mapped iobase for qp */
	void *iobase;
	/* nb_descriptors or requests a h/w queue can accommodate */
	uint16_t nb_descriptors;
	/* Maximum number of h/w descriptors needed by a request */
	uint16_t max_descs_req;
	/* h/w ops associated with qp */
	struct bcmfs_hw_queue_pair_ops *ops;
};

struct bcmfs_queue {
	/* Base virt address */
	void *base_addr;
	/* Base iova */
	rte_iova_t base_phys_addr;
	/* Queue type */
	enum bcmfs_queue_type q_type;
	/* Queue size based on nb_descriptors and max_descs_reqs */
	uint32_t queue_size;
	union {
		/* s/w pointer for tx h/w queue*/
		uint32_t tx_write_ptr;
		/* s/w pointer for completion h/w queue*/
		uint32_t cmpl_read_ptr;
	};
	/* number of inflight descriptor accumulated  before next db ring */
	uint16_t descs_inflight;
	/* Memzone name */
	char memz_name[RTE_MEMZONE_NAMESIZE];
};

struct bcmfs_qp {
	/* Queue-pair ID */
	uint16_t qpair_id;
	/* Mapped IO address */
	void *ioreg;
	/* A TX queue */
	struct bcmfs_queue tx_q;
	/* A Completion queue */
	struct bcmfs_queue cmpl_q;
	/* Number of requests queue can accommodate */
	uint32_t nb_descriptors;
	/* Number of pending requests and enqueued to h/w queue */
	uint16_t nb_pending_requests;
	/* A pool which act as a hash for <request-ID and virt address> pair */
	unsigned long *ctx_pool;
	/* virt address for mem allocated for bitmap */
	void *ctx_bmp_mem;
	/* Bitmap */
	struct rte_bitmap *ctx_bmp;
	/* Associated stats */
	struct bcmfs_qp_stats stats;
	/* h/w ops associated with qp */
	struct bcmfs_hw_queue_pair_ops *ops;
	/* bcmfs requests pool*/
	struct rte_mempool *sr_mp;
	/* a temporary buffer to keep message pointers */
	struct bcmfs_qp_message *infl_msgs[BCMFS_MAX_REQS_BUFF];

} __rte_cache_aligned;

/* Structure defining h/w queue pair operations */
struct bcmfs_hw_queue_pair_ops {
	/* ops name */
	char name[BCMFS_HW_OPS_NAMESIZE];
	/* Enqueue an object */
	int (*enq_one_req)(struct bcmfs_qp *qp, void *obj);
	/* Ring doorbell */
	void (*ring_db)(struct bcmfs_qp *qp);
	/* Dequeue objects */
	uint16_t (*dequeue)(struct bcmfs_qp *qp, void **obj,
			    uint16_t nb_ops);
	/* Start the h/w queue */
	int (*startq)(struct bcmfs_qp *qp);
	/* Stop the h/w queue */
	void (*stopq)(struct bcmfs_qp *qp);
};

uint16_t
bcmfs_enqueue_op_burst(void *qp, void **ops, uint16_t nb_ops);
uint16_t
bcmfs_dequeue_op_burst(void *qp, void **ops, uint16_t nb_ops);
int
bcmfs_qp_release(struct bcmfs_qp **qp_addr);
int
bcmfs_qp_setup(struct bcmfs_qp **qp_addr,
	       uint16_t queue_pair_id,
	       struct bcmfs_qp_config *bcmfs_conf);

/* stats functions*/
void bcmfs_qp_stats_get(struct bcmfs_qp **qp, int num_qp,
			struct bcmfs_qp_stats *stats);
void bcmfs_qp_stats_reset(struct bcmfs_qp **qp, int num_qp);

#endif /* _BCMFS_QP_H_ */
