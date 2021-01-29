/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX_EP_COMMON_H_
#define _OTX_EP_COMMON_H_

#define otx_ep_info(fmt, args...)				\
	rte_log(RTE_LOG_INFO, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

#define otx_ep_err(fmt, args...)				\
	rte_log(RTE_LOG_ERR, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

#define otx_ep_dbg(fmt, args...)				\
	rte_log(RTE_LOG_DEBUG, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

/* OTX_EP EP VF device data structure */
struct otx_ep_device {
	/* PCI device pointer */
	struct rte_pci_device *pdev;
	uint16_t chip_id;
	struct rte_eth_dev *eth_dev;
	int port_id;
	/* Memory mapped h/w address */
	uint8_t *hw_addr;
};

extern int otx_net_ep_logtype;
#endif  /* _OTX_EP_COMMON_H_ */
