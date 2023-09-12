/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "cpfl_ethdev.h"
#include <idpf_common_virtchnl.h>

int
cpfl_cc_vport_list_get(struct cpfl_adapter_ext *adapter,
		       struct cpfl_vport_id *vi,
		       struct cpchnl2_get_vport_list_response *response)
{
	struct cpchnl2_get_vport_list_request request;
	struct idpf_cmd_info args;
	int err;

	memset(&request, 0, sizeof(request));
	request.func_type = vi->func_type;
	request.pf_id = vi->pf_id;
	request.vf_id = vi->vf_id;

	memset(&args, 0, sizeof(args));
	args.ops = CPCHNL2_OP_GET_VPORT_LIST;
	args.in_args = (uint8_t *)&request;
	args.in_args_size = sizeof(struct cpchnl2_get_vport_list_request);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to execute command of CPCHNL2_OP_GET_VPORT_LIST");
		return err;
	}

	rte_memcpy(response, args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);

	return 0;
}

int
cpfl_cc_vport_info_get(struct cpfl_adapter_ext *adapter,
		       struct cpchnl2_vport_id *vport_id,
		       struct cpfl_vport_id *vi,
		       struct cpchnl2_get_vport_info_response *response)
{
	struct cpchnl2_get_vport_info_request request;
	struct idpf_cmd_info args;
	int err;

	request.vport.vport_id = vport_id->vport_id;
	request.vport.vport_type = vport_id->vport_type;
	request.func.func_type = vi->func_type;
	request.func.pf_id = vi->pf_id;
	request.func.vf_id = vi->vf_id;

	memset(&args, 0, sizeof(args));
	args.ops = CPCHNL2_OP_GET_VPORT_INFO;
	args.in_args = (uint8_t *)&request;
	args.in_args_size = sizeof(struct cpchnl2_get_vport_info_request);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to execute command of CPCHNL2_OP_GET_VPORT_INFO");
		return err;
	}

	rte_memcpy(response, args.out_buffer, sizeof(*response));

	return 0;
}
