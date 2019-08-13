/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_crypto.h>
#include <rte_security.h>

#include <caam_jr_config.h>
#include <caam_jr_hw_specific.h>
#include <caam_jr_pvt.h>
#include <caam_jr_log.h>

/* Used to retry resetting a job ring in SEC hardware. */
#define SEC_TIMEOUT 100000

/* @brief Process Jump Halt Condition related errors
 *
 * @param [in]  error_code        The error code in the descriptor status word
 */
static inline void
hw_handle_jmp_halt_cond_err(union hw_error_code error_code)
{
	CAAM_JR_DEBUG("JMP: %d, Descriptor Index: 0x%x, Condition: 0x%x",
			error_code.error_desc.jmp_halt_cond_src.jmp,
			error_code.error_desc.jmp_halt_cond_src.desc_idx,
			error_code.error_desc.jmp_halt_cond_src.cond);
	(void)error_code;
}

/* @brief Process DECO related errors
 *
 * @param [in]  error_code        The error code in the descriptor status word
 */
static inline void
hw_handle_deco_err(union hw_error_code error_code)
{
	CAAM_JR_DEBUG("JMP: %d, Descriptor Index: 0x%x",
			error_code.error_desc.deco_src.jmp,
			error_code.error_desc.deco_src.desc_idx);

	switch (error_code.error_desc.deco_src.desc_err) {
	case SEC_HW_ERR_DECO_HFN_THRESHOLD:
		CAAM_JR_DEBUG(" Warning: Descriptor completed normally,"
			"but 3GPP HFN matches or exceeds the Threshold ");
		break;
	default:
		CAAM_JR_DEBUG("Error 0x%04x not implemented",
				error_code.error_desc.deco_src.desc_err);
		break;
	}
}

/* @brief Process  Jump Halt User Status related errors
 *
 * @param [in]  error_code        The error code in the descriptor status word
 */
static inline void
hw_handle_jmp_halt_user_err(union hw_error_code error_code __rte_unused)
{
	CAAM_JR_DEBUG(" Not implemented");
}

/* @brief Process CCB related errors
 *
 * @param [in]  error_code        The error code in the descriptor status word
 */
static inline void
hw_handle_ccb_err(union hw_error_code hw_error_code __rte_unused)
{
	CAAM_JR_DEBUG(" Not implemented");
}

/* @brief Process Job Ring related errors
 *
 * @param [in]  error_code        The error code in the descriptor status word
 */
static inline void
hw_handle_jr_err(union hw_error_code hw_error_code __rte_unused)
{
	CAAM_JR_DEBUG(" Not implemented");
}

int
hw_reset_job_ring(struct sec_job_ring_t *job_ring)
{
	int ret = 0;

	ASSERT(job_ring->register_base_addr != NULL);

	/* First reset the job ring in hw */
	ret = hw_shutdown_job_ring(job_ring);
	SEC_ASSERT(ret == 0, ret, "Failed resetting job ring in hardware");

	/* In order to have the HW JR in a workable state
	 * after a reset, I need to re-write the input
	 * queue size, input start address, output queue
	 * size and output start address
	 */
	/* Write the JR input queue size to the HW register */
	hw_set_input_ring_size(job_ring, SEC_JOB_RING_SIZE);

	/* Write the JR output queue size to the HW register */
	hw_set_output_ring_size(job_ring, SEC_JOB_RING_SIZE);

	/* Write the JR input queue start address */
	hw_set_input_ring_start_addr(job_ring,
			caam_jr_dma_vtop(job_ring->input_ring));
	CAAM_JR_DEBUG(" Set input ring base address to : Virtual: 0x%" PRIx64
		      ",Physical: 0x%" PRIx64 ", Read from HW: 0x%" PRIx64,
		      (uint64_t)(uintptr_t)job_ring->input_ring,
		      caam_jr_dma_vtop(job_ring->input_ring),
		      hw_get_inp_queue_base(job_ring));

	/* Write the JR output queue start address */
	hw_set_output_ring_start_addr(job_ring,
			caam_jr_dma_vtop(job_ring->output_ring));
	CAAM_JR_DEBUG(" Set output ring base address to: Virtual: 0x%" PRIx64
		      ",Physical: 0x%" PRIx64 ", Read from HW: 0x%" PRIx64,
		      (uint64_t)(uintptr_t)job_ring->output_ring,
		      caam_jr_dma_vtop(job_ring->output_ring),
		      hw_get_out_queue_base(job_ring));
	return ret;
}

int
hw_shutdown_job_ring(struct sec_job_ring_t *job_ring)
{
	unsigned int timeout = SEC_TIMEOUT;
	uint32_t tmp = 0;
	int usleep_interval = 10;

	if (job_ring->register_base_addr == NULL) {
		CAAM_JR_ERR("Jr[%p] has reg base addr as NULL.driver not init",
			job_ring);
		return 0;
	}

	CAAM_JR_INFO("Resetting Job ring %p", job_ring);

	/*
	 * Mask interrupts since we are going to poll
	 * for reset completion status
	 * Also, at POR, interrupts are ENABLED on a JR, thus
	 * this is the point where I can disable them without
	 * changing the code logic too much
	 */
	caam_jr_disable_irqs(job_ring->irq_fd);

	/* initiate flush (required prior to reset) */
	SET_JR_REG(JRCR, job_ring, JR_REG_JRCR_VAL_RESET);

	/* dummy read */
	tmp = GET_JR_REG(JRCR, job_ring);

	do {
		tmp = GET_JR_REG(JRINT, job_ring);
		usleep(usleep_interval);
	} while (((tmp & JRINT_ERR_HALT_MASK) ==
			JRINT_ERR_HALT_INPROGRESS) && --timeout);

	CAAM_JR_INFO("JRINT is %x", tmp);
	if ((tmp & JRINT_ERR_HALT_MASK) != JRINT_ERR_HALT_COMPLETE ||
		timeout == 0) {
		CAAM_JR_ERR("0x%x, %d", tmp, timeout);
		/* unmask interrupts */
		if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
			caam_jr_enable_irqs(job_ring->irq_fd);
		return -1;
	}

	/* Initiate reset */
	timeout = SEC_TIMEOUT;
	SET_JR_REG(JRCR, job_ring, JR_REG_JRCR_VAL_RESET);

	do {
		tmp = GET_JR_REG(JRCR, job_ring);
		usleep(usleep_interval);
	} while ((tmp & JR_REG_JRCR_VAL_RESET) && --timeout);

	CAAM_JR_DEBUG("JRCR is %x", tmp);
	if (timeout == 0) {
		CAAM_JR_ERR("Failed to reset hw job ring %p", job_ring);
		/* unmask interrupts */
		if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
			caam_jr_enable_irqs(job_ring->irq_fd);
		return -1;
	}
	/* unmask interrupts */
	if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
		caam_jr_enable_irqs(job_ring->irq_fd);
	return 0;

}

void
hw_handle_job_ring_error(struct sec_job_ring_t *job_ring __rte_unused,
			 uint32_t error_code)
{
	union hw_error_code hw_err_code;

	hw_err_code.error = error_code;
	switch (hw_err_code.error_desc.value.ssrc) {
	case SEC_HW_ERR_SSRC_NO_SRC:
		ASSERT(hw_err_code.error_desc.no_status_src.res == 0);
		CAAM_JR_ERR("No Status Source ");
		break;
	case SEC_HW_ERR_SSRC_CCB_ERR:
		CAAM_JR_ERR("CCB Status Source");
		hw_handle_ccb_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JMP_HALT_U:
		CAAM_JR_ERR("Jump Halt User Status Source");
		hw_handle_jmp_halt_user_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_DECO:
		CAAM_JR_ERR("DECO Status Source");
		hw_handle_deco_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JR:
		CAAM_JR_ERR("Job Ring Status Source");
		hw_handle_jr_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JMP_HALT_COND:
		CAAM_JR_ERR("Jump Halt Condition Codes");
		hw_handle_jmp_halt_cond_err(hw_err_code);
		break;
	default:
		ASSERT(0);
		CAAM_JR_ERR("Unknown SSRC");
		break;
	}
}

void
hw_job_ring_error_print(struct sec_job_ring_t *job_ring, int code)
{
	switch (code) {
	case JRINT_ERR_WRITE_STATUS:
		CAAM_JR_ERR("Error writing status to Output Ring ");
		break;
	case JRINT_ERR_BAD_INPUT_BASE:
		CAAM_JR_ERR(
		"Bad Input Ring Base (%p) (not on a 4-byte boundary) ",
		(void *)job_ring);
		break;
	case JRINT_ERR_BAD_OUTPUT_BASE:
		CAAM_JR_ERR(
		"Bad Output Ring Base (%p) (not on a 4-byte boundary) ",
		(void *)job_ring);
		break;
	case JRINT_ERR_WRITE_2_IRBA:
		CAAM_JR_ERR(
		"Invalid write to Input Ring Base Address Register ");
		break;
	case JRINT_ERR_WRITE_2_ORBA:
		CAAM_JR_ERR(
		"Invalid write to Output Ring Base Address Register ");
		break;
	case JRINT_ERR_RES_B4_HALT:
		CAAM_JR_ERR(
		"Job Ring [%p] released before Job Ring is halted",
		(void *)job_ring);
		break;
	case JRINT_ERR_REM_TOO_MANY:
		CAAM_JR_ERR("Removed too many jobs from job ring [%p]",
			(void *)job_ring);
		break;
	case JRINT_ERR_ADD_TOO_MANY:
		CAAM_JR_ERR("Added too many jobs on job ring [%p]", job_ring);
		break;
	default:
		CAAM_JR_ERR(" Unknown SEC JR Error :%d",
				code);
		break;
	}
}

int
hw_job_ring_set_coalescing_param(struct sec_job_ring_t *job_ring,
				 uint16_t irq_coalescing_timer,
				 uint8_t irq_coalescing_count)
{
	uint32_t reg_val = 0;

	ASSERT(job_ring != NULL);
	if (job_ring->register_base_addr == NULL) {
		CAAM_JR_ERR("Jr[%p] has reg base addr as NULL.driver not init",
			job_ring);
		return -1;
	}
	/* Set descriptor count coalescing */
	reg_val |= (irq_coalescing_count << JR_REG_JRCFG_LO_ICDCT_SHIFT);

	/* Set coalescing timer value */
	reg_val |= (irq_coalescing_timer << JR_REG_JRCFG_LO_ICTT_SHIFT);

	/* Update parameters in HW */
	SET_JR_REG_LO(JRCFG, job_ring, reg_val);
	CAAM_JR_DEBUG("Set coalescing params on jr %p timer:%d, desc count: %d",
			job_ring, irq_coalescing_timer, irq_coalescing_timer);

	return 0;
}

int
hw_job_ring_enable_coalescing(struct sec_job_ring_t *job_ring)
{
	uint32_t reg_val = 0;

	ASSERT(job_ring != NULL);
	if (job_ring->register_base_addr == NULL) {
		CAAM_JR_ERR("Jr[%p] has reg base addr as NULL.driver not init",
			job_ring);
		return -1;
	}

	/* Get the current value of the register */
	reg_val = GET_JR_REG_LO(JRCFG, job_ring);

	/* Enable coalescing */
	reg_val |= JR_REG_JRCFG_LO_ICEN_EN;

	/* Write in hw */
	SET_JR_REG_LO(JRCFG, job_ring, reg_val);

	CAAM_JR_DEBUG("Enabled coalescing on jr %p ",
			job_ring);

	return 0;
}

int
hw_job_ring_disable_coalescing(struct sec_job_ring_t *job_ring)
{
	uint32_t reg_val = 0;

	ASSERT(job_ring != NULL);

	if (job_ring->register_base_addr == NULL) {
		CAAM_JR_ERR("Jr[%p] has reg base addr as NULL.driver not init",
			job_ring);
		return -1;
	}

	/* Get the current value of the register */
	reg_val = GET_JR_REG_LO(JRCFG, job_ring);

	/* Disable coalescing */
	reg_val &= ~JR_REG_JRCFG_LO_ICEN_EN;

	/* Write in hw */
	SET_JR_REG_LO(JRCFG, job_ring, reg_val);
	CAAM_JR_DEBUG("Disabled coalescing on jr %p ", job_ring);

	return 0;
}
