/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019 Solarflare Communications Inc.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER

	__checkReturn	efx_rc_t
ef10_proxy_auth_init(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);

	return (0);
}

			void
ef10_proxy_auth_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);
}

static	__checkReturn	efx_rc_t
efx_mcdi_proxy_configure(
	__in		efx_nic_t *enp,
	__in		boolean_t disable_proxy,
	__in		uint64_t req_buffer_addr,
	__in		uint64_t resp_buffer_addr,
	__in		uint64_t stat_buffer_addr,
	__in		size_t req_size,
	__in		size_t resp_size,
	__in		uint32_t block_cnt,
	__in		uint8_t *op_maskp,
	__in		size_t op_mask_size)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_PROXY_CONFIGURE_EXT_IN_LEN,
		MC_CMD_PROXY_CONFIGURE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_PROXY_CONFIGURE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PROXY_CONFIGURE_EXT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PROXY_CONFIGURE_OUT_LEN;

	if (!disable_proxy) {
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_FLAGS, 1);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REQUEST_BUFF_ADDR_LO,
			req_buffer_addr & 0xffffffff);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REQUEST_BUFF_ADDR_HI,
			req_buffer_addr >> 32);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REPLY_BUFF_ADDR_LO,
			resp_buffer_addr & 0xffffffff);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REPLY_BUFF_ADDR_HI,
			resp_buffer_addr >> 32);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_STATUS_BUFF_ADDR_LO,
			stat_buffer_addr & 0xffffffff);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_STATUS_BUFF_ADDR_HI,
			stat_buffer_addr >> 32);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REQUEST_BLOCK_SIZE,
			req_size);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_REPLY_BLOCK_SIZE,
			resp_size);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_STATUS_BLOCK_SIZE,
			MC_PROXY_STATUS_BUFFER_LEN);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_IN_NUM_BLOCKS,
			block_cnt);
		memcpy(MCDI_IN2(req, efx_byte_t,
				PROXY_CONFIGURE_IN_ALLOWED_MCDI_MASK),
			op_maskp, op_mask_size);
		MCDI_IN_SET_DWORD(req, PROXY_CONFIGURE_EXT_IN_RESERVED,
			EFX_PROXY_CONFIGURE_MAGIC);
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

static	__checkReturn	efx_rc_t
efx_mcdi_privilege_modify(
	__in		efx_nic_t *enp,
	__in		uint32_t fn_group,
	__in		uint32_t pf_index,
	__in		uint32_t vf_index,
	__in		uint32_t add_privileges_mask,
	__in		uint32_t remove_privileges_mask)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_PRIVILEGE_MODIFY_IN_LEN,
		MC_CMD_PRIVILEGE_MODIFY_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_PRIVILEGE_MODIFY;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PRIVILEGE_MODIFY_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PRIVILEGE_MODIFY_OUT_LEN;

	EFSYS_ASSERT(fn_group <= MC_CMD_PRIVILEGE_MODIFY_IN_ONE);

	MCDI_IN_SET_DWORD(req, PRIVILEGE_MODIFY_IN_FN_GROUP, fn_group);

	if ((fn_group == MC_CMD_PRIVILEGE_MODIFY_IN_ONE) ||
	    (fn_group == MC_CMD_PRIVILEGE_MODIFY_IN_VFS_OF_PF)) {
		MCDI_IN_POPULATE_DWORD_2(req,
		    PRIVILEGE_MODIFY_IN_FUNCTION,
		    PRIVILEGE_MODIFY_IN_FUNCTION_PF, pf_index,
		    PRIVILEGE_MODIFY_IN_FUNCTION_VF, vf_index);
	}

	MCDI_IN_SET_DWORD(req, PRIVILEGE_MODIFY_IN_ADD_MASK,
		add_privileges_mask);
	MCDI_IN_SET_DWORD(req, PRIVILEGE_MODIFY_IN_REMOVE_MASK,
		remove_privileges_mask);

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

static	__checkReturn			efx_rc_t
efx_proxy_auth_fill_op_mask(
	__in_ecount(op_count)		uint32_t *op_listp,
	__in				size_t op_count,
	__out_ecount(op_mask_size)	uint32_t *op_maskp,
	__in				size_t op_mask_size)
{
	efx_rc_t rc;
	uint32_t op;

	if ((op_listp == NULL) || (op_maskp == NULL)) {
		rc = EINVAL;
		goto fail1;
	}

	while (op_count--) {
		op = *op_listp++;
		if (op > op_mask_size * 32) {
			rc = EINVAL;
			goto fail2;
		}
		op_maskp[op / 32] |= 1u << (op & 31);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
ef10_proxy_auth_mc_config(
	__in			efx_nic_t *enp,
	__in_ecount(block_cnt)	efsys_mem_t *request_bufferp,
	__in_ecount(block_cnt)	efsys_mem_t *response_bufferp,
	__in_ecount(block_cnt)	efsys_mem_t *status_bufferp,
	__in			uint32_t block_cnt,
	__in_ecount(op_count)	uint32_t *op_listp,
	__in			size_t op_count)
{
#define	PROXY_OPS_MASK_SIZE						\
	(EFX_DIV_ROUND_UP(						\
	    MC_CMD_PROXY_CONFIGURE_IN_ALLOWED_MCDI_MASK_LEN,		\
	    sizeof (uint32_t)))

	efx_rc_t rc;
	uint32_t op_mask[PROXY_OPS_MASK_SIZE] = {0};

	/* Prepare the operation mask from operation list array */
	if ((rc = efx_proxy_auth_fill_op_mask(op_listp, op_count,
			op_mask, PROXY_OPS_MASK_SIZE) != 0))
		goto fail1;

	if ((rc = efx_mcdi_proxy_configure(enp, B_FALSE,
			EFSYS_MEM_ADDR(request_bufferp),
			EFSYS_MEM_ADDR(response_bufferp),
			EFSYS_MEM_ADDR(status_bufferp),
			EFSYS_MEM_SIZE(request_bufferp) / block_cnt,
			EFSYS_MEM_SIZE(response_bufferp) / block_cnt,
			block_cnt, (uint8_t *)&op_mask,
			sizeof (op_mask))) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
ef10_proxy_auth_disable(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_proxy_configure(enp, B_TRUE,
			0, 0, 0, 0, 0, 0, NULL, 0) != 0))
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
ef10_proxy_auth_privilege_modify(
	__in		efx_nic_t *enp,
	__in		uint32_t fn_group,
	__in		uint32_t pf_index,
	__in		uint32_t vf_index,
	__in		uint32_t add_privileges_mask,
	__in		uint32_t remove_privileges_mask)
{
	return (efx_mcdi_privilege_modify(enp, fn_group, pf_index, vf_index,
			add_privileges_mask, remove_privileges_mask));
}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
