/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rte_eal.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include "args.h"
#include "init.h"
#include "main.h"
#include "../include/conf.h"


static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_DCB_NONE,
	},
};

static struct rte_eth_fc_conf fc_conf = {
    .mode       = RTE_FC_TX_PAUSE,
    .high_water = 80 * 510 / 100,
    .low_water  = 60 * 510 / 100,
    .pause_time = 1337,
    .send_xon   = 0,
};


void configure_eth_port(uint8_t port_id)
{
    int ret;

    rte_eth_dev_stop(port_id);

    ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure port %u (error %d)\n",
                               (unsigned) port_id, ret);

    /* Initialize the port's RX queue */
    ret = rte_eth_rx_queue_setup(port_id, 0, RX_DESC_PER_QUEUE,
				rte_eth_dev_socket_id(port_id),
				NULL,
				mbuf_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup RX queue on "
                               "port %u (error %d)\n", (unsigned) port_id, ret);

    /* Initialize the port's TX queue */
    ret = rte_eth_tx_queue_setup(port_id, 0, TX_DESC_PER_QUEUE,
				rte_eth_dev_socket_id(port_id),
				NULL);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup TX queue on "
                               "port %u (error %d)\n", (unsigned) port_id, ret);

    /* Initialize the port's flow control */
    ret = rte_eth_dev_flow_ctrl_set(port_id, &fc_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup hardware flow control on "
                               "port %u (error %d)\n", (unsigned) port_id, ret);

    /* Start the port */
    ret = rte_eth_dev_start(port_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to start port %u (error %d)\n",
                               (unsigned) port_id, ret);

    /* Put it in promiscuous mode */
    rte_eth_promiscuous_enable(port_id);
}

void
init_dpdk(void)
{
    if (rte_eth_dev_count() < 2)
        rte_exit(EXIT_FAILURE, "Not enough ethernet port available\n");
}

void init_ring(int lcore_id, uint8_t port_id)
{
    struct rte_ring *ring;
    char ring_name[RTE_RING_NAMESIZE];

    snprintf(ring_name, RTE_RING_NAMESIZE,
		"core%d_port%d", lcore_id, port_id);
    ring = rte_ring_create(ring_name, RING_SIZE, rte_socket_id(),
                           RING_F_SP_ENQ | RING_F_SC_DEQ);

    if (ring == NULL)
        rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

    rte_ring_set_water_mark(ring, 80 * RING_SIZE / 100);

    rings[lcore_id][port_id] = ring;
}

void
pair_ports(void)
{
    uint8_t i, j;

    /* Pair ports with their "closest neighbour" in the portmask */
    for (i = 0; i < RTE_MAX_ETHPORTS; i++)
        if (is_bit_set(i, portmask))
            for (j = (uint8_t) (i + 1); j < RTE_MAX_ETHPORTS; j++)
                if (is_bit_set(j, portmask)) {
                    port_pairs[i] = j;
                    port_pairs[j] = i;
                    i = j;
                    break;
                }
}

void
setup_shared_variables(void)
{
    const struct rte_memzone *qw_memzone;

    qw_memzone = rte_memzone_reserve(QUOTA_WATERMARK_MEMZONE_NAME, 2 * sizeof(int),
                                     rte_socket_id(), RTE_MEMZONE_2MB);
    if (qw_memzone == NULL)
        rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

    quota = qw_memzone->addr;
    low_watermark = (unsigned int *) qw_memzone->addr + sizeof(int);
}
