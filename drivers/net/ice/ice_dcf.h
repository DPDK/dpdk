/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _ICE_DCF_H_
#define _ICE_DCF_H_

#include <rte_ethdev_driver.h>

#include <iavf_prototype.h>
#include <iavf_adminq_cmd.h>
#include <iavf_type.h>

#include "ice_logs.h"

struct dcf_virtchnl_cmd {
	TAILQ_ENTRY(dcf_virtchnl_cmd) next;

	enum virtchnl_ops v_op;
	enum iavf_status v_ret;

	uint16_t req_msglen;
	uint8_t *req_msg;

	uint16_t rsp_msglen;
	uint16_t rsp_buflen;
	uint8_t *rsp_msgbuf;

	volatile int pending;
};

struct ice_dcf_hw {
	struct iavf_hw avf;

	rte_spinlock_t vc_cmd_send_lock;
	rte_spinlock_t vc_cmd_queue_lock;
	TAILQ_HEAD(, dcf_virtchnl_cmd) vc_cmd_queue;
	uint8_t *arq_buf;

	struct virtchnl_version_info virtchnl_version;
	struct virtchnl_vf_resource *vf_res; /* VF resource */
	struct virtchnl_vsi_resource *vsi_res; /* LAN VSI */
	uint16_t vsi_id;
};

int ice_dcf_execute_virtchnl_cmd(struct ice_dcf_hw *hw,
				 struct dcf_virtchnl_cmd *cmd);

int ice_dcf_init_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw);
void ice_dcf_uninit_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw);

#endif /* _ICE_DCF_H_ */
