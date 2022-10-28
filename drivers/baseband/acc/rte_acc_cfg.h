/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _RTE_ACC_CFG_H_
#define _RTE_ACC_CFG_H_

/**
 * @file rte_acc_cfg.h
 *
 * Functions for configuring ACC HW, exposed directly to applications.
 * Configuration related to encoding/decoding is done through the
 * librte_bbdev library.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#include <stdint.h>
#include <stdbool.h>
#include <rte_compat.h>
#include "rte_acc_common_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Configure a ACC device in PF mode notably for bbdev-test
 *
 * @param dev_name
 *   The name of the device. This is the short form of PCI BDF, e.g. 00:01.0.
 *   It can also be retrieved for a bbdev device from the dev_name field in the
 *   rte_bbdev_info structure returned by rte_bbdev_info_get().
 * @param conf
 *   Configuration to apply to ACC HW.
 *
 * @return
 *   Zero on success, negative value on failure.
 */
__rte_experimental
int
rte_acc_configure(const char *dev_name, struct rte_acc_conf *conf);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ACC_CFG_H_ */
