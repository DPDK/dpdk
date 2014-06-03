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

#include <rte_eal.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

#include <rte_byteorder.h>

#include "args.h"
#include "main.h"
#include "init.h"
#include "../include/conf.h"


#ifdef QW_SOFTWARE_FC
#define SEND_PAUSE_FRAME(port_id, duration) send_pause_frame(port_id, duration)
#else
#define SEND_PAUSE_FRAME(port_id, duration) do { } while(0)
#endif

#define ETHER_TYPE_FLOW_CONTROL 0x8808

struct ether_fc_frame {
    uint16_t opcode;
    uint16_t param;
} __attribute__((__packed__));


int *quota;
unsigned int *low_watermark;

uint8_t port_pairs[RTE_MAX_ETHPORTS];

struct rte_ring *rings[RTE_MAX_LCORE][RTE_MAX_ETHPORTS];
struct rte_mempool *mbuf_pool;


static void send_pause_frame(uint8_t port_id, uint16_t duration)
{
    struct rte_mbuf *mbuf;
    struct ether_fc_frame *pause_frame;
    struct ether_hdr *hdr;
    struct ether_addr mac_addr;

    RTE_LOG(DEBUG, USER1, "Sending PAUSE frame (duration=%d) on port %d\n",
            duration, port_id);

    /* Get a mbuf from the pool */
    mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (unlikely(mbuf == NULL))
        return;

    /* Prepare a PAUSE frame */
    hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
    pause_frame = (struct ether_fc_frame *) &hdr[1];

    rte_eth_macaddr_get(port_id, &mac_addr);
    ether_addr_copy(&mac_addr, &hdr->s_addr);

    void *tmp = &hdr->d_addr.addr_bytes[0];
    *((uint64_t *)tmp) = 0x010000C28001ULL;

    hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_FLOW_CONTROL);

    pause_frame->opcode = rte_cpu_to_be_16(0x0001);
    pause_frame->param  = rte_cpu_to_be_16(duration);

    mbuf->pkt.pkt_len  = 60;
    mbuf->pkt.data_len = 60;

    rte_eth_tx_burst(port_id, 0, &mbuf, 1);
}

/**
 * Get the previous enabled lcore ID
 *
 * @param lcore_id
 *   The current lcore ID.
 * @return
 *   The previous enabled lcore_id or -1 if not found.
 */
static unsigned int
get_previous_lcore_id(unsigned int lcore_id)
{
    int i;

    for (i = lcore_id - 1; i >= 0; i--)
        if (rte_lcore_is_enabled(i))
            return i;

    return -1;
}

/**
 * Get the last enabled lcore ID
 *
 * @return
 *   The last enabled lcore_id.
 */
static unsigned int
get_last_lcore_id(void)
{
    int i;

    for (i = RTE_MAX_LCORE; i >= 0; i--)
        if (rte_lcore_is_enabled(i))
            return i;

    return 0;
}

static void
receive_stage(__attribute__((unused)) void *args)
{
    int i, ret;

    uint8_t port_id;
    uint16_t nb_rx_pkts;

    unsigned int lcore_id;

    struct rte_mbuf *pkts[MAX_PKT_QUOTA];
    struct rte_ring *ring;
    enum ring_state ring_state[RTE_MAX_ETHPORTS] = { RING_READY };

    lcore_id = rte_lcore_id();

    RTE_LOG(INFO, USER1,
            "%s() started on core %u\n", __func__, lcore_id);

    while (1) {

        /* Process each port round robin style */
        for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {

            if (!is_bit_set(port_id, portmask))
                continue;

            ring = rings[lcore_id][port_id];

            if (ring_state[port_id] != RING_READY) {
                if (rte_ring_count(ring) > *low_watermark)
                    continue;
                else
                    ring_state[port_id] = RING_READY;
            }

            /* Enqueue received packets on the RX ring */
            nb_rx_pkts = rte_eth_rx_burst(port_id, 0, pkts, (uint16_t) *quota);
            ret = rte_ring_enqueue_bulk(ring, (void *) pkts, nb_rx_pkts);
            if (ret == -EDQUOT) {
                ring_state[port_id] = RING_OVERLOADED;
                send_pause_frame(port_id, 1337);
            }

            else if (ret == -ENOBUFS) {

                /* Return  mbufs to the pool, effectively dropping packets */
                for (i = 0; i < nb_rx_pkts; i++)
                    rte_pktmbuf_free(pkts[i]);
            }
        }
    }
}

static void
pipeline_stage(__attribute__((unused)) void *args)
{
    int i, ret;
    int nb_dq_pkts;

    uint8_t port_id;

    unsigned int lcore_id, previous_lcore_id;

    void *pkts[MAX_PKT_QUOTA];
    struct rte_ring *rx, *tx;
    enum ring_state ring_state[RTE_MAX_ETHPORTS] = { RING_READY };

    lcore_id = rte_lcore_id();
    previous_lcore_id = get_previous_lcore_id(lcore_id);

    RTE_LOG(INFO, USER1,
            "%s() started on core %u - processing packets from core %u\n",
            __func__, lcore_id, previous_lcore_id);

    while (1) {

        for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {

            if (!is_bit_set(port_id, portmask))
                continue;

            tx = rings[lcore_id][port_id];
            rx = rings[previous_lcore_id][port_id];

            if (ring_state[port_id] != RING_READY) {
                if (rte_ring_count(tx) > *low_watermark)
                    continue;
                else
                    ring_state[port_id] = RING_READY;
            }

            /* Dequeue up to quota mbuf from rx */
            nb_dq_pkts = rte_ring_dequeue_burst(rx, pkts, *quota);
            if (unlikely(nb_dq_pkts < 0))
                continue;

            /* Enqueue them on tx */
            ret = rte_ring_enqueue_bulk(tx, pkts, nb_dq_pkts);
            if (ret == -EDQUOT)
                ring_state[port_id] = RING_OVERLOADED;

            else if (ret == -ENOBUFS) {

                /* Return  mbufs to the pool, effectively dropping packets */
                for (i = 0; i < nb_dq_pkts; i++)
                    rte_pktmbuf_free(pkts[i]);
            }
        }
    }
}

static void
send_stage(__attribute__((unused)) void *args)
{
	uint16_t nb_dq_pkts;

    uint8_t port_id;
    uint8_t dest_port_id;

    unsigned int lcore_id, previous_lcore_id;

    struct rte_ring *tx;
    struct rte_mbuf *tx_pkts[MAX_PKT_QUOTA];

    lcore_id = rte_lcore_id();
    previous_lcore_id = get_previous_lcore_id(lcore_id);

    RTE_LOG(INFO, USER1,
            "%s() started on core %u - processing packets from core %u\n",
            __func__, lcore_id, previous_lcore_id);

    while (1) {

        /* Process each ring round robin style */
        for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {

            if (!is_bit_set(port_id, portmask))
                continue;

            dest_port_id = port_pairs[port_id];
            tx = rings[previous_lcore_id][port_id];

            if (rte_ring_empty(tx))
                continue;

            /* Dequeue packets from tx and send them */
            nb_dq_pkts = (uint16_t) rte_ring_dequeue_burst(tx, (void *) tx_pkts, *quota);
            rte_eth_tx_burst(dest_port_id, 0, tx_pkts, nb_dq_pkts);

            /* TODO: Check if nb_dq_pkts == nb_tx_pkts? */
        }
    }
}

int
MAIN(int argc, char **argv)
{
    int ret;
    unsigned int lcore_id, master_lcore_id, last_lcore_id;

    uint8_t port_id;

    rte_set_log_level(RTE_LOG_INFO);

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");

    argc -= ret;
    argv += ret;

    init_dpdk();
    setup_shared_variables();

    *quota = 32;
    *low_watermark = 60 * RING_SIZE / 100;

    last_lcore_id   = get_last_lcore_id();
    master_lcore_id = rte_get_master_lcore();

    /* Parse the application's arguments */
    ret = parse_qw_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid quota/watermark argument(s)\n");

    /* Create a pool of mbuf to store packets */
    mbuf_pool = rte_mempool_create("mbuf_pool", MBUF_PER_POOL, MBUF_SIZE, 32,
                                   sizeof(struct rte_pktmbuf_pool_private),
                                   rte_pktmbuf_pool_init, NULL,
                                   rte_pktmbuf_init, NULL,
                                   rte_socket_id(), 0);
    if (mbuf_pool == NULL)
        rte_panic("%s\n", rte_strerror(rte_errno));

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
        if (is_bit_set(port_id, portmask)) {
            configure_eth_port(port_id);
            init_ring(master_lcore_id, port_id);
        }

    pair_ports();

    /* Start pipeline_connect() on all the available slave lcore but the last */
    for (lcore_id = 0 ; lcore_id < last_lcore_id; lcore_id++) {
        if (rte_lcore_is_enabled(lcore_id) && lcore_id != master_lcore_id) {

            for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
                if (is_bit_set(port_id, portmask))
                    init_ring(lcore_id, port_id);

            /* typecast is a workaround for GCC 4.3 bug */
            rte_eal_remote_launch((int (*)(void *))pipeline_stage, NULL, lcore_id);
        }
    }

    /* Start send_stage() on the last slave core */
    /* typecast is a workaround for GCC 4.3 bug */
    rte_eal_remote_launch((int (*)(void *))send_stage, NULL, last_lcore_id);

    /* Start receive_stage() on the master core */
    receive_stage(NULL);

    return 0;
}
