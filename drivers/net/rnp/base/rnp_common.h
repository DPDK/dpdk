/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_COMMON_H_
#define _RNP_COMMON_H_

#define RNP_NIC_RESET		_NIC_(0x0010)
#define RNP_GET_MAC_HI(macaddr) (((macaddr[0]) << 8) | (macaddr[1]))
#define RNP_GET_MAC_LO(macaddr) \
	((macaddr[2] << 24) | (macaddr[3] << 16) | \
	 ((macaddr[4] << 8)) | (macaddr[5]))
int rnp_init_hw(struct rnp_hw *hw);
int rnp_setup_common_ops(struct rnp_hw *hw);

#endif /* _RNP_COMMON_H_ */
