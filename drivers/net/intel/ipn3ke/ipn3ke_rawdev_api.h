/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_RAWDEV_API_H_
#define _IFPGA_RAWDEV_API_H_

#include <rte_ether.h>

enum ifpga_rawdev_retimer_media_type {
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_UNKNOWN = 0,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_100GBASE_LR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_100GBASE_SR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_100GBASE_CR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_40GBASE_LR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_400GBASE_SR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_40GBASE_CR4,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_25GBASE_SR,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_25GBASE_CR,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_10GBASE_LR,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_10GBASE_SR,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_10GBASE_DAC,
	IFPGA_RAWDEV_RETIMER_MEDIA_TYPE_DEFAULT
};

enum ifpga_rawdev_retimer_mac_type {
	IFPGA_RAWDEV_RETIMER_MAC_TYPE_UNKNOWN = 0,
	IFPGA_RAWDEV_RETIMER_MAC_TYPE_100GE_CAUI,
	IFPGA_RAWDEVG_RETIMER_MAC_TYPE_40GE_XLAUI,
	IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI,
	IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI,
	IFPGA_RAWDEV_RETIMER_MAC_TYPE_DEFAULT
};

#define IFPGA_RAWDEV_LINK_SPEED_10GB_SHIFT    0x0
#define IFPGA_RAWDEV_LINK_SPEED_40GB_SHIFT    0x1
#define IFPGA_RAWDEV_LINK_SPEED_25GB_SHIFT    0x2

enum ifpga_rawdev_link_speed {
	IFPGA_RAWDEV_LINK_SPEED_UNKNOWN = 0,
	IFPGA_RAWDEV_LINK_SPEED_10GB =
		(1 << IFPGA_RAWDEV_LINK_SPEED_10GB_SHIFT),
	IFPGA_RAWDEV_LINK_SPEED_40GB =
		(1 << IFPGA_RAWDEV_LINK_SPEED_40GB_SHIFT),
	IFPGA_RAWDEV_LINK_SPEED_25GB =
		(1 << IFPGA_RAWDEV_LINK_SPEED_25GB_SHIFT),
};

struct ifpga_rawdevg_retimer_info {
	int retimer_num;
	int port_num;
	enum ifpga_rawdev_retimer_media_type media_type;
	enum ifpga_rawdev_retimer_mac_type mac_type;
};

struct ifpga_rawdevg_link_info {
	int port;
	int link_up;
	enum ifpga_rawdev_link_speed link_speed;
};

struct ipn3ke_pub_func {
	struct ifpga_rawdev *(*get_ifpga_rawdev)(const struct rte_rawdev *rdv);
	int (*set_i40e_sw_dev)(uint16_t port_id, struct rte_eth_dev *sw_dev);
};

/**
 * @internal
 * The publid functions of bridge PAC N3000 FPGA and I40e.
 */
extern struct ipn3ke_pub_func ipn3ke_bridge_func;


#endif /* _IFPGA_RAWDEV_H_ */
