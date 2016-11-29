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

#ifndef _SYS_SIENA_IMPL_H
#define	_SYS_SIENA_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_mcdi.h"
#include "siena_flash.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	SIENA_NVRAM_CHUNK 0x80

extern	__checkReturn	efx_rc_t
siena_nic_probe(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_nic_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_nic_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
siena_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern			void
siena_nic_fini(
	__in		efx_nic_t *enp);

extern			void
siena_nic_unprobe(
	__in		efx_nic_t *enp);

#define	SIENA_SRAM_ROWS	0x12000

extern			void
siena_sram_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
siena_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_sram_pattern_fn_t func);

#endif	/* EFSYS_OPT_DIAG */

#if EFSYS_OPT_MCDI

extern	__checkReturn	efx_rc_t
siena_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp);

extern			void
siena_mcdi_send_request(
	__in			efx_nic_t *enp,
	__in_bcount(hdr_len)	void *hdrp,
	__in			size_t hdr_len,
	__in_bcount(sdu_len)	void *sdup,
	__in			size_t sdu_len);

extern	__checkReturn	boolean_t
siena_mcdi_poll_response(
	__in		efx_nic_t *enp);

extern			void
siena_mcdi_read_response(
	__in			efx_nic_t *enp,
	__out_bcount(length)	void *bufferp,
	__in			size_t offset,
	__in			size_t length);

extern			efx_rc_t
siena_mcdi_poll_reboot(
	__in		efx_nic_t *enp);

extern			void
siena_mcdi_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_mcdi_feature_supported(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_feature_id_t id,
	__out		boolean_t *supportedp);

extern			void
siena_mcdi_get_timeout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__out		uint32_t *timeoutp);

#endif /* EFSYS_OPT_MCDI */

typedef struct siena_link_state_s {
	uint32_t		sls_adv_cap_mask;
	uint32_t		sls_lp_cap_mask;
	unsigned int		sls_fcntl;
	efx_link_mode_t		sls_link_mode;
	boolean_t		sls_mac_up;
} siena_link_state_t;

extern			void
siena_phy_link_ev(
	__in		efx_nic_t *enp,
	__in		efx_qword_t *eqp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
siena_phy_get_link(
	__in		efx_nic_t *enp,
	__out		siena_link_state_t *slsp);

extern	__checkReturn	efx_rc_t
siena_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on);

extern	__checkReturn	efx_rc_t
siena_phy_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_phy_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

extern						void
siena_phy_decode_stats(
	__in					efx_nic_t *enp,
	__in					uint32_t vmask,
	__in_opt				efsys_mem_t *esmp,
	__out_opt				uint64_t *smaskp,
	__inout_ecount_opt(EFX_PHY_NSTATS)	uint32_t *stat);

extern	__checkReturn			efx_rc_t
siena_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_BIST

extern	__checkReturn		efx_rc_t
siena_phy_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

extern	__checkReturn		efx_rc_t
siena_phy_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type,
	__out			efx_bist_result_t *resultp,
	__out_opt __drv_when(count > 0, __notnull)
	uint32_t	*value_maskp,
	__out_ecount_opt(count)	__drv_when(count > 0, __notnull)
	unsigned long	*valuesp,
	__in			size_t count);

extern				void
siena_phy_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

#endif	/* EFSYS_OPT_BIST */

extern	__checkReturn	efx_rc_t
siena_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
siena_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

extern	__checkReturn	efx_rc_t
siena_mac_reconfigure(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
siena_mac_pdu_get(
	__in	efx_nic_t *enp,
	__out	size_t *pdu);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SIENA_IMPL_H */
