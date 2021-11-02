/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_MP_H_
#define _HNS3_MP_H_

/* Local data for primary or secondary process. */
struct hns3_process_local_data {
	bool init_done; /* Process action register completed flag. */
	int eth_dev_cnt; /* Ethdev count under the current process. */
};
extern struct hns3_process_local_data process_data;

void hns3_mp_req_start_rxtx(struct rte_eth_dev *dev);
void hns3_mp_req_stop_rxtx(struct rte_eth_dev *dev);
int hns3_mp_init_primary(void);
void hns3_mp_uninit(void);
int hns3_mp_init_secondary(void);

#endif /* _HNS3_MP_H_ */
