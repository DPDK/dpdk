/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <getopt.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/virtio_net.h>
#include <linux/virtio_ring.h>
#include <sys/param.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>

#include "main.h"
#include "rte_vhost.h"
#include "vxlan.h"
#include "vxlan_setup.h"

#define IPV4_HEADER_LEN 20
#define UDP_HEADER_LEN  8
#define VXLAN_HEADER_LEN 8

#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

#define IP_DN_FRAGMENT_FLAG 0x0040

/* Used to compare MAC addresses. */
#define MAC_ADDR_CMP 0xFFFFFFFFFFFFULL

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 512

/* Default inner VLAN ID */
#define INNER_VLAN_ID 100

/* VXLAN device */
struct vxlan_conf vxdev;

struct ipv4_hdr app_ip_hdr[VXLAN_N_PORTS];
struct ether_hdr app_l2_hdr[VXLAN_N_PORTS];

/* local VTEP IP address */
uint8_t vxlan_multicast_ips[2][4] = { {239, 1, 1, 1 }, {239, 1, 2, 1 } };

/* Remote VTEP IP address */
uint8_t vxlan_overlay_ips[2][4] = { {192, 168, 10, 1}, {192, 168, 30, 1} };

/* Remote VTEP MAC address */
uint8_t peer_mac[6] = {0x00, 0x11, 0x01, 0x00, 0x00, 0x01};

/* VXLAN RX filter type */
uint8_t tep_filter_type[] = {RTE_TUNNEL_FILTER_IMAC_TENID,
			RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID,
			RTE_TUNNEL_FILTER_OMAC_TENID_IMAC,};

/* Options for configuring ethernet port */
static struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
		.offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM |
			     DEV_TX_OFFLOAD_UDP_CKSUM |
			     DEV_TX_OFFLOAD_TCP_CKSUM |
			     DEV_TX_OFFLOAD_SCTP_CKSUM |
			     DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
			     DEV_TX_OFFLOAD_TCP_TSO |
			     DEV_TX_OFFLOAD_MULTI_SEGS |
			     DEV_TX_OFFLOAD_VXLAN_TNL_TSO),
	},
};

/**
 * The one or two device(s) that belongs to the same tenant ID can
 * be assigned in a VM.
 */
const uint16_t tenant_id_conf[] = {
	1000, 1000, 1001, 1001, 1002, 1002, 1003, 1003,
	1004, 1004, 1005, 1005, 1006, 1006, 1007, 1007,
	1008, 1008, 1009, 1009, 1010, 1010, 1011, 1011,
	1012, 1012, 1013, 1013, 1014, 1014, 1015, 1015,
	1016, 1016, 1017, 1017, 1018, 1018, 1019, 1019,
	1020, 1020, 1021, 1021, 1022, 1022, 1023, 1023,
	1024, 1024, 1025, 1025, 1026, 1026, 1027, 1027,
	1028, 1028, 1029, 1029, 1030, 1030, 1031, 1031,
};

/**
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
int
vxlan_port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	uint16_t rx_rings, tx_rings = (uint16_t)rte_lcore_count();
	uint16_t rx_ring_size = RTE_TEST_RX_DESC_DEFAULT;
	uint16_t tx_ring_size = RTE_TEST_TX_DESC_DEFAULT;
	struct rte_eth_udp_tunnel tunnel_udp;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_txconf *txconf;
	struct vxlan_conf *pconf = &vxdev;
	struct rte_eth_conf local_port_conf = port_conf;

	pconf->dst_port = udp_port;

	rte_eth_dev_info_get(port, &dev_info);

	if (dev_info.max_rx_queues > MAX_QUEUES) {
		rte_exit(EXIT_FAILURE,
			"please define MAX_QUEUES no less than %u in %s\n",
			dev_info.max_rx_queues, __FILE__);
	}

	rxconf = &dev_info.default_rxconf;
	txconf = &dev_info.default_txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rx_rings = nb_devices;
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	/* Configure ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings,
				       &local_port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &rx_ring_size,
			&tx_ring_size);
	if (retval != 0)
		return retval;

	/* Setup the queues. */
	rxconf->offloads = local_port_conf.rxmode.offloads;
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
						rte_eth_dev_socket_id(port),
						rxconf,
						mbuf_pool);
		if (retval < 0)
			return retval;
	}
	txconf->offloads = local_port_conf.txmode.offloads;
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
						rte_eth_dev_socket_id(port),
						txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the device. */
	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Configure UDP port for UDP tunneling */
	tunnel_udp.udp_port = udp_port;
	tunnel_udp.prot_type = RTE_TUNNEL_TYPE_VXLAN;
	retval = rte_eth_dev_udp_tunnel_port_add(port, &tunnel_udp);
	if (retval < 0)
		return retval;
	rte_eth_macaddr_get(port, &ports_eth_addr[port]);
	RTE_LOG(INFO, PORT, "Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			port,
			ports_eth_addr[port].addr_bytes[0],
			ports_eth_addr[port].addr_bytes[1],
			ports_eth_addr[port].addr_bytes[2],
			ports_eth_addr[port].addr_bytes[3],
			ports_eth_addr[port].addr_bytes[4],
			ports_eth_addr[port].addr_bytes[5]);

	if (tso_segsz != 0) {
		struct rte_eth_dev_info dev_info;
		rte_eth_dev_info_get(port, &dev_info);
		if ((dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_TSO) == 0)
			RTE_LOG(WARNING, PORT,
				"hardware TSO offload is not supported\n");
	}
	return 0;
}

static int
vxlan_rx_process(struct rte_mbuf *pkt)
{
	int ret = 0;

	if (rx_decap)
		ret = decapsulation(pkt);

	return ret;
}

static void
vxlan_tx_process(uint8_t queue_id, struct rte_mbuf *pkt)
{
	if (tx_encap)
		encapsulation(pkt, queue_id);

	return;
}

/*
 * This function learns the MAC address of the device and set init
 * L2 header and L3 header info.
 */
int
vxlan_link(struct vhost_dev *vdev, struct rte_mbuf *m)
{
	int i, ret;
	struct ether_hdr *pkt_hdr;
	uint64_t portid = vdev->vid;
	struct ipv4_hdr *ip;

	struct rte_eth_tunnel_filter_conf tunnel_filter_conf;

	if (unlikely(portid >= VXLAN_N_PORTS)) {
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) WARNING: Not configuring device,"
			"as already have %d ports for VXLAN.",
			vdev->vid, VXLAN_N_PORTS);
		return -1;
	}

	/* Learn MAC address of guest device from packet */
	pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	if (is_same_ether_addr(&(pkt_hdr->s_addr), &vdev->mac_address)) {
		RTE_LOG(INFO, VHOST_DATA,
			"(%d) WARNING: This device is using an existing"
			" MAC address and has not been registered.\n",
			vdev->vid);
		return -1;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		vdev->mac_address.addr_bytes[i] =
			vxdev.port[portid].vport_mac.addr_bytes[i] =
			pkt_hdr->s_addr.addr_bytes[i];
		vxdev.port[portid].peer_mac.addr_bytes[i] = peer_mac[i];
	}

	memset(&tunnel_filter_conf, 0,
		sizeof(struct rte_eth_tunnel_filter_conf));

	ether_addr_copy(&ports_eth_addr[0], &tunnel_filter_conf.outer_mac);
	tunnel_filter_conf.filter_type = tep_filter_type[filter_idx];

	/* inner MAC */
	ether_addr_copy(&vdev->mac_address, &tunnel_filter_conf.inner_mac);

	tunnel_filter_conf.queue_id = vdev->rx_q;
	tunnel_filter_conf.tenant_id = tenant_id_conf[vdev->rx_q];

	if (tep_filter_type[filter_idx] == RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID)
		tunnel_filter_conf.inner_vlan = INNER_VLAN_ID;

	tunnel_filter_conf.tunnel_type = RTE_TUNNEL_TYPE_VXLAN;

	ret = rte_eth_dev_filter_ctrl(ports[0],
		RTE_ETH_FILTER_TUNNEL,
		RTE_ETH_FILTER_ADD,
		&tunnel_filter_conf);
	if (ret) {
		RTE_LOG(ERR, VHOST_DATA,
			"%d Failed to add device MAC address to cloud filter\n",
		vdev->rx_q);
		return -1;
	}

	/* Print out inner MAC and VNI info. */
	RTE_LOG(INFO, VHOST_DATA,
		"(%d) MAC_ADDRESS %02x:%02x:%02x:%02x:%02x:%02x and VNI %d registered\n",
		vdev->rx_q,
		vdev->mac_address.addr_bytes[0],
		vdev->mac_address.addr_bytes[1],
		vdev->mac_address.addr_bytes[2],
		vdev->mac_address.addr_bytes[3],
		vdev->mac_address.addr_bytes[4],
		vdev->mac_address.addr_bytes[5],
		tenant_id_conf[vdev->rx_q]);

	vxdev.port[portid].vport_id = portid;

	for (i = 0; i < 4; i++) {
		/* Local VTEP IP */
		vxdev.port_ip |= vxlan_multicast_ips[portid][i] << (8 * i);
		/* Remote VTEP IP */
		vxdev.port[portid].peer_ip |=
			vxlan_overlay_ips[portid][i] << (8 * i);
	}

	vxdev.out_key = tenant_id_conf[vdev->rx_q];
	ether_addr_copy(&vxdev.port[portid].peer_mac,
			&app_l2_hdr[portid].d_addr);
	ether_addr_copy(&ports_eth_addr[0],
			&app_l2_hdr[portid].s_addr);
	app_l2_hdr[portid].ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	ip = &app_ip_hdr[portid];
	ip->version_ihl = IP_VHL_DEF;
	ip->type_of_service = 0;
	ip->total_length = 0;
	ip->packet_id = 0;
	ip->fragment_offset = IP_DN_FRAGMENT_FLAG;
	ip->time_to_live = IP_DEFTTL;
	ip->next_proto_id = IPPROTO_UDP;
	ip->hdr_checksum = 0;
	ip->src_addr = vxdev.port_ip;
	ip->dst_addr = vxdev.port[portid].peer_ip;

	/* Set device as ready for RX. */
	vdev->ready = DEVICE_RX;

	return 0;
}

/**
 * Removes cloud filter. Ensures that nothing is adding buffers to the RX
 * queue before disabling RX on the device.
 */
void
vxlan_unlink(struct vhost_dev *vdev)
{
	unsigned i = 0, rx_count;
	int ret;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_eth_tunnel_filter_conf tunnel_filter_conf;

	if (vdev->ready == DEVICE_RX) {
		memset(&tunnel_filter_conf, 0,
			sizeof(struct rte_eth_tunnel_filter_conf));

		ether_addr_copy(&ports_eth_addr[0], &tunnel_filter_conf.outer_mac);
		ether_addr_copy(&vdev->mac_address, &tunnel_filter_conf.inner_mac);
		tunnel_filter_conf.tenant_id = tenant_id_conf[vdev->rx_q];
		tunnel_filter_conf.filter_type = tep_filter_type[filter_idx];

		if (tep_filter_type[filter_idx] ==
			RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID)
			tunnel_filter_conf.inner_vlan = INNER_VLAN_ID;

		tunnel_filter_conf.queue_id = vdev->rx_q;
		tunnel_filter_conf.tunnel_type = RTE_TUNNEL_TYPE_VXLAN;

		ret = rte_eth_dev_filter_ctrl(ports[0],
				RTE_ETH_FILTER_TUNNEL,
				RTE_ETH_FILTER_DELETE,
				&tunnel_filter_conf);
		if (ret) {
			RTE_LOG(ERR, VHOST_DATA,
				"%d Failed to add device MAC address to cloud filter\n",
				vdev->rx_q);
			return;
		}
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			vdev->mac_address.addr_bytes[i] = 0;

		/* Clear out the receive buffers */
		rx_count = rte_eth_rx_burst(ports[0],
				(uint16_t)vdev->rx_q,
				pkts_burst, MAX_PKT_BURST);

		while (rx_count) {
			for (i = 0; i < rx_count; i++)
				rte_pktmbuf_free(pkts_burst[i]);

			rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)vdev->rx_q,
					pkts_burst, MAX_PKT_BURST);
		}
		vdev->ready = DEVICE_MAC_LEARNING;
	}
}

/* Transmit packets after encapsulating */
int
vxlan_tx_pkts(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
	int ret = 0;
	uint16_t i;

	for (i = 0; i < nb_pkts; i++)
		vxlan_tx_process(queue_id, tx_pkts[i]);

	ret = rte_eth_tx_burst(port_id, queue_id, tx_pkts, nb_pkts);

	return ret;
}

/* Check for decapsulation and pass packets directly to VIRTIO device */
int
vxlan_rx_pkts(int vid, struct rte_mbuf **pkts_burst, uint32_t rx_count)
{
	uint32_t i = 0;
	uint32_t count = 0;
	int ret;
	struct rte_mbuf *pkts_valid[rx_count];

	for (i = 0; i < rx_count; i++) {
		if (enable_stats) {
			rte_atomic64_add(
				&dev_statistics[vid].rx_bad_ip_csum,
				(pkts_burst[i]->ol_flags & PKT_RX_IP_CKSUM_BAD)
				!= 0);
			rte_atomic64_add(
				&dev_statistics[vid].rx_bad_ip_csum,
				(pkts_burst[i]->ol_flags & PKT_RX_L4_CKSUM_BAD)
				!= 0);
		}
		ret = vxlan_rx_process(pkts_burst[i]);
		if (unlikely(ret < 0))
			continue;

		pkts_valid[count] = pkts_burst[i];
			count++;
	}

	ret = rte_vhost_enqueue_burst(vid, VIRTIO_RXQ, pkts_valid, count);
	return ret;
}
