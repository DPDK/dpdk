/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD

	__checkReturn	efx_rc_t
rhead_tx_init(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	/* Nothing to do here */
	return (0);
}

			void
rhead_tx_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	/* Nothing to do here */
}

	__checkReturn	efx_rc_t
rhead_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__in		efx_txq_t *etp,
	__out		unsigned int *addedp)
{
	efx_rc_t rc;

	/*
	 * NMC manages the NMMU entries, and so buffer table IDs are
	 * ignored here
	 */
	_NOTE(ARGUNUSED(id))

	if ((rc = efx_mcdi_init_txq(enp, ndescs, eep->ee_index, label, index,
	    flags, esmp)) != 0)
		goto fail1;

	/*
	 * Return the initial queue index which is zero since no option
	 * descriptors are sent at start of day.
	 */
	*addedp = 0;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
rhead_tx_qdestroy(
	__in	efx_txq_t *etp)
{
	_NOTE(ARGUNUSED(etp))
	/* Nothing to do here */
}

	__checkReturn		efx_rc_t
rhead_tx_qpost(
	__in			efx_txq_t *etp,
	__in_ecount(ndescs)	efx_buffer_t *eb,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__inout			unsigned int *addedp)
{
	_NOTE(ARGUNUSED(etp))
	_NOTE(ARGUNUSED(eb))
	_NOTE(ARGUNUSED(ndescs))
	_NOTE(ARGUNUSED(completed))
	_NOTE(ARGUNUSED(addedp))

	/* FIXME Implement the method for Riverhead */

	return (ENOTSUP);
}

			void
rhead_tx_qpush(
	__in		efx_txq_t *etp,
	__in		unsigned int added,
	__in		unsigned int pushed)
{
	_NOTE(ARGUNUSED(etp, added, pushed))

	/* FIXME Implement the method for Riverhead */
	EFSYS_ASSERT(B_FALSE);
}

	__checkReturn	efx_rc_t
rhead_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns)
{
	_NOTE(ARGUNUSED(etp))
	_NOTE(ARGUNUSED(ns))

	/* FIXME Implement the method for Riverhead */

	return (ENOTSUP);
}

	__checkReturn	efx_rc_t
rhead_tx_qflush(
	__in		efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	efx_rc_t rc;

	if ((rc = efx_mcdi_fini_txq(enp, etp->et_index)) != 0)
		goto fail1;

	return (0);

fail1:
	/*
	 * EALREADY is not an error, but indicates that the MC has rebooted and
	 * that the TXQ has already been destroyed. Callers need to know that
	 * the TXQ flush has completed to avoid waiting until timeout for a
	 * flush done event that will not be delivered.
	 */
	if (rc != EALREADY)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
rhead_tx_qenable(
	__in		efx_txq_t *etp)
{
	_NOTE(ARGUNUSED(etp))
	/* Nothing to do here */
}

	__checkReturn		efx_rc_t
rhead_tx_qdesc_post(
	__in			efx_txq_t *etp,
	__in_ecount(ndescs)	efx_desc_t *ed,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__inout			unsigned int *addedp)
{
	_NOTE(ARGUNUSED(etp))
	_NOTE(ARGUNUSED(ed))
	_NOTE(ARGUNUSED(ndescs))
	_NOTE(ARGUNUSED(completed))
	_NOTE(ARGUNUSED(addedp))

	/* FIXME Implement the method for Riverhead */

	return (ENOTSUP);
}

#if EFSYS_OPT_QSTATS

			void
rhead_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat)
{
	unsigned int id;

	for (id = 0; id < TX_NQSTATS; id++) {
		efsys_stat_t *essp = &stat[id];

		EFSYS_STAT_INCR(essp, etp->et_stat[id]);
		etp->et_stat[id] = 0;
	}
}

#endif /* EFSYS_OPT_QSTATS */

#endif /* EFSYS_OPT_RIVERHEAD */
