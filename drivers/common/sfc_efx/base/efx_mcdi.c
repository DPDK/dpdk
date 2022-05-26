/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2008-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MCDI

/*
 * There are three versions of the MCDI interface:
 *  - MCDIv0: Siena BootROM. Transport uses MCDIv1 headers.
 *  - MCDIv1: Siena firmware and Huntington BootROM.
 *  - MCDIv2: EF10 firmware (Huntington/Medford) and Medford BootROM.
 *            Transport uses MCDIv2 headers.
 *
 * MCDIv2 Header NOT_EPOCH flag
 * ----------------------------
 * A new epoch begins at initial startup or after an MC reboot, and defines when
 * the MC should reject stale MCDI requests.
 *
 * The first MCDI request sent by the host should contain NOT_EPOCH=0, and all
 * subsequent requests (until the next MC reboot) should contain NOT_EPOCH=1.
 *
 * After rebooting the MC will fail all requests with NOT_EPOCH=1 by writing a
 * response with ERROR=1 and DATALEN=0 until a request is seen with NOT_EPOCH=0.
 */



#if EFSYS_OPT_SIENA

static const efx_mcdi_ops_t	__efx_mcdi_siena_ops = {
	siena_mcdi_init,		/* emco_init */
	siena_mcdi_send_request,	/* emco_send_request */
	siena_mcdi_poll_reboot,		/* emco_poll_reboot */
	siena_mcdi_poll_response,	/* emco_poll_response */
	siena_mcdi_read_response,	/* emco_read_response */
	siena_mcdi_fini,		/* emco_fini */
	siena_mcdi_feature_supported,	/* emco_feature_supported */
	siena_mcdi_get_timeout,		/* emco_get_timeout */
};

#endif	/* EFSYS_OPT_SIENA */

#if EFX_OPTS_EF10()

static const efx_mcdi_ops_t	__efx_mcdi_ef10_ops = {
	ef10_mcdi_init,			/* emco_init */
	ef10_mcdi_send_request,		/* emco_send_request */
	ef10_mcdi_poll_reboot,		/* emco_poll_reboot */
	ef10_mcdi_poll_response,	/* emco_poll_response */
	ef10_mcdi_read_response,	/* emco_read_response */
	ef10_mcdi_fini,			/* emco_fini */
	ef10_mcdi_feature_supported,	/* emco_feature_supported */
	ef10_mcdi_get_timeout,		/* emco_get_timeout */
};

#endif	/* EFX_OPTS_EF10() */

#if EFSYS_OPT_RIVERHEAD

static const efx_mcdi_ops_t	__efx_mcdi_rhead_ops = {
	ef10_mcdi_init,			/* emco_init */
	ef10_mcdi_send_request,		/* emco_send_request */
	ef10_mcdi_poll_reboot,		/* emco_poll_reboot */
	ef10_mcdi_poll_response,	/* emco_poll_response */
	ef10_mcdi_read_response,	/* emco_read_response */
	ef10_mcdi_fini,			/* emco_fini */
	ef10_mcdi_feature_supported,	/* emco_feature_supported */
	ef10_mcdi_get_timeout,		/* emco_get_timeout */
};

#endif	/* EFSYS_OPT_RIVERHEAD */



	__checkReturn	efx_rc_t
efx_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *emtp)
{
	const efx_mcdi_ops_t *emcop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, ==, 0);

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		emcop = &__efx_mcdi_siena_ops;
		break;
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		emcop = &__efx_mcdi_ef10_ops;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		emcop = &__efx_mcdi_ef10_ops;
		break;
#endif	/* EFSYS_OPT_MEDFORD */

#if EFSYS_OPT_MEDFORD2
	case EFX_FAMILY_MEDFORD2:
		emcop = &__efx_mcdi_ef10_ops;
		break;
#endif	/* EFSYS_OPT_MEDFORD2 */

#if EFSYS_OPT_RIVERHEAD
	case EFX_FAMILY_RIVERHEAD:
		emcop = &__efx_mcdi_rhead_ops;
		break;
#endif	/* EFSYS_OPT_RIVERHEAD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	if (enp->en_features & EFX_FEATURE_MCDI_DMA) {
		/* MCDI requires a DMA buffer in host memory */
		if ((emtp == NULL) || (emtp->emt_dma_mem) == NULL) {
			rc = EINVAL;
			goto fail2;
		}
	}
	enp->en_mcdi.em_emtp = emtp;

	if (emcop != NULL && emcop->emco_init != NULL) {
		if ((rc = emcop->emco_init(enp, emtp)) != 0)
			goto fail3;
	}

	enp->en_mcdi.em_emcop = emcop;
	enp->en_mod_flags |= EFX_MOD_MCDI;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_mcdi.em_emcop = NULL;
	enp->en_mcdi.em_emtp = NULL;
	enp->en_mod_flags &= ~EFX_MOD_MCDI;

	return (rc);
}

			void
efx_mcdi_fini(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, ==, EFX_MOD_MCDI);

	if (emcop != NULL && emcop->emco_fini != NULL)
		emcop->emco_fini(enp);

	emip->emi_port = 0;
	emip->emi_aborted = 0;

	enp->en_mcdi.em_emcop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_MCDI;
}

			void
efx_mcdi_new_epoch(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efsys_lock_state_t state;

	/* Start a new epoch (allow fresh MCDI requests to succeed) */
	EFSYS_LOCK(enp->en_eslp, state);
	emip->emi_new_epoch = B_TRUE;
	EFSYS_UNLOCK(enp->en_eslp, state);
}

static			void
efx_mcdi_send_request(
	__in		efx_nic_t *enp,
	__in		void *hdrp,
	__in		size_t hdr_len,
	__in		void *sdup,
	__in		size_t sdu_len)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;

	emcop->emco_send_request(enp, hdrp, hdr_len, sdup, sdu_len);
}

static			efx_rc_t
efx_mcdi_poll_reboot(
	__in		efx_nic_t *enp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	efx_rc_t rc;

	rc = emcop->emco_poll_reboot(enp);
	return (rc);
}

static			boolean_t
efx_mcdi_poll_response(
	__in		efx_nic_t *enp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	boolean_t available;

	available = emcop->emco_poll_response(enp);
	return (available);
}

static			void
efx_mcdi_read_response(
	__in		efx_nic_t *enp,
	__out		void *bufferp,
	__in		size_t offset,
	__in		size_t length)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;

	emcop->emco_read_response(enp, bufferp, offset, length);
}

			void
efx_mcdi_request_start(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		boolean_t ev_cpl)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t hdr[2];
	size_t hdr_len;
	unsigned int max_version;
	unsigned int seq;
	unsigned int xflags;
	boolean_t new_epoch;
	efsys_lock_state_t state;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/*
	 * efx_mcdi_request_start() is naturally serialised against both
	 * efx_mcdi_request_poll() and efx_mcdi_ev_cpl()/efx_mcdi_ev_death(),
	 * by virtue of there only being one outstanding MCDI request.
	 * Unfortunately, upper layers may also call efx_mcdi_request_abort()
	 * at any time, to timeout a pending mcdi request, That request may
	 * then subsequently complete, meaning efx_mcdi_ev_cpl() or
	 * efx_mcdi_ev_death() may end up running in parallel with
	 * efx_mcdi_request_start(). This race is handled by ensuring that
	 * %emi_pending_req, %emi_ev_cpl and %emi_seq are protected by the
	 * en_eslp lock.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	EFSYS_ASSERT(emip->emi_pending_req == NULL);
	emip->emi_pending_req = emrp;
	emip->emi_ev_cpl = ev_cpl;
	emip->emi_poll_cnt = 0;
	seq = emip->emi_seq++ & EFX_MASK32(MCDI_HEADER_SEQ);
	new_epoch = emip->emi_new_epoch;
	max_version = emip->emi_max_version;
	EFSYS_UNLOCK(enp->en_eslp, state);

	xflags = 0;
	if (ev_cpl)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	/*
	 * Huntington firmware supports MCDIv2, but the Huntington BootROM only
	 * supports MCDIv1. Use MCDIv1 headers for MCDIv1 commands where
	 * possible to support this.
	 */
	if ((max_version >= 2) &&
	    ((emrp->emr_cmd > MC_CMD_CMD_SPACE_ESCAPE_7) ||
	    (emrp->emr_in_length > MCDI_CTL_SDU_LEN_MAX_V1) ||
	    (emrp->emr_out_length > MCDI_CTL_SDU_LEN_MAX_V1))) {
		/* Construct MCDI v2 header */
		hdr_len = sizeof (hdr);
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, MC_CMD_V2_EXTN,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, 0,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);

		EFX_POPULATE_DWORD_2(hdr[1],
		    MC_CMD_V2_EXTN_IN_EXTENDED_CMD, emrp->emr_cmd,
		    MC_CMD_V2_EXTN_IN_ACTUAL_LEN, emrp->emr_in_length);
	} else {
		/* Construct MCDI v1 header */
		hdr_len = sizeof (hdr[0]);
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, emrp->emr_cmd,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, emrp->emr_in_length,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);
	}

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context, EFX_LOG_MCDI_REQUEST,
		    &hdr[0], hdr_len,
		    emrp->emr_in_buf, emrp->emr_in_length);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */

	efx_mcdi_send_request(enp, &hdr[0], hdr_len,
	    emrp->emr_in_buf, emrp->emr_in_length);
}


static			void
efx_mcdi_read_response_header(
	__in		efx_nic_t *enp,
	__inout		efx_mcdi_req_t *emrp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t hdr[2];
	unsigned int hdr_len;
	unsigned int data_len;
	unsigned int seq;
	unsigned int cmd;
	unsigned int error;
	efx_rc_t rc;

	EFSYS_ASSERT(emrp != NULL);

	efx_mcdi_read_response(enp, &hdr[0], 0, sizeof (hdr[0]));
	hdr_len = sizeof (hdr[0]);

	cmd = EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_CODE);
	seq = EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_SEQ);
	error = EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_ERROR);

	if (cmd != MC_CMD_V2_EXTN) {
		data_len = EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_DATALEN);
	} else {
		efx_mcdi_read_response(enp, &hdr[1], hdr_len, sizeof (hdr[1]));
		hdr_len += sizeof (hdr[1]);

		cmd = EFX_DWORD_FIELD(hdr[1], MC_CMD_V2_EXTN_IN_EXTENDED_CMD);
		data_len =
		    EFX_DWORD_FIELD(hdr[1], MC_CMD_V2_EXTN_IN_ACTUAL_LEN);
	}

	if (error && (data_len == 0)) {
		/* The MC has rebooted since the request was sent. */
		EFSYS_SPIN(EFX_MCDI_STATUS_SLEEP_US);
		efx_mcdi_poll_reboot(enp);
		rc = EIO;
		goto fail1;
	}
#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
	if (((cmd != emrp->emr_cmd) && (emrp->emr_cmd != MC_CMD_PROXY_CMD)) ||
#else
	if ((cmd != emrp->emr_cmd) ||
#endif
	    (seq != ((emip->emi_seq - 1) & EFX_MASK32(MCDI_HEADER_SEQ)))) {
		/* Response is for a different request */
		rc = EIO;
		goto fail2;
	}
	if (error) {
		efx_dword_t err[2];
		unsigned int err_len = MIN(data_len, sizeof (err));
		int err_code = MC_CMD_ERR_EPROTO;
		int err_arg = 0;

		/* Read error code (and arg num for MCDI v2 commands) */
		efx_mcdi_read_response(enp, &err, hdr_len, err_len);

		if (err_len >= (MC_CMD_ERR_CODE_OFST + sizeof (efx_dword_t)))
			err_code = EFX_DWORD_FIELD(err[0], EFX_DWORD_0);
#ifdef WITH_MCDI_V2
		if (err_len >= (MC_CMD_ERR_ARG_OFST + sizeof (efx_dword_t)))
			err_arg = EFX_DWORD_FIELD(err[1], EFX_DWORD_0);
#endif
		emrp->emr_err_code = err_code;
		emrp->emr_err_arg = err_arg;

#if EFSYS_OPT_MCDI_PROXY_AUTH
		if ((err_code == MC_CMD_ERR_PROXY_PENDING) &&
		    (err_len == sizeof (err))) {
			/*
			 * The MCDI request would normally fail with EPERM, but
			 * firmware has forwarded it to an authorization agent
			 * attached to a privileged PF.
			 *
			 * Save the authorization request handle. The client
			 * must wait for a PROXY_RESPONSE event, or timeout.
			 */
			emrp->emr_proxy_handle = err_arg;
		}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */

#if EFSYS_OPT_MCDI_LOGGING
		if (emtp->emt_logger != NULL) {
			emtp->emt_logger(emtp->emt_context,
			    EFX_LOG_MCDI_RESPONSE,
			    &hdr[0], hdr_len,
			    &err[0], err_len);
		}
#endif /* EFSYS_OPT_MCDI_LOGGING */

		if (!emrp->emr_quiet) {
			EFSYS_PROBE3(mcdi_err_arg, int, emrp->emr_cmd,
			    int, err_code, int, err_arg);
		}

		rc = efx_mcdi_request_errcode(err_code);
		goto fail3;
	}

	emrp->emr_rc = 0;
	emrp->emr_out_length_used = data_len;
#if EFSYS_OPT_MCDI_PROXY_AUTH
	emrp->emr_proxy_handle = 0;
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */
	return;

fail3:
fail2:
fail1:
	emrp->emr_rc = rc;
	emrp->emr_out_length_used = 0;
}

static			void
efx_mcdi_finish_response(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_dword_t hdr[2];
	unsigned int hdr_len;
	size_t bytes;
	unsigned int resp_off;
#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
	unsigned int resp_cmd;
	boolean_t proxied_cmd_resp = B_FALSE;
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */

	if (emrp->emr_out_buf == NULL)
		return;

	/* Read the command header to detect MCDI response format */
	hdr_len = sizeof (hdr[0]);
	efx_mcdi_read_response(enp, &hdr[0], 0, hdr_len);
	if (EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_CODE) == MC_CMD_V2_EXTN) {
		/*
		 * Read the actual payload length. The length given in the event
		 * is only correct for responses with the V1 format.
		 */
		efx_mcdi_read_response(enp, &hdr[1], hdr_len, sizeof (hdr[1]));
		hdr_len += sizeof (hdr[1]);
		resp_off = hdr_len;

		emrp->emr_out_length_used = EFX_DWORD_FIELD(hdr[1],
						MC_CMD_V2_EXTN_IN_ACTUAL_LEN);
#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
		/*
		 * A proxy MCDI command is executed by PF on behalf of
		 * one of its VFs. The command to be proxied follows
		 * immediately afterward in the host buffer.
		 * PROXY_CMD inner call complete response should be copied to
		 * output buffer so that it can be returned to the requesting
		 * function in MC_CMD_PROXY_COMPLETE payload.
		 */
		resp_cmd =
			EFX_DWORD_FIELD(hdr[1], MC_CMD_V2_EXTN_IN_EXTENDED_CMD);
		proxied_cmd_resp = ((emrp->emr_cmd == MC_CMD_PROXY_CMD) &&
					(resp_cmd != MC_CMD_PROXY_CMD));
		if (proxied_cmd_resp) {
			resp_off = 0;
			emrp->emr_out_length_used += hdr_len;
		}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
	} else {
		resp_off = hdr_len;
	}

	/* Copy payload out into caller supplied buffer */
	bytes = MIN(emrp->emr_out_length_used, emrp->emr_out_length);
	efx_mcdi_read_response(enp, emrp->emr_out_buf, resp_off, bytes);

	/* Report bytes copied to caller (response message may be larger) */
	emrp->emr_out_length_used = bytes;

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context,
		    EFX_LOG_MCDI_RESPONSE,
		    &hdr[0], hdr_len,
		    emrp->emr_out_buf, bytes);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */
}


	__checkReturn	boolean_t
efx_mcdi_request_poll(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_mcdi_req_t *emrp;
	efsys_lock_state_t state;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/* Serialise against post-watchdog efx_mcdi_ev* */
	EFSYS_LOCK(enp->en_eslp, state);

	EFSYS_ASSERT(emip->emi_pending_req != NULL);
	EFSYS_ASSERT(!emip->emi_ev_cpl);
	emrp = emip->emi_pending_req;

	/* Check if hardware is unavailable */
	if (efx_nic_hw_unavailable(enp)) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		return (B_FALSE);
	}

	/* Check for reboot atomically w.r.t efx_mcdi_request_start */
	if (emip->emi_poll_cnt++ == 0) {
		if ((rc = efx_mcdi_poll_reboot(enp)) != 0) {
			emip->emi_pending_req = NULL;
			EFSYS_UNLOCK(enp->en_eslp, state);

			/* Reboot/Assertion */
			if (rc == EIO || rc == EINTR)
				efx_mcdi_raise_exception(enp, emrp, rc);

			goto fail1;
		}
	}

	/* Check if a response is available */
	if (efx_mcdi_poll_response(enp) == B_FALSE) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		return (B_FALSE);
	}

	/* Read the response header */
	efx_mcdi_read_response_header(enp, emrp);

	/* Request complete */
	emip->emi_pending_req = NULL;

	/* Ensure stale MCDI requests fail after an MC reboot. */
	emip->emi_new_epoch = B_FALSE;

	EFSYS_UNLOCK(enp->en_eslp, state);

	if ((rc = emrp->emr_rc) != 0)
		goto fail2;

	efx_mcdi_finish_response(enp, emrp);
	return (B_TRUE);

fail2:
	if (!emrp->emr_quiet)
		EFSYS_PROBE(fail2);
fail1:
	if (!emrp->emr_quiet)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (B_TRUE);
}

	__checkReturn	boolean_t
efx_mcdi_request_abort(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_mcdi_req_t *emrp;
	boolean_t aborted;
	efsys_lock_state_t state;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/*
	 * efx_mcdi_ev_* may have already completed this event, and be
	 * spinning/blocked on the upper layer lock. So it *is* legitimate
	 * to for emi_pending_req to be NULL. If there is a pending event
	 * completed request, then provide a "credit" to allow
	 * efx_mcdi_ev_cpl() to accept a single spurious completion.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	emrp = emip->emi_pending_req;
	aborted = (emrp != NULL);
	if (aborted) {
		emip->emi_pending_req = NULL;

		/* Error the request */
		emrp->emr_out_length_used = 0;
		emrp->emr_rc = ETIMEDOUT;

		/* Provide a credit for seqno/emr_pending_req mismatches */
		if (emip->emi_ev_cpl)
			++emip->emi_aborted;

		/*
		 * The upper layer has called us, so we don't
		 * need to complete the request.
		 */
	}
	EFSYS_UNLOCK(enp->en_eslp, state);

	return (aborted);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_client_handle(
	__in		efx_nic_t *enp,
	__in		efx_pcie_interface_t intf,
	__in		uint16_t pf,
	__in		uint16_t vf,
	__out		uint32_t *handle)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_CLIENT_HANDLE_IN_LEN,
	    MC_CMD_GET_CLIENT_HANDLE_OUT_LEN);
	uint32_t pcie_intf;
	efx_rc_t rc;

	if (handle == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	rc = efx_mcdi_intf_to_pcie(intf, &pcie_intf);
	if (rc != 0)
		goto fail2;

	req.emr_cmd = MC_CMD_GET_CLIENT_HANDLE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_CLIENT_HANDLE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_CLIENT_HANDLE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, GET_CLIENT_HANDLE_IN_TYPE,
	    MC_CMD_GET_CLIENT_HANDLE_IN_TYPE_FUNC);
	MCDI_IN_SET_WORD(req, GET_CLIENT_HANDLE_IN_FUNC_PF, pf);
	MCDI_IN_SET_WORD(req, GET_CLIENT_HANDLE_IN_FUNC_VF, vf);
	MCDI_IN_SET_DWORD(req, GET_CLIENT_HANDLE_IN_FUNC_INTF, pcie_intf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	if (req.emr_out_length_used < MC_CMD_GET_CLIENT_HANDLE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail4;
	}

	*handle = MCDI_OUT_DWORD(req, GET_CLIENT_HANDLE_OUT_HANDLE);

	return 0;
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_own_client_handle(
	__in		efx_nic_t *enp,
	__out		uint32_t *handle)
{
	efx_rc_t rc;

	rc = efx_mcdi_get_client_handle(enp, EFX_PCIE_INTERFACE_CALLER,
	    PCIE_FUNCTION_PF_NULL, PCIE_FUNCTION_VF_NULL, handle);
	if (rc != 0)
		goto fail1;

	return (0);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

			void
efx_mcdi_get_timeout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__out		uint32_t *timeoutp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;

	emcop->emco_get_timeout(enp, emrp, timeoutp);
}

	__checkReturn	efx_rc_t
efx_mcdi_request_errcode(
	__in		unsigned int err)
{

	switch (err) {
		/* MCDI v1 */
	case MC_CMD_ERR_EPERM:
		return (EACCES);
	case MC_CMD_ERR_ENOENT:
		return (ENOENT);
	case MC_CMD_ERR_EINTR:
		return (EINTR);
	case MC_CMD_ERR_EACCES:
		return (EACCES);
	case MC_CMD_ERR_EBUSY:
		return (EBUSY);
	case MC_CMD_ERR_EINVAL:
		return (EINVAL);
	case MC_CMD_ERR_EDEADLK:
		return (EDEADLK);
	case MC_CMD_ERR_ENOSYS:
		return (ENOTSUP);
	case MC_CMD_ERR_ETIME:
		return (ETIMEDOUT);
	case MC_CMD_ERR_ENOTSUP:
		return (ENOTSUP);
	case MC_CMD_ERR_EALREADY:
		return (EALREADY);

		/* MCDI v2 */
	case MC_CMD_ERR_EEXIST:
		return (EEXIST);
#ifdef MC_CMD_ERR_EAGAIN
	case MC_CMD_ERR_EAGAIN:
		return (EAGAIN);
#endif
#ifdef MC_CMD_ERR_ENOSPC
	case MC_CMD_ERR_ENOSPC:
		return (ENOSPC);
#endif
	case MC_CMD_ERR_ERANGE:
		return (ERANGE);

	case MC_CMD_ERR_ALLOC_FAIL:
		return (ENOMEM);
	case MC_CMD_ERR_NO_VADAPTOR:
		return (ENOENT);
	case MC_CMD_ERR_NO_EVB_PORT:
		return (ENOENT);
	case MC_CMD_ERR_NO_VSWITCH:
		return (ENODEV);
	case MC_CMD_ERR_VLAN_LIMIT:
		return (EINVAL);
	case MC_CMD_ERR_BAD_PCI_FUNC:
		return (ENODEV);
	case MC_CMD_ERR_BAD_VLAN_MODE:
		return (EINVAL);
	case MC_CMD_ERR_BAD_VSWITCH_TYPE:
		return (EINVAL);
	case MC_CMD_ERR_BAD_VPORT_TYPE:
		return (EINVAL);
	case MC_CMD_ERR_MAC_EXIST:
		return (EEXIST);

	case MC_CMD_ERR_PROXY_PENDING:
		return (EAGAIN);

	default:
		EFSYS_PROBE1(mc_pcol_error, int, err);
		return (EIO);
	}
}

			void
efx_mcdi_raise_exception(
	__in		efx_nic_t *enp,
	__in_opt	efx_mcdi_req_t *emrp,
	__in		int rc)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efx_mcdi_exception_t exception;

	/* Reboot or Assertion failure only */
	EFSYS_ASSERT(rc == EIO || rc == EINTR);

	/*
	 * If MC_CMD_REBOOT causes a reboot (dependent on parameters),
	 * then the EIO is not worthy of an exception.
	 */
	if (emrp != NULL && emrp->emr_cmd == MC_CMD_REBOOT && rc == EIO)
		return;

	exception = (rc == EIO)
		? EFX_MCDI_EXCEPTION_MC_REBOOT
		: EFX_MCDI_EXCEPTION_MC_BADASSERT;

	emtp->emt_exception(emtp->emt_context, exception);
}

			void
efx_mcdi_execute(
	__in		efx_nic_t *enp,
	__inout		efx_mcdi_req_t *emrp)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	emrp->emr_quiet = B_FALSE;
	emtp->emt_execute(emtp->emt_context, emrp);
}

			void
efx_mcdi_execute_quiet(
	__in		efx_nic_t *enp,
	__inout		efx_mcdi_req_t *emrp)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	emrp->emr_quiet = B_TRUE;
	emtp->emt_execute(emtp->emt_context, emrp);
}

			void
efx_mcdi_ev_cpl(
	__in		efx_nic_t *enp,
	__in		unsigned int seq,
	__in		unsigned int outlen,
	__in		int errcode)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efx_mcdi_req_t *emrp;
	efsys_lock_state_t state;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	/*
	 * Serialise against efx_mcdi_request_poll()/efx_mcdi_request_start()
	 * when we're completing an aborted request.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	if (emip->emi_pending_req == NULL || !emip->emi_ev_cpl ||
	    (seq != ((emip->emi_seq - 1) & EFX_MASK32(MCDI_HEADER_SEQ)))) {
		EFSYS_ASSERT(emip->emi_aborted > 0);
		if (emip->emi_aborted > 0)
			--emip->emi_aborted;
		EFSYS_UNLOCK(enp->en_eslp, state);
		return;
	}

	emrp = emip->emi_pending_req;
	emip->emi_pending_req = NULL;
	EFSYS_UNLOCK(enp->en_eslp, state);

	if (emip->emi_max_version >= 2) {
		/* MCDIv2 response details do not fit into an event. */
		efx_mcdi_read_response_header(enp, emrp);
	} else {
		if (errcode != 0) {
			if (!emrp->emr_quiet) {
				EFSYS_PROBE2(mcdi_err, int, emrp->emr_cmd,
				    int, errcode);
			}
			emrp->emr_out_length_used = 0;
			emrp->emr_rc = efx_mcdi_request_errcode(errcode);
		} else {
			emrp->emr_out_length_used = outlen;
			emrp->emr_rc = 0;
		}
	}
	if (emrp->emr_rc == 0)
		efx_mcdi_finish_response(enp, emrp);

	emtp->emt_ev_cpl(emtp->emt_context);
}

#if EFSYS_OPT_MCDI_PROXY_AUTH

	__checkReturn	efx_rc_t
efx_mcdi_get_proxy_handle(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__out		uint32_t *handlep)
{
	efx_rc_t rc;

	_NOTE(ARGUNUSED(enp))

	/*
	 * Return proxy handle from MCDI request that returned with error
	 * MC_MCD_ERR_PROXY_PENDING. This handle is used to wait for a matching
	 * PROXY_RESPONSE event.
	 */
	if ((emrp == NULL) || (handlep == NULL)) {
		rc = EINVAL;
		goto fail1;
	}
	if ((emrp->emr_rc != 0) &&
	    (emrp->emr_err_code == MC_CMD_ERR_PROXY_PENDING)) {
		*handlep = emrp->emr_proxy_handle;
		rc = 0;
	} else {
		*handlep = 0;
		rc = ENOENT;
	}
	return (rc);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

			void
efx_mcdi_ev_proxy_response(
	__in		efx_nic_t *enp,
	__in		unsigned int handle,
	__in		unsigned int status)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efx_rc_t rc;

	/*
	 * Handle results of an authorization request for a privileged MCDI
	 * command. If authorization was granted then we must re-issue the
	 * original MCDI request. If authorization failed or timed out,
	 * then the original MCDI request should be completed with the
	 * result code from this event.
	 */
	rc = (status == 0) ? 0 : efx_mcdi_request_errcode(status);

	emtp->emt_ev_proxy_response(emtp->emt_context, handle, rc);
}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
			void
efx_mcdi_ev_proxy_request(
	__in		efx_nic_t *enp,
	__in		unsigned int index)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;

	if (emtp->emt_ev_proxy_request != NULL)
		emtp->emt_ev_proxy_request(emtp->emt_context, index);
}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
			void
efx_mcdi_ev_death(
	__in		efx_nic_t *enp,
	__in		int rc)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efx_mcdi_req_t *emrp = NULL;
	boolean_t ev_cpl;
	efsys_lock_state_t state;

	/*
	 * The MCDI request (if there is one) has been terminated, either
	 * by a BADASSERT or REBOOT event.
	 *
	 * If there is an outstanding event-completed MCDI operation, then we
	 * will never receive the completion event (because both MCDI
	 * completions and BADASSERT events are sent to the same evq). So
	 * complete this MCDI op.
	 *
	 * This function might run in parallel with efx_mcdi_request_poll()
	 * for poll completed mcdi requests, and also with
	 * efx_mcdi_request_start() for post-watchdog completions.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	emrp = emip->emi_pending_req;
	ev_cpl = emip->emi_ev_cpl;
	if (emrp != NULL && emip->emi_ev_cpl) {
		emip->emi_pending_req = NULL;

		emrp->emr_out_length_used = 0;
		emrp->emr_rc = rc;
		++emip->emi_aborted;
	}

	/*
	 * Since we're running in parallel with a request, consume the
	 * status word before dropping the lock.
	 */
	if (rc == EIO || rc == EINTR) {
		EFSYS_SPIN(EFX_MCDI_STATUS_SLEEP_US);
		(void) efx_mcdi_poll_reboot(enp);
		emip->emi_new_epoch = B_TRUE;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);

	efx_mcdi_raise_exception(enp, emrp, rc);

	if (emrp != NULL && ev_cpl)
		emtp->emt_ev_cpl(emtp->emt_context);
}

	__checkReturn		efx_rc_t
efx_mcdi_get_version(
	__in			efx_nic_t *enp,
	__in			uint32_t flags,
	__out			efx_mcdi_version_t *verp)
{
	efx_nic_board_info_t *board_infop = &verp->emv_board_info;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_VERSION_EXT_IN_LEN,
	    MC_CMD_GET_VERSION_V2_OUT_LEN);
	efx_word_t *ver_words;
	uint16_t version[4];
	efx_mcdi_req_t req;
	uint32_t firmware;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(sizeof (verp->emv_version) ==
	    MC_CMD_GET_VERSION_OUT_VERSION_LEN);
	EFX_STATIC_ASSERT(sizeof (verp->emv_firmware) ==
	    MC_CMD_GET_VERSION_OUT_FIRMWARE_LEN);

	EFX_STATIC_ASSERT(EFX_MCDI_VERSION_BOARD_INFO ==
	    (1U << MC_CMD_GET_VERSION_V2_OUT_BOARD_EXT_INFO_PRESENT_LBN));

	EFX_STATIC_ASSERT(sizeof (board_infop->enbi_serial) ==
	    MC_CMD_GET_VERSION_V2_OUT_BOARD_SERIAL_LEN);
	EFX_STATIC_ASSERT(sizeof (board_infop->enbi_name) ==
	    MC_CMD_GET_VERSION_V2_OUT_BOARD_NAME_LEN);
	EFX_STATIC_ASSERT(sizeof (board_infop->enbi_revision) ==
	    MC_CMD_GET_VERSION_V2_OUT_BOARD_REVISION_LEN);

	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	req.emr_cmd = MC_CMD_GET_VERSION;
	req.emr_in_buf = payload;
	req.emr_out_buf = payload;

	if ((flags & EFX_MCDI_VERSION_BOARD_INFO) != 0) {
		/* Request basic + extended version information. */
		req.emr_in_length = MC_CMD_GET_VERSION_EXT_IN_LEN;
		req.emr_out_length = MC_CMD_GET_VERSION_V2_OUT_LEN;
	} else {
		/* Request only basic version information. */
		req.emr_in_length = MC_CMD_GET_VERSION_IN_LEN;
		req.emr_out_length = MC_CMD_GET_VERSION_OUT_LEN;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	/* bootrom support */
	if (req.emr_out_length_used == MC_CMD_GET_VERSION_V0_OUT_LEN) {
		version[0] = version[1] = version[2] = version[3] = 0;
		firmware = MCDI_OUT_DWORD(req, GET_VERSION_OUT_FIRMWARE);
		goto out;
	}

	if (req.emr_out_length_used < req.emr_out_length) {
		rc = EMSGSIZE;
		goto fail2;
	}

	ver_words = MCDI_OUT2(req, efx_word_t, GET_VERSION_OUT_VERSION);
	version[0] = EFX_WORD_FIELD(ver_words[0], EFX_WORD_0);
	version[1] = EFX_WORD_FIELD(ver_words[1], EFX_WORD_0);
	version[2] = EFX_WORD_FIELD(ver_words[2], EFX_WORD_0);
	version[3] = EFX_WORD_FIELD(ver_words[3], EFX_WORD_0);
	firmware = MCDI_OUT_DWORD(req, GET_VERSION_OUT_FIRMWARE);

out:
	memset(verp, 0, sizeof (*verp));

	verp->emv_version[0] = version[0];
	verp->emv_version[1] = version[1];
	verp->emv_version[2] = version[2];
	verp->emv_version[3] = version[3];
	verp->emv_firmware = firmware;

	verp->emv_flags = MCDI_OUT_DWORD(req, GET_VERSION_V2_OUT_FLAGS);
	verp->emv_flags &= flags;

	if ((verp->emv_flags & EFX_MCDI_VERSION_BOARD_INFO) != 0) {
		memcpy(board_infop->enbi_serial,
		    MCDI_OUT2(req, char, GET_VERSION_V2_OUT_BOARD_SERIAL),
		    sizeof (board_infop->enbi_serial));
		memcpy(board_infop->enbi_name,
		    MCDI_OUT2(req, char, GET_VERSION_V2_OUT_BOARD_NAME),
		    sizeof (board_infop->enbi_name));
		board_infop->enbi_revision =
		    MCDI_OUT_DWORD(req, GET_VERSION_V2_OUT_BOARD_REVISION);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn		efx_rc_t
efx_mcdi_get_boot_status(
	__in			efx_nic_t *enp,
	__out			efx_mcdi_boot_t *statusp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_BOOT_STATUS_IN_LEN,
	    MC_CMD_GET_BOOT_STATUS_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_features, &, EFX_FEATURE_MCDI);

	req.emr_cmd = MC_CMD_GET_BOOT_STATUS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_BOOT_STATUS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_BOOT_STATUS_OUT_LEN;

	efx_mcdi_execute_quiet(enp, &req);

	/*
	 * NOTE: Unprivileged functions cannot access boot status,
	 *       so the MCDI request will return EACCES. This is
	 *       also checked in efx_mcdi_version.
	 */

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_BOOT_STATUS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_DWORD_FIELD(req, GET_BOOT_STATUS_OUT_FLAGS,
	    GET_BOOT_STATUS_OUT_FLAGS_PRIMARY))
		*statusp = EFX_MCDI_BOOT_PRIMARY;
	else
		*statusp = EFX_MCDI_BOOT_SECONDARY;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_version(
	__in			efx_nic_t *enp,
	__out_ecount_opt(4)	uint16_t versionp[4],
	__out_opt		uint32_t *buildp,
	__out_opt		efx_mcdi_boot_t *statusp)
{
	efx_mcdi_version_t ver;
	efx_mcdi_boot_t status;
	efx_rc_t rc;

	rc = efx_mcdi_get_version(enp, 0, &ver);
	if (rc != 0)
		goto fail1;

	/* The bootrom doesn't understand BOOT_STATUS */
	if (MC_FW_VERSION_IS_BOOTLOADER(ver.emv_firmware)) {
		status = EFX_MCDI_BOOT_ROM;
		goto out;
	}

	rc = efx_mcdi_get_boot_status(enp, &status);
	if (rc == EACCES) {
		/* Unprivileged functions cannot access BOOT_STATUS */
		status = EFX_MCDI_BOOT_PRIMARY;
		memset(ver.emv_version, 0, sizeof (ver.emv_version));
		ver.emv_firmware = 0;
	} else if (rc != 0) {
		goto fail2;
	}

out:
	if (versionp != NULL)
		memcpy(versionp, ver.emv_version, sizeof (ver.emv_version));
	if (buildp != NULL)
		*buildp = ver.emv_firmware;
	if (statusp != NULL)
		*statusp = status;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_capabilities(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *flagsp,
	__out_opt	uint16_t *rx_dpcpu_fw_idp,
	__out_opt	uint16_t *tx_dpcpu_fw_idp,
	__out_opt	uint32_t *flags2p,
	__out_opt	uint32_t *tso2ncp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_CAPABILITIES_IN_LEN,
		MC_CMD_GET_CAPABILITIES_V2_OUT_LEN);
	boolean_t v2_capable;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_CAPABILITIES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_CAPABILITIES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_CAPABILITIES_V2_OUT_LEN;

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_CAPABILITIES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (flagsp != NULL)
		*flagsp = MCDI_OUT_DWORD(req, GET_CAPABILITIES_OUT_FLAGS1);

	if (rx_dpcpu_fw_idp != NULL)
		*rx_dpcpu_fw_idp = MCDI_OUT_WORD(req,
					GET_CAPABILITIES_OUT_RX_DPCPU_FW_ID);

	if (tx_dpcpu_fw_idp != NULL)
		*tx_dpcpu_fw_idp = MCDI_OUT_WORD(req,
					GET_CAPABILITIES_OUT_TX_DPCPU_FW_ID);

	if (req.emr_out_length_used < MC_CMD_GET_CAPABILITIES_V2_OUT_LEN)
		v2_capable = B_FALSE;
	else
		v2_capable = B_TRUE;

	if (flags2p != NULL) {
		*flags2p = (v2_capable) ?
			MCDI_OUT_DWORD(req, GET_CAPABILITIES_V2_OUT_FLAGS2) :
			0;
	}

	if (tso2ncp != NULL) {
		*tso2ncp = (v2_capable) ?
			MCDI_OUT_WORD(req,
				GET_CAPABILITIES_V2_OUT_TX_TSO_V2_N_CONTEXTS) :
			0;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_do_reboot(
	__in		efx_nic_t *enp,
	__in		boolean_t after_assertion)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_REBOOT_IN_LEN,
		MC_CMD_REBOOT_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	/*
	 * We could require the caller to have caused en_mod_flags=0 to
	 * call this function. This doesn't help the other port though,
	 * who's about to get the MC ripped out from underneath them.
	 * Since they have to cope with the subsequent fallout of MCDI
	 * failures, we should as well.
	 */
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	req.emr_cmd = MC_CMD_REBOOT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_REBOOT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_REBOOT_OUT_LEN;

	MCDI_IN_SET_DWORD(req, REBOOT_IN_FLAGS,
	    (after_assertion ? MC_CMD_REBOOT_FLAGS_AFTER_ASSERTION : 0));

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc == EACCES) {
		/* Unprivileged functions cannot reboot the MC. */
		goto out;
	}

	/* A successful reboot request returns EIO. */
	if (req.emr_rc != 0 && req.emr_rc != EIO) {
		rc = req.emr_rc;
		goto fail1;
	}

out:
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_reboot(
	__in		efx_nic_t *enp)
{
	return (efx_mcdi_do_reboot(enp, B_FALSE));
}

	__checkReturn	efx_rc_t
efx_mcdi_exit_assertion_handler(
	__in		efx_nic_t *enp)
{
	return (efx_mcdi_do_reboot(enp, B_TRUE));
}

	__checkReturn	efx_rc_t
efx_mcdi_read_assertion(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_ASSERTS_IN_LEN,
		MC_CMD_GET_ASSERTS_OUT_LEN);
	const char *reason;
	unsigned int flags;
	unsigned int index;
	unsigned int ofst;
	int retry;
	efx_rc_t rc;

	/*
	 * Before we attempt to chat to the MC, we should verify that the MC
	 * isn't in it's assertion handler, either due to a previous reboot,
	 * or because we're reinitializing due to an eec_exception().
	 *
	 * Use GET_ASSERTS to read any assertion state that may be present.
	 * Retry this command twice. Once because a boot-time assertion failure
	 * might cause the 1st MCDI request to fail. And once again because
	 * we might race with efx_mcdi_exit_assertion_handler() running on
	 * partner port(s) on the same NIC.
	 */
	retry = 2;
	do {
		(void) memset(payload, 0, sizeof (payload));
		req.emr_cmd = MC_CMD_GET_ASSERTS;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_GET_ASSERTS_IN_LEN;
		req.emr_out_buf = payload;
		req.emr_out_length = MC_CMD_GET_ASSERTS_OUT_LEN;

		MCDI_IN_SET_DWORD(req, GET_ASSERTS_IN_CLEAR, 1);
		efx_mcdi_execute_quiet(enp, &req);

	} while ((req.emr_rc == EINTR || req.emr_rc == EIO) && retry-- > 0);

	if (req.emr_rc != 0) {
		if (req.emr_rc == EACCES) {
			/* Unprivileged functions cannot clear assertions. */
			goto out;
		}
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_ASSERTS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	/* Print out any assertion state recorded */
	flags = MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_GLOBAL_FLAGS);
	if (flags == MC_CMD_GET_ASSERTS_FLAGS_NO_FAILS)
		return (0);

	reason = (flags == MC_CMD_GET_ASSERTS_FLAGS_SYS_FAIL)
		? "system-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_THR_FAIL)
		? "thread-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_WDOG_FIRED)
		? "watchdog reset"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_ADDR_TRAP)
		? "illegal address trap"
		: "unknown assertion";
	EFSYS_PROBE3(mcpu_assertion,
	    const char *, reason, unsigned int,
	    MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_SAVED_PC_OFFS),
	    unsigned int,
	    MCDI_OUT_DWORD(req, GET_ASSERTS_OUT_THREAD_OFFS));

	/* Print out the registers (r1 ... r31) */
	ofst = MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_OFST;
	for (index = 1;
		index < 1 + MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_NUM;
		index++) {
		EFSYS_PROBE2(mcpu_register, unsigned int, index, unsigned int,
			    EFX_DWORD_FIELD(*MCDI_OUT(req, efx_dword_t, ofst),
					    EFX_DWORD_0));
		ofst += sizeof (efx_dword_t);
	}
	EFSYS_ASSERT(ofst <= MC_CMD_GET_ASSERTS_OUT_LEN);

out:
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


/*
 * Internal routines for for specific MCDI requests.
 */

	__checkReturn	efx_rc_t
efx_mcdi_drv_attach(
	__in		efx_nic_t *enp,
	__in		boolean_t attach)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_DRV_ATTACH_IN_V2_LEN,
		MC_CMD_DRV_ATTACH_EXT_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_DRV_ATTACH;
	req.emr_in_buf = payload;
	if (enp->en_drv_version[0] == '\0') {
		req.emr_in_length = MC_CMD_DRV_ATTACH_IN_LEN;
	} else {
		req.emr_in_length = MC_CMD_DRV_ATTACH_IN_V2_LEN;
	}
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_DRV_ATTACH_EXT_OUT_LEN;

	/*
	 * Typically, client drivers use DONT_CARE for the datapath firmware
	 * type to ensure that the driver can attach to an unprivileged
	 * function. The datapath firmware type to use is controlled by the
	 * 'sfboot' utility.
	 * If a client driver wishes to attach with a specific datapath firmware
	 * type, that can be passed in second argument of efx_nic_probe API. One
	 * such example is the ESXi native driver that attempts attaching with
	 * FULL_FEATURED datapath firmware type first and fall backs to
	 * DONT_CARE datapath firmware type if MC_CMD_DRV_ATTACH fails.
	 */
	MCDI_IN_POPULATE_DWORD_2(req, DRV_ATTACH_IN_NEW_STATE,
	    DRV_ATTACH_IN_ATTACH, attach ? 1 : 0,
	    DRV_ATTACH_IN_SUBVARIANT_AWARE, EFSYS_OPT_FW_SUBVARIANT_AWARE);
	MCDI_IN_SET_DWORD(req, DRV_ATTACH_IN_UPDATE, 1);
	MCDI_IN_SET_DWORD(req, DRV_ATTACH_IN_FIRMWARE_ID, enp->efv);

	if (req.emr_in_length >= MC_CMD_DRV_ATTACH_IN_V2_LEN) {
		EFX_STATIC_ASSERT(sizeof (enp->en_drv_version) ==
		    MC_CMD_DRV_ATTACH_IN_V2_DRIVER_VERSION_LEN);
		memcpy(MCDI_IN2(req, char, DRV_ATTACH_IN_V2_DRIVER_VERSION),
		    enp->en_drv_version,
		    MC_CMD_DRV_ATTACH_IN_V2_DRIVER_VERSION_LEN);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_DRV_ATTACH_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_get_board_cfg(
	__in			efx_nic_t *enp,
	__out_opt		uint32_t *board_typep,
	__out_opt		efx_dword_t *capabilitiesp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6])
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_BOARD_CFG_IN_LEN,
		MC_CMD_GET_BOARD_CFG_OUT_LENMIN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_BOARD_CFG;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_BOARD_CFG_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_BOARD_CFG_OUT_LENMIN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_BOARD_CFG_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (mac_addrp != NULL) {
		uint8_t *addrp;

		if (emip->emi_port == 1) {
			addrp = MCDI_OUT2(req, uint8_t,
			    GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT0);
		} else if (emip->emi_port == 2) {
			addrp = MCDI_OUT2(req, uint8_t,
			    GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT1);
		} else {
			rc = EINVAL;
			goto fail3;
		}

		EFX_MAC_ADDR_COPY(mac_addrp, addrp);
	}

	if (capabilitiesp != NULL) {
		if (emip->emi_port == 1) {
			*capabilitiesp = *MCDI_OUT2(req, efx_dword_t,
			    GET_BOARD_CFG_OUT_CAPABILITIES_PORT0);
		} else if (emip->emi_port == 2) {
			*capabilitiesp = *MCDI_OUT2(req, efx_dword_t,
			    GET_BOARD_CFG_OUT_CAPABILITIES_PORT1);
		} else {
			rc = EINVAL;
			goto fail4;
		}
	}

	if (board_typep != NULL) {
		*board_typep = MCDI_OUT_DWORD(req,
		    GET_BOARD_CFG_OUT_BOARD_TYPE);
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_resource_limits(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *nevqp,
	__out_opt	uint32_t *nrxqp,
	__out_opt	uint32_t *ntxqp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_RESOURCE_LIMITS_IN_LEN,
		MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_RESOURCE_LIMITS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_RESOURCE_LIMITS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (nevqp != NULL)
		*nevqp = MCDI_OUT_DWORD(req, GET_RESOURCE_LIMITS_OUT_EVQ);
	if (nrxqp != NULL)
		*nrxqp = MCDI_OUT_DWORD(req, GET_RESOURCE_LIMITS_OUT_RXQ);
	if (ntxqp != NULL)
		*ntxqp = MCDI_OUT_DWORD(req, GET_RESOURCE_LIMITS_OUT_TXQ);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_phy_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_PHY_CFG_IN_LEN,
		MC_CMD_GET_PHY_CFG_OUT_LEN);
#if EFSYS_OPT_NAMES
	const char *namep;
	size_t namelen;
#endif
	uint32_t phy_media_type;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_PHY_CFG;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PHY_CFG_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_PHY_CFG_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_PHY_CFG_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	encp->enc_phy_type = MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_TYPE);
#if EFSYS_OPT_NAMES
	namep = MCDI_OUT2(req, char, GET_PHY_CFG_OUT_NAME);
	namelen = MIN(sizeof (encp->enc_phy_name) - 1,
		    strnlen(namep, MC_CMD_GET_PHY_CFG_OUT_NAME_LEN));
	(void) memset(encp->enc_phy_name, 0,
	    sizeof (encp->enc_phy_name));
	memcpy(encp->enc_phy_name, namep, namelen);
#endif	/* EFSYS_OPT_NAMES */
	(void) memset(encp->enc_phy_revision, 0,
	    sizeof (encp->enc_phy_revision));
	memcpy(encp->enc_phy_revision,
		MCDI_OUT2(req, char, GET_PHY_CFG_OUT_REVISION),
		MIN(sizeof (encp->enc_phy_revision) - 1,
		    MC_CMD_GET_PHY_CFG_OUT_REVISION_LEN));
#if EFSYS_OPT_PHY_LED_CONTROL
	encp->enc_led_mask = ((1 << EFX_PHY_LED_DEFAULT) |
			    (1 << EFX_PHY_LED_OFF) |
			    (1 << EFX_PHY_LED_ON));
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	/* Get the media type of the fixed port, if recognised. */
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_XAUI == EFX_PHY_MEDIA_XAUI);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_CX4 == EFX_PHY_MEDIA_CX4);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_KX4 == EFX_PHY_MEDIA_KX4);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_XFP == EFX_PHY_MEDIA_XFP);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_SFP_PLUS == EFX_PHY_MEDIA_SFP_PLUS);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_BASE_T == EFX_PHY_MEDIA_BASE_T);
	EFX_STATIC_ASSERT(MC_CMD_MEDIA_QSFP_PLUS == EFX_PHY_MEDIA_QSFP_PLUS);
	phy_media_type = MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_MEDIA_TYPE);
	epp->ep_fixed_port_type = (efx_phy_media_type_t)phy_media_type;
	if (epp->ep_fixed_port_type >= EFX_PHY_MEDIA_NTYPES)
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_INVALID;

	epp->ep_phy_cap_mask =
		MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_SUPPORTED_CAP);
#if EFSYS_OPT_PHY_FLAGS
	encp->enc_phy_flags_mask = MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_FLAGS);
#endif	/* EFSYS_OPT_PHY_FLAGS */

	encp->enc_port = (uint8_t)MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_PRT);

	/* Populate internal state */
	encp->enc_mcdi_mdio_channel =
		(uint8_t)MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_CHANNEL);

#if EFSYS_OPT_PHY_STATS
	encp->enc_mcdi_phy_stat_mask =
		MCDI_OUT_DWORD(req, GET_PHY_CFG_OUT_STATS_MASK);
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_BIST
	encp->enc_bist_mask = 0;
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST_CABLE_SHORT))
		encp->enc_bist_mask |= (1 << EFX_BIST_TYPE_PHY_CABLE_SHORT);
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST_CABLE_LONG))
		encp->enc_bist_mask |= (1 << EFX_BIST_TYPE_PHY_CABLE_LONG);
	if (MCDI_OUT_DWORD_FIELD(req, GET_PHY_CFG_OUT_FLAGS,
	    GET_PHY_CFG_OUT_BIST))
		encp->enc_bist_mask |= (1 << EFX_BIST_TYPE_PHY_NORMAL);
#endif  /* EFSYS_OPT_BIST */

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_firmware_update_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	efx_rc_t rc;

	if (emcop != NULL) {
		if ((rc = emcop->emco_feature_supported(enp,
			    EFX_MCDI_FEATURE_FW_UPDATE, supportedp)) != 0)
			goto fail1;
	} else {
		/* Earlier devices always supported updates */
		*supportedp = B_TRUE;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_macaddr_change_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	efx_rc_t rc;

	if (emcop != NULL) {
		if ((rc = emcop->emco_feature_supported(enp,
			    EFX_MCDI_FEATURE_MACADDR_CHANGE, supportedp)) != 0)
			goto fail1;
	} else {
		/* Earlier devices always supported MAC changes */
		*supportedp = B_TRUE;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_link_control_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	efx_rc_t rc;

	if (emcop != NULL) {
		if ((rc = emcop->emco_feature_supported(enp,
			    EFX_MCDI_FEATURE_LINK_CONTROL, supportedp)) != 0)
			goto fail1;
	} else {
		/* Earlier devices always supported link control */
		*supportedp = B_TRUE;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_mac_spoofing_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp)
{
	const efx_mcdi_ops_t *emcop = enp->en_mcdi.em_emcop;
	efx_rc_t rc;

	if (emcop != NULL) {
		if ((rc = emcop->emco_feature_supported(enp,
			    EFX_MCDI_FEATURE_MAC_SPOOFING, supportedp)) != 0)
			goto fail1;
	} else {
		/* Earlier devices always supported MAC spoofing */
		*supportedp = B_TRUE;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_BIST

#if EFX_OPTS_EF10()
/*
 * Enter bist offline mode. This is a fw mode which puts the NIC into a state
 * where memory BIST tests can be run and not much else can interfere or happen.
 * A reboot is required to exit this mode.
 */
	__checkReturn		efx_rc_t
efx_mcdi_bist_enable_offline(
	__in			efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(MC_CMD_ENABLE_OFFLINE_BIST_IN_LEN == 0);
	EFX_STATIC_ASSERT(MC_CMD_ENABLE_OFFLINE_BIST_OUT_LEN == 0);

	req.emr_cmd = MC_CMD_ENABLE_OFFLINE_BIST;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif /* EFX_OPTS_EF10() */

	__checkReturn		efx_rc_t
efx_mcdi_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_START_BIST_IN_LEN,
		MC_CMD_START_BIST_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_START_BIST;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_START_BIST_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_START_BIST_OUT_LEN;

	switch (type) {
	case EFX_BIST_TYPE_PHY_NORMAL:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE, MC_CMD_PHY_BIST);
		break;
	case EFX_BIST_TYPE_PHY_CABLE_SHORT:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE,
		    MC_CMD_PHY_BIST_CABLE_SHORT);
		break;
	case EFX_BIST_TYPE_PHY_CABLE_LONG:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE,
		    MC_CMD_PHY_BIST_CABLE_LONG);
		break;
	case EFX_BIST_TYPE_MC_MEM:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE,
		    MC_CMD_MC_MEM_BIST);
		break;
	case EFX_BIST_TYPE_SAT_MEM:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE,
		    MC_CMD_PORT_MEM_BIST);
		break;
	case EFX_BIST_TYPE_REG:
		MCDI_IN_SET_DWORD(req, START_BIST_IN_TYPE,
		    MC_CMD_REG_BIST);
		break;
	default:
		EFSYS_ASSERT(0);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_BIST */


/* Enable logging of some events (e.g. link state changes) */
	__checkReturn	efx_rc_t
efx_mcdi_log_ctrl(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_LOG_CTRL_IN_LEN,
		MC_CMD_LOG_CTRL_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_LOG_CTRL;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_LOG_CTRL_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_LOG_CTRL_OUT_LEN;

	MCDI_IN_SET_DWORD(req, LOG_CTRL_IN_LOG_DEST,
		    MC_CMD_LOG_CTRL_IN_LOG_DEST_EVQ);
	MCDI_IN_SET_DWORD(req, LOG_CTRL_IN_LOG_DEST_EVQ, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


#if EFSYS_OPT_MAC_STATS

	__checkReturn	efx_rc_t
efx_mcdi_mac_stats(
	__in		efx_nic_t *enp,
	__in		uint32_t vport_id,
	__in_opt	efsys_mem_t *esmp,
	__in		efx_stats_action_t action,
	__in		uint16_t period_ms)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_MAC_STATS_IN_LEN,
		MC_CMD_MAC_STATS_V2_OUT_DMA_LEN);
	int clear = (action == EFX_STATS_CLEAR);
	int upload = (action == EFX_STATS_UPLOAD);
	int enable = (action == EFX_STATS_ENABLE_NOEVENTS);
	int events = (action == EFX_STATS_ENABLE_EVENTS);
	int disable = (action == EFX_STATS_DISABLE);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_MAC_STATS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAC_STATS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAC_STATS_V2_OUT_DMA_LEN;

	MCDI_IN_POPULATE_DWORD_6(req, MAC_STATS_IN_CMD,
	    MAC_STATS_IN_DMA, upload,
	    MAC_STATS_IN_CLEAR, clear,
	    MAC_STATS_IN_PERIODIC_CHANGE, enable | events | disable,
	    MAC_STATS_IN_PERIODIC_ENABLE, enable | events,
	    MAC_STATS_IN_PERIODIC_NOEVENT, !events,
	    MAC_STATS_IN_PERIOD_MS, (enable | events) ? period_ms : 0);

	if (enable || events || upload) {
		const efx_nic_cfg_t *encp = &enp->en_nic_cfg;
		uint32_t bytes;

		/* Periodic stats or stats upload require a DMA buffer */
		if (esmp == NULL) {
			rc = EINVAL;
			goto fail1;
		}

		if (encp->enc_mac_stats_nstats < MC_CMD_MAC_NSTATS) {
			/* MAC stats count too small for legacy MAC stats */
			rc = ENOSPC;
			goto fail2;
		}

		bytes = encp->enc_mac_stats_nstats * sizeof (efx_qword_t);

		if (EFSYS_MEM_SIZE(esmp) < bytes) {
			/* DMA buffer too small */
			rc = ENOSPC;
			goto fail3;
		}

		MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_LO,
			    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
		MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_ADDR_HI,
			    EFSYS_MEM_ADDR(esmp) >> 32);
		MCDI_IN_SET_DWORD(req, MAC_STATS_IN_DMA_LEN, bytes);
	}

	/*
	 * NOTE: Do not use EVB_PORT_ID_ASSIGNED when disabling periodic stats,
	 *	 as this may fail (and leave periodic DMA enabled) if the
	 *	 vadapter has already been deleted.
	 */
	MCDI_IN_SET_DWORD(req, MAC_STATS_IN_PORT_ID,
		(disable ? EVB_PORT_ID_NULL : vport_id));

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		/* EF10: Expect ENOENT if no DMA queues are initialised */
		if ((req.emr_rc != ENOENT) ||
		    (enp->en_rx_qcount + enp->en_tx_qcount != 0)) {
			rc = req.emr_rc;
			goto fail4;
		}
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_clear(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_mac_stats(enp, enp->en_vport_id, NULL,
			EFX_STATS_CLEAR, 0)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_upload(
	__in		efx_nic_t *enp,
	__in		efsys_mem_t *esmp)
{
	efx_rc_t rc;

	/*
	 * The MC DMAs aggregate statistics for our convenience, so we can
	 * avoid having to pull the statistics buffer into the cache to
	 * maintain cumulative statistics.
	 */
	if ((rc = efx_mcdi_mac_stats(enp, enp->en_vport_id, esmp,
			EFX_STATS_UPLOAD, 0)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_periodic(
	__in		efx_nic_t *enp,
	__in		efsys_mem_t *esmp,
	__in		uint16_t period_ms,
	__in		boolean_t events)
{
	efx_rc_t rc;

	/*
	 * The MC DMAs aggregate statistics for our convenience, so we can
	 * avoid having to pull the statistics buffer into the cache to
	 * maintain cumulative statistics.
	 * Huntington uses a fixed 1sec period.
	 * Medford uses a fixed 1sec period before v6.2.1.1033 firmware.
	 */
	if (period_ms == 0)
		rc = efx_mcdi_mac_stats(enp, enp->en_vport_id, NULL,
			EFX_STATS_DISABLE, 0);
	else if (events)
		rc = efx_mcdi_mac_stats(enp, enp->en_vport_id, esmp,
			EFX_STATS_ENABLE_EVENTS, period_ms);
	else
		rc = efx_mcdi_mac_stats(enp, enp->en_vport_id, esmp,
			EFX_STATS_ENABLE_NOEVENTS, period_ms);

	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_MAC_STATS */

#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()

	__checkReturn		efx_rc_t
efx_mcdi_intf_from_pcie(
	__in			uint32_t pcie_intf,
	__out			efx_pcie_interface_t *efx_intf)
{
	efx_rc_t rc;

	switch (pcie_intf) {
	case PCIE_INTERFACE_CALLER:
		*efx_intf = EFX_PCIE_INTERFACE_CALLER;
		break;
	case PCIE_INTERFACE_HOST_PRIMARY:
		*efx_intf = EFX_PCIE_INTERFACE_HOST_PRIMARY;
		break;
	case PCIE_INTERFACE_NIC_EMBEDDED:
		*efx_intf = EFX_PCIE_INTERFACE_NIC_EMBEDDED;
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_intf_to_pcie(
	__in			efx_pcie_interface_t efx_intf,
	__out			uint32_t *pcie_intf)
{
	efx_rc_t rc;

	switch (efx_intf) {
	case EFX_PCIE_INTERFACE_CALLER:
		*pcie_intf = PCIE_INTERFACE_CALLER;
		break;
	case EFX_PCIE_INTERFACE_HOST_PRIMARY:
		*pcie_intf = PCIE_INTERFACE_HOST_PRIMARY;
		break;
	case EFX_PCIE_INTERFACE_NIC_EMBEDDED:
		*pcie_intf = PCIE_INTERFACE_NIC_EMBEDDED;
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

/*
 * This function returns the pf and vf number of a function.  If it is a pf the
 * vf number is 0xffff.  The vf number is the index of the vf on that
 * function. So if you have 3 vfs on pf 0 the 3 vfs will return (pf=0,vf=0),
 * (pf=0,vf=1), (pf=0,vf=2) aand the pf will return (pf=0, vf=0xffff).
 */
	__checkReturn		efx_rc_t
efx_mcdi_get_function_info(
	__in			efx_nic_t *enp,
	__out			uint32_t *pfp,
	__out_opt		uint32_t *vfp,
	__out_opt		efx_pcie_interface_t *intfp)
{
	efx_pcie_interface_t intf;
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_FUNCTION_INFO_IN_LEN,
		MC_CMD_GET_FUNCTION_INFO_OUT_V2_LEN);
	uint32_t pcie_intf;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_FUNCTION_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_FUNCTION_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_FUNCTION_INFO_OUT_V2_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_FUNCTION_INFO_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*pfp = MCDI_OUT_DWORD(req, GET_FUNCTION_INFO_OUT_PF);
	if (vfp != NULL)
		*vfp = MCDI_OUT_DWORD(req, GET_FUNCTION_INFO_OUT_VF);

	if (req.emr_out_length < MC_CMD_GET_FUNCTION_INFO_OUT_V2_LEN) {
		intf = EFX_PCIE_INTERFACE_HOST_PRIMARY;
	} else {
		pcie_intf = MCDI_OUT_DWORD(req,
		    GET_FUNCTION_INFO_OUT_V2_INTF);

		rc = efx_mcdi_intf_from_pcie(pcie_intf, &intf);
		if (rc != 0)
			goto fail3;
	}

	if (intfp != NULL)
		*intfp = intf;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_privilege_mask(
	__in			efx_nic_t *enp,
	__in			uint32_t pf,
	__in			uint32_t vf,
	__out			uint32_t *maskp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_PRIVILEGE_MASK_IN_LEN,
		MC_CMD_PRIVILEGE_MASK_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_PRIVILEGE_MASK;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PRIVILEGE_MASK_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PRIVILEGE_MASK_OUT_LEN;

	MCDI_IN_POPULATE_DWORD_2(req, PRIVILEGE_MASK_IN_FUNCTION,
	    PRIVILEGE_MASK_IN_FUNCTION_PF, pf,
	    PRIVILEGE_MASK_IN_FUNCTION_VF, vf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_PRIVILEGE_MASK_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*maskp = MCDI_OUT_DWORD(req, PRIVILEGE_MASK_OUT_OLD_MASK);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */

	__checkReturn		efx_rc_t
efx_mcdi_set_workaround(
	__in			efx_nic_t *enp,
	__in			uint32_t type,
	__in			boolean_t enabled,
	__out_opt		uint32_t *flagsp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_WORKAROUND_IN_LEN,
		MC_CMD_WORKAROUND_EXT_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_WORKAROUND;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_WORKAROUND_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_WORKAROUND_OUT_LEN;

	MCDI_IN_SET_DWORD(req, WORKAROUND_IN_TYPE, type);
	MCDI_IN_SET_DWORD(req, WORKAROUND_IN_ENABLED, enabled ? 1 : 0);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (flagsp != NULL) {
		if (req.emr_out_length_used >= MC_CMD_WORKAROUND_EXT_OUT_LEN)
			*flagsp = MCDI_OUT_DWORD(req, WORKAROUND_EXT_OUT_FLAGS);
		else
			*flagsp = 0;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn		efx_rc_t
efx_mcdi_get_workarounds(
	__in			efx_nic_t *enp,
	__out_opt		uint32_t *implementedp,
	__out_opt		uint32_t *enabledp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, 0, MC_CMD_GET_WORKAROUNDS_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_WORKAROUNDS;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_WORKAROUNDS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_WORKAROUNDS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (implementedp != NULL) {
		*implementedp =
		    MCDI_OUT_DWORD(req, GET_WORKAROUNDS_OUT_IMPLEMENTED);
	}

	if (enabledp != NULL) {
		*enabledp = MCDI_OUT_DWORD(req, GET_WORKAROUNDS_OUT_ENABLED);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * Size of media information page in accordance with SFF-8472 and SFF-8436.
 * It is used in MCDI interface as well.
 */
#define	EFX_PHY_MEDIA_INFO_PAGE_SIZE		0x80

/*
 * Transceiver identifiers from SFF-8024 Table 4-1.
 */
#define	EFX_SFF_TRANSCEIVER_ID_SFP		0x03 /* SFP/SFP+/SFP28 */
#define	EFX_SFF_TRANSCEIVER_ID_QSFP		0x0c /* QSFP */
#define	EFX_SFF_TRANSCEIVER_ID_QSFP_PLUS	0x0d /* QSFP+ or later */
#define	EFX_SFF_TRANSCEIVER_ID_QSFP28		0x11 /* QSFP28 or later */

static	__checkReturn		efx_rc_t
efx_mcdi_get_phy_media_info(
	__in			efx_nic_t *enp,
	__in			uint32_t mcdi_page,
	__in			uint8_t offset,
	__in			uint8_t len,
	__out_bcount(len)	uint8_t *data)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_PHY_MEDIA_INFO_IN_LEN,
		MC_CMD_GET_PHY_MEDIA_INFO_OUT_LEN(
			EFX_PHY_MEDIA_INFO_PAGE_SIZE));
	efx_rc_t rc;

	EFSYS_ASSERT((uint32_t)offset + len <= EFX_PHY_MEDIA_INFO_PAGE_SIZE);

	req.emr_cmd = MC_CMD_GET_PHY_MEDIA_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PHY_MEDIA_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length =
	    MC_CMD_GET_PHY_MEDIA_INFO_OUT_LEN(EFX_PHY_MEDIA_INFO_PAGE_SIZE);

	MCDI_IN_SET_DWORD(req, GET_PHY_MEDIA_INFO_IN_PAGE, mcdi_page);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used !=
	    MC_CMD_GET_PHY_MEDIA_INFO_OUT_LEN(EFX_PHY_MEDIA_INFO_PAGE_SIZE)) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_DWORD(req, GET_PHY_MEDIA_INFO_OUT_DATALEN) !=
	    EFX_PHY_MEDIA_INFO_PAGE_SIZE) {
		rc = EIO;
		goto fail3;
	}

	memcpy(data,
	    MCDI_OUT2(req, uint8_t, GET_PHY_MEDIA_INFO_OUT_DATA) + offset,
	    len);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_phy_module_get_info(
	__in			efx_nic_t *enp,
	__in			uint8_t dev_addr,
	__in			size_t offset,
	__in			size_t len,
	__out_bcount(len)	uint8_t *data)
{
	efx_port_t *epp = &(enp->en_port);
	efx_rc_t rc;
	uint32_t mcdi_lower_page;
	uint32_t mcdi_upper_page;
	uint8_t id;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	/*
	 * Map device address to MC_CMD_GET_PHY_MEDIA_INFO pages.
	 * Offset plus length interface allows to access page 0 only.
	 * I.e. non-zero upper pages are not accessible.
	 * See SFF-8472 section 4 Memory Organization and SFF-8436 section 7.6
	 * QSFP+ Memory Map for details on how information is structured
	 * and accessible.
	 */
	switch (epp->ep_fixed_port_type) {
	case EFX_PHY_MEDIA_SFP_PLUS:
	case EFX_PHY_MEDIA_QSFP_PLUS:
		/* Port type supports modules */
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	/*
	 * For all supported port types, MCDI page 0 offset 0 holds the
	 * transceiver identifier. Probe to determine the data layout.
	 * Definitions from SFF-8024 Table 4-1.
	 */
	rc = efx_mcdi_get_phy_media_info(enp,
		    0, 0, sizeof(id), &id);
	if (rc != 0)
		goto fail2;

	switch (id) {
	case EFX_SFF_TRANSCEIVER_ID_SFP:
		/*
		 * In accordance with SFF-8472 Diagnostic Monitoring
		 * Interface for Optical Transceivers section 4 Memory
		 * Organization two 2-wire addresses are defined.
		 */
		switch (dev_addr) {
		/* Base information */
		case EFX_PHY_MEDIA_INFO_DEV_ADDR_SFP_BASE:
			/*
			 * MCDI page 0 should be used to access lower
			 * page 0 (0x00 - 0x7f) at the device address 0xA0.
			 */
			mcdi_lower_page = 0;
			/*
			 * MCDI page 1 should be used to access  upper
			 * page 0 (0x80 - 0xff) at the device address 0xA0.
			 */
			mcdi_upper_page = 1;
			break;
		/* Diagnostics */
		case EFX_PHY_MEDIA_INFO_DEV_ADDR_SFP_DDM:
			/*
			 * MCDI page 2 should be used to access lower
			 * page 0 (0x00 - 0x7f) at the device address 0xA2.
			 */
			mcdi_lower_page = 2;
			/*
			 * MCDI page 3 should be used to access upper
			 * page 0 (0x80 - 0xff) at the device address 0xA2.
			 */
			mcdi_upper_page = 3;
			break;
		default:
			rc = ENOTSUP;
			goto fail3;
		}
		break;
	case EFX_SFF_TRANSCEIVER_ID_QSFP:
	case EFX_SFF_TRANSCEIVER_ID_QSFP_PLUS:
	case EFX_SFF_TRANSCEIVER_ID_QSFP28:
		switch (dev_addr) {
		case EFX_PHY_MEDIA_INFO_DEV_ADDR_QSFP:
			/*
			 * MCDI page -1 should be used to access lower page 0
			 * (0x00 - 0x7f).
			 */
			mcdi_lower_page = (uint32_t)-1;
			/*
			 * MCDI page 0 should be used to access upper page 0
			 * (0x80h - 0xff).
			 */
			mcdi_upper_page = 0;
			break;
		default:
			rc = ENOTSUP;
			goto fail3;
		}
		break;
	default:
		rc = ENOTSUP;
		goto fail3;
	}

	EFX_STATIC_ASSERT(EFX_PHY_MEDIA_INFO_PAGE_SIZE <= 0xFF);

	if (offset < EFX_PHY_MEDIA_INFO_PAGE_SIZE) {
		size_t read_len =
		    MIN(len, EFX_PHY_MEDIA_INFO_PAGE_SIZE - offset);

		rc = efx_mcdi_get_phy_media_info(enp,
		    mcdi_lower_page, (uint8_t)offset, (uint8_t)read_len, data);
		if (rc != 0)
			goto fail4;

		data += read_len;
		len -= read_len;

		offset = 0;
	} else {
		offset -= EFX_PHY_MEDIA_INFO_PAGE_SIZE;
	}

	if (len > 0) {
		EFSYS_ASSERT3U(len, <=, EFX_PHY_MEDIA_INFO_PAGE_SIZE);
		EFSYS_ASSERT3U(offset, <, EFX_PHY_MEDIA_INFO_PAGE_SIZE);

		rc = efx_mcdi_get_phy_media_info(enp,
		    mcdi_upper_page, (uint8_t)offset, (uint8_t)len, data);
		if (rc != 0)
			goto fail5;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()

#define	INIT_EVQ_MAXNBUFS	MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_MAXNUM

#if EFX_OPTS_EF10()
# if (INIT_EVQ_MAXNBUFS < EF10_EVQ_MAXNBUFS)
#  error "INIT_EVQ_MAXNBUFS too small"
# endif
#endif /* EFX_OPTS_EF10 */
#if EFSYS_OPT_RIVERHEAD
# if (INIT_EVQ_MAXNBUFS < RHEAD_EVQ_MAXNBUFS)
#  error "INIT_EVQ_MAXNBUFS too small"
# endif
#endif /* EFSYS_OPT_RIVERHEAD */

	__checkReturn	efx_rc_t
efx_mcdi_init_evq(
	__in		efx_nic_t *enp,
	__in		unsigned int instance,
	__in		efsys_mem_t *esmp,
	__in		size_t nevs,
	__in		uint32_t irq,
	__in		uint32_t target_evq,
	__in		uint32_t us,
	__in		uint32_t flags,
	__in		boolean_t low_latency)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
		MC_CMD_INIT_EVQ_V2_IN_LEN(INIT_EVQ_MAXNBUFS),
		MC_CMD_INIT_EVQ_V2_OUT_LEN);
	boolean_t interrupting;
	int ev_extended_width;
	int ev_cut_through;
	int ev_merge;
	unsigned int evq_type;
	efx_qword_t *dma_addr;
	uint64_t addr;
	int npages;
	int i;
	efx_rc_t rc;

	npages = efx_evq_nbufs(enp, nevs, flags);
	if (npages > INIT_EVQ_MAXNBUFS) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_INIT_EVQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_INIT_EVQ_V2_IN_LEN(npages);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_INIT_EVQ_V2_OUT_LEN;

	MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_SIZE, nevs);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_INSTANCE, instance);

	interrupting = ((flags & EFX_EVQ_FLAGS_NOTIFY_MASK) ==
	    EFX_EVQ_FLAGS_NOTIFY_INTERRUPT);

	if (interrupting)
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_IRQ_NUM, irq);
	else
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TARGET_EVQ, target_evq);

	if (encp->enc_init_evq_v2_supported) {
		/*
		 * On Medford the low latency license is required to enable RX
		 * and event cut through and to disable RX batching.  If event
		 * queue type in flags is auto, we let the firmware decide the
		 * settings to use. If the adapter has a low latency license,
		 * it will choose the best settings for low latency, otherwise
		 * it will choose the best settings for throughput.
		 */
		switch (flags & EFX_EVQ_FLAGS_TYPE_MASK) {
		case EFX_EVQ_FLAGS_TYPE_AUTO:
			evq_type = MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_AUTO;
			break;
		case EFX_EVQ_FLAGS_TYPE_THROUGHPUT:
			evq_type = MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_THROUGHPUT;
			break;
		case EFX_EVQ_FLAGS_TYPE_LOW_LATENCY:
			evq_type = MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_LOW_LATENCY;
			break;
		default:
			rc = EINVAL;
			goto fail2;
		}
		/* EvQ type controls merging, no manual settings */
		ev_merge = 0;
		ev_cut_through = 0;
	} else {
		/* EvQ types other than manual are not supported */
		evq_type = MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_MANUAL;
		/*
		 * On Huntington RX and TX event batching can only be requested
		 * together (even if the datapath firmware doesn't actually
		 * support RX batching). If event cut through is enabled no RX
		 * batching will occur.
		 *
		 * So always enable RX and TX event batching, and enable event
		 * cut through if we want low latency operation.
		 */
		ev_merge = 1;
		switch (flags & EFX_EVQ_FLAGS_TYPE_MASK) {
		case EFX_EVQ_FLAGS_TYPE_AUTO:
			ev_cut_through = low_latency ? 1 : 0;
			break;
		case EFX_EVQ_FLAGS_TYPE_THROUGHPUT:
			ev_cut_through = 0;
			break;
		case EFX_EVQ_FLAGS_TYPE_LOW_LATENCY:
			ev_cut_through = 1;
			break;
		default:
			rc = EINVAL;
			goto fail2;
		}
	}

	/*
	 * On EF100, extended width event queues have a different event
	 * descriptor layout and are used to support descriptor proxy queues.
	 */
	ev_extended_width = 0;
#if EFSYS_OPT_EV_EXTENDED_WIDTH
	if (encp->enc_init_evq_extended_width_supported) {
		if (flags & EFX_EVQ_FLAGS_EXTENDED_WIDTH)
			ev_extended_width = 1;
	}
#endif

	MCDI_IN_POPULATE_DWORD_8(req, INIT_EVQ_V2_IN_FLAGS,
	    INIT_EVQ_V2_IN_FLAG_INTERRUPTING, interrupting,
	    INIT_EVQ_V2_IN_FLAG_RPTR_DOS, 0,
	    INIT_EVQ_V2_IN_FLAG_INT_ARMD, 0,
	    INIT_EVQ_V2_IN_FLAG_CUT_THRU, ev_cut_through,
	    INIT_EVQ_V2_IN_FLAG_RX_MERGE, ev_merge,
	    INIT_EVQ_V2_IN_FLAG_TX_MERGE, ev_merge,
	    INIT_EVQ_V2_IN_FLAG_TYPE, evq_type,
	    INIT_EVQ_V2_IN_FLAG_EXT_WIDTH, ev_extended_width);

	/* If the value is zero then disable the timer */
	if (us == 0) {
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_MODE,
		    MC_CMD_INIT_EVQ_V2_IN_TMR_MODE_DIS);
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_LOAD, 0);
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_RELOAD, 0);
	} else {
		unsigned int ticks;

		if ((rc = efx_ev_usecs_to_ticks(enp, us, &ticks)) != 0)
			goto fail3;

		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_MODE,
		    MC_CMD_INIT_EVQ_V2_IN_TMR_INT_HLDOFF);
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_LOAD, ticks);
		MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_TMR_RELOAD, ticks);
	}

	MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_COUNT_MODE,
	    MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_DIS);
	MCDI_IN_SET_DWORD(req, INIT_EVQ_V2_IN_COUNT_THRSHLD, 0);

	dma_addr = MCDI_IN2(req, efx_qword_t, INIT_EVQ_V2_IN_DMA_ADDR);
	addr = EFSYS_MEM_ADDR(esmp);

	for (i = 0; i < npages; i++) {
		EFX_POPULATE_QWORD_2(*dma_addr,
		    EFX_DWORD_1, (uint32_t)(addr >> 32),
		    EFX_DWORD_0, (uint32_t)(addr & 0xffffffff));

		dma_addr++;
		addr += EFX_BUF_SIZE;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	if (encp->enc_init_evq_v2_supported) {
		if (req.emr_out_length_used < MC_CMD_INIT_EVQ_V2_OUT_LEN) {
			rc = EMSGSIZE;
			goto fail5;
		}
		EFSYS_PROBE1(mcdi_evq_flags, uint32_t,
			    MCDI_OUT_DWORD(req, INIT_EVQ_V2_OUT_FLAGS));
	} else {
		if (req.emr_out_length_used < MC_CMD_INIT_EVQ_OUT_LEN) {
			rc = EMSGSIZE;
			goto fail6;
		}
	}

	/* NOTE: ignore the returned IRQ param as firmware does not set it. */

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_fini_evq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_FINI_EVQ_IN_LEN,
		MC_CMD_FINI_EVQ_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_FINI_EVQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FINI_EVQ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FINI_EVQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FINI_EVQ_IN_INSTANCE, instance);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	/*
	 * EALREADY is not an error, but indicates that the MC has rebooted and
	 * that the EVQ has already been destroyed.
	 */
	if (rc != EALREADY)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_init_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t ndescs,
	__in		efx_evq_t *eep,
	__in		uint32_t label,
	__in		uint32_t instance,
	__in		efsys_mem_t *esmp,
	__in		const efx_mcdi_init_rxq_params_t *params)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_INIT_RXQ_V5_IN_LEN,
		MC_CMD_INIT_RXQ_V5_OUT_LEN);
	int npages = efx_rxq_nbufs(enp, ndescs);
	int i;
	efx_qword_t *dma_addr;
	uint64_t addr;
	efx_rc_t rc;
	uint32_t dma_mode;
	boolean_t want_outer_classes;
	boolean_t no_cont_ev;

	EFSYS_ASSERT3U(ndescs, <=, encp->enc_rxq_max_ndescs);

	if ((esmp == NULL) ||
	    (EFSYS_MEM_SIZE(esmp) < efx_rxq_size(enp, ndescs))) {
		rc = EINVAL;
		goto fail1;
	}

	no_cont_ev = (eep->ee_flags & EFX_EVQ_FLAGS_NO_CONT_EV);
	if ((no_cont_ev == B_TRUE) && (params->disable_scatter == B_FALSE)) {
		/* TODO: Support scatter in NO_CONT_EV mode */
		rc = EINVAL;
		goto fail2;
	}

	if (params->ps_buf_size > 0)
		dma_mode = MC_CMD_INIT_RXQ_EXT_IN_PACKED_STREAM;
	else if (params->es_bufs_per_desc > 0)
		dma_mode = MC_CMD_INIT_RXQ_V3_IN_EQUAL_STRIDE_SUPER_BUFFER;
	else
		dma_mode = MC_CMD_INIT_RXQ_EXT_IN_SINGLE_PACKET;

	if (encp->enc_tunnel_encapsulations_supported != 0 &&
	    !params->want_inner_classes) {
		/*
		 * WANT_OUTER_CLASSES can only be specified on hardware which
		 * supports tunnel encapsulation offloads, even though it is
		 * effectively the behaviour the hardware gives.
		 *
		 * Also, on hardware which does support such offloads, older
		 * firmware rejects the flag if the offloads are not supported
		 * by the current firmware variant, which means this may fail if
		 * the capabilities are not updated when the firmware variant
		 * changes. This is not an issue on newer firmware, as it was
		 * changed in bug 69842 (v6.4.2.1007) to permit this flag to be
		 * specified on all firmware variants.
		 */
		want_outer_classes = B_TRUE;
	} else {
		want_outer_classes = B_FALSE;
	}

	req.emr_cmd = MC_CMD_INIT_RXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_INIT_RXQ_V5_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_INIT_RXQ_V5_OUT_LEN;

	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_SIZE, ndescs);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_TARGET_EVQ, eep->ee_index);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_LABEL, label);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_INSTANCE, instance);
	MCDI_IN_POPULATE_DWORD_10(req, INIT_RXQ_EXT_IN_FLAGS,
	    INIT_RXQ_EXT_IN_FLAG_BUFF_MODE, 0,
	    INIT_RXQ_EXT_IN_FLAG_HDR_SPLIT, 0,
	    INIT_RXQ_EXT_IN_FLAG_TIMESTAMP, 0,
	    INIT_RXQ_EXT_IN_CRC_MODE, 0,
	    INIT_RXQ_EXT_IN_FLAG_PREFIX, 1,
	    INIT_RXQ_EXT_IN_FLAG_DISABLE_SCATTER, params->disable_scatter,
	    INIT_RXQ_EXT_IN_DMA_MODE,
	    dma_mode,
	    INIT_RXQ_EXT_IN_PACKED_STREAM_BUFF_SIZE, params->ps_buf_size,
	    INIT_RXQ_EXT_IN_FLAG_WANT_OUTER_CLASSES, want_outer_classes,
	    INIT_RXQ_EXT_IN_FLAG_NO_CONT_EV, no_cont_ev);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_OWNER_ID, 0);
	MCDI_IN_SET_DWORD(req, INIT_RXQ_EXT_IN_PORT_ID, enp->en_vport_id);

	if (params->es_bufs_per_desc > 0) {
		MCDI_IN_SET_DWORD(req,
		    INIT_RXQ_V3_IN_ES_PACKET_BUFFERS_PER_BUCKET,
		    params->es_bufs_per_desc);
		MCDI_IN_SET_DWORD(req,
		    INIT_RXQ_V3_IN_ES_MAX_DMA_LEN, params->es_max_dma_len);
		MCDI_IN_SET_DWORD(req,
		    INIT_RXQ_V3_IN_ES_PACKET_STRIDE, params->es_buf_stride);
		MCDI_IN_SET_DWORD(req,
		    INIT_RXQ_V3_IN_ES_HEAD_OF_LINE_BLOCK_TIMEOUT,
		    params->hol_block_timeout);
	}

	if (encp->enc_init_rxq_with_buffer_size)
		MCDI_IN_SET_DWORD(req, INIT_RXQ_V4_IN_BUFFER_SIZE_BYTES,
		    params->buf_size);

	MCDI_IN_SET_DWORD(req, INIT_RXQ_V5_IN_RX_PREFIX_ID, params->prefix_id);

	dma_addr = MCDI_IN2(req, efx_qword_t, INIT_RXQ_IN_DMA_ADDR);
	addr = EFSYS_MEM_ADDR(esmp);

	for (i = 0; i < npages; i++) {
		EFX_POPULATE_QWORD_2(*dma_addr,
		    EFX_DWORD_1, (uint32_t)(addr >> 32),
		    EFX_DWORD_0, (uint32_t)(addr & 0xffffffff));

		dma_addr++;
		addr += EFX_BUF_SIZE;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_fini_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_FINI_RXQ_IN_LEN,
		MC_CMD_FINI_RXQ_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_FINI_RXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FINI_RXQ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FINI_RXQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FINI_RXQ_IN_INSTANCE, instance);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	/*
	 * EALREADY is not an error, but indicates that the MC has rebooted and
	 * that the RXQ has already been destroyed.
	 */
	if (rc != EALREADY)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_init_txq(
	__in		efx_nic_t *enp,
	__in		uint32_t ndescs,
	__in		uint32_t target_evq,
	__in		uint32_t label,
	__in		uint32_t instance,
	__in		uint16_t flags,
	__in		efsys_mem_t *esmp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_INIT_TXQ_EXT_IN_LEN,
		MC_CMD_INIT_TXQ_OUT_LEN);
	efx_qword_t *dma_addr;
	uint64_t addr;
	int npages;
	int i;
	efx_rc_t rc;

	EFSYS_ASSERT(MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_MAXNUM >=
	    efx_txq_nbufs(enp, enp->en_nic_cfg.enc_txq_max_ndescs));

	if ((esmp == NULL) ||
	    (EFSYS_MEM_SIZE(esmp) < efx_txq_size(enp, ndescs))) {
		rc = EINVAL;
		goto fail1;
	}

	npages = efx_txq_nbufs(enp, ndescs);
	if (MC_CMD_INIT_TXQ_IN_LEN(npages) > sizeof (payload)) {
		rc = EINVAL;
		goto fail2;
	}

	req.emr_cmd = MC_CMD_INIT_TXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_INIT_TXQ_IN_LEN(npages);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_INIT_TXQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_SIZE, ndescs);
	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_TARGET_EVQ, target_evq);
	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_LABEL, label);
	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_INSTANCE, instance);

	MCDI_IN_POPULATE_DWORD_9(req, INIT_TXQ_IN_FLAGS,
	    INIT_TXQ_IN_FLAG_BUFF_MODE, 0,
	    INIT_TXQ_IN_FLAG_IP_CSUM_DIS,
	    (flags & EFX_TXQ_CKSUM_IPV4) ? 0 : 1,
	    INIT_TXQ_IN_FLAG_TCP_CSUM_DIS,
	    (flags & EFX_TXQ_CKSUM_TCPUDP) ? 0 : 1,
	    INIT_TXQ_EXT_IN_FLAG_INNER_IP_CSUM_EN,
	    (flags & EFX_TXQ_CKSUM_INNER_IPV4) ? 1 : 0,
	    INIT_TXQ_EXT_IN_FLAG_INNER_TCP_CSUM_EN,
	    (flags & EFX_TXQ_CKSUM_INNER_TCPUDP) ? 1 : 0,
	    INIT_TXQ_EXT_IN_FLAG_TSOV2_EN, (flags & EFX_TXQ_FATSOV2) ? 1 : 0,
	    INIT_TXQ_IN_FLAG_TCP_UDP_ONLY, 0,
	    INIT_TXQ_IN_CRC_MODE, 0,
	    INIT_TXQ_IN_FLAG_TIMESTAMP, 0);

	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_OWNER_ID, 0);
	MCDI_IN_SET_DWORD(req, INIT_TXQ_IN_PORT_ID, enp->en_vport_id);

	dma_addr = MCDI_IN2(req, efx_qword_t, INIT_TXQ_IN_DMA_ADDR);
	addr = EFSYS_MEM_ADDR(esmp);

	for (i = 0; i < npages; i++) {
		EFX_POPULATE_QWORD_2(*dma_addr,
		    EFX_DWORD_1, (uint32_t)(addr >> 32),
		    EFX_DWORD_0, (uint32_t)(addr & 0xffffffff));

		dma_addr++;
		addr += EFX_BUF_SIZE;
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_fini_txq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_FINI_TXQ_IN_LEN,
		MC_CMD_FINI_TXQ_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_FINI_TXQ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FINI_TXQ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FINI_TXQ_OUT_LEN;

	MCDI_IN_SET_DWORD(req, FINI_TXQ_IN_INSTANCE, instance);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	/*
	 * EALREADY is not an error, but indicates that the MC has rebooted and
	 * that the TXQ has already been destroyed.
	 */
	if (rc != EALREADY)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */

	__checkReturn	efx_rc_t
efx_mcdi_get_nic_addr_info(
	__in		efx_nic_t *enp,
	__out		uint32_t *mapping_typep)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_DESC_ADDR_INFO_IN_LEN,
		MC_CMD_GET_DESC_ADDR_INFO_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_GET_DESC_ADDR_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_DESC_ADDR_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_DESC_ADDR_INFO_OUT_LEN;

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_DESC_ADDR_INFO_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*mapping_typep =
	    MCDI_OUT_DWORD(req, GET_DESC_ADDR_INFO_OUT_MAPPING_TYPE);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_get_nic_addr_regions(
	__in		efx_nic_t *enp,
	__out		efx_nic_dma_region_info_t *endrip)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_DESC_ADDR_REGIONS_IN_LEN,
		MC_CMD_GET_DESC_ADDR_REGIONS_OUT_LENMAX_MCDI2);
	efx_xword_t *regions;
	efx_mcdi_req_t req;
	efx_rc_t rc;
	size_t alloc_size;
	unsigned int nregions;
	unsigned int i;

	req.emr_cmd = MC_CMD_GET_DESC_ADDR_REGIONS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_DESC_ADDR_REGIONS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_DESC_ADDR_REGIONS_OUT_LENMAX_MCDI2;

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used <
	    MC_CMD_GET_DESC_ADDR_REGIONS_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	nregions = MC_CMD_GET_DESC_ADDR_REGIONS_OUT_REGIONS_NUM(
	    req.emr_out_length_used);

	EFX_STATIC_ASSERT(sizeof (*regions) == DESC_ADDR_REGION_LEN);
	regions = MCDI_OUT2(req, efx_xword_t,
	    GET_DESC_ADDR_REGIONS_OUT_REGIONS);

	alloc_size = nregions * sizeof(endrip->endri_regions[0]);
	if (alloc_size / sizeof (endrip->endri_regions[0]) != nregions) {
		rc = ENOMEM;
		goto fail3;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip,
	    alloc_size,
	    endrip->endri_regions);
	if (endrip->endri_regions == NULL) {
		rc = ENOMEM;
		goto fail4;
	}

	endrip->endri_count = nregions;
	for (i = 0; i < nregions; ++i) {
		efx_nic_dma_region_t *region_info;

		region_info = &endrip->endri_regions[i];

		region_info->endr_inuse = B_FALSE;

		region_info->endr_nic_base =
		    MCDI_OUT_INDEXED_MEMBER_QWORD(req,
		        GET_DESC_ADDR_REGIONS_OUT_REGIONS, i,
		        DESC_ADDR_REGION_DESC_ADDR_BASE);

		region_info->endr_trgt_base =
		    MCDI_OUT_INDEXED_MEMBER_QWORD(req,
		        GET_DESC_ADDR_REGIONS_OUT_REGIONS, i,
		        DESC_ADDR_REGION_TRGT_ADDR_BASE);

		region_info->endr_window_log2 =
		    MCDI_OUT_INDEXED_MEMBER_DWORD(req,
		        GET_DESC_ADDR_REGIONS_OUT_REGIONS, i,
		        DESC_ADDR_REGION_WINDOW_SIZE_LOG2);

		region_info->endr_align_log2 =
		    MCDI_OUT_INDEXED_MEMBER_DWORD(req,
		        GET_DESC_ADDR_REGIONS_OUT_REGIONS, i,
		        DESC_ADDR_REGION_TRGT_ADDR_ALIGN_LOG2);
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_set_nic_addr_regions(
	__in		efx_nic_t *enp,
	__in		const efx_nic_dma_region_info_t *endrip)
{
	EFX_MCDI_DECLARE_BUF(payload,
		MC_CMD_SET_DESC_ADDR_REGIONS_IN_LENMAX_MCDI2,
		MC_CMD_SET_DESC_ADDR_REGIONS_OUT_LEN);
	efx_qword_t *trgt_addr_base;
	efx_mcdi_req_t req;
	unsigned int i;
	efx_rc_t rc;

	if (endrip->endri_count >
	    MC_CMD_SET_DESC_ADDR_REGIONS_IN_TRGT_ADDR_BASE_MAXNUM) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_SET_DESC_ADDR_REGIONS;
	req.emr_in_buf = payload;
	req.emr_in_length =
	    MC_CMD_SET_DESC_ADDR_REGIONS_IN_LEN(endrip->endri_count);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_DESC_ADDR_REGIONS_OUT_LEN;

	EFX_STATIC_ASSERT(sizeof (*trgt_addr_base) ==
	    MC_CMD_SET_DESC_ADDR_REGIONS_IN_TRGT_ADDR_BASE_LEN);
	trgt_addr_base = MCDI_OUT2(req, efx_qword_t,
	    SET_DESC_ADDR_REGIONS_IN_TRGT_ADDR_BASE);

	for (i = 0; i < endrip->endri_count; ++i) {
		const efx_nic_dma_region_t *region_info;

		region_info = &endrip->endri_regions[i];

		if (region_info->endr_inuse != B_TRUE)
			continue;

		EFX_STATIC_ASSERT(sizeof (1U) * 8 >=
		    MC_CMD_SET_DESC_ADDR_REGIONS_IN_TRGT_ADDR_BASE_MAXNUM);
		MCDI_IN_SET_DWORD(req,
		    SET_DESC_ADDR_REGIONS_IN_SET_REGION_MASK, 1U << i);

		MCDI_IN_SET_INDEXED_QWORD(req,
		    SET_DESC_ADDR_REGIONS_IN_TRGT_ADDR_BASE, i,
		    region_info->endr_trgt_base);
	}

	efx_mcdi_execute_quiet(enp, &req);

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

#endif	/* EFSYS_OPT_MCDI */
