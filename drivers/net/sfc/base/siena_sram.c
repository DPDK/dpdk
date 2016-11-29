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

#if EFSYS_OPT_SIENA

			void
siena_sram_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_oword_t oword;
	uint32_t rx_base, tx_base;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	rx_base = encp->enc_buftbl_limit;
	tx_base = rx_base + (encp->enc_rxq_limit *
	    EFX_RXQ_DC_NDESCS(EFX_RXQ_DC_SIZE));

	/* Initialize the transmit descriptor cache */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_TX_DC_BASE_ADR, tx_base);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_TX_DC_CFG_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_TX_DC_SIZE, EFX_TXQ_DC_SIZE);
	EFX_BAR_WRITEO(enp, FR_AZ_TX_DC_CFG_REG, &oword);

	/* Initialize the receive descriptor cache */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_RX_DC_BASE_ADR, rx_base);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_RX_DC_CFG_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DC_SIZE, EFX_RXQ_DC_SIZE);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_DC_CFG_REG, &oword);

	/* Set receive descriptor pre-fetch low water mark */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DC_PF_LWM, 56);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_DC_PF_WM_REG, &oword);

	/* Set the event queue to use for SRAM updates */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_UPD_EVQ_ID, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_UPD_EVQ_REG, &oword);
}

#endif	/* EFSYS_OPT_SIENA */
