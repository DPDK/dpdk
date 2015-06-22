/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VXLAN_SETUP_H_
#define VXLAN_SETUP_H_

extern uint16_t nb_devices;
extern uint16_t udp_port;
extern uint8_t filter_idx;
extern uint8_t ports[RTE_MAX_ETHPORTS];
extern struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
extern uint32_t enable_stats;
extern struct device_statistics dev_statistics[MAX_DEVICES];
extern uint8_t rx_decap;
extern uint8_t tx_encap;

typedef int (*ol_port_configure_t)(uint8_t port,
				   struct rte_mempool *mbuf_pool);

typedef int (*ol_tunnel_setup_t)(struct vhost_dev *vdev,
				 struct rte_mbuf *m);

typedef void (*ol_tunnel_destroy_t)(struct vhost_dev *vdev);

typedef int (*ol_tx_handle_t)(uint8_t port_id, uint16_t queue_id,
			      struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

typedef int (*ol_rx_handle_t)(struct virtio_net *dev, struct rte_mbuf **pkts,
			      uint32_t count);

typedef int (*ol_param_handle)(struct virtio_net *dev);

struct ol_switch_ops {
	ol_port_configure_t        port_configure;
	ol_tunnel_setup_t          tunnel_setup;
	ol_tunnel_destroy_t        tunnel_destroy;
	ol_tx_handle_t             tx_handle;
	ol_rx_handle_t             rx_handle;
	ol_param_handle            param_handle;
};

int
vxlan_port_init(uint8_t port, struct rte_mempool *mbuf_pool);

int
vxlan_link(struct vhost_dev *vdev, struct rte_mbuf *m);

void
vxlan_unlink(struct vhost_dev *vdev);

int
vxlan_tx_pkts(uint8_t port_id, uint16_t queue_id,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
int
vxlan_rx_pkts(struct virtio_net *dev, struct rte_mbuf **pkts, uint32_t count);

#endif /* VXLAN_SETUP_H_ */
