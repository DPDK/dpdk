/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _CPFL_CONTROLQ_H_
#define _CPFL_CONTROLQ_H_

#include "base/idpf_osdep.h"
#include "base/idpf_controlq_api.h"

#define CPFL_CTLQ_DESCRIPTOR_SIZE	32
#define CPFL_CTLQ_MAILBOX_BUFFER_SIZE	4096
#define CPFL_CTLQ_CFGQ_BUFFER_SIZE	256
#define CPFL_DFLT_MBX_RING_LEN		512
#define CPFL_CFGQ_RING_LEN		512

/* CRQ/CSQ specific error codes */
#define CPFL_ERR_CTLQ_ERROR             -74     /* -EBADMSG */
#define CPFL_ERR_CTLQ_TIMEOUT           -110    /* -ETIMEDOUT */
#define CPFL_ERR_CTLQ_FULL              -28     /* -ENOSPC */
#define CPFL_ERR_CTLQ_NO_WORK           -42     /* -ENOMSG */
#define CPFL_ERR_CTLQ_EMPTY             -105    /* -ENOBUFS */

/* Generic queue info structures */
/* MB, CONFIG and EVENT q do not have extended info */
struct cpfl_ctlq_create_info {
	enum idpf_ctlq_type type;
	int id; /* absolute queue offset passed as input
		 * -1 for default mailbox if present
		 */
	uint16_t len; /* Queue length passed as input */
	uint16_t buf_size; /* buffer size passed as input */
	uint64_t base_address; /* output, HPA of the Queue start  */
	struct idpf_ctlq_reg reg; /* registers accessed by ctlqs */
	/* Pass down previously allocated descriptor ring and buffer memory
	 * for each control queue to be created
	 */
	struct idpf_dma_mem ring_mem;
	/* The CP will allocate one large buffer that the CPFlib will piece
	 * into individual buffers for each descriptor
	 */
	struct idpf_dma_mem buf_mem;

	int ext_info_size;
	void *ext_info; /* Specific to q type */
};

int cpfl_ctlq_alloc_ring_res(struct idpf_hw *hw,
			     struct idpf_ctlq_info *cq,
			     struct cpfl_ctlq_create_info *qinfo);
int cpfl_ctlq_add(struct idpf_hw *hw,
		  struct cpfl_ctlq_create_info *qinfo,
		  struct idpf_ctlq_info **cq);
int cpfl_ctlq_send(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
		   u16 num_q_msg, struct idpf_ctlq_msg q_msg[]);
int cpfl_ctlq_clean_sq(struct idpf_ctlq_info *cq, u16 *clean_count,
		       struct idpf_ctlq_msg *msg_status[]);
int cpfl_ctlq_post_rx_buffs(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
			    u16 *buff_count, struct idpf_dma_mem **buffs);
int cpfl_ctlq_recv(struct idpf_ctlq_info *cq, u16 *num_q_msg,
		   struct idpf_ctlq_msg *q_msg);
int cpfl_vport_ctlq_add(struct idpf_hw *hw,
			struct cpfl_ctlq_create_info *qinfo,
			struct idpf_ctlq_info **cq);
void cpfl_vport_ctlq_remove(struct idpf_hw *hw, struct idpf_ctlq_info *cq);
int cpfl_vport_ctlq_send(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
			 u16 num_q_msg, struct idpf_ctlq_msg q_msg[]);
int cpfl_vport_ctlq_recv(struct idpf_ctlq_info *cq, u16 *num_q_msg,
			 struct idpf_ctlq_msg q_msg[]);

int cpfl_vport_ctlq_post_rx_buffs(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
				  u16 *buff_count, struct idpf_dma_mem **buffs);
int cpfl_vport_ctlq_clean_sq(struct idpf_ctlq_info *cq, u16 *clean_count,
			     struct idpf_ctlq_msg *msg_status[]);
#endif
