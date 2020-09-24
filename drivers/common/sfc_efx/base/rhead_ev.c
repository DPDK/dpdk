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

static			boolean_t
rhead_ev_dispatch(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
rhead_ev_rx_packets(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
rhead_ev_tx_completion(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static	__checkReturn	boolean_t
rhead_ev_mcdi(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

#if EFSYS_OPT_EV_EXTENDED_WIDTH
static			boolean_t
rhead_ev_ew_dispatch(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static			void
rhead_ev_ew_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

#if EFSYS_OPT_DESC_PROXY
static			boolean_t
rhead_ev_ew_txq_desc(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

static			boolean_t
rhead_ev_ew_virtq_desc(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);
#endif /* EFSYS_OPT_DESC_PROXY */
#endif /* EFSYS_OPT_EV_EXTENDED_WIDTH */


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
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	size_t desc_size;
	uint32_t irq;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(id))	/* buftbl id managed by MC */

	desc_size = encp->enc_ev_desc_size;
#if EFSYS_OPT_EV_EXTENDED_WIDTH
	if (flags & EFX_EVQ_FLAGS_EXTENDED_WIDTH)
		desc_size = encp->enc_ev_ew_desc_size;
#endif
	EFSYS_ASSERT(desc_size != 0);

	if (EFSYS_MEM_SIZE(esmp) < (ndescs * desc_size)) {
		/* Buffer too small for event queue descriptors */
		rc = EINVAL;
		goto fail1;
	}

	/* Set up the handler table */
	eep->ee_rx	= rhead_ev_rx_packets;
	eep->ee_tx	= rhead_ev_tx_completion;
	eep->ee_driver	= NULL; /* FIXME */
	eep->ee_drv_gen	= NULL; /* FIXME */
	eep->ee_mcdi	= rhead_ev_mcdi;

#if EFSYS_OPT_DESC_PROXY
	eep->ee_ew_txq_desc	= rhead_ev_ew_txq_desc;
	eep->ee_ew_virtq_desc	= rhead_ev_ew_virtq_desc;
#endif /* EFSYS_OPT_DESC_PROXY */

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
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
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
	EFX_BAR_FCW_WRITED(enp, ER_GZ_EVQ_INT_PRIME, &dword);

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

static	__checkReturn	boolean_t
rhead_ev_dispatch(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	boolean_t should_abort;
	uint32_t code;

	code = EFX_QWORD_FIELD(*eventp, ESF_GZ_E_TYPE);
	switch (code) {
	case ESE_GZ_EF100_EV_RX_PKTS:
		should_abort = eep->ee_rx(eep, eventp, eecp, arg);
		break;
	case ESE_GZ_EF100_EV_TX_COMPLETION:
		should_abort = eep->ee_tx(eep, eventp, eecp, arg);
		break;
	case ESE_GZ_EF100_EV_MCDI:
		should_abort = eep->ee_mcdi(eep, eventp, eecp, arg);
		break;
	default:
		EFSYS_PROBE3(bad_event, unsigned int, eep->ee_index,
		    uint32_t, EFX_QWORD_FIELD(*eventp, EFX_DWORD_1),
		    uint32_t, EFX_QWORD_FIELD(*eventp, EFX_DWORD_0));

		EFSYS_ASSERT(eecp->eec_exception != NULL);
		(void) eecp->eec_exception(arg, EFX_EXCEPTION_EV_ERROR, code);
		should_abort = B_TRUE;
		break;
	}

	return (should_abort);
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

#if EFSYS_OPT_EV_EXTENDED_WIDTH
	if (eep->ee_flags & EFX_EVQ_FLAGS_EXTENDED_WIDTH) {
		rhead_ev_ew_qpoll(eep, countp, eecp, arg);
		return;
	}
#endif /* EFSYS_OPT_EV_EXTENDED_WIDTH */

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

			EFX_EV_QSTAT_INCR(eep, EV_ALL);

			should_abort =
			    rhead_ev_dispatch(eep, &(ev[index]), eecp, arg);

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

#if EFSYS_OPT_EV_EXTENDED_WIDTH
static			boolean_t
rhead_ev_ew_dispatch(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	boolean_t should_abort;
	uint32_t code;

	EFSYS_ASSERT((eep->ee_flags & EFX_EVQ_FLAGS_EXTENDED_WIDTH) != 0);

	code = EFX_XWORD_FIELD(*eventp, ESF_GZ_EV_256_EV32_TYPE);
	switch (code) {
	case ESE_GZ_EF100_EVEW_64BIT:
		/* NOTE: ignore phase bit in encapsulated 64bit event. */
		should_abort =
		    rhead_ev_dispatch(eep, &eventp->ex_qword[0], eecp, arg);
		break;

#if EFSYS_OPT_DESC_PROXY
	case ESE_GZ_EF100_EVEW_TXQ_DESC:
		should_abort = eep->ee_ew_txq_desc(eep, eventp, eecp, arg);
		break;

	case ESE_GZ_EF100_EVEW_VIRTQ_DESC:
		should_abort = eep->ee_ew_virtq_desc(eep, eventp, eecp, arg);
		break;
#endif /* EFSYS_OPT_DESC_PROXY */

	default:
		/* Omit currently unused reserved bits from the probe. */
		EFSYS_PROBE7(ew_bad_event, unsigned int, eep->ee_index,
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_7),
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_4),
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_3),
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_2),
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_1),
		    uint32_t, EFX_XWORD_FIELD(*eventp, EFX_DWORD_0));

		EFSYS_ASSERT(eecp->eec_exception != NULL);
		(void) eecp->eec_exception(arg, EFX_EXCEPTION_EV_ERROR, code);
		should_abort = B_TRUE;
	}

	return (should_abort);
}

/*
 * Poll extended width event queue. Size of the batch is equal to cache line
 * size divided by event size.
 */
#define	EF100_EV_EW_BATCH	2

/*
 * Check if event is present.
 *
 * Riverhead EvQs use a phase bit to indicate the presence of valid events,
 * by flipping the phase bit on each wrap of the write index.
 */
#define	EF100_EV_EW_PRESENT(_xword, _phase_bit)				\
	(EFX_XWORD_FIELD((_xword), ESF_GZ_EV_256_EV32_PHASE) == (_phase_bit))

static			void
rhead_ev_ew_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_xword_t ev[EF100_EV_EW_BATCH];
	unsigned int batch;
	unsigned int phase_bit;
	unsigned int total;
	unsigned int count;
	unsigned int index;
	size_t offset;

	EFSYS_ASSERT3U(eep->ee_magic, ==, EFX_EVQ_MAGIC);
	EFSYS_ASSERT((eep->ee_flags & EFX_EVQ_FLAGS_EXTENDED_WIDTH) != 0);
	EFSYS_ASSERT(countp != NULL);
	EFSYS_ASSERT(eecp != NULL);

	count = *countp;
	do {
		/* Read up until the end of the batch period */
		batch = EF100_EV_EW_BATCH - (count & (EF100_EV_EW_BATCH - 1));
		phase_bit = (count & (eep->ee_mask + 1)) != 0;
		offset = (count & eep->ee_mask) * sizeof (efx_xword_t);
		for (total = 0; total < batch; ++total) {
			EFSYS_MEM_READX(eep->ee_esmp, offset, &(ev[total]));

			if (!EF100_EV_EW_PRESENT(ev[total], phase_bit))
				break;

			/* Omit unused reserved bits from the probe. */
			EFSYS_PROBE7(ew_event, unsigned int, eep->ee_index,
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_7),
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_4),
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_3),
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_2),
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_1),
			    uint32_t, EFX_XWORD_FIELD(ev[total], EFX_DWORD_0));

			offset += sizeof (efx_xword_t);
		}

		/* Process the batch of events */
		for (index = 0; index < total; ++index) {
			boolean_t should_abort;

			EFX_EV_QSTAT_INCR(eep, EV_ALL);

			should_abort =
			    rhead_ev_ew_dispatch(eep, &(ev[index]), eecp, arg);

			if (should_abort) {
				/* Ignore subsequent events */
				total = index + 1;

				/*
				 * Poison batch to ensure the outer
				 * loop is broken out of.
				 */
				EFSYS_ASSERT(batch <= EF100_EV_EW_BATCH);
				batch += (EF100_EV_EW_BATCH << 1);
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
#endif /* EFSYS_OPT_EV_EXTENDED_WIDTH */


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

static	__checkReturn	boolean_t
rhead_ev_rx_packets(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t label;
	uint32_t num_packets;
	boolean_t should_abort;

	EFX_EV_QSTAT_INCR(eep, EV_RX);

	/* Discard events after RXQ/TXQ errors, or hardware not available */
	if (enp->en_reset_flags &
	    (EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR | EFX_RESET_HW_UNAVAIL))
		return (B_FALSE);

	label = EFX_QWORD_FIELD(*eqp, ESF_GZ_EV_RXPKTS_Q_LABEL);

	/*
	 * On EF100 the EV_RX event reports the number of received
	 * packets (unlike EF10 which reports a descriptor index).
	 * The client driver is responsible for maintaining the Rx
	 * descriptor index, and computing how many descriptors are
	 * occupied by each received packet (based on the Rx buffer size
	 * and the packet length from the Rx prefix).
	 */
	num_packets = EFX_QWORD_FIELD(*eqp, ESF_GZ_EV_RXPKTS_NUM_PKT);

	/*
	 * The receive event may indicate more than one packet, and so
	 * does not contain the packet length. Read the packet length
	 * from the prefix when handling each packet.
	 */
	EFSYS_ASSERT(eecp->eec_rx_packets != NULL);
	should_abort = eecp->eec_rx_packets(arg, label, num_packets,
	    EFX_PKT_PREFIX_LEN);

	return (should_abort);
}

static	__checkReturn	boolean_t
rhead_ev_tx_completion(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_nic_t *enp = eep->ee_enp;
	uint32_t num_descs;
	uint32_t label;
	boolean_t should_abort;

	EFX_EV_QSTAT_INCR(eep, EV_TX);

	/* Discard events after RXQ/TXQ errors, or hardware not available */
	if (enp->en_reset_flags &
	    (EFX_RESET_RXQ_ERR | EFX_RESET_TXQ_ERR | EFX_RESET_HW_UNAVAIL))
		return (B_FALSE);

	label = EFX_QWORD_FIELD(*eqp, ESF_GZ_EV_TXCMPL_Q_LABEL);

	/*
	 * On EF100 the EV_TX event reports the number of completed Tx
	 * descriptors (on EF10, the event reports the low bits of the
	 * index of the last completed descriptor).
	 * The client driver completion callback will compute the
	 * descriptor index, so that is not needed here.
	 */
	num_descs = EFX_QWORD_FIELD(*eqp, ESF_GZ_EV_TXCMPL_NUM_DESC);

	EFSYS_PROBE2(tx_ndescs, uint32_t, label, unsigned int, num_descs);

	EFSYS_ASSERT(eecp->eec_tx_ndescs != NULL);
	should_abort = eecp->eec_tx_ndescs(arg, label, num_descs);

	return (should_abort);
}

static	__checkReturn	boolean_t
rhead_ev_mcdi(
	__in		efx_evq_t *eep,
	__in		efx_qword_t *eqp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	boolean_t ret;

	/*
	 * Event format was changed post Riverhead R1 and now
	 * MCDI event layout on EF100 is exactly the same as on EF10
	 * except added QDMA phase bit which is unused on EF10.
	 */
	ret = ef10_ev_mcdi(eep, eqp, eecp, arg);

	return (ret);
}

#if EFSYS_OPT_DESC_PROXY
static			boolean_t
rhead_ev_ew_txq_desc(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_oword_t txq_desc;
	uint16_t vi_id;
	boolean_t should_abort;

	_NOTE(ARGUNUSED(eep))

	vi_id = EFX_XWORD_FIELD(*eventp, ESF_GZ_EV_TXQ_DP_VI_ID);

	/*
	 * NOTE: This is the raw descriptor data, and has not been converted
	 * to host endian. The handler callback must use the EFX_OWORD macros
	 * to extract the descriptor fields as host endian values.
	 */
	txq_desc = eventp->ex_oword[0];

	EFSYS_ASSERT(eecp->eec_desc_proxy_txq_desc != NULL);
	should_abort = eecp->eec_desc_proxy_txq_desc(arg, vi_id, txq_desc);

	return (should_abort);
}
#endif /* EFSYS_OPT_DESC_PROXY */


#if EFSYS_OPT_DESC_PROXY
static			boolean_t
rhead_ev_ew_virtq_desc(
	__in		efx_evq_t *eep,
	__in		efx_xword_t *eventp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg)
{
	efx_oword_t virtq_desc;
	uint16_t vi_id;
	uint16_t avail;
	boolean_t should_abort;

	_NOTE(ARGUNUSED(eep))

	vi_id = EFX_XWORD_FIELD(*eventp, ESF_GZ_EV_VQ_DP_VI_ID);
	avail = EFX_XWORD_FIELD(*eventp, ESF_GZ_EV_VQ_DP_AVAIL_ENTRY);

	/*
	 * NOTE: This is the raw descriptor data, and has not been converted
	 * to host endian. The handler callback must use the EFX_OWORD macros
	 * to extract the descriptor fields as host endian values.
	 */
	virtq_desc = eventp->ex_oword[0];

	EFSYS_ASSERT(eecp->eec_desc_proxy_virtq_desc != NULL);
	should_abort =
	    eecp->eec_desc_proxy_virtq_desc(arg, vi_id, avail, virtq_desc);

	return (should_abort);
}
#endif /* EFSYS_OPT_DESC_PROXY */

#endif	/* EFSYS_OPT_RIVERHEAD */
