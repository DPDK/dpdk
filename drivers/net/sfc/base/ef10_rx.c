/*
 * Copyright (c) 2012-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD


static	__checkReturn	efx_rc_t
efx_mcdi_init_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t size,
	__in		uint32_t target_evq,
	__in		uint32_t label,
	__in		uint32_t instance,
	__in		efsys_mem_t *esmp,
	__in		boolean_t disable_scatter,
	__in		uint32_t ps_bufsize)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_INIT_RXQ_EXT_IN_LEN,
			    MC_CMD_INIT_RXQ_EXT_OUT_LEN)];
	int npages = EFX_RXQ_NBUFS(size);
	int i;
	efx_qword_t *dma_addr;
	uint64_t addr;
	efx_rc_t rc;
	uint32_t dma_mode;

	/* If this changes, then the payload size might need to change. */
	EFSYS_ASSERT3U(MC_CMD_INIT_RXQ_OUT_LEN, ==, 0);
	EFSYS_ASSERT3U(size, <=, EFX_RXQ_MAXNDESCS);

	if (ps_bufsize > 0)
		dma_mode = MC_CMD_INIT_RXQ_EXT_IN_PACKED_STREAM;
	else
		dma_mode = MC_CMD_INIT_RXQ_EXT_IN_SINGLE_PACKET;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_INIT_RXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_INIT_RXQ_EXT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_INIT_RXQ_EXT_OUT_LEN;

	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_SIZE, size);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_TARGET_EVQ, target_evq);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_LABEL, label);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_INSTANCE, instance);
	MCDI_IN_POPULATE_DWORD_8(req, INIT_RXQ_EXT_IN_FLAGS,
	    INIT_RXQ_EXT_IN_FLAG_BUFF_MODE, 0,
	    INIT_RXQ_EXT_IN_FLAG_HDR_SPLIT, 0,
	    INIT_RXQ_EXT_IN_FLAG_TIMESTAMP, 0,
	    INIT_RXQ_EXT_IN_CRC_MODE, 0,
	    INIT_RXQ_EXT_IN_FLAG_PREFIX, 1,
	    INIT_RXQ_EXT_IN_FLAG_DISABLE_SCATTER, disable_scatter,
	    INIT_RXQ_EXT_IN_DMA_MODE,
	    dma_mode,
	    INIT_RXQ_EXT_IN_PACKED_STREAM_BUFF_SIZE, ps_bufsize);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_OWNER_ID, 0);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_PORT_ID, EVB_PORT_ID_ASSIGNED);

	dma_addr = MCDI_IN2(req, efx_qword_t, INIT_RXQ_IN_DMA_ADDR);
	addr = EFSYS_MEM_ADDR(esmp);

	for (i = 0; i < npages; i++) {
		EFX_POPULATE_QWORD_2(*dma_addr,
		    EFX_DWORD_1, (uint32_t)(addr >> 32),
		    EFX_DWORD_0, (uint32_t)(addr & 0xffffffff));

		dma_addr++;
		addr += EFX_BUF_SIZE;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_fini_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FINI_RXQ_IN_LEN,
			    MC_CMD_FINI_RXQ_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FINI_RXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FINI_RXQ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FINI_RXQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FINI_RXQ_IN_INSTANCE, instance);

	efx_mcdi_execute_quiet(enp, &req);

	if ((req.emr_rc != 0) && (req.emr_rc != MC_CMD_ERR_EALREADY)) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
ef10_rx_init(
	__in		efx_nic_t *enp)
{

	return (0);
}


/*
 * EF10 RX pseudo-header
 * ---------------------
 *
 * Receive packets are prefixed by an (optional) 14 byte pseudo-header:
 *
 *  +00: Toeplitz hash value.
 *       (32bit little-endian)
 *  +04: Outer VLAN tag. Zero if the packet did not have an outer VLAN tag.
 *       (16bit big-endian)
 *  +06: Inner VLAN tag. Zero if the packet did not have an inner VLAN tag.
 *       (16bit big-endian)
 *  +08: Packet Length. Zero if the RX datapath was in cut-through mode.
 *       (16bit little-endian)
 *  +10: MAC timestamp. Zero if timestamping is not enabled.
 *       (32bit little-endian)
 *
 * See "The RX Pseudo-header" in SF-109306-TC.
 */

	__checkReturn	efx_rc_t
ef10_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	_NOTE(ARGUNUSED(enp))

	/*
	 * The RX pseudo-header contains the packet length, excluding the
	 * pseudo-header. If the hardware receive datapath was operating in
	 * cut-through mode then the length in the RX pseudo-header will be
	 * zero, and the packet length must be obtained from the DMA length
	 * reported in the RX event.
	 */
	*lengthp = buffer[8] | (buffer[9] << 8);
	return (0);
}

			void
ef10_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added)
{
	efx_qword_t qword;
	unsigned int i;
	unsigned int offset;
	unsigned int id;

	/* The client driver must not overfill the queue */
	EFSYS_ASSERT3U(added - completed + n, <=,
	    EFX_RXQ_LIMIT(erp->er_mask + 1));

	id = added & (erp->er_mask);
	for (i = 0; i < n; i++) {
		EFSYS_PROBE4(rx_post, unsigned int, erp->er_index,
		    unsigned int, id, efsys_dma_addr_t, addrp[i],
		    size_t, size);

		EFX_POPULATE_QWORD_3(qword,
		    ESF_DZ_RX_KER_BYTE_CNT, (uint32_t)(size),
		    ESF_DZ_RX_KER_BUF_ADDR_DW0,
		    (uint32_t)(addrp[i] & 0xffffffff),
		    ESF_DZ_RX_KER_BUF_ADDR_DW1,
		    (uint32_t)(addrp[i] >> 32));

		offset = id * sizeof (efx_qword_t);
		EFSYS_MEM_WRITEQ(erp->er_esmp, offset, &qword);

		id = (id + 1) & (erp->er_mask);
	}
}

			void
ef10_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added,
	__inout	unsigned int *pushedp)
{
	efx_nic_t *enp = erp->er_enp;
	unsigned int pushed = *pushedp;
	uint32_t wptr;
	efx_dword_t dword;

	/* Hardware has alignment restriction for WPTR */
	wptr = P2ALIGN(added, EF10_RX_WPTR_ALIGN);
	if (pushed == wptr)
		return;

	*pushedp = wptr;

	/* Push the populated descriptors out */
	wptr &= erp->er_mask;

	EFX_POPULATE_DWORD_1(dword, ERF_DZ_RX_DESC_WPTR, wptr);

	/* Guarantee ordering of memory (descriptors) and PIO (doorbell) */
	EFX_DMA_SYNC_QUEUE_FOR_DEVICE(erp->er_esmp, erp->er_mask + 1,
	    wptr, pushed & erp->er_mask);
	EFSYS_PIO_WRITE_BARRIER();
	EFX_BAR_TBL_WRITED(enp, ER_DZ_RX_DESC_UPD_REG,
			    erp->er_index, &dword, B_FALSE);
}

	__checkReturn	efx_rc_t
ef10_rx_qflush(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_rc_t rc;

	if ((rc = efx_mcdi_fini_rxq(enp, erp->er_index)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
ef10_rx_qenable(
	__in	efx_rxq_t *erp)
{
	/* FIXME */
	_NOTE(ARGUNUSED(erp))
	/* FIXME */
}

	__checkReturn	efx_rc_t
ef10_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;
	boolean_t disable_scatter;
	unsigned int ps_buf_size;

	_NOTE(ARGUNUSED(id, erp))

	EFX_STATIC_ASSERT(EFX_EV_RX_NLABELS == (1 << ESF_DZ_RX_QLABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_RX_NLABELS);
	EFSYS_ASSERT3U(enp->en_rx_qcount + 1, <, encp->enc_rxq_limit);

	EFX_STATIC_ASSERT(ISP2(EFX_RXQ_MAXNDESCS));
	EFX_STATIC_ASSERT(ISP2(EFX_RXQ_MINNDESCS));

	if (!ISP2(n) || (n < EFX_RXQ_MINNDESCS) || (n > EFX_RXQ_MAXNDESCS)) {
		rc = EINVAL;
		goto fail1;
	}
	if (index >= encp->enc_rxq_limit) {
		rc = EINVAL;
		goto fail2;
	}

	switch (type) {
	case EFX_RXQ_TYPE_DEFAULT:
	case EFX_RXQ_TYPE_SCATTER:
		ps_buf_size = 0;
		break;
	default:
		rc = ENOTSUP;
		goto fail3;
	}

	EFSYS_ASSERT(ps_buf_size == 0);

	/* Scatter can only be disabled if the firmware supports doing so */
	if (type == EFX_RXQ_TYPE_SCATTER)
		disable_scatter = B_FALSE;
	else
		disable_scatter = encp->enc_rx_disable_scatter_supported;

	if ((rc = efx_mcdi_init_rxq(enp, n, eep->ee_index, label, index,
		    esmp, disable_scatter, ps_buf_size)) != 0)
		goto fail6;

	erp->er_eep = eep;
	erp->er_label = label;

	ef10_ev_rxlabel_init(eep, erp, label, ps_buf_size != 0);

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
ef10_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_evq_t *eep = erp->er_eep;
	unsigned int label = erp->er_label;

	ef10_ev_rxlabel_fini(eep, label);

	EFSYS_ASSERT(enp->en_rx_qcount != 0);
	--enp->en_rx_qcount;

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_rxq_t), erp);
}

		void
ef10_rx_fini(
	__in	efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */
