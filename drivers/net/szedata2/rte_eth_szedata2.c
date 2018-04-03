/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015 - 2016 CESNET
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
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <libsze2.h>

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include "rte_eth_szedata2.h"
#include "szedata2_iobuf.h"

#define RTE_ETH_SZEDATA2_MAX_RX_QUEUES 32
#define RTE_ETH_SZEDATA2_MAX_TX_QUEUES 32
#define RTE_ETH_SZEDATA2_TX_LOCK_SIZE (32 * 1024 * 1024)

/**
 * size of szedata2_packet header with alignment
 */
#define RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED 8

#define RTE_SZEDATA2_DRIVER_NAME net_szedata2

#define SZEDATA2_DEV_PATH_FMT "/dev/szedataII%u"

struct szedata2_rx_queue {
	struct szedata *sze;
	uint8_t rx_channel;
	uint16_t in_port;
	struct rte_mempool *mb_pool;
	volatile uint64_t rx_pkts;
	volatile uint64_t rx_bytes;
	volatile uint64_t err_pkts;
};

struct szedata2_tx_queue {
	struct szedata *sze;
	uint8_t tx_channel;
	volatile uint64_t tx_pkts;
	volatile uint64_t tx_bytes;
	volatile uint64_t err_pkts;
};

struct pmd_internals {
	struct szedata2_rx_queue rx_queue[RTE_ETH_SZEDATA2_MAX_RX_QUEUES];
	struct szedata2_tx_queue tx_queue[RTE_ETH_SZEDATA2_MAX_TX_QUEUES];
	uint16_t max_rx_queues;
	uint16_t max_tx_queues;
	char sze_dev[PATH_MAX];
	struct rte_mem_resource *pci_rsc;
};

static struct ether_addr eth_addr = {
	.addr_bytes = { 0x00, 0x11, 0x17, 0x00, 0x00, 0x00 }
};

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
	uint16_t mbuf_segs;
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
eth_rx_queue_start(struct rte_eth_dev *dev, uint16_t rxq_id)
{
	struct szedata2_rx_queue *rxq = dev->data->rx_queues[rxq_id];
	int ret;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	if (rxq->sze == NULL) {
		uint32_t rx = 1 << rxq->rx_channel;
		uint32_t tx = 0;
		rxq->sze = szedata_open(internals->sze_dev);
		if (rxq->sze == NULL)
			return -EINVAL;
		ret = szedata_subscribe3(rxq->sze, &rx, &tx);
		if (ret != 0 || rx == 0)
			goto err;
	}

	ret = szedata_start(rxq->sze);
	if (ret != 0)
		goto err;
	dev->data->rx_queue_state[rxq_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;

err:
	szedata_close(rxq->sze);
	rxq->sze = NULL;
	return -EINVAL;
}

static int
eth_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rxq_id)
{
	struct szedata2_rx_queue *rxq = dev->data->rx_queues[rxq_id];

	if (rxq->sze != NULL) {
		szedata_close(rxq->sze);
		rxq->sze = NULL;
	}

	dev->data->rx_queue_state[rxq_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int
eth_tx_queue_start(struct rte_eth_dev *dev, uint16_t txq_id)
{
	struct szedata2_tx_queue *txq = dev->data->tx_queues[txq_id];
	int ret;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	if (txq->sze == NULL) {
		uint32_t rx = 0;
		uint32_t tx = 1 << txq->tx_channel;
		txq->sze = szedata_open(internals->sze_dev);
		if (txq->sze == NULL)
			return -EINVAL;
		ret = szedata_subscribe3(txq->sze, &rx, &tx);
		if (ret != 0 || tx == 0)
			goto err;
	}

	ret = szedata_start(txq->sze);
	if (ret != 0)
		goto err;
	dev->data->tx_queue_state[txq_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;

err:
	szedata_close(txq->sze);
	txq->sze = NULL;
	return -EINVAL;
}

static int
eth_tx_queue_stop(struct rte_eth_dev *dev, uint16_t txq_id)
{
	struct szedata2_tx_queue *txq = dev->data->tx_queues[txq_id];

	if (txq->sze != NULL) {
		szedata_close(txq->sze);
		txq->sze = NULL;
	}

	dev->data->tx_queue_state[txq_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	for (i = 0; i < nb_rx; i++) {
		ret = eth_rx_queue_start(dev, i);
		if (ret != 0)
			goto err_rx;
	}

	for (i = 0; i < nb_tx; i++) {
		ret = eth_tx_queue_start(dev, i);
		if (ret != 0)
			goto err_tx;
	}

	return 0;

err_tx:
	for (i = 0; i < nb_tx; i++)
		eth_tx_queue_stop(dev, i);
err_rx:
	for (i = 0; i < nb_rx; i++)
		eth_rx_queue_stop(dev, i);
	return ret;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	for (i = 0; i < nb_tx; i++)
		eth_tx_queue_stop(dev, i);

	for (i = 0; i < nb_rx; i++)
		eth_rx_queue_stop(dev, i);
}

static int
eth_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	if (data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_SCATTER) {
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

	dev_info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	dev_info->if_index = 0;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = internals->max_rx_queues;
	dev_info->max_tx_queues = internals->max_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_SCATTER;
	dev_info->tx_offload_capa = 0;
	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_queue_offload_capa = 0;
	dev_info->speed_capa = ETH_LINK_SPEED_100G;
}

static int
eth_stats_get(struct rte_eth_dev *dev,
		struct rte_eth_stats *stats)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;
	uint64_t rx_total = 0;
	uint64_t tx_total = 0;
	uint64_t tx_err_total = 0;
	uint64_t rx_total_bytes = 0;
	uint64_t tx_total_bytes = 0;
	const struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS && i < nb_rx; i++) {
		stats->q_ipackets[i] = internals->rx_queue[i].rx_pkts;
		stats->q_ibytes[i] = internals->rx_queue[i].rx_bytes;
		rx_total += stats->q_ipackets[i];
		rx_total_bytes += stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS && i < nb_tx; i++) {
		stats->q_opackets[i] = internals->tx_queue[i].tx_pkts;
		stats->q_obytes[i] = internals->tx_queue[i].tx_bytes;
		stats->q_errors[i] = internals->tx_queue[i].err_pkts;
		tx_total += stats->q_opackets[i];
		tx_total_bytes += stats->q_obytes[i];
		tx_err_total += stats->q_errors[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
	stats->oerrors = tx_err_total;

	return 0;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < nb_rx; i++) {
		internals->rx_queue[i].rx_pkts = 0;
		internals->rx_queue[i].rx_bytes = 0;
		internals->rx_queue[i].err_pkts = 0;
	}
	for (i = 0; i < nb_tx; i++) {
		internals->tx_queue[i].tx_pkts = 0;
		internals->tx_queue[i].tx_bytes = 0;
		internals->tx_queue[i].err_pkts = 0;
	}
}

static void
eth_rx_queue_release(void *q)
{
	struct szedata2_rx_queue *rxq = (struct szedata2_rx_queue *)q;
	if (rxq->sze != NULL) {
		szedata_close(rxq->sze);
		rxq->sze = NULL;
	}
}

static void
eth_tx_queue_release(void *q)
{
	struct szedata2_tx_queue *txq = (struct szedata2_tx_queue *)q;
	if (txq->sze != NULL) {
		szedata_close(txq->sze);
		txq->sze = NULL;
	}
}

static void
eth_dev_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	eth_dev_stop(dev);

	for (i = 0; i < nb_rx; i++) {
		eth_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;
	for (i = 0; i < nb_tx; i++) {
		eth_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

/**
 * Function takes value from first IBUF status register.
 * Values in IBUF and OBUF should be same.
 *
 * @param internals
 *     Pointer to device private structure.
 * @return
 *     Link speed constant.
 */
static inline enum szedata2_link_speed
get_link_speed(const struct pmd_internals *internals)
{
	const volatile struct szedata2_ibuf *ibuf =
		ibuf_ptr_by_index(internals->pci_rsc, 0);
	uint32_t speed = (szedata2_read32(&ibuf->ibuf_st) & 0x70) >> 4;
	switch (speed) {
	case 0x03:
		return SZEDATA2_LINK_SPEED_10G;
	case 0x04:
		return SZEDATA2_LINK_SPEED_40G;
	case 0x05:
		return SZEDATA2_LINK_SPEED_100G;
	default:
		return SZEDATA2_LINK_SPEED_DEFAULT;
	}
}

static int
eth_link_update(struct rte_eth_dev *dev,
		int wait_to_complete __rte_unused)
{
	struct rte_eth_link link;
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	const volatile struct szedata2_ibuf *ibuf;
	uint32_t i;
	bool link_is_up = false;

	memset(&link, 0, sizeof(link));

	switch (get_link_speed(internals)) {
	case SZEDATA2_LINK_SPEED_10G:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case SZEDATA2_LINK_SPEED_40G:
		link.link_speed = ETH_SPEED_NUM_40G;
		break;
	case SZEDATA2_LINK_SPEED_100G:
		link.link_speed = ETH_SPEED_NUM_100G;
		break;
	default:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	}

	/* szedata2 uses only full duplex */
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	for (i = 0; i < szedata2_ibuf_count; i++) {
		ibuf = ibuf_ptr_by_index(internals->pci_rsc, i);
		/*
		 * Link is considered up if at least one ibuf is enabled
		 * and up.
		 */
		if (ibuf_is_enabled(ibuf) && ibuf_is_link_up(ibuf)) {
			link_is_up = true;
			break;
		}
	}

	link.link_status = link_is_up ? ETH_LINK_UP : ETH_LINK_DOWN;

	link.link_autoneg = ETH_LINK_FIXED;

	rte_eth_linkstatus_set(dev, &link);
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++)
		ibuf_enable(ibuf_ptr_by_index(internals->pci_rsc, i));
	for (i = 0; i < szedata2_obuf_count; i++)
		obuf_enable(obuf_ptr_by_index(internals->pci_rsc, i));
	return 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++)
		ibuf_disable(ibuf_ptr_by_index(internals->pci_rsc, i));
	for (i = 0; i < szedata2_obuf_count; i++)
		obuf_disable(obuf_ptr_by_index(internals->pci_rsc, i));
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
	struct szedata2_rx_queue *rxq = &internals->rx_queue[rx_queue_id];
	int ret;
	uint32_t rx = 1 << rx_queue_id;
	uint32_t tx = 0;

	rxq->sze = szedata_open(internals->sze_dev);
	if (rxq->sze == NULL)
		return -EINVAL;
	ret = szedata_subscribe3(rxq->sze, &rx, &tx);
	if (ret != 0 || rx == 0) {
		szedata_close(rxq->sze);
		rxq->sze = NULL;
		return -EINVAL;
	}
	rxq->rx_channel = rx_queue_id;
	rxq->in_port = dev->data->port_id;
	rxq->mb_pool = mb_pool;
	rxq->rx_pkts = 0;
	rxq->rx_bytes = 0;
	rxq->err_pkts = 0;

	dev->data->rx_queues[rx_queue_id] = rxq;
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
	struct szedata2_tx_queue *txq = &internals->tx_queue[tx_queue_id];
	int ret;
	uint32_t rx = 0;
	uint32_t tx = 1 << tx_queue_id;

	txq->sze = szedata_open(internals->sze_dev);
	if (txq->sze == NULL)
		return -EINVAL;
	ret = szedata_subscribe3(txq->sze, &rx, &tx);
	if (ret != 0 || tx == 0) {
		szedata_close(txq->sze);
		txq->sze = NULL;
		return -EINVAL;
	}
	txq->tx_channel = tx_queue_id;
	txq->tx_pkts = 0;
	txq->tx_bytes = 0;
	txq->err_pkts = 0;

	dev->data->tx_queues[tx_queue_id] = txq;
	return 0;
}

static void
eth_mac_addr_set(struct rte_eth_dev *dev __rte_unused,
		struct ether_addr *mac_addr __rte_unused)
{
}

static void
eth_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++) {
		ibuf_mac_mode_write(ibuf_ptr_by_index(internals->pci_rsc, i),
				SZEDATA2_MAC_CHMODE_PROMISC);
	}
}

static void
eth_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++) {
		ibuf_mac_mode_write(ibuf_ptr_by_index(internals->pci_rsc, i),
				SZEDATA2_MAC_CHMODE_ONLY_VALID);
	}
}

static void
eth_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++) {
		ibuf_mac_mode_write(ibuf_ptr_by_index(internals->pci_rsc, i),
				SZEDATA2_MAC_CHMODE_ALL_MULTICAST);
	}
}

static void
eth_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < szedata2_ibuf_count; i++) {
		ibuf_mac_mode_write(ibuf_ptr_by_index(internals->pci_rsc, i),
				SZEDATA2_MAC_CHMODE_ONLY_VALID);
	}
}

static const struct eth_dev_ops ops = {
	.dev_start          = eth_dev_start,
	.dev_stop           = eth_dev_stop,
	.dev_set_link_up    = eth_dev_set_link_up,
	.dev_set_link_down  = eth_dev_set_link_down,
	.dev_close          = eth_dev_close,
	.dev_configure      = eth_dev_configure,
	.dev_infos_get      = eth_dev_info,
	.promiscuous_enable   = eth_promiscuous_enable,
	.promiscuous_disable  = eth_promiscuous_disable,
	.allmulticast_enable  = eth_allmulticast_enable,
	.allmulticast_disable = eth_allmulticast_disable,
	.rx_queue_start     = eth_rx_queue_start,
	.rx_queue_stop      = eth_rx_queue_stop,
	.tx_queue_start     = eth_tx_queue_start,
	.tx_queue_stop      = eth_tx_queue_stop,
	.rx_queue_setup     = eth_rx_queue_setup,
	.tx_queue_setup     = eth_tx_queue_setup,
	.rx_queue_release   = eth_rx_queue_release,
	.tx_queue_release   = eth_tx_queue_release,
	.link_update        = eth_link_update,
	.stats_get          = eth_stats_get,
	.stats_reset        = eth_stats_reset,
	.mac_addr_set       = eth_mac_addr_set,
};

/*
 * This function goes through sysfs and looks for an index of szedata2
 * device file (/dev/szedataIIX, where X is the index).
 *
 * @return
 *           0 on success
 *          -1 on error
 */
static int
get_szedata2_index(const struct rte_pci_addr *pcislot_addr, uint32_t *index)
{
	DIR *dir;
	struct dirent *entry;
	int ret;
	uint32_t tmp_index;
	FILE *fd;
	char pcislot_path[PATH_MAX];
	uint32_t domain;
	uint32_t bus;
	uint32_t devid;
	uint32_t function;

	dir = opendir("/sys/class/combo");
	if (dir == NULL)
		return -1;

	/*
	 * Iterate through all combosixX directories.
	 * When the value in /sys/class/combo/combosixX/device/pcislot
	 * file is the location of the ethernet device dev, "X" is the
	 * index of the device.
	 */
	while ((entry = readdir(dir)) != NULL) {
		ret = sscanf(entry->d_name, "combosix%u", &tmp_index);
		if (ret != 1)
			continue;

		snprintf(pcislot_path, PATH_MAX,
			"/sys/class/combo/combosix%u/device/pcislot",
			tmp_index);

		fd = fopen(pcislot_path, "r");
		if (fd == NULL)
			continue;

		ret = fscanf(fd, "%4" PRIx16 ":%2" PRIx8 ":%2" PRIx8 ".%" PRIx8,
				&domain, &bus, &devid, &function);
		fclose(fd);
		if (ret != 4)
			continue;

		if (pcislot_addr->domain == domain &&
				pcislot_addr->bus == bus &&
				pcislot_addr->devid == devid &&
				pcislot_addr->function == function) {
			*index = tmp_index;
			closedir(dir);
			return 0;
		}
	}

	closedir(dir);
	return -1;
}

static int
rte_szedata2_eth_dev_init(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_internals *internals = (struct pmd_internals *)
		data->dev_private;
	struct szedata *szedata_temp;
	int ret;
	uint32_t szedata2_index;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_pci_addr *pci_addr = &pci_dev->addr;
	struct rte_mem_resource *pci_rsc =
		&pci_dev->mem_resource[PCI_RESOURCE_NUMBER];
	char rsc_filename[PATH_MAX];
	void *pci_resource_ptr = NULL;
	int fd;

	RTE_LOG(INFO, PMD, "Initializing szedata2 device (" PCI_PRI_FMT ")\n",
			pci_addr->domain, pci_addr->bus, pci_addr->devid,
			pci_addr->function);

	/* Get index of szedata2 device file and create path to device file */
	ret = get_szedata2_index(pci_addr, &szedata2_index);
	if (ret != 0) {
		RTE_LOG(ERR, PMD, "Failed to get szedata2 device index!\n");
		return -ENODEV;
	}
	snprintf(internals->sze_dev, PATH_MAX, SZEDATA2_DEV_PATH_FMT,
			szedata2_index);

	RTE_LOG(INFO, PMD, "SZEDATA2 path: %s\n", internals->sze_dev);

	/*
	 * Get number of available DMA RX and TX channels, which is maximum
	 * number of queues that can be created and store it in private device
	 * data structure.
	 */
	szedata_temp = szedata_open(internals->sze_dev);
	if (szedata_temp == NULL) {
		RTE_LOG(ERR, PMD, "szedata_open(): failed to open %s",
				internals->sze_dev);
		return -EINVAL;
	}
	internals->max_rx_queues = szedata_ifaces_available(szedata_temp,
			SZE2_DIR_RX);
	internals->max_tx_queues = szedata_ifaces_available(szedata_temp,
			SZE2_DIR_TX);
	szedata_close(szedata_temp);

	RTE_LOG(INFO, PMD, "Available DMA channels RX: %u TX: %u\n",
			internals->max_rx_queues, internals->max_tx_queues);

	/* Set rx, tx burst functions */
	if (data->scattered_rx == 1)
		dev->rx_pkt_burst = eth_szedata2_rx_scattered;
	else
		dev->rx_pkt_burst = eth_szedata2_rx;
	dev->tx_pkt_burst = eth_szedata2_tx;

	/* Set function callbacks for Ethernet API */
	dev->dev_ops = &ops;

	rte_eth_copy_pci_info(dev, pci_dev);

	/* mmap pci resource0 file to rte_mem_resource structure */
	if (pci_dev->mem_resource[PCI_RESOURCE_NUMBER].phys_addr ==
			0) {
		RTE_LOG(ERR, PMD, "Missing resource%u file\n",
				PCI_RESOURCE_NUMBER);
		return -EINVAL;
	}
	snprintf(rsc_filename, PATH_MAX,
		"%s/" PCI_PRI_FMT "/resource%u", rte_pci_get_sysfs_path(),
		pci_addr->domain, pci_addr->bus,
		pci_addr->devid, pci_addr->function, PCI_RESOURCE_NUMBER);
	fd = open(rsc_filename, O_RDWR);
	if (fd < 0) {
		RTE_LOG(ERR, PMD, "Could not open file %s\n", rsc_filename);
		return -EINVAL;
	}

	pci_resource_ptr = mmap(0,
			pci_dev->mem_resource[PCI_RESOURCE_NUMBER].len,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (pci_resource_ptr == MAP_FAILED) {
		RTE_LOG(ERR, PMD, "Could not mmap file %s (fd = %d)\n",
				rsc_filename, fd);
		return -EINVAL;
	}
	pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr = pci_resource_ptr;
	internals->pci_rsc = pci_rsc;

	RTE_LOG(DEBUG, PMD, "resource%u phys_addr = 0x%llx len = %llu "
			"virt addr = %llx\n", PCI_RESOURCE_NUMBER,
			(unsigned long long)pci_rsc->phys_addr,
			(unsigned long long)pci_rsc->len,
			(unsigned long long)pci_rsc->addr);

	/* Get link state */
	eth_link_update(dev, 0);

	/* Allocate space for one mac address */
	data->mac_addrs = rte_zmalloc(data->name, sizeof(struct ether_addr),
			RTE_CACHE_LINE_SIZE);
	if (data->mac_addrs == NULL) {
		RTE_LOG(ERR, PMD, "Could not alloc space for MAC address!\n");
		munmap(pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr,
		       pci_dev->mem_resource[PCI_RESOURCE_NUMBER].len);
		return -EINVAL;
	}

	ether_addr_copy(&eth_addr, data->mac_addrs);

	/* At initial state COMBO card is in promiscuous mode so disable it */
	eth_promiscuous_disable(dev);

	RTE_LOG(INFO, PMD, "szedata2 device ("
			PCI_PRI_FMT ") successfully initialized\n",
			pci_addr->domain, pci_addr->bus, pci_addr->devid,
			pci_addr->function);

	return 0;
}

static int
rte_szedata2_eth_dev_uninit(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_pci_addr *pci_addr = &pci_dev->addr;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;
	munmap(pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr,
	       pci_dev->mem_resource[PCI_RESOURCE_NUMBER].len);

	RTE_LOG(INFO, PMD, "szedata2 device ("
			PCI_PRI_FMT ") successfully uninitialized\n",
			pci_addr->domain, pci_addr->bus, pci_addr->devid,
			pci_addr->function);

	return 0;
}

static const struct rte_pci_id rte_szedata2_pci_id_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE,
				PCI_DEVICE_ID_NETCOPE_COMBO80G)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE,
				PCI_DEVICE_ID_NETCOPE_COMBO100G)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE,
				PCI_DEVICE_ID_NETCOPE_COMBO100G2)
	},
	{
		.vendor_id = 0,
	}
};

static int szedata2_eth_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct pmd_internals), rte_szedata2_eth_dev_init);
}

static int szedata2_eth_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev,
		rte_szedata2_eth_dev_uninit);
}

static struct rte_pci_driver szedata2_eth_driver = {
	.id_table = rte_szedata2_pci_id_table,
	.probe = szedata2_eth_pci_probe,
	.remove = szedata2_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(RTE_SZEDATA2_DRIVER_NAME, szedata2_eth_driver);
RTE_PMD_REGISTER_PCI_TABLE(RTE_SZEDATA2_DRIVER_NAME, rte_szedata2_pci_id_table);
RTE_PMD_REGISTER_KMOD_DEP(RTE_SZEDATA2_DRIVER_NAME,
	"* combo6core & combov3 & szedata2 & szedata2_cv3");
