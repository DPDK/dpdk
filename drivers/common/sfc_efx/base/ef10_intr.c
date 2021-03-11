/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2012-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFX_OPTS_EF10()

	__checkReturn	efx_rc_t
ef10_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp)
{
	_NOTE(ARGUNUSED(enp, type, esmp))
	return (0);
}


			void
ef10_intr_enable(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}


			void
ef10_intr_disable(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}


			void
ef10_intr_disable_unlocked(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}


static	__checkReturn	efx_rc_t
efx_mcdi_trigger_interrupt(
	__in		efx_nic_t *enp,
	__in		unsigned int level)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_TRIGGER_INTERRUPT_IN_LEN,
		MC_CMD_TRIGGER_INTERRUPT_OUT_LEN);
	efx_rc_t rc;

	EFSYS_ASSERT(EFX_FAMILY_IS_EF10(enp));

	if (level >= enp->en_nic_cfg.enc_intr_limit) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_TRIGGER_INTERRUPT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_TRIGGER_INTERRUPT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_TRIGGER_INTERRUPT_OUT_LEN;

	MCDI_IN_SET_DWORD(req, TRIGGER_INTERRUPT_IN_INTR_LEVEL, level);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	if (encp->enc_bug41750_workaround) {
		/*
		 * bug 41750: Test interrupts don't work on Greenport
		 * bug 50084: Test interrupts don't work on VFs
		 */
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = efx_mcdi_trigger_interrupt(enp, level)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
ef10_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *qmaskp)
{
	efx_dword_t dword;

	EFSYS_ASSERT(EFX_FAMILY_IS_EF10(enp));

	/* Read the queue mask and implicitly acknowledge the interrupt. */
	EFX_BAR_READD(enp, ER_DZ_BIU_INT_ISR_REG, &dword, B_FALSE);
	*qmaskp = EFX_DWORD_FIELD(dword, EFX_DWORD_0);

	EFSYS_PROBE1(qmask, uint32_t, *qmaskp);

	*fatalp = B_FALSE;
}

			void
ef10_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp)
{
	EFSYS_ASSERT(EFX_FAMILY_IS_EF10(enp));

	_NOTE(ARGUNUSED(enp, message))

	/* EF10 fatal errors are reported via events */
	*fatalp = B_FALSE;
}

			void
ef10_intr_fatal(
	__in		efx_nic_t *enp)
{
	/* EF10 fatal errors are reported via events */
	_NOTE(ARGUNUSED(enp))
}

			void
ef10_intr_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

#endif	/* EFX_OPTS_EF10() */
