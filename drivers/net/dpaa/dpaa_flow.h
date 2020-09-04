/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017,2019 NXP
 */

#ifndef __DPAA_FLOW_H__
#define __DPAA_FLOW_H__

int dpaa_fm_init(void);
int dpaa_fm_term(void);
int dpaa_fm_config(struct rte_eth_dev *dev, uint64_t req_dist_set);
int dpaa_fm_deconfig(struct dpaa_if *dpaa_intf, struct fman_if *fif);
void dpaa_write_fm_config_to_file(void);

#endif
