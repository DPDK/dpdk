/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */
#ifndef _OTX_CRYPTODEV_HW_ACCESS_H_
#define _OTX_CRYPTODEV_HW_ACCESS_H_

#include <stdbool.h>

#include <rte_branch_prediction.h>
#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_memory.h>
#include <rte_prefetch.h>

#include "cpt_common.h"
#include "cpt_hw_types.h"
#include "cpt_mcode_defines.h"
#include "cpt_pmd_logs.h"

#define CPT_INTR_POLL_INTERVAL_MS	(50)

/* Default command queue length */
#define DEFAULT_CMD_QCHUNKS		2
#define DEFAULT_CMD_QCHUNK_SIZE		1023
#define DEFAULT_CMD_QLEN \
		(DEFAULT_CMD_QCHUNK_SIZE * DEFAULT_CMD_QCHUNKS)

#define CPT_CSR_REG_BASE(cpt)		((cpt)->reg_base)

/* Read hw register */
#define CPT_READ_CSR(__hw_addr, __offset) \
	rte_read64_relaxed((uint8_t *)__hw_addr + __offset)

/* Write hw register */
#define CPT_WRITE_CSR(__hw_addr, __offset, __val) \
	rte_write64_relaxed((__val), ((uint8_t *)__hw_addr + __offset))

/* cpt instance */
struct cpt_instance {
	uint32_t queue_id;
	uintptr_t rsvd;
	struct rte_mempool *sess_mp;
	struct rte_mempool *sess_mp_priv;
	struct cpt_qp_meta_info meta_info;
};

struct command_chunk {
	/** 128-byte aligned real_vaddr */
	uint8_t *head;
	/** 128-byte aligned real_dma_addr */
	phys_addr_t dma_addr;
};

/**
 * Command queue structure
 */
struct command_queue {
	/** Command queue host write idx */
	uint32_t idx;
	/** Command queue chunk */
	uint32_t cchunk;
	/** Command queue head; instructions are inserted here */
	uint8_t *qhead;
	/** Command chunk list head */
	struct command_chunk chead[DEFAULT_CMD_QCHUNKS];
};

/**
 * CPT VF device structure
 */
struct cpt_vf {
	/** CPT instance */
	struct cpt_instance instance;
	/** Register start address */
	uint8_t *reg_base;
	/** Command queue information */
	struct command_queue cqueue;
	/** Pending queue information */
	struct pending_queue pqueue;

	/** Below fields are accessed only in control path */

	/** Env specific pdev representing the pci dev */
	void *pdev;
	/** Calculated queue size */
	uint32_t qsize;
	/** Device index (0...CPT_MAX_VQ_NUM)*/
	uint8_t  vfid;
	/** VF type of cpt_vf_type_t (SE_TYPE(2) or AE_TYPE(1) */
	uint8_t  vftype;
	/** VF group (0 - 8) */
	uint8_t  vfgrp;
	/** Operating node: Bits (46:44) in BAR0 address */
	uint8_t  node;

	/** VF-PF mailbox communication */

	/** Flag if acked */
	bool pf_acked;
	/** Flag if not acked */
	bool pf_nacked;

	/** Device name */
	char dev_name[32];
} __rte_cache_aligned;

/*
 * CPT Registers map for 81xx
 */

/* VF registers */
#define CPTX_VQX_CTL(a, b)		(0x0000100ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_SADDR(a, b)		(0x0000200ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_DONE_WAIT(a, b)	(0x0000400ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_INPROG(a, b)		(0x0000410ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_DONE(a, b)		(0x0000420ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ACK(a, b)		(0x0000440ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_INT_W1S(a, b)	(0x0000460ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_INT_W1C(a, b)	(0x0000468ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ENA_W1S(a, b)	(0x0000470ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ENA_W1C(a, b)	(0x0000478ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_INT(a, b)		(0x0000500ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_INT_W1S(a, b)	(0x0000508ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_ENA_W1S(a, b)	(0x0000510ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_ENA_W1C(a, b)	(0x0000518ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DOORBELL(a, b)		(0x0000600ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VFX_PF_MBOXX(a, b, c)	(0x0001000ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b) + \
					 8ll * ((c) & 0x1))

/* VF HAL functions */

void
otx_cpt_poll_misc(struct cpt_vf *cptvf);

int
otx_cpt_hw_init(struct cpt_vf *cptvf, void *pdev, void *reg_base, char *name);

int
otx_cpt_deinit_device(void *dev);

int
otx_cpt_get_resource(const struct rte_cryptodev *dev, uint8_t group,
		     struct cpt_instance **instance, uint16_t qp_id);

int
otx_cpt_put_resource(struct cpt_instance *instance);

int
otx_cpt_start_device(void *cptvf);

void
otx_cpt_stop_device(void *cptvf);

/* Write to VQX_DOORBELL register
 */
static __rte_always_inline void
otx_cpt_write_vq_doorbell(struct cpt_vf *cptvf, uint32_t val)
{
	cptx_vqx_doorbell_t vqx_dbell;

	vqx_dbell.u = 0;
	vqx_dbell.s.dbell_cnt = val * 8; /* Num of Instructions * 8 words */
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_DOORBELL(0, 0), vqx_dbell.u);
}

static __rte_always_inline uint32_t
otx_cpt_read_vq_doorbell(struct cpt_vf *cptvf)
{
	cptx_vqx_doorbell_t vqx_dbell;

	vqx_dbell.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				   CPTX_VQX_DOORBELL(0, 0));
	return vqx_dbell.s.dbell_cnt;
}

static __rte_always_inline void
otx_cpt_ring_dbell(struct cpt_instance *instance, uint16_t count)
{
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	/* Memory barrier to flush pending writes */
	rte_smp_wmb();
	otx_cpt_write_vq_doorbell(cptvf, count);
}

static __rte_always_inline void *
get_cpt_inst(struct command_queue *cqueue)
{
	CPT_LOG_DP_DEBUG("CPT queue idx %u\n", cqueue->idx);
	return &cqueue->qhead[cqueue->idx * CPT_INST_SIZE];
}

static __rte_always_inline void
fill_cpt_inst(struct cpt_instance *instance, void *req)
{
	struct command_queue *cqueue;
	cpt_inst_s_t *cpt_ist_p;
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	struct cpt_request_info *user_req = (struct cpt_request_info *)req;
	cqueue = &cptvf->cqueue;
	cpt_ist_p = get_cpt_inst(cqueue);
	rte_prefetch_non_temporal(cpt_ist_p);

	/* EI0, EI1, EI2, EI3 are already prepared */
	/* HW W0 */
	cpt_ist_p->u[0] = 0;
	/* HW W1 */
	cpt_ist_p->s8x.res_addr = user_req->comp_baddr;
	/* HW W2 */
	cpt_ist_p->u[2] = 0;
	/* HW W3 */
	cpt_ist_p->s8x.wq_ptr = 0;

	/* MC EI0 */
	cpt_ist_p->s8x.ei0 = user_req->ist.ei0;
	/* MC EI1 */
	cpt_ist_p->s8x.ei1 = user_req->ist.ei1;
	/* MC EI2 */
	cpt_ist_p->s8x.ei2 = user_req->ist.ei2;
	/* MC EI3 */
	cpt_ist_p->s8x.ei3 = user_req->ist.ei3;
}

static __rte_always_inline void
mark_cpt_inst(struct cpt_instance *instance)
{
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	struct command_queue *queue = &cptvf->cqueue;
	if (unlikely(++queue->idx >= DEFAULT_CMD_QCHUNK_SIZE)) {
		uint32_t cchunk = queue->cchunk;
		MOD_INC(cchunk, DEFAULT_CMD_QCHUNKS);
		queue->qhead = queue->chead[cchunk].head;
		queue->idx = 0;
		queue->cchunk = cchunk;
	}
}

static __rte_always_inline uint8_t
check_nb_command_id(struct cpt_request_info *user_req,
		struct cpt_instance *instance)
{
	uint8_t ret = ERR_REQ_PENDING;
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	volatile cpt_res_s_t *cptres;

	cptres = (volatile cpt_res_s_t *)user_req->completion_addr;

	if (unlikely(cptres->s8x.compcode == CPT_8X_COMP_E_NOTDONE)) {
		/*
		 * Wait for some time for this command to get completed
		 * before timing out
		 */
		if (rte_get_timer_cycles() < user_req->time_out)
			return ret;
		/*
		 * TODO: See if alternate caddr can be used to not loop
		 * longer than needed.
		 */
		if ((cptres->s8x.compcode == CPT_8X_COMP_E_NOTDONE) &&
		    (user_req->extra_time < TIME_IN_RESET_COUNT)) {
			user_req->extra_time++;
			return ret;
		}

		if (cptres->s8x.compcode != CPT_8X_COMP_E_NOTDONE)
			goto complete;

		ret = ERR_REQ_TIMEOUT;
		CPT_LOG_DP_ERR("Request %p timedout", user_req);
		otx_cpt_poll_misc(cptvf);
		goto exit;
	}

complete:
	if (likely(cptres->s8x.compcode == CPT_8X_COMP_E_GOOD)) {
		ret = 0; /* success */
		if (unlikely((uint8_t)*user_req->alternate_caddr)) {
			ret = (uint8_t)*user_req->alternate_caddr;
			CPT_LOG_DP_ERR("Request %p : failed with microcode"
				" error, MC completion code : 0x%x", user_req,
				ret);
		}
		CPT_LOG_DP_DEBUG("MC status %.8x\n",
			   *((volatile uint32_t *)user_req->alternate_caddr));
		CPT_LOG_DP_DEBUG("HW status %.8x\n",
			   *((volatile uint32_t *)user_req->completion_addr));
	} else if ((cptres->s8x.compcode == CPT_8X_COMP_E_SWERR) ||
		   (cptres->s8x.compcode == CPT_8X_COMP_E_FAULT)) {
		ret = (uint8_t)*user_req->alternate_caddr;
		if (!ret)
			ret = ERR_BAD_ALT_CCODE;
		CPT_LOG_DP_DEBUG("Request %p : failed with %s : err code :%x",
			   user_req,
			   (cptres->s8x.compcode == CPT_8X_COMP_E_FAULT) ?
			   "DMA Fault" : "Software error", ret);
	} else {
		CPT_LOG_DP_ERR("Request %p : unexpected completion code %d",
			   user_req, cptres->s8x.compcode);
		ret = (uint8_t)*user_req->alternate_caddr;
	}

exit:
	return ret;
}

#endif /* _OTX_CRYPTODEV_HW_ACCESS_H_ */
