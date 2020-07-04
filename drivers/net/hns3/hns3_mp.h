/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#ifndef _HNS3_MP_H_
#define _HNS3_MP_H_

void hns3_mp_req_start_rxtx(struct rte_eth_dev *dev);
void hns3_mp_req_stop_rxtx(struct rte_eth_dev *dev);
int hns3_mp_init_primary(void);
void hns3_mp_uninit_primary(void);
int hns3_mp_init_secondary(void);

#endif /* _HNS3_MP_H_ */
