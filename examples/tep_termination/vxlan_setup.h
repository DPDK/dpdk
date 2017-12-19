/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef VXLAN_SETUP_H_
#define VXLAN_SETUP_H_

extern uint16_t nb_devices;
extern uint16_t udp_port;
extern uint8_t filter_idx;
extern uint16_t ports[RTE_MAX_ETHPORTS];
extern struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
extern uint32_t enable_stats;
extern struct device_statistics dev_statistics[MAX_DEVICES];
extern uint8_t rx_decap;
extern uint8_t tx_encap;

typedef int (*ol_port_configure_t)(uint16_t port,
				   struct rte_mempool *mbuf_pool);

typedef int (*ol_tunnel_setup_t)(struct vhost_dev *vdev,
				 struct rte_mbuf *m);

typedef void (*ol_tunnel_destroy_t)(struct vhost_dev *vdev);

typedef int (*ol_tx_handle_t)(uint16_t port_id, uint16_t queue_id,
			      struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

typedef int (*ol_rx_handle_t)(int vid, struct rte_mbuf **pkts,
			      uint32_t count);

typedef int (*ol_param_handle)(int vid);

struct ol_switch_ops {
	ol_port_configure_t        port_configure;
	ol_tunnel_setup_t          tunnel_setup;
	ol_tunnel_destroy_t        tunnel_destroy;
	ol_tx_handle_t             tx_handle;
	ol_rx_handle_t             rx_handle;
	ol_param_handle            param_handle;
};

int
vxlan_port_init(uint16_t port, struct rte_mempool *mbuf_pool);

int
vxlan_link(struct vhost_dev *vdev, struct rte_mbuf *m);

void
vxlan_unlink(struct vhost_dev *vdev);

int
vxlan_tx_pkts(uint16_t port_id, uint16_t queue_id,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
int
vxlan_rx_pkts(int vid, struct rte_mbuf **pkts, uint32_t count);

#endif /* VXLAN_SETUP_H_ */
