/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define TIME_SEC_IN_MS 1000

static int
roc_ml_reg_wait_to_clear(struct roc_ml *roc_ml, uint64_t offset, uint64_t mask)
{
	uint64_t start_cycle;
	uint64_t wait_cycles;
	uint64_t reg_val;

	wait_cycles = (ROC_ML_TIMEOUT_MS * plt_tsc_hz()) / TIME_SEC_IN_MS;
	start_cycle = plt_tsc_cycles();
	do {
		reg_val = roc_ml_reg_read64(roc_ml, offset);

		if (!(reg_val & mask))
			return 0;
	} while (plt_tsc_cycles() - start_cycle < wait_cycles);

	return -ETIME;
}

uint64_t
roc_ml_reg_read64(struct roc_ml *roc_ml, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	return plt_read64(PLT_PTR_ADD(ml->ml_reg_addr, offset));
}

void
roc_ml_reg_write64(struct roc_ml *roc_ml, uint64_t val, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	plt_write64(val, PLT_PTR_ADD(ml->ml_reg_addr, offset));
}

uint32_t
roc_ml_reg_read32(struct roc_ml *roc_ml, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	return plt_read32(PLT_PTR_ADD(ml->ml_reg_addr, offset));
}

void
roc_ml_reg_write32(struct roc_ml *roc_ml, uint32_t val, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	plt_write32(val, PLT_PTR_ADD(ml->ml_reg_addr, offset));
}

void
roc_ml_reg_save(struct roc_ml *roc_ml, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	if (offset == ML_MLR_BASE) {
		ml->ml_mlr_base =
			FIELD_GET(ROC_ML_MLR_BASE_BASE, roc_ml_reg_read64(roc_ml, offset));
		ml->ml_mlr_base_saved = true;
	}
}

void *
roc_ml_addr_ap2mlip(struct roc_ml *roc_ml, void *addr)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);
	uint64_t ml_mlr_base;

	ml_mlr_base = (ml->ml_mlr_base_saved) ? ml->ml_mlr_base :
						FIELD_GET(ROC_ML_MLR_BASE_BASE,
							  roc_ml_reg_read64(roc_ml, ML_MLR_BASE));
	return PLT_PTR_ADD(addr, ML_AXI_START_ADDR - ml_mlr_base);
}

void *
roc_ml_addr_mlip2ap(struct roc_ml *roc_ml, void *addr)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);
	uint64_t ml_mlr_base;

	ml_mlr_base = (ml->ml_mlr_base_saved) ? ml->ml_mlr_base :
						FIELD_GET(ROC_ML_MLR_BASE_BASE,
							  roc_ml_reg_read64(roc_ml, ML_MLR_BASE));
	return PLT_PTR_ADD(addr, ml_mlr_base - ML_AXI_START_ADDR);
}

uint64_t
roc_ml_addr_pa_to_offset(struct roc_ml *roc_ml, uint64_t phys_addr)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	if (roc_model_is_cn10ka())
		return phys_addr - ml->pci_dev->mem_resource[0].phys_addr;
	else
		return phys_addr - ml->pci_dev->mem_resource[0].phys_addr - ML_MLAB_BLK_OFFSET;
}

uint64_t
roc_ml_addr_offset_to_pa(struct roc_ml *roc_ml, uint64_t offset)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	if (roc_model_is_cn10ka())
		return ml->pci_dev->mem_resource[0].phys_addr + offset;
	else
		return ml->pci_dev->mem_resource[0].phys_addr + ML_MLAB_BLK_OFFSET + offset;
}

void
roc_ml_scratch_write_job(struct roc_ml *roc_ml, void *work_ptr)
{
	union ml_scratch_work_ptr_s reg_work_ptr;
	union ml_scratch_fw_ctrl_s reg_fw_ctrl;

	reg_work_ptr.u64 = 0;
	reg_work_ptr.s.work_ptr = PLT_U64_CAST(roc_ml_addr_ap2mlip(roc_ml, work_ptr));

	reg_fw_ctrl.u64 = 0;
	reg_fw_ctrl.s.valid = 1;

	roc_ml_reg_write64(roc_ml, reg_work_ptr.u64, ML_SCRATCH_WORK_PTR);
	roc_ml_reg_write64(roc_ml, reg_fw_ctrl.u64, ML_SCRATCH_FW_CTRL);
}

bool
roc_ml_scratch_is_valid_bit_set(struct roc_ml *roc_ml)
{
	union ml_scratch_fw_ctrl_s reg_fw_ctrl;

	reg_fw_ctrl.u64 = roc_ml_reg_read64(roc_ml, ML_SCRATCH_FW_CTRL);

	if (reg_fw_ctrl.s.valid == 1)
		return true;

	return false;
}

bool
roc_ml_scratch_is_done_bit_set(struct roc_ml *roc_ml)
{
	union ml_scratch_fw_ctrl_s reg_fw_ctrl;

	reg_fw_ctrl.u64 = roc_ml_reg_read64(roc_ml, ML_SCRATCH_FW_CTRL);

	if (reg_fw_ctrl.s.done == 1)
		return true;

	return false;
}

bool
roc_ml_scratch_enqueue(struct roc_ml *roc_ml, void *work_ptr)
{
	union ml_scratch_work_ptr_s reg_work_ptr;
	union ml_scratch_fw_ctrl_s reg_fw_ctrl;
	bool ret = false;

	reg_work_ptr.u64 = 0;
	reg_work_ptr.s.work_ptr = PLT_U64_CAST(roc_ml_addr_ap2mlip(roc_ml, work_ptr));

	reg_fw_ctrl.u64 = 0;
	reg_fw_ctrl.s.valid = 1;

	if (plt_spinlock_trylock(&roc_ml->sp_spinlock) != 0) {
		bool valid = roc_ml_scratch_is_valid_bit_set(roc_ml);
		bool done = roc_ml_scratch_is_done_bit_set(roc_ml);

		if (valid == done) {
			roc_ml_clk_force_on(roc_ml);
			roc_ml_dma_stall_off(roc_ml);

			roc_ml_reg_write64(roc_ml, reg_work_ptr.u64, ML_SCRATCH_WORK_PTR);
			roc_ml_reg_write64(roc_ml, reg_fw_ctrl.u64, ML_SCRATCH_FW_CTRL);

			ret = true;
		}
		plt_spinlock_unlock(&roc_ml->sp_spinlock);
	}

	return ret;
}

bool
roc_ml_scratch_dequeue(struct roc_ml *roc_ml, void *work_ptr)
{
	union ml_scratch_work_ptr_s reg_work_ptr;
	bool ret = false;

	if (plt_spinlock_trylock(&roc_ml->sp_spinlock) != 0) {
		bool valid = roc_ml_scratch_is_valid_bit_set(roc_ml);
		bool done = roc_ml_scratch_is_done_bit_set(roc_ml);

		if (valid && done) {
			reg_work_ptr.u64 = roc_ml_reg_read64(roc_ml, ML_SCRATCH_WORK_PTR);
			if (work_ptr ==
			    roc_ml_addr_mlip2ap(roc_ml, PLT_PTR_CAST(reg_work_ptr.u64))) {
				roc_ml_dma_stall_on(roc_ml);
				roc_ml_clk_force_off(roc_ml);

				roc_ml_reg_write64(roc_ml, 0, ML_SCRATCH_WORK_PTR);
				roc_ml_reg_write64(roc_ml, 0, ML_SCRATCH_FW_CTRL);
				ret = true;
			}
		}
		plt_spinlock_unlock(&roc_ml->sp_spinlock);
	}

	return ret;
}

void
roc_ml_scratch_queue_reset(struct roc_ml *roc_ml)
{
	if (plt_spinlock_trylock(&roc_ml->sp_spinlock) != 0) {
		roc_ml_dma_stall_on(roc_ml);
		roc_ml_clk_force_off(roc_ml);
		roc_ml_reg_write64(roc_ml, 0, ML_SCRATCH_WORK_PTR);
		roc_ml_reg_write64(roc_ml, 0, ML_SCRATCH_FW_CTRL);
		plt_spinlock_unlock(&roc_ml->sp_spinlock);
	}
}

bool
roc_ml_jcmdq_enqueue_lf(struct roc_ml *roc_ml, struct ml_job_cmd_s *job_cmd)
{
	bool ret = false;

	if (FIELD_GET(ROC_ML_JCMDQ_STATUS_AVAIL_COUNT,
		      roc_ml_reg_read64(roc_ml, ML_JCMDQ_STATUS)) != 0) {
		roc_ml_reg_write64(roc_ml, job_cmd->w0.u64, ML_JCMDQ_IN(0));
		roc_ml_reg_write64(roc_ml, job_cmd->w1.u64, ML_JCMDQ_IN(1));
		ret = true;
	}

	return ret;
}

bool
roc_ml_jcmdq_enqueue_sl(struct roc_ml *roc_ml, struct ml_job_cmd_s *job_cmd)
{
	bool ret = false;

	if (plt_spinlock_trylock(&roc_ml->fp_spinlock) != 0) {
		if (FIELD_GET(ROC_ML_JCMDQ_STATUS_AVAIL_COUNT,
			      roc_ml_reg_read64(roc_ml, ML_JCMDQ_STATUS)) != 0) {
			roc_ml_reg_write64(roc_ml, job_cmd->w0.u64, ML_JCMDQ_IN(0));
			roc_ml_reg_write64(roc_ml, job_cmd->w1.u64, ML_JCMDQ_IN(1));
			ret = true;
		}
		plt_spinlock_unlock(&roc_ml->fp_spinlock);
	}

	return ret;
}

void
roc_ml_clk_force_on(struct roc_ml *roc_ml)
{
	uint64_t reg_val = 0;

	reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
	reg_val |= ROC_ML_CFG_MLIP_CLK_FORCE;
	roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);
}

void
roc_ml_clk_force_off(struct roc_ml *roc_ml)
{
	uint64_t reg_val = 0;

	roc_ml_reg_write64(roc_ml, 0, ML_SCRATCH_WORK_PTR);

	reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
	reg_val &= ~ROC_ML_CFG_MLIP_CLK_FORCE;
	roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);
}

void
roc_ml_dma_stall_on(struct roc_ml *roc_ml)
{
	uint64_t reg_val = 0;

	reg_val = roc_ml_reg_read64(roc_ml, ML_JOB_MGR_CTRL);
	reg_val |= ROC_ML_JOB_MGR_CTRL_STALL_ON_IDLE;
	roc_ml_reg_write64(roc_ml, reg_val, ML_JOB_MGR_CTRL);
}

void
roc_ml_dma_stall_off(struct roc_ml *roc_ml)
{
	uint64_t reg_val = 0;

	reg_val = roc_ml_reg_read64(roc_ml, ML_JOB_MGR_CTRL);
	reg_val &= ~ROC_ML_JOB_MGR_CTRL_STALL_ON_IDLE;
	roc_ml_reg_write64(roc_ml, reg_val, ML_JOB_MGR_CTRL);
}

bool
roc_ml_mlip_is_enabled(struct roc_ml *roc_ml)
{
	uint64_t reg_val;

	reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);

	if ((reg_val & ROC_ML_CFG_MLIP_ENA) != 0)
		return true;

	return false;
}

int
roc_ml_mlip_reset(struct roc_ml *roc_ml, bool force)
{
	uint64_t reg_val;

	/* Force reset */
	if (force) {
		/* Set ML(0)_CFG[ENA] = 0. */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

		/* Set ML(0)_CFG[MLIP_ENA] = 0. */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_MLIP_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

		/* Clear ML_MLR_BASE */
		roc_ml_reg_write64(roc_ml, 0, ML_MLR_BASE);
	}

	if (roc_model_is_cn10ka()) {
		/* Wait for all active jobs to finish.
		 * ML_CFG[ENA] : When set, MLW will accept job commands. This
		 * bit can be cleared at any time. If [BUSY] is set, software
		 * must wait until [BUSY] == 0 before setting this bit.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_CFG, ROC_ML_CFG_BUSY);

		/* (1) Set ML(0)_AXI_BRIDGE_CTRL(0..1)[FENCE] = 1 to instruct
		 * the AXI bridge not to accept any new transactions from MLIP.
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(1));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));

		/* (2) Wait until ML(0)_AXI_BRIDGE_CTRL(0..1)[BUSY] = 0 which
		 * indicates that there is no outstanding transactions on
		 * AXI-NCB paths.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_AXI_BRIDGE_CTRL(0),
					 ROC_ML_AXI_BRIDGE_CTRL_BUSY);
		roc_ml_reg_wait_to_clear(roc_ml, ML_AXI_BRIDGE_CTRL(1),
					 ROC_ML_AXI_BRIDGE_CTRL_BUSY);

		/* (3) Wait until ML(0)_JOB_MGR_CTRL[BUSY] = 0 which indicates
		 * that there are no pending jobs in the MLW's job manager.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_JOB_MGR_CTRL, ROC_ML_JOB_MGR_CTRL_BUSY);

		/* (4) Set ML(0)_CFG[ENA] = 0. */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

		/* (5) Set ML(0)_CFG[MLIP_ENA] = 0. */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_MLIP_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

		/* (6) Set ML(0)_AXI_BRIDGE_CTRL(0..1)[FENCE] = 0.*/
		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val &= ~ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));
	}

	if (roc_model_is_cnf10kb()) {
		/* (1) Clear MLAB(0)_CFG[ENA]. Any new jobs will bypass the job
		 * execution stages and their completions will be returned to
		 * PSM.
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

		/* (2) Quiesce the ACC and DMA AXI interfaces: For each of the
		 * two MLAB(0)_AXI_BRIDGE_CTRL(0..1) registers:
		 *
		 * (a) Set MLAB(0)_AXI_BRIDGE_CTRL(0..1)[FENCE] to block new AXI
		 * commands from MLIP.
		 *
		 * (b) Poll MLAB(0)_AXI_BRIDGE_CTRL(0..1)[BUSY] == 0.
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));

		roc_ml_reg_wait_to_clear(roc_ml, ML_AXI_BRIDGE_CTRL(0),
					 ROC_ML_AXI_BRIDGE_CTRL_BUSY);

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(1));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));

		roc_ml_reg_wait_to_clear(roc_ml, ML_AXI_BRIDGE_CTRL(1),
					 ROC_ML_AXI_BRIDGE_CTRL_BUSY);

		/* (3) Clear MLAB(0)_CFG[MLIP_ENA] to reset MLIP.
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_CFG);
		reg_val &= ~ROC_ML_CFG_MLIP_ENA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_CFG);

cnf10kb_mlip_reset_stage_4a:
		/* (4) Flush any outstanding jobs in MLAB's job execution
		 * stages:
		 *
		 * (a) Wait for completion stage to clear:
		 *   - Poll MLAB(0)_STG(0..2)_STATUS[VALID] == 0.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_STGX_STATUS(0), ROC_ML_STG_STATUS_VALID);
		roc_ml_reg_wait_to_clear(roc_ml, ML_STGX_STATUS(1), ROC_ML_STG_STATUS_VALID);
		roc_ml_reg_wait_to_clear(roc_ml, ML_STGX_STATUS(2), ROC_ML_STG_STATUS_VALID);

cnf10kb_mlip_reset_stage_4b:
		/* (4b) Clear job run stage: Poll
		 * MLAB(0)_STG_CONTROL[RUN_TO_COMP] == 0.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_STG_CONTROL, ROC_ML_STG_CONTROL_RUN_TO_COMP);

		/* (4b) Clear job run stage: If MLAB(0)_STG(1)_STATUS[VALID] ==
		 * 1:
		 *     - Set MLAB(0)_STG_CONTROL[RUN_TO_COMP].
		 *     - Poll MLAB(0)_STG_CONTROL[RUN_TO_COMP] == 0.
		 *     - Repeat step (a) to clear job completion stage.
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_STGX_STATUS(1));
		if (reg_val & ROC_ML_STG_STATUS_VALID) {
			reg_val = roc_ml_reg_read64(roc_ml, ML_STG_CONTROL);
			reg_val |= ROC_ML_STG_CONTROL_RUN_TO_COMP;
			roc_ml_reg_write64(roc_ml, reg_val, ML_STG_CONTROL);

			roc_ml_reg_wait_to_clear(roc_ml, ML_STG_CONTROL,
						 ROC_ML_STG_CONTROL_RUN_TO_COMP);

			goto cnf10kb_mlip_reset_stage_4a;
		}

		/* (4c) Clear job fetch stage: Poll
		 * MLAB(0)_STG_CONTROL[FETCH_TO_RUN] == 0.
		 */
		roc_ml_reg_wait_to_clear(roc_ml, ML_STG_CONTROL, ROC_ML_STG_CONTROL_FETCH_TO_RUN);

		/* (4c) Clear job fetch stage: If
		 * MLAB(0)_STG(0..2)_STATUS[VALID] == 1:
		 *     - Set MLAB(0)_STG_CONTROL[FETCH_TO_RUN].
		 *     - Poll MLAB(0)_STG_CONTROL[FETCH_TO_RUN] == 0.
		 *     - Repeat step (b) to clear job run and completion stages.
		 */
		reg_val = (roc_ml_reg_read64(roc_ml, ML_STGX_STATUS(0)) |
			   roc_ml_reg_read64(roc_ml, ML_STGX_STATUS(1)) |
			   roc_ml_reg_read64(roc_ml, ML_STGX_STATUS(2)));

		if (reg_val & ROC_ML_STG_STATUS_VALID) {
			reg_val = roc_ml_reg_read64(roc_ml, ML_STG_CONTROL);
			reg_val |= ROC_ML_STG_CONTROL_RUN_TO_COMP;
			roc_ml_reg_write64(roc_ml, reg_val, ML_STG_CONTROL);

			roc_ml_reg_wait_to_clear(roc_ml, ML_STG_CONTROL,
						 ROC_ML_STG_CONTROL_RUN_TO_COMP);

			goto cnf10kb_mlip_reset_stage_4b;
		}

		/* (5) Reset the ACC and DMA AXI interfaces: For each of the two
		 * MLAB(0)_AXI_BRIDGE_CTRL(0..1) registers:
		 *
		 * (5a) Set and then clear
		 * MLAB(0)_AXI_BRIDGE_CTRL(0..1)[FLUSH_WRITE_DATA].
		 *
		 * (5b) Clear MLAB(0)_AXI_BRIDGE_CTRL(0..1)[FENCE].
		 */
		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FLUSH_WRITE_DATA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val &= ~ROC_ML_AXI_BRIDGE_CTRL_FLUSH_WRITE_DATA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(0));
		reg_val &= ~ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(0));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(1));
		reg_val |= ROC_ML_AXI_BRIDGE_CTRL_FLUSH_WRITE_DATA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(1));
		reg_val &= ~ROC_ML_AXI_BRIDGE_CTRL_FLUSH_WRITE_DATA;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));

		reg_val = roc_ml_reg_read64(roc_ml, ML_AXI_BRIDGE_CTRL(1));
		reg_val &= ~ROC_ML_AXI_BRIDGE_CTRL_FENCE;
		roc_ml_reg_write64(roc_ml, reg_val, ML_AXI_BRIDGE_CTRL(1));
	}

	return 0;
}

int
roc_ml_dev_init(struct roc_ml *roc_ml)
{
	struct plt_pci_device *pci_dev;
	struct dev *dev;
	struct ml *ml;

	if (roc_ml == NULL || roc_ml->pci_dev == NULL)
		return -EINVAL;

	PLT_STATIC_ASSERT(sizeof(struct ml) <= ROC_ML_MEM_SZ);

	ml = roc_ml_to_ml_priv(roc_ml);
	memset(ml, 0, sizeof(*ml));
	pci_dev = roc_ml->pci_dev;
	dev = &ml->dev;

	ml->pci_dev = pci_dev;
	dev->roc_ml = roc_ml;

	ml->ml_reg_addr = ml->pci_dev->mem_resource[0].addr;
	ml->ml_mlr_base = 0;
	ml->ml_mlr_base_saved = false;

	plt_ml_dbg("ML: PCI Physical Address : 0x%016lx", ml->pci_dev->mem_resource[0].phys_addr);
	plt_ml_dbg("ML: PCI Virtual Address : 0x%016lx",
		   PLT_U64_CAST(ml->pci_dev->mem_resource[0].addr));

	plt_spinlock_init(&roc_ml->sp_spinlock);
	plt_spinlock_init(&roc_ml->fp_spinlock);

	return 0;
}

int
roc_ml_dev_fini(struct roc_ml *roc_ml)
{
	struct ml *ml = roc_ml_to_ml_priv(roc_ml);

	if (ml == NULL)
		return -EINVAL;

	return 0;
}

int
roc_ml_blk_init(struct roc_bphy *roc_bphy, struct roc_ml *roc_ml)
{
	struct dev *dev;
	struct ml *ml;

	if ((roc_ml == NULL) || (roc_bphy == NULL))
		return -EINVAL;

	PLT_STATIC_ASSERT(sizeof(struct ml) <= ROC_ML_MEM_SZ);

	ml = roc_ml_to_ml_priv(roc_ml);
	memset(ml, 0, sizeof(*ml));

	dev = &ml->dev;

	ml->pci_dev = roc_bphy->pci_dev;
	dev->roc_ml = roc_ml;

	plt_ml_dbg(
		"MLAB: Physical Address : 0x%016lx",
		PLT_PTR_ADD_U64_CAST(ml->pci_dev->mem_resource[0].phys_addr, ML_MLAB_BLK_OFFSET));
	plt_ml_dbg("MLAB: Virtual Address : 0x%016lx",
		   PLT_PTR_ADD_U64_CAST(ml->pci_dev->mem_resource[0].addr, ML_MLAB_BLK_OFFSET));

	ml->ml_reg_addr = PLT_PTR_ADD(ml->pci_dev->mem_resource[0].addr, ML_MLAB_BLK_OFFSET);
	ml->ml_mlr_base = 0;
	ml->ml_mlr_base_saved = false;

	plt_spinlock_init(&roc_ml->sp_spinlock);
	plt_spinlock_init(&roc_ml->fp_spinlock);

	return 0;
}

int
roc_ml_blk_fini(struct roc_bphy *roc_bphy, struct roc_ml *roc_ml)
{
	struct ml *ml;

	if ((roc_ml == NULL) || (roc_bphy == NULL))
		return -EINVAL;

	ml = roc_ml_to_ml_priv(roc_ml);

	if (ml == NULL)
		return -EINVAL;

	return 0;
}

uint16_t
roc_ml_sso_pf_func_get(void)
{
	return idev_sso_pffunc_get();
}
