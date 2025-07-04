/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD

	__checkReturn	efx_rc_t
rhead_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t end_padding;
	uint32_t bandwidth;
	efx_rc_t rc;

	if ((rc = efx_mcdi_nic_board_cfg(enp)) != 0)
		goto fail1;

	/*
	 * The tunnel encapsulation initialization happens unconditionally
	 * for now.
	 */
	encp->enc_tunnel_encapsulations_supported =
	    (1u << EFX_TUNNEL_PROTOCOL_VXLAN) |
	    (1u << EFX_TUNNEL_PROTOCOL_NVGRE);

	/*
	 * Software limitation inherited from EF10. This limit is not
	 * increased since the hardware does not report this limit, it is
	 * handled internally resulting in a tunnel add error when there is no
	 * space for more UDP tunnels.
	 */
	encp->enc_tunnel_config_udp_entries_max = EFX_TUNNEL_MAXNENTRIES;

	encp->enc_clk_mult = 1; /* not used for Riverhead */

	EFX_STATIC_ASSERT(MC_CMD_INIT_RXQ_V4_IN_BUFFER_SIZE_BYTES_LEN == 4);
	/* Agrees with MC_CMD_INIT_RXQ_V4_IN_BUFFER_SIZE_BYTES_LEN */
	encp->enc_rx_dma_desc_size_max = UINT32_MAX;

	/*
	 * FIXME There are TxSend and TxSeg descriptors on Riverhead.
	 * TxSeg is bigger than TxSend.
	 */
	encp->enc_tx_dma_desc_size_max = EFX_MASK32(ESF_GZ_TX_SEND_LEN);
	/* No boundary crossing limits */
	encp->enc_tx_dma_desc_boundary = 0;

	/*
	 * Initialise design parameters to either a runtime value read from
	 * the design parameters area or the well known default value
	 * (see SF-119689-TC section 4.4 for details).
	 * FIXME: Read design parameters area values.
	 */
	encp->enc_tx_tso_max_header_ndescs =
	    ESE_EF100_DP_GZ_TSO_MAX_HDR_NUM_SEGS_DEFAULT;
	encp->enc_tx_tso_max_header_length =
	    ESE_EF100_DP_GZ_TSO_MAX_HDR_LEN_DEFAULT;
	encp->enc_tx_tso_max_payload_ndescs =
	    ESE_EF100_DP_GZ_TSO_MAX_PAYLOAD_NUM_SEGS_DEFAULT;
	encp->enc_tx_tso_max_payload_length =
	    ESE_EF100_DP_GZ_TSO_MAX_PAYLOAD_LEN_DEFAULT;
	encp->enc_tx_tso_max_nframes =
	    ESE_EF100_DP_GZ_TSO_MAX_NUM_FRAMES_DEFAULT;

	/*
	 * Riverhead does not put any restrictions on TCP header offset limit.
	 */
	encp->enc_tx_tso_tcp_header_offset_limit = UINT32_MAX;

	/*
	 * Set resource limits for MC_CMD_ALLOC_VIS. Note that we cannot use
	 * MC_CMD_GET_RESOURCE_LIMITS here as that reports the available
	 * resources (allocated to this PCIe function), which is zero until
	 * after we have allocated VIs.
	 */
	encp->enc_evq_limit = 1024;
	encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
	encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;

	encp->enc_buftbl_limit = UINT32_MAX;

	/*
	 * Riverhead event queue creation completes
	 * immediately (no initial event).
	 */
	encp->enc_evq_init_done_ev_supported = B_FALSE;

	/*
	 * Enable firmware workarounds for hardware errata.
	 * Expected responses are:
	 *  - 0 (zero):
	 *	Success: workaround enabled or disabled as requested.
	 *  - MC_CMD_ERR_ENOSYS (reported as ENOTSUP):
	 *	Firmware does not support the MC_CMD_WORKAROUND request.
	 *	(assume that the workaround is not supported).
	 *  - MC_CMD_ERR_ENOENT (reported as ENOENT):
	 *	Firmware does not support the requested workaround.
	 *  - MC_CMD_ERR_EPERM  (reported as EACCES):
	 *	Unprivileged function cannot enable/disable workarounds.
	 *
	 * See efx_mcdi_request_errcode() for MCDI error translations.
	 */

	/*
	 * Replay engine on Riverhead should suppress duplicate packets
	 * (e.g. because of exact multicast and all-multicast filters
	 * match) to the same RxQ.
	 */
	encp->enc_bug26807_workaround = B_FALSE;

	/*
	 * Checksums for TSO sends should always be correct on Riverhead.
	 * FIXME: revisit when TSO support is implemented.
	 */
	encp->enc_bug61297_workaround = B_FALSE;

	encp->enc_evq_max_nevs = RHEAD_EVQ_MAXNEVS;
	encp->enc_evq_min_nevs = RHEAD_EVQ_MINNEVS;
	encp->enc_rxq_max_ndescs = RHEAD_RXQ_MAXNDESCS;
	encp->enc_rxq_min_ndescs = RHEAD_RXQ_MINNDESCS;
	encp->enc_txq_max_ndescs = RHEAD_TXQ_MAXNDESCS;
	encp->enc_txq_min_ndescs = RHEAD_TXQ_MINNDESCS;

	/* Riverhead FW does not support event queue timers yet. */
	encp->enc_evq_timer_quantum_ns = 0;
	encp->enc_evq_timer_max_us = 0;

#if EFSYS_OPT_EV_EXTENDED_WIDTH
	encp->enc_ev_ew_desc_size = RHEAD_EVQ_EW_DESC_SIZE;
#else
	encp->enc_ev_ew_desc_size = 0;
#endif

	encp->enc_ev_desc_size = RHEAD_EVQ_DESC_SIZE;
	encp->enc_rx_desc_size = RHEAD_RXQ_DESC_SIZE;
	encp->enc_tx_desc_size = RHEAD_TXQ_DESC_SIZE;

	/* No required alignment for WPTR updates */
	encp->enc_rx_push_align = 1;

	/* Riverhead supports a single Rx prefix size. */
	encp->enc_rx_prefix_size = ESE_GZ_RX_PKT_PREFIX_LEN;

	/* Alignment for receive packet DMA buffers. */
	encp->enc_rx_buf_align_start = 1;

	/* Get the RX DMA end padding alignment configuration. */
	if ((rc = efx_mcdi_get_rxdp_config(enp, &end_padding)) != 0) {
		if (rc != EACCES)
			goto fail2;

		/* Assume largest tail padding size supported by hardware. */
		end_padding = 128;
	}
	encp->enc_rx_buf_align_end = end_padding;

	/* FIXME: It should be extracted from design parameters (Bug 86844) */
	encp->enc_rx_scatter_max = 7;

	/*
	 * Riverhead stores a single global copy of VPD, not per-PF as on
	 * Huntington.
	 */
	encp->enc_vpd_is_global = B_TRUE;

	rc = ef10_nic_get_port_mode_bandwidth(enp, &bandwidth);
	if (rc != 0)
		goto fail3;
	encp->enc_required_pcie_bandwidth_mbps = bandwidth;
	encp->enc_max_pcie_link_gen = EFX_PCIE_LINK_SPEED_GEN3;

	/*
	 * FIXME: MCDI table API support depends on an EF100 firmware build
	 * and an EF100 platform. It should be discovered by using a capability
	 * flag from MCDI that is not implemented yet.
	 * Right now we can safely rely on the return code from the libefx
	 * MCDI Table API.
	 */
	encp->enc_table_api_supported = B_TRUE;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
rhead_nic_probe(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	efx_rc_t rc;

	EFSYS_ASSERT(EFX_FAMILY_IS_EF100(enp));

	/* Read and clear any assertion state */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;

	/* Exit the assertion handler */
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		if (rc != EACCES)
			goto fail2;

	if ((rc = efx_mcdi_drv_attach(enp, B_TRUE)) != 0)
		goto fail3;

	/* Get remaining controller-specific board config */
	if ((rc = enop->eno_board_cfg(enp)) != 0)
		goto fail4;

	/*
	 * Set default driver config limits (based on board config).
	 *
	 * FIXME: For now allocate a fixed number of VIs which is likely to be
	 * sufficient and small enough to allow multiple functions on the same
	 * port.
	 */
	edcp->edc_min_vi_count = edcp->edc_max_vi_count =
	    MIN(128, MAX(encp->enc_rxq_limit, encp->enc_txq_limit));

	/*
	 * The client driver must configure and enable PIO buffer support,
	 * but there is no PIO support on Riverhead anyway.
	 */
	edcp->edc_max_piobuf_count = 0;
	edcp->edc_pio_alloc_size = 0;

#if EFSYS_OPT_MAC_STATS
	/* Wipe the MAC statistics */
	if ((rc = efx_mcdi_mac_stats_clear(enp)) != 0)
		goto fail5;
#endif

#if EFSYS_OPT_LOOPBACK
	if ((rc = efx_mcdi_get_loopback_modes(enp)) != 0)
		goto fail6;
#endif

	return (0);

#if EFSYS_OPT_LOOPBACK
fail6:
	EFSYS_PROBE(fail6);
#endif
#if EFSYS_OPT_MAC_STATS
fail5:
	EFSYS_PROBE(fail5);
#endif
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
rhead_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	uint32_t min_evq_count, max_evq_count;
	uint32_t min_rxq_count, max_rxq_count;
	uint32_t min_txq_count, max_txq_count;
	efx_rc_t rc;

	if (edlp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/* Get minimum required and maximum usable VI limits */
	min_evq_count = MIN(edlp->edl_min_evq_count, encp->enc_evq_limit);
	min_rxq_count = MIN(edlp->edl_min_rxq_count, encp->enc_rxq_limit);
	min_txq_count = MIN(edlp->edl_min_txq_count, encp->enc_txq_limit);

	edcp->edc_min_vi_count =
	    MAX(min_evq_count, MAX(min_rxq_count, min_txq_count));

	max_evq_count = MIN(edlp->edl_max_evq_count, encp->enc_evq_limit);
	max_rxq_count = MIN(edlp->edl_max_rxq_count, encp->enc_rxq_limit);
	max_txq_count = MIN(edlp->edl_max_txq_count, encp->enc_txq_limit);

	edcp->edc_max_vi_count =
	    MAX(max_evq_count, MAX(max_rxq_count, max_txq_count));

	/* There is no PIO support on Riverhead */
	edcp->edc_max_piobuf_count = 0;
	edcp->edc_pio_alloc_size = 0;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
rhead_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	/* ef10_nic_reset() is called to recover from BADASSERT failures. */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail2;

	if ((rc = efx_mcdi_entity_reset(enp)) != 0)
		goto fail3;

	/* Clear RX/TX DMA queue errors */
	enp->en_reset_flags &= ~(EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
rhead_nic_init(
	__in		efx_nic_t *enp)
{
	const efx_drv_cfg_t *edcp = &(enp->en_drv_cfg);
	uint32_t min_vi_count, max_vi_count;
	uint32_t vi_count, vi_base, vi_shift;
	uint32_t vi_window_size;
	efx_rc_t rc;
	boolean_t alloc_vadaptor = B_TRUE;

	EFSYS_ASSERT(EFX_FAMILY_IS_EF100(enp));
	EFSYS_ASSERT3U(edcp->edc_max_piobuf_count, ==, 0);

	/* Enable reporting of some events (e.g. link change) */
	if ((rc = efx_mcdi_log_ctrl(enp)) != 0)
		goto fail1;

	min_vi_count = edcp->edc_min_vi_count;
	max_vi_count = edcp->edc_max_vi_count;

	/* Ensure that the previously attached driver's VIs are freed */
	if ((rc = efx_mcdi_free_vis(enp)) != 0)
		goto fail2;

	/*
	 * Reserve VI resources (EVQ+RXQ+TXQ) for this PCIe function. If this
	 * fails then retrying the request for fewer VI resources may succeed.
	 */
	vi_count = 0;
	if ((rc = efx_mcdi_alloc_vis(enp, min_vi_count, max_vi_count,
		    &vi_base, &vi_count, &vi_shift)) != 0)
		goto fail3;

	EFSYS_PROBE2(vi_alloc, uint32_t, vi_base, uint32_t, vi_count);

	if (vi_count < min_vi_count) {
		rc = ENOMEM;
		goto fail4;
	}

	enp->en_arch.ef10.ena_vi_base = vi_base;
	enp->en_arch.ef10.ena_vi_count = vi_count;
	enp->en_arch.ef10.ena_vi_shift = vi_shift;

	EFSYS_ASSERT3U(enp->en_nic_cfg.enc_vi_window_shift, !=,
	    EFX_VI_WINDOW_SHIFT_INVALID);
	EFSYS_ASSERT3U(enp->en_nic_cfg.enc_vi_window_shift, <=,
	    EFX_VI_WINDOW_SHIFT_64K);
	vi_window_size = 1U << enp->en_nic_cfg.enc_vi_window_shift;

	/* Save UC memory mapping details */
	enp->en_arch.ef10.ena_uc_mem_map_offset = 0;
	enp->en_arch.ef10.ena_uc_mem_map_size =
	    vi_window_size * enp->en_arch.ef10.ena_vi_count;

	/* No WC memory mapping since PIO is not supported */
	enp->en_arch.ef10.ena_pio_write_vi_base = 0;
	enp->en_arch.ef10.ena_wc_mem_map_offset = 0;
	enp->en_arch.ef10.ena_wc_mem_map_size = 0;

	enp->en_nic_cfg.enc_mcdi_max_payload_length = MCDI_CTL_SDU_LEN_MAX_V2;

	/*
	 * For SR-IOV use case, vAdaptor is allocated for PF and associated VFs
	 * during NIC initialization when vSwitch is created and vPorts are
	 * allocated. Hence, skip vAdaptor allocation for EVB and update vPort
	 * ID in NIC structure with the one allocated for PF.
	 */

	enp->en_vport_id = EVB_PORT_ID_ASSIGNED;
#if EFSYS_OPT_EVB
	if ((enp->en_vswitchp != NULL) && (enp->en_vswitchp->ev_evcp != NULL)) {
		/* For EVB use vPort allocated on vSwitch */
		enp->en_vport_id = enp->en_vswitchp->ev_evcp->evc_vport_id;
		alloc_vadaptor = B_FALSE;
	}
#endif
	if (alloc_vadaptor != B_FALSE) {
		/* Allocate a vAdaptor attached to our upstream vPort/pPort */
		if ((rc = ef10_upstream_port_vadaptor_alloc(enp)) != 0)
			goto fail5;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);

fail4:
	EFSYS_PROBE(fail4);

	(void) efx_mcdi_free_vis(enp);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
rhead_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp)
{
	/*
	 * Report VIs that the client driver can use.
	 * Do not include VIs used for PIO buffer writes.
	 */
	*vi_countp = enp->en_arch.ef10.ena_vi_count;

	return (0);
}

	__checkReturn	efx_rc_t
rhead_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	efx_rc_t rc;

	EFSYS_ASSERT(EFX_FAMILY_IS_EF100(enp));

	/*
	 * TODO: Specify host memory mapping alignment and granularity
	 * in efx_drv_limits_t so that they can be taken into account
	 * when allocating extra VIs for PIO writes.
	 */
	switch (region) {
	case EFX_REGION_VI:
		/* UC mapped memory BAR region for VI registers */
		*offsetp = enp->en_arch.ef10.ena_uc_mem_map_offset;
		*sizep = enp->en_arch.ef10.ena_uc_mem_map_size;
		break;

	case EFX_REGION_PIO_WRITE_VI:
		/* WC mapped memory BAR region for piobuf writes */
		*offsetp = enp->en_arch.ef10.ena_wc_mem_map_offset;
		*sizep = enp->en_arch.ef10.ena_wc_mem_map_size;
		break;

	default:
		rc = EINVAL;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	boolean_t
rhead_nic_hw_unavailable(
	__in		efx_nic_t *enp)
{
	efx_dword_t dword;

	if (enp->en_reset_flags & EFX_RESET_HW_UNAVAIL)
		return (B_TRUE);

	EFX_BAR_FCW_READD(enp, ER_GZ_MC_SFT_STATUS, &dword);
	if (EFX_DWORD_FIELD(dword, EFX_DWORD_0) == 0xffffffff)
		goto unavail;

	return (B_FALSE);

unavail:
	rhead_nic_set_hw_unavailable(enp);

	return (B_TRUE);
}

			void
rhead_nic_set_hw_unavailable(
	__in		efx_nic_t *enp)
{
	EFSYS_PROBE(hw_unavail);
	enp->en_reset_flags |= EFX_RESET_HW_UNAVAIL;
}

			void
rhead_nic_fini(
	__in		efx_nic_t *enp)
{
	boolean_t do_vadaptor_free = B_TRUE;

#if EFSYS_OPT_EVB
	if (enp->en_vswitchp != NULL) {
		/*
		 * For SR-IOV the vAdaptor is freed with the vSwitch,
		 * so do not free it here.
		 */
		do_vadaptor_free = B_FALSE;
	}
#endif
	if (do_vadaptor_free != B_FALSE) {
		(void) efx_mcdi_vadaptor_free(enp, enp->en_vport_id);
		enp->en_vport_id = EVB_PORT_ID_NULL;
	}

	(void) efx_mcdi_free_vis(enp);
	enp->en_arch.ef10.ena_vi_count = 0;
}

			void
rhead_nic_unprobe(
	__in		efx_nic_t *enp)
{
	(void) efx_mcdi_drv_attach(enp, B_FALSE);
}

#if EFSYS_OPT_DIAG

	__checkReturn	efx_rc_t
rhead_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	/* FIXME */
	_NOTE(ARGUNUSED(enp))
	_NOTE(CONSTANTCONDITION)
	if (B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}
	/* FIXME */

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn			efx_rc_t
rhead_nic_xilinx_cap_tbl_read_ef100_locator(
	__in				efsys_bar_t *esbp,
	__in				efsys_dma_addr_t offset,
	__out				efx_bar_region_t *ebrp)
{
	efx_oword_t entry;
	uint32_t rev;
	uint32_t len;
	efx_rc_t rc;

	/*
	 * Xilinx Capabilities Table requires 32bit aligned reads.
	 * See SF-119689-TC section 4.2.2 "Discovery Steps".
	 */
	EFSYS_BAR_READD(esbp, offset +
			(EFX_LOW_BIT(ESF_GZ_CFGBAR_ENTRY_FORMAT) / 8),
			&entry.eo_dword[0], B_FALSE);
	EFSYS_BAR_READD(esbp, offset +
			(EFX_LOW_BIT(ESF_GZ_CFGBAR_ENTRY_SIZE) / 8),
			&entry.eo_dword[1], B_FALSE);

	rev = EFX_OWORD_FIELD32(entry, ESF_GZ_CFGBAR_ENTRY_REV);
	len = EFX_OWORD_FIELD32(entry, ESF_GZ_CFGBAR_ENTRY_SIZE);

	if (rev != ESE_GZ_CFGBAR_ENTRY_REV_EF100 ||
	    len < ESE_GZ_CFGBAR_ENTRY_SIZE_EF100) {
		rc = EINVAL;
		goto fail1;
	}

	EFSYS_BAR_READD(esbp, offset +
			(EFX_LOW_BIT(ESF_GZ_CFGBAR_EF100_BAR) / 8),
			&entry.eo_dword[2], B_FALSE);

	ebrp->ebr_index = EFX_OWORD_FIELD32(entry, ESF_GZ_CFGBAR_EF100_BAR);
	ebrp->ebr_offset = EFX_OWORD_FIELD32(entry,
			ESF_GZ_CFGBAR_EF100_FUNC_CTL_WIN_OFF) <<
			ESE_GZ_EF100_FUNC_CTL_WIN_OFF_SHIFT;
	ebrp->ebr_type = EFX_BAR_TYPE_MEM;
	ebrp->ebr_length = 0;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_RIVERHEAD */
