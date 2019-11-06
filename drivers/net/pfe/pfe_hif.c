/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include "pfe_logs.h"
#include "pfe_mod.h"
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

static int
pfe_hif_alloc_descr(struct pfe_hif *hif)
{
	void *addr;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	addr = rte_zmalloc(NULL, HIF_RX_DESC_NT * sizeof(struct hif_desc) +
		HIF_TX_DESC_NT * sizeof(struct hif_desc), RTE_CACHE_LINE_SIZE);
	if (!addr) {
		PFE_PMD_ERR("Could not allocate buffer descriptors!");
		err = -ENOMEM;
		goto err0;
	}

	hif->descr_baseaddr_p = pfe_mem_vtop((uintptr_t)addr);
	hif->descr_baseaddr_v = addr;
	hif->rx_ring_size = HIF_RX_DESC_NT;
	hif->tx_ring_size = HIF_TX_DESC_NT;

	return 0;

err0:
	return err;
}

static void
pfe_hif_free_descr(struct pfe_hif *hif)
{
	PMD_INIT_FUNC_TRACE();

	rte_free(hif->descr_baseaddr_v);
}

/* pfe_hif_release_buffers */
static void
pfe_hif_release_buffers(struct pfe_hif *hif)
{
	struct hif_desc	*desc;
	uint32_t i = 0;
	struct rte_mbuf *mbuf;
	struct rte_pktmbuf_pool_private *mb_priv;

	hif->rx_base = hif->descr_baseaddr_v;

	/*Free Rx buffers */
	desc = hif->rx_base;
	mb_priv = rte_mempool_get_priv(hif->shm->pool);
	for (i = 0; i < hif->rx_ring_size; i++) {
		if (readl(&desc->data)) {
			if (i < hif->shm->rx_buf_pool_cnt &&
			    !hif->shm->rx_buf_pool[i]) {
				mbuf = hif->rx_buf_vaddr[i] + PFE_PKT_HEADER_SZ
					- sizeof(struct rte_mbuf)
					- RTE_PKTMBUF_HEADROOM
					- mb_priv->mbuf_priv_size;
				hif->shm->rx_buf_pool[i] = mbuf;
			}
		}
		writel(0, &desc->data);
		writel(0, &desc->status);
		writel(0, &desc->ctrl);
		desc++;
	}
}

/*
 * pfe_hif_init_buffers
 * This function initializes the HIF Rx/Tx ring descriptors and
 * initialize Rx queue with buffers.
 */
int
pfe_hif_init_buffers(struct pfe_hif *hif)
{
	struct hif_desc	*desc, *first_desc_p;
	uint32_t i = 0;

	PMD_INIT_FUNC_TRACE();

	/* Check enough Rx buffers available in the shared memory */
	if (hif->shm->rx_buf_pool_cnt < hif->rx_ring_size)
		return -ENOMEM;

	hif->rx_base = hif->descr_baseaddr_v;
	memset(hif->rx_base, 0, hif->rx_ring_size * sizeof(struct hif_desc));

	/*Initialize Rx descriptors */
	desc = hif->rx_base;
	first_desc_p = (struct hif_desc *)hif->descr_baseaddr_p;

	for (i = 0; i < hif->rx_ring_size; i++) {
		/* Initialize Rx buffers from the shared memory */
		struct rte_mbuf *mbuf =
			(struct rte_mbuf *)hif->shm->rx_buf_pool[i];

		/* PFE mbuf structure is as follow:
		 * ----------------------------------------------------------+
		 * | mbuf  | priv | headroom (annotation + PFE data) | data  |
		 * ----------------------------------------------------------+
		 *
		 * As we are expecting additional information like parse
		 * results, eth id, queue id from PFE block along with data.
		 * so we have to provide additional memory for each packet to
		 * HIF rx rings so that PFE block can write its headers.
		 * so, we are giving the data pointor to HIF rings whose
		 * calculation is as below:
		 * mbuf->data_pointor - Required_header_size
		 *
		 * We are utilizing the HEADROOM area to receive the PFE
		 * block headers. On packet reception, HIF driver will use
		 * PFE headers information based on which it will decide
		 * the clients and fill the parse results.
		 * after that application can use/overwrite the HEADROOM area.
		 */
		hif->rx_buf_vaddr[i] =
			(void *)((size_t)mbuf->buf_addr + mbuf->data_off -
					PFE_PKT_HEADER_SZ);
		hif->rx_buf_addr[i] =
			(void *)(size_t)(rte_pktmbuf_iova(mbuf) -
					PFE_PKT_HEADER_SZ);
		hif->rx_buf_len[i] =  mbuf->buf_len - RTE_PKTMBUF_HEADROOM;

		hif->shm->rx_buf_pool[i] = NULL;

		writel(DDR_PHYS_TO_PFE(hif->rx_buf_addr[i]),
					&desc->data);
		writel(0, &desc->status);

		/*
		 * Ensure everything else is written to DDR before
		 * writing bd->ctrl
		 */
		rte_wmb();

		writel((BD_CTRL_PKT_INT_EN | BD_CTRL_LIFM
			| BD_CTRL_DIR | BD_CTRL_DESC_EN
			| BD_BUF_LEN(hif->rx_buf_len[i])), &desc->ctrl);

		/* Chain descriptors */
		writel((u32)DDR_PHYS_TO_PFE(first_desc_p + i + 1), &desc->next);
		desc++;
	}

	/* Overwrite last descriptor to chain it to first one*/
	desc--;
	writel((u32)DDR_PHYS_TO_PFE(first_desc_p), &desc->next);

	hif->rxtoclean_index = 0;

	/*Initialize Rx buffer descriptor ring base address */
	writel(DDR_PHYS_TO_PFE(hif->descr_baseaddr_p), HIF_RX_BDP_ADDR);

	hif->tx_base = hif->rx_base + hif->rx_ring_size;
	first_desc_p = (struct hif_desc *)hif->descr_baseaddr_p +
				hif->rx_ring_size;
	memset(hif->tx_base, 0, hif->tx_ring_size * sizeof(struct hif_desc));

	/*Initialize tx descriptors */
	desc = hif->tx_base;

	for (i = 0; i < hif->tx_ring_size; i++) {
		/* Chain descriptors */
		writel((u32)DDR_PHYS_TO_PFE(first_desc_p + i + 1), &desc->next);
		writel(0, &desc->ctrl);
		desc++;
	}

	/* Overwrite last descriptor to chain it to first one */
	desc--;
	writel((u32)DDR_PHYS_TO_PFE(first_desc_p), &desc->next);
	hif->txavail = hif->tx_ring_size;
	hif->txtosend = 0;
	hif->txtoclean = 0;
	hif->txtoflush = 0;

	/*Initialize Tx buffer descriptor ring base address */
	writel((u32)DDR_PHYS_TO_PFE(first_desc_p), HIF_TX_BDP_ADDR);

	return 0;
}

/*
 * pfe_hif_client_register
 *
 * This function used to register a client driver with the HIF driver.
 *
 * Return value:
 * 0 - on Successful registration
 */
static int
pfe_hif_client_register(struct pfe_hif *hif, u32 client_id,
			struct hif_client_shm *client_shm)
{
	struct hif_client *client = &hif->client[client_id];
	u32 i, cnt;
	struct rx_queue_desc *rx_qbase;
	struct tx_queue_desc *tx_qbase;
	struct hif_rx_queue *rx_queue;
	struct hif_tx_queue *tx_queue;
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	rte_spinlock_lock(&hif->tx_lock);

	if (test_bit(client_id, &hif->shm->g_client_status[0])) {
		PFE_PMD_ERR("client %d already registered", client_id);
		err = -1;
		goto unlock;
	}

	memset(client, 0, sizeof(struct hif_client));

	/* Initialize client Rx queues baseaddr, size */

	cnt = CLIENT_CTRL_RX_Q_CNT(client_shm->ctrl);
	/* Check if client is requesting for more queues than supported */
	if (cnt > HIF_CLIENT_QUEUES_MAX)
		cnt = HIF_CLIENT_QUEUES_MAX;

	client->rx_qn = cnt;
	rx_qbase = (struct rx_queue_desc *)client_shm->rx_qbase;
	for (i = 0; i < cnt; i++) {
		rx_queue = &client->rx_q[i];
		rx_queue->base = rx_qbase + i * client_shm->rx_qsize;
		rx_queue->size = client_shm->rx_qsize;
		rx_queue->write_idx = 0;
	}

	/* Initialize client Tx queues baseaddr, size */
	cnt = CLIENT_CTRL_TX_Q_CNT(client_shm->ctrl);

	/* Check if client is requesting for more queues than supported */
	if (cnt > HIF_CLIENT_QUEUES_MAX)
		cnt = HIF_CLIENT_QUEUES_MAX;

	client->tx_qn = cnt;
	tx_qbase = (struct tx_queue_desc *)client_shm->tx_qbase;
	for (i = 0; i < cnt; i++) {
		tx_queue = &client->tx_q[i];
		tx_queue->base = tx_qbase + i * client_shm->tx_qsize;
		tx_queue->size = client_shm->tx_qsize;
		tx_queue->ack_idx = 0;
	}

	set_bit(client_id, &hif->shm->g_client_status[0]);

unlock:
	rte_spinlock_unlock(&hif->tx_lock);

	return err;
}

/*
 * pfe_hif_client_unregister
 *
 * This function used to unregister a client  from the HIF driver.
 *
 */
static void
pfe_hif_client_unregister(struct pfe_hif *hif, u32 client_id)
{
	PMD_INIT_FUNC_TRACE();

	/*
	 * Mark client as no longer available (which prevents further packet
	 * receive for this client)
	 */
	rte_spinlock_lock(&hif->tx_lock);

	if (!test_bit(client_id, &hif->shm->g_client_status[0])) {
		PFE_PMD_ERR("client %d not registered", client_id);

		rte_spinlock_unlock(&hif->tx_lock);
		return;
	}

	clear_bit(client_id, &hif->shm->g_client_status[0]);

	rte_spinlock_unlock(&hif->tx_lock);
}

/*
 * client_put_rxpacket-
 */
static struct rte_mbuf *
client_put_rxpacket(struct hif_rx_queue *queue,
		void *pkt, u32 len,
		u32 flags, u32 client_ctrl,
		struct rte_mempool *pool,
		u32 *rem_len)
{
	struct rx_queue_desc *desc = queue->base + queue->write_idx;
	struct rte_mbuf *mbuf = NULL;


	if (readl(&desc->ctrl) & CL_DESC_OWN) {
		mbuf = rte_cpu_to_le_64(rte_pktmbuf_alloc(pool));
		if (unlikely(!mbuf)) {
			PFE_PMD_WARN("Buffer allocation failure\n");
			return NULL;
		}

		desc->data = pkt;
		desc->client_ctrl = client_ctrl;
		/*
		 * Ensure everything else is written to DDR before
		 * writing bd->ctrl
		 */
		rte_wmb();
		writel(CL_DESC_BUF_LEN(len) | flags, &desc->ctrl);
		queue->write_idx = (queue->write_idx + 1)
				    & (queue->size - 1);

		*rem_len = mbuf->buf_len;
	}

	return mbuf;
}

/*
 * pfe_hif_rx_process-
 * This function does pfe hif rx queue processing.
 * Dequeue packet from Rx queue and send it to corresponding client queue
 */
int
pfe_hif_rx_process(struct pfe *pfe, int budget)
{
	struct hif_desc	*desc;
	struct hif_hdr *pkt_hdr;
	struct __hif_hdr hif_hdr;
	void *free_buf;
	int rtc, len, rx_processed = 0;
	struct __hif_desc local_desc;
	int flags = 0, wait_for_last = 0, retry = 0;
	unsigned int buf_size = 0;
	struct rte_mbuf *mbuf = NULL;
	struct pfe_hif *hif = &pfe->hif;

	rte_spinlock_lock(&hif->lock);

	rtc = hif->rxtoclean_index;

	while (rx_processed < budget) {
		desc = hif->rx_base + rtc;

		__memcpy12(&local_desc, desc);

		/* ACK pending Rx interrupt */
		if (local_desc.ctrl & BD_CTRL_DESC_EN) {
			if (unlikely(wait_for_last))
				continue;
			else
				break;
		}

		len = BD_BUF_LEN(local_desc.ctrl);
		pkt_hdr = (struct hif_hdr *)hif->rx_buf_vaddr[rtc];

		/* Track last HIF header received */
		if (!hif->started) {
			hif->started = 1;

			__memcpy8(&hif_hdr, pkt_hdr);

			hif->qno = hif_hdr.hdr.q_num;
			hif->client_id = hif_hdr.hdr.client_id;
			hif->client_ctrl = (hif_hdr.hdr.client_ctrl1 << 16) |
						hif_hdr.hdr.client_ctrl;
			flags = CL_DESC_FIRST;

		} else {
			flags = 0;
		}

		if (local_desc.ctrl & BD_CTRL_LIFM) {
			flags |= CL_DESC_LAST;
			wait_for_last = 0;
		} else {
			wait_for_last = 1;
		}

		/* Check for valid client id and still registered */
		if (hif->client_id >= HIF_CLIENTS_MAX ||
		    !(test_bit(hif->client_id,
			&hif->shm->g_client_status[0]))) {
			PFE_PMD_INFO("packet with invalid client id %d qnum %d",
				hif->client_id, hif->qno);

			free_buf = hif->rx_buf_addr[rtc];

			goto pkt_drop;
		}

		/* Check to valid queue number */
		if (hif->client[hif->client_id].rx_qn <= hif->qno) {
			PFE_DP_LOG(DEBUG, "packet with invalid queue: %d",
					hif->qno);
			hif->qno = 0;
		}

retry:
		mbuf =
		client_put_rxpacket(&hif->client[hif->client_id].rx_q[hif->qno],
				    (void *)pkt_hdr, len, flags,
				    hif->client_ctrl, hif->shm->pool,
				    &buf_size);

		if (unlikely(!mbuf)) {
			if (!retry) {
				pfe_tx_do_cleanup(pfe);
				retry = 1;
				goto retry;
			}
			rx_processed = budget;

			if (flags & CL_DESC_FIRST)
				hif->started = 0;

			PFE_DP_LOG(DEBUG, "No buffers");
			break;
		}

		retry = 0;

		free_buf = (void *)(size_t)rte_pktmbuf_iova(mbuf);
		free_buf = free_buf - PFE_PKT_HEADER_SZ;

		/*Fill free buffer in the descriptor */
		hif->rx_buf_addr[rtc] = free_buf;
		hif->rx_buf_vaddr[rtc] = (void *)((size_t)mbuf->buf_addr +
				mbuf->data_off - PFE_PKT_HEADER_SZ);
		hif->rx_buf_len[rtc] = buf_size - RTE_PKTMBUF_HEADROOM;

pkt_drop:
		writel(DDR_PHYS_TO_PFE(free_buf), &desc->data);
		/*
		 * Ensure everything else is written to DDR before
		 * writing bd->ctrl
		 */
		rte_wmb();
		writel((BD_CTRL_PKT_INT_EN | BD_CTRL_LIFM | BD_CTRL_DIR |
			BD_CTRL_DESC_EN | BD_BUF_LEN(hif->rx_buf_len[rtc])),
			&desc->ctrl);

		rtc = (rtc + 1) & (hif->rx_ring_size - 1);

		if (local_desc.ctrl & BD_CTRL_LIFM) {
			if (!(hif->client_ctrl & HIF_CTRL_RX_CONTINUED))
				rx_processed++;

			hif->started = 0;
		}
	}


	hif->rxtoclean_index = rtc;
	rte_spinlock_unlock(&hif->lock);

	/* we made some progress, re-start rx dma in case it stopped */
	hif_rx_dma_start();

	return rx_processed;
}

/*
 * client_ack_txpacket-
 * This function ack the Tx packet in the give client Tx queue by resetting
 * ownership bit in the descriptor.
 */
static int
client_ack_txpacket(struct pfe_hif *hif, unsigned int client_id,
		    unsigned int q_no)
{
	struct hif_tx_queue *queue = &hif->client[client_id].tx_q[q_no];
	struct tx_queue_desc *desc = queue->base + queue->ack_idx;

	if (readl(&desc->ctrl) & CL_DESC_OWN) {
		writel((readl(&desc->ctrl) & ~CL_DESC_OWN), &desc->ctrl);
		queue->ack_idx = (queue->ack_idx + 1) & (queue->size - 1);

		return 0;

	} else {
		/*This should not happen */
		PFE_PMD_ERR("%d %d %d %d %d %p %d",
		       hif->txtosend, hif->txtoclean, hif->txavail,
			client_id, q_no, queue, queue->ack_idx);
		return 1;
	}
}

static void
__hif_tx_done_process(struct pfe *pfe, int count)
{
	struct hif_desc *desc;
	struct hif_desc_sw *desc_sw;
	unsigned int ttc, tx_avl;
	int pkts_done[HIF_CLIENTS_MAX] = {0, 0};
	struct pfe_hif *hif = &pfe->hif;

	ttc = hif->txtoclean;
	tx_avl = hif->txavail;

	while ((tx_avl < hif->tx_ring_size) && count--) {
		desc = hif->tx_base + ttc;

		if (readl(&desc->ctrl) & BD_CTRL_DESC_EN)
			break;

		desc_sw = &hif->tx_sw_queue[ttc];

		if (desc_sw->client_id > HIF_CLIENTS_MAX)
			PFE_PMD_ERR("Invalid cl id %d", desc_sw->client_id);

		pkts_done[desc_sw->client_id]++;

		client_ack_txpacket(hif, desc_sw->client_id, desc_sw->q_no);

		ttc = (ttc + 1) & (hif->tx_ring_size - 1);
		tx_avl++;
	}

	if (pkts_done[0])
		hif_lib_indicate_client(pfe->hif_client[0], EVENT_TXDONE_IND,
				0);
	if (pkts_done[1])
		hif_lib_indicate_client(pfe->hif_client[1], EVENT_TXDONE_IND,
				0);
	hif->txtoclean = ttc;
	hif->txavail = tx_avl;
}

static inline void
hif_tx_done_process(struct pfe *pfe, int count)
{
	struct pfe_hif *hif = &pfe->hif;
	rte_spinlock_lock(&hif->tx_lock);
	__hif_tx_done_process(pfe, count);
	rte_spinlock_unlock(&hif->tx_lock);
}

void
pfe_tx_do_cleanup(struct pfe *pfe)
{
	hif_tx_done_process(pfe, HIF_TX_DESC_NT);
}

/*
 * __hif_xmit_pkt -
 * This function puts one packet in the HIF Tx queue
 */
void
hif_xmit_pkt(struct pfe_hif *hif, unsigned int client_id, unsigned int
	     q_no, void *data, u32 len, unsigned int flags)
{
	struct hif_desc	*desc;
	struct hif_desc_sw *desc_sw;

	desc = hif->tx_base + hif->txtosend;
	desc_sw = &hif->tx_sw_queue[hif->txtosend];

	desc_sw->len = len;
	desc_sw->client_id = client_id;
	desc_sw->q_no = q_no;
	desc_sw->flags = flags;

	writel((u32)DDR_PHYS_TO_PFE(data), &desc->data);

	hif->txtosend = (hif->txtosend + 1) & (hif->tx_ring_size - 1);
	hif->txavail--;

	if ((!((flags & HIF_DATA_VALID) && (flags &
				HIF_LAST_BUFFER))))
		goto skip_tx;

	/*
	 * Ensure everything else is written to DDR before
	 * writing bd->ctrl
	 */
	rte_wmb();

	do {
		desc_sw = &hif->tx_sw_queue[hif->txtoflush];
		desc = hif->tx_base + hif->txtoflush;

		if (desc_sw->flags & HIF_LAST_BUFFER) {
			writel((BD_CTRL_LIFM |
			       BD_CTRL_BRFETCH_DISABLE | BD_CTRL_RTFETCH_DISABLE
			       | BD_CTRL_PARSE_DISABLE | BD_CTRL_DESC_EN |
				 BD_BUF_LEN(desc_sw->len)),
				&desc->ctrl);
		} else {
			writel((BD_CTRL_DESC_EN |
				BD_BUF_LEN(desc_sw->len)), &desc->ctrl);
		}
		hif->txtoflush = (hif->txtoflush + 1) & (hif->tx_ring_size - 1);
	}
	while (hif->txtoflush != hif->txtosend)
		;

skip_tx:
	return;
}

void
hif_process_client_req(struct pfe_hif *hif, int req,
			    int data1, __rte_unused int data2)
{
	unsigned int client_id = data1;

	if (client_id >= HIF_CLIENTS_MAX) {
		PFE_PMD_ERR("client id %d out of bounds", client_id);
		return;
	}

	switch (req) {
	case REQUEST_CL_REGISTER:
			/* Request for register a client */
			PFE_PMD_INFO("register client_id %d", client_id);
			pfe_hif_client_register(hif, client_id, (struct
				hif_client_shm *)&hif->shm->client[client_id]);
			break;

	case REQUEST_CL_UNREGISTER:
			PFE_PMD_INFO("unregister client_id %d", client_id);

			/* Request for unregister a client */
			pfe_hif_client_unregister(hif, client_id);

			break;

	default:
			PFE_PMD_ERR("unsupported request %d", req);
			break;
	}

	/*
	 * Process client Tx queues
	 * Currently we don't have checking for tx pending
	 */
}

#if defined(LS1012A_PFE_RESET_WA)
static void
pfe_hif_disable_rx_desc(struct pfe_hif *hif)
{
	u32 ii;
	struct hif_desc	*desc = hif->rx_base;

	/*Mark all descriptors as LAST_BD */
	for (ii = 0; ii < hif->rx_ring_size; ii++) {
		desc->ctrl |= BD_CTRL_LAST_BD;
		desc++;
	}
}

struct class_rx_hdr_t {
	u32     next_ptr;       /* ptr to the start of the first DDR buffer */
	u16     length;         /* total packet length */
	u16     phyno;          /* input physical port number */
	u32     status;         /* gemac status bits */
	u32     status2;            /* reserved for software usage */
};

/* STATUS_BAD_FRAME_ERR is set for all errors (including checksums if enabled)
 * except overflow
 */
#define STATUS_BAD_FRAME_ERR            BIT(16)
#define STATUS_LENGTH_ERR               BIT(17)
#define STATUS_CRC_ERR                  BIT(18)
#define STATUS_TOO_SHORT_ERR            BIT(19)
#define STATUS_TOO_LONG_ERR             BIT(20)
#define STATUS_CODE_ERR                 BIT(21)
#define STATUS_MC_HASH_MATCH            BIT(22)
#define STATUS_CUMULATIVE_ARC_HIT       BIT(23)
#define STATUS_UNICAST_HASH_MATCH       BIT(24)
#define STATUS_IP_CHECKSUM_CORRECT      BIT(25)
#define STATUS_TCP_CHECKSUM_CORRECT     BIT(26)
#define STATUS_UDP_CHECKSUM_CORRECT     BIT(27)
#define STATUS_OVERFLOW_ERR             BIT(28) /* GPI error */
#define MIN_PKT_SIZE			64
#define DUMMY_PKT_COUNT			128

static inline void
copy_to_lmem(u32 *dst, u32 *src, int len)
{
	int i;

	for (i = 0; i < len; i += sizeof(u32))	{
		*dst = htonl(*src);
		dst++; src++;
	}
}
#if defined(RTE_TOOLCHAIN_GCC)
__attribute__ ((optimize(1)))
#endif
static void
send_dummy_pkt_to_hif(void)
{
	void *lmem_ptr, *ddr_ptr, *lmem_virt_addr;
	u64 physaddr;
	struct class_rx_hdr_t local_hdr;
	static u32 dummy_pkt[] =  {
		0x33221100, 0x2b785544, 0xd73093cb, 0x01000608,
		0x04060008, 0x2b780200, 0xd73093cb, 0x0a01a8c0,
		0x33221100, 0xa8c05544, 0x00000301, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0xbe86c51f };

	ddr_ptr = (void *)(size_t)readl(BMU2_BASE_ADDR + BMU_ALLOC_CTRL);
	if (!ddr_ptr)
		return;

	lmem_ptr = (void *)(size_t)readl(BMU1_BASE_ADDR + BMU_ALLOC_CTRL);
	if (!lmem_ptr)
		return;

	PFE_PMD_INFO("Sending a dummy pkt to HIF %p %p", ddr_ptr, lmem_ptr);
	physaddr = DDR_VIRT_TO_PFE(ddr_ptr);

	lmem_virt_addr = (void *)CBUS_PFE_TO_VIRT((unsigned long)lmem_ptr);

	local_hdr.phyno = htons(0); /* RX_PHY_0 */
	local_hdr.length = htons(MIN_PKT_SIZE);

	local_hdr.next_ptr = htonl((u32)physaddr);
	/*Mark checksum is correct */
	local_hdr.status = htonl((STATUS_IP_CHECKSUM_CORRECT |
				STATUS_UDP_CHECKSUM_CORRECT |
				STATUS_TCP_CHECKSUM_CORRECT |
				STATUS_UNICAST_HASH_MATCH |
				STATUS_CUMULATIVE_ARC_HIT));
	copy_to_lmem((u32 *)lmem_virt_addr, (u32 *)&local_hdr,
		     sizeof(local_hdr));

	copy_to_lmem((u32 *)(lmem_virt_addr + LMEM_HDR_SIZE), (u32 *)dummy_pkt,
		     0x40);

	writel((unsigned long)lmem_ptr, CLASS_INQ_PKTPTR);
}

void
pfe_hif_rx_idle(struct pfe_hif *hif)
{
	int hif_stop_loop = DUMMY_PKT_COUNT;
	u32 rx_status;

	pfe_hif_disable_rx_desc(hif);
	PFE_PMD_INFO("Bringing hif to idle state...");
	writel(0, HIF_INT_ENABLE);
	/*If HIF Rx BDP is busy send a dummy packet */
	do {
		rx_status = readl(HIF_RX_STATUS);
		if (rx_status & BDP_CSR_RX_DMA_ACTV)
			send_dummy_pkt_to_hif();

		sleep(1);
	} while (--hif_stop_loop);

	if (readl(HIF_RX_STATUS) & BDP_CSR_RX_DMA_ACTV)
		PFE_PMD_ERR("Failed\n");
	else
		PFE_PMD_INFO("Done\n");
}
#endif

/*
 * pfe_hif_init
 * This function initializes the baseaddresses and irq, etc.
 */
int
pfe_hif_init(struct pfe *pfe)
{
	struct pfe_hif *hif = &pfe->hif;
	int err;

	PMD_INIT_FUNC_TRACE();

#if defined(LS1012A_PFE_RESET_WA)
	pfe_hif_rx_idle(hif);
#endif

	err = pfe_hif_alloc_descr(hif);
	if (err)
		goto err0;

	rte_spinlock_init(&hif->tx_lock);
	rte_spinlock_init(&hif->lock);

	gpi_enable(HGPI_BASE_ADDR);
	if (getenv("PFE_INTR_SUPPORT")) {
		struct epoll_event epoll_ev;
		int event_fd = -1, epoll_fd, pfe_cdev_fd;

		pfe_cdev_fd = open(PFE_CDEV_PATH, O_RDWR);
		if (pfe_cdev_fd < 0) {
			PFE_PMD_WARN("Unable to open PFE device file (%s).\n",
				     PFE_CDEV_PATH);
			pfe->cdev_fd = PFE_CDEV_INVALID_FD;
			return -1;
		}
		pfe->cdev_fd = pfe_cdev_fd;

		event_fd = eventfd(0, EFD_NONBLOCK);
		/* hif interrupt enable */
		err = ioctl(pfe->cdev_fd, PFE_CDEV_HIF_INTR_EN, &event_fd);
		if (err) {
			PFE_PMD_ERR("\nioctl failed for intr enable err: %d\n",
					errno);
			goto err0;
		}
		epoll_fd = epoll_create(1);
		epoll_ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
		epoll_ev.data.fd = event_fd;
		err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &epoll_ev);
		if (err < 0) {
			PFE_PMD_ERR("epoll_ctl failed with err = %d\n", errno);
			goto err0;
		}
		pfe->hif.epoll_fd = epoll_fd;
	}
	return 0;
err0:
	return err;
}

/* pfe_hif_exit- */
void
pfe_hif_exit(struct pfe *pfe)
{
	struct pfe_hif *hif = &pfe->hif;

	PMD_INIT_FUNC_TRACE();

	rte_spinlock_lock(&hif->lock);
	hif->shm->g_client_status[0] = 0;
	/* Make sure all clients are disabled*/
	hif->shm->g_client_status[1] = 0;

	rte_spinlock_unlock(&hif->lock);

	if (hif->setuped) {
#if defined(LS1012A_PFE_RESET_WA)
		pfe_hif_rx_idle(hif);
#endif
		/*Disable Rx/Tx */
		hif_rx_disable();
		hif_tx_disable();

		pfe_hif_release_buffers(hif);
		pfe_hif_shm_clean(hif->shm);

		pfe_hif_free_descr(hif);
		pfe->hif.setuped = 0;
	}
	gpi_disable(HGPI_BASE_ADDR);
}
