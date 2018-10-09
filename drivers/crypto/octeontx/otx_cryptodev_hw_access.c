/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */
#include <string.h>
#include <unistd.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>

#include "otx_cryptodev_hw_access.h"
#include "otx_cryptodev_mbox.h"

#include "cpt_pmd_logs.h"
#include "cpt_hw_types.h"

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
	strncpy(cptvf->dev_name, name, 32);

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
