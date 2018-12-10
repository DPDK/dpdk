/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <rte_memzone.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>

#include "vnic_dev.h"
#include "vnic_resource.h"
#include "vnic_devcmd.h"
#include "vnic_nic.h"
#include "vnic_stats.h"


enum vnic_proxy_type {
	PROXY_NONE,
	PROXY_BY_BDF,
	PROXY_BY_INDEX,
};

struct vnic_res {
	void __iomem *vaddr;
	dma_addr_t bus_addr;
	unsigned int count;
};

struct vnic_intr_coal_timer_info {
	u32 mul;
	u32 div;
	u32 max_usec;
};

struct vnic_dev {
	void *priv;
	struct rte_pci_device *pdev;
	struct vnic_res res[RES_TYPE_MAX];
	enum vnic_dev_intr_mode intr_mode;
	struct vnic_devcmd __iomem *devcmd;
	struct vnic_devcmd_notify *notify;
	struct vnic_devcmd_notify notify_copy;
	dma_addr_t notify_pa;
	u32 notify_sz;
	dma_addr_t linkstatus_pa;
	struct vnic_stats *stats;
	dma_addr_t stats_pa;
	struct vnic_devcmd_fw_info *fw_info;
	dma_addr_t fw_info_pa;
	enum vnic_proxy_type proxy;
	u32 proxy_index;
	u64 args[VNIC_DEVCMD_NARGS];
	int in_reset;
	struct vnic_intr_coal_timer_info intr_coal_timer_info;
	void *(*alloc_consistent)(void *priv, size_t size,
		dma_addr_t *dma_handle, u8 *name);
	void (*free_consistent)(void *priv,
		size_t size, void *vaddr,
		dma_addr_t dma_handle);
	struct vnic_counter_counts *flow_counters;
	dma_addr_t flow_counters_pa;
	u8 flow_counters_dma_active;
};

#define VNIC_MAX_RES_HDR_SIZE \
	(sizeof(struct vnic_resource_header) + \
	sizeof(struct vnic_resource) * RES_TYPE_MAX)
#define VNIC_RES_STRIDE	128

#define VNIC_MAX_FLOW_COUNTERS 2048

void *vnic_dev_priv(struct vnic_dev *vdev)
{
	return vdev->priv;
}

void vnic_register_cbacks(struct vnic_dev *vdev,
	void *(*alloc_consistent)(void *priv, size_t size,
	    dma_addr_t *dma_handle, u8 *name),
	void (*free_consistent)(void *priv,
	    size_t size, void *vaddr,
	    dma_addr_t dma_handle))
{
	vdev->alloc_consistent = alloc_consistent;
	vdev->free_consistent = free_consistent;
}

static int vnic_dev_discover_res(struct vnic_dev *vdev,
	struct vnic_dev_bar *bar, unsigned int num_bars)
{
	struct vnic_resource_header __iomem *rh;
	struct mgmt_barmap_hdr __iomem *mrh;
	struct vnic_resource __iomem *r;
	u8 type;

	if (num_bars == 0)
		return -EINVAL;

	if (bar->len < VNIC_MAX_RES_HDR_SIZE) {
		pr_err("vNIC BAR0 res hdr length error\n");
		return -EINVAL;
	}

	rh  = bar->vaddr;
	mrh = bar->vaddr;
	if (!rh) {
		pr_err("vNIC BAR0 res hdr not mem-mapped\n");
		return -EINVAL;
	}

	/* Check for mgmt vnic in addition to normal vnic */
	if ((ioread32(&rh->magic) != VNIC_RES_MAGIC) ||
		(ioread32(&rh->version) != VNIC_RES_VERSION)) {
		if ((ioread32(&mrh->magic) != MGMTVNIC_MAGIC) ||
			(ioread32(&mrh->version) != MGMTVNIC_VERSION)) {
			pr_err("vNIC BAR0 res magic/version error " \
				"exp (%lx/%lx) or (%lx/%lx), curr (%x/%x)\n",
				VNIC_RES_MAGIC, VNIC_RES_VERSION,
				MGMTVNIC_MAGIC, MGMTVNIC_VERSION,
				ioread32(&rh->magic), ioread32(&rh->version));
			return -EINVAL;
		}
	}

	if (ioread32(&mrh->magic) == MGMTVNIC_MAGIC)
		r = (struct vnic_resource __iomem *)(mrh + 1);
	else
		r = (struct vnic_resource __iomem *)(rh + 1);


	while ((type = ioread8(&r->type)) != RES_TYPE_EOL) {
		u8 bar_num = ioread8(&r->bar);
		u32 bar_offset = ioread32(&r->bar_offset);
		u32 count = ioread32(&r->count);
		u32 len;

		r++;

		if (bar_num >= num_bars)
			continue;

		if (!bar[bar_num].len || !bar[bar_num].vaddr)
			continue;

		switch (type) {
		case RES_TYPE_WQ:
		case RES_TYPE_RQ:
		case RES_TYPE_CQ:
		case RES_TYPE_INTR_CTRL:
			/* each count is stride bytes long */
			len = count * VNIC_RES_STRIDE;
			if (len + bar_offset > bar[bar_num].len) {
				pr_err("vNIC BAR0 resource %d " \
					"out-of-bounds, offset 0x%x + " \
					"size 0x%x > bar len 0x%lx\n",
					type, bar_offset,
					len,
					bar[bar_num].len);
				return -EINVAL;
			}
			break;
		case RES_TYPE_INTR_PBA_LEGACY:
		case RES_TYPE_DEVCMD:
			len = count;
			break;
		default:
			continue;
		}

		vdev->res[type].count = count;
		vdev->res[type].vaddr = (char __iomem *)bar[bar_num].vaddr +
		    bar_offset;
		vdev->res[type].bus_addr = bar[bar_num].bus_addr + bar_offset;
	}

	return 0;
}

unsigned int vnic_dev_get_res_count(struct vnic_dev *vdev,
	enum vnic_res_type type)
{
	return vdev->res[type].count;
}

void __iomem *vnic_dev_get_res(struct vnic_dev *vdev, enum vnic_res_type type,
	unsigned int index)
{
	if (!vdev->res[type].vaddr)
		return NULL;

	switch (type) {
	case RES_TYPE_WQ:
	case RES_TYPE_RQ:
	case RES_TYPE_CQ:
	case RES_TYPE_INTR_CTRL:
		return (char __iomem *)vdev->res[type].vaddr +
			index * VNIC_RES_STRIDE;
	default:
		return (char __iomem *)vdev->res[type].vaddr;
	}
}

unsigned int vnic_dev_desc_ring_size(struct vnic_dev_ring *ring,
	unsigned int desc_count, unsigned int desc_size)
{
	/* The base address of the desc rings must be 512 byte aligned.
	 * Descriptor count is aligned to groups of 32 descriptors.  A
	 * count of 0 means the maximum 4096 descriptors.  Descriptor
	 * size is aligned to 16 bytes.
	 */

	unsigned int count_align = 32;
	unsigned int desc_align = 16;

	ring->base_align = 512;

	if (desc_count == 0)
		desc_count = 4096;

	ring->desc_count = VNIC_ALIGN(desc_count, count_align);

	ring->desc_size = VNIC_ALIGN(desc_size, desc_align);

	ring->size = ring->desc_count * ring->desc_size;
	ring->size_unaligned = ring->size + ring->base_align;

	return ring->size_unaligned;
}

void vnic_dev_clear_desc_ring(struct vnic_dev_ring *ring)
{
	memset(ring->descs, 0, ring->size);
}

int vnic_dev_alloc_desc_ring(struct vnic_dev *vdev,
	struct vnic_dev_ring *ring,
	unsigned int desc_count, unsigned int desc_size,
	__attribute__((unused)) unsigned int socket_id,
	char *z_name)
{
	void *alloc_addr;
	dma_addr_t alloc_pa = 0;

	vnic_dev_desc_ring_size(ring, desc_count, desc_size);
	alloc_addr = vdev->alloc_consistent(vdev->priv,
					    ring->size_unaligned,
					    &alloc_pa, (u8 *)z_name);
	if (!alloc_addr) {
		pr_err("Failed to allocate ring (size=%d), aborting\n",
			(int)ring->size);
		return -ENOMEM;
	}
	ring->descs_unaligned = alloc_addr;
	if (!alloc_pa) {
		pr_err("Failed to map allocated ring (size=%d), aborting\n",
			(int)ring->size);
		vdev->free_consistent(vdev->priv,
				      ring->size_unaligned,
				      alloc_addr,
				      alloc_pa);
		return -ENOMEM;
	}
	ring->base_addr_unaligned = alloc_pa;

	ring->base_addr = VNIC_ALIGN(ring->base_addr_unaligned,
		ring->base_align);
	ring->descs = (u8 *)ring->descs_unaligned +
	    (ring->base_addr - ring->base_addr_unaligned);

	vnic_dev_clear_desc_ring(ring);

	ring->desc_avail = ring->desc_count - 1;

	return 0;
}

void vnic_dev_free_desc_ring(__attribute__((unused))  struct vnic_dev *vdev,
	struct vnic_dev_ring *ring)
{
	if (ring->descs) {
		vdev->free_consistent(vdev->priv,
				      ring->size_unaligned,
				      ring->descs_unaligned,
				      ring->base_addr_unaligned);
		ring->descs = NULL;
	}
}

static int _vnic_dev_cmd(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
	int wait)
{
	struct vnic_devcmd __iomem *devcmd = vdev->devcmd;
	unsigned int i;
	int delay;
	u32 status;
	int err;

	status = ioread32(&devcmd->status);
	if (status == 0xFFFFFFFF) {
		/* PCI-e target device is gone */
		return -ENODEV;
	}
	if (status & STAT_BUSY) {

		pr_err("Busy devcmd %d\n",  _CMD_N(cmd));
		return -EBUSY;
	}

	if (_CMD_DIR(cmd) & _CMD_DIR_WRITE) {
		for (i = 0; i < VNIC_DEVCMD_NARGS; i++)
			writeq(vdev->args[i], &devcmd->args[i]);
		wmb(); /* complete all writes initiated till now */
	}

	iowrite32(cmd, &devcmd->cmd);

	if ((_CMD_FLAGS(cmd) & _CMD_FLAGS_NOWAIT))
		return 0;

	for (delay = 0; delay < wait; delay++) {

		udelay(100);

		status = ioread32(&devcmd->status);
		if (status == 0xFFFFFFFF) {
			/* PCI-e target device is gone */
			return -ENODEV;
		}

		if (!(status & STAT_BUSY)) {
			if (status & STAT_ERROR) {
				err = -(int)readq(&devcmd->args[0]);
				if (cmd != CMD_CAPABILITY)
					pr_err("Devcmd %d failed " \
						"with error code %d\n",
						_CMD_N(cmd), err);
				return err;
			}

			if (_CMD_DIR(cmd) & _CMD_DIR_READ) {
				rmb();/* finish all reads initiated till now */
				for (i = 0; i < VNIC_DEVCMD_NARGS; i++)
					vdev->args[i] = readq(&devcmd->args[i]);
			}

			return 0;
		}
	}

	pr_err("Timedout devcmd %d\n", _CMD_N(cmd));
	return -ETIMEDOUT;
}

static int vnic_dev_cmd_proxy(struct vnic_dev *vdev,
	enum vnic_devcmd_cmd proxy_cmd, enum vnic_devcmd_cmd cmd,
	u64 *args, int nargs, int wait)
{
	u32 status;
	int err;

	/*
	 * Proxy command consumes 2 arguments. One for proxy index,
	 * the other is for command to be proxied
	 */
	if (nargs > VNIC_DEVCMD_NARGS - 2) {
		pr_err("number of args %d exceeds the maximum\n", nargs);
		return -EINVAL;
	}
	memset(vdev->args, 0, sizeof(vdev->args));

	vdev->args[0] = vdev->proxy_index;
	vdev->args[1] = cmd;
	memcpy(&vdev->args[2], args, nargs * sizeof(args[0]));

	err = _vnic_dev_cmd(vdev, proxy_cmd, wait);
	if (err)
		return err;

	status = (u32)vdev->args[0];
	if (status & STAT_ERROR) {
		err = (int)vdev->args[1];
		if (err != ERR_ECMDUNKNOWN ||
		    cmd != CMD_CAPABILITY)
			pr_err("Error %d proxy devcmd %d\n", err, _CMD_N(cmd));
		return err;
	}

	memcpy(args, &vdev->args[1], nargs * sizeof(args[0]));

	return 0;
}

static int vnic_dev_cmd_no_proxy(struct vnic_dev *vdev,
	enum vnic_devcmd_cmd cmd, u64 *args, int nargs, int wait)
{
	int err;

	if (nargs > VNIC_DEVCMD_NARGS) {
		pr_err("number of args %d exceeds the maximum\n", nargs);
		return -EINVAL;
	}
	memset(vdev->args, 0, sizeof(vdev->args));
	memcpy(vdev->args, args, nargs * sizeof(args[0]));

	err = _vnic_dev_cmd(vdev, cmd, wait);

	memcpy(args, vdev->args, nargs * sizeof(args[0]));

	return err;
}

int vnic_dev_cmd(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
	u64 *a0, u64 *a1, int wait)
{
	u64 args[2];
	int err;

	args[0] = *a0;
	args[1] = *a1;
	memset(vdev->args, 0, sizeof(vdev->args));

	switch (vdev->proxy) {
	case PROXY_BY_INDEX:
		err =  vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_INDEX, cmd,
				args, ARRAY_SIZE(args), wait);
		break;
	case PROXY_BY_BDF:
		err =  vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_BDF, cmd,
				args, ARRAY_SIZE(args), wait);
		break;
	case PROXY_NONE:
	default:
		err = vnic_dev_cmd_no_proxy(vdev, cmd, args, 2, wait);
		break;
	}

	if (err == 0) {
		*a0 = args[0];
		*a1 = args[1];
	}

	return err;
}

int vnic_dev_cmd_args(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
		      u64 *args, int nargs, int wait)
{
	switch (vdev->proxy) {
	case PROXY_BY_INDEX:
		return vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_INDEX, cmd,
				args, nargs, wait);
	case PROXY_BY_BDF:
		return vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_BDF, cmd,
				args, nargs, wait);
	case PROXY_NONE:
	default:
		return vnic_dev_cmd_no_proxy(vdev, cmd, args, nargs, wait);
	}
}

int vnic_dev_fw_info(struct vnic_dev *vdev,
		     struct vnic_devcmd_fw_info **fw_info)
{
	char name[NAME_MAX];
	u64 a0, a1 = 0;
	int wait = 1000;
	int err = 0;
	static u32 instance;

	if (!vdev->fw_info) {
		snprintf((char *)name, sizeof(name), "vnic_fw_info-%u",
			 instance++);
		vdev->fw_info = vdev->alloc_consistent(vdev->priv,
			sizeof(struct vnic_devcmd_fw_info),
			&vdev->fw_info_pa, (u8 *)name);
		if (!vdev->fw_info)
			return -ENOMEM;
		a0 = vdev->fw_info_pa;
		a1 = sizeof(struct vnic_devcmd_fw_info);
		err = vnic_dev_cmd(vdev, CMD_MCPU_FW_INFO,
				   &a0, &a1, wait);
	}
	*fw_info = vdev->fw_info;
	return err;
}

static int vnic_dev_advanced_filters_cap(struct vnic_dev *vdev, u64 *args,
		int nargs)
{
	memset(args, 0, nargs * sizeof(*args));
	args[0] = CMD_ADD_ADV_FILTER;
	args[1] = FILTER_CAP_MODE_V1_FLAG;
	return vnic_dev_cmd_args(vdev, CMD_CAPABILITY, args, nargs, 1000);
}

int vnic_dev_capable_adv_filters(struct vnic_dev *vdev)
{
	u64 a0 = CMD_ADD_ADV_FILTER, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);
	if (err)
		return 0;
	return (a1 >= (u32)FILTER_DPDK_1);
}

/*  Determine the "best" filtering mode VIC is capaible of. Returns one of 3
 *  value or 0 on error:
 *	FILTER_DPDK_1- advanced filters availabile
 *	FILTER_USNIC_IP_FLAG - advanced filters but with the restriction that
 *		the IP layer must explicitly specified. I.e. cannot have a UDP
 *		filter that matches both IPv4 and IPv6.
 *	FILTER_IPV4_5TUPLE - fallback if either of the 2 above aren't available.
 *		all other filter types are not available.
 *   Retrun true in filter_tags if supported
 */
int vnic_dev_capable_filter_mode(struct vnic_dev *vdev, u32 *mode,
				 u8 *filter_actions)
{
	u64 args[4];
	int err;
	u32 max_level = 0;

	err = vnic_dev_advanced_filters_cap(vdev, args, 4);

	/* determine supported filter actions */
	*filter_actions = FILTER_ACTION_RQ_STEERING_FLAG; /* always available */
	if (args[2] == FILTER_CAP_MODE_V1)
		*filter_actions = args[3];

	if (err || ((args[0] == 1) && (args[1] == 0))) {
		/* Adv filter Command not supported or adv filters available but
		 * not enabled. Try the normal filter capability command.
		 */
		args[0] = CMD_ADD_FILTER;
		args[1] = 0;
		err = vnic_dev_cmd_args(vdev, CMD_CAPABILITY, args, 2, 1000);
		if (err)
			return err;
		max_level = args[1];
		goto parse_max_level;
	} else if (args[2] == FILTER_CAP_MODE_V1) {
		/* parse filter capability mask in args[1] */
		if (args[1] & FILTER_DPDK_1_FLAG)
			*mode = FILTER_DPDK_1;
		else if (args[1] & FILTER_USNIC_IP_FLAG)
			*mode = FILTER_USNIC_IP;
		else if (args[1] & FILTER_IPV4_5TUPLE_FLAG)
			*mode = FILTER_IPV4_5TUPLE;
		return 0;
	}
	max_level = args[1];
parse_max_level:
	if (max_level >= (u32)FILTER_USNIC_IP)
		*mode = FILTER_USNIC_IP;
	else
		*mode = FILTER_IPV4_5TUPLE;
	return 0;
}

void vnic_dev_capable_udp_rss_weak(struct vnic_dev *vdev, bool *cfg_chk,
				   bool *weak)
{
	u64 a0 = CMD_NIC_CFG, a1 = 0;
	int wait = 1000;
	int err;

	*cfg_chk = false;
	*weak = false;
	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);
	if (err == 0 && a0 != 0 && a1 != 0) {
		*cfg_chk = true;
		*weak = !!((a1 >> 32) & CMD_NIC_CFG_CAPF_UDP_WEAK);
	}
}

int vnic_dev_capable(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd)
{
	u64 a0 = (u32)cmd, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);

	return !(err || a0);
}

int vnic_dev_spec(struct vnic_dev *vdev, unsigned int offset, size_t size,
	void *value)
{
	u64 a0, a1;
	int wait = 1000;
	int err;

	a0 = offset;
	a1 = size;

	err = vnic_dev_cmd(vdev, CMD_DEV_SPEC, &a0, &a1, wait);

	switch (size) {
	case 1:
		*(u8 *)value = (u8)a0;
		break;
	case 2:
		*(u16 *)value = (u16)a0;
		break;
	case 4:
		*(u32 *)value = (u32)a0;
		break;
	case 8:
		*(u64 *)value = a0;
		break;
	default:
		BUG();
		break;
	}

	return err;
}

int vnic_dev_stats_clear(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_STATS_CLEAR, &a0, &a1, wait);
}

int vnic_dev_stats_dump(struct vnic_dev *vdev, struct vnic_stats **stats)
{
	u64 a0, a1;
	int wait = 1000;

	if (!vdev->stats)
		return -ENOMEM;

	*stats = vdev->stats;
	a0 = vdev->stats_pa;
	a1 = sizeof(struct vnic_stats);

	return vnic_dev_cmd(vdev, CMD_STATS_DUMP, &a0, &a1, wait);
}

/*
 * Configure counter DMA
 */
int vnic_dev_counter_dma_cfg(struct vnic_dev *vdev, u32 period,
			     u32 num_counters)
{
	u64 args[3];
	int wait = 1000;
	int err;

	if (num_counters > VNIC_MAX_FLOW_COUNTERS)
		return -ENOMEM;
	if (period > 0 && (period < VNIC_COUNTER_DMA_MIN_PERIOD ||
	    num_counters == 0))
		return -EINVAL;

	args[0] = num_counters;
	args[1] = vdev->flow_counters_pa;
	args[2] = period;
	err =  vnic_dev_cmd_args(vdev, CMD_COUNTER_DMA_CONFIG, args, 3, wait);

	/* record if DMAs need to be stopped on close */
	if (!err)
		vdev->flow_counters_dma_active = (num_counters != 0 &&
						  period != 0);

	return err;
}

int vnic_dev_close(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_CLOSE, &a0, &a1, wait);
}

int vnic_dev_enable_wait(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	if (vnic_dev_capable(vdev, CMD_ENABLE_WAIT))
		return vnic_dev_cmd(vdev, CMD_ENABLE_WAIT, &a0, &a1, wait);
	else
		return vnic_dev_cmd(vdev, CMD_ENABLE, &a0, &a1, wait);
}

int vnic_dev_disable(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_DISABLE, &a0, &a1, wait);
}

int vnic_dev_open(struct vnic_dev *vdev, int arg)
{
	u64 a0 = (u32)arg, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OPEN, &a0, &a1, wait);
}

int vnic_dev_open_done(struct vnic_dev *vdev, int *done)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;

	*done = 0;

	err = vnic_dev_cmd(vdev, CMD_OPEN_STATUS, &a0, &a1, wait);
	if (err)
		return err;

	*done = (a0 == 0);

	return 0;
}

int vnic_dev_get_mac_addr(struct vnic_dev *vdev, u8 *mac_addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err, i;

	for (i = 0; i < ETH_ALEN; i++)
		mac_addr[i] = 0;

	err = vnic_dev_cmd(vdev, CMD_GET_MAC_ADDR, &a0, &a1, wait);
	if (err)
		return err;

	for (i = 0; i < ETH_ALEN; i++)
		mac_addr[i] = ((u8 *)&a0)[i];

	return 0;
}

int vnic_dev_packet_filter(struct vnic_dev *vdev, int directed, int multicast,
	int broadcast, int promisc, int allmulti)
{
	u64 a0, a1 = 0;
	int wait = 1000;
	int err;

	a0 = (directed ? CMD_PFILTER_DIRECTED : 0) |
	     (multicast ? CMD_PFILTER_MULTICAST : 0) |
	     (broadcast ? CMD_PFILTER_BROADCAST : 0) |
	     (promisc ? CMD_PFILTER_PROMISCUOUS : 0) |
	     (allmulti ? CMD_PFILTER_ALL_MULTICAST : 0);

	err = vnic_dev_cmd(vdev, CMD_PACKET_FILTER, &a0, &a1, wait);
	if (err)
		pr_err("Can't set packet filter\n");

	return err;
}

int vnic_dev_add_addr(struct vnic_dev *vdev, u8 *addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		((u8 *)&a0)[i] = addr[i];

	err = vnic_dev_cmd(vdev, CMD_ADDR_ADD, &a0, &a1, wait);
	if (err)
		pr_err("Can't add addr [%02x:%02x:%02x:%02x:%02x:%02x], %d\n",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
			err);

	return err;
}

int vnic_dev_del_addr(struct vnic_dev *vdev, u8 *addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		((u8 *)&a0)[i] = addr[i];

	err = vnic_dev_cmd(vdev, CMD_ADDR_DEL, &a0, &a1, wait);
	if (err)
		pr_err("Can't del addr [%02x:%02x:%02x:%02x:%02x:%02x], %d\n",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
			err);

	return err;
}

int vnic_dev_set_ig_vlan_rewrite_mode(struct vnic_dev *vdev,
	u8 ig_vlan_rewrite_mode)
{
	u64 a0 = ig_vlan_rewrite_mode, a1 = 0;
	int wait = 1000;

	if (vnic_dev_capable(vdev, CMD_IG_VLAN_REWRITE_MODE))
		return vnic_dev_cmd(vdev, CMD_IG_VLAN_REWRITE_MODE,
				&a0, &a1, wait);
	else
		return 0;
}

void vnic_dev_set_reset_flag(struct vnic_dev *vdev, int state)
{
	vdev->in_reset = state;
}

static inline int vnic_dev_in_reset(struct vnic_dev *vdev)
{
	return vdev->in_reset;
}

int vnic_dev_notify_setcmd(struct vnic_dev *vdev,
	void *notify_addr, dma_addr_t notify_pa, u16 intr)
{
	u64 a0, a1;
	int wait = 1000;
	int r;

	memset(notify_addr, 0, sizeof(struct vnic_devcmd_notify));
	if (!vnic_dev_in_reset(vdev)) {
		vdev->notify = notify_addr;
		vdev->notify_pa = notify_pa;
	}

	a0 = (u64)notify_pa;
	a1 = ((u64)intr << 32) & 0x0000ffff00000000ULL;
	a1 += sizeof(struct vnic_devcmd_notify);

	r = vnic_dev_cmd(vdev, CMD_NOTIFY, &a0, &a1, wait);
	if (!vnic_dev_in_reset(vdev))
		vdev->notify_sz = (r == 0) ? (u32)a1 : 0;

	return r;
}

int vnic_dev_notify_set(struct vnic_dev *vdev, u16 intr)
{
	void *notify_addr = NULL;
	dma_addr_t notify_pa = 0;
	char name[NAME_MAX];
	static u32 instance;

	if (vdev->notify || vdev->notify_pa) {
		return vnic_dev_notify_setcmd(vdev, vdev->notify,
					      vdev->notify_pa, intr);
	}
	if (!vnic_dev_in_reset(vdev)) {
		snprintf((char *)name, sizeof(name),
			"vnic_notify-%u", instance++);
		notify_addr = vdev->alloc_consistent(vdev->priv,
			sizeof(struct vnic_devcmd_notify),
			&notify_pa, (u8 *)name);
		if (!notify_addr)
			return -ENOMEM;
	}

	return vnic_dev_notify_setcmd(vdev, notify_addr, notify_pa, intr);
}

int vnic_dev_notify_unsetcmd(struct vnic_dev *vdev)
{
	u64 a0, a1;
	int wait = 1000;
	int err;

	a0 = 0;  /* paddr = 0 to unset notify buffer */
	a1 = 0x0000ffff00000000ULL; /* intr num = -1 to unreg for intr */
	a1 += sizeof(struct vnic_devcmd_notify);

	err = vnic_dev_cmd(vdev, CMD_NOTIFY, &a0, &a1, wait);
	if (!vnic_dev_in_reset(vdev)) {
		vdev->notify = NULL;
		vdev->notify_pa = 0;
		vdev->notify_sz = 0;
	}

	return err;
}

int vnic_dev_notify_unset(struct vnic_dev *vdev)
{
	if (vdev->notify && !vnic_dev_in_reset(vdev)) {
		vdev->free_consistent(vdev->priv,
			sizeof(struct vnic_devcmd_notify),
			vdev->notify,
			vdev->notify_pa);
	}

	return vnic_dev_notify_unsetcmd(vdev);
}

static int vnic_dev_notify_ready(struct vnic_dev *vdev)
{
	u32 *words;
	unsigned int nwords = vdev->notify_sz / 4;
	unsigned int i;
	u32 csum;

	if (!vdev->notify || !vdev->notify_sz)
		return 0;

	do {
		csum = 0;
		rte_memcpy(&vdev->notify_copy, vdev->notify, vdev->notify_sz);
		words = (u32 *)&vdev->notify_copy;
		for (i = 1; i < nwords; i++)
			csum += words[i];
	} while (csum != words[0]);

	return 1;
}

int vnic_dev_init(struct vnic_dev *vdev, int arg)
{
	u64 a0 = (u32)arg, a1 = 0;
	int wait = 1000;
	int r = 0;

	if (vnic_dev_capable(vdev, CMD_INIT))
		r = vnic_dev_cmd(vdev, CMD_INIT, &a0, &a1, wait);
	else {
		vnic_dev_cmd(vdev, CMD_INIT_v1, &a0, &a1, wait);
		if (a0 & CMD_INITF_DEFAULT_MAC) {
			/* Emulate these for old CMD_INIT_v1 which
			 * didn't pass a0 so no CMD_INITF_*.
			 */
			vnic_dev_cmd(vdev, CMD_GET_MAC_ADDR, &a0, &a1, wait);
			vnic_dev_cmd(vdev, CMD_ADDR_ADD, &a0, &a1, wait);
		}
	}
	return r;
}

void vnic_dev_intr_coal_timer_info_default(struct vnic_dev *vdev)
{
	/* Default: hardware intr coal timer is in units of 1.5 usecs */
	vdev->intr_coal_timer_info.mul = 2;
	vdev->intr_coal_timer_info.div = 3;
	vdev->intr_coal_timer_info.max_usec =
		vnic_dev_intr_coal_timer_hw_to_usec(vdev, 0xffff);
}

int vnic_dev_link_status(struct vnic_dev *vdev)
{
	if (!vnic_dev_notify_ready(vdev))
		return 0;

	return vdev->notify_copy.link_state;
}

u32 vnic_dev_port_speed(struct vnic_dev *vdev)
{
	if (!vnic_dev_notify_ready(vdev))
		return 0;

	return vdev->notify_copy.port_speed;
}

u32 vnic_dev_intr_coal_timer_usec_to_hw(struct vnic_dev *vdev, u32 usec)
{
	return (usec * vdev->intr_coal_timer_info.mul) /
		vdev->intr_coal_timer_info.div;
}

u32 vnic_dev_intr_coal_timer_hw_to_usec(struct vnic_dev *vdev, u32 hw_cycles)
{
	return (hw_cycles * vdev->intr_coal_timer_info.div) /
		vdev->intr_coal_timer_info.mul;
}

u32 vnic_dev_get_intr_coal_timer_max(struct vnic_dev *vdev)
{
	return vdev->intr_coal_timer_info.max_usec;
}

int vnic_dev_alloc_stats_mem(struct vnic_dev *vdev)
{
	char name[NAME_MAX];
	static u32 instance;

	snprintf((char *)name, sizeof(name), "vnic_stats-%u", instance++);
	vdev->stats = vdev->alloc_consistent(vdev->priv,
					     sizeof(struct vnic_stats),
					     &vdev->stats_pa, (u8 *)name);
	return vdev->stats == NULL ? -ENOMEM : 0;
}

/*
 * Initialize for up to VNIC_MAX_FLOW_COUNTERS
 */
int vnic_dev_alloc_counter_mem(struct vnic_dev *vdev)
{
	char name[NAME_MAX];
	static u32 instance;

	snprintf((char *)name, sizeof(name), "vnic_flow_ctrs-%u", instance++);
	vdev->flow_counters = vdev->alloc_consistent(vdev->priv,
					     sizeof(struct vnic_counter_counts)
					     * VNIC_MAX_FLOW_COUNTERS,
					     &vdev->flow_counters_pa,
					     (u8 *)name);
	vdev->flow_counters_dma_active = 0;
	return vdev->flow_counters == NULL ? -ENOMEM : 0;
}

void vnic_dev_unregister(struct vnic_dev *vdev)
{
	if (vdev) {
		if (vdev->notify)
			vdev->free_consistent(vdev->priv,
				sizeof(struct vnic_devcmd_notify),
				vdev->notify,
				vdev->notify_pa);
		if (vdev->stats)
			vdev->free_consistent(vdev->priv,
				sizeof(struct vnic_stats),
				vdev->stats, vdev->stats_pa);
		if (vdev->flow_counters) {
			/* turn off counter DMAs before freeing memory */
			if (vdev->flow_counters_dma_active)
				vnic_dev_counter_dma_cfg(vdev, 0, 0);

			vdev->free_consistent(vdev->priv,
				sizeof(struct vnic_counter_counts)
				* VNIC_MAX_FLOW_COUNTERS,
				vdev->flow_counters, vdev->flow_counters_pa);
		}
		if (vdev->fw_info)
			vdev->free_consistent(vdev->priv,
				sizeof(struct vnic_devcmd_fw_info),
				vdev->fw_info, vdev->fw_info_pa);
		rte_free(vdev);
	}
}

struct vnic_dev *vnic_dev_register(struct vnic_dev *vdev,
	void *priv, struct rte_pci_device *pdev, struct vnic_dev_bar *bar,
	unsigned int num_bars)
{
	if (!vdev) {
		char name[NAME_MAX];
		snprintf((char *)name, sizeof(name), "%s-vnic",
			  pdev->device.name);
		vdev = (struct vnic_dev *)rte_zmalloc_socket(name,
					sizeof(struct vnic_dev),
					RTE_CACHE_LINE_SIZE,
					pdev->device.numa_node);
		if (!vdev)
			return NULL;
	}

	vdev->priv = priv;
	vdev->pdev = pdev;

	if (vnic_dev_discover_res(vdev, bar, num_bars))
		goto err_out;

	vdev->devcmd = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD, 0);
	if (!vdev->devcmd)
		goto err_out;

	return vdev;

err_out:
	vnic_dev_unregister(vdev);
	return NULL;
}

/*
 *  vnic_dev_classifier: Add/Delete classifier entries
 *  @vdev: vdev of the device
 *  @cmd: CLSF_ADD for Add filter
 *        CLSF_DEL for Delete filter
 *  @entry: In case of ADD filter, the caller passes the RQ number in this
 *          variable.
 *          This function stores the filter_id returned by the
 *          firmware in the same variable before return;
 *
 *          In case of DEL filter, the caller passes the RQ number. Return
 *          value is irrelevant.
 * @data: filter data
 * @action: action data
 */
int vnic_dev_classifier(struct vnic_dev *vdev, u8 cmd, u16 *entry,
	struct filter_v2 *data, struct filter_action_v2 *action_v2)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	dma_addr_t tlv_pa;
	int ret = -EINVAL;
	struct filter_tlv *tlv, *tlv_va;
	u64 tlv_size;
	u32 filter_size, action_size;
	static unsigned int unique_id;
	char z_name[RTE_MEMZONE_NAMESIZE];
	enum vnic_devcmd_cmd dev_cmd;

	if (cmd == CLSF_ADD) {
		dev_cmd = (data->type >= FILTER_DPDK_1) ?
			  CMD_ADD_ADV_FILTER : CMD_ADD_FILTER;

		filter_size = vnic_filter_size(data);
		action_size = vnic_action_size(action_v2);

		tlv_size = filter_size + action_size +
		    2*sizeof(struct filter_tlv);
		snprintf((char *)z_name, sizeof(z_name),
			"vnic_clsf_%u", unique_id++);
		tlv_va = vdev->alloc_consistent(vdev->priv,
			tlv_size, &tlv_pa, (u8 *)z_name);
		if (!tlv_va)
			return -ENOMEM;
		tlv = tlv_va;
		a0 = tlv_pa;
		a1 = tlv_size;
		memset(tlv, 0, tlv_size);
		tlv->type = CLSF_TLV_FILTER;
		tlv->length = filter_size;
		memcpy(&tlv->val, (void *)data, filter_size);

		tlv = (struct filter_tlv *)((char *)tlv +
					 sizeof(struct filter_tlv) +
					 filter_size);

		tlv->type = CLSF_TLV_ACTION;
		tlv->length = action_size;
		memcpy(&tlv->val, (void *)action_v2, action_size);
		ret = vnic_dev_cmd(vdev, dev_cmd, &a0, &a1, wait);
		*entry = (u16)a0;
		vdev->free_consistent(vdev->priv, tlv_size, tlv_va, tlv_pa);
	} else if (cmd == CLSF_DEL) {
		a0 = *entry;
		ret = vnic_dev_cmd(vdev, CMD_DEL_FILTER, &a0, &a1, wait);
	}

	return ret;
}

int vnic_dev_overlay_offload_ctrl(struct vnic_dev *vdev, u8 overlay, u8 config)
{
	u64 a0 = overlay;
	u64 a1 = config;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OVERLAY_OFFLOAD_CTRL, &a0, &a1, wait);
}

int vnic_dev_overlay_offload_cfg(struct vnic_dev *vdev, u8 overlay,
				 u16 vxlan_udp_port_number)
{
	u64 a1 = vxlan_udp_port_number;
	u64 a0 = overlay;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OVERLAY_OFFLOAD_CFG, &a0, &a1, wait);
}

int vnic_dev_capable_vxlan(struct vnic_dev *vdev)
{
	u64 a0 = VIC_FEATURE_VXLAN;
	u64 a1 = 0;
	int wait = 1000;
	int ret;

	ret = vnic_dev_cmd(vdev, CMD_GET_SUPP_FEATURE_VER, &a0, &a1, wait);
	/* 1 if the NIC can do VXLAN for both IPv4 and IPv6 with multiple WQs */
	return ret == 0 &&
		(a1 & (FEATURE_VXLAN_IPV6 | FEATURE_VXLAN_MULTI_WQ)) ==
		(FEATURE_VXLAN_IPV6 | FEATURE_VXLAN_MULTI_WQ);
}

bool vnic_dev_counter_alloc(struct vnic_dev *vdev, uint32_t *idx)
{
	u64 a0 = 0;
	u64 a1 = 0;
	int wait = 1000;

	if (vnic_dev_cmd(vdev, CMD_COUNTER_ALLOC, &a0, &a1, wait))
		return false;
	*idx = (uint32_t)a0;
	return true;
}

bool vnic_dev_counter_free(struct vnic_dev *vdev, uint32_t idx)
{
	u64 a0 = idx;
	u64 a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_COUNTER_FREE, &a0, &a1,
			    wait) == 0;
}

bool vnic_dev_counter_query(struct vnic_dev *vdev, uint32_t idx,
			    bool reset, uint64_t *packets, uint64_t *bytes)
{
	u64 a0 = idx;
	u64 a1 = reset ? 1 : 0;
	int wait = 1000;

	if (reset) {
		/* query/reset returns updated counters */
		if (vnic_dev_cmd(vdev, CMD_COUNTER_QUERY, &a0, &a1, wait))
			return false;
		*packets = a0;
		*bytes = a1;
	} else {
		/* Get values DMA'd from the adapter */
		*packets = vdev->flow_counters[idx].vcc_packets;
		*bytes = vdev->flow_counters[idx].vcc_bytes;
	}
	return true;
}
