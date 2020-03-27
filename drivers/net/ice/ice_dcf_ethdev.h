/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _ICE_DCF_ETHDEV_H_
#define _ICE_DCF_ETHDEV_H_

#include "ice_ethdev.h"
#include "ice_dcf.h"

#define ICE_DCF_MAX_RINGS  1

struct ice_dcf_queue {
	uint64_t dummy;
};

struct ice_dcf_adapter {
	struct ice_dcf_hw real_hw;
	struct rte_ether_addr mac_addr;
	struct ice_dcf_queue rxqs[ICE_DCF_MAX_RINGS];
	struct ice_dcf_queue txqs[ICE_DCF_MAX_RINGS];
};

#endif /* _ICE_DCF_ETHDEV_H_ */
