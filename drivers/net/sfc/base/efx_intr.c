/*
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
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


	__checkReturn	efx_rc_t
efx_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (enp->en_mod_flags & EFX_MOD_INTR) {
		rc = EINVAL;
		goto fail1;
	}

	eip->ei_esmp = esmp;
	eip->ei_type = type;
	eip->ei_level = 0;

	enp->en_mod_flags |= EFX_MOD_INTR;

	switch (enp->en_family) {

	default:
		EFSYS_ASSERT(B_FALSE);
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = eiop->eio_init(enp, type, esmp)) != 0)
		goto fail3;

	eip->ei_eiop = eiop;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
efx_intr_fini(
	__in	efx_nic_t *enp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_fini(enp);

	enp->en_mod_flags &= ~EFX_MOD_INTR;
}

			void
efx_intr_enable(
	__in		efx_nic_t *enp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_enable(enp);
}

			void
efx_intr_disable(
	__in		efx_nic_t *enp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_disable(enp);
}

			void
efx_intr_disable_unlocked(
	__in		efx_nic_t *enp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_disable_unlocked(enp);
}


	__checkReturn	efx_rc_t
efx_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	return (eiop->eio_trigger(enp, level));
}

			void
efx_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *qmaskp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_status_line(enp, fatalp, qmaskp);
}

			void
efx_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_status_message(enp, message, fatalp);
}

		void
efx_intr_fatal(
	__in	efx_nic_t *enp)
{
	efx_intr_t *eip = &(enp->en_intr);
	const efx_intr_ops_t *eiop = eip->ei_eiop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	eiop->eio_fatal(enp);
}


/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */

