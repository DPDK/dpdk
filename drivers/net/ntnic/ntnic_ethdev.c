/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_vfio.h>
#include <rte_ethdev.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>
#include <rte_kvargs.h>

#include <sys/queue.h>

#include "ntlog.h"
#include "ntdrv_4ga.h"
#include "ntos_drv.h"
#include "ntos_system.h"
#include "nthw_fpga_instances.h"
#include "ntnic_vfio.h"
#include "ntnic_mod_reg.h"
#include "nt_util.h"

#define HW_MAX_PKT_LEN (10000)
#define MAX_MTU (HW_MAX_PKT_LEN - RTE_ETHER_HDR_LEN - RTE_ETHER_CRC_LEN)

#define EXCEPTION_PATH_HID 0

#define MAX_TOTAL_QUEUES       128

#define SG_NB_HW_RX_DESCRIPTORS 1024
#define SG_NB_HW_TX_DESCRIPTORS 1024
#define SG_HW_RX_PKT_BUFFER_SIZE (1024 << 1)
#define SG_HW_TX_PKT_BUFFER_SIZE (1024 << 1)

/* Max RSS queues */
#define MAX_QUEUES 125

#define NUM_VQ_SEGS(_data_size_)                                                                  \
	({                                                                                        \
		size_t _size = (_data_size_);                                                     \
		size_t _segment_count = ((_size + SG_HDR_SIZE) > SG_HW_TX_PKT_BUFFER_SIZE)        \
			? (((_size + SG_HDR_SIZE) + SG_HW_TX_PKT_BUFFER_SIZE - 1) /               \
			   SG_HW_TX_PKT_BUFFER_SIZE)                                              \
			: 1;                                                                      \
		_segment_count;                                                                   \
	})

#define VIRTQ_DESCR_IDX(_tx_pkt_idx_)                                                             \
	(((_tx_pkt_idx_) + first_vq_descr_idx) % SG_NB_HW_TX_DESCRIPTORS)

#define VIRTQ_DESCR_IDX_NEXT(_vq_descr_idx_) (((_vq_descr_idx_) + 1) % SG_NB_HW_TX_DESCRIPTORS)

#define ONE_G_SIZE  0x40000000
#define ONE_G_MASK  (ONE_G_SIZE - 1)

#define MAX_RX_PACKETS   128
#define MAX_TX_PACKETS   128

int kill_pmd;

#define ETH_DEV_NTNIC_HELP_ARG "help"
#define ETH_DEV_NTHW_RXQUEUES_ARG "rxqs"
#define ETH_DEV_NTHW_TXQUEUES_ARG "txqs"

static const char *const valid_arguments[] = {
	ETH_DEV_NTNIC_HELP_ARG,
	ETH_DEV_NTHW_RXQUEUES_ARG,
	ETH_DEV_NTHW_TXQUEUES_ARG,
	NULL,
};


static const struct rte_pci_id nthw_pci_id_map[] = {
	{ RTE_PCI_DEVICE(NT_HW_PCI_VENDOR_ID, NT_HW_PCI_DEVICE_ID_NT200A02) },
	{
		.vendor_id = 0,
	},	/* sentinel */
};

static const struct sg_ops_s *sg_ops;

static rte_spinlock_t hwlock = RTE_SPINLOCK_INITIALIZER;

/*
 * Store and get adapter info
 */

static struct drv_s *_g_p_drv[NUM_ADAPTER_MAX] = { NULL };

static void
store_pdrv(struct drv_s *p_drv)
{
	if (p_drv->adapter_no >= NUM_ADAPTER_MAX) {
		NT_LOG(ERR, NTNIC,
			"Internal error adapter number %u out of range. Max number of adapters: %u",
			p_drv->adapter_no, NUM_ADAPTER_MAX);
		return;
	}

	if (_g_p_drv[p_drv->adapter_no] != 0) {
		NT_LOG(WRN, NTNIC,
			"Overwriting adapter structure for PCI  " PCIIDENT_PRINT_STR
			" with adapter structure for PCI  " PCIIDENT_PRINT_STR,
			PCIIDENT_TO_DOMAIN(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_BUSNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_DEVNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_FUNCNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_DOMAIN(p_drv->ntdrv.pciident),
			PCIIDENT_TO_BUSNR(p_drv->ntdrv.pciident),
			PCIIDENT_TO_DEVNR(p_drv->ntdrv.pciident),
			PCIIDENT_TO_FUNCNR(p_drv->ntdrv.pciident));
	}

	rte_spinlock_lock(&hwlock);
	_g_p_drv[p_drv->adapter_no] = p_drv;
	rte_spinlock_unlock(&hwlock);
}

static struct drv_s *
get_pdrv_from_pci(struct rte_pci_addr addr)
{
	int i;
	struct drv_s *p_drv = NULL;
	rte_spinlock_lock(&hwlock);

	for (i = 0; i < NUM_ADAPTER_MAX; i++) {
		if (_g_p_drv[i]) {
			if (PCIIDENT_TO_DOMAIN(_g_p_drv[i]->ntdrv.pciident) == addr.domain &&
				PCIIDENT_TO_BUSNR(_g_p_drv[i]->ntdrv.pciident) == addr.bus) {
				p_drv = _g_p_drv[i];
				break;
			}
		}
	}

	rte_spinlock_unlock(&hwlock);
	return p_drv;
}

static int
eth_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete __rte_unused)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	if (eth_dev->data->dev_started) {
		const bool port_link_status = port_ops->get_link_status(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_status =
			port_link_status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;

		nt_link_speed_t port_link_speed =
			port_ops->get_link_speed(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_speed =
			nt_link_speed_to_eth_speed_num(port_link_speed);

		nt_link_duplex_t nt_link_duplex =
			port_ops->get_link_duplex(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_duplex = nt_link_duplex_to_eth_duplex(nt_link_duplex);

	} else {
		eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
		eth_dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		eth_dev->data->dev_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	}

	return 0;
}

static int
eth_dev_infos_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *dev_info)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	dev_info->driver_name = internals->name;
	dev_info->max_mac_addrs = NUM_MAC_ADDRS_PER_PORT;
	dev_info->max_rx_pktlen = HW_MAX_PKT_LEN;
	dev_info->max_mtu = MAX_MTU;

	if (internals->p_drv) {
		dev_info->max_rx_queues = internals->nb_rx_queues;
		dev_info->max_tx_queues = internals->nb_tx_queues;

		dev_info->min_rx_bufsize = 64;

		const uint32_t nt_port_speed_capa =
			port_ops->get_link_speed_capabilities(p_adapter_info, n_intf_no);
		dev_info->speed_capa = nt_link_speed_capa_to_eth_speed_capa(nt_port_speed_capa);
	}

	return 0;
}

static __rte_always_inline int copy_virtqueue_to_mbuf(struct rte_mbuf *mbuf,
	struct rte_mempool *mb_pool,
	struct nthw_received_packets *hw_recv,
	int max_segs,
	uint16_t data_len)
{
	int src_pkt = 0;
	/*
	 * 1. virtqueue packets may be segmented
	 * 2. the mbuf size may be too small and may need to be segmented
	 */
	char *data = (char *)hw_recv->addr + SG_HDR_SIZE;
	char *dst = (char *)mbuf->buf_addr + RTE_PKTMBUF_HEADROOM;

	/* set packet length */
	mbuf->pkt_len = data_len - SG_HDR_SIZE;

	int remain = mbuf->pkt_len;
	/* First cpy_size is without header */
	int cpy_size = (data_len > SG_HW_RX_PKT_BUFFER_SIZE)
		? SG_HW_RX_PKT_BUFFER_SIZE - SG_HDR_SIZE
		: remain;

	struct rte_mbuf *m = mbuf;	/* if mbuf segmentation is needed */

	while (++src_pkt <= max_segs) {
		/* keep track of space in dst */
		int cpto_size = rte_pktmbuf_tailroom(m);

		if (cpy_size > cpto_size) {
			int new_cpy_size = cpto_size;

			rte_memcpy((void *)dst, (void *)data, new_cpy_size);
			m->data_len += new_cpy_size;
			remain -= new_cpy_size;
			cpy_size -= new_cpy_size;

			data += new_cpy_size;

			/*
			 * loop if remaining data from this virtqueue seg
			 * cannot fit in one extra mbuf
			 */
			do {
				m->next = rte_pktmbuf_alloc(mb_pool);

				if (unlikely(!m->next))
					return -1;

				m = m->next;

				/* Headroom is not needed in chained mbufs */
				rte_pktmbuf_prepend(m, rte_pktmbuf_headroom(m));
				dst = (char *)m->buf_addr;
				m->data_len = 0;
				m->pkt_len = 0;

				cpto_size = rte_pktmbuf_tailroom(m);

				int actual_cpy_size =
					(cpy_size > cpto_size) ? cpto_size : cpy_size;

				rte_memcpy((void *)dst, (void *)data, actual_cpy_size);
				m->pkt_len += actual_cpy_size;
				m->data_len += actual_cpy_size;

				remain -= actual_cpy_size;
				cpy_size -= actual_cpy_size;

				data += actual_cpy_size;

				mbuf->nb_segs++;

			} while (cpy_size && remain);

		} else {
			/* all data from this virtqueue segment can fit in current mbuf */
			rte_memcpy((void *)dst, (void *)data, cpy_size);
			m->data_len += cpy_size;

			if (mbuf->nb_segs > 1)
				m->pkt_len += cpy_size;

			remain -= cpy_size;
		}

		/* packet complete - all data from current virtqueue packet has been copied */
		if (remain == 0)
			break;

		/* increment dst to data end */
		dst = rte_pktmbuf_mtod_offset(m, char *, m->data_len);
		/* prepare for next virtqueue segment */
		data = (char *)hw_recv[src_pkt].addr;	/* following packets are full data */

		cpy_size = (remain > SG_HW_RX_PKT_BUFFER_SIZE) ? SG_HW_RX_PKT_BUFFER_SIZE : remain;
	};

	if (src_pkt > max_segs) {
		NT_LOG(ERR, NTNIC,
			"Did not receive correct number of segment for a whole packet");
		return -1;
	}

	return src_pkt;
}

static uint16_t eth_dev_rx_scg(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	struct rte_mbuf *mbuf;
	struct ntnic_rx_queue *rx_q = queue;
	uint16_t num_rx = 0;

	struct nthw_received_packets hw_recv[MAX_RX_PACKETS];

	if (kill_pmd)
		return 0;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (nb_pkts > MAX_RX_PACKETS)
		nb_pkts = MAX_RX_PACKETS;

	uint16_t whole_pkts = 0;
	uint16_t hw_recv_pkt_segs = 0;

	if (sg_ops != NULL) {
		hw_recv_pkt_segs =
			sg_ops->nthw_get_rx_packets(rx_q->vq, nb_pkts, hw_recv, &whole_pkts);

		if (!hw_recv_pkt_segs)
			return 0;
	}

	nb_pkts = whole_pkts;

	int src_pkt = 0;/* from 0 to hw_recv_pkt_segs */

	for (i = 0; i < nb_pkts; i++) {
		bufs[i] = rte_pktmbuf_alloc(rx_q->mb_pool);

		if (!bufs[i]) {
			NT_LOG(ERR, NTNIC, "ERROR - no more buffers mbuf in mempool");
			goto err_exit;
		}

		mbuf = bufs[i];

		struct _pkt_hdr_rx *phdr = (struct _pkt_hdr_rx *)hw_recv[src_pkt].addr;

		if (phdr->cap_len < SG_HDR_SIZE) {
			NT_LOG(ERR, NTNIC,
				"Pkt len of zero received. No header!! - dropping packets");
			rte_pktmbuf_free(mbuf);
			goto err_exit;
		}

		{
			if (phdr->cap_len <= SG_HW_RX_PKT_BUFFER_SIZE &&
				(phdr->cap_len - SG_HDR_SIZE) <= rte_pktmbuf_tailroom(mbuf)) {
				mbuf->data_len = phdr->cap_len - SG_HDR_SIZE;
				rte_memcpy(rte_pktmbuf_mtod(mbuf, char *),
					(char *)hw_recv[src_pkt].addr + SG_HDR_SIZE,
					mbuf->data_len);

				mbuf->pkt_len = mbuf->data_len;
				src_pkt++;

			} else {
				int cpy_segs = copy_virtqueue_to_mbuf(mbuf, rx_q->mb_pool,
						&hw_recv[src_pkt],
						hw_recv_pkt_segs - src_pkt,
						phdr->cap_len);

				if (cpy_segs < 0) {
					/* Error */
					rte_pktmbuf_free(mbuf);
					goto err_exit;
				}

				src_pkt += cpy_segs;
			}

			num_rx++;

			mbuf->ol_flags &= ~(RTE_MBUF_F_RX_FDIR_ID | RTE_MBUF_F_RX_FDIR);
			mbuf->port = (uint16_t)-1;
		}
	}

err_exit:

	if (sg_ops != NULL)
		sg_ops->nthw_release_rx_packets(rx_q->vq, hw_recv_pkt_segs);

	return num_rx;
}

static int copy_mbuf_to_virtqueue(struct nthw_cvirtq_desc *cvq_desc,
	uint16_t vq_descr_idx,
	struct nthw_memory_descriptor *vq_bufs,
	int max_segs,
	struct rte_mbuf *mbuf)
{
	/*
	 * 1. mbuf packet may be segmented
	 * 2. the virtqueue buffer size may be too small and may need to be segmented
	 */

	char *data = rte_pktmbuf_mtod(mbuf, char *);
	char *dst = (char *)vq_bufs[vq_descr_idx].virt_addr + SG_HDR_SIZE;

	int remain = mbuf->pkt_len;
	int cpy_size = mbuf->data_len;

	struct rte_mbuf *m = mbuf;
	int cpto_size = SG_HW_TX_PKT_BUFFER_SIZE - SG_HDR_SIZE;

	cvq_desc->b[vq_descr_idx].len = SG_HDR_SIZE;

	int cur_seg_num = 0;	/* start from 0 */

	while (m) {
		/* Can all data in current src segment be in current dest segment */
		if (cpy_size > cpto_size) {
			int new_cpy_size = cpto_size;

			rte_memcpy((void *)dst, (void *)data, new_cpy_size);

			cvq_desc->b[vq_descr_idx].len += new_cpy_size;

			remain -= new_cpy_size;
			cpy_size -= new_cpy_size;

			data += new_cpy_size;

			/*
			 * Loop if remaining data from this virtqueue seg cannot fit in one extra
			 * mbuf
			 */
			do {
				vq_add_flags(cvq_desc, vq_descr_idx, VIRTQ_DESC_F_NEXT);

				int next_vq_descr_idx = VIRTQ_DESCR_IDX_NEXT(vq_descr_idx);

				vq_set_next(cvq_desc, vq_descr_idx, next_vq_descr_idx);

				vq_descr_idx = next_vq_descr_idx;

				vq_set_flags(cvq_desc, vq_descr_idx, 0);
				vq_set_next(cvq_desc, vq_descr_idx, 0);

				if (++cur_seg_num > max_segs)
					break;

				dst = (char *)vq_bufs[vq_descr_idx].virt_addr;
				cpto_size = SG_HW_TX_PKT_BUFFER_SIZE;

				int actual_cpy_size =
					(cpy_size > cpto_size) ? cpto_size : cpy_size;
				rte_memcpy((void *)dst, (void *)data, actual_cpy_size);

				cvq_desc->b[vq_descr_idx].len = actual_cpy_size;

				remain -= actual_cpy_size;
				cpy_size -= actual_cpy_size;
				cpto_size -= actual_cpy_size;

				data += actual_cpy_size;

			} while (cpy_size && remain);

		} else {
			/* All data from this segment can fit in current virtqueue buffer */
			rte_memcpy((void *)dst, (void *)data, cpy_size);

			cvq_desc->b[vq_descr_idx].len += cpy_size;

			remain -= cpy_size;
			cpto_size -= cpy_size;
		}

		/* Packet complete - all segments from current mbuf has been copied */
		if (remain == 0)
			break;

		/* increment dst to data end */
		dst = (char *)vq_bufs[vq_descr_idx].virt_addr + cvq_desc->b[vq_descr_idx].len;

		m = m->next;

		if (!m) {
			NT_LOG(ERR, NTNIC, "ERROR: invalid packet size");
			break;
		}

		/* Prepare for next mbuf segment */
		data = rte_pktmbuf_mtod(m, char *);
		cpy_size = m->data_len;
	};

	cur_seg_num++;

	if (cur_seg_num > max_segs) {
		NT_LOG(ERR, NTNIC,
			"Did not receive correct number of segment for a whole packet");
		return -1;
	}

	return cur_seg_num;
}

static uint16_t eth_dev_tx_scg(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	uint16_t pkt;
	uint16_t first_vq_descr_idx = 0;

	struct nthw_cvirtq_desc cvq_desc;

	struct nthw_memory_descriptor *vq_bufs;

	struct ntnic_tx_queue *tx_q = queue;

	int nb_segs = 0, i;
	int pkts_sent = 0;
	uint16_t nb_segs_arr[MAX_TX_PACKETS];

	if (kill_pmd)
		return 0;

	if (nb_pkts > MAX_TX_PACKETS)
		nb_pkts = MAX_TX_PACKETS;

	/*
	 * count all segments needed to contain all packets in vq buffers
	 */
	for (i = 0; i < nb_pkts; i++) {
		/* build the num segments array for segmentation control and release function */
		int vq_segs = NUM_VQ_SEGS(bufs[i]->pkt_len);
		nb_segs_arr[i] = vq_segs;
		nb_segs += vq_segs;
	}

	if (!nb_segs)
		goto exit_out;

	if (sg_ops == NULL)
		goto exit_out;

	int got_nb_segs = sg_ops->nthw_get_tx_packets(tx_q->vq, nb_segs, &first_vq_descr_idx,
			&cvq_desc /*&vq_descr,*/, &vq_bufs);

	if (!got_nb_segs)
		goto exit_out;

	/*
	 * we may get less vq buffers than we have asked for
	 * calculate last whole packet that can fit into what
	 * we have got
	 */
	while (got_nb_segs < nb_segs) {
		if (!--nb_pkts)
			goto exit_out;

		nb_segs -= NUM_VQ_SEGS(bufs[nb_pkts]->pkt_len);

		if (nb_segs <= 0)
			goto exit_out;
	}

	/*
	 * nb_pkts & nb_segs, got it all, ready to copy
	 */
	int seg_idx = 0;
	int last_seg_idx = seg_idx;

	for (pkt = 0; pkt < nb_pkts; ++pkt) {
		uint16_t vq_descr_idx = VIRTQ_DESCR_IDX(seg_idx);

		vq_set_flags(&cvq_desc, vq_descr_idx, 0);
		vq_set_next(&cvq_desc, vq_descr_idx, 0);

		if (bufs[pkt]->nb_segs == 1 && nb_segs_arr[pkt] == 1) {
			rte_memcpy((void *)((char *)vq_bufs[vq_descr_idx].virt_addr + SG_HDR_SIZE),
				rte_pktmbuf_mtod(bufs[pkt], void *), bufs[pkt]->pkt_len);

			cvq_desc.b[vq_descr_idx].len = bufs[pkt]->pkt_len + SG_HDR_SIZE;

			seg_idx++;

		} else {
			int cpy_segs = copy_mbuf_to_virtqueue(&cvq_desc, vq_descr_idx, vq_bufs,
					nb_segs - last_seg_idx, bufs[pkt]);

			if (cpy_segs < 0)
				break;

			seg_idx += cpy_segs;
		}

		last_seg_idx = seg_idx;
		rte_pktmbuf_free(bufs[pkt]);
		pkts_sent++;
	}

exit_out:

	if (sg_ops != NULL) {
		if (pkts_sent)
			sg_ops->nthw_release_tx_packets(tx_q->vq, pkts_sent, nb_segs_arr);
	}

	return pkts_sent;
}

static int allocate_hw_virtio_queues(struct rte_eth_dev *eth_dev, int vf_num, struct hwq_s *hwq,
	int num_descr, int buf_size)
{
	int i, res;
	uint32_t size;
	uint64_t iova_addr;

	NT_LOG(DBG, NTNIC, "***** Configure IOMMU for HW queues on VF %i *****", vf_num);

	/* Just allocate 1MB to hold all combined descr rings */
	uint64_t tot_alloc_size = 0x100000 + buf_size * num_descr;

	void *virt =
		rte_malloc_socket("VirtQDescr", tot_alloc_size, nt_util_align_size(tot_alloc_size),
			eth_dev->data->numa_node);

	if (!virt)
		return -1;

	uint64_t gp_offset = (uint64_t)virt & ONE_G_MASK;
	rte_iova_t hpa = rte_malloc_virt2iova(virt);

	NT_LOG(DBG, NTNIC, "Allocated virtio descr rings : virt "
		"%p [0x%" PRIX64 "],hpa %" PRIX64 " [0x%" PRIX64 "]",
		virt, gp_offset, hpa, hpa & ONE_G_MASK);

	/*
	 * Same offset on both HPA and IOVA
	 * Make sure 1G boundary is never crossed
	 */
	if (((hpa & ONE_G_MASK) != gp_offset) ||
		(((uint64_t)virt + tot_alloc_size) & ~ONE_G_MASK) !=
		((uint64_t)virt & ~ONE_G_MASK)) {
		NT_LOG(ERR, NTNIC, "*********************************************************");
		NT_LOG(ERR, NTNIC, "ERROR, no optimal IOMMU mapping available hpa: %016" PRIX64
			"(%016" PRIX64 "), gp_offset: %016" PRIX64 " size: %" PRIu64,
			hpa, hpa & ONE_G_MASK, gp_offset, tot_alloc_size);
		NT_LOG(ERR, NTNIC, "*********************************************************");

		rte_free(virt);

		/* Just allocate 1MB to hold all combined descr rings */
		size = 0x100000;
		void *virt = rte_malloc_socket("VirtQDescr", size, 4096, eth_dev->data->numa_node);

		if (!virt)
			return -1;

		res = nt_vfio_dma_map(vf_num, virt, &iova_addr, size);

		NT_LOG(DBG, NTNIC, "VFIO MMAP res %i, vf_num %i", res, vf_num);

		if (res != 0)
			return -1;

		hwq->vf_num = vf_num;
		hwq->virt_queues_ctrl.virt_addr = virt;
		hwq->virt_queues_ctrl.phys_addr = (void *)iova_addr;
		hwq->virt_queues_ctrl.len = size;

		NT_LOG(DBG, NTNIC,
			"Allocated for virtio descr rings combined 1MB : %p, IOVA %016" PRIX64 "",
			virt, iova_addr);

		size = num_descr * sizeof(struct nthw_memory_descriptor);
		hwq->pkt_buffers =
			rte_zmalloc_socket("rx_pkt_buffers", size, 64, eth_dev->data->numa_node);

		if (!hwq->pkt_buffers) {
			NT_LOG(ERR, NTNIC,
				"Failed to allocated buffer array for hw-queue %p, total size %i, elements %i",
				hwq->pkt_buffers, size, num_descr);
			rte_free(virt);
			return -1;
		}

		size = buf_size * num_descr;
		void *virt_addr =
			rte_malloc_socket("pkt_buffer_pkts", size, 4096, eth_dev->data->numa_node);

		if (!virt_addr) {
			NT_LOG(ERR, NTNIC,
				"Failed allocate packet buffers for hw-queue %p, buf size %i, elements %i",
				hwq->pkt_buffers, buf_size, num_descr);
			rte_free(hwq->pkt_buffers);
			rte_free(virt);
			return -1;
		}

		res = nt_vfio_dma_map(vf_num, virt_addr, &iova_addr, size);

		NT_LOG(DBG, NTNIC,
			"VFIO MMAP res %i, virt %p, iova %016" PRIX64 ", vf_num %i, num pkt bufs %i, tot size %i",
			res, virt_addr, iova_addr, vf_num, num_descr, size);

		if (res != 0)
			return -1;

		for (i = 0; i < num_descr; i++) {
			hwq->pkt_buffers[i].virt_addr =
				(void *)((char *)virt_addr + ((uint64_t)(i) * buf_size));
			hwq->pkt_buffers[i].phys_addr =
				(void *)(iova_addr + ((uint64_t)(i) * buf_size));
			hwq->pkt_buffers[i].len = buf_size;
		}

		return 0;
	}	/* End of: no optimal IOMMU mapping available */

	res = nt_vfio_dma_map(vf_num, virt, &iova_addr, ONE_G_SIZE);

	if (res != 0) {
		NT_LOG(ERR, NTNIC, "VFIO MMAP FAILED! res %i, vf_num %i", res, vf_num);
		return -1;
	}

	hwq->vf_num = vf_num;
	hwq->virt_queues_ctrl.virt_addr = virt;
	hwq->virt_queues_ctrl.phys_addr = (void *)(iova_addr);
	hwq->virt_queues_ctrl.len = 0x100000;
	iova_addr += 0x100000;

	NT_LOG(DBG, NTNIC,
		"VFIO MMAP: virt_addr=%p phys_addr=%p size=%" PRIX32 " hpa=%" PRIX64 "",
		hwq->virt_queues_ctrl.virt_addr, hwq->virt_queues_ctrl.phys_addr,
		hwq->virt_queues_ctrl.len, rte_malloc_virt2iova(hwq->virt_queues_ctrl.virt_addr));

	size = num_descr * sizeof(struct nthw_memory_descriptor);
	hwq->pkt_buffers =
		rte_zmalloc_socket("rx_pkt_buffers", size, 64, eth_dev->data->numa_node);

	if (!hwq->pkt_buffers) {
		NT_LOG(ERR, NTNIC,
			"Failed to allocated buffer array for hw-queue %p, total size %i, elements %i",
			hwq->pkt_buffers, size, num_descr);
		rte_free(virt);
		return -1;
	}

	void *virt_addr = (void *)((uint64_t)virt + 0x100000);

	for (i = 0; i < num_descr; i++) {
		hwq->pkt_buffers[i].virt_addr =
			(void *)((char *)virt_addr + ((uint64_t)(i) * buf_size));
		hwq->pkt_buffers[i].phys_addr = (void *)(iova_addr + ((uint64_t)(i) * buf_size));
		hwq->pkt_buffers[i].len = buf_size;
	}

	return 0;
}

static void release_hw_virtio_queues(struct hwq_s *hwq)
{
	if (!hwq || hwq->vf_num == 0)
		return;

	hwq->vf_num = 0;
}

static int deallocate_hw_virtio_queues(struct hwq_s *hwq)
{
	int vf_num = hwq->vf_num;

	void *virt = hwq->virt_queues_ctrl.virt_addr;

	int res = nt_vfio_dma_unmap(vf_num, hwq->virt_queues_ctrl.virt_addr,
			(uint64_t)hwq->virt_queues_ctrl.phys_addr, ONE_G_SIZE);

	if (res != 0) {
		NT_LOG(ERR, NTNIC, "VFIO UNMMAP FAILED! res %i, vf_num %i", res, vf_num);
		return -1;
	}

	release_hw_virtio_queues(hwq);
	rte_free(hwq->pkt_buffers);
	rte_free(virt);
	return 0;
}

static void eth_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct ntnic_tx_queue *tx_q = &internals->txq_scg[queue_id];
	deallocate_hw_virtio_queues(&tx_q->hwq);
}

static void eth_rx_queue_release(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct ntnic_rx_queue *rx_q = &internals->rxq_scg[queue_id];
	deallocate_hw_virtio_queues(&rx_q->hwq);
}

static int num_queues_alloced;

/* Returns num queue starting at returned queue num or -1 on fail */
static int allocate_queue(int num)
{
	int next_free = num_queues_alloced;
	NT_LOG_DBGX(DBG, NTNIC, "num_queues_alloced=%u, New queues=%u, Max queues=%u",
		num_queues_alloced, num, MAX_TOTAL_QUEUES);

	if (num_queues_alloced + num > MAX_TOTAL_QUEUES)
		return -1;

	num_queues_alloced += num;
	return next_free;
}

static int eth_rx_scg_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t rx_queue_id,
	uint16_t nb_rx_desc __rte_unused,
	unsigned int socket_id __rte_unused,
	const struct rte_eth_rxconf *rx_conf __rte_unused,
	struct rte_mempool *mb_pool)
{
	NT_LOG_DBGX(DBG, NTNIC, "Rx queue setup");
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct ntnic_rx_queue *rx_q = &internals->rxq_scg[rx_queue_id];
	struct drv_s *p_drv = internals->p_drv;
	struct ntdrv_4ga_s *p_nt_drv = &p_drv->ntdrv;

	if (sg_ops == NULL) {
		NT_LOG_DBGX(DBG, NTNIC, "SG module is not initialized");
		return 0;
	}

	if (internals->type == PORT_TYPE_OVERRIDE) {
		rx_q->mb_pool = mb_pool;
		eth_dev->data->rx_queues[rx_queue_id] = rx_q;
		mbp_priv = rte_mempool_get_priv(rx_q->mb_pool);
		rx_q->buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);
		rx_q->enabled = 1;
		return 0;
	}

	NT_LOG(DBG, NTNIC, "(%i) NTNIC RX OVS-SW queue setup: queue id %i, hw queue index %i",
		internals->port, rx_queue_id, rx_q->queue.hw_id);

	rx_q->mb_pool = mb_pool;

	eth_dev->data->rx_queues[rx_queue_id] = rx_q;

	mbp_priv = rte_mempool_get_priv(rx_q->mb_pool);
	rx_q->buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);
	rx_q->enabled = 1;

	if (allocate_hw_virtio_queues(eth_dev, EXCEPTION_PATH_HID, &rx_q->hwq,
			SG_NB_HW_RX_DESCRIPTORS, SG_HW_RX_PKT_BUFFER_SIZE) < 0)
		return -1;

	rx_q->nb_hw_rx_descr = SG_NB_HW_RX_DESCRIPTORS;

	rx_q->profile = p_drv->ntdrv.adapter_info.fpga_info.profile;

	rx_q->vq =
		sg_ops->nthw_setup_mngd_rx_virt_queue(p_nt_drv->adapter_info.fpga_info.mp_nthw_dbs,
			rx_q->queue.hw_id,	/* index */
			rx_q->nb_hw_rx_descr,
			EXCEPTION_PATH_HID,	/* host_id */
			1,	/* header NT DVIO header for exception path */
			&rx_q->hwq.virt_queues_ctrl,
			rx_q->hwq.pkt_buffers,
			SPLIT_RING,
			-1);

	NT_LOG(DBG, NTNIC, "(%i) NTNIC RX OVS-SW queues successfully setup", internals->port);

	return 0;
}

static int eth_tx_scg_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t tx_queue_id,
	uint16_t nb_tx_desc __rte_unused,
	unsigned int socket_id __rte_unused,
	const struct rte_eth_txconf *tx_conf __rte_unused)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG_DBGX(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	NT_LOG_DBGX(DBG, NTNIC, "Tx queue setup");
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct drv_s *p_drv = internals->p_drv;
	struct ntdrv_4ga_s *p_nt_drv = &p_drv->ntdrv;
	struct ntnic_tx_queue *tx_q = &internals->txq_scg[tx_queue_id];

	if (internals->type == PORT_TYPE_OVERRIDE) {
		eth_dev->data->tx_queues[tx_queue_id] = tx_q;
		return 0;
	}

	if (sg_ops == NULL) {
		NT_LOG_DBGX(DBG, NTNIC, "SG module is not initialized");
		return 0;
	}

	NT_LOG(DBG, NTNIC, "(%i) NTNIC TX OVS-SW queue setup: queue id %i, hw queue index %i",
		tx_q->port, tx_queue_id, tx_q->queue.hw_id);

	if (tx_queue_id > internals->nb_tx_queues) {
		NT_LOG(ERR, NTNIC, "Error invalid tx queue id");
		return -1;
	}

	eth_dev->data->tx_queues[tx_queue_id] = tx_q;

	/* Calculate target ID for HW  - to be used in NTDVIO0 header bypass_port */
	if (tx_q->rss_target_id >= 0) {
		/* bypass to a multiqueue port - qsl-hsh index */
		tx_q->target_id = tx_q->rss_target_id + 0x90;

	} else if (internals->vpq[tx_queue_id].hw_id > -1) {
		/* virtual port - queue index */
		tx_q->target_id = internals->vpq[tx_queue_id].hw_id;

	} else {
		/* Phy port - phy port identifier */
		/* output/bypass to MAC */
		tx_q->target_id = (int)(tx_q->port + 0x80);
	}

	if (allocate_hw_virtio_queues(eth_dev, EXCEPTION_PATH_HID, &tx_q->hwq,
			SG_NB_HW_TX_DESCRIPTORS, SG_HW_TX_PKT_BUFFER_SIZE) < 0) {
		return -1;
	}

	tx_q->nb_hw_tx_descr = SG_NB_HW_TX_DESCRIPTORS;

	tx_q->profile = p_drv->ntdrv.adapter_info.fpga_info.profile;

	uint32_t port, header;
	port = tx_q->port;	/* transmit port */
	header = 0;	/* header type VirtIO-Net */

	tx_q->vq =
		sg_ops->nthw_setup_mngd_tx_virt_queue(p_nt_drv->adapter_info.fpga_info.mp_nthw_dbs,
			tx_q->queue.hw_id,	/* index */
			tx_q->nb_hw_tx_descr,	/* queue size */
			EXCEPTION_PATH_HID,	/* host_id always VF4 */
			port,
			/*
			 * in_port - in vswitch mode has
			 * to move tx port from OVS excep.
			 * away from VM tx port,
			 * because of QoS is matched by port id!
			 */
			tx_q->port + 128,
			header,
			&tx_q->hwq.virt_queues_ctrl,
			tx_q->hwq.pkt_buffers,
			SPLIT_RING,
			-1,
			IN_ORDER);

	tx_q->enabled = 1;

	NT_LOG(DBG, NTNIC, "(%i) NTNIC TX OVS-SW queues successfully setup", internals->port);

	if (internals->type == PORT_TYPE_PHYSICAL) {
		struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;
		NT_LOG(DBG, NTNIC, "Port %i is ready for data. Enable port",
			internals->n_intf_no);
		port_ops->set_adm_state(p_adapter_info, internals->n_intf_no, true);
	}

	return 0;
}

static int eth_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int eth_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int eth_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->tx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int eth_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->tx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int
eth_mac_addr_add(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mac_addr,
	uint32_t index,
	uint32_t vmdq __rte_unused)
{
	struct rte_ether_addr *const eth_addrs = eth_dev->data->mac_addrs;

	assert(index < NUM_MAC_ADDRS_PER_PORT);

	if (index >= NUM_MAC_ADDRS_PER_PORT) {
		const struct pmd_internals *const internals =
			(struct pmd_internals *)eth_dev->data->dev_private;
		NT_LOG_DBGX(DBG, NTNIC, "Port %i: illegal index %u (>= %u)",
			internals->n_intf_no, index, NUM_MAC_ADDRS_PER_PORT);
		return -1;
	}

	eth_addrs[index] = *mac_addr;

	return 0;
}

static int
eth_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct rte_ether_addr *const eth_addrs = dev->data->mac_addrs;

	eth_addrs[0U] = *mac_addr;

	return 0;
}

static int
eth_set_mc_addr_list(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mc_addr_set,
	uint32_t nb_mc_addr)
{
	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct rte_ether_addr *const mc_addrs = internals->mc_addrs;
	size_t i;

	if (nb_mc_addr >= NUM_MULTICAST_ADDRS_PER_PORT) {
		NT_LOG_DBGX(DBG, NTNIC,
			"Port %i: too many multicast addresses %u (>= %u)",
			internals->n_intf_no, nb_mc_addr, NUM_MULTICAST_ADDRS_PER_PORT);
		return -1;
	}

	for (i = 0U; i < NUM_MULTICAST_ADDRS_PER_PORT; i++)
		if (i < nb_mc_addr)
			mc_addrs[i] = mc_addr_set[i];

		else
			(void)memset(&mc_addrs[i], 0, sizeof(mc_addrs[i]));

	return 0;
}

static int
eth_dev_configure(struct rte_eth_dev *eth_dev)
{
	NT_LOG_DBGX(DBG, NTNIC, "Called for eth_dev %p", eth_dev);

	/* The device is ALWAYS running promiscuous mode. */
	eth_dev->data->promiscuous ^= ~eth_dev->data->promiscuous;
	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	NT_LOG_DBGX(DBG, NTNIC, "Port %u", internals->n_intf_no);

	/* Start queues */
	uint q;

	for (q = 0; q < internals->nb_rx_queues; q++)
		eth_rx_queue_start(eth_dev, q);

	for (q = 0; q < internals->nb_tx_queues; q++)
		eth_tx_queue_start(eth_dev, q);

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE) {
		eth_dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	} else {
		/* Enable the port */
		port_ops->set_adm_state(p_adapter_info, internals->n_intf_no, true);

		/*
		 * wait for link on port
		 * If application starts sending too soon before FPGA port is ready, garbage is
		 * produced
		 */
		int loop = 0;

		while (port_ops->get_link_status(p_adapter_info, n_intf_no) == RTE_ETH_LINK_DOWN) {
			/* break out after 5 sec */
			if (++loop >= 50) {
				NT_LOG_DBGX(DBG, NTNIC,
					"TIMEOUT No link on port %i (5sec timeout)",
					internals->n_intf_no);
				break;
			}

			nt_os_wait_usec(100 * 1000);
		}

		if (internals->lpbk_mode) {
			if (internals->lpbk_mode & 1 << 0) {
				port_ops->set_loopback_mode(p_adapter_info, n_intf_no,
					NT_LINK_LOOPBACK_HOST);
			}

			if (internals->lpbk_mode & 1 << 1) {
				port_ops->set_loopback_mode(p_adapter_info, n_intf_no,
					NT_LINK_LOOPBACK_LINE);
			}
		}
	}

	return 0;
}

static int
eth_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	NT_LOG_DBGX(DBG, NTNIC, "Port %u", internals->n_intf_no);

	if (internals->type != PORT_TYPE_VIRTUAL) {
		uint q;

		for (q = 0; q < internals->nb_rx_queues; q++)
			eth_rx_queue_stop(eth_dev, q);

		for (q = 0; q < internals->nb_tx_queues; q++)
			eth_tx_queue_stop(eth_dev, q);
	}

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;

	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;
	const int port = internals->n_intf_no;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(port == internals->n_intf_no);

	port_ops->set_adm_state(p_adapter_info, port, true);

	return 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;

	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;
	const int port = internals->n_intf_no;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(port == internals->n_intf_no);

	port_ops->set_link_status(p_adapter_info, port, false);

	return 0;
}

static void
drv_deinit(struct drv_s *p_drv)
{
	const struct adapter_ops *adapter_ops = get_adapter_ops();

	if (adapter_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Adapter module uninitialized");
		return;
	}

	if (p_drv == NULL)
		return;

	ntdrv_4ga_t *p_nt_drv = &p_drv->ntdrv;

	/* stop adapter */
	adapter_ops->deinit(&p_nt_drv->adapter_info);

	/* clean memory */
	rte_free(p_drv);
	p_drv = NULL;
}

static int
eth_dev_close(struct rte_eth_dev *eth_dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct drv_s *p_drv = internals->p_drv;

	if (internals->type != PORT_TYPE_VIRTUAL) {
		struct ntnic_rx_queue *rx_q = internals->rxq_scg;
		struct ntnic_tx_queue *tx_q = internals->txq_scg;

		uint q;

		if (sg_ops != NULL) {
			for (q = 0; q < internals->nb_rx_queues; q++)
				sg_ops->nthw_release_mngd_rx_virt_queue(rx_q[q].vq);

			for (q = 0; q < internals->nb_tx_queues; q++)
				sg_ops->nthw_release_mngd_tx_virt_queue(tx_q[q].vq);
		}
	}

	internals->p_drv = NULL;

	if (p_drv) {
		/* decrease initialized ethernet devices */
		p_drv->n_eth_dev_init_count--;

		/*
		 * rte_pci_dev has no private member for p_drv
		 * wait until all rte_eth_dev's are closed - then close adapters via p_drv
		 */
		if (!p_drv->n_eth_dev_init_count)
			drv_deinit(p_drv);
	}

	return 0;
}

static int
eth_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version, size_t fw_size)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	fpga_info_t *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;
	const int length = snprintf(fw_version, fw_size, "%03d-%04d-%02d-%02d",
			fpga_info->n_fpga_type_id, fpga_info->n_fpga_prod_id,
			fpga_info->n_fpga_ver_id, fpga_info->n_fpga_rev_id);

	if ((size_t)length < fw_size) {
		/* We have space for the version string */
		return 0;

	} else {
		/* We do not have space for the version string -return the needed space */
		return length + 1;
	}
}

static int
promiscuous_enable(struct rte_eth_dev __rte_unused(*dev))
{
	NT_LOG(DBG, NTHW, "The device always run promiscuous mode");
	return 0;
}

static const struct eth_dev_ops nthw_eth_dev_ops = {
	.dev_configure = eth_dev_configure,
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_set_link_up = eth_dev_set_link_up,
	.dev_set_link_down = eth_dev_set_link_down,
	.dev_close = eth_dev_close,
	.link_update = eth_link_update,
	.dev_infos_get = eth_dev_infos_get,
	.fw_version_get = eth_fw_version_get,
	.rx_queue_setup = eth_rx_scg_queue_setup,
	.rx_queue_start = eth_rx_queue_start,
	.rx_queue_stop = eth_rx_queue_stop,
	.rx_queue_release = eth_rx_queue_release,
	.tx_queue_setup = eth_tx_scg_queue_setup,
	.tx_queue_start = eth_tx_queue_start,
	.tx_queue_stop = eth_tx_queue_stop,
	.tx_queue_release = eth_tx_queue_release,
	.mac_addr_add = eth_mac_addr_add,
	.mac_addr_set = eth_mac_addr_set,
	.set_mc_addr_list = eth_set_mc_addr_list,
	.promiscuous_enable = promiscuous_enable,
};

static int
nthw_pci_dev_init(struct rte_pci_device *pci_dev)
{
	nt_vfio_init();
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	const struct adapter_ops *adapter_ops = get_adapter_ops();

	if (adapter_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Adapter module uninitialized");
		return -1;
	}

	int res;
	struct drv_s *p_drv;
	ntdrv_4ga_t *p_nt_drv;
	hw_info_t *p_hw_info;
	fpga_info_t *fpga_info;
	uint32_t n_port_mask = -1;	/* All ports enabled by default */
	uint32_t nb_rx_queues = 1;
	uint32_t nb_tx_queues = 1;
	struct flow_queue_id_s queue_ids[MAX_QUEUES];
	int n_phy_ports;
	struct port_link_speed pls_mbps[NUM_ADAPTER_PORTS_MAX] = { 0 };
	int num_port_speeds = 0;
	NT_LOG_DBGX(DBG, NTNIC, "Dev %s PF #%i Init : %02x:%02x:%i", pci_dev->name,
		pci_dev->addr.function, pci_dev->addr.bus, pci_dev->addr.devid,
		pci_dev->addr.function);

	/*
	 * Process options/arguments
	 */
	if (pci_dev->device.devargs && pci_dev->device.devargs->args) {
		int kvargs_count;
		struct rte_kvargs *kvlist =
			rte_kvargs_parse(pci_dev->device.devargs->args, valid_arguments);

		if (kvlist == NULL)
			return -1;

		/*
		 * Argument: help
		 * NOTE: this argument/option check should be the first as it will stop
		 * execution after producing its output
		 */
		{
			if (rte_kvargs_get(kvlist, ETH_DEV_NTNIC_HELP_ARG)) {
				size_t i;

				for (i = 0; i < RTE_DIM(valid_arguments); i++)
					if (valid_arguments[i] == NULL)
						break;

				exit(0);
			}
		}

		/*
		 * rxq option/argument
		 * The number of rxq (hostbuffers) allocated in memory.
		 * Default is 32 RX Hostbuffers
		 */
		kvargs_count = rte_kvargs_count(kvlist, ETH_DEV_NTHW_RXQUEUES_ARG);

		if (kvargs_count != 0) {
			assert(kvargs_count == 1);
			res = rte_kvargs_process(kvlist, ETH_DEV_NTHW_RXQUEUES_ARG, &string_to_u32,
					&nb_rx_queues);

			if (res < 0) {
				NT_LOG_DBGX(ERR, NTNIC,
					"problem with command line arguments: res=%d",
					res);
				return -1;
			}

			NT_LOG_DBGX(DBG, NTNIC, "devargs: %s=%u",
				ETH_DEV_NTHW_RXQUEUES_ARG, nb_rx_queues);
		}

		/*
		 * txq option/argument
		 * The number of txq (hostbuffers) allocated in memory.
		 * Default is 32 TX Hostbuffers
		 */
		kvargs_count = rte_kvargs_count(kvlist, ETH_DEV_NTHW_TXQUEUES_ARG);

		if (kvargs_count != 0) {
			assert(kvargs_count == 1);
			res = rte_kvargs_process(kvlist, ETH_DEV_NTHW_TXQUEUES_ARG, &string_to_u32,
					&nb_tx_queues);

			if (res < 0) {
				NT_LOG_DBGX(ERR, NTNIC,
					"problem with command line arguments: res=%d",
					res);
				return -1;
			}

			NT_LOG_DBGX(DBG, NTNIC, "devargs: %s=%u",
				ETH_DEV_NTHW_TXQUEUES_ARG, nb_tx_queues);
		}
	}


	/* alloc */
	p_drv = rte_zmalloc_socket(pci_dev->name, sizeof(struct drv_s), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (!p_drv) {
		NT_LOG_DBGX(ERR, NTNIC, "%s: error %d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), -1);
		return -1;
	}

	/* Setup VFIO context */
	int vfio = nt_vfio_setup(pci_dev);

	if (vfio < 0) {
		NT_LOG_DBGX(ERR, NTNIC, "%s: vfio_setup error %d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), -1);
		rte_free(p_drv);
		return -1;
	}

	/* context */
	p_nt_drv = &p_drv->ntdrv;
	p_hw_info = &p_nt_drv->adapter_info.hw_info;
	fpga_info = &p_nt_drv->adapter_info.fpga_info;

	p_drv->p_dev = pci_dev;

	/* Set context for NtDrv */
	p_nt_drv->pciident = BDF_TO_PCIIDENT(pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
	p_nt_drv->adapter_info.n_rx_host_buffers = nb_rx_queues;
	p_nt_drv->adapter_info.n_tx_host_buffers = nb_tx_queues;

	fpga_info->bar0_addr = (void *)pci_dev->mem_resource[0].addr;
	fpga_info->bar0_size = pci_dev->mem_resource[0].len;
	fpga_info->numa_node = pci_dev->device.numa_node;
	fpga_info->pciident = p_nt_drv->pciident;
	fpga_info->adapter_no = p_drv->adapter_no;

	p_nt_drv->adapter_info.hw_info.pci_class_id = pci_dev->id.class_id;
	p_nt_drv->adapter_info.hw_info.pci_vendor_id = pci_dev->id.vendor_id;
	p_nt_drv->adapter_info.hw_info.pci_device_id = pci_dev->id.device_id;
	p_nt_drv->adapter_info.hw_info.pci_sub_vendor_id = pci_dev->id.subsystem_vendor_id;
	p_nt_drv->adapter_info.hw_info.pci_sub_device_id = pci_dev->id.subsystem_device_id;

	NT_LOG(DBG, NTNIC, "%s: " PCIIDENT_PRINT_STR " %04X:%04X: %04X:%04X:",
		p_nt_drv->adapter_info.mp_adapter_id_str, PCIIDENT_TO_DOMAIN(p_nt_drv->pciident),
		PCIIDENT_TO_BUSNR(p_nt_drv->pciident), PCIIDENT_TO_DEVNR(p_nt_drv->pciident),
		PCIIDENT_TO_FUNCNR(p_nt_drv->pciident),
		p_nt_drv->adapter_info.hw_info.pci_vendor_id,
		p_nt_drv->adapter_info.hw_info.pci_device_id,
		p_nt_drv->adapter_info.hw_info.pci_sub_vendor_id,
		p_nt_drv->adapter_info.hw_info.pci_sub_device_id);

	p_nt_drv->b_shutdown = false;
	p_nt_drv->adapter_info.pb_shutdown = &p_nt_drv->b_shutdown;

	for (int i = 0; i < num_port_speeds; ++i) {
		struct adapter_info_s *p_adapter_info = &p_nt_drv->adapter_info;
		nt_link_speed_t link_speed = convert_link_speed(pls_mbps[i].link_speed);
		port_ops->set_link_speed(p_adapter_info, i, link_speed);
	}

	/* store context */
	store_pdrv(p_drv);

	/* initialize nt4ga nthw fpga module instance in drv */
	int err = adapter_ops->init(&p_nt_drv->adapter_info);

	if (err != 0) {
		NT_LOG(ERR, NTNIC, "%s: Cannot initialize the adapter instance",
			p_nt_drv->adapter_info.mp_adapter_id_str);
		return -1;
	}

	/* Initialize the queue system */
	if (err == 0) {
		sg_ops = get_sg_ops();

		if (sg_ops != NULL) {
			err = sg_ops->nthw_virt_queue_init(fpga_info);

			if (err != 0) {
				NT_LOG(ERR, NTNIC,
					"%s: Cannot initialize scatter-gather queues",
					p_nt_drv->adapter_info.mp_adapter_id_str);

			} else {
				NT_LOG(DBG, NTNIC, "%s: Initialized scatter-gather queues",
					p_nt_drv->adapter_info.mp_adapter_id_str);
			}

		} else {
			NT_LOG_DBGX(DBG, NTNIC, "SG module is not initialized");
		}
	}

	/* Start ctrl, monitor, stat thread only for primary process. */
	if (err == 0) {
		/* mp_adapter_id_str is initialized after nt4ga_adapter_init(p_nt_drv) */
		const char *const p_adapter_id_str = p_nt_drv->adapter_info.mp_adapter_id_str;
		(void)p_adapter_id_str;
		NT_LOG(DBG, NTNIC,
			"%s: %s: AdapterPCI=" PCIIDENT_PRINT_STR " Hw=0x%02X_rev%d PhyPorts=%d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), p_adapter_id_str,
			PCIIDENT_TO_DOMAIN(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_BUSNR(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_DEVNR(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_FUNCNR(p_nt_drv->adapter_info.fpga_info.pciident),
			p_hw_info->hw_platform_id, fpga_info->nthw_hw_info.hw_id,
			fpga_info->n_phy_ports);

	} else {
		NT_LOG_DBGX(ERR, NTNIC, "%s: error=%d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), err);
		return -1;
	}

	n_phy_ports = fpga_info->n_phy_ports;

	for (int n_intf_no = 0; n_intf_no < n_phy_ports; n_intf_no++) {
		const char *const p_port_id_str = p_nt_drv->adapter_info.mp_port_id_str[n_intf_no];
		(void)p_port_id_str;
		struct pmd_internals *internals = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		char name[32];
		int i;

		if ((1 << n_intf_no) & ~n_port_mask) {
			NT_LOG_DBGX(DBG, NTNIC,
				"%s: interface #%d: skipping due to portmask 0x%02X",
				p_port_id_str, n_intf_no, n_port_mask);
			continue;
		}

		snprintf(name, sizeof(name), "ntnic%d", n_intf_no);
		NT_LOG_DBGX(DBG, NTNIC, "%s: interface #%d: %s: '%s'", p_port_id_str,
			n_intf_no, (pci_dev->name[0] ? pci_dev->name : "NA"), name);

		internals = rte_zmalloc_socket(name, sizeof(struct pmd_internals),
				RTE_CACHE_LINE_SIZE, pci_dev->device.numa_node);

		if (!internals) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: %s: error=%d",
				(pci_dev->name[0] ? pci_dev->name : "NA"), name, -1);
			return -1;
		}

		internals->pci_dev = pci_dev;
		internals->n_intf_no = n_intf_no;
		internals->type = PORT_TYPE_PHYSICAL;
		internals->nb_rx_queues = nb_rx_queues;
		internals->nb_tx_queues = nb_tx_queues;

		/* Not used queue index as dest port in bypass - use 0x80 + port nr */
		for (i = 0; i < MAX_QUEUES; i++)
			internals->vpq[i].hw_id = -1;


		/* Setup queue_ids */
		if (nb_rx_queues > 1) {
			NT_LOG(DBG, NTNIC,
				"(%i) NTNIC configured with Rx multi queues. %i queues",
				internals->n_intf_no, nb_rx_queues);
		}

		if (nb_tx_queues > 1) {
			NT_LOG(DBG, NTNIC,
				"(%i) NTNIC configured with Tx multi queues. %i queues",
				internals->n_intf_no, nb_tx_queues);
		}

		int max_num_queues = (nb_rx_queues > nb_tx_queues) ? nb_rx_queues : nb_tx_queues;
		int start_queue = allocate_queue(max_num_queues);

		if (start_queue < 0)
			return -1;

		for (i = 0; i < (int)max_num_queues; i++) {
			queue_ids[i].id = i;
			queue_ids[i].hw_id = start_queue + i;

			internals->rxq_scg[i].queue = queue_ids[i];
			/* use same index in Rx and Tx rings */
			internals->txq_scg[i].queue = queue_ids[i];
			internals->rxq_scg[i].enabled = 0;
			internals->txq_scg[i].type = internals->type;
			internals->rxq_scg[i].type = internals->type;
			internals->rxq_scg[i].port = internals->port;
		}

		/* no tx queues - tx data goes out on phy */
		internals->vpq_nb_vq = 0;

		for (i = 0; i < (int)nb_tx_queues; i++) {
			internals->txq_scg[i].port = internals->port;
			internals->txq_scg[i].enabled = 0;
		}

		/* Set MAC address (but only if the MAC address is permitted) */
		if (n_intf_no < fpga_info->nthw_hw_info.vpd_info.mn_mac_addr_count) {
			const uint64_t mac =
				fpga_info->nthw_hw_info.vpd_info.mn_mac_addr_value + n_intf_no;
			internals->eth_addrs[0].addr_bytes[0] = (mac >> 40) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[1] = (mac >> 32) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[2] = (mac >> 24) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[3] = (mac >> 16) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[4] = (mac >> 8) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[5] = (mac >> 0) & 0xFFu;
		}

		eth_dev = rte_eth_dev_allocate(name);

		if (!eth_dev) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: %s: error=%d",
				(pci_dev->name[0] ? pci_dev->name : "NA"), name, -1);
			return -1;
		}

		/* connect structs */
		internals->p_drv = p_drv;
		eth_dev->data->dev_private = internals;
		eth_dev->data->mac_addrs = rte_malloc(NULL,
					NUM_MAC_ADDRS_PER_PORT * sizeof(struct rte_ether_addr), 0);
		rte_memcpy(&eth_dev->data->mac_addrs[0],
					&internals->eth_addrs[0], RTE_ETHER_ADDR_LEN);

		NT_LOG_DBGX(DBG, NTNIC, "Setting up RX functions for SCG");
		eth_dev->rx_pkt_burst = eth_dev_rx_scg;
		eth_dev->tx_pkt_burst = eth_dev_tx_scg;
		eth_dev->tx_pkt_prepare = NULL;

		struct rte_eth_link pmd_link;
		pmd_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		pmd_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		pmd_link.link_status = RTE_ETH_LINK_DOWN;
		pmd_link.link_autoneg = RTE_ETH_LINK_AUTONEG;

		eth_dev->device = &pci_dev->device;
		eth_dev->data->dev_link = pmd_link;
		eth_dev->dev_ops = &nthw_eth_dev_ops;

		eth_dev_pci_specific_init(eth_dev, pci_dev);
		rte_eth_dev_probing_finish(eth_dev);

		/* increase initialized ethernet devices - PF */
		p_drv->n_eth_dev_init_count++;
	}

	return 0;
}

static int
nthw_pci_dev_deinit(struct rte_eth_dev *eth_dev __rte_unused)
{
	NT_LOG_DBGX(DBG, NTNIC, "PCI device deinitialization");

	int i;
	char name[32];

	struct pmd_internals *internals = eth_dev->data->dev_private;
	ntdrv_4ga_t *p_ntdrv = &internals->p_drv->ntdrv;
	fpga_info_t *fpga_info = &p_ntdrv->adapter_info.fpga_info;
	const int n_phy_ports = fpga_info->n_phy_ports;

	/* let running threads end Rx and Tx activity */
	if (sg_ops != NULL) {
		nt_os_wait_usec(1 * 1000 * 1000);

		while (internals) {
			for (i = internals->nb_tx_queues - 1; i >= 0; i--) {
				sg_ops->nthw_release_mngd_tx_virt_queue(internals->txq_scg[i].vq);
				release_hw_virtio_queues(&internals->txq_scg[i].hwq);
			}

			for (i = internals->nb_rx_queues - 1; i >= 0; i--) {
				sg_ops->nthw_release_mngd_rx_virt_queue(internals->rxq_scg[i].vq);
				release_hw_virtio_queues(&internals->rxq_scg[i].hwq);
			}

			internals = internals->next;
		}
	}

	for (i = 0; i < n_phy_ports; i++) {
		sprintf(name, "ntnic%d", i);
		eth_dev = rte_eth_dev_allocated(name);
		if (eth_dev == NULL)
			continue; /* port already released */
		rte_eth_dev_release_port(eth_dev);
	}

	nt_vfio_remove(EXCEPTION_PATH_HID);
	return 0;
}

static int
nthw_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int ret;

	NT_LOG_DBGX(DBG, NTNIC, "pcidev: name: '%s'", pci_dev->name);
	NT_LOG_DBGX(DBG, NTNIC, "devargs: name: '%s'", pci_dev->device.name);

	if (pci_dev->device.devargs) {
		NT_LOG_DBGX(DBG, NTNIC, "devargs: args: '%s'",
			(pci_dev->device.devargs->args ? pci_dev->device.devargs->args : "NULL"));
		NT_LOG_DBGX(DBG, NTNIC, "devargs: data: '%s'",
			(pci_dev->device.devargs->data ? pci_dev->device.devargs->data : "NULL"));
	}

	const int n_rte_vfio_no_io_mmu_enabled = rte_vfio_noiommu_is_enabled();
	NT_LOG(DBG, NTNIC, "vfio_no_iommu_enabled=%d", n_rte_vfio_no_io_mmu_enabled);

	if (n_rte_vfio_no_io_mmu_enabled) {
		NT_LOG(ERR, NTNIC, "vfio_no_iommu_enabled=%d: this PMD needs VFIO IOMMU",
			n_rte_vfio_no_io_mmu_enabled);
		return -1;
	}

	const enum rte_iova_mode n_rte_io_va_mode = rte_eal_iova_mode();
	NT_LOG(DBG, NTNIC, "iova mode=%d", n_rte_io_va_mode);

	NT_LOG(DBG, NTNIC,
		"busid=" PCI_PRI_FMT
		" pciid=%04x:%04x_%04x:%04x locstr=%s @ numanode=%d: drv=%s drvalias=%s",
		pci_dev->addr.domain, pci_dev->addr.bus, pci_dev->addr.devid,
		pci_dev->addr.function, pci_dev->id.vendor_id, pci_dev->id.device_id,
		pci_dev->id.subsystem_vendor_id, pci_dev->id.subsystem_device_id,
		pci_dev->name[0] ? pci_dev->name : "NA",
		pci_dev->device.numa_node,
		pci_dev->driver->driver.name ? pci_dev->driver->driver.name : "NA",
		pci_dev->driver->driver.alias ? pci_dev->driver->driver.alias : "NA");


	ret = nthw_pci_dev_init(pci_dev);

	NT_LOG_DBGX(DBG, NTNIC, "leave: ret=%d", ret);
	return ret;
}

static int
nthw_pci_remove(struct rte_pci_device *pci_dev)
{
	NT_LOG_DBGX(DBG, NTNIC);

	struct drv_s *p_drv = get_pdrv_from_pci(pci_dev->addr);
	drv_deinit(p_drv);

	return rte_eth_dev_pci_generic_remove(pci_dev, nthw_pci_dev_deinit);
}

static struct rte_pci_driver rte_nthw_pmd = {
	.id_table = nthw_pci_id_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = nthw_pci_probe,
	.remove = nthw_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ntnic, rte_nthw_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ntnic, nthw_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ntnic, "* vfio-pci");

RTE_LOG_REGISTER_SUFFIX(nt_log_general, general, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_nthw, nthw, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_filter, filter, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_ntnic, ntnic, INFO);
