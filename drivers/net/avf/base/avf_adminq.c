/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include "avf_status.h"
#include "avf_type.h"
#include "avf_register.h"
#include "avf_adminq.h"
#include "avf_prototype.h"

/**
 *  avf_adminq_init_regs - Initialize AdminQ registers
 *  @hw: pointer to the hardware structure
 *
 *  This assumes the alloc_asq and alloc_arq functions have already been called
 **/
STATIC void avf_adminq_init_regs(struct avf_hw *hw)
{
	/* set head and tail registers in our local struct */
	if (avf_is_vf(hw)) {
		hw->aq.asq.tail = AVF_ATQT1;
		hw->aq.asq.head = AVF_ATQH1;
		hw->aq.asq.len  = AVF_ATQLEN1;
		hw->aq.asq.bal  = AVF_ATQBAL1;
		hw->aq.asq.bah  = AVF_ATQBAH1;
		hw->aq.arq.tail = AVF_ARQT1;
		hw->aq.arq.head = AVF_ARQH1;
		hw->aq.arq.len  = AVF_ARQLEN1;
		hw->aq.arq.bal  = AVF_ARQBAL1;
		hw->aq.arq.bah  = AVF_ARQBAH1;
	}
}

/**
 *  avf_alloc_adminq_asq_ring - Allocate Admin Queue send rings
 *  @hw: pointer to the hardware structure
 **/
enum avf_status_code avf_alloc_adminq_asq_ring(struct avf_hw *hw)
{
	enum avf_status_code ret_code;

	ret_code = avf_allocate_dma_mem(hw, &hw->aq.asq.desc_buf,
					 avf_mem_atq_ring,
					 (hw->aq.num_asq_entries *
					 sizeof(struct avf_aq_desc)),
					 AVF_ADMINQ_DESC_ALIGNMENT);
	if (ret_code)
		return ret_code;

	ret_code = avf_allocate_virt_mem(hw, &hw->aq.asq.cmd_buf,
					  (hw->aq.num_asq_entries *
					  sizeof(struct avf_asq_cmd_details)));
	if (ret_code) {
		avf_free_dma_mem(hw, &hw->aq.asq.desc_buf);
		return ret_code;
	}

	return ret_code;
}

/**
 *  avf_alloc_adminq_arq_ring - Allocate Admin Queue receive rings
 *  @hw: pointer to the hardware structure
 **/
enum avf_status_code avf_alloc_adminq_arq_ring(struct avf_hw *hw)
{
	enum avf_status_code ret_code;

	ret_code = avf_allocate_dma_mem(hw, &hw->aq.arq.desc_buf,
					 avf_mem_arq_ring,
					 (hw->aq.num_arq_entries *
					 sizeof(struct avf_aq_desc)),
					 AVF_ADMINQ_DESC_ALIGNMENT);

	return ret_code;
}

/**
 *  avf_free_adminq_asq - Free Admin Queue send rings
 *  @hw: pointer to the hardware structure
 *
 *  This assumes the posted send buffers have already been cleaned
 *  and de-allocated
 **/
void avf_free_adminq_asq(struct avf_hw *hw)
{
	avf_free_dma_mem(hw, &hw->aq.asq.desc_buf);
}

/**
 *  avf_free_adminq_arq - Free Admin Queue receive rings
 *  @hw: pointer to the hardware structure
 *
 *  This assumes the posted receive buffers have already been cleaned
 *  and de-allocated
 **/
void avf_free_adminq_arq(struct avf_hw *hw)
{
	avf_free_dma_mem(hw, &hw->aq.arq.desc_buf);
}

/**
 *  avf_alloc_arq_bufs - Allocate pre-posted buffers for the receive queue
 *  @hw: pointer to the hardware structure
 **/
STATIC enum avf_status_code avf_alloc_arq_bufs(struct avf_hw *hw)
{
	enum avf_status_code ret_code;
	struct avf_aq_desc *desc;
	struct avf_dma_mem *bi;
	int i;

	/* We'll be allocating the buffer info memory first, then we can
	 * allocate the mapped buffers for the event processing
	 */

	/* buffer_info structures do not need alignment */
	ret_code = avf_allocate_virt_mem(hw, &hw->aq.arq.dma_head,
		(hw->aq.num_arq_entries * sizeof(struct avf_dma_mem)));
	if (ret_code)
		goto alloc_arq_bufs;
	hw->aq.arq.r.arq_bi = (struct avf_dma_mem *)hw->aq.arq.dma_head.va;

	/* allocate the mapped buffers */
	for (i = 0; i < hw->aq.num_arq_entries; i++) {
		bi = &hw->aq.arq.r.arq_bi[i];
		ret_code = avf_allocate_dma_mem(hw, bi,
						 avf_mem_arq_buf,
						 hw->aq.arq_buf_size,
						 AVF_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_arq_bufs;

		/* now configure the descriptors for use */
		desc = AVF_ADMINQ_DESC(hw->aq.arq, i);

		desc->flags = CPU_TO_LE16(AVF_AQ_FLAG_BUF);
		if (hw->aq.arq_buf_size > AVF_AQ_LARGE_BUF)
			desc->flags |= CPU_TO_LE16(AVF_AQ_FLAG_LB);
		desc->opcode = 0;
		/* This is in accordance with Admin queue design, there is no
		 * register for buffer size configuration
		 */
		desc->datalen = CPU_TO_LE16((u16)bi->size);
		desc->retval = 0;
		desc->cookie_high = 0;
		desc->cookie_low = 0;
		desc->params.external.addr_high =
			CPU_TO_LE32(AVF_HI_DWORD(bi->pa));
		desc->params.external.addr_low =
			CPU_TO_LE32(AVF_LO_DWORD(bi->pa));
		desc->params.external.param0 = 0;
		desc->params.external.param1 = 0;
	}

alloc_arq_bufs:
	return ret_code;

unwind_alloc_arq_bufs:
	/* don't try to free the one that failed... */
	i--;
	for (; i >= 0; i--)
		avf_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);
	avf_free_virt_mem(hw, &hw->aq.arq.dma_head);

	return ret_code;
}

/**
 *  avf_alloc_asq_bufs - Allocate empty buffer structs for the send queue
 *  @hw: pointer to the hardware structure
 **/
STATIC enum avf_status_code avf_alloc_asq_bufs(struct avf_hw *hw)
{
	enum avf_status_code ret_code;
	struct avf_dma_mem *bi;
	int i;

	/* No mapped memory needed yet, just the buffer info structures */
	ret_code = avf_allocate_virt_mem(hw, &hw->aq.asq.dma_head,
		(hw->aq.num_asq_entries * sizeof(struct avf_dma_mem)));
	if (ret_code)
		goto alloc_asq_bufs;
	hw->aq.asq.r.asq_bi = (struct avf_dma_mem *)hw->aq.asq.dma_head.va;

	/* allocate the mapped buffers */
	for (i = 0; i < hw->aq.num_asq_entries; i++) {
		bi = &hw->aq.asq.r.asq_bi[i];
		ret_code = avf_allocate_dma_mem(hw, bi,
						 avf_mem_asq_buf,
						 hw->aq.asq_buf_size,
						 AVF_ADMINQ_DESC_ALIGNMENT);
		if (ret_code)
			goto unwind_alloc_asq_bufs;
	}
alloc_asq_bufs:
	return ret_code;

unwind_alloc_asq_bufs:
	/* don't try to free the one that failed... */
	i--;
	for (; i >= 0; i--)
		avf_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);
	avf_free_virt_mem(hw, &hw->aq.asq.dma_head);

	return ret_code;
}

/**
 *  avf_free_arq_bufs - Free receive queue buffer info elements
 *  @hw: pointer to the hardware structure
 **/
STATIC void avf_free_arq_bufs(struct avf_hw *hw)
{
	int i;

	/* free descriptors */
	for (i = 0; i < hw->aq.num_arq_entries; i++)
		avf_free_dma_mem(hw, &hw->aq.arq.r.arq_bi[i]);

	/* free the descriptor memory */
	avf_free_dma_mem(hw, &hw->aq.arq.desc_buf);

	/* free the dma header */
	avf_free_virt_mem(hw, &hw->aq.arq.dma_head);
}

/**
 *  avf_free_asq_bufs - Free send queue buffer info elements
 *  @hw: pointer to the hardware structure
 **/
STATIC void avf_free_asq_bufs(struct avf_hw *hw)
{
	int i;

	/* only unmap if the address is non-NULL */
	for (i = 0; i < hw->aq.num_asq_entries; i++)
		if (hw->aq.asq.r.asq_bi[i].pa)
			avf_free_dma_mem(hw, &hw->aq.asq.r.asq_bi[i]);

	/* free the buffer info list */
	avf_free_virt_mem(hw, &hw->aq.asq.cmd_buf);

	/* free the descriptor memory */
	avf_free_dma_mem(hw, &hw->aq.asq.desc_buf);

	/* free the dma header */
	avf_free_virt_mem(hw, &hw->aq.asq.dma_head);
}

/**
 *  avf_config_asq_regs - configure ASQ registers
 *  @hw: pointer to the hardware structure
 *
 *  Configure base address and length registers for the transmit queue
 **/
STATIC enum avf_status_code avf_config_asq_regs(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;
	u32 reg = 0;

	/* Clear Head and Tail */
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);

	/* set starting point */
#ifdef INTEGRATED_VF
	if (avf_is_vf(hw))
		wr32(hw, hw->aq.asq.len, (hw->aq.num_asq_entries |
					  AVF_ATQLEN1_ATQENABLE_MASK));
#else
	wr32(hw, hw->aq.asq.len, (hw->aq.num_asq_entries |
				  AVF_ATQLEN1_ATQENABLE_MASK));
#endif /* INTEGRATED_VF */
	wr32(hw, hw->aq.asq.bal, AVF_LO_DWORD(hw->aq.asq.desc_buf.pa));
	wr32(hw, hw->aq.asq.bah, AVF_HI_DWORD(hw->aq.asq.desc_buf.pa));

	/* Check one register to verify that config was applied */
	reg = rd32(hw, hw->aq.asq.bal);
	if (reg != AVF_LO_DWORD(hw->aq.asq.desc_buf.pa))
		ret_code = AVF_ERR_ADMIN_QUEUE_ERROR;

	return ret_code;
}

/**
 *  avf_config_arq_regs - ARQ register configuration
 *  @hw: pointer to the hardware structure
 *
 * Configure base address and length registers for the receive (event queue)
 **/
STATIC enum avf_status_code avf_config_arq_regs(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;
	u32 reg = 0;

	/* Clear Head and Tail */
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);

	/* set starting point */
#ifdef INTEGRATED_VF
	if (avf_is_vf(hw))
		wr32(hw, hw->aq.arq.len, (hw->aq.num_arq_entries |
					  AVF_ARQLEN1_ARQENABLE_MASK));
#else
	wr32(hw, hw->aq.arq.len, (hw->aq.num_arq_entries |
				  AVF_ARQLEN1_ARQENABLE_MASK));
#endif /* INTEGRATED_VF */
	wr32(hw, hw->aq.arq.bal, AVF_LO_DWORD(hw->aq.arq.desc_buf.pa));
	wr32(hw, hw->aq.arq.bah, AVF_HI_DWORD(hw->aq.arq.desc_buf.pa));

	/* Update tail in the HW to post pre-allocated buffers */
	wr32(hw, hw->aq.arq.tail, hw->aq.num_arq_entries - 1);

	/* Check one register to verify that config was applied */
	reg = rd32(hw, hw->aq.arq.bal);
	if (reg != AVF_LO_DWORD(hw->aq.arq.desc_buf.pa))
		ret_code = AVF_ERR_ADMIN_QUEUE_ERROR;

	return ret_code;
}

/**
 *  avf_init_asq - main initialization routine for ASQ
 *  @hw: pointer to the hardware structure
 *
 *  This is the main initialization routine for the Admin Send Queue
 *  Prior to calling this function, drivers *MUST* set the following fields
 *  in the hw->aq structure:
 *     - hw->aq.num_asq_entries
 *     - hw->aq.arq_buf_size
 *
 *  Do *NOT* hold the lock when calling this as the memory allocation routines
 *  called are not going to be atomic context safe
 **/
enum avf_status_code avf_init_asq(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;

	if (hw->aq.asq.count > 0) {
		/* queue already initialized */
		ret_code = AVF_ERR_NOT_READY;
		goto init_adminq_exit;
	}

	/* verify input for valid configuration */
	if ((hw->aq.num_asq_entries == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = AVF_ERR_CONFIG;
		goto init_adminq_exit;
	}

	hw->aq.asq.next_to_use = 0;
	hw->aq.asq.next_to_clean = 0;

	/* allocate the ring memory */
	ret_code = avf_alloc_adminq_asq_ring(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_exit;

	/* allocate buffers in the rings */
	ret_code = avf_alloc_asq_bufs(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_free_rings;

	/* initialize base registers */
	ret_code = avf_config_asq_regs(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_free_rings;

	/* success! */
	hw->aq.asq.count = hw->aq.num_asq_entries;
	goto init_adminq_exit;

init_adminq_free_rings:
	avf_free_adminq_asq(hw);

init_adminq_exit:
	return ret_code;
}

/**
 *  avf_init_arq - initialize ARQ
 *  @hw: pointer to the hardware structure
 *
 *  The main initialization routine for the Admin Receive (Event) Queue.
 *  Prior to calling this function, drivers *MUST* set the following fields
 *  in the hw->aq structure:
 *     - hw->aq.num_asq_entries
 *     - hw->aq.arq_buf_size
 *
 *  Do *NOT* hold the lock when calling this as the memory allocation routines
 *  called are not going to be atomic context safe
 **/
enum avf_status_code avf_init_arq(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;

	if (hw->aq.arq.count > 0) {
		/* queue already initialized */
		ret_code = AVF_ERR_NOT_READY;
		goto init_adminq_exit;
	}

	/* verify input for valid configuration */
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0)) {
		ret_code = AVF_ERR_CONFIG;
		goto init_adminq_exit;
	}

	hw->aq.arq.next_to_use = 0;
	hw->aq.arq.next_to_clean = 0;

	/* allocate the ring memory */
	ret_code = avf_alloc_adminq_arq_ring(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_exit;

	/* allocate buffers in the rings */
	ret_code = avf_alloc_arq_bufs(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_free_rings;

	/* initialize base registers */
	ret_code = avf_config_arq_regs(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_free_rings;

	/* success! */
	hw->aq.arq.count = hw->aq.num_arq_entries;
	goto init_adminq_exit;

init_adminq_free_rings:
	avf_free_adminq_arq(hw);

init_adminq_exit:
	return ret_code;
}

/**
 *  avf_shutdown_asq - shutdown the ASQ
 *  @hw: pointer to the hardware structure
 *
 *  The main shutdown routine for the Admin Send Queue
 **/
enum avf_status_code avf_shutdown_asq(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;

	avf_acquire_spinlock(&hw->aq.asq_spinlock);

	if (hw->aq.asq.count == 0) {
		ret_code = AVF_ERR_NOT_READY;
		goto shutdown_asq_out;
	}

	/* Stop firmware AdminQ processing */
	wr32(hw, hw->aq.asq.head, 0);
	wr32(hw, hw->aq.asq.tail, 0);
	wr32(hw, hw->aq.asq.len, 0);
	wr32(hw, hw->aq.asq.bal, 0);
	wr32(hw, hw->aq.asq.bah, 0);

	hw->aq.asq.count = 0; /* to indicate uninitialized queue */

	/* free ring buffers */
	avf_free_asq_bufs(hw);

shutdown_asq_out:
	avf_release_spinlock(&hw->aq.asq_spinlock);
	return ret_code;
}

/**
 *  avf_shutdown_arq - shutdown ARQ
 *  @hw: pointer to the hardware structure
 *
 *  The main shutdown routine for the Admin Receive Queue
 **/
enum avf_status_code avf_shutdown_arq(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;

	avf_acquire_spinlock(&hw->aq.arq_spinlock);

	if (hw->aq.arq.count == 0) {
		ret_code = AVF_ERR_NOT_READY;
		goto shutdown_arq_out;
	}

	/* Stop firmware AdminQ processing */
	wr32(hw, hw->aq.arq.head, 0);
	wr32(hw, hw->aq.arq.tail, 0);
	wr32(hw, hw->aq.arq.len, 0);
	wr32(hw, hw->aq.arq.bal, 0);
	wr32(hw, hw->aq.arq.bah, 0);

	hw->aq.arq.count = 0; /* to indicate uninitialized queue */

	/* free ring buffers */
	avf_free_arq_bufs(hw);

shutdown_arq_out:
	avf_release_spinlock(&hw->aq.arq_spinlock);
	return ret_code;
}

/**
 *  avf_init_adminq - main initialization routine for Admin Queue
 *  @hw: pointer to the hardware structure
 *
 *  Prior to calling this function, drivers *MUST* set the following fields
 *  in the hw->aq structure:
 *     - hw->aq.num_asq_entries
 *     - hw->aq.num_arq_entries
 *     - hw->aq.arq_buf_size
 *     - hw->aq.asq_buf_size
 **/
enum avf_status_code avf_init_adminq(struct avf_hw *hw)
{
	enum avf_status_code ret_code;

	/* verify input for valid configuration */
	if ((hw->aq.num_arq_entries == 0) ||
	    (hw->aq.num_asq_entries == 0) ||
	    (hw->aq.arq_buf_size == 0) ||
	    (hw->aq.asq_buf_size == 0)) {
		ret_code = AVF_ERR_CONFIG;
		goto init_adminq_exit;
	}
	avf_init_spinlock(&hw->aq.asq_spinlock);
	avf_init_spinlock(&hw->aq.arq_spinlock);

	/* Set up register offsets */
	avf_adminq_init_regs(hw);

	/* setup ASQ command write back timeout */
	hw->aq.asq_cmd_timeout = AVF_ASQ_CMD_TIMEOUT;

	/* allocate the ASQ */
	ret_code = avf_init_asq(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_destroy_spinlocks;

	/* allocate the ARQ */
	ret_code = avf_init_arq(hw);
	if (ret_code != AVF_SUCCESS)
		goto init_adminq_free_asq;

	ret_code = AVF_SUCCESS;

	/* success! */
	goto init_adminq_exit;

init_adminq_free_asq:
	avf_shutdown_asq(hw);
init_adminq_destroy_spinlocks:
	avf_destroy_spinlock(&hw->aq.asq_spinlock);
	avf_destroy_spinlock(&hw->aq.arq_spinlock);

init_adminq_exit:
	return ret_code;
}

/**
 *  avf_shutdown_adminq - shutdown routine for the Admin Queue
 *  @hw: pointer to the hardware structure
 **/
enum avf_status_code avf_shutdown_adminq(struct avf_hw *hw)
{
	enum avf_status_code ret_code = AVF_SUCCESS;

	if (avf_check_asq_alive(hw))
		avf_aq_queue_shutdown(hw, true);

	avf_shutdown_asq(hw);
	avf_shutdown_arq(hw);
	avf_destroy_spinlock(&hw->aq.asq_spinlock);
	avf_destroy_spinlock(&hw->aq.arq_spinlock);

	if (hw->nvm_buff.va)
		avf_free_virt_mem(hw, &hw->nvm_buff);

	return ret_code;
}

/**
 *  avf_clean_asq - cleans Admin send queue
 *  @hw: pointer to the hardware structure
 *
 *  returns the number of free desc
 **/
u16 avf_clean_asq(struct avf_hw *hw)
{
	struct avf_adminq_ring *asq = &(hw->aq.asq);
	struct avf_asq_cmd_details *details;
	u16 ntc = asq->next_to_clean;
	struct avf_aq_desc desc_cb;
	struct avf_aq_desc *desc;

	desc = AVF_ADMINQ_DESC(*asq, ntc);
	details = AVF_ADMINQ_DETAILS(*asq, ntc);
	while (rd32(hw, hw->aq.asq.head) != ntc) {
		avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
			   "ntc %d head %d.\n", ntc, rd32(hw, hw->aq.asq.head));

		if (details->callback) {
			AVF_ADMINQ_CALLBACK cb_func =
					(AVF_ADMINQ_CALLBACK)details->callback;
			avf_memcpy(&desc_cb, desc, sizeof(struct avf_aq_desc),
				    AVF_DMA_TO_DMA);
			cb_func(hw, &desc_cb);
		}
		avf_memset(desc, 0, sizeof(*desc), AVF_DMA_MEM);
		avf_memset(details, 0, sizeof(*details), AVF_NONDMA_MEM);
		ntc++;
		if (ntc == asq->count)
			ntc = 0;
		desc = AVF_ADMINQ_DESC(*asq, ntc);
		details = AVF_ADMINQ_DETAILS(*asq, ntc);
	}

	asq->next_to_clean = ntc;

	return AVF_DESC_UNUSED(asq);
}

/**
 *  avf_asq_done - check if FW has processed the Admin Send Queue
 *  @hw: pointer to the hw struct
 *
 *  Returns true if the firmware has processed all descriptors on the
 *  admin send queue. Returns false if there are still requests pending.
 **/
bool avf_asq_done(struct avf_hw *hw)
{
	/* AQ designers suggest use of head for better
	 * timing reliability than DD bit
	 */
	return rd32(hw, hw->aq.asq.head) == hw->aq.asq.next_to_use;

}

/**
 *  avf_asq_send_command - send command to Admin Queue
 *  @hw: pointer to the hw struct
 *  @desc: prefilled descriptor describing the command (non DMA mem)
 *  @buff: buffer to use for indirect commands
 *  @buff_size: size of buffer for indirect commands
 *  @cmd_details: pointer to command details structure
 *
 *  This is the main send command driver routine for the Admin Queue send
 *  queue.  It runs the queue, cleans the queue, etc
 **/
enum avf_status_code avf_asq_send_command(struct avf_hw *hw,
				struct avf_aq_desc *desc,
				void *buff, /* can be NULL */
				u16  buff_size,
				struct avf_asq_cmd_details *cmd_details)
{
	enum avf_status_code status = AVF_SUCCESS;
	struct avf_dma_mem *dma_buff = NULL;
	struct avf_asq_cmd_details *details;
	struct avf_aq_desc *desc_on_ring;
	bool cmd_completed = false;
	u16  retval = 0;
	u32  val = 0;

	avf_acquire_spinlock(&hw->aq.asq_spinlock);

	hw->aq.asq_last_status = AVF_AQ_RC_OK;

	if (hw->aq.asq.count == 0) {
		avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Admin queue not initialized.\n");
		status = AVF_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	val = rd32(hw, hw->aq.asq.head);
	if (val >= hw->aq.num_asq_entries) {
		avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
			   "AQTX: head overrun at %d\n", val);
		status = AVF_ERR_QUEUE_EMPTY;
		goto asq_send_command_error;
	}

	details = AVF_ADMINQ_DETAILS(hw->aq.asq, hw->aq.asq.next_to_use);
	if (cmd_details) {
		avf_memcpy(details,
			    cmd_details,
			    sizeof(struct avf_asq_cmd_details),
			    AVF_NONDMA_TO_NONDMA);

		/* If the cmd_details are defined copy the cookie.  The
		 * CPU_TO_LE32 is not needed here because the data is ignored
		 * by the FW, only used by the driver
		 */
		if (details->cookie) {
			desc->cookie_high =
				CPU_TO_LE32(AVF_HI_DWORD(details->cookie));
			desc->cookie_low =
				CPU_TO_LE32(AVF_LO_DWORD(details->cookie));
		}
	} else {
		avf_memset(details, 0,
			    sizeof(struct avf_asq_cmd_details),
			    AVF_NONDMA_MEM);
	}

	/* clear requested flags and then set additional flags if defined */
	desc->flags &= ~CPU_TO_LE16(details->flags_dis);
	desc->flags |= CPU_TO_LE16(details->flags_ena);

	if (buff_size > hw->aq.asq_buf_size) {
		avf_debug(hw,
			   AVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Invalid buffer size: %d.\n",
			   buff_size);
		status = AVF_ERR_INVALID_SIZE;
		goto asq_send_command_error;
	}

	if (details->postpone && !details->async) {
		avf_debug(hw,
			   AVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Async flag not set along with postpone flag");
		status = AVF_ERR_PARAM;
		goto asq_send_command_error;
	}

	/* call clean and check queue available function to reclaim the
	 * descriptors that were processed by FW, the function returns the
	 * number of desc available
	 */
	/* the clean function called here could be called in a separate thread
	 * in case of asynchronous completions
	 */
	if (avf_clean_asq(hw) == 0) {
		avf_debug(hw,
			   AVF_DEBUG_AQ_MESSAGE,
			   "AQTX: Error queue is full.\n");
		status = AVF_ERR_ADMIN_QUEUE_FULL;
		goto asq_send_command_error;
	}

	/* initialize the temp desc pointer with the right desc */
	desc_on_ring = AVF_ADMINQ_DESC(hw->aq.asq, hw->aq.asq.next_to_use);

	/* if the desc is available copy the temp desc to the right place */
	avf_memcpy(desc_on_ring, desc, sizeof(struct avf_aq_desc),
		    AVF_NONDMA_TO_DMA);

	/* if buff is not NULL assume indirect command */
	if (buff != NULL) {
		dma_buff = &(hw->aq.asq.r.asq_bi[hw->aq.asq.next_to_use]);
		/* copy the user buff into the respective DMA buff */
		avf_memcpy(dma_buff->va, buff, buff_size,
			    AVF_NONDMA_TO_DMA);
		desc_on_ring->datalen = CPU_TO_LE16(buff_size);

		/* Update the address values in the desc with the pa value
		 * for respective buffer
		 */
		desc_on_ring->params.external.addr_high =
				CPU_TO_LE32(AVF_HI_DWORD(dma_buff->pa));
		desc_on_ring->params.external.addr_low =
				CPU_TO_LE32(AVF_LO_DWORD(dma_buff->pa));
	}

	/* bump the tail */
	avf_debug(hw, AVF_DEBUG_AQ_MESSAGE, "AQTX: desc and buffer:\n");
	avf_debug_aq(hw, AVF_DEBUG_AQ_COMMAND, (void *)desc_on_ring,
		      buff, buff_size);
	(hw->aq.asq.next_to_use)++;
	if (hw->aq.asq.next_to_use == hw->aq.asq.count)
		hw->aq.asq.next_to_use = 0;
	if (!details->postpone)
		wr32(hw, hw->aq.asq.tail, hw->aq.asq.next_to_use);

	/* if cmd_details are not defined or async flag is not set,
	 * we need to wait for desc write back
	 */
	if (!details->async && !details->postpone) {
		u32 total_delay = 0;

		do {
			/* AQ designers suggest use of head for better
			 * timing reliability than DD bit
			 */
			if (avf_asq_done(hw))
				break;
			avf_usec_delay(50);
			total_delay += 50;
		} while (total_delay < hw->aq.asq_cmd_timeout);
	}

	/* if ready, copy the desc back to temp */
	if (avf_asq_done(hw)) {
		avf_memcpy(desc, desc_on_ring, sizeof(struct avf_aq_desc),
			    AVF_DMA_TO_NONDMA);
		if (buff != NULL)
			avf_memcpy(buff, dma_buff->va, buff_size,
				    AVF_DMA_TO_NONDMA);
		retval = LE16_TO_CPU(desc->retval);
		if (retval != 0) {
			avf_debug(hw,
				   AVF_DEBUG_AQ_MESSAGE,
				   "AQTX: Command completed with error 0x%X.\n",
				   retval);

			/* strip off FW internal code */
			retval &= 0xff;
		}
		cmd_completed = true;
		if ((enum avf_admin_queue_err)retval == AVF_AQ_RC_OK)
			status = AVF_SUCCESS;
		else
			status = AVF_ERR_ADMIN_QUEUE_ERROR;
		hw->aq.asq_last_status = (enum avf_admin_queue_err)retval;
	}

	avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
		   "AQTX: desc and buffer writeback:\n");
	avf_debug_aq(hw, AVF_DEBUG_AQ_COMMAND, (void *)desc, buff, buff_size);

	/* save writeback aq if requested */
	if (details->wb_desc)
		avf_memcpy(details->wb_desc, desc_on_ring,
			    sizeof(struct avf_aq_desc), AVF_DMA_TO_NONDMA);

	/* update the error if time out occurred */
	if ((!cmd_completed) &&
	    (!details->async && !details->postpone)) {
		if (rd32(hw, hw->aq.asq.len) & AVF_ATQLEN1_ATQCRIT_MASK) {
			avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
				   "AQTX: AQ Critical error.\n");
			status = AVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR;
		} else {
			avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
				   "AQTX: Writeback timeout.\n");
			status = AVF_ERR_ADMIN_QUEUE_TIMEOUT;
		}
	}

asq_send_command_error:
	avf_release_spinlock(&hw->aq.asq_spinlock);
	return status;
}

/**
 *  avf_fill_default_direct_cmd_desc - AQ descriptor helper function
 *  @desc:     pointer to the temp descriptor (non DMA mem)
 *  @opcode:   the opcode can be used to decide which flags to turn off or on
 *
 *  Fill the desc with default values
 **/
void avf_fill_default_direct_cmd_desc(struct avf_aq_desc *desc,
				       u16 opcode)
{
	/* zero out the desc */
	avf_memset((void *)desc, 0, sizeof(struct avf_aq_desc),
		    AVF_NONDMA_MEM);
	desc->opcode = CPU_TO_LE16(opcode);
	desc->flags = CPU_TO_LE16(AVF_AQ_FLAG_SI);
}

/**
 *  avf_clean_arq_element
 *  @hw: pointer to the hw struct
 *  @e: event info from the receive descriptor, includes any buffers
 *  @pending: number of events that could be left to process
 *
 *  This function cleans one Admin Receive Queue element and returns
 *  the contents through e.  It can also return how many events are
 *  left to process through 'pending'
 **/
enum avf_status_code avf_clean_arq_element(struct avf_hw *hw,
					     struct avf_arq_event_info *e,
					     u16 *pending)
{
	enum avf_status_code ret_code = AVF_SUCCESS;
	u16 ntc = hw->aq.arq.next_to_clean;
	struct avf_aq_desc *desc;
	struct avf_dma_mem *bi;
	u16 desc_idx;
	u16 datalen;
	u16 flags;
	u16 ntu;

	/* pre-clean the event info */
	avf_memset(&e->desc, 0, sizeof(e->desc), AVF_NONDMA_MEM);

	/* take the lock before we start messing with the ring */
	avf_acquire_spinlock(&hw->aq.arq_spinlock);

	if (hw->aq.arq.count == 0) {
		avf_debug(hw, AVF_DEBUG_AQ_MESSAGE,
			   "AQRX: Admin queue not initialized.\n");
		ret_code = AVF_ERR_QUEUE_EMPTY;
		goto clean_arq_element_err;
	}

	/* set next_to_use to head */
#ifdef INTEGRATED_VF
	if (!avf_is_vf(hw))
		ntu = rd32(hw, hw->aq.arq.head) & AVF_PF_ARQH_ARQH_MASK;
	else
		ntu = rd32(hw, hw->aq.arq.head) & AVF_ARQH1_ARQH_MASK;
#else
	ntu = rd32(hw, hw->aq.arq.head) & AVF_ARQH1_ARQH_MASK;
#endif /* INTEGRATED_VF */
	if (ntu == ntc) {
		/* nothing to do - shouldn't need to update ring's values */
		ret_code = AVF_ERR_ADMIN_QUEUE_NO_WORK;
		goto clean_arq_element_out;
	}

	/* now clean the next descriptor */
	desc = AVF_ADMINQ_DESC(hw->aq.arq, ntc);
	desc_idx = ntc;

	hw->aq.arq_last_status =
		(enum avf_admin_queue_err)LE16_TO_CPU(desc->retval);
	flags = LE16_TO_CPU(desc->flags);
	if (flags & AVF_AQ_FLAG_ERR) {
		ret_code = AVF_ERR_ADMIN_QUEUE_ERROR;
		avf_debug(hw,
			   AVF_DEBUG_AQ_MESSAGE,
			   "AQRX: Event received with error 0x%X.\n",
			   hw->aq.arq_last_status);
	}

	avf_memcpy(&e->desc, desc, sizeof(struct avf_aq_desc),
		    AVF_DMA_TO_NONDMA);
	datalen = LE16_TO_CPU(desc->datalen);
	e->msg_len = min(datalen, e->buf_len);
	if (e->msg_buf != NULL && (e->msg_len != 0))
		avf_memcpy(e->msg_buf,
			    hw->aq.arq.r.arq_bi[desc_idx].va,
			    e->msg_len, AVF_DMA_TO_NONDMA);

	avf_debug(hw, AVF_DEBUG_AQ_MESSAGE, "AQRX: desc and buffer:\n");
	avf_debug_aq(hw, AVF_DEBUG_AQ_COMMAND, (void *)desc, e->msg_buf,
		      hw->aq.arq_buf_size);

	/* Restore the original datalen and buffer address in the desc,
	 * FW updates datalen to indicate the event message
	 * size
	 */
	bi = &hw->aq.arq.r.arq_bi[ntc];
	avf_memset((void *)desc, 0, sizeof(struct avf_aq_desc), AVF_DMA_MEM);

	desc->flags = CPU_TO_LE16(AVF_AQ_FLAG_BUF);
	if (hw->aq.arq_buf_size > AVF_AQ_LARGE_BUF)
		desc->flags |= CPU_TO_LE16(AVF_AQ_FLAG_LB);
	desc->datalen = CPU_TO_LE16((u16)bi->size);
	desc->params.external.addr_high = CPU_TO_LE32(AVF_HI_DWORD(bi->pa));
	desc->params.external.addr_low = CPU_TO_LE32(AVF_LO_DWORD(bi->pa));

	/* set tail = the last cleaned desc index. */
	wr32(hw, hw->aq.arq.tail, ntc);
	/* ntc is updated to tail + 1 */
	ntc++;
	if (ntc == hw->aq.num_arq_entries)
		ntc = 0;
	hw->aq.arq.next_to_clean = ntc;
	hw->aq.arq.next_to_use = ntu;

clean_arq_element_out:
	/* Set pending if needed, unlock and return */
	if (pending != NULL)
		*pending = (ntc > ntu ? hw->aq.arq.count : 0) + (ntu - ntc);
clean_arq_element_err:
	avf_release_spinlock(&hw->aq.arq_spinlock);

	return ret_code;
}

