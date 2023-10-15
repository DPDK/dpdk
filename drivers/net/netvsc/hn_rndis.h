/* SPDX-License-Identifier: BSD-3-Clause */

#include "rndis.h"

struct hn_data;

void hn_rndis_receive_response(struct hn_data *hv,
			      const void *data, uint32_t len);
void	hn_rndis_link_status(struct rte_eth_dev *dev, const void *msg);
int	hn_rndis_attach(struct hn_data *hv);
void	hn_rndis_detach(struct hn_data *hv);
int	hn_rndis_get_eaddr(struct hn_data *hv, uint8_t *eaddr);
int	hn_rndis_get_mtu(struct hn_data *hv, uint32_t *mtu);
int	hn_rndis_get_linkstatus(struct hn_data *hv);
int	hn_rndis_get_linkspeed(struct hn_data *hv);
int	hn_rndis_set_rxfilter(struct hn_data *hv, uint32_t filter);
void	hn_rndis_rx_ctrl(struct hn_data *hv, const void *data,
			 int dlen);
int	hn_rndis_get_offload(struct hn_data *hv,
			     struct rte_eth_dev_info *dev_info);
int	hn_rndis_conf_offload(struct hn_data *hv,
			      uint64_t tx_offloads,
			      uint64_t rx_offloads);
int	hn_rndis_query_rsscaps(struct hn_data *hv,
			       unsigned int *rxr_cnt0);
int	hn_rndis_query_rss(struct hn_data *hv,
			   struct rte_eth_rss_conf *rss_conf);
int	hn_rndis_conf_rss(struct hn_data *hv, uint32_t flags);
uint32_t hn_rndis_get_ptypes(struct hn_data *hv);

#ifdef RTE_LIBRTE_NETVSC_DEBUG_DUMP
void hn_rndis_dump(const void *buf);
#else
#define hn_rndis_dump(buf)
#endif
