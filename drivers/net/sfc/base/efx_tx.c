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

#define	EFX_TX_QSTAT_INCR(_etp, _stat)


	__checkReturn	efx_rc_t
efx_tx_init(
	__in		efx_nic_t *enp)
{
	const efx_tx_ops_t *etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (!(enp->en_mod_flags & EFX_MOD_EV)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_mod_flags & EFX_MOD_TX) {
		rc = EINVAL;
		goto fail2;
	}

	switch (enp->en_family) {

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail3;
	}

	EFSYS_ASSERT3U(enp->en_tx_qcount, ==, 0);

	if ((rc = etxop->etxo_init(enp)) != 0)
		goto fail4;

	enp->en_etxop = etxop;
	enp->en_mod_flags |= EFX_MOD_TX;
	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_etxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_TX;
	return (rc);
}

			void
efx_tx_fini(
	__in	efx_nic_t *enp)
{
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TX);
	EFSYS_ASSERT3U(enp->en_tx_qcount, ==, 0);

	etxop->etxo_fini(enp);

	enp->en_etxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_TX;
}

	__checkReturn	efx_rc_t
efx_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__deref_out	efx_txq_t **etpp,
	__out		unsigned int *addedp)
{
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_txq_t *etp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TX);

	EFSYS_ASSERT3U(enp->en_tx_qcount + 1, <, encp->enc_txq_limit);

	/* Allocate an TXQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_txq_t), etp);

	if (etp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	etp->et_magic = EFX_TXQ_MAGIC;
	etp->et_enp = enp;
	etp->et_index = index;
	etp->et_mask = n - 1;
	etp->et_esmp = esmp;

	/* Initial descriptor index may be modified by etxo_qcreate */
	*addedp = 0;

	if ((rc = etxop->etxo_qcreate(enp, index, label, esmp,
	    n, id, flags, eep, etp, addedp)) != 0)
		goto fail2;

	enp->en_tx_qcount++;
	*etpp = etp;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_txq_t), etp);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

		void
efx_tx_qdestroy(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	EFSYS_ASSERT(enp->en_tx_qcount != 0);
	--enp->en_tx_qcount;

	etxop->etxo_qdestroy(etp);

	/* Free the TXQ object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_txq_t), etp);
}

	__checkReturn	efx_rc_t
efx_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if ((rc = etxop->etxo_qpost(etp, eb,
	    n, completed, addedp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

			void
efx_tx_qpush(
	__in	efx_txq_t *etp,
	__in	unsigned int added,
	__in	unsigned int pushed)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	etxop->etxo_qpush(etp, added, pushed);
}

	__checkReturn	efx_rc_t
efx_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if ((rc = etxop->etxo_qpace(etp, ns)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_tx_qflush(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if ((rc = etxop->etxo_qflush(etp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

			void
efx_tx_qenable(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	etxop->etxo_qenable(etp);
}

	__checkReturn	efx_rc_t
efx_tx_qpio_enable(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if (~enp->en_features & EFX_FEATURE_PIO_BUFFERS) {
		rc = ENOTSUP;
		goto fail1;
	}
	if (etxop->etxo_qpio_enable == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}
	if ((rc = etxop->etxo_qpio_enable(etp)) != 0)
		goto fail3;

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
efx_tx_qpio_disable(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if (etxop->etxo_qpio_disable != NULL)
		etxop->etxo_qpio_disable(etp);
}

	__checkReturn	efx_rc_t
efx_tx_qpio_write(
	__in			efx_txq_t *etp,
	__in_ecount(buf_length)	uint8_t *buffer,
	__in			size_t buf_length,
	__in			size_t pio_buf_offset)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if (etxop->etxo_qpio_write != NULL) {
		if ((rc = etxop->etxo_qpio_write(etp, buffer, buf_length,
						pio_buf_offset)) != 0)
			goto fail1;
		return (0);
	}

	return (ENOTSUP);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_tx_qpio_post(
	__in			efx_txq_t *etp,
	__in			size_t pkt_length,
	__in			unsigned int completed,
	__inout			unsigned int *addedp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if (etxop->etxo_qpio_post != NULL) {
		if ((rc = etxop->etxo_qpio_post(etp, pkt_length, completed,
						addedp)) != 0)
			goto fail1;
		return (0);
	}

	return (ENOTSUP);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_tx_qdesc_post(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_desc_t *ed,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if ((rc = etxop->etxo_qdesc_post(etp, ed,
	    n, completed, addedp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	void
efx_tx_qdesc_dma_create(
	__in	efx_txq_t *etp,
	__in	efsys_dma_addr_t addr,
	__in	size_t size,
	__in	boolean_t eop,
	__out	efx_desc_t *edp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT(etxop->etxo_qdesc_dma_create != NULL);

	etxop->etxo_qdesc_dma_create(etp, addr, size, eop, edp);
}

	void
efx_tx_qdesc_tso_create(
	__in	efx_txq_t *etp,
	__in	uint16_t ipv4_id,
	__in	uint32_t tcp_seq,
	__in	uint8_t  tcp_flags,
	__out	efx_desc_t *edp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT(etxop->etxo_qdesc_tso_create != NULL);

	etxop->etxo_qdesc_tso_create(etp, ipv4_id, tcp_seq, tcp_flags, edp);
}

	void
efx_tx_qdesc_tso2_create(
	__in			efx_txq_t *etp,
	__in			uint16_t ipv4_id,
	__in			uint32_t tcp_seq,
	__in			uint16_t mss,
	__out_ecount(count)	efx_desc_t *edp,
	__in			int count)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT(etxop->etxo_qdesc_tso2_create != NULL);

	etxop->etxo_qdesc_tso2_create(etp, ipv4_id, tcp_seq, mss, edp, count);
}

	void
efx_tx_qdesc_vlantci_create(
	__in	efx_txq_t *etp,
	__in	uint16_t tci,
	__out	efx_desc_t *edp)
{
	efx_nic_t *enp = etp->et_enp;
	const efx_tx_ops_t *etxop = enp->en_etxop;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT(etxop->etxo_qdesc_vlantci_create != NULL);

	etxop->etxo_qdesc_vlantci_create(etp, tci, edp);
}


