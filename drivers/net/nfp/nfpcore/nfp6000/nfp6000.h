/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NFP6000_H__
#define __NFP_NFP6000_H__

#define NFP_ISL_EMEM0                   24

#define NFP_MU_ADDR_ACCESS_TYPE_MASK    3ULL
#define NFP_MU_ADDR_ACCESS_TYPE_DIRECT  2ULL

#define PUSHPULL(pull, push)       (((pull) << 4) | ((push) << 0))
#define PUSH_WIDTH(push_pull)      pushpull_width((push_pull) >> 0)
#define PULL_WIDTH(push_pull)      pushpull_width((push_pull) >> 4)

static inline int
pushpull_width(int pp)
{
	pp &= 0xf;
	if (pp == 0)
		return -EINVAL;

	return 2 << pp;
}


static inline int
nfp_cppat_mu_locality_lsb(int mode, int addr40)
{
	switch (mode) {
	case 0 ... 3:
		return addr40 ? 38 : 30;
	default:
		return -EINVAL;
	}
}

#endif /* NFP_NFP6000_H */
