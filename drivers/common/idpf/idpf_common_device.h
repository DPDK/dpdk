/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_DEVICE_H_
#define _IDPF_COMMON_DEVICE_H_

#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>

struct idpf_adapter {
	struct idpf_hw hw;
	struct virtchnl2_version_info virtchnl_version;
	struct virtchnl2_get_capabilities caps;
	volatile uint32_t pend_cmd; /* pending command not finished */
	uint32_t cmd_retval; /* return value of the cmd response from cp */
	uint8_t *mbx_resp; /* buffer to store the mailbox response from cp */
};

#endif /* _IDPF_COMMON_DEVICE_H_ */
