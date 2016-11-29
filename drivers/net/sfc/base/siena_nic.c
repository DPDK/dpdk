/*
 * Copyright (c) 2009-2016 Solarflare Communications Inc.
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
#include "mcdi_mon.h"

#if EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
siena_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint8_t mac_addr[6];
	efx_dword_t capabilities;
	uint32_t board_type;
	uint32_t nevq, nrxq, ntxq;
	efx_rc_t rc;

	/* External port identifier using one-based port numbering */
	encp->enc_external_port = (uint8_t)enp->en_mcdi.em_emip.emi_port;

	/* Board configuration */
	if ((rc = efx_mcdi_get_board_cfg(enp, &board_type,
		    &capabilities, mac_addr)) != 0)
		goto fail1;

	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, mac_addr);

	encp->enc_board_type = board_type;

	/*
	 * There is no possibility to determine the number of PFs on Siena
	 * by issuing MCDI request, and it is not an easy task to find the
	 * value based on the board type, so 'enc_hw_pf_count' is set to 1
	 */
	encp->enc_hw_pf_count = 1;

	/* Additional capabilities */
	encp->enc_clk_mult = 1;
	if (EFX_DWORD_FIELD(capabilities, MC_CMD_CAPABILITIES_TURBO)) {
		enp->en_features |= EFX_FEATURE_TURBO;

		if (EFX_DWORD_FIELD(capabilities,
			MC_CMD_CAPABILITIES_TURBO_ACTIVE)) {
			encp->enc_clk_mult = 2;
		}
	}

	encp->enc_evq_timer_quantum_ns =
		EFX_EVQ_SIENA_TIMER_QUANTUM_NS / encp->enc_clk_mult;
	encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		FRF_CZ_TC_TIMER_VAL_WIDTH) / 1000;

	/* When hash header insertion is enabled, Siena inserts 16 bytes */
	encp->enc_rx_prefix_size = 16;

	/* Alignment for receive packet DMA buffers */
	encp->enc_rx_buf_align_start = 1;
	encp->enc_rx_buf_align_end = 1;

	/* Alignment for WPTR updates */
	encp->enc_rx_push_align = 1;

	/* Resource limits */
	rc = efx_mcdi_get_resource_limits(enp, &nevq, &nrxq, &ntxq);
	if (rc != 0) {
		if (rc != ENOTSUP)
			goto fail2;

		nevq = 1024;
		nrxq = EFX_RXQ_LIMIT_TARGET;
		ntxq = EFX_TXQ_LIMIT_TARGET;
	}
	encp->enc_evq_limit = nevq;
	encp->enc_rxq_limit = MIN(EFX_RXQ_LIMIT_TARGET, nrxq);
	encp->enc_txq_limit = MIN(EFX_TXQ_LIMIT_TARGET, ntxq);

	encp->enc_txq_max_ndescs = 4096;

	encp->enc_buftbl_limit = SIENA_SRAM_ROWS -
	    (encp->enc_txq_limit * EFX_TXQ_DC_NDESCS(EFX_TXQ_DC_SIZE)) -
	    (encp->enc_rxq_limit * EFX_RXQ_DC_NDESCS(EFX_RXQ_DC_SIZE));

	encp->enc_hw_tx_insert_vlan_enabled = B_FALSE;
	encp->enc_fw_assisted_tso_enabled = B_FALSE;
	encp->enc_fw_assisted_tso_v2_enabled = B_FALSE;
	encp->enc_fw_assisted_tso_v2_n_contexts = 0;
	encp->enc_allow_set_mac_with_installed_filters = B_TRUE;
	encp->enc_rx_packed_stream_supported = B_FALSE;
	encp->enc_rx_var_packed_stream_supported = B_FALSE;

	/* Siena supports two 10G ports, and 8 lanes of PCIe Gen2 */
	encp->enc_required_pcie_bandwidth_mbps = 2 * 10000;
	encp->enc_max_pcie_link_gen = EFX_PCIE_LINK_SPEED_GEN2;

	encp->enc_fw_verified_nvram_update_required = B_FALSE;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
siena_phy_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	/* Fill out fields in enp->en_port and enp->en_nic_cfg from MCDI */
	if ((rc = efx_mcdi_get_phy_cfg(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
siena_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	siena_link_state_t sls;
	unsigned int mask;
	efx_oword_t oword;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Test BIU */
	if ((rc = efx_nic_biu_test(enp)) != 0)
		goto fail1;

	/* Clear the region register */
	EFX_POPULATE_OWORD_4(oword,
	    FRF_AZ_ADR_REGION0, 0,
	    FRF_AZ_ADR_REGION1, (1 << 16),
	    FRF_AZ_ADR_REGION2, (2 << 16),
	    FRF_AZ_ADR_REGION3, (3 << 16));
	EFX_BAR_WRITEO(enp, FR_AZ_ADR_REGION_REG, &oword);

	/* Read clear any assertion state */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail2;

	/* Exit the assertion handler */
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail3;

	/* Wrestle control from the BMC */
	if ((rc = efx_mcdi_drv_attach(enp, B_TRUE)) != 0)
		goto fail4;

	if ((rc = siena_board_cfg(enp)) != 0)
		goto fail5;

	if ((rc = siena_phy_cfg(enp)) != 0)
		goto fail6;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = siena_nic_reset(enp)) != 0)
		goto fail7;
	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail8;
	epp->ep_default_adv_cap_mask = sls.sls_adv_cap_mask;
	epp->ep_adv_cap_mask = sls.sls_adv_cap_mask;

	encp->enc_features = enp->en_features;

	return (0);

fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
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
siena_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* siena_nic_reset() is called to recover from BADASSERT failures. */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail2;

	/*
	 * Bug24908: ENTITY_RESET_IN_LEN is non zero but zero may be supplied
	 * for backwards compatibility with PORT_RESET_IN_LEN.
	 */
	EFX_STATIC_ASSERT(MC_CMD_ENTITY_RESET_OUT_LEN == 0);

	req.emr_cmd = MC_CMD_ENTITY_RESET;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (0);
}

static			void
siena_nic_rx_cfg(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;

	/*
	 * RX_INGR_EN is always enabled on Siena, because we rely on
	 * the RX parser to be resiliant to missing SOP/EOP.
	 */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_INGR_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Disable parsing of additional 802.1Q in Q packets */
	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_FILTER_ALL_VLAN_ETHERTYPES, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
}

static			void
siena_nic_usrev_dis(
	__in		efx_nic_t *enp)
{
	efx_oword_t	oword;

	EFX_POPULATE_OWORD_1(oword, FRF_CZ_USREV_DIS, 1);
	EFX_BAR_WRITEO(enp, FR_CZ_USR_EV_CFG, &oword);
}

	__checkReturn	efx_rc_t
siena_nic_init(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Enable reporting of some events (e.g. link change) */
	if ((rc = efx_mcdi_log_ctrl(enp)) != 0)
		goto fail1;

	siena_sram_init(enp);

	/* Configure Siena's RX block */
	siena_nic_rx_cfg(enp);

	/* Disable USR_EVents for now */
	siena_nic_usrev_dis(enp);

	/* bug17057: Ensure set_link is called */
	if ((rc = siena_phy_reconfigure(enp)) != 0)
		goto fail2;

	enp->en_nic_cfg.enc_mcdi_max_payload_length = MCDI_CTL_SDU_LEN_MAX_V1;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
siena_nic_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

			void
siena_nic_unprobe(
	__in		efx_nic_t *enp)
{
	(void) efx_mcdi_drv_attach(enp, B_FALSE);
}

#endif	/* EFSYS_OPT_SIENA */
