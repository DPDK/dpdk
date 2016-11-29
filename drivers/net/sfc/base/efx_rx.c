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
efx_rx_init(
	__inout		efx_nic_t *enp)
{
	const efx_rx_ops_t *erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (!(enp->en_mod_flags & EFX_MOD_EV)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_mod_flags & EFX_MOD_RX) {
		rc = EINVAL;
		goto fail2;
	}

	switch (enp->en_family) {

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail3;
	}

	if ((rc = erxop->erxo_init(enp)) != 0)
		goto fail4;

	enp->en_erxop = erxop;
	enp->en_mod_flags |= EFX_MOD_RX;
	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_erxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_RX;
	return (rc);
}

			void
efx_rx_fini(
	__in		efx_nic_t *enp)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_rx_qcount, ==, 0);

	erxop->erxo_fini(enp);

	enp->en_erxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_RX;
}

			void
efx_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qpost(erp, addrp, size, n, completed, added);
}

			void
efx_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qpush(erp, added, pushedp);
}

	__checkReturn	efx_rc_t
efx_rx_qflush(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	if ((rc = erxop->erxo_qflush(erp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_rx_qenable(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qenable(erp);
}

	__checkReturn	efx_rc_t
efx_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__deref_out	efx_rxq_t **erpp)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rxq_t *erp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	/* Allocate an RXQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_rxq_t), erp);

	if (erp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	erp->er_magic = EFX_RXQ_MAGIC;
	erp->er_enp = enp;
	erp->er_index = index;
	erp->er_mask = n - 1;
	erp->er_esmp = esmp;

	if ((rc = erxop->erxo_qcreate(enp, index, label, type, esmp, n, id,
	    eep, erp)) != 0)
		goto fail2;

	enp->en_rx_qcount++;
	*erpp = erp;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_rxq_t), erp);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_rx_qdestroy(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qdestroy(erp);
}

	__checkReturn	efx_rc_t
efx_pseudo_hdr_pkt_length_get(
	__in		efx_rxq_t *erp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	return (erxop->erxo_prefix_pktlen(enp, buffer, lengthp));
}

