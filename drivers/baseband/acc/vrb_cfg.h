/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _VRB_CFG_H_
#define _VRB_CFG_H_

/**
 * @file vrb_cfg.h
 *
 * Functions for configuring VRB1 HW.
 * Configuration related to encoding/decoding is done through the
 * librte_bbdev library.
 */

/**
 * Configure a VRB1 device.
 *
 * @param dev_name
 *   The name of the device. This is the short form of PCI BDF, e.g. 00:01.0.
 *   It can also be retrieved for a bbdev device from the dev_name field in the
 *   rte_bbdev_info structure returned by rte_bbdev_info_get().
 * @param conf
 *   Configuration to apply to VRB1 HW.
 *
 * @return
 *   Zero on success, negative value on failure.
 */
int
vrb1_configure(const char *dev_name, struct rte_acc_conf *conf);

/**
 * Configure a VRB2 device.
 *
 * @param dev_name
 *   The name of the device. This is the short form of PCI BDF, e.g. 00:01.0.
 *   It can also be retrieved for a bbdev device from the dev_name field in the
 *   rte_bbdev_info structure returned by rte_bbdev_info_get().
 * @param conf
 *   Configuration to apply to VRB2 HW.
 *
 * @return
 *   Zero on success, negative value on failure.
 */
int
vrb2_configure(const char *dev_name, struct rte_acc_conf *conf);

#endif /* _VRB_CFG_H_ */
