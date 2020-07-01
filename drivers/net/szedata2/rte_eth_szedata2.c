/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 - 2016 CESNET
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
#include "szedata2_logs.h"

#define RTE_ETH_SZEDATA2_MAX_RX_QUEUES 32
#define RTE_ETH_SZEDATA2_MAX_TX_QUEUES 32
#define RTE_ETH_SZEDATA2_TX_LOCK_SIZE (32 * 1024 * 1024)

/**
 * size of szedata2_packet header with alignment
 */
#define RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED 8

#define RTE_SZEDATA2_DRIVER_NAME net_szedata2

#define SZEDATA2_DEV_PATH_FMT "/dev/szedataII%u"

/**
 * Format string for suffix used to differentiate between Ethernet ports
 * on the same PCI device.
 */
#define SZEDATA2_ETH_DEV_NAME_SUFFIX_FMT "-port%u"

/**
 * Maximum number of ports for one device.
 */
#define SZEDATA2_MAX_PORTS 2

/**
 * Entry in list of PCI devices for this driver.
 */
struct pci_dev_list_entry;
struct pci_dev_list_entry {
	LIST_ENTRY(pci_dev_list_entry) next;
	struct rte_pci_device *pci_dev;
	unsigned int port_count;
};

/* List of PCI devices with number of ports for this driver. */
LIST_HEAD(pci_dev_list, pci_dev_list_entry) szedata2_pci_dev_list =
	LIST_HEAD_INITIALIZER(szedata2_pci_dev_list);

struct port_info {
	unsigned int rx_base_id;
	unsigned int tx_base_id;
	unsigned int rx_count;
	unsigned int tx_count;
	int numa_node;
};

struct pmd_internals {
	struct rte_eth_dev *dev;
	uint16_t max_rx_queues;
	uint16_t max_tx_queues;
	unsigned int rxq_base_id;
	unsigned int txq_base_id;
	char *sze_dev_path;
};

struct szedata2_rx_queue {
	struct pmd_internals *priv;
	struct szedata *sze;
	uint8_t rx_channel;
	uint16_t qid;
	uint16_t in_port;
	struct rte_mempool *mb_pool;
	volatile uint64_t rx_pkts;
	volatile uint64_t rx_bytes;
	volatile uint64_t err_pkts;
};

struct szedata2_tx_queue {
	struct pmd_internals *priv;
	struct szedata *sze;
	uint8_t tx_channel;
	uint16_t qid;
	volatile uint64_t tx_pkts;
	volatile uint64_t tx_bytes;
	volatile uint64_t err_pkts;
};

static struct rte_ether_addr eth_addr = {
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

		if (unlikely(mbuf == NULL)) {
			sze_q->priv->dev->data->rx_mbuf_alloc_failed++;
			break;
		}

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
			PMD_DRV_LOG(ERR,
				"SZE segment %d bytes will not fit in one mbuf "
				"(%d bytes), scattered mode is not enabled, "
				"drop packet!!",
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
	uint64_t *mbuf_failed_ptr =
		&sze_q->priv->dev->data->rx_mbuf_alloc_failed;

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
			sze_q->priv->dev->data->rx_mbuf_alloc_failed++;
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
					(*mbuf_failed_ptr)++;
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
						(*mbuf_failed_ptr)++;
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
		rxq->sze = szedata_open(internals->sze_dev_path);
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
		txq->sze = szedata_open(internals->sze_dev_path);
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

static int
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

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

	return 0;
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

	for (i = 0; i < nb_rx; i++) {
		struct szedata2_rx_queue *rxq = dev->data->rx_queues[i];

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxq->rx_pkts;
			stats->q_ibytes[i] = rxq->rx_bytes;
		}
		rx_total += rxq->rx_pkts;
		rx_total_bytes += rxq->rx_bytes;
	}

	for (i = 0; i < nb_tx; i++) {
		struct szedata2_tx_queue *txq = dev->data->tx_queues[i];

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txq->tx_pkts;
			stats->q_obytes[i] = txq->tx_bytes;
		}
		tx_total += txq->tx_pkts;
		tx_total_bytes += txq->tx_bytes;
		tx_err_total += txq->err_pkts;
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
	stats->oerrors = tx_err_total;
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	for (i = 0; i < nb_rx; i++) {
		struct szedata2_rx_queue *rxq = dev->data->rx_queues[i];
		rxq->rx_pkts = 0;
		rxq->rx_bytes = 0;
		rxq->err_pkts = 0;
	}
	for (i = 0; i < nb_tx; i++) {
		struct szedata2_tx_queue *txq = dev->data->tx_queues[i];
		txq->tx_pkts = 0;
		txq->tx_bytes = 0;
		txq->err_pkts = 0;
	}

	return 0;
}

static void
eth_rx_queue_release(void *q)
{
	struct szedata2_rx_queue *rxq = (struct szedata2_rx_queue *)q;

	if (rxq != NULL) {
		if (rxq->sze != NULL)
			szedata_close(rxq->sze);
		rte_free(rxq);
	}
}

static void
eth_tx_queue_release(void *q)
{
	struct szedata2_tx_queue *txq = (struct szedata2_tx_queue *)q;

	if (txq != NULL) {
		if (txq->sze != NULL)
			szedata_close(txq->sze);
		rte_free(txq);
	}
}

static void
eth_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint16_t i;
	uint16_t nb_rx = dev->data->nb_rx_queues;
	uint16_t nb_tx = dev->data->nb_tx_queues;

	eth_dev_stop(dev);

	free(internals->sze_dev_path);

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

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;
}

static int
eth_link_update(struct rte_eth_dev *dev,
		int wait_to_complete __rte_unused)
{
	struct rte_eth_link link;

	memset(&link, 0, sizeof(link));

	link.link_speed = ETH_SPEED_NUM_100G;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_status = ETH_LINK_UP;
	link.link_autoneg = ETH_LINK_FIXED;

	rte_eth_linkstatus_set(dev, &link);
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Setting link up is not supported.");
	return 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Setting link down is not supported.");
	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct szedata2_rx_queue *rxq;
	int ret;
	struct pmd_internals *internals = dev->data->dev_private;
	uint8_t rx_channel = internals->rxq_base_id + rx_queue_id;
	uint32_t rx = 1 << rx_channel;
	uint32_t tx = 0;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->rx_queues[rx_queue_id] != NULL) {
		eth_rx_queue_release(dev->data->rx_queues[rx_queue_id]);
		dev->data->rx_queues[rx_queue_id] = NULL;
	}

	rxq = rte_zmalloc_socket("szedata2 rx queue",
			sizeof(struct szedata2_rx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc_socket() failed for rx queue id "
				"%" PRIu16 "!", rx_queue_id);
		return -ENOMEM;
	}

	rxq->priv = internals;
	rxq->sze = szedata_open(internals->sze_dev_path);
	if (rxq->sze == NULL) {
		PMD_INIT_LOG(ERR, "szedata_open() failed for rx queue id "
				"%" PRIu16 "!", rx_queue_id);
		eth_rx_queue_release(rxq);
		return -EINVAL;
	}
	ret = szedata_subscribe3(rxq->sze, &rx, &tx);
	if (ret != 0 || rx == 0) {
		PMD_INIT_LOG(ERR, "szedata_subscribe3() failed for rx queue id "
				"%" PRIu16 "!", rx_queue_id);
		eth_rx_queue_release(rxq);
		return -EINVAL;
	}
	rxq->rx_channel = rx_channel;
	rxq->qid = rx_queue_id;
	rxq->in_port = dev->data->port_id;
	rxq->mb_pool = mb_pool;
	rxq->rx_pkts = 0;
	rxq->rx_bytes = 0;
	rxq->err_pkts = 0;

	dev->data->rx_queues[rx_queue_id] = rxq;

	PMD_INIT_LOG(DEBUG, "Configured rx queue id %" PRIu16 " on socket "
			"%u (channel id %u).", rxq->qid, socket_id,
			rxq->rx_channel);

	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct szedata2_tx_queue *txq;
	int ret;
	struct pmd_internals *internals = dev->data->dev_private;
	uint8_t tx_channel = internals->txq_base_id + tx_queue_id;
	uint32_t rx = 0;
	uint32_t tx = 1 << tx_channel;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queues[tx_queue_id] != NULL) {
		eth_tx_queue_release(dev->data->tx_queues[tx_queue_id]);
		dev->data->tx_queues[tx_queue_id] = NULL;
	}

	txq = rte_zmalloc_socket("szedata2 tx queue",
			sizeof(struct szedata2_tx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc_socket() failed for tx queue id "
				"%" PRIu16 "!", tx_queue_id);
		return -ENOMEM;
	}

	txq->priv = internals;
	txq->sze = szedata_open(internals->sze_dev_path);
	if (txq->sze == NULL) {
		PMD_INIT_LOG(ERR, "szedata_open() failed for tx queue id "
				"%" PRIu16 "!", tx_queue_id);
		eth_tx_queue_release(txq);
		return -EINVAL;
	}
	ret = szedata_subscribe3(txq->sze, &rx, &tx);
	if (ret != 0 || tx == 0) {
		PMD_INIT_LOG(ERR, "szedata_subscribe3() failed for tx queue id "
				"%" PRIu16 "!", tx_queue_id);
		eth_tx_queue_release(txq);
		return -EINVAL;
	}
	txq->tx_channel = tx_channel;
	txq->qid = tx_queue_id;
	txq->tx_pkts = 0;
	txq->tx_bytes = 0;
	txq->err_pkts = 0;

	dev->data->tx_queues[tx_queue_id] = txq;

	PMD_INIT_LOG(DEBUG, "Configured tx queue id %" PRIu16 " on socket "
			"%u (channel id %u).", txq->qid, socket_id,
			txq->tx_channel);

	return 0;
}

static int
eth_mac_addr_set(struct rte_eth_dev *dev __rte_unused,
		struct rte_ether_addr *mac_addr __rte_unused)
{
	return 0;
}

static int
eth_promiscuous_enable(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Enabling promiscuous mode is not supported. "
			"The card is always in promiscuous mode.");
	return 0;
}

static int
eth_promiscuous_disable(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Disabling promiscuous mode is not supported. "
			"The card is always in promiscuous mode.");
	return -ENOTSUP;
}

static int
eth_allmulticast_enable(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Enabling allmulticast mode is not supported.");
	return -ENOTSUP;
}

static int
eth_allmulticast_disable(struct rte_eth_dev *dev __rte_unused)
{
	PMD_DRV_LOG(WARNING, "Disabling allmulticast mode is not supported.");
	return -ENOTSUP;
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
	uint8_t bus;
	uint8_t devid;
	uint8_t function;

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

		ret = fscanf(fd, "%8" SCNx32 ":%2" SCNx8 ":%2" SCNx8 ".%" SCNx8,
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

/**
 * @brief Initializes rte_eth_dev device.
 * @param dev Device to initialize.
 * @param pi Structure with info about DMA queues.
 * @return 0 on success, negative error code on error.
 */
static int
rte_szedata2_eth_dev_init(struct rte_eth_dev *dev, struct port_info *pi)
{
	int ret;
	uint32_t szedata2_index;
	char name[PATH_MAX];
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_internals *internals = (struct pmd_internals *)
		data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	PMD_INIT_FUNC_TRACE();

	PMD_INIT_LOG(INFO, "Initializing eth_dev %s (driver %s)", data->name,
			RTE_STR(RTE_SZEDATA2_DRIVER_NAME));

	/* Let rte_eth_dev_close() release the port resources */
	dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	/* Fill internal private structure. */
	internals->dev = dev;
	/* Get index of szedata2 device file and create path to device file */
	ret = get_szedata2_index(&pci_dev->addr, &szedata2_index);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to get szedata2 device index!");
		return -ENODEV;
	}
	snprintf(name, PATH_MAX, SZEDATA2_DEV_PATH_FMT, szedata2_index);
	internals->sze_dev_path = strdup(name);
	if (internals->sze_dev_path == NULL) {
		PMD_INIT_LOG(ERR, "strdup() failed!");
		return -ENOMEM;
	}
	PMD_INIT_LOG(INFO, "SZEDATA2 path: %s", internals->sze_dev_path);
	internals->max_rx_queues = pi->rx_count;
	internals->max_tx_queues = pi->tx_count;
	internals->rxq_base_id = pi->rx_base_id;
	internals->txq_base_id = pi->tx_base_id;
	PMD_INIT_LOG(INFO, "%u RX DMA channels from id %u",
			internals->max_rx_queues, internals->rxq_base_id);
	PMD_INIT_LOG(INFO, "%u TX DMA channels from id %u",
			internals->max_tx_queues, internals->txq_base_id);

	/* Set rx, tx burst functions */
	if (data->scattered_rx == 1)
		dev->rx_pkt_burst = eth_szedata2_rx_scattered;
	else
		dev->rx_pkt_burst = eth_szedata2_rx;
	dev->tx_pkt_burst = eth_szedata2_tx;

	/* Set function callbacks for Ethernet API */
	dev->dev_ops = &ops;

	/* Get link state */
	eth_link_update(dev, 0);

	/* Allocate space for one mac address */
	data->mac_addrs = rte_zmalloc(data->name, sizeof(struct rte_ether_addr),
			RTE_CACHE_LINE_SIZE);
	if (data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Could not alloc space for MAC address!");
		free(internals->sze_dev_path);
		return -ENOMEM;
	}

	rte_ether_addr_copy(&eth_addr, data->mac_addrs);

	PMD_INIT_LOG(INFO, "%s device %s successfully initialized",
			RTE_STR(RTE_SZEDATA2_DRIVER_NAME), data->name);

	return 0;
}

/**
 * @brief Unitializes rte_eth_dev device.
 * @param dev Device to uninitialize.
 * @return 0 on success, negative error code on error.
 */
static int
rte_szedata2_eth_dev_uninit(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	eth_dev_close(dev);

	PMD_DRV_LOG(INFO, "%s device %s successfully uninitialized",
			RTE_STR(RTE_SZEDATA2_DRIVER_NAME), dev->data->name);

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
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETCOPE,
				PCI_DEVICE_ID_NETCOPE_NFB200G2QL)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_SILICOM,
				PCI_DEVICE_ID_FB2CGG3)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_SILICOM,
				PCI_DEVICE_ID_FB2CGG3D)
	},
	{
		.vendor_id = 0,
	}
};

/**
 * @brief Gets info about DMA queues for ports.
 * @param pci_dev PCI device structure.
 * @param port_count Pointer to variable set with number of ports.
 * @param pi Pointer to array of structures with info about DMA queues
 *           for ports.
 * @param max_ports Maximum number of ports.
 * @return 0 on success, negative error code on error.
 */
static int
get_port_info(struct rte_pci_device *pci_dev, unsigned int *port_count,
		struct port_info *pi, unsigned int max_ports)
{
	struct szedata *szedata_temp;
	char sze_dev_path[PATH_MAX];
	uint32_t szedata2_index;
	int ret;
	uint16_t max_rx_queues;
	uint16_t max_tx_queues;

	if (max_ports == 0)
		return -EINVAL;

	memset(pi, 0, max_ports * sizeof(struct port_info));
	*port_count = 0;

	/* Get index of szedata2 device file and create path to device file */
	ret = get_szedata2_index(&pci_dev->addr, &szedata2_index);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to get szedata2 device index!");
		return -ENODEV;
	}
	snprintf(sze_dev_path, PATH_MAX, SZEDATA2_DEV_PATH_FMT, szedata2_index);

	/*
	 * Get number of available DMA RX and TX channels, which is maximum
	 * number of queues that can be created.
	 */
	szedata_temp = szedata_open(sze_dev_path);
	if (szedata_temp == NULL) {
		PMD_INIT_LOG(ERR, "szedata_open(%s) failed", sze_dev_path);
		return -EINVAL;
	}
	max_rx_queues = szedata_ifaces_available(szedata_temp, SZE2_DIR_RX);
	max_tx_queues = szedata_ifaces_available(szedata_temp, SZE2_DIR_TX);
	PMD_INIT_LOG(INFO, "Available DMA channels RX: %u TX: %u",
			max_rx_queues, max_tx_queues);
	if (max_rx_queues > RTE_ETH_SZEDATA2_MAX_RX_QUEUES) {
		PMD_INIT_LOG(ERR, "%u RX queues exceeds supported number %u",
				max_rx_queues, RTE_ETH_SZEDATA2_MAX_RX_QUEUES);
		szedata_close(szedata_temp);
		return -EINVAL;
	}
	if (max_tx_queues > RTE_ETH_SZEDATA2_MAX_TX_QUEUES) {
		PMD_INIT_LOG(ERR, "%u TX queues exceeds supported number %u",
				max_tx_queues, RTE_ETH_SZEDATA2_MAX_TX_QUEUES);
		szedata_close(szedata_temp);
		return -EINVAL;
	}

	if (pci_dev->id.device_id == PCI_DEVICE_ID_NETCOPE_NFB200G2QL) {
		unsigned int i;
		unsigned int rx_queues = max_rx_queues / max_ports;
		unsigned int tx_queues = max_tx_queues / max_ports;

		/*
		 * Number of queues reported by szedata_ifaces_available()
		 * is the number of all queues from all DMA controllers which
		 * may reside at different numa locations.
		 * All queues from the same DMA controller have the same numa
		 * node.
		 * Numa node from the first queue of each DMA controller is
		 * retrieved.
		 * If the numa node differs from the numa node of the queues
		 * from the previous DMA controller the queues are assigned
		 * to the next port.
		 */

		for (i = 0; i < max_ports; i++) {
			int numa_rx = szedata_get_area_numa_node(szedata_temp,
				SZE2_DIR_RX, rx_queues * i);
			int numa_tx = szedata_get_area_numa_node(szedata_temp,
				SZE2_DIR_TX, tx_queues * i);
			unsigned int port_rx_queues = numa_rx != -1 ?
				rx_queues : 0;
			unsigned int port_tx_queues = numa_tx != -1 ?
				tx_queues : 0;
			PMD_INIT_LOG(DEBUG, "%u rx queues from id %u, numa %d",
					rx_queues, rx_queues * i, numa_rx);
			PMD_INIT_LOG(DEBUG, "%u tx queues from id %u, numa %d",
					tx_queues, tx_queues * i, numa_tx);

			if (port_rx_queues != 0 && port_tx_queues != 0 &&
					numa_rx != numa_tx) {
				PMD_INIT_LOG(ERR, "RX queue %u numa %d differs "
						"from TX queue %u numa %d "
						"unexpectedly",
						rx_queues * i, numa_rx,
						tx_queues * i, numa_tx);
				szedata_close(szedata_temp);
				return -EINVAL;
			} else if (port_rx_queues == 0 && port_tx_queues == 0) {
				continue;
			} else {
				unsigned int j;
				unsigned int current = *port_count;
				int port_numa = port_rx_queues != 0 ?
					numa_rx : numa_tx;

				for (j = 0; j < *port_count; j++) {
					if (pi[j].numa_node ==
							port_numa) {
						current = j;
						break;
					}
				}
				if (pi[current].rx_count == 0 &&
						pi[current].tx_count == 0) {
					pi[current].rx_base_id = rx_queues * i;
					pi[current].tx_base_id = tx_queues * i;
					(*port_count)++;
				} else if ((rx_queues * i !=
						pi[current].rx_base_id +
						pi[current].rx_count) ||
						(tx_queues * i !=
						 pi[current].tx_base_id +
						 pi[current].tx_count)) {
					PMD_INIT_LOG(ERR, "Queue ids does not "
							"fulfill constraints");
					szedata_close(szedata_temp);
					return -EINVAL;
				}
				pi[current].rx_count += port_rx_queues;
				pi[current].tx_count += port_tx_queues;
				pi[current].numa_node = port_numa;
			}
		}
	} else {
		pi[0].rx_count = max_rx_queues;
		pi[0].tx_count = max_tx_queues;
		pi[0].numa_node = pci_dev->device.numa_node;
		*port_count = 1;
	}

	szedata_close(szedata_temp);
	return 0;
}

/**
 * @brief Allocates rte_eth_dev device.
 * @param pci_dev Corresponding PCI device.
 * @param numa_node NUMA node on which device is allocated.
 * @param port_no Id of rte_eth_device created on PCI device pci_dev.
 * @return Pointer to allocated device or NULL on error.
 */
static struct rte_eth_dev *
szedata2_eth_dev_allocate(struct rte_pci_device *pci_dev, int numa_node,
		unsigned int port_no)
{
	struct rte_eth_dev *eth_dev;
	char name[RTE_ETH_NAME_MAX_LEN];

	PMD_INIT_FUNC_TRACE();

	snprintf(name, RTE_ETH_NAME_MAX_LEN, "%s"
			SZEDATA2_ETH_DEV_NAME_SUFFIX_FMT,
			pci_dev->device.name, port_no);
	PMD_INIT_LOG(DEBUG, "Allocating eth_dev %s", name);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev)
			return NULL;

		eth_dev->data->dev_private = rte_zmalloc_socket(name,
			sizeof(struct pmd_internals), RTE_CACHE_LINE_SIZE,
			numa_node);
		if (!eth_dev->data->dev_private) {
			rte_eth_dev_release_port(eth_dev);
			return NULL;
		}
	} else {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev)
			return NULL;
	}

	eth_dev->device = &pci_dev->device;
	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->numa_node = numa_node;
	return eth_dev;
}

/**
 * @brief Releases interval of rte_eth_dev devices from array.
 * @param eth_devs Array of pointers to rte_eth_dev devices.
 * @param from Index in array eth_devs to start with.
 * @param to Index in array right after the last element to release.
 *
 * Used for releasing at failed initialization.
 */
static void
szedata2_eth_dev_release_interval(struct rte_eth_dev **eth_devs,
		unsigned int from, unsigned int to)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = from; i < to; i++) {
		rte_szedata2_eth_dev_uninit(eth_devs[i]);
		rte_eth_dev_pci_release(eth_devs[i]);
	}
}

/**
 * @brief Callback .probe for struct rte_pci_driver.
 */
static int szedata2_eth_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	struct port_info port_info[SZEDATA2_MAX_PORTS];
	unsigned int port_count;
	int ret;
	unsigned int i;
	struct pci_dev_list_entry *list_entry;
	struct rte_eth_dev *eth_devs[SZEDATA2_MAX_PORTS] = {NULL,};

	PMD_INIT_FUNC_TRACE();

	ret = get_port_info(pci_dev, &port_count, port_info,
			SZEDATA2_MAX_PORTS);
	if (ret != 0)
		return ret;

	if (port_count == 0) {
		PMD_INIT_LOG(ERR, "No available ports!");
		return -ENODEV;
	}

	list_entry = rte_zmalloc(NULL, sizeof(struct pci_dev_list_entry),
			RTE_CACHE_LINE_SIZE);
	if (list_entry == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc() failed!");
		return -ENOMEM;
	}

	for (i = 0; i < port_count; i++) {
		eth_devs[i] = szedata2_eth_dev_allocate(pci_dev,
				port_info[i].numa_node, i);
		if (eth_devs[i] == NULL) {
			PMD_INIT_LOG(ERR, "Failed to alloc eth_dev for port %u",
					i);
			szedata2_eth_dev_release_interval(eth_devs, 0, i);
			rte_free(list_entry);
			return -ENOMEM;
		}

		ret = rte_szedata2_eth_dev_init(eth_devs[i], &port_info[i]);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to init eth_dev for port %u",
					i);
			rte_eth_dev_pci_release(eth_devs[i]);
			szedata2_eth_dev_release_interval(eth_devs, 0, i);
			rte_free(list_entry);
			return ret;
		}

		rte_eth_dev_probing_finish(eth_devs[i]);
	}

	/*
	 * Add pci_dev to list of PCI devices for this driver
	 * which is used at remove callback to release all created eth_devs.
	 */
	list_entry->pci_dev = pci_dev;
	list_entry->port_count = port_count;
	LIST_INSERT_HEAD(&szedata2_pci_dev_list, list_entry, next);
	return 0;
}

/**
 * @brief Callback .remove for struct rte_pci_driver.
 */
static int szedata2_eth_pci_remove(struct rte_pci_device *pci_dev)
{
	unsigned int i;
	unsigned int port_count;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_dev *eth_dev;
	int ret;
	int retval = 0;
	bool found = false;
	struct pci_dev_list_entry *list_entry = NULL;

	PMD_INIT_FUNC_TRACE();

	LIST_FOREACH(list_entry, &szedata2_pci_dev_list, next) {
		if (list_entry->pci_dev == pci_dev) {
			port_count = list_entry->port_count;
			found = true;
			break;
		}
	}
	LIST_REMOVE(list_entry, next);
	rte_free(list_entry);

	if (!found) {
		PMD_DRV_LOG(ERR, "PCI device " PCI_PRI_FMT " not found",
				pci_dev->addr.domain, pci_dev->addr.bus,
				pci_dev->addr.devid, pci_dev->addr.function);
		return -ENODEV;
	}

	for (i = 0; i < port_count; i++) {
		snprintf(name, RTE_ETH_NAME_MAX_LEN, "%s"
				SZEDATA2_ETH_DEV_NAME_SUFFIX_FMT,
				pci_dev->device.name, i);
		PMD_DRV_LOG(DEBUG, "Removing eth_dev %s", name);
		eth_dev = rte_eth_dev_allocated(name);
		if (!eth_dev) {
			PMD_DRV_LOG(ERR, "eth_dev %s not found", name);
			retval = retval ? retval : -ENODEV;
		}

		ret = rte_szedata2_eth_dev_uninit(eth_dev);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "eth_dev %s uninit failed", name);
			retval = retval ? retval : ret;
		}

		rte_eth_dev_pci_release(eth_dev);
	}

	return retval;
}

static struct rte_pci_driver szedata2_eth_driver = {
	.id_table = rte_szedata2_pci_id_table,
	.probe = szedata2_eth_pci_probe,
	.remove = szedata2_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(RTE_SZEDATA2_DRIVER_NAME, szedata2_eth_driver);
RTE_PMD_REGISTER_PCI_TABLE(RTE_SZEDATA2_DRIVER_NAME, rte_szedata2_pci_id_table);
RTE_PMD_REGISTER_KMOD_DEP(RTE_SZEDATA2_DRIVER_NAME,
	"* combo6core & combov3 & szedata2 & ( szedata2_cv3 | szedata2_cv3_fdt )");
RTE_LOG_REGISTER(szedata2_logtype_init, pmd.net.szedata2.init, NOTICE);
RTE_LOG_REGISTER(szedata2_logtype_driver, pmd.net.szedata2.driver, NOTICE);
