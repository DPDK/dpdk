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

#define	EFX_EV_QSTAT_INCR(_eep, _stat)

#define	EFX_EV_PRESENT(_qword)						\
	(EFX_QWORD_FIELD((_qword), EFX_DWORD_0) != 0xffffffff &&	\
	EFX_QWORD_FIELD((_qword), EFX_DWORD_1) != 0xffffffff)



	__checkReturn	efx_rc_t
efx_ev_init(
	__in		efx_nic_t *enp)
{
	const efx_ev_ops_t *eevop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);

	if (enp->en_mod_flags & EFX_MOD_EV) {
		rc = EINVAL;
		goto fail1;
	}

	switch (enp->en_family) {

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_ASSERT3U(enp->en_ev_qcount, ==, 0);

	if ((rc = eevop->eevo_init(enp)) != 0)
		goto fail2;

	enp->en_eevop = eevop;
	enp->en_mod_flags |= EFX_MOD_EV;
	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_eevop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_EV;
	return (rc);
}

		void
efx_ev_fini(
	__in	efx_nic_t *enp)
{
	const efx_ev_ops_t *eevop = enp->en_eevop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_INTR);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_EV);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));
	EFSYS_ASSERT3U(enp->en_ev_qcount, ==, 0);

	eevop->eevo_fini(enp);

	enp->en_eevop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_EV;
}


	__checkReturn	efx_rc_t
efx_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint32_t us,
	__in		uint32_t flags,
	__deref_out	efx_evq_t **eepp)
{
	const efx_ev_ops_t *eevop = enp->en_eevop;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_evq_t *eep;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_EV);

	EFSYS_ASSERT3U(enp->en_ev_qcount + 1, <, encp->enc_evq_limit);

	switch (flags & EFX_EVQ_FLAGS_NOTIFY_MASK) {
	case EFX_EVQ_FLAGS_NOTIFY_INTERRUPT:
		break;
	case EFX_EVQ_FLAGS_NOTIFY_DISABLED:
		if (us != 0) {
			rc = EINVAL;
			goto fail1;
		}
		break;
	default:
		rc = EINVAL;
		goto fail2;
	}

	/* Allocate an EVQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_evq_t), eep);
	if (eep == NULL) {
		rc = ENOMEM;
		goto fail3;
	}

	eep->ee_magic = EFX_EVQ_MAGIC;
	eep->ee_enp = enp;
	eep->ee_index = index;
	eep->ee_mask = n - 1;
	eep->ee_flags = flags;
	eep->ee_esmp = esmp;

	/*
	 * Set outputs before the queue is created because interrupts may be
	 * raised for events immediately after the queue is created, before the
	 * function call below returns. See bug58606.
	 *
	 * The eepp pointer passed in by the client must therefore point to data
	 * shared with the client's event processing context.
	 */
	enp->en_ev_qcount++;
	*eepp = eep;

	if ((rc = eevop->eevo_qcreate(enp, index, esmp, n, id, us, flags,
	    eep)) != 0)
		goto fail4;

	return (0);

fail4:
	EFSYS_PROBE(fail4);

	*eepp = NULL;
	enp->en_ev_qcount--;
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_evq_t), eep);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

		void
efx_ev_qdestroy(
	__in	efx_evq_t *eep)
{
	efx_nic_t *enp = eep->ee_enp;
	const efx_ev_ops_t *eevop = enp->en_eevop;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);

	EFSYS_ASSERT(enp->en_ev_qcount != 0);
	--enp->en_ev_qcount;

	eevop->eevo_qdestroy(eep);

	/* Free the EVQ object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_evq_t), eep);
}

	__checkReturn	efx_rc_t
efx_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count)
{
	efx_nic_t *enp = eep->ee_enp;
	const efx_ev_ops_t *eevop = enp->en_eevop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);

	if (!(enp->en_mod_flags & EFX_MOD_INTR)) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = eevop->eevo_qprime(eep, count)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	boolean_t
efx_ev_qpending(
	__in		efx_evq_t *eep,
	__in		unsigned int count)
{
	size_t offset;
	efx_qword_t qword;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);

	offset = (count & eep->ee_mask) * sizeof (efx_qword_t);
	EFSYS_MEM_READQ(eep->ee_esmp, offset, &qword);

	return (EFX_EV_PRESENT(qword));
}

#define	EFX_EV_BATCH	8

			void
efx_ev_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_qword_t ev[EFX_EV_BATCH];
	unsigned int batch;
	unsigned int total;
	unsigned int count;
	unsigned int index;
	size_t offset;

	/* Ensure events codes match for EF10 (Huntington/Medford) and Siena */
	EFX_STATIC_ASSERT(ESF_DZ_EV_CODE_LBN == FSF_AZ_EV_CODE_LBN);
	EFX_STATIC_ASSERT(ESF_DZ_EV_CODE_WIDTH == FSF_AZ_EV_CODE_WIDTH);

	EFX_STATIC_ASSERT(ESE_DZ_EV_CODE_RX_EV == FSE_AZ_EV_CODE_RX_EV);
	EFX_STATIC_ASSERT(ESE_DZ_EV_CODE_TX_EV == FSE_AZ_EV_CODE_TX_EV);
	EFX_STATIC_ASSERT(ESE_DZ_EV_CODE_DRIVER_EV == FSE_AZ_EV_CODE_DRIVER_EV);
	EFX_STATIC_ASSERT(ESE_DZ_EV_CODE_DRV_GEN_EV ==
	    FSE_AZ_EV_CODE_DRV_GEN_EV);

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);
	EFSYS_ASSERT(countp != NULL);
	EFSYS_ASSERT(eecp != NULL);

	count = *countp;
	do {
		/* Read up until the end of the batch period */
		batch = EFX_EV_BATCH - (count & (EFX_EV_BATCH - 1));
		offset = (count & eep->ee_mask) * sizeof (efx_qword_t);
		for (total = 0; total < batch; ++total) {
			EFSYS_MEM_READQ(eep->ee_esmp, offset, &(ev[total]));

			if (!EFX_EV_PRESENT(ev[total]))
				break;

			EFSYS_PROBE3(event, unsigned int, eep->ee_index,
			    uint32_t, EFX_QWORD_FIELD(ev[total], EFX_DWORD_1),
			    uint32_t, EFX_QWORD_FIELD(ev[total], EFX_DWORD_0));

			offset += sizeof (efx_qword_t);
		}

		/* Process the batch of events */
		for (index = 0; index < total; ++index) {
			boolean_t should_abort;
			uint32_t code;

			EFX_EV_QSTAT_INCR(eep, EV_ALL);

			code = EFX_QWORD_FIELD(ev[index], FSF_AZ_EV_CODE);
			switch (code) {
			case FSE_AZ_EV_CODE_RX_EV:
				should_abort = eep->ee_rx(eep,
				    &(ev[index]), eecp, arg);
				break;
			case FSE_AZ_EV_CODE_TX_EV:
				should_abort = eep->ee_tx(eep,
				    &(ev[index]), eecp, arg);
				break;
			case FSE_AZ_EV_CODE_DRIVER_EV:
				should_abort = eep->ee_driver(eep,
				    &(ev[index]), eecp, arg);
				break;
			case FSE_AZ_EV_CODE_DRV_GEN_EV:
				should_abort = eep->ee_drv_gen(eep,
				    &(ev[index]), eecp, arg);
				break;
			case FSE_AZ_EV_CODE_GLOBAL_EV:
				if (eep->ee_global) {
					should_abort = eep->ee_global(eep,
					    &(ev[index]), eecp, arg);
					break;
				}
				/* else fallthrough */
			default:
				EFSYS_PROBE3(bad_event,
				    unsigned int, eep->ee_index,
				    uint32_t,
				    EFX_QWORD_FIELD(ev[index], EFX_DWORD_1),
				    uint32_t,
				    EFX_QWORD_FIELD(ev[index], EFX_DWORD_0));

				EFSYS_ASSERT(eecp->eec_exception != NULL);
				(void) eecp->eec_exception(arg,
					EFX_EXCEPTION_EV_ERROR, code);
				should_abort = B_TRUE;
			}
			if (should_abort) {
				/* Ignore subsequent events */
				total = index + 1;
				break;
			}
		}

		/*
		 * Now that the hardware has most likely moved onto dma'ing
		 * into the next cache line, clear the processed events. Take
		 * care to only clear out events that we've processed
		 */
		EFX_SET_QWORD(ev[0]);
		offset = (count & eep->ee_mask) * sizeof (efx_qword_t);
		for (index = 0; index < total; ++index) {
			EFSYS_MEM_WRITEQ(eep->ee_esmp, offset, &(ev[0]));
			offset += sizeof (efx_qword_t);
		}

		count += total;

	} while (total == batch);

	*countp = count;
}

			void
efx_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data)
{
	efx_nic_t *enp = eep->ee_enp;
	const efx_ev_ops_t *eevop = enp->en_eevop;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);

	EFSYS_ASSERT(eevop != NULL &&
	    eevop->eevo_qpost != NULL);

	eevop->eevo_qpost(eep, data);
}

	__checkReturn	efx_rc_t
efx_ev_usecs_to_ticks(
	__in		efx_nic_t *enp,
	__in		unsigned int us,
	__out		unsigned int *ticksp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	unsigned int ticks;

	/* Convert microseconds to a timer tick count */
	if (us == 0)
		ticks = 0;
	else if (us * 1000 < encp->enc_evq_timer_quantum_ns)
		ticks = 1;	/* Never round down to zero */
	else
		ticks = us * 1000 / encp->enc_evq_timer_quantum_ns;

	*ticksp = ticks;
	return (0);
}

	__checkReturn	efx_rc_t
efx_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us)
{
	efx_nic_t *enp = eep->ee_enp;
	const efx_ev_ops_t *eevop = enp->en_eevop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);

	if ((eep->ee_flags & EFX_EVQ_FLAGS_NOTIFY_MASK) ==
	    EFX_EVQ_FLAGS_NOTIFY_DISABLED) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = eevop->eevo_qmoderate(eep, us)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

