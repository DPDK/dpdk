/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_common.h>
#include <unistd.h>

#include "ntos_drv.h"
#include "nt_util.h"
#include "ntnic_virt_queue.h"
#include "ntnic_mod_reg.h"
#include "ntlog.h"

#define STRUCT_ALIGNMENT (4 * 1024LU)
#define MAX_VIRT_QUEUES 128

#define LAST_QUEUE 127
#define DISABLE 0
#define ENABLE 1
#define RX_AM_DISABLE DISABLE
#define RX_AM_ENABLE ENABLE
#define RX_UW_DISABLE DISABLE
#define RX_UW_ENABLE ENABLE
#define RX_Q_DISABLE DISABLE
#define RX_Q_ENABLE ENABLE
#define RX_AM_POLL_SPEED 5
#define RX_UW_POLL_SPEED 9
#define INIT_QUEUE 1

#define TX_AM_DISABLE DISABLE
#define TX_AM_ENABLE ENABLE
#define TX_UW_DISABLE DISABLE
#define TX_UW_ENABLE ENABLE
#define TX_Q_DISABLE DISABLE
#define TX_Q_ENABLE ENABLE
#define TX_AM_POLL_SPEED 5
#define TX_UW_POLL_SPEED 8

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

#define vq_log_arg(vq, format, ...)

/*
 * Packed Ring helper macros
 */
#define PACKED(vq_type) ((vq_type) == PACKED_RING ? 1 : 0)

#define avail_flag(vq) ((vq)->avail_wrap_count ? VIRTQ_DESC_F_AVAIL : 0)
#define used_flag_inv(vq) ((vq)->avail_wrap_count ? 0 : VIRTQ_DESC_F_USED)

#define inc_avail(vq, num)                                                                        \
	do {                                                                                      \
		struct nthw_virt_queue *temp_vq = (vq);                                           \
		temp_vq->next_avail += (num);                                                     \
		if (temp_vq->next_avail >= temp_vq->queue_size) {                                 \
			temp_vq->next_avail -= temp_vq->queue_size;                               \
			temp_vq->avail_wrap_count ^= 1;                                           \
		}                                                                                 \
	} while (0)

#define inc_used(vq, num) do { \
	struct nthw_virt_queue *temp_vq = (vq); \
	temp_vq->next_used += (num); \
	if (temp_vq->next_used >= temp_vq->queue_size) { \
		temp_vq->next_used -= temp_vq->queue_size; \
		temp_vq->used_wrap_count ^= 1; \
	} \
} while (0)

struct __rte_packed_begin virtq_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];	/* Queue Size */
} __rte_packed_end;

struct __rte_packed_begin virtq_used_elem {
	/* Index of start of used descriptor chain. */
	uint32_t id;
	/* Total length of the descriptor chain which was used (written to) */
	uint32_t len;
} __rte_packed_end;

struct __rte_packed_begin virtq_used {
	uint16_t flags;
	uint16_t idx;
	struct virtq_used_elem ring[];	/* Queue Size */
} __rte_packed_end;

struct virtq_struct_layout_s {
	size_t used_offset;
	size_t desc_offset;
};

enum nthw_virt_queue_usage {
	NTHW_VIRTQ_UNUSED = 0,
	NTHW_VIRTQ_UNMANAGED,
	NTHW_VIRTQ_MANAGED
};

struct nthw_virt_queue {
	/* Pointers to virt-queue structs */
	union {
		struct {
			/* SPLIT virtqueue */
			struct virtq_avail *p_avail;
			struct virtq_used *p_used;
			struct virtq_desc *p_desc;
			/* Control variables for virt-queue structs */
			uint16_t am_idx;
			uint16_t used_idx;
			uint16_t cached_idx;
			uint16_t tx_descr_avail_idx;
		};
		struct {
			/* PACKED virtqueue */
			struct pvirtq_event_suppress *driver_event;
			struct pvirtq_event_suppress *device_event;
			struct pvirtq_desc *desc;
			struct {
				uint16_t next;
				uint16_t num;
			} outs;
			/*
			 * when in-order release used Tx packets from FPGA it may collapse
			 * into a batch. When getting new Tx buffers we may only need
			 * partial
			 */
			uint16_t next_avail;
			uint16_t next_used;
			uint16_t avail_wrap_count;
			uint16_t used_wrap_count;
		};
	};

	/* Array with packet buffers */
	struct nthw_memory_descriptor *p_virtual_addr;

	/* Queue configuration info */
	nthw_dbs_t *mp_nthw_dbs;

	enum nthw_virt_queue_usage usage;
	uint16_t irq_vector;
	uint16_t vq_type;
	uint16_t in_order;

	uint16_t queue_size;
	uint32_t index;
	uint32_t am_enable;
	uint32_t host_id;
	uint32_t port;	/* Only used by TX queues */
	uint32_t virtual_port;	/* Only used by TX queues */
	uint32_t header;
	/*
	 * Only used by TX queues:
	 *   0: VirtIO-Net header (12 bytes).
	 *   1: Napatech DVIO0 descriptor (12 bytes).
	 */
	void *avail_struct_phys_addr;
	void *used_struct_phys_addr;
	void *desc_struct_phys_addr;
};

struct pvirtq_struct_layout_s {
	size_t driver_event_offset;
	size_t device_event_offset;
};

static struct nthw_virt_queue rxvq[MAX_VIRT_QUEUES];
static struct nthw_virt_queue txvq[MAX_VIRT_QUEUES];

static void dbs_init_rx_queue(nthw_dbs_t *p_nthw_dbs, uint32_t queue, uint32_t start_idx,
	uint32_t start_ptr)
{
	uint32_t busy;
	uint32_t init;
	uint32_t dummy;

	do {
		nthw_get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	nthw_set_rx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		nthw_get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static void dbs_init_tx_queue(nthw_dbs_t *p_nthw_dbs, uint32_t queue, uint32_t start_idx,
	uint32_t start_ptr)
{
	uint32_t busy;
	uint32_t init;
	uint32_t dummy;

	do {
		nthw_get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	nthw_set_tx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		nthw_get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static int nthw_virt_queue_init(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);

	nthw_fpga_t *const p_fpga = p_fpga_info->mp_fpga;
	nthw_dbs_t *p_nthw_dbs;
	int res = 0;
	uint32_t i;

	p_fpga_info->mp_nthw_dbs = NULL;

	p_nthw_dbs = nthw_dbs_new();

	if (p_nthw_dbs == NULL)
		return -1;

	res = nthw_dbs_init(NULL, p_fpga, 0);/* Check that DBS exists in FPGA */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	res = nthw_dbs_init(p_nthw_dbs, p_fpga, 0);	/* Create DBS module */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	p_fpga_info->mp_nthw_dbs = p_nthw_dbs;

	for (i = 0; i < MAX_VIRT_QUEUES; ++i) {
		rxvq[i].usage = NTHW_VIRTQ_UNUSED;
		txvq[i].usage = NTHW_VIRTQ_UNUSED;
	}

	nthw_dbs_reset(p_nthw_dbs);

	for (i = 0; i < NT_DBS_RX_QUEUES_MAX; ++i)
		dbs_init_rx_queue(p_nthw_dbs, i, 0, 0);

	for (i = 0; i < NT_DBS_TX_QUEUES_MAX; ++i)
		dbs_init_tx_queue(p_nthw_dbs, i, 0, 0);

	nthw_set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_DISABLE, RX_AM_POLL_SPEED, RX_UW_DISABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	nthw_set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	nthw_set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_ENABLE);

	nthw_set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_DISABLE, TX_AM_POLL_SPEED, TX_UW_DISABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	nthw_set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	nthw_set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
		TX_UW_POLL_SPEED, TX_Q_ENABLE);

	return 0;
}

static struct virtq_struct_layout_s dbs_calc_struct_layout(uint32_t queue_size)
{
	/* + sizeof(uint16_t); ("avail->used_event" is not used) */
	size_t avail_mem = sizeof(struct virtq_avail) + queue_size * sizeof(uint16_t);
	size_t avail_mem_aligned = ((avail_mem % STRUCT_ALIGNMENT) == 0)
		? avail_mem
		: STRUCT_ALIGNMENT * (avail_mem / STRUCT_ALIGNMENT + 1);

	/* + sizeof(uint16_t); ("used->avail_event" is not used) */
	size_t used_mem = sizeof(struct virtq_used) + queue_size * sizeof(struct virtq_used_elem);
	size_t used_mem_aligned = ((used_mem % STRUCT_ALIGNMENT) == 0)
		? used_mem
		: STRUCT_ALIGNMENT * (used_mem / STRUCT_ALIGNMENT + 1);

	struct virtq_struct_layout_s virtq_layout;
	virtq_layout.used_offset = avail_mem_aligned;
	virtq_layout.desc_offset = avail_mem_aligned + used_mem_aligned;

	return virtq_layout;
}

static void dbs_initialize_avail_struct(void *addr, uint16_t queue_size,
	uint16_t initial_avail_idx)
{
	uint16_t i;
	struct virtq_avail *p_avail = (struct virtq_avail *)addr;

	p_avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
	p_avail->idx = initial_avail_idx;

	for (i = 0; i < queue_size; ++i)
		p_avail->ring[i] = i;
}

static void dbs_initialize_used_struct(void *addr, uint16_t queue_size)
{
	int i;
	struct virtq_used *p_used = (struct virtq_used *)addr;

	p_used->flags = 1;
	p_used->idx = 0;

	for (i = 0; i < queue_size; ++i) {
		p_used->ring[i].id = 0;
		p_used->ring[i].len = 0;
	}
}

static void
dbs_initialize_descriptor_struct(void *addr,
	struct nthw_memory_descriptor *packet_buffer_descriptors,
	uint16_t queue_size, uint16_t flgs)
{
	if (packet_buffer_descriptors) {
		int i;
		struct virtq_desc *p_desc = (struct virtq_desc *)addr;

		for (i = 0; i < queue_size; ++i) {
			p_desc[i].addr = (uint64_t)packet_buffer_descriptors[i].phys_addr;
			p_desc[i].len = packet_buffer_descriptors[i].len;
			p_desc[i].flags = flgs;
			p_desc[i].next = 0;
		}
	}
}

static void
dbs_initialize_virt_queue_structs(void *avail_struct_addr, void *used_struct_addr,
	void *desc_struct_addr,
	struct nthw_memory_descriptor *packet_buffer_descriptors,
	uint16_t queue_size, uint16_t initial_avail_idx, uint16_t flgs)
{
	dbs_initialize_avail_struct(avail_struct_addr, queue_size, initial_avail_idx);
	dbs_initialize_used_struct(used_struct_addr, queue_size);
	dbs_initialize_descriptor_struct(desc_struct_addr, packet_buffer_descriptors, queue_size,
		flgs);
}

static uint8_t dbs_qsize_log2(uint16_t qsize)
{
	uint8_t qs = 0;

	while (qsize) {
		qsize = qsize >> 1;
		++qs;
	}

	--qs;
	return qs;
}

static struct nthw_virt_queue *nthw_setup_rx_virt_queue(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint16_t start_idx,
	uint16_t start_ptr,
	void *avail_struct_phys_addr,
	void *used_struct_phys_addr,
	void *desc_struct_phys_addr,
	uint16_t queue_size,
	uint32_t host_id,
	uint32_t header,
	uint32_t vq_type,
	int irq_vector,
	uint8_t rx_deferred_start)
{
	uint32_t qs = dbs_qsize_log2(queue_size);
	uint32_t int_enable;
	uint32_t vec;
	uint32_t istk;

	/*
	 * Setup DBS module - DSF00094
	 * 3. Configure the DBS.RX_DR_DATA memory; good idea to initialize all
	 * DBS_RX_QUEUES entries.
	 */
	if (nthw_set_rx_dr_data(p_nthw_dbs, index, (uint64_t)desc_struct_phys_addr,
		host_id, qs, header, 0) != 0) {
		return NULL;
	}

	/*
	 * 4. Configure the DBS.RX_UW_DATA memory; good idea to initialize all
	 *   DBS_RX_QUEUES entries.
	 *   Notice: We always start out with interrupts disabled (by setting the
	 *     "irq_vector" argument to -1). Queues that require interrupts will have
	 *     it enabled at a later time (after we have enabled vfio interrupts in
	 *     the kernel).
	 */
	int_enable = 0;
	vec = 0;
	istk = 0;
	NT_LOG_DBGX(DBG, NTNIC, "nthw_set_rx_uw_data int=0 irq_vector=%i", irq_vector);

	if (nthw_set_rx_uw_data(p_nthw_dbs, index,
			(uint64_t)used_struct_phys_addr,
			host_id, qs, 0, int_enable, vec, istk) != 0) {
		return NULL;
	}

	/*
	 * 2. Configure the DBS.RX_AM_DATA memory and enable the queues you plan to use;
	 *  good idea to initialize all DBS_RX_QUEUES entries.
	 *  Notice: We do this only for queues that don't require interrupts (i.e. if
	 *    irq_vector < 0). Queues that require interrupts will have RX_AM_DATA enabled
	 *    at a later time (after we have enabled vfio interrupts in the kernel).
	 */
	if (irq_vector < 0) {
		if (nthw_set_rx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr,
				RX_AM_DISABLE, host_id, 0,
				0) != 0) {
			return NULL;
		}
	}

	/*
	 * 5. Initialize all RX queues (all DBS_RX_QUEUES of them) using the
	 *   DBS.RX_INIT register.
	 */
	dbs_init_rx_queue(p_nthw_dbs, index, start_idx, start_ptr);

	/*
	 * 2. Configure the DBS.RX_AM_DATA memory and enable the queues you plan to use;
	 *  good idea to initialize all DBS_RX_QUEUES entries.
	 */
	uint32_t enable = rx_deferred_start ? RX_AM_DISABLE : RX_AM_ENABLE;
	if (nthw_set_rx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr, enable,
			host_id, 0, irq_vector >= 0 ? 1 : 0) != 0) {
		return NULL;
	}

	/* Save queue state */
	rxvq[index].usage = NTHW_VIRTQ_UNMANAGED;
	rxvq[index].mp_nthw_dbs = p_nthw_dbs;
	rxvq[index].index = index;
	rxvq[index].queue_size = queue_size;
	rxvq[index].am_enable = (irq_vector < 0) ? RX_AM_ENABLE : RX_AM_DISABLE;
	rxvq[index].host_id = host_id;
	rxvq[index].avail_struct_phys_addr = avail_struct_phys_addr;
	rxvq[index].used_struct_phys_addr = used_struct_phys_addr;
	rxvq[index].desc_struct_phys_addr = desc_struct_phys_addr;
	rxvq[index].vq_type = vq_type;
	rxvq[index].in_order = 0;	/* not used */
	rxvq[index].irq_vector = irq_vector;

	/* Return queue handle */
	return &rxvq[index];
}

static int dbs_wait_hw_queue_shutdown(struct nthw_virt_queue *vq, int rx);

static int dbs_wait_on_busy(struct nthw_virt_queue *vq, uint32_t *idle, int rx)
{
	uint32_t busy;
	uint32_t queue;
	int err = 0;
	nthw_dbs_t *p_nthw_dbs = vq->mp_nthw_dbs;

	do {
		if (rx)
			err = nthw_get_rx_idle(p_nthw_dbs, idle, &queue, &busy);

		else
			err = nthw_get_tx_idle(p_nthw_dbs, idle, &queue, &busy);
	} while (!err && busy);

	return err;
}

static int dbs_wait_hw_queue_shutdown(struct nthw_virt_queue *vq, int rx)
{
	int err = 0;
	uint32_t idle = 0;
	nthw_dbs_t *p_nthw_dbs = vq->mp_nthw_dbs;

	err = dbs_wait_on_busy(vq, &idle, rx);

	if (err) {
		if (err == -ENOTSUP) {
			nthw_os_wait_usec(200000);
			return 0;
		}

		return -1;
	}

	do {
		if (rx)
			err = nthw_set_rx_idle(p_nthw_dbs, 1, vq->index);

		else
			err = nthw_set_tx_idle(p_nthw_dbs, 1, vq->index);

		if (err)
			return -1;

		if (dbs_wait_on_busy(vq, &idle, rx) != 0)
			return -1;

	} while (idle == 0);

	return 0;
}

static int dbs_internal_release_rx_virt_queue(struct nthw_virt_queue *rxvq)
{
	if (rxvq == NULL)
		return -1;

	nthw_dbs_t *p_nthw_dbs = rxvq->mp_nthw_dbs;

	/* Clear UW */
	rxvq->used_struct_phys_addr = NULL;

	if (nthw_set_rx_uw_data(p_nthw_dbs, rxvq->index, (uint64_t)rxvq->used_struct_phys_addr,
			rxvq->host_id, 0, PACKED(rxvq->vq_type), 0, 0, 0) != 0) {
		return -1;
	}

	/* Disable AM */
	rxvq->am_enable = RX_AM_DISABLE;

	if (nthw_set_rx_am_data(p_nthw_dbs,
			rxvq->index,
			(uint64_t)rxvq->avail_struct_phys_addr,
			rxvq->am_enable,
			rxvq->host_id,
			PACKED(rxvq->vq_type),
			0) != 0) {
		return -1;
	}

	/* Let the FPGA finish packet processing */
	if (dbs_wait_hw_queue_shutdown(rxvq, 1) != 0)
		return -1;

	/* Clear rest of AM */
	rxvq->avail_struct_phys_addr = NULL;
	rxvq->host_id = 0;

	if (nthw_set_rx_am_data(p_nthw_dbs,
			rxvq->index,
			(uint64_t)rxvq->avail_struct_phys_addr,
			rxvq->am_enable,
			rxvq->host_id,
			PACKED(rxvq->vq_type),
			0) != 0)
		return -1;

	/* Clear DR */
	rxvq->desc_struct_phys_addr = NULL;

	if (nthw_set_rx_dr_data(p_nthw_dbs,
			rxvq->index,
			(uint64_t)rxvq->desc_struct_phys_addr,
			rxvq->host_id,
			0,
			rxvq->header,
			PACKED(rxvq->vq_type)) != 0)
		return -1;

	/* Initialize queue */
	dbs_init_rx_queue(p_nthw_dbs, rxvq->index, 0, 0);

	/* Reset queue state */
	rxvq->usage = NTHW_VIRTQ_UNUSED;
	rxvq->mp_nthw_dbs = p_nthw_dbs;
	rxvq->index = 0;
	rxvq->queue_size = 0;

	return 0;
}

static int nthw_release_mngd_rx_virt_queue(struct nthw_virt_queue *rxvq)
{
	if (rxvq == NULL || rxvq->usage != NTHW_VIRTQ_MANAGED)
		return -1;

	if (rxvq->p_virtual_addr) {
		free(rxvq->p_virtual_addr);
		rxvq->p_virtual_addr = NULL;
	}

	return dbs_internal_release_rx_virt_queue(rxvq);
}

static int dbs_internal_release_tx_virt_queue(struct nthw_virt_queue *txvq)
{
	if (txvq == NULL)
		return -1;

	nthw_dbs_t *p_nthw_dbs = txvq->mp_nthw_dbs;

	/* Clear UW */
	txvq->used_struct_phys_addr = NULL;

	if (nthw_set_tx_uw_data(p_nthw_dbs, txvq->index, (uint64_t)txvq->used_struct_phys_addr,
			txvq->host_id, 0, PACKED(txvq->vq_type), 0, 0, 0,
			txvq->in_order) != 0) {
		return -1;
	}

	/* Disable AM */
	txvq->am_enable = TX_AM_DISABLE;

	if (nthw_set_tx_am_data(p_nthw_dbs,
			txvq->index,
			(uint64_t)txvq->avail_struct_phys_addr,
			txvq->am_enable,
			txvq->host_id,
			PACKED(txvq->vq_type),
			0) != 0) {
		return -1;
	}

	/* Let the FPGA finish packet processing */
	if (dbs_wait_hw_queue_shutdown(txvq, 0) != 0)
		return -1;

	/* Clear rest of AM */
	txvq->avail_struct_phys_addr = NULL;
	txvq->host_id = 0;

	if (nthw_set_tx_am_data(p_nthw_dbs,
			txvq->index,
			(uint64_t)txvq->avail_struct_phys_addr,
			txvq->am_enable,
			txvq->host_id,
			PACKED(txvq->vq_type),
			0) != 0) {
		return -1;
	}

	/* Clear DR */
	txvq->desc_struct_phys_addr = NULL;
	txvq->port = 0;
	txvq->header = 0;

	if (nthw_set_tx_dr_data(p_nthw_dbs,
			txvq->index,
			(uint64_t)txvq->desc_struct_phys_addr,
			txvq->host_id,
			0,
			txvq->port,
			txvq->header,
			PACKED(txvq->vq_type)) != 0) {
		return -1;
	}

	/* Clear QP */
	txvq->virtual_port = 0;

	if (nthw_dbs_set_tx_qp_data(p_nthw_dbs, txvq->index, txvq->virtual_port) != 0)
		return -1;

	/* Initialize queue */
	dbs_init_tx_queue(p_nthw_dbs, txvq->index, 0, 0);

	/* Reset queue state */
	txvq->usage = NTHW_VIRTQ_UNUSED;
	txvq->mp_nthw_dbs = p_nthw_dbs;
	txvq->index = 0;
	txvq->queue_size = 0;

	return 0;
}

static int nthw_release_mngd_tx_virt_queue(struct nthw_virt_queue *txvq)
{
	if (txvq == NULL || txvq->usage != NTHW_VIRTQ_MANAGED)
		return -1;

	if (txvq->p_virtual_addr) {
		free(txvq->p_virtual_addr);
		txvq->p_virtual_addr = NULL;
	}

	return dbs_internal_release_tx_virt_queue(txvq);
}

static struct nthw_virt_queue *nthw_setup_tx_virt_queue(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint16_t start_idx,
	uint16_t start_ptr,
	void *avail_struct_phys_addr,
	void *used_struct_phys_addr,
	void *desc_struct_phys_addr,
	uint16_t queue_size,
	uint32_t host_id,
	uint32_t port,
	uint32_t virtual_port,
	uint32_t header,
	uint32_t vq_type,
	int irq_vector,
	uint32_t in_order,
	uint8_t tx_deferred_start)
{
	uint32_t int_enable;
	uint32_t vec;
	uint32_t istk;
	uint32_t qs = dbs_qsize_log2(queue_size);

	/*
	 * Setup DBS module - DSF00094
	 * 3. Configure the DBS.TX_DR_DATA memory; good idea to initialize all
	 *    DBS_TX_QUEUES entries.
	 */
	if (nthw_set_tx_dr_data(p_nthw_dbs, index, (uint64_t)desc_struct_phys_addr,
			host_id, qs, port, header, 0) != 0) {
		return NULL;
	}

	/*
	 * 4. Configure the DBS.TX_UW_DATA memory; good idea to initialize all
	 *    DBS_TX_QUEUES entries.
	 *    Notice: We always start out with interrupts disabled (by setting the
	 *            "irq_vector" argument to -1). Queues that require interrupts will have
	 *             it enabled at a later time (after we have enabled vfio interrupts in the
	 *             kernel).
	 */
	int_enable = 0;
	vec = 0;
	istk = 0;

	if (nthw_set_tx_uw_data(p_nthw_dbs, index,
			(uint64_t)used_struct_phys_addr,
			host_id, qs, 0, int_enable, vec, istk, in_order) != 0) {
		return NULL;
	}

	/*
	 * 2. Configure the DBS.TX_AM_DATA memory and enable the queues you plan to use;
	 *    good idea to initialize all DBS_TX_QUEUES entries.
	 */
	if (nthw_set_tx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr, TX_AM_DISABLE,
			host_id, 0, irq_vector >= 0 ? 1 : 0) != 0) {
		return NULL;
	}

	/*
	 * 5. Initialize all TX queues (all DBS_TX_QUEUES of them) using the
	 *    DBS.TX_INIT register.
	 */
	dbs_init_tx_queue(p_nthw_dbs, index, start_idx, start_ptr);

	if (nthw_dbs_set_tx_qp_data(p_nthw_dbs, index, virtual_port) != 0)
		return NULL;

	/*
	 * 2. Configure the DBS.TX_AM_DATA memory and enable the queues you plan to use;
	 *    good idea to initialize all DBS_TX_QUEUES entries.
	 *    Notice: We do this only for queues that don't require interrupts (i.e. if
	 *            irq_vector < 0). Queues that require interrupts will have TX_AM_DATA
	 *            enabled at a later time (after we have enabled vfio interrupts in the
	 *            kernel).
	 */
	if (irq_vector < 0) {
		uint32_t enable = tx_deferred_start ? TX_AM_DISABLE : TX_AM_ENABLE;
		if (nthw_set_tx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr,
				enable, host_id, 0, 0) != 0) {
			return NULL;
		}
	}

	/* Save queue state */
	txvq[index].usage = NTHW_VIRTQ_UNMANAGED;
	txvq[index].mp_nthw_dbs = p_nthw_dbs;
	txvq[index].index = index;
	txvq[index].queue_size = queue_size;
	txvq[index].am_enable = (irq_vector < 0) ? TX_AM_ENABLE : TX_AM_DISABLE;
	txvq[index].host_id = host_id;
	txvq[index].port = port;
	txvq[index].virtual_port = virtual_port;
	txvq[index].avail_struct_phys_addr = avail_struct_phys_addr;
	txvq[index].used_struct_phys_addr = used_struct_phys_addr;
	txvq[index].desc_struct_phys_addr = desc_struct_phys_addr;
	txvq[index].vq_type = vq_type;
	txvq[index].in_order = in_order;
	txvq[index].irq_vector = irq_vector;

	/* Return queue handle */
	return &txvq[index];
}

static struct nthw_virt_queue *
nthw_setup_mngd_rx_virt_queue_split(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t header,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	int irq_vector,
	uint8_t rx_deferred_start)
{
	struct virtq_struct_layout_s virtq_struct_layout = dbs_calc_struct_layout(queue_size);

	dbs_initialize_virt_queue_structs(p_virt_struct_area->virt_addr,
		(char *)p_virt_struct_area->virt_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->virt_addr +
		virtq_struct_layout.desc_offset,
		p_packet_buffers,
		(uint16_t)queue_size,
		p_packet_buffers ? (uint16_t)queue_size : 0,
		VIRTQ_DESC_F_WRITE /* Rx */);

	rxvq[index].p_avail = p_virt_struct_area->virt_addr;
	rxvq[index].p_used =
		(void *)((char *)p_virt_struct_area->virt_addr + virtq_struct_layout.used_offset);
	rxvq[index].p_desc =
		(void *)((char *)p_virt_struct_area->virt_addr + virtq_struct_layout.desc_offset);

	rxvq[index].am_idx = p_packet_buffers ? (uint16_t)queue_size : 0;
	rxvq[index].used_idx = 0;
	rxvq[index].cached_idx = 0;
	rxvq[index].p_virtual_addr = NULL;

	if (p_packet_buffers) {
		rxvq[index].p_virtual_addr = malloc(queue_size * sizeof(*p_packet_buffers));
		if (rxvq[index].p_virtual_addr)
			memcpy(rxvq[index].p_virtual_addr, p_packet_buffers,
				queue_size * sizeof(*p_packet_buffers));
	}

	nthw_setup_rx_virt_queue(p_nthw_dbs, index, 0, 0, (void *)p_virt_struct_area->phys_addr,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.desc_offset,
		(uint16_t)queue_size, host_id, header, SPLIT_RING, irq_vector, rx_deferred_start);

	rxvq[index].usage = NTHW_VIRTQ_MANAGED;

	return &rxvq[index];
}

static struct nthw_virt_queue *
nthw_setup_mngd_tx_virt_queue_split(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t port,
	uint32_t virtual_port,
	uint32_t header,
	int irq_vector,
	uint32_t in_order,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	uint8_t tx_deferred_start)
{
	struct virtq_struct_layout_s virtq_struct_layout = dbs_calc_struct_layout(queue_size);

	dbs_initialize_virt_queue_structs(p_virt_struct_area->virt_addr,
		(char *)p_virt_struct_area->virt_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->virt_addr +
		virtq_struct_layout.desc_offset,
		p_packet_buffers,
		(uint16_t)queue_size,
		0,
		0 /* Tx */);

	txvq[index].p_avail = p_virt_struct_area->virt_addr;
	txvq[index].p_used =
		(void *)((char *)p_virt_struct_area->virt_addr + virtq_struct_layout.used_offset);
	txvq[index].p_desc =
		(void *)((char *)p_virt_struct_area->virt_addr + virtq_struct_layout.desc_offset);
	txvq[index].queue_size = (uint16_t)queue_size;
	txvq[index].am_idx = 0;
	txvq[index].used_idx = 0;
	txvq[index].cached_idx = 0;
	txvq[index].p_virtual_addr = NULL;

	txvq[index].tx_descr_avail_idx = 0;

	if (p_packet_buffers) {
		txvq[index].p_virtual_addr = malloc(queue_size * sizeof(*p_packet_buffers));
		if (txvq[index].p_virtual_addr)
			memcpy(txvq[index].p_virtual_addr, p_packet_buffers,
				queue_size * sizeof(*p_packet_buffers));
	}

	nthw_setup_tx_virt_queue(p_nthw_dbs, index, 0, 0, (void *)p_virt_struct_area->phys_addr,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.desc_offset,
		(uint16_t)queue_size, host_id, port, virtual_port, header,
		SPLIT_RING, irq_vector, in_order, tx_deferred_start);

	txvq[index].usage = NTHW_VIRTQ_MANAGED;

	return &txvq[index];
}

/*
 * Packed Ring
 */
static int nthw_setup_managed_virt_queue_packed(struct nthw_virt_queue *vq,
	struct pvirtq_struct_layout_s *pvirtq_layout,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	uint16_t flags,
	int rx)
{
	/* page aligned */
	RTE_ASSERT(((uintptr_t)p_virt_struct_area->phys_addr & 0xfff) == 0);
	RTE_ASSERT(p_packet_buffers);

	/* clean canvas */
	if (p_virt_struct_area->virt_addr == NULL)
		return -1;

	memset(p_virt_struct_area->virt_addr, 0,
		sizeof(struct pvirtq_desc) * vq->queue_size +
		sizeof(struct pvirtq_event_suppress) * 2 + sizeof(int) * vq->queue_size);

	pvirtq_layout->device_event_offset = sizeof(struct pvirtq_desc) * vq->queue_size;
	pvirtq_layout->driver_event_offset =
		pvirtq_layout->device_event_offset + sizeof(struct pvirtq_event_suppress);

	vq->desc = p_virt_struct_area->virt_addr;
	vq->device_event = (void *)((uintptr_t)vq->desc + pvirtq_layout->device_event_offset);
	vq->driver_event = (void *)((uintptr_t)vq->desc + pvirtq_layout->driver_event_offset);

	vq->next_avail = 0;
	vq->next_used = 0;
	vq->avail_wrap_count = 1;
	vq->used_wrap_count = 1;

	/*
	 * Only possible if FPGA always delivers in-order
	 * Buffer ID used is the index in the p_packet_buffers array
	 */
	unsigned int i;
	struct pvirtq_desc *p_desc = vq->desc;

	for (i = 0; i < vq->queue_size; i++) {
		if (rx) {
			p_desc[i].addr = (uint64_t)p_packet_buffers[i].phys_addr;
			p_desc[i].len = p_packet_buffers[i].len;
		}

		p_desc[i].id = i;
		p_desc[i].flags = flags;
	}

	if (rx)
		vq->avail_wrap_count ^= 1;	/* filled up available buffers for Rx */
	else
		vq->used_wrap_count ^= 1;	/* pre-fill free buffer IDs */

	if (vq->queue_size == 0)
		return -1;	/* don't allocate memory with size of 0 bytes */

	vq->p_virtual_addr = malloc(vq->queue_size * sizeof(*p_packet_buffers));

	if (vq->p_virtual_addr == NULL)
		return -1;

	memcpy(vq->p_virtual_addr, p_packet_buffers, vq->queue_size * sizeof(*p_packet_buffers));

	/* Not used yet by FPGA - make sure we disable */
	vq->device_event->flags = RING_EVENT_FLAGS_DISABLE;

	return 0;
}

static struct nthw_virt_queue *
nthw_setup_managed_rx_virt_queue_packed(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t header,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	int irq_vector,
	uint8_t rx_deferred_start)
{
	struct pvirtq_struct_layout_s pvirtq_layout;
	struct nthw_virt_queue *vq = &rxvq[index];
	/* Set size and setup packed vq ring */
	vq->queue_size = queue_size;

	/* Use Avail flag bit == 1 because wrap bit is initially set to 1 - and Used is inverse */
	if (nthw_setup_managed_virt_queue_packed(vq, &pvirtq_layout, p_virt_struct_area,
			p_packet_buffers,
			VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_AVAIL, 1) != 0)
		return NULL;

	nthw_setup_rx_virt_queue(p_nthw_dbs, index, 0x8000, 0,	/* start wrap ring counter as 1 */
		(void *)((uintptr_t)p_virt_struct_area->phys_addr +
			pvirtq_layout.driver_event_offset),
		(void *)((uintptr_t)p_virt_struct_area->phys_addr +
			pvirtq_layout.device_event_offset),
		p_virt_struct_area->phys_addr, (uint16_t)queue_size, host_id,
		header, PACKED_RING, irq_vector, rx_deferred_start);

	vq->usage = NTHW_VIRTQ_MANAGED;
	return vq;
}

static struct nthw_virt_queue *
nthw_setup_managed_tx_virt_queue_packed(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t port,
	uint32_t virtual_port,
	uint32_t header,
	int irq_vector,
	uint32_t in_order,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	uint8_t tx_deferred_start)
{
	struct pvirtq_struct_layout_s pvirtq_layout;
	struct nthw_virt_queue *vq = &txvq[index];
	/* Set size and setup packed vq ring */
	vq->queue_size = queue_size;

	if (nthw_setup_managed_virt_queue_packed(vq, &pvirtq_layout, p_virt_struct_area,
			p_packet_buffers, 0, 0) != 0)
		return NULL;

	nthw_setup_tx_virt_queue(p_nthw_dbs, index, 0x8000, 0,	/* start wrap ring counter as 1 */
		(void *)((uintptr_t)p_virt_struct_area->phys_addr +
			pvirtq_layout.driver_event_offset),
		(void *)((uintptr_t)p_virt_struct_area->phys_addr +
			pvirtq_layout.device_event_offset),
		p_virt_struct_area->phys_addr, (uint16_t)queue_size, host_id,
		port, virtual_port, header, PACKED_RING, irq_vector, in_order, tx_deferred_start);

	vq->usage = NTHW_VIRTQ_MANAGED;
	return vq;
}

/*
 * Create a Managed Rx Virt Queue
 *
 * Notice: The queue will be created with interrupts disabled.
 *   If interrupts are required, make sure to call nthw_enable_rx_virt_queue()
 *   afterwards.
 */
static struct nthw_virt_queue *
nthw_setup_mngd_rx_virt_queue(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t header,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	uint32_t vq_type,
	int irq_vector,
	uint8_t rx_deferred_start)
{
	switch (vq_type) {
	case SPLIT_RING:
		return nthw_setup_mngd_rx_virt_queue_split(p_nthw_dbs, index, queue_size,
				host_id, header, p_virt_struct_area,
				p_packet_buffers, irq_vector, rx_deferred_start);

	case PACKED_RING:
		return nthw_setup_managed_rx_virt_queue_packed(p_nthw_dbs, index, queue_size,
				host_id, header, p_virt_struct_area,
				p_packet_buffers, irq_vector, rx_deferred_start);

	default:
		break;
	}

	return NULL;
}

/*
 * Create a Managed Tx Virt Queue
 *
 * Notice: The queue will be created with interrupts disabled.
 *   If interrupts are required, make sure to call nthw_enable_tx_virt_queue()
 *   afterwards.
 */
static struct nthw_virt_queue *
nthw_setup_mngd_tx_virt_queue(nthw_dbs_t *p_nthw_dbs,
	uint32_t index,
	uint32_t queue_size,
	uint32_t host_id,
	uint32_t port,
	uint32_t virtual_port,
	uint32_t header,
	struct nthw_memory_descriptor *p_virt_struct_area,
	struct nthw_memory_descriptor *p_packet_buffers,
	uint32_t vq_type,
	int irq_vector,
	uint32_t in_order,
	uint8_t tx_deferred_start)
{
	switch (vq_type) {
	case SPLIT_RING:
		return nthw_setup_mngd_tx_virt_queue_split(p_nthw_dbs, index, queue_size,
				host_id, port, virtual_port, header,
				irq_vector, in_order,
				p_virt_struct_area,
				p_packet_buffers, tx_deferred_start);

	case PACKED_RING:
		return nthw_setup_managed_tx_virt_queue_packed(p_nthw_dbs, index, queue_size,
				host_id, port, virtual_port, header,
				irq_vector, in_order,
				p_virt_struct_area,
				p_packet_buffers, tx_deferred_start);

	default:
		break;
	}

	return NULL;
}

static int nthw_switch_rx_virt_queue(nthw_dbs_t *p_nthw_dbs, uint32_t index, uint32_t enable)
{
	return nthw_set_rx_am_data_enable(p_nthw_dbs, index, enable);
}

static int nthw_switch_tx_virt_queue(nthw_dbs_t *p_nthw_dbs, uint32_t index, uint32_t enable)
{
	return nthw_set_tx_am_data_enable(p_nthw_dbs, index, enable);
}

static uint16_t nthw_get_rx_packets(struct nthw_virt_queue *rxvq,
	uint16_t n,
	struct nthw_received_packets *rp,
	uint16_t *nb_pkts)
{
	uint16_t segs = 0;
	uint16_t pkts = 0;

	if (rxvq->vq_type == SPLIT_RING) {
		uint16_t i;
		uint16_t entries_ready = (uint16_t)(rxvq->cached_idx - rxvq->used_idx);

		if (entries_ready < n) {
			/* Look for more packets */
			rxvq->cached_idx = rxvq->p_used->idx;
			entries_ready = (uint16_t)(rxvq->cached_idx - rxvq->used_idx);

			if (entries_ready == 0) {
				*nb_pkts = 0;
				return 0;
			}

			if (n > entries_ready)
				n = entries_ready;
		}

		/*
		 * Give packets - make sure all packets are whole packets.
		 * Valid because queue_size is always 2^n
		 */
		const uint16_t queue_mask = (uint16_t)(rxvq->queue_size - 1);
		const uint32_t buf_len = rxvq->p_desc[0].len;

		uint16_t used = rxvq->used_idx;

		for (i = 0; i < n; ++i) {
			uint32_t id = rxvq->p_used->ring[used & queue_mask].id;
			rp[i].addr = rxvq->p_virtual_addr[id].virt_addr;
			rp[i].len = rxvq->p_used->ring[used & queue_mask].len;

			uint32_t pkt_len = ((struct _pkt_hdr_rx *)rp[i].addr)->cap_len;

			if (pkt_len > buf_len) {
				/* segmented */
				int nbsegs = (pkt_len + buf_len - 1) / buf_len;

				if (((int)i + nbsegs) > n) {
					/* don't have enough segments - break out */
					break;
				}

				int ii;

				for (ii = 1; ii < nbsegs; ii++) {
					++i;
					id = rxvq->p_used->ring[(used + ii) & queue_mask].id;
					rp[i].addr = rxvq->p_virtual_addr[id].virt_addr;
					rp[i].len =
						rxvq->p_used->ring[(used + ii) & queue_mask].len;
				}

				used += nbsegs;

			} else {
				++used;
			}

			pkts++;
			segs = i + 1;
		}

		rxvq->used_idx = used;

	} else if (rxvq->vq_type == PACKED_RING) {
		/* This requires in-order behavior from FPGA */
		int i;

		for (i = 0; i < n; i++) {
			struct pvirtq_desc *desc = &rxvq->desc[rxvq->next_used];

			uint16_t flags = desc->flags;
			uint8_t avail = !!(flags & VIRTQ_DESC_F_AVAIL);
			uint8_t used = !!(flags & VIRTQ_DESC_F_USED);

			if (avail != rxvq->used_wrap_count || used != rxvq->used_wrap_count)
				break;

			rp[pkts].addr = rxvq->p_virtual_addr[desc->id].virt_addr;
			rp[pkts].len = desc->len;
			pkts++;

			inc_used(rxvq, 1);
		}

		segs = pkts;
	}

	*nb_pkts = pkts;
	return segs;
}

/*
 * Put buffers back into Avail Ring
 */
static void nthw_release_rx_packets(struct nthw_virt_queue *rxvq, uint16_t n)
{
	if (rxvq->vq_type == SPLIT_RING) {
		rxvq->am_idx = (uint16_t)(rxvq->am_idx + n);
		rxvq->p_avail->idx = rxvq->am_idx;

	} else if (rxvq->vq_type == PACKED_RING) {
		int i;
		/*
		 * Defer flags update on first segment - due to serialization towards HW and
		 * when jumbo segments are added
		 */

		uint16_t first_flags = VIRTQ_DESC_F_WRITE | avail_flag(rxvq) | used_flag_inv(rxvq);
		struct pvirtq_desc *first_desc = &rxvq->desc[rxvq->next_avail];

		uint32_t len = rxvq->p_virtual_addr[0].len;	/* all same size */

		/* Optimization point: use in-order release */

		for (i = 0; i < n; i++) {
			struct pvirtq_desc *desc = &rxvq->desc[rxvq->next_avail];

			desc->id = rxvq->next_avail;
			desc->addr = (uint64_t)rxvq->p_virtual_addr[desc->id].phys_addr;
			desc->len = len;

			if (i)
				desc->flags = VIRTQ_DESC_F_WRITE | avail_flag(rxvq) |
					used_flag_inv(rxvq);

			inc_avail(rxvq, 1);
		}

		rte_rmb();
		first_desc->flags = first_flags;
	}
}

static uint16_t nthw_get_tx_packets(struct nthw_virt_queue *txvq,
	uint16_t n,
	uint16_t *first_idx,
	struct nthw_cvirtq_desc *cvq,
	struct nthw_memory_descriptor **p_virt_addr)
{
	int m = 0;
	uint16_t queue_mask =
		(uint16_t)(txvq->queue_size - 1);	/* Valid because queue_size is always 2^n */
	*p_virt_addr = txvq->p_virtual_addr;

	if (txvq->vq_type == SPLIT_RING) {
		cvq->s = txvq->p_desc;
		cvq->vq_type = SPLIT_RING;

		*first_idx = txvq->tx_descr_avail_idx;

		uint16_t entries_used =
			(uint16_t)((txvq->tx_descr_avail_idx - txvq->cached_idx) & queue_mask);
		uint16_t entries_ready = (uint16_t)(txvq->queue_size - 1 - entries_used);

		vq_log_arg(txvq,
			"ask %i: descrAvail %i, cachedidx %i, used: %i, ready %i used->idx %i",
			n, txvq->tx_descr_avail_idx, txvq->cached_idx, entries_used, entries_ready,
			txvq->p_used->idx);

		if (entries_ready < n) {
			/*
			 * Look for more packets.
			 * Using the used_idx in the avail ring since they are held synchronous
			 * because of in-order
			 */
			txvq->cached_idx =
				txvq->p_avail->ring[(txvq->p_used->idx - 1) & queue_mask];

			vq_log_arg(txvq, "Update: get cachedidx %i (used_idx-1 %i)",
				txvq->cached_idx, (txvq->p_used->idx - 1) & queue_mask);
			entries_used =
				(uint16_t)((txvq->tx_descr_avail_idx - txvq->cached_idx)
				& queue_mask);
			entries_ready = (uint16_t)(txvq->queue_size - 1 - entries_used);
			vq_log_arg(txvq, "new used: %i, ready %i", entries_used, entries_ready);

			if (n > entries_ready)
				n = entries_ready;
		}

	} else if (txvq->vq_type == PACKED_RING) {
		int i;

		cvq->p = txvq->desc;
		cvq->vq_type = PACKED_RING;

		if (txvq->outs.num) {
			*first_idx = txvq->outs.next;
			uint16_t num = min(n, txvq->outs.num);
			txvq->outs.next = (txvq->outs.next + num) & queue_mask;
			txvq->outs.num -= num;

			if (n == num)
				return n;

			m = num;
			n -= num;

		} else {
			*first_idx = txvq->next_used;
		}

		/* iterate the ring - this requires in-order behavior from FPGA */
		for (i = 0; i < n; i++) {
			struct pvirtq_desc *desc = &txvq->desc[txvq->next_used];

			uint16_t flags = desc->flags;
			uint8_t avail = !!(flags & VIRTQ_DESC_F_AVAIL);
			uint8_t used = !!(flags & VIRTQ_DESC_F_USED);

			if (avail != txvq->used_wrap_count || used != txvq->used_wrap_count) {
				n = i;
				break;
			}

			uint16_t incr = (desc->id - txvq->next_used) & queue_mask;
			i += incr;
			inc_used(txvq, incr + 1);
		}

		if (i > n) {
			int outs_num = i - n;
			txvq->outs.next = (txvq->next_used - outs_num) & queue_mask;
			txvq->outs.num = outs_num;
		}

	} else {
		return 0;
	}

	return m + n;
}

static void nthw_release_tx_packets(struct nthw_virt_queue *txvq, uint16_t n, uint16_t n_segs[])
{
	int i;

	if (txvq->vq_type == SPLIT_RING) {
		/* Valid because queue_size is always 2^n */
		uint16_t queue_mask = (uint16_t)(txvq->queue_size - 1);

		vq_log_arg(txvq, "pkts %i, avail idx %i, start at %i", n, txvq->am_idx,
			txvq->tx_descr_avail_idx);

		for (i = 0; i < n; i++) {
			int idx = txvq->am_idx & queue_mask;
			txvq->p_avail->ring[idx] = txvq->tx_descr_avail_idx;
			txvq->tx_descr_avail_idx =
				(txvq->tx_descr_avail_idx + n_segs[i]) & queue_mask;
			txvq->am_idx++;
		}

		/* Make sure the ring has been updated before HW reads index update */
		rte_mb();
		txvq->p_avail->idx = txvq->am_idx;
		vq_log_arg(txvq, "new avail idx %i, descr_idx %i", txvq->p_avail->idx,
			txvq->tx_descr_avail_idx);

	} else if (txvq->vq_type == PACKED_RING) {
		/*
		 * Defer flags update on first segment - due to serialization towards HW and
		 * when jumbo segments are added
		 */

		uint16_t first_flags = avail_flag(txvq) | used_flag_inv(txvq);
		struct pvirtq_desc *first_desc = &txvq->desc[txvq->next_avail];

		for (i = 0; i < n; i++) {
			struct pvirtq_desc *desc = &txvq->desc[txvq->next_avail];

			desc->id = txvq->next_avail;
			desc->addr = (uint64_t)txvq->p_virtual_addr[desc->id].phys_addr;

			if (i)
				/* bitwise-or here because next flags may already have been setup
				 */
				desc->flags |= avail_flag(txvq) | used_flag_inv(txvq);

			inc_avail(txvq, 1);
		}

		/* Proper read barrier before FPGA may see first flags */
		rte_rmb();
		first_desc->flags = first_flags;
	}
}

static struct sg_ops_s sg_ops = {
	.nthw_setup_rx_virt_queue = nthw_setup_rx_virt_queue,
	.nthw_setup_tx_virt_queue = nthw_setup_tx_virt_queue,
	.nthw_setup_mngd_rx_virt_queue = nthw_setup_mngd_rx_virt_queue,
	.nthw_release_mngd_rx_virt_queue = nthw_release_mngd_rx_virt_queue,
	.nthw_setup_mngd_tx_virt_queue = nthw_setup_mngd_tx_virt_queue,
	.nthw_release_mngd_tx_virt_queue = nthw_release_mngd_tx_virt_queue,
	.nthw_switch_rx_virt_queue = nthw_switch_rx_virt_queue,
	.nthw_switch_tx_virt_queue = nthw_switch_tx_virt_queue,
	.nthw_get_rx_packets = nthw_get_rx_packets,
	.nthw_release_rx_packets = nthw_release_rx_packets,
	.nthw_get_tx_packets = nthw_get_tx_packets,
	.nthw_release_tx_packets = nthw_release_tx_packets,
	.nthw_virt_queue_init = nthw_virt_queue_init
};

void nthw_sg_init(void)
{
	NT_LOG(INF, NTNIC, "SG ops initialized");
	register_sg_ops(&sg_ops);
}
