/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <unistd.h>

#include "ntos_drv.h"
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

struct __rte_aligned(8) virtq_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];	/* Queue Size */
};

struct __rte_aligned(8) virtq_used_elem {
	/* Index of start of used descriptor chain. */
	uint32_t id;
	/* Total length of the descriptor chain which was used (written to) */
	uint32_t len;
};

struct __rte_aligned(8) virtq_used {
	uint16_t flags;
	uint16_t idx;
	struct virtq_used_elem ring[];	/* Queue Size */
};

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
	/*
	 * Only used by TX queues:
	 *   0: VirtIO-Net header (12 bytes).
	 *   1: Napatech DVIO0 descriptor (12 bytes).
	 */
	void *avail_struct_phys_addr;
	void *used_struct_phys_addr;
	void *desc_struct_phys_addr;
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
		get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	set_rx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static void dbs_init_tx_queue(nthw_dbs_t *p_nthw_dbs, uint32_t queue, uint32_t start_idx,
	uint32_t start_ptr)
{
	uint32_t busy;
	uint32_t init;
	uint32_t dummy;

	do {
		get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	set_tx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static int nthw_virt_queue_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);

	nthw_fpga_t *const p_fpga = p_fpga_info->mp_fpga;
	nthw_dbs_t *p_nthw_dbs;
	int res = 0;
	uint32_t i;

	p_fpga_info->mp_nthw_dbs = NULL;

	p_nthw_dbs = nthw_dbs_new();

	if (p_nthw_dbs == NULL)
		return -1;

	res = dbs_init(NULL, p_fpga, 0);/* Check that DBS exists in FPGA */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	res = dbs_init(p_nthw_dbs, p_fpga, 0);	/* Create DBS module */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	p_fpga_info->mp_nthw_dbs = p_nthw_dbs;

	for (i = 0; i < MAX_VIRT_QUEUES; ++i) {
		rxvq[i].usage = NTHW_VIRTQ_UNUSED;
		txvq[i].usage = NTHW_VIRTQ_UNUSED;
	}

	dbs_reset(p_nthw_dbs);

	for (i = 0; i < NT_DBS_RX_QUEUES_MAX; ++i)
		dbs_init_rx_queue(p_nthw_dbs, i, 0, 0);

	for (i = 0; i < NT_DBS_TX_QUEUES_MAX; ++i)
		dbs_init_tx_queue(p_nthw_dbs, i, 0, 0);

	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_DISABLE, RX_AM_POLL_SPEED, RX_UW_DISABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_ENABLE);

	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_DISABLE, TX_AM_POLL_SPEED, TX_UW_DISABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
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

static uint16_t dbs_qsize_log2(uint16_t qsize)
{
	uint32_t qs = 0;

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
	int irq_vector)
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
	if (set_rx_dr_data(p_nthw_dbs, index, (uint64_t)desc_struct_phys_addr, host_id, qs, header,
			0) != 0) {
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
	NT_LOG_DBGX(DBG, NTNIC, "set_rx_uw_data int=0 irq_vector=%u", irq_vector);

	if (set_rx_uw_data(p_nthw_dbs, index,
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
		if (set_rx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr,
				RX_AM_DISABLE, host_id, 0,
				irq_vector >= 0 ? 1 : 0) != 0) {
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
	if (set_rx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr, RX_AM_ENABLE,
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
	uint32_t in_order)
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
	if (set_tx_dr_data(p_nthw_dbs, index, (uint64_t)desc_struct_phys_addr, host_id, qs, port,
			header, 0) != 0) {
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

	if (set_tx_uw_data(p_nthw_dbs, index,
			(uint64_t)used_struct_phys_addr,
			host_id, qs, 0, int_enable, vec, istk, in_order) != 0) {
		return NULL;
	}

	/*
	 * 2. Configure the DBS.TX_AM_DATA memory and enable the queues you plan to use;
	 *    good idea to initialize all DBS_TX_QUEUES entries.
	 */
	if (set_tx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr, TX_AM_DISABLE,
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
		if (set_tx_am_data(p_nthw_dbs, index, (uint64_t)avail_struct_phys_addr,
				TX_AM_ENABLE, host_id, 0,
				irq_vector >= 0 ? 1 : 0) != 0) {
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
	int irq_vector)
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
		memcpy(rxvq[index].p_virtual_addr, p_packet_buffers,
			queue_size * sizeof(*p_packet_buffers));
	}

	nthw_setup_rx_virt_queue(p_nthw_dbs, index, 0, 0, (void *)p_virt_struct_area->phys_addr,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.desc_offset,
		(uint16_t)queue_size, host_id, header, SPLIT_RING, irq_vector);

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
	struct nthw_memory_descriptor *p_packet_buffers)
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
		memcpy(txvq[index].p_virtual_addr, p_packet_buffers,
			queue_size * sizeof(*p_packet_buffers));
	}

	nthw_setup_tx_virt_queue(p_nthw_dbs, index, 0, 0, (void *)p_virt_struct_area->phys_addr,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.used_offset,
		(char *)p_virt_struct_area->phys_addr +
		virtq_struct_layout.desc_offset,
		(uint16_t)queue_size, host_id, port, virtual_port, header,
		SPLIT_RING, irq_vector, in_order);

	txvq[index].usage = NTHW_VIRTQ_MANAGED;

	return &txvq[index];
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
	int irq_vector)
{
	switch (vq_type) {
	case SPLIT_RING:
		return nthw_setup_mngd_rx_virt_queue_split(p_nthw_dbs, index, queue_size,
				host_id, header, p_virt_struct_area,
				p_packet_buffers, irq_vector);

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
	uint32_t in_order)
{
	switch (vq_type) {
	case SPLIT_RING:
		return nthw_setup_mngd_tx_virt_queue_split(p_nthw_dbs, index, queue_size,
				host_id, port, virtual_port, header,
				irq_vector, in_order,
				p_virt_struct_area,
				p_packet_buffers);

	default:
		break;
	}

	return NULL;
}

static struct sg_ops_s sg_ops = {
	.nthw_setup_rx_virt_queue = nthw_setup_rx_virt_queue,
	.nthw_setup_tx_virt_queue = nthw_setup_tx_virt_queue,
	.nthw_setup_mngd_rx_virt_queue = nthw_setup_mngd_rx_virt_queue,
	.nthw_setup_mngd_tx_virt_queue = nthw_setup_mngd_tx_virt_queue,
	.nthw_virt_queue_init = nthw_virt_queue_init
};

void sg_init(void)
{
	NT_LOG(INF, NTNIC, "SG ops initialized");
	register_sg_ops(&sg_ops);
}
