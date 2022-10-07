/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017,2019,2022 NXP
 */

#ifndef __DPAA_FLOW_H__
#define __DPAA_FLOW_H__

int dpaa_fm_init(void);
int dpaa_fm_term(void);
int dpaa_fm_config(struct rte_eth_dev *dev, uint64_t req_dist_set);
int dpaa_fm_deconfig(struct dpaa_if *dpaa_intf, struct fman_if *fif);
void dpaa_write_fm_config_to_file(void);
int dpaa_port_vsp_update(struct dpaa_if *dpaa_intf,
	bool fmc_mode, uint8_t vsp_id, uint32_t bpid, struct fman_if *fif,
	u32 mbuf_data_room_size);
int dpaa_port_vsp_cleanup(struct dpaa_if *dpaa_intf, struct fman_if *fif);
int dpaa_port_fmc_init(struct fman_if *fif,
		       uint32_t *fqids, int8_t *vspids, int max_nb_rxq);

#endif
