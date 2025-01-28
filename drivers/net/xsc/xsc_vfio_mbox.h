/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_CMDQ_H_
#define _XSC_CMDQ_H_

#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>
#include <rte_byteorder.h>
#include <rte_io.h>

#include "xsc_dev.h"
#include "xsc_cmd.h"

#define XSC_CMDQ_DATA_SIZE		512
#define XSC_CMDQ_REQ_INLINE_SIZE	8
#define XSC_CMDQ_RSP_INLINE_SIZE	14

struct xsc_cmdq_config {
	uint32_t req_pid_addr;
	uint32_t req_cid_addr;
	uint32_t rsp_pid_addr;
	uint32_t rsp_cid_addr;
	uint32_t req_h_addr;
	uint32_t req_l_addr;
	uint32_t rsp_h_addr;
	uint32_t rsp_l_addr;
	uint32_t elt_sz_addr;
	uint32_t depth_addr;
};

struct xsc_cmd_queue {
	struct xsc_cmdq_req_layout *req_lay;
	struct xsc_cmdq_rsp_layout *rsp_lay;
	const struct rte_memzone *req_mz;
	const struct rte_memzone *rsp_mz;
	uint32_t req_pid;
	uint32_t rsp_cid;
	uint8_t  owner_bit; /* CMDQ owner bit */
	uint8_t  owner_learn; /* Learn ownerbit from hw */
	uint8_t  depth_n; /* Log 2 of CMDQ depth */
	uint8_t  depth_m; /* CMDQ depth mask */
	struct rte_mempool *mbox_buf_pool; /* CMDQ data pool */
	struct xsc_cmdq_config *config;
	rte_spinlock_t lock;
};

struct xsc_cmdq_mbox_buf {
	uint8_t    data[XSC_CMDQ_DATA_SIZE];
	uint8_t    rsv0[48];
	rte_be64_t next; /* Next buf dma addr */
	rte_be32_t block_num;
	uint8_t    owner_status;
	uint8_t    token;
	uint8_t    ctrl_sig;
	uint8_t    sig;
};

struct xsc_cmdq_mbox {
	struct xsc_cmdq_mbox_buf *buf;
	rte_iova_t buf_dma;
	struct xsc_cmdq_mbox     *next;
};

/* CMDQ request msg inline */
struct xsc_cmdq_req_hdr {
	rte_be32_t data[XSC_CMDQ_REQ_INLINE_SIZE];
};

struct xsc_cmdq_req_msg {
	uint32_t                  len;
	struct xsc_cmdq_req_hdr    hdr;
	struct xsc_cmdq_mbox       *next;
};

/* CMDQ response msg inline */
struct xsc_cmdq_rsp_hdr {
	rte_be32_t data[XSC_CMDQ_RSP_INLINE_SIZE];
};

struct xsc_cmdq_rsp_msg {
	uint32_t                  len;
	struct xsc_cmdq_rsp_hdr    hdr;
	struct xsc_cmdq_mbox       *next;
};

/* HW will use this for some records(e.g. vf_id) */
struct xsc_cmdq_rsv {
	uint16_t vf_id;
	uint8_t  rsv[2];
};

/* CMDQ request entry layout */
struct xsc_cmdq_req_layout {
	struct xsc_cmdq_rsv rsv0;
	rte_be32_t          inlen;
	rte_be64_t          in_ptr;
	rte_be32_t          in[XSC_CMDQ_REQ_INLINE_SIZE];
	rte_be64_t          out_ptr;
	rte_be32_t          outlen;
	uint8_t             token;
	uint8_t             sig;
	uint8_t             idx;
	uint8_t             type:7;
	uint8_t             owner_bit:1;
};

/* CMDQ response entry layout */
struct xsc_cmdq_rsp_layout {
	struct xsc_cmdq_rsv rsv0;
	rte_be32_t          out[XSC_CMDQ_RSP_INLINE_SIZE];
	uint8_t             token;
	uint8_t             sig;
	uint8_t             idx;
	uint8_t             type:7;
	uint8_t             owner_bit:1;
};

struct xsc_cmdq_dummy_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint8_t                  rsv[8];
};

struct xsc_cmdq_dummy_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t                   rsv[8];
};

struct xsc_vfio_priv {
	struct xsc_cmd_queue *cmdq;
};

int xsc_vfio_mbox_init(struct xsc_dev *xdev);
void xsc_vfio_mbox_destroy(struct xsc_cmd_queue *cmdq);
int xsc_vfio_mbox_exec(struct xsc_dev *xdev,
		       void *data_in, int in_len,
		       void *data_out, int out_len);

#endif /* _XSC_CMDQ_H_ */
