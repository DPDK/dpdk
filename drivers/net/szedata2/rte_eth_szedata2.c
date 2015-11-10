/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015 CESNET
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
 *     * Neither the name of CESNET nor the names of its
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

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <err.h>

#include <libsze2.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include "rte_eth_szedata2.h"

#define RTE_ETH_SZEDATA2_DEV_PATH_ARG "dev_path"
#define RTE_ETH_SZEDATA2_RX_IFACES_ARG "rx_ifaces"
#define RTE_ETH_SZEDATA2_TX_IFACES_ARG "tx_ifaces"

#define RTE_ETH_SZEDATA2_MAX_RX_QUEUES 32
#define RTE_ETH_SZEDATA2_MAX_TX_QUEUES 32
#define RTE_ETH_SZEDATA2_TX_LOCK_SIZE (32 * 1024 * 1024)

/**
 * size of szedata2_packet header with alignment
 */
#define RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED 8

struct szedata2_rx_queue {
	struct szedata *sze;
	uint8_t rx_channel;
	uint8_t in_port;
	struct rte_mempool *mb_pool;
	volatile uint64_t rx_pkts;
	volatile uint64_t rx_bytes;
	volatile uint64_t err_pkts;
};

struct szedata2_tx_queue {
	struct szedata *sze;
	uint8_t tx_channel;
	volatile uint64_t tx_pkts;
	volatile uint64_t err_pkts;
	volatile uint64_t tx_bytes;
};

struct rxtx_szedata2 {
	uint32_t num_of_rx;
	uint32_t num_of_tx;
	uint32_t sze_rx_mask_req;
	uint32_t sze_tx_mask_req;
	char *sze_dev;
};

struct pmd_internals {
	struct szedata2_rx_queue rx_queue[RTE_ETH_SZEDATA2_MAX_RX_QUEUES];
	struct szedata2_tx_queue tx_queue[RTE_ETH_SZEDATA2_MAX_TX_QUEUES];
	unsigned nb_rx_queues;
	unsigned nb_tx_queues;
	uint32_t num_of_rx;
	uint32_t num_of_tx;
	uint32_t sze_rx_req;
	uint32_t sze_tx_req;
	int if_index;
	char *sze_dev;
};

static const char *valid_arguments[] = {
	RTE_ETH_SZEDATA2_DEV_PATH_ARG,
	RTE_ETH_SZEDATA2_RX_IFACES_ARG,
	RTE_ETH_SZEDATA2_TX_IFACES_ARG,
	NULL
};

static struct ether_addr eth_addr = {
	.addr_bytes = { 0x00, 0x11, 0x17, 0x00, 0x00, 0x00 }
};
static const char *drivername = "SZEdata2 PMD";
static struct rte_eth_link pmd_link = {
		.link_speed = ETH_LINK_SPEED_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = 0
};


static uint32_t
count_ones(uint32_t num)
{
	num = num - ((num >> 1) & 0x55555555); /* reuse input as temporary */
	num = (num & 0x33333333) + ((num >> 2) & 0x33333333);        /* temp */
	return (((num + (num >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; /* count */
}

static uint16_t
eth_szedata2_rx(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	unsigned int i;
	struct rte_mbuf *mbuf;
	struct szedata2_rx_queue *sze_q = queue;
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint16_t num_rx = 0;
	uint16_t buf_size;
	uint16_t sg_size;
	uint16_t hw_size;
	uint16_t packet_size;
	uint64_t num_bytes = 0;
	struct szedata *sze = sze_q->sze;
	uint8_t *header_ptr = NULL; /* header of packet */
	uint8_t *packet_ptr1 = NULL;
	uint8_t *packet_ptr2 = NULL;
	uint16_t packet_len1 = 0;
	uint16_t packet_len2 = 0;
	uint16_t hw_data_align;

	if (unlikely(sze_q->sze == NULL || nb_pkts == 0))
		return 0;

	/*
	 * Reads the given number of packets from szedata2 channel given
	 * by queue and copies the packet data into a newly allocated mbuf
	 * to return.
	 */
	for (i = 0; i < nb_pkts; i++) {
		mbuf = rte_pktmbuf_alloc(sze_q->mb_pool);

		if (unlikely(mbuf == NULL))
			break;

		/* get the next sze packet */
		if (sze->ct_rx_lck != NULL && !sze->ct_rx_rem_bytes &&
				sze->ct_rx_lck->next == NULL) {
			/* unlock old data */
			szedata_rx_unlock_data(sze_q->sze, sze->ct_rx_lck_orig);
			sze->ct_rx_lck_orig = NULL;
			sze->ct_rx_lck = NULL;
		}

		if (!sze->ct_rx_rem_bytes && sze->ct_rx_lck_orig == NULL) {
			/* nothing to read, lock new data */
			sze->ct_rx_lck = szedata_rx_lock_data(sze_q->sze, ~0U);
			sze->ct_rx_lck_orig = sze->ct_rx_lck;

			if (sze->ct_rx_lck == NULL) {
				/* nothing to lock */
				rte_pktmbuf_free(mbuf);
				break;
			}

			sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;
			sze->ct_rx_rem_bytes = sze->ct_rx_lck->len;

			if (!sze->ct_rx_rem_bytes) {
				rte_pktmbuf_free(mbuf);
				break;
			}
		}

		if (sze->ct_rx_rem_bytes < RTE_SZE2_PACKET_HEADER_SIZE) {
			/*
			 * cut in header
			 * copy parts of header to merge buffer
			 */
			if (sze->ct_rx_lck->next == NULL) {
				rte_pktmbuf_free(mbuf);
				break;
			}

			/* copy first part of header */
			rte_memcpy(sze->ct_rx_buffer, sze->ct_rx_cur_ptr,
					sze->ct_rx_rem_bytes);

			/* copy second part of header */
			sze->ct_rx_lck = sze->ct_rx_lck->next;
			sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;
			rte_memcpy(sze->ct_rx_buffer + sze->ct_rx_rem_bytes,
				sze->ct_rx_cur_ptr,
				RTE_SZE2_PACKET_HEADER_SIZE -
				sze->ct_rx_rem_bytes);

			sze->ct_rx_cur_ptr += RTE_SZE2_PACKET_HEADER_SIZE -
				sze->ct_rx_rem_bytes;
			sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
				RTE_SZE2_PACKET_HEADER_SIZE +
				sze->ct_rx_rem_bytes;

			header_ptr = (uint8_t *)sze->ct_rx_buffer;
		} else {
			/* not cut */
			header_ptr = (uint8_t *)sze->ct_rx_cur_ptr;
			sze->ct_rx_cur_ptr += RTE_SZE2_PACKET_HEADER_SIZE;
			sze->ct_rx_rem_bytes -= RTE_SZE2_PACKET_HEADER_SIZE;
		}

		sg_size = le16toh(*((uint16_t *)header_ptr));
		hw_size = le16toh(*(((uint16_t *)header_ptr) + 1));
		packet_size = sg_size -
			RTE_SZE2_ALIGN8(RTE_SZE2_PACKET_HEADER_SIZE + hw_size);


		/* checks if packet all right */
		if (!sg_size)
			errx(5, "Zero segsize");

		/* check sg_size and hwsize */
		if (hw_size > sg_size - RTE_SZE2_PACKET_HEADER_SIZE) {
			errx(10, "Hwsize bigger than expected. Segsize: %d, "
				"hwsize: %d", sg_size, hw_size);
		}

		hw_data_align =
			RTE_SZE2_ALIGN8(RTE_SZE2_PACKET_HEADER_SIZE + hw_size) -
			RTE_SZE2_PACKET_HEADER_SIZE;

		if (sze->ct_rx_rem_bytes >=
				(uint16_t)(sg_size -
				RTE_SZE2_PACKET_HEADER_SIZE)) {
			/* no cut */
			/* one packet ready - go to another */
			packet_ptr1 = sze->ct_rx_cur_ptr + hw_data_align;
			packet_len1 = packet_size;
			packet_ptr2 = NULL;
			packet_len2 = 0;

			sze->ct_rx_cur_ptr += RTE_SZE2_ALIGN8(sg_size) -
				RTE_SZE2_PACKET_HEADER_SIZE;
			sze->ct_rx_rem_bytes -= RTE_SZE2_ALIGN8(sg_size) -
				RTE_SZE2_PACKET_HEADER_SIZE;
		} else {
			/* cut in data */
			if (sze->ct_rx_lck->next == NULL) {
				errx(6, "Need \"next\" lock, "
					"but it is missing: %u",
					sze->ct_rx_rem_bytes);
			}

			/* skip hw data */
			if (sze->ct_rx_rem_bytes <= hw_data_align) {
				uint16_t rem_size = hw_data_align -
					sze->ct_rx_rem_bytes;

				/* MOVE to next lock */
				sze->ct_rx_lck = sze->ct_rx_lck->next;
				sze->ct_rx_cur_ptr =
					(void *)(((uint8_t *)
					(sze->ct_rx_lck->start)) + rem_size);

				packet_ptr1 = sze->ct_rx_cur_ptr;
				packet_len1 = packet_size;
				packet_ptr2 = NULL;
				packet_len2 = 0;

				sze->ct_rx_cur_ptr +=
					RTE_SZE2_ALIGN8(packet_size);
				sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
					rem_size - RTE_SZE2_ALIGN8(packet_size);
			} else {
				/* get pointer and length from first part */
				packet_ptr1 = sze->ct_rx_cur_ptr +
					hw_data_align;
				packet_len1 = sze->ct_rx_rem_bytes -
					hw_data_align;

				/* MOVE to next lock */
				sze->ct_rx_lck = sze->ct_rx_lck->next;
				sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;

				/* get pointer and length from second part */
				packet_ptr2 = sze->ct_rx_cur_ptr;
				packet_len2 = packet_size - packet_len1;

				sze->ct_rx_cur_ptr +=
					RTE_SZE2_ALIGN8(packet_size) -
					packet_len1;
				sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
					(RTE_SZE2_ALIGN8(packet_size) -
					 packet_len1);
			}
		}

		if (unlikely(packet_ptr1 == NULL)) {
			rte_pktmbuf_free(mbuf);
			break;
		}

		/* get the space available for data in the mbuf */
		mbp_priv = rte_mempool_get_priv(sze_q->mb_pool);
		buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size -
				RTE_PKTMBUF_HEADROOM);

		if (packet_size <= buf_size) {
			/* sze packet will fit in one mbuf, go ahead and copy */
			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *),
					packet_ptr1, packet_len1);
			if (packet_ptr2 != NULL) {
				rte_memcpy((void *)(rte_pktmbuf_mtod(mbuf,
					uint8_t *) + packet_len1),
					packet_ptr2, packet_len2);
			}
			mbuf->data_len = (uint16_t)packet_size;

			mbuf->pkt_len = packet_size;
			mbuf->port = sze_q->in_port;
			bufs[num_rx] = mbuf;
			num_rx++;
			num_bytes += packet_size;
		} else {
			/*
			 * sze packet will not fit in one mbuf,
			 * scattered mode is not enabled, drop packet
			 */
			RTE_LOG(ERR, PMD,
				"SZE segment %d bytes will not fit in one mbuf "
				"(%d bytes), scattered mode is not enabled, "
				"drop packet!!\n",
				packet_size, buf_size);
			rte_pktmbuf_free(mbuf);
		}
	}

	sze_q->rx_pkts += num_rx;
	sze_q->rx_bytes += num_bytes;
	return num_rx;
}

static uint16_t
eth_szedata2_rx_scattered(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	unsigned int i;
	struct rte_mbuf *mbuf;
	struct szedata2_rx_queue *sze_q = queue;
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint16_t num_rx = 0;
	uint16_t buf_size;
	uint16_t sg_size;
	uint16_t hw_size;
	uint16_t packet_size;
	uint64_t num_bytes = 0;
	struct szedata *sze = sze_q->sze;
	uint8_t *header_ptr = NULL; /* header of packet */
	uint8_t *packet_ptr1 = NULL;
	uint8_t *packet_ptr2 = NULL;
	uint16_t packet_len1 = 0;
	uint16_t packet_len2 = 0;
	uint16_t hw_data_align;

	if (unlikely(sze_q->sze == NULL || nb_pkts == 0))
		return 0;

	/*
	 * Reads the given number of packets from szedata2 channel given
	 * by queue and copies the packet data into a newly allocated mbuf
	 * to return.
	 */
	for (i = 0; i < nb_pkts; i++) {
		const struct szedata_lock *ct_rx_lck_backup;
		unsigned int ct_rx_rem_bytes_backup;
		unsigned char *ct_rx_cur_ptr_backup;

		/* get the next sze packet */
		if (sze->ct_rx_lck != NULL && !sze->ct_rx_rem_bytes &&
				sze->ct_rx_lck->next == NULL) {
			/* unlock old data */
			szedata_rx_unlock_data(sze_q->sze, sze->ct_rx_lck_orig);
			sze->ct_rx_lck_orig = NULL;
			sze->ct_rx_lck = NULL;
		}

		/*
		 * Store items from sze structure which can be changed
		 * before mbuf allocating. Use these items in case of mbuf
		 * allocating failure.
		 */
		ct_rx_lck_backup = sze->ct_rx_lck;
		ct_rx_rem_bytes_backup = sze->ct_rx_rem_bytes;
		ct_rx_cur_ptr_backup = sze->ct_rx_cur_ptr;

		if (!sze->ct_rx_rem_bytes && sze->ct_rx_lck_orig == NULL) {
			/* nothing to read, lock new data */
			sze->ct_rx_lck = szedata_rx_lock_data(sze_q->sze, ~0U);
			sze->ct_rx_lck_orig = sze->ct_rx_lck;

			/*
			 * Backup items from sze structure must be updated
			 * after locking to contain pointers to new locks.
			 */
			ct_rx_lck_backup = sze->ct_rx_lck;
			ct_rx_rem_bytes_backup = sze->ct_rx_rem_bytes;
			ct_rx_cur_ptr_backup = sze->ct_rx_cur_ptr;

			if (sze->ct_rx_lck == NULL)
				/* nothing to lock */
				break;

			sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;
			sze->ct_rx_rem_bytes = sze->ct_rx_lck->len;

			if (!sze->ct_rx_rem_bytes)
				break;
		}

		if (sze->ct_rx_rem_bytes < RTE_SZE2_PACKET_HEADER_SIZE) {
			/*
			 * cut in header - copy parts of header to merge buffer
			 */
			if (sze->ct_rx_lck->next == NULL)
				break;

			/* copy first part of header */
			rte_memcpy(sze->ct_rx_buffer, sze->ct_rx_cur_ptr,
					sze->ct_rx_rem_bytes);

			/* copy second part of header */
			sze->ct_rx_lck = sze->ct_rx_lck->next;
			sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;
			rte_memcpy(sze->ct_rx_buffer + sze->ct_rx_rem_bytes,
				sze->ct_rx_cur_ptr,
				RTE_SZE2_PACKET_HEADER_SIZE -
				sze->ct_rx_rem_bytes);

			sze->ct_rx_cur_ptr += RTE_SZE2_PACKET_HEADER_SIZE -
				sze->ct_rx_rem_bytes;
			sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
				RTE_SZE2_PACKET_HEADER_SIZE +
				sze->ct_rx_rem_bytes;

			header_ptr = (uint8_t *)sze->ct_rx_buffer;
		} else {
			/* not cut */
			header_ptr = (uint8_t *)sze->ct_rx_cur_ptr;
			sze->ct_rx_cur_ptr += RTE_SZE2_PACKET_HEADER_SIZE;
			sze->ct_rx_rem_bytes -= RTE_SZE2_PACKET_HEADER_SIZE;
		}

		sg_size = le16toh(*((uint16_t *)header_ptr));
		hw_size = le16toh(*(((uint16_t *)header_ptr) + 1));
		packet_size = sg_size -
			RTE_SZE2_ALIGN8(RTE_SZE2_PACKET_HEADER_SIZE + hw_size);


		/* checks if packet all right */
		if (!sg_size)
			errx(5, "Zero segsize");

		/* check sg_size and hwsize */
		if (hw_size > sg_size - RTE_SZE2_PACKET_HEADER_SIZE) {
			errx(10, "Hwsize bigger than expected. Segsize: %d, "
					"hwsize: %d", sg_size, hw_size);
		}

		hw_data_align =
			RTE_SZE2_ALIGN8((RTE_SZE2_PACKET_HEADER_SIZE +
			hw_size)) - RTE_SZE2_PACKET_HEADER_SIZE;

		if (sze->ct_rx_rem_bytes >=
				(uint16_t)(sg_size -
				RTE_SZE2_PACKET_HEADER_SIZE)) {
			/* no cut */
			/* one packet ready - go to another */
			packet_ptr1 = sze->ct_rx_cur_ptr + hw_data_align;
			packet_len1 = packet_size;
			packet_ptr2 = NULL;
			packet_len2 = 0;

			sze->ct_rx_cur_ptr += RTE_SZE2_ALIGN8(sg_size) -
				RTE_SZE2_PACKET_HEADER_SIZE;
			sze->ct_rx_rem_bytes -= RTE_SZE2_ALIGN8(sg_size) -
				RTE_SZE2_PACKET_HEADER_SIZE;
		} else {
			/* cut in data */
			if (sze->ct_rx_lck->next == NULL) {
				errx(6, "Need \"next\" lock, but it is "
					"missing: %u", sze->ct_rx_rem_bytes);
			}

			/* skip hw data */
			if (sze->ct_rx_rem_bytes <= hw_data_align) {
				uint16_t rem_size = hw_data_align -
					sze->ct_rx_rem_bytes;

				/* MOVE to next lock */
				sze->ct_rx_lck = sze->ct_rx_lck->next;
				sze->ct_rx_cur_ptr =
					(void *)(((uint8_t *)
					(sze->ct_rx_lck->start)) + rem_size);

				packet_ptr1 = sze->ct_rx_cur_ptr;
				packet_len1 = packet_size;
				packet_ptr2 = NULL;
				packet_len2 = 0;

				sze->ct_rx_cur_ptr +=
					RTE_SZE2_ALIGN8(packet_size);
				sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
					rem_size - RTE_SZE2_ALIGN8(packet_size);
			} else {
				/* get pointer and length from first part */
				packet_ptr1 = sze->ct_rx_cur_ptr +
					hw_data_align;
				packet_len1 = sze->ct_rx_rem_bytes -
					hw_data_align;

				/* MOVE to next lock */
				sze->ct_rx_lck = sze->ct_rx_lck->next;
				sze->ct_rx_cur_ptr = sze->ct_rx_lck->start;

				/* get pointer and length from second part */
				packet_ptr2 = sze->ct_rx_cur_ptr;
				packet_len2 = packet_size - packet_len1;

				sze->ct_rx_cur_ptr +=
					RTE_SZE2_ALIGN8(packet_size) -
					packet_len1;
				sze->ct_rx_rem_bytes = sze->ct_rx_lck->len -
					(RTE_SZE2_ALIGN8(packet_size) -
					 packet_len1);
			}
		}

		if (unlikely(packet_ptr1 == NULL))
			break;

		mbuf = rte_pktmbuf_alloc(sze_q->mb_pool);

		if (unlikely(mbuf == NULL)) {
			/*
			 * Restore items from sze structure to state after
			 * unlocking (eventually locking).
			 */
			sze->ct_rx_lck = ct_rx_lck_backup;
			sze->ct_rx_rem_bytes = ct_rx_rem_bytes_backup;
			sze->ct_rx_cur_ptr = ct_rx_cur_ptr_backup;
			break;
		}

		/* get the space available for data in the mbuf */
		mbp_priv = rte_mempool_get_priv(sze_q->mb_pool);
		buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size -
				RTE_PKTMBUF_HEADROOM);

		if (packet_size <= buf_size) {
			/* sze packet will fit in one mbuf, go ahead and copy */
			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *),
					packet_ptr1, packet_len1);
			if (packet_ptr2 != NULL) {
				rte_memcpy((void *)
					(rte_pktmbuf_mtod(mbuf, uint8_t *) +
					packet_len1), packet_ptr2, packet_len2);
			}
			mbuf->data_len = (uint16_t)packet_size;
		} else {
			/*
			 * sze packet will not fit in one mbuf,
			 * scatter packet into more mbufs
			 */
			struct rte_mbuf *m = mbuf;
			uint16_t len = rte_pktmbuf_tailroom(mbuf);

			/* copy first part of packet */
			/* fill first mbuf */
			rte_memcpy(rte_pktmbuf_append(mbuf, len), packet_ptr1,
				len);
			packet_len1 -= len;
			packet_ptr1 = ((uint8_t *)packet_ptr1) + len;

			while (packet_len1 > 0) {
				/* fill new mbufs */
				m->next = rte_pktmbuf_alloc(sze_q->mb_pool);

				if (unlikely(m->next == NULL)) {
					rte_pktmbuf_free(mbuf);
					/*
					 * Restore items from sze structure
					 * to state after unlocking (eventually
					 * locking).
					 */
					sze->ct_rx_lck = ct_rx_lck_backup;
					sze->ct_rx_rem_bytes =
						ct_rx_rem_bytes_backup;
					sze->ct_rx_cur_ptr =
						ct_rx_cur_ptr_backup;
					goto finish;
				}

				m = m->next;

				len = RTE_MIN(rte_pktmbuf_tailroom(m),
					packet_len1);
				rte_memcpy(rte_pktmbuf_append(mbuf, len),
					packet_ptr1, len);

				(mbuf->nb_segs)++;
				packet_len1 -= len;
				packet_ptr1 = ((uint8_t *)packet_ptr1) + len;
			}

			if (packet_ptr2 != NULL) {
				/* copy second part of packet, if exists */
				/* fill the rest of currently last mbuf */
				len = rte_pktmbuf_tailroom(m);
				rte_memcpy(rte_pktmbuf_append(mbuf, len),
					packet_ptr2, len);
				packet_len2 -= len;
				packet_ptr2 = ((uint8_t *)packet_ptr2) + len;

				while (packet_len2 > 0) {
					/* fill new mbufs */
					m->next = rte_pktmbuf_alloc(
							sze_q->mb_pool);

					if (unlikely(m->next == NULL)) {
						rte_pktmbuf_free(mbuf);
						/*
						 * Restore items from sze
						 * structure to state after
						 * unlocking (eventually
						 * locking).
						 */
						sze->ct_rx_lck =
							ct_rx_lck_backup;
						sze->ct_rx_rem_bytes =
							ct_rx_rem_bytes_backup;
						sze->ct_rx_cur_ptr =
							ct_rx_cur_ptr_backup;
						goto finish;
					}

					m = m->next;

					len = RTE_MIN(rte_pktmbuf_tailroom(m),
						packet_len2);
					rte_memcpy(
						rte_pktmbuf_append(mbuf, len),
						packet_ptr2, len);

					(mbuf->nb_segs)++;
					packet_len2 -= len;
					packet_ptr2 = ((uint8_t *)packet_ptr2) +
						len;
				}
			}
		}
		mbuf->pkt_len = packet_size;
		mbuf->port = sze_q->in_port;
		bufs[num_rx] = mbuf;
		num_rx++;
		num_bytes += packet_size;
	}

finish:
	sze_q->rx_pkts += num_rx;
	sze_q->rx_bytes += num_bytes;
	return num_rx;
}

static uint16_t
eth_szedata2_tx(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	struct rte_mbuf *mbuf;
	struct szedata2_tx_queue *sze_q = queue;
	uint16_t num_tx = 0;
	uint64_t num_bytes = 0;

	const struct szedata_lock *lck;
	uint32_t lock_size;
	uint32_t lock_size2;
	void *dst;
	uint32_t pkt_len;
	uint32_t hwpkt_len;
	uint32_t unlock_size;
	uint32_t rem_len;
	uint8_t mbuf_segs;
	uint16_t pkt_left = nb_pkts;

	if (sze_q->sze == NULL || nb_pkts == 0)
		return 0;

	while (pkt_left > 0) {
		unlock_size = 0;
		lck = szedata_tx_lock_data(sze_q->sze,
			RTE_ETH_SZEDATA2_TX_LOCK_SIZE,
			sze_q->tx_channel);
		if (lck == NULL)
			continue;

		dst = lck->start;
		lock_size = lck->len;
		lock_size2 = lck->next ? lck->next->len : 0;

next_packet:
		mbuf = bufs[nb_pkts - pkt_left];

		pkt_len = mbuf->pkt_len;
		mbuf_segs = mbuf->nb_segs;

		hwpkt_len = RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED +
			RTE_SZE2_ALIGN8(pkt_len);

		if (lock_size + lock_size2 < hwpkt_len) {
			szedata_tx_unlock_data(sze_q->sze, lck, unlock_size);
			continue;
		}

		num_bytes += pkt_len;

		if (lock_size > hwpkt_len) {
			void *tmp_dst;

			rem_len = 0;

			/* write packet length at first 2 bytes in 8B header */
			*((uint16_t *)dst) = htole16(
					RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED +
					pkt_len);
			*(((uint16_t *)dst) + 1) = htole16(0);

			/* copy packet from mbuf */
			tmp_dst = ((uint8_t *)(dst)) +
				RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED;
			if (mbuf_segs == 1) {
				/*
				 * non-scattered packet,
				 * transmit from one mbuf
				 */
				rte_memcpy(tmp_dst,
					rte_pktmbuf_mtod(mbuf, const void *),
					pkt_len);
			} else {
				/* scattered packet, transmit from more mbufs */
				struct rte_mbuf *m = mbuf;
				while (m) {
					rte_memcpy(tmp_dst,
						rte_pktmbuf_mtod(m,
						const void *),
						m->data_len);
					tmp_dst = ((uint8_t *)(tmp_dst)) +
						m->data_len;
					m = m->next;
				}
			}


			dst = ((uint8_t *)dst) + hwpkt_len;
			unlock_size += hwpkt_len;
			lock_size -= hwpkt_len;

			rte_pktmbuf_free(mbuf);
			num_tx++;
			pkt_left--;
			if (pkt_left == 0) {
				szedata_tx_unlock_data(sze_q->sze, lck,
					unlock_size);
				break;
			}
			goto next_packet;
		} else if (lock_size + lock_size2 >= hwpkt_len) {
			void *tmp_dst;
			uint16_t write_len;

			/* write packet length at first 2 bytes in 8B header */
			*((uint16_t *)dst) =
				htole16(RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED +
					pkt_len);
			*(((uint16_t *)dst) + 1) = htole16(0);

			/*
			 * If the raw packet (pkt_len) is smaller than lock_size
			 * get the correct length for memcpy
			 */
			write_len =
				pkt_len < lock_size -
				RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED ?
				pkt_len :
				lock_size - RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED;

			rem_len = hwpkt_len - lock_size;

			tmp_dst = ((uint8_t *)(dst)) +
				RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED;
			if (mbuf_segs == 1) {
				/*
				 * non-scattered packet,
				 * transmit from one mbuf
				 */
				/* copy part of packet to first area */
				rte_memcpy(tmp_dst,
					rte_pktmbuf_mtod(mbuf, const void *),
					write_len);

				if (lck->next)
					dst = lck->next->start;

				/* copy part of packet to second area */
				rte_memcpy(dst,
					(const void *)(rte_pktmbuf_mtod(mbuf,
							const uint8_t *) +
					write_len), pkt_len - write_len);
			} else {
				/* scattered packet, transmit from more mbufs */
				struct rte_mbuf *m = mbuf;
				uint16_t written = 0;
				uint16_t to_write = 0;
				bool new_mbuf = true;
				uint16_t write_off = 0;

				/* copy part of packet to first area */
				while (m && written < write_len) {
					to_write = RTE_MIN(m->data_len,
							write_len - written);
					rte_memcpy(tmp_dst,
						rte_pktmbuf_mtod(m,
							const void *),
						to_write);

					tmp_dst = ((uint8_t *)(tmp_dst)) +
						to_write;
					if (m->data_len <= write_len -
							written) {
						m = m->next;
						new_mbuf = true;
					} else {
						new_mbuf = false;
					}
					written += to_write;
				}

				if (lck->next)
					dst = lck->next->start;

				tmp_dst = dst;
				written = 0;
				write_off = new_mbuf ? 0 : to_write;

				/* copy part of packet to second area */
				while (m && written < pkt_len - write_len) {
					rte_memcpy(tmp_dst, (const void *)
						(rte_pktmbuf_mtod(m,
						uint8_t *) + write_off),
						m->data_len - write_off);

					tmp_dst = ((uint8_t *)(tmp_dst)) +
						(m->data_len - write_off);
					written += m->data_len - write_off;
					m = m->next;
					write_off = 0;
				}
			}

			dst = ((uint8_t *)dst) + rem_len;
			unlock_size += hwpkt_len;
			lock_size = lock_size2 - rem_len;
			lock_size2 = 0;

			rte_pktmbuf_free(mbuf);
			num_tx++;
		}

		szedata_tx_unlock_data(sze_q->sze, lck, unlock_size);
		pkt_left--;
	}

	sze_q->tx_pkts += num_tx;
	sze_q->err_pkts += nb_pkts - num_tx;
	sze_q->tx_bytes += num_bytes;
	return num_tx;
}

static int
init_rx_channels(struct rte_eth_dev *dev, int v)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;
	uint32_t i;
	uint32_t count = internals->num_of_rx;
	uint32_t num_sub = 0;
	uint32_t x;
	uint32_t rx;
	uint32_t tx;

	rx = internals->sze_rx_req;
	tx = 0;

	for (i = 0; i < count; i++) {
		/*
		 * Open, subscribe rx,tx channels and start device
		 */
		if (v)
			RTE_LOG(INFO, PMD, "Opening SZE device %u. time\n", i);

		internals->rx_queue[num_sub].sze =
			szedata_open(internals->sze_dev);
		if (internals->rx_queue[num_sub].sze == NULL)
			return -1;

		/* separate least significant non-zero bit */
		x = rx & ((~rx) + 1);

		if (v)
			RTE_LOG(INFO, PMD, "Subscribing rx channel: 0x%x "
				"tx channel: 0x%x\n", x, tx);

		ret = szedata_subscribe3(internals->rx_queue[num_sub].sze,
				&x, &tx);
		if (ret) {
			szedata_close(internals->rx_queue[num_sub].sze);
			internals->rx_queue[num_sub].sze = NULL;
			return -1;
		}

		if (v)
			RTE_LOG(INFO, PMD, "Subscribed rx channel: 0x%x "
				"tx channel: 0x%x\n", x, tx);

		if (x) {
			if (v)
				RTE_LOG(INFO, PMD, "Starting SZE device for "
					"rx queue: %u\n", num_sub);

			ret = szedata_start(internals->rx_queue[num_sub].sze);
			if (ret) {
				szedata_close(internals->rx_queue[num_sub].sze);
				internals->rx_queue[num_sub].sze = NULL;
				return -1;
			}

			/*
			 * set to 1 all bits lower than bit set to 1
			 * and that bit to 0
			 */
			x -= 1;
			internals->rx_queue[num_sub].rx_channel =
				count_ones(x);

			if (v)
				RTE_LOG(INFO, PMD, "Subscribed rx channel "
					"no: %u\n",
					internals->rx_queue[num_sub].rx_channel
					);

			num_sub++;
			internals->nb_rx_queues = num_sub;
		} else {
			if (v)
				RTE_LOG(INFO, PMD,
					"Could not subscribe any rx channel. "
					"Closing SZE device\n");

			szedata_close(internals->rx_queue[num_sub].sze);
			internals->rx_queue[num_sub].sze = NULL;
		}

		/* set least significant non-zero bit to zero */
		rx = rx & (rx - 1);
	}

	dev->data->nb_rx_queues = (uint16_t)num_sub;

	if (v)
		RTE_LOG(INFO, PMD, "Successfully opened rx channels: %u\n",
			num_sub);

	return 0;
}

static int
init_tx_channels(struct rte_eth_dev *dev, int v)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;
	uint32_t i;
	uint32_t count = internals->num_of_tx;
	uint32_t num_sub = 0;
	uint32_t x;
	uint32_t rx;
	uint32_t tx;

	rx = 0;
	tx = internals->sze_tx_req;

	for (i = 0; i < count; i++) {
		/*
		 * Open, subscribe rx,tx channels and start device
		 */
		if (v)
			RTE_LOG(INFO, PMD, "Opening SZE device %u. time\n",
				i + internals->num_of_rx);

		internals->tx_queue[num_sub].sze =
			szedata_open(internals->sze_dev);
		if (internals->tx_queue[num_sub].sze == NULL)
			return -1;

		/* separate least significant non-zero bit */
		x = tx & ((~tx) + 1);

		if (v)
			RTE_LOG(INFO, PMD, "Subscribing rx channel: 0x%x "
				"tx channel: 0x%x\n", rx, x);

		ret = szedata_subscribe3(internals->tx_queue[num_sub].sze,
				&rx, &x);
		if (ret) {
			szedata_close(internals->tx_queue[num_sub].sze);
			internals->tx_queue[num_sub].sze = NULL;
			return -1;
		}

		if (v)
			RTE_LOG(INFO, PMD, "Subscribed rx channel: 0x%x "
				"tx channel: 0x%x\n", rx, x);

		if (x) {
			if (v)
				RTE_LOG(INFO, PMD, "Starting SZE device for "
					"tx queue: %u\n", num_sub);

			ret = szedata_start(internals->tx_queue[num_sub].sze);
			if (ret) {
				szedata_close(internals->tx_queue[num_sub].sze);
				internals->tx_queue[num_sub].sze = NULL;
				return -1;
			}

			/*
			 * set to 1 all bits lower than bit set to 1
			 * and that bit to 0
			 */
			x -= 1;
			internals->tx_queue[num_sub].tx_channel =
				count_ones(x);

			if (v)
				RTE_LOG(INFO, PMD, "Subscribed tx channel "
					"no: %u\n",
					internals->tx_queue[num_sub].tx_channel
					);

			num_sub++;
			internals->nb_tx_queues = num_sub;
		} else {
			if (v)
				RTE_LOG(INFO, PMD,
					"Could not subscribe any tx channel. "
					"Closing SZE device\n");

			szedata_close(internals->tx_queue[num_sub].sze);
			internals->tx_queue[num_sub].sze = NULL;
		}

		/* set least significant non-zero bit to zero */
		tx = tx & (tx - 1);
	}

	dev->data->nb_tx_queues = (uint16_t)num_sub;

	if (v)
		RTE_LOG(INFO, PMD, "Successfully opened tx channels: %u\n",
			num_sub);

	return 0;
}

static void
close_rx_channels(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint32_t i;
	uint32_t num_sub = internals->nb_rx_queues;

	for (i = 0; i < num_sub; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}
	/* set number of rx queues to zero */
	internals->nb_rx_queues = 0;
	dev->data->nb_rx_queues = (uint16_t)0;
}

static void
close_tx_channels(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint32_t i;
	uint32_t num_sub = internals->nb_tx_queues;

	for (i = 0; i < num_sub; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}
	/* set number of rx queues to zero */
	internals->nb_tx_queues = 0;
	dev->data->nb_tx_queues = (uint16_t)0;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;

	if (internals->nb_rx_queues == 0) {
		ret = init_rx_channels(dev, 0);
		if (ret != 0) {
			close_rx_channels(dev);
			return -1;
		}
	}

	if (internals->nb_tx_queues == 0) {
		ret = init_tx_channels(dev, 0);
		if (ret != 0) {
			close_tx_channels(dev);
			close_rx_channels(dev);
			return -1;
		}
	}

	dev->data->dev_link.link_status = 1;
	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < internals->nb_rx_queues; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}

	for (i = 0; i < internals->nb_tx_queues; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}

	internals->nb_rx_queues = 0;
	internals->nb_tx_queues = 0;

	dev->data->nb_rx_queues = (uint16_t)0;
	dev->data->nb_tx_queues = (uint16_t)0;

	dev->data->dev_link.link_status = 0;
}

static int
eth_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	if (data->dev_conf.rxmode.enable_scatter == 1) {
		dev->rx_pkt_burst = eth_szedata2_rx_scattered;
		data->scattered_rx = 1;
	} else {
		dev->rx_pkt_burst = eth_szedata2_rx;
		data->scattered_rx = 0;
	}
	return 0;
}

static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev_info->driver_name = drivername;
	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = (uint16_t)internals->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)internals->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static void
eth_stats_get(struct rte_eth_dev *dev,
		struct rte_eth_stats *stats)
{
	unsigned i;
	uint64_t rx_total = 0;
	uint64_t tx_total = 0;
	uint64_t tx_err_total = 0;
	uint64_t rx_total_bytes = 0;
	uint64_t tx_total_bytes = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_rx_queues; i++) {
		stats->q_ipackets[i] = internal->rx_queue[i].rx_pkts;
		stats->q_ibytes[i] = internal->rx_queue[i].rx_bytes;
		rx_total += stats->q_ipackets[i];
		rx_total_bytes += stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_tx_queues; i++) {
		stats->q_opackets[i] = internal->tx_queue[i].tx_pkts;
		stats->q_errors[i] = internal->tx_queue[i].err_pkts;
		stats->q_obytes[i] = internal->tx_queue[i].tx_bytes;
		tx_total += stats->q_opackets[i];
		tx_err_total += stats->q_errors[i];
		tx_total_bytes += stats->q_obytes[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
	stats->oerrors = tx_err_total;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal = dev->data->dev_private;
	for (i = 0; i < internal->nb_rx_queues; i++) {
		internal->rx_queue[i].rx_pkts = 0;
		internal->rx_queue[i].rx_bytes = 0;
	}
	for (i = 0; i < internal->nb_tx_queues; i++) {
		internal->tx_queue[i].tx_pkts = 0;
		internal->tx_queue[i].err_pkts = 0;
		internal->tx_queue[i].tx_bytes = 0;
	}
}

static void
eth_dev_close(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < internals->nb_rx_queues; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}

	for (i = 0; i < internals->nb_tx_queues; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}

	internals->nb_rx_queues = 0;
	internals->nb_tx_queues = 0;

	dev->data->nb_rx_queues = (uint16_t)0;
	dev->data->nb_tx_queues = (uint16_t)0;
}

static void
eth_queue_release(void *q __rte_unused)
{
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct szedata2_rx_queue *szedata2_q =
		&internals->rx_queue[rx_queue_id];
	szedata2_q->mb_pool = mb_pool;
	dev->data->rx_queues[rx_queue_id] = szedata2_q;
	szedata2_q->in_port = dev->data->port_id;
	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id] = &internals->tx_queue[tx_queue_id];
	return 0;
}

static void
eth_mac_addr_set(struct rte_eth_dev *dev __rte_unused,
		struct ether_addr *mac_addr __rte_unused)
{
}

static struct eth_dev_ops ops = {
		.dev_start          = eth_dev_start,
		.dev_stop           = eth_dev_stop,
		.dev_close          = eth_dev_close,
		.dev_configure      = eth_dev_configure,
		.dev_infos_get      = eth_dev_info,
		.rx_queue_setup     = eth_rx_queue_setup,
		.tx_queue_setup     = eth_tx_queue_setup,
		.rx_queue_release   = eth_queue_release,
		.tx_queue_release   = eth_queue_release,
		.link_update        = eth_link_update,
		.stats_get          = eth_stats_get,
		.stats_reset        = eth_stats_reset,
		.mac_addr_set       = eth_mac_addr_set,
};

static int
parse_mask(const char *mask_str, uint32_t *mask_num)
{
	char *endptr;
	long int value;

	value = strtol(mask_str, &endptr, 0);
	if (*endptr != '\0' || value > UINT32_MAX || value < 0)
		return -1;

	*mask_num = (uint32_t)value;
	return 0;
}

static int
add_rx_mask(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rxtx_szedata2 *szedata2 = extra_args;
	uint32_t mask;

	if (parse_mask(value, &mask) != 0)
		return -1;

	szedata2->sze_rx_mask_req |= mask;
	return 0;
}

static int
add_tx_mask(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rxtx_szedata2 *szedata2 = extra_args;
	uint32_t mask;

	if (parse_mask(value, &mask) != 0)
		return -1;

	szedata2->sze_tx_mask_req |= mask;
	return 0;
}

static int
rte_pmd_init_internals(const char *name, const unsigned nb_rx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node,
		struct pmd_internals **internals,
		struct rte_eth_dev **eth_dev)
{
	struct rte_eth_dev_data *data = NULL;

	RTE_LOG(INFO, PMD,
			"Creating szedata2-backed ethdev on numa socket %u\n",
			numa_node);

	/*
	 * now do all data allocation - for eth_dev structure
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto error;

	*internals = rte_zmalloc_socket(name, sizeof(**internals), 0,
			numa_node);
	if (*internals == NULL)
		goto error;

	/* reserve an ethdev entry */
	*eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (*eth_dev == NULL)
		goto error;

	/*
	 * now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in pci_driver
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 *
	 * NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the rings are local per-process
	 */

	(*internals)->nb_rx_queues = nb_rx_queues;
	(*internals)->nb_tx_queues = nb_tx_queues;

	(*internals)->if_index = 0;

	data->dev_private = *internals;
	data->port_id = (*eth_dev)->data->port_id;
	snprintf(data->name, sizeof(data->name), "%s", (*eth_dev)->data->name);
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &eth_addr;

	(*eth_dev)->data = data;
	(*eth_dev)->dev_ops = &ops;
	(*eth_dev)->data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	(*eth_dev)->driver = NULL;
	(*eth_dev)->data->kdrv = RTE_KDRV_NONE;
	(*eth_dev)->data->drv_name = drivername;
	(*eth_dev)->data->numa_node = numa_node;

	return 0;

error:
	rte_free(data);
	rte_free(*internals);
	return -1;
}

static int
rte_eth_from_szedata2(const char *name,
		struct rxtx_szedata2 *szedata2,
		const unsigned numa_node)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_eth_dev_data *data = NULL;
	int ret;

	if (rte_pmd_init_internals(name, 0, 0, numa_node,
			&internals, &eth_dev) < 0)
		return -1;

	data = eth_dev->data;

	internals->sze_dev = szedata2->sze_dev;
	internals->sze_rx_req = szedata2->sze_rx_mask_req;
	internals->sze_tx_req = szedata2->sze_tx_mask_req;
	internals->num_of_rx = szedata2->num_of_rx;
	internals->num_of_tx = szedata2->num_of_tx;

	RTE_LOG(INFO, PMD, "Number of rx channels to open: %u mask: 0x%x\n",
			internals->num_of_rx, internals->sze_rx_req);
	RTE_LOG(INFO, PMD, "Number of tx channels to open: %u mask: 0x%x\n",
			internals->num_of_tx, internals->sze_tx_req);

	ret = init_rx_channels(eth_dev, 1);
	if (ret != 0) {
		close_rx_channels(eth_dev);
		return -1;
	}

	ret = init_tx_channels(eth_dev, 1);
	if (ret != 0) {
		close_tx_channels(eth_dev);
		close_rx_channels(eth_dev);
		return -1;
	}

	if (data->dev_conf.rxmode.enable_scatter == 1 ||
		data->scattered_rx == 1) {
		eth_dev->rx_pkt_burst = eth_szedata2_rx_scattered;
		data->scattered_rx = 1;
	} else {
		eth_dev->rx_pkt_burst = eth_szedata2_rx;
		data->scattered_rx = 0;
	}
	eth_dev->tx_pkt_burst = eth_szedata2_tx;

	return 0;
}


static int
rte_pmd_szedata2_devinit(const char *name, const char *params)
{
	unsigned numa_node;
	int ret;
	struct rte_kvargs *kvlist;
	unsigned k_idx;
	struct rte_kvargs_pair *pair = NULL;
	struct rxtx_szedata2 szedata2 = { 0, 0, 0, 0, NULL };
	bool dev_path_missing = true;

	RTE_LOG(INFO, PMD, "Initializing pmd_szedata2 for %s\n", name);

	numa_node = rte_socket_id();

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return -1;

	/*
	 * Get szedata2 device path and rx,tx channels from passed arguments.
	 */

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_DEV_PATH_ARG) != 1)
		goto err;

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_RX_IFACES_ARG) < 1)
		goto err;

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_TX_IFACES_ARG) < 1)
		goto err;

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		if (strstr(pair->key, RTE_ETH_SZEDATA2_DEV_PATH_ARG) != NULL) {
			szedata2.sze_dev = pair->value;
			dev_path_missing = false;
			break;
		}
	}

	if (dev_path_missing)
		goto err;

	ret = rte_kvargs_process(kvlist, RTE_ETH_SZEDATA2_RX_IFACES_ARG,
			&add_rx_mask, &szedata2);
	if (ret < 0)
		goto err;

	ret = rte_kvargs_process(kvlist, RTE_ETH_SZEDATA2_TX_IFACES_ARG,
			&add_tx_mask, &szedata2);
	if (ret < 0)
		goto err;

	szedata2.num_of_rx = count_ones(szedata2.sze_rx_mask_req);
	szedata2.num_of_tx = count_ones(szedata2.sze_tx_mask_req);

	RTE_LOG(INFO, PMD, "SZE device found at path %s\n", szedata2.sze_dev);

	return rte_eth_from_szedata2(name, &szedata2, numa_node);
err:
	rte_kvargs_free(kvlist);
	return -1;
}

static int
rte_pmd_szedata2_devuninit(const char *name)
{
	struct rte_eth_dev *dev = NULL;

	RTE_LOG(INFO, PMD, "Uninitializing pmd_szedata2 for %s "
			"on numa socket %u\n", name, rte_socket_id());

	if (name == NULL)
		return -1;

	dev = rte_eth_dev_allocated(name);
	if (dev == NULL)
		return -1;

	rte_free(dev->data->dev_private);
	rte_free(dev->data);
	rte_eth_dev_release_port(dev);
	return 0;
}

static struct rte_driver pmd_szedata2_drv = {
	.name = "eth_szedata2",
	.type = PMD_VDEV,
	.init = rte_pmd_szedata2_devinit,
	.uninit = rte_pmd_szedata2_devuninit,
};

PMD_REGISTER_DRIVER(pmd_szedata2_drv);
