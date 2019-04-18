/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>

#include "otx_cryptodev_hw_access.h"
#include "otx_cryptodev_mbox.h"

#include "cpt_pmd_logs.h"
#include "cpt_pmd_ops_helper.h"
#include "cpt_hw_types.h"

#define METABUF_POOL_CACHE_SIZE	512

/*
 * VF HAL functions
 * Access its own BAR0/4 registers by passing VF number as 0.
 * OS/PCI maps them accordingly.
 */

static int
otx_cpt_vf_init(struct cpt_vf *cptvf)
{
	int ret = 0;

	/* Check ready with PF */
	/* Gets chip ID / device Id from PF if ready */
	ret = otx_cpt_check_pf_ready(cptvf);
	if (ret) {
		CPT_LOG_ERR("%s: PF not responding to READY msg",
				cptvf->dev_name);
		ret = -EBUSY;
		goto exit;
	}

	CPT_LOG_DP_DEBUG("%s: %s done", cptvf->dev_name, __func__);

exit:
	return ret;
}

/*
 * Read Interrupt status of the VF
 *
 * @param   cptvf	cptvf structure
 */
static uint64_t
otx_cpt_read_vf_misc_intr_status(struct cpt_vf *cptvf)
{
	return CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf), CPTX_VQX_MISC_INT(0, 0));
}

/*
 * Clear mailbox interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_mbox_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.mbox = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear instruction NCB read error interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_irde_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.irde = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear NCB result write response error interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_nwrp_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.nwrp = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear swerr interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_swerr_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.swerr = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear hwerr interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_hwerr_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.hwerr = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear translation fault interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_fault_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.fault = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/*
 * Clear doorbell overflow interrupt of the VF
 *
 * @param   cptvf	cptvf structure
 */
static void
otx_cpt_clear_dovf_intr(struct cpt_vf *cptvf)
{
	cptx_vqx_misc_int_t vqx_misc_int;

	vqx_misc_int.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				      CPTX_VQX_MISC_INT(0, 0));
	/* W1C for the VF */
	vqx_misc_int.s.dovf = 1;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_MISC_INT(0, 0), vqx_misc_int.u);
}

/* Write to VQX_CTL register
 */
static void
otx_cpt_write_vq_ctl(struct cpt_vf *cptvf, bool val)
{
	cptx_vqx_ctl_t vqx_ctl;

	vqx_ctl.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				 CPTX_VQX_CTL(0, 0));
	vqx_ctl.s.ena = val;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_CTL(0, 0), vqx_ctl.u);
}

/* Write to VQX_INPROG register
 */
static void
otx_cpt_write_vq_inprog(struct cpt_vf *cptvf, uint8_t val)
{
	cptx_vqx_inprog_t vqx_inprg;

	vqx_inprg.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				   CPTX_VQX_INPROG(0, 0));
	vqx_inprg.s.inflight = val;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_INPROG(0, 0), vqx_inprg.u);
}

/* Write to VQX_DONE_WAIT NUMWAIT register
 */
static void
otx_cpt_write_vq_done_numwait(struct cpt_vf *cptvf, uint32_t val)
{
	cptx_vqx_done_wait_t vqx_dwait;

	vqx_dwait.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				   CPTX_VQX_DONE_WAIT(0, 0));
	vqx_dwait.s.num_wait = val;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_DONE_WAIT(0, 0), vqx_dwait.u);
}

/* Write to VQX_DONE_WAIT NUM_WAIT register
 */
static void
otx_cpt_write_vq_done_timewait(struct cpt_vf *cptvf, uint16_t val)
{
	cptx_vqx_done_wait_t vqx_dwait;

	vqx_dwait.u = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				   CPTX_VQX_DONE_WAIT(0, 0));
	vqx_dwait.s.time_wait = val;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_DONE_WAIT(0, 0), vqx_dwait.u);
}

/* Write to VQX_SADDR register
 */
static void
otx_cpt_write_vq_saddr(struct cpt_vf *cptvf, uint64_t val)
{
	cptx_vqx_saddr_t vqx_saddr;

	vqx_saddr.u = val;
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VQX_SADDR(0, 0), vqx_saddr.u);
}

static void
otx_cpt_vfvq_init(struct cpt_vf *cptvf)
{
	uint64_t base_addr = 0;

	/* Disable the VQ */
	otx_cpt_write_vq_ctl(cptvf, 0);

	/* Reset the doorbell */
	otx_cpt_write_vq_doorbell(cptvf, 0);
	/* Clear inflight */
	otx_cpt_write_vq_inprog(cptvf, 0);

	/* Write VQ SADDR */
	base_addr = (uint64_t)(cptvf->cqueue.chead[0].dma_addr);
	otx_cpt_write_vq_saddr(cptvf, base_addr);

	/* Configure timerhold / coalescence */
	otx_cpt_write_vq_done_timewait(cptvf, CPT_TIMER_THOLD);
	otx_cpt_write_vq_done_numwait(cptvf, CPT_COUNT_THOLD);

	/* Enable the VQ */
	otx_cpt_write_vq_ctl(cptvf, 1);
}

static int
cpt_vq_init(struct cpt_vf *cptvf, uint8_t group)
{
	int err;

	/* Convey VQ LEN to PF */
	err = otx_cpt_send_vq_size_msg(cptvf);
	if (err) {
		CPT_LOG_ERR("%s: PF not responding to QLEN msg",
			    cptvf->dev_name);
		err = -EBUSY;
		goto cleanup;
	}

	/* CPT VF device initialization */
	otx_cpt_vfvq_init(cptvf);

	/* Send msg to PF to assign currnet Q to required group */
	cptvf->vfgrp = group;
	err = otx_cpt_send_vf_grp_msg(cptvf, group);
	if (err) {
		CPT_LOG_ERR("%s: PF not responding to VF_GRP msg",
			    cptvf->dev_name);
		err = -EBUSY;
		goto cleanup;
	}

	CPT_LOG_DP_DEBUG("%s: %s done", cptvf->dev_name, __func__);
	return 0;

cleanup:
	return err;
}

void
otx_cpt_poll_misc(struct cpt_vf *cptvf)
{
	uint64_t intr;

	intr = otx_cpt_read_vf_misc_intr_status(cptvf);

	if (!intr)
		return;

	/* Check for MISC interrupt types */
	if (likely(intr & CPT_VF_INTR_MBOX_MASK)) {
		CPT_LOG_DP_DEBUG("%s: Mailbox interrupt 0x%lx on CPT VF %d",
			cptvf->dev_name, (unsigned int long)intr, cptvf->vfid);
		otx_cpt_handle_mbox_intr(cptvf);
		otx_cpt_clear_mbox_intr(cptvf);
	} else if (unlikely(intr & CPT_VF_INTR_IRDE_MASK)) {
		otx_cpt_clear_irde_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: Instruction NCB read error interrupt "
				"0x%lx on CPT VF %d", cptvf->dev_name,
				(unsigned int long)intr, cptvf->vfid);
	} else if (unlikely(intr & CPT_VF_INTR_NWRP_MASK)) {
		otx_cpt_clear_nwrp_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: NCB response write error interrupt 0x%lx"
				" on CPT VF %d", cptvf->dev_name,
				(unsigned int long)intr, cptvf->vfid);
	} else if (unlikely(intr & CPT_VF_INTR_SWERR_MASK)) {
		otx_cpt_clear_swerr_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: Software error interrupt 0x%lx on CPT VF "
				"%d", cptvf->dev_name, (unsigned int long)intr,
				cptvf->vfid);
	} else if (unlikely(intr & CPT_VF_INTR_HWERR_MASK)) {
		otx_cpt_clear_hwerr_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: Hardware error interrupt 0x%lx on CPT VF "
				"%d", cptvf->dev_name, (unsigned int long)intr,
				cptvf->vfid);
	} else if (unlikely(intr & CPT_VF_INTR_FAULT_MASK)) {
		otx_cpt_clear_fault_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: Translation fault interrupt 0x%lx on CPT VF "
				"%d", cptvf->dev_name, (unsigned int long)intr,
				cptvf->vfid);
	} else if (unlikely(intr & CPT_VF_INTR_DOVF_MASK)) {
		otx_cpt_clear_dovf_intr(cptvf);
		CPT_LOG_DP_DEBUG("%s: Doorbell overflow interrupt 0x%lx on CPT VF "
				"%d", cptvf->dev_name, (unsigned int long)intr,
				cptvf->vfid);
	} else
		CPT_LOG_DP_ERR("%s: Unhandled interrupt 0x%lx in CPT VF %d",
				cptvf->dev_name, (unsigned int long)intr,
				cptvf->vfid);
}

int
otx_cpt_hw_init(struct cpt_vf *cptvf, void *pdev, void *reg_base, char *name)
{
	memset(cptvf, 0, sizeof(struct cpt_vf));

	/* Bar0 base address */
	cptvf->reg_base = reg_base;

	/* Save device name */
	strlcpy(cptvf->dev_name, name, (sizeof(cptvf->dev_name)));

	cptvf->pdev = pdev;

	/* To clear if there are any pending mbox msgs */
	otx_cpt_poll_misc(cptvf);

	if (otx_cpt_vf_init(cptvf)) {
		CPT_LOG_ERR("Failed to initialize CPT VF device");
		return -1;
	}

	return 0;
}

int
otx_cpt_deinit_device(void *dev)
{
	struct cpt_vf *cptvf = (struct cpt_vf *)dev;

	/* Do misc work one last time */
	otx_cpt_poll_misc(cptvf);

	return 0;
}

static int
otx_cpt_metabuf_mempool_create(const struct rte_cryptodev *dev,
			       struct cpt_instance *instance, uint8_t qp_id,
			       int nb_elements)
{
	char mempool_name[RTE_MEMPOOL_NAMESIZE];
	int sg_mlen, lb_mlen, max_mlen, ret;
	struct cpt_qp_meta_info *meta_info;
	struct rte_mempool *pool;

	/* Get meta len for scatter gather mode */
	sg_mlen = cpt_pmd_ops_helper_get_mlen_sg_mode();

	/* Extra 32B saved for future considerations */
	sg_mlen += 4 * sizeof(uint64_t);

	/* Get meta len for linear buffer (direct) mode */
	lb_mlen = cpt_pmd_ops_helper_get_mlen_direct_mode();

	/* Extra 32B saved for future considerations */
	lb_mlen += 4 * sizeof(uint64_t);

	/* Check max requirement for meta buffer */
	max_mlen = RTE_MAX(lb_mlen, sg_mlen);

	/* Allocate mempool */

	snprintf(mempool_name, RTE_MEMPOOL_NAMESIZE, "otx_cpt_mb_%u:%u",
		 dev->data->dev_id, qp_id);

	pool = rte_mempool_create_empty(mempool_name, nb_elements, max_mlen,
					METABUF_POOL_CACHE_SIZE, 0,
					rte_socket_id(), 0);

	if (pool == NULL) {
		CPT_LOG_ERR("Could not create mempool for metabuf");
		return rte_errno;
	}

	ret = rte_mempool_set_ops_byname(pool, RTE_MBUF_DEFAULT_MEMPOOL_OPS,
					 NULL);
	if (ret) {
		CPT_LOG_ERR("Could not set mempool ops");
		goto mempool_free;
	}

	ret = rte_mempool_populate_default(pool);
	if (ret <= 0) {
		CPT_LOG_ERR("Could not populate metabuf pool");
		goto mempool_free;
	}

	meta_info = &instance->meta_info;

	meta_info->pool = pool;
	meta_info->lb_mlen = lb_mlen;
	meta_info->sg_mlen = sg_mlen;

	return 0;

mempool_free:
	rte_mempool_free(pool);
	return ret;
}

static void
otx_cpt_metabuf_mempool_destroy(struct cpt_instance *instance)
{
	struct cpt_qp_meta_info *meta_info = &instance->meta_info;

	rte_mempool_free(meta_info->pool);

	meta_info->pool = NULL;
	meta_info->lb_mlen = 0;
	meta_info->sg_mlen = 0;
}

int
otx_cpt_get_resource(const struct rte_cryptodev *dev, uint8_t group,
		     struct cpt_instance **instance, uint16_t qp_id)
{
	int ret = -ENOENT, len, qlen, i;
	int chunk_len, chunks, chunk_size;
	struct cpt_vf *cptvf = dev->data->dev_private;
	struct cpt_instance *cpt_instance;
	struct command_chunk *chunk_head = NULL, *chunk_prev = NULL;
	struct command_chunk *chunk = NULL;
	uint8_t *mem;
	const struct rte_memzone *rz;
	uint64_t dma_addr = 0, alloc_len, used_len;
	uint64_t *next_ptr;
	uint64_t pg_sz = sysconf(_SC_PAGESIZE);

	CPT_LOG_DP_DEBUG("Initializing cpt resource %s", cptvf->dev_name);

	cpt_instance = &cptvf->instance;

	memset(&cptvf->cqueue, 0, sizeof(cptvf->cqueue));
	memset(&cptvf->pqueue, 0, sizeof(cptvf->pqueue));

	/* Chunks are of fixed size buffers */
	chunks = DEFAULT_CMD_QCHUNKS;
	chunk_len = DEFAULT_CMD_QCHUNK_SIZE;

	qlen = chunks * chunk_len;
	/* Chunk size includes 8 bytes of next chunk ptr */
	chunk_size = chunk_len * CPT_INST_SIZE + CPT_NEXT_CHUNK_PTR_SIZE;

	/* For command chunk structures */
	len = chunks * RTE_ALIGN(sizeof(struct command_chunk), 8);

	/* For pending queue */
	len += qlen * RTE_ALIGN(sizeof(struct rid), 8);

	/* So that instruction queues start as pg size aligned */
	len = RTE_ALIGN(len, pg_sz);

	/* For Instruction queues */
	len += chunks * RTE_ALIGN(chunk_size, 128);

	/* Wastage after instruction queues */
	len = RTE_ALIGN(len, pg_sz);

	rz = rte_memzone_reserve_aligned(cptvf->dev_name, len, cptvf->node,
					 RTE_MEMZONE_SIZE_HINT_ONLY |
					 RTE_MEMZONE_256MB,
					 RTE_CACHE_LINE_SIZE);
	if (!rz) {
		ret = rte_errno;
		goto exit;
	}

	mem = rz->addr;
	dma_addr = rz->phys_addr;
	alloc_len = len;

	memset(mem, 0, len);

	cpt_instance->rsvd = (uintptr_t)rz;

	ret = otx_cpt_metabuf_mempool_create(dev, cpt_instance, qp_id, qlen);
	if (ret) {
		CPT_LOG_ERR("Could not create mempool for metabuf");
		goto memzone_free;
	}

	/* Pending queue setup */
	cptvf->pqueue.rid_queue = (struct rid *)mem;
	cptvf->pqueue.enq_tail = 0;
	cptvf->pqueue.deq_head = 0;
	cptvf->pqueue.pending_count = 0;

	mem +=  qlen * RTE_ALIGN(sizeof(struct rid), 8);
	len -=  qlen * RTE_ALIGN(sizeof(struct rid), 8);
	dma_addr += qlen * RTE_ALIGN(sizeof(struct rid), 8);

	/* Alignment wastage */
	used_len = alloc_len - len;
	mem += RTE_ALIGN(used_len, pg_sz) - used_len;
	len -= RTE_ALIGN(used_len, pg_sz) - used_len;
	dma_addr += RTE_ALIGN(used_len, pg_sz) - used_len;

	/* Init instruction queues */
	chunk_head = &cptvf->cqueue.chead[0];
	i = qlen;

	chunk_prev = NULL;
	for (i = 0; i < DEFAULT_CMD_QCHUNKS; i++) {
		int csize;

		chunk = &cptvf->cqueue.chead[i];
		chunk->head = mem;
		chunk->dma_addr = dma_addr;

		csize = RTE_ALIGN(chunk_size, 128);
		mem += csize;
		dma_addr += csize;
		len -= csize;

		if (chunk_prev) {
			next_ptr = (uint64_t *)(chunk_prev->head +
						chunk_size - 8);
			*next_ptr = (uint64_t)chunk->dma_addr;
		}
		chunk_prev = chunk;
	}
	/* Circular loop */
	next_ptr = (uint64_t *)(chunk_prev->head + chunk_size - 8);
	*next_ptr = (uint64_t)chunk_head->dma_addr;

	assert(!len);

	/* This is used for CPT(0)_PF_Q(0..15)_CTL.size config */
	cptvf->qsize = chunk_size / 8;
	cptvf->cqueue.qhead = chunk_head->head;
	cptvf->cqueue.idx = 0;
	cptvf->cqueue.cchunk = 0;

	if (cpt_vq_init(cptvf, group)) {
		CPT_LOG_ERR("Failed to initialize CPT VQ of device %s",
			    cptvf->dev_name);
		ret = -EBUSY;
		goto mempool_destroy;
	}

	*instance = cpt_instance;

	CPT_LOG_DP_DEBUG("Crypto device (%s) initialized", cptvf->dev_name);

	return 0;

mempool_destroy:
	otx_cpt_metabuf_mempool_destroy(cpt_instance);
memzone_free:
	rte_memzone_free(rz);
exit:
	*instance = NULL;
	return ret;
}

int
otx_cpt_put_resource(struct cpt_instance *instance)
{
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	struct rte_memzone *rz;

	if (!cptvf) {
		CPT_LOG_ERR("Invalid CPTVF handle");
		return -EINVAL;
	}

	CPT_LOG_DP_DEBUG("Releasing cpt device %s", cptvf->dev_name);

	otx_cpt_metabuf_mempool_destroy(instance);

	rz = (struct rte_memzone *)instance->rsvd;
	rte_memzone_free(rz);
	return 0;
}

int
otx_cpt_start_device(void *dev)
{
	int rc;
	struct cpt_vf *cptvf = (struct cpt_vf *)dev;

	rc = otx_cpt_send_vf_up(cptvf);
	if (rc) {
		CPT_LOG_ERR("Failed to mark CPT VF device %s UP, rc = %d",
			    cptvf->dev_name, rc);
		return -EFAULT;
	}

	if ((cptvf->vftype != SE_TYPE) && (cptvf->vftype != AE_TYPE)) {
		CPT_LOG_ERR("Fatal error, unexpected vf type %u, for CPT VF "
			    "device %s", cptvf->vftype, cptvf->dev_name);
		return -ENOENT;
	}

	return 0;
}

void
otx_cpt_stop_device(void *dev)
{
	int rc;
	uint32_t pending, retries = 5;
	struct cpt_vf *cptvf = (struct cpt_vf *)dev;

	/* Wait for pending entries to complete */
	pending = otx_cpt_read_vq_doorbell(cptvf);
	while (pending) {
		CPT_LOG_DP_DEBUG("%s: Waiting for pending %u cmds to complete",
				 cptvf->dev_name, pending);
		sleep(1);
		pending = otx_cpt_read_vq_doorbell(cptvf);
		retries--;
		if (!retries)
			break;
	}

	if (!retries && pending) {
		CPT_LOG_ERR("%s: Timeout waiting for commands(%u)",
			    cptvf->dev_name, pending);
		return;
	}

	rc = otx_cpt_send_vf_down(cptvf);
	if (rc) {
		CPT_LOG_ERR("Failed to bring down vf %s, rc %d",
			    cptvf->dev_name, rc);
		return;
	}
}
