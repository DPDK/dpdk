/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER

	__checkReturn	efx_rc_t
ef10_proxy_auth_init(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(EFX_FAMILY_IS_EF10(enp));

	return (0);
}

			void
ef10_proxy_auth_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(EFX_FAMILY_IS_EF10(enp));
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

static	__checkReturn	efx_rc_t
efx_mcdi_privilege_mask_set(
	__in		efx_nic_t *enp,
	__in		uint32_t vf_index,
	__in		uint32_t mask,
	__in		uint32_t value)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_PRIVILEGE_MASK_IN_LEN,
		MC_CMD_PRIVILEGE_MASK_OUT_LEN);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_req_t req;
	efx_rc_t rc;
	uint32_t old_mask = 0;
	uint32_t new_mask = 0;

	EFSYS_ASSERT((value & ~mask) == 0);

	req.emr_cmd = MC_CMD_PRIVILEGE_MASK;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PRIVILEGE_MASK_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PRIVILEGE_MASK_OUT_LEN;

	/* Get privilege mask */
	MCDI_IN_POPULATE_DWORD_2(req, PRIVILEGE_MASK_IN_FUNCTION,
		PRIVILEGE_MASK_IN_FUNCTION_PF, encp->enc_pf,
		PRIVILEGE_MASK_IN_FUNCTION_VF, vf_index);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used != MC_CMD_PRIVILEGE_MASK_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	old_mask = *MCDI_OUT2(req, uint32_t, PRIVILEGE_MASK_OUT_OLD_MASK);
	new_mask = old_mask & ~mask;
	new_mask |= (value & mask);

	if (new_mask == old_mask)
		return (0);

	new_mask |= MC_CMD_PRIVILEGE_MASK_IN_DO_CHANGE;
	memset(payload, 0, sizeof (payload));

	req.emr_cmd = MC_CMD_PRIVILEGE_MASK;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PRIVILEGE_MASK_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PRIVILEGE_MASK_OUT_LEN;

	/* Set privilege mask */
	MCDI_IN_SET_DWORD(req, PRIVILEGE_MASK_IN_NEW_MASK, new_mask);

	efx_mcdi_execute(enp, &req);
	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	if (req.emr_out_length_used != MC_CMD_PRIVILEGE_MASK_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail4;
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
ef10_proxy_auth_set_privilege_mask(
	__in		efx_nic_t *enp,
	__in		uint32_t vf_index,
	__in		uint32_t mask,
	__in		uint32_t value)
{
	return (efx_mcdi_privilege_mask_set(enp, vf_index,
			mask, value));
}

static	__checkReturn	efx_rc_t
efx_mcdi_proxy_complete(
	__in		efx_nic_t *enp,
	__in		uint32_t fn_index,
	__in		uint32_t proxy_result,
	__in		uint32_t handle)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_PROXY_COMPLETE_IN_LEN,
		MC_CMD_PROXY_COMPLETE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_PROXY_COMPLETE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_PROXY_COMPLETE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_PROXY_COMPLETE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, PROXY_COMPLETE_IN_BLOCK_INDEX, fn_index);
	MCDI_IN_SET_DWORD(req, PROXY_COMPLETE_IN_STATUS, proxy_result);
	MCDI_IN_SET_DWORD(req, PROXY_COMPLETE_IN_HANDLE, handle);

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

	__checkReturn	efx_rc_t
ef10_proxy_auth_complete_request(
	__in		efx_nic_t *enp,
	__in		uint32_t fn_index,
	__in		uint32_t proxy_result,
	__in		uint32_t handle)
{
	return (efx_mcdi_proxy_complete(enp, fn_index,
			proxy_result, handle));
}

static	__checkReturn			efx_rc_t
efx_mcdi_proxy_cmd(
	__in				efx_nic_t *enp,
	__in				uint32_t pf_index,
	__in				uint32_t vf_index,
	__in_bcount(request_size)	uint8_t *request_bufferp,
	__in				size_t request_size,
	__out_bcount(response_size)	uint8_t *response_bufferp,
	__in				size_t response_size,
	__out_opt			size_t *response_size_actualp)
{
	efx_dword_t *inbufp;
	efx_mcdi_req_t req;
	efx_rc_t rc;

	if (request_size % sizeof (*inbufp) != 0) {
		rc = EINVAL;
		goto fail1;
	}

	EFSYS_KMEM_ALLOC(enp, (MC_CMD_PROXY_CMD_IN_LEN + request_size), inbufp);

	req.emr_cmd = MC_CMD_PROXY_CMD;
	req.emr_in_buf = (uint8_t *) inbufp;
	req.emr_in_length = MC_CMD_PROXY_CMD_IN_LEN + request_size;
	req.emr_out_buf = response_bufferp;
	req.emr_out_length = response_size;

	MCDI_IN_POPULATE_DWORD_2(req, PROXY_CMD_IN_TARGET,
		 PROXY_CMD_IN_TARGET_PF, pf_index,
		 PROXY_CMD_IN_TARGET_VF, vf_index);

	/* Proxied command should be located just after PROXY_CMD */
	memcpy(&inbufp[MC_CMD_PROXY_CMD_IN_LEN / sizeof (*inbufp)],
		request_bufferp, request_size);

	efx_mcdi_execute(enp, &req);

	EFSYS_KMEM_FREE(enp, (MC_CMD_PROXY_CMD_IN_LEN + request_size), inbufp);
	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (response_size_actualp != NULL)
		*response_size_actualp = req.emr_out_length_used;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
ef10_proxy_auth_get_privilege_mask(
	__in		efx_nic_t *enp,
	__in		uint32_t pf_index,
	__in		uint32_t vf_index,
	__out		uint32_t *maskp)
{
	return (efx_mcdi_privilege_mask(enp, pf_index, vf_index, maskp));
}


	__checkReturn	efx_rc_t
ef10_proxy_auth_exec_cmd(
	__in		efx_nic_t *enp,
	__inout		efx_proxy_cmd_params_t *paramsp)
{
	return (efx_mcdi_proxy_cmd(enp, paramsp->pf_index, paramsp->vf_index,
			paramsp->request_bufferp, paramsp->request_size,
			paramsp->response_bufferp, paramsp->response_size,
			paramsp->response_size_actualp));
}
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
