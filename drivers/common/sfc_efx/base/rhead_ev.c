/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_RIVERHEAD

/*
 * Non-interrupting event queue requires interrupting event queue to
 * refer to for wake-up events even if wake ups are never used.
 * It could be even non-allocated event queue.
 */
#define	EFX_RHEAD_ALWAYS_INTERRUPTING_EVQ_INDEX	(0)


	__checkReturn	efx_rc_t
rhead_ev_init(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))

	return (0);
}

			void
rhead_ev_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

	__checkReturn	efx_rc_t
rhead_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		uint32_t us,
	__in		uint32_t flags,
	__in		efx_evq_t *eep)
{
	uint32_t irq;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(id))	/* buftbl id managed by MC */

	/* Set up the handler table */
	eep->ee_rx	= NULL; /* FIXME */
	eep->ee_tx	= NULL; /* FIXME */
	eep->ee_driver	= NULL; /* FIXME */
	eep->ee_drv_gen	= NULL; /* FIXME */
	eep->ee_mcdi	= NULL; /* FIXME */

	/* Set up the event queue */
	/* INIT_EVQ expects function-relative vector number */
	if ((flags & EFX_EVQ_FLAGS_NOTIFY_MASK) ==
	    EFX_EVQ_FLAGS_NOTIFY_INTERRUPT) {
		irq = index;
	} else if (index == EFX_RHEAD_ALWAYS_INTERRUPTING_EVQ_INDEX) {
		irq = index;
		flags = (flags & ~EFX_EVQ_FLAGS_NOTIFY_MASK) |
		    EFX_EVQ_FLAGS_NOTIFY_INTERRUPT;
	} else {
		irq = EFX_RHEAD_ALWAYS_INTERRUPTING_EVQ_INDEX;
	}

	/*
	 * Interrupts may be raised for events immediately after the queue is
	 * created. See bug58606.
	 */
	rc = efx_mcdi_init_evq(enp, index, esmp, ndescs, irq, us, flags,
	    B_FALSE);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
rhead_ev_qdestroy(
	__in		efx_evq_t *eep)
{
	efx_nic_t *enp = eep->ee_enp;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_RIVERHEAD);

	(void) efx_mcdi_fini_evq(enp, eep->ee_index);
}

	__checkReturn	efx_rc_t
rhead_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t rptr;
	efx_dword_t dword;

	rptr = count & eep->ee_mask;

	EFX_POPULATE_DWORD_2(dword, ERF_GZ_EVQ_ID, eep->ee_index,
	    ERF_GZ_IDX, rptr);
	/* EVQ_INT_PRIME lives function control window only on Riverhead */
	EFX_BAR_WRITED(enp, ER_GZ_EVQ_INT_PRIME, &dword, B_FALSE);

	return (0);
}

			void
rhead_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data)
{
	_NOTE(ARGUNUSED(eep, data))

	/* Not implemented yet */
	EFSYS_ASSERT(B_FALSE);
}

/*
 * Poll event queue in batches. Size of the batch is equal to cache line
 * size divided by event size.
 *
 * Event queue is written by NIC and read by CPU. If CPU starts reading
 * of events on the cache line, read all remaining events in a tight
 * loop while event is present.
 */
#define	EF100_EV_BATCH	8

/*
 * Check if event is present.
 *
 * Riverhead EvQs use a phase bit to indicate the presence of valid events,
 * by flipping the phase bit on each wrap of the write index.
 */
#define	EF100_EV_PRESENT(_qword, _phase_bit)				\
	(EFX_QWORD_FIELD((_qword), ESF_GZ_EV_EVQ_PHASE) == _phase_bit)

			void
rhead_ev_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_qword_t ev[EF100_EV_BATCH];
	unsigned int batch;
	unsigned int phase_bit;
	unsigned int total;
	unsigned int count;
	unsigned int index;
	size_t offset;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);
	EFSYS_ASSERT(countp != NULL);
	EFSYS_ASSERT(eecp != NULL);

	count = *countp;
	do {
		/* Read up until the end of the batch period */
		batch = EF100_EV_BATCH - (count & (EF100_EV_BATCH - 1));
		phase_bit = (count & (eep->ee_mask + 1)) != 0;
		offset = (count & eep->ee_mask) * sizeof (efx_qword_t);
		for (total = 0; total < batch; ++total) {
			EFSYS_MEM_READQ(eep->ee_esmp, offset, &(ev[total]));

			if (!EF100_EV_PRESENT(ev[total], phase_bit))
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

			code = EFX_QWORD_FIELD(ev[index], ESF_GZ_E_TYPE);
			switch (code) {
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

				/*
				 * Poison batch to ensure the outer
				 * loop is broken out of.
				 */
				EFSYS_ASSERT(batch <= EF100_EV_BATCH);
				batch += (EF100_EV_BATCH << 1);
				EFSYS_ASSERT(total != batch);
				break;
			}
		}

		/*
		 * There is no necessity to clear processed events since
		 * phase bit which is flipping on each write index wrap
		 * is used for event presence indication.
		 */

		count += total;

	} while (total == batch);

	*countp = count;
}

	__checkReturn	efx_rc_t
rhead_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us)
{
	_NOTE(ARGUNUSED(eep, us))

	return (ENOTSUP);
}


#if EFSYS_OPT_QSTATS
			void
rhead_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat)
{
	unsigned int id;

	for (id = 0; id < EV_NQSTATS; id++) {
		efsys_stat_t *essp = &stat[id];

		EFSYS_STAT_INCR(essp, eep->ee_stat[id]);
		eep->ee_stat[id] = 0;
	}
}
#endif /* EFSYS_OPT_QSTATS */

#endif	/* EFSYS_OPT_RIVERHEAD */
