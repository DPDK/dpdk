/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_fw_cmd.h"

static void
rnp_build_phy_abalities_req(struct rnp_mbx_fw_cmd_req *req,
			    struct rnp_fw_req_arg *req_arg __rte_unused,
			    void *cookie)
{
	struct rnp_get_phy_ablity *arg = (struct rnp_get_phy_ablity *)req->data;

	req->flags = 0;
	req->opcode = RNP_GET_PHY_ABALITY;
	req->datalen = sizeof(*arg);
	req->cookie = cookie;
	req->reply_lo = 0;
	req->reply_hi = 0;

	arg->requester = RNP_REQUEST_BY_DPDK;
}

static void
rnp_build_reset_phy_req(struct rnp_mbx_fw_cmd_req *req,
			void *cookie)
{
	req->flags = 0;
	req->opcode = RNP_RESET_PHY;
	req->datalen = 0;
	req->reply_lo = 0;
	req->reply_hi = 0;
	req->cookie = cookie;
}

static void
rnp_build_get_macaddress_req(struct rnp_mbx_fw_cmd_req *req,
			     struct rnp_fw_req_arg *req_arg,
			     void *cookie)
{
	struct rnp_mac_addr_req *arg = (struct rnp_mac_addr_req *)req->data;

	req->flags = 0;
	req->opcode = RNP_GET_MAC_ADDRESS;
	req->datalen = sizeof(*arg);
	req->cookie = cookie;
	req->reply_lo = 0;
	req->reply_hi = 0;

	arg->lane_mask = RTE_BIT32(req_arg->param0);
	arg->pfvf_num = req_arg->param1;
}

static inline void
rnp_build_get_lane_status_req(struct rnp_mbx_fw_cmd_req *req,
			      struct rnp_fw_req_arg *req_arg,
			      void *cookie)
{
	struct rnp_get_lane_st_req *arg = (struct rnp_get_lane_st_req *)req->data;

	req->flags = 0;
	req->opcode = RNP_GET_LANE_STATUS;
	req->datalen = sizeof(*arg);
	req->cookie = cookie;
	req->reply_lo = 0;
	req->reply_hi = 0;

	arg->nr_lane = req_arg->param0;
}

int rnp_build_fwcmd_req(struct rnp_mbx_fw_cmd_req *req,
			struct rnp_fw_req_arg *arg,
			void *cookie)
{
	int err = 0;

	switch (arg->opcode) {
	case RNP_GET_PHY_ABALITY:
		rnp_build_phy_abalities_req(req, arg, cookie);
		break;
	case RNP_RESET_PHY:
		rnp_build_reset_phy_req(req, cookie);
		break;
	case RNP_GET_MAC_ADDRESS:
		rnp_build_get_macaddress_req(req, arg, cookie);
		break;
	case RNP_GET_LANE_STATUS:
		rnp_build_get_lane_status_req(req, arg, cookie);
		break;
	default:
		err = -EOPNOTSUPP;
	}

	return err;
}
