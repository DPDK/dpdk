/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * nfp_cpp_pcie_ops.c
 * Authors: Vinayak Tammineedi <vinayak.tammineedi@netronome.com>
 *
 * Multiplexes the NFP BARs between NFP internal resources and
 * implements the PCIe specific interface for generic CPP bus access.
 *
 * The BARs are managed and allocated if they are available.
 * The generic CPP bus abstraction builds upon this BAR interface.
 */

#include "nfp6000_pcie.h"

#include <unistd.h>
#include <fcntl.h>

#include <rte_io.h>

#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp_target.h"
#include "nfp6000/nfp6000.h"
#include "../nfp_logs.h"

#define NFP_PCIE_BAR(_pf)        (0x30000 + ((_pf) & 7) * 0xc0)

#define NFP_PCIE_BAR_PCIE2CPP_ACTION_BASEADDRESS(_x)  (((_x) & 0x1f) << 16)
#define NFP_PCIE_BAR_PCIE2CPP_ACTION_BASEADDRESS_OF(_x) (((_x) >> 16) & 0x1f)
#define NFP_PCIE_BAR_PCIE2CPP_BASEADDRESS(_x)         (((_x) & 0xffff) << 0)
#define NFP_PCIE_BAR_PCIE2CPP_BASEADDRESS_OF(_x)      (((_x) >> 0) & 0xffff)
#define NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT(_x)        (((_x) & 0x3) << 27)
#define NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_OF(_x)     (((_x) >> 27) & 0x3)
#define NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_32BIT    0
#define NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_64BIT    1
#define NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_0BYTE    3
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE(_x)             (((_x) & 0x7) << 29)
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_OF(_x)          (((_x) >> 29) & 0x7)
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_FIXED         0
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_BULK          1
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_TARGET        2
#define NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_GENERAL       3
#define NFP_PCIE_BAR_PCIE2CPP_TARGET_BASEADDRESS(_x)  (((_x) & 0xf) << 23)
#define NFP_PCIE_BAR_PCIE2CPP_TARGET_BASEADDRESS_OF(_x) (((_x) >> 23) & 0xf)
#define NFP_PCIE_BAR_PCIE2CPP_TOKEN_BASEADDRESS(_x)   (((_x) & 0x3) << 21)
#define NFP_PCIE_BAR_PCIE2CPP_TOKEN_BASEADDRESS_OF(_x) (((_x) >> 21) & 0x3)

/*
 * Minimal size of the PCIe cfg memory we depend on being mapped,
 * queue controller and DMA controller don't have to be covered.
 */
#define NFP_PCI_MIN_MAP_SIZE        0x080000        /* 512K */

#define NFP_PCIE_P2C_FIXED_SIZE(bar)               (1 << (bar)->bitsize)
#define NFP_PCIE_P2C_BULK_SIZE(bar)                (1 << (bar)->bitsize)
#define NFP_PCIE_P2C_GENERAL_TARGET_OFFSET(bar, x) ((x) << ((bar)->bitsize - 2))
#define NFP_PCIE_P2C_GENERAL_TOKEN_OFFSET(bar, x) ((x) << ((bar)->bitsize - 4))
#define NFP_PCIE_P2C_GENERAL_SIZE(bar)             (1 << ((bar)->bitsize - 4))

#define NFP_PCIE_P2C_EXPBAR_OFFSET(bar_index)      ((bar_index) * 4)

struct nfp_pcie_user;
struct nfp6000_area_priv;

/* Describes BAR configuration and usage */
struct nfp_bar {
	struct nfp_pcie_user *nfp;    /**< Backlink to owner */
	uint32_t barcfg;     /**< BAR config CSR */
	uint64_t base;       /**< Base CPP offset */
	uint64_t mask;       /**< Mask of the BAR aperture (read only) */
	uint32_t bitsize;    /**< Bit size of the BAR aperture (read only) */
	uint32_t index;      /**< Index of the BAR */
	bool lock;           /**< If the BAR has been locked */

	char *iomem;         /**< mapped IO memory */
	struct rte_mem_resource *resource;    /**< IOMEM resource window */
};

#define NFP_PCI_BAR_MAX    (PCI_64BIT_BAR_COUNT * 8)

struct nfp_pcie_user {
	struct rte_pci_device *pci_dev;
	const struct nfp_dev_info *dev_info;

	int lock;

	/* PCI BAR management */
	uint32_t bars;
	struct nfp_bar bar[NFP_PCI_BAR_MAX];

	/* Reserved BAR access */
	char *csr;
};

/* Generic CPP bus access interface. */
struct nfp6000_area_priv {
	struct nfp_bar *bar;
	uint32_t bar_offset;

	int target;
	int action;
	int token;
	uint64_t offset;
	struct {
		int read;
		int write;
		int bar;
	} width;
	size_t size;
	char *iomem;
};

static uint32_t
nfp_bar_maptype(struct nfp_bar *bar)
{
	return NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_OF(bar->barcfg);
}

#define TARGET_WIDTH_32    4
#define TARGET_WIDTH_64    8

static int
nfp_compute_bar(const struct nfp_bar *bar,
		uint32_t *bar_config,
		uint64_t *bar_base,
		int target,
		int action,
		int token,
		uint64_t offset,
		size_t size,
		int width)
{
	uint64_t mask;
	uint32_t newcfg;
	uint32_t bitsize;

	if (target >= NFP_CPP_NUM_TARGETS)
		return -EINVAL;

	switch (width) {
	case 8:
		newcfg = NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT
				(NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_64BIT);
		break;
	case 4:
		newcfg = NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT
				(NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_32BIT);
		break;
	case 0:
		newcfg = NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT
				(NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_0BYTE);
		break;
	default:
		return -EINVAL;
	}

	if (action != NFP_CPP_ACTION_RW && action != 0) {
		/* Fixed CPP mapping with specific action */
		mask = ~(NFP_PCIE_P2C_FIXED_SIZE(bar) - 1);

		newcfg |= NFP_PCIE_BAR_PCIE2CPP_MAPTYPE
				(NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_FIXED);
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_TARGET_BASEADDRESS(target);
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_ACTION_BASEADDRESS(action);
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_TOKEN_BASEADDRESS(token);

		if ((offset & mask) != ((offset + size - 1) & mask))
			return -EINVAL;

		offset &= mask;
		bitsize = 40 - 16;
	} else {
		mask = ~(NFP_PCIE_P2C_BULK_SIZE(bar) - 1);

		/* Bulk mapping */
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_MAPTYPE
				(NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_BULK);
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_TARGET_BASEADDRESS(target);
		newcfg |= NFP_PCIE_BAR_PCIE2CPP_TOKEN_BASEADDRESS(token);

		if ((offset & mask) != ((offset + size - 1) & mask))
			return -EINVAL;

		offset &= mask;
		bitsize = 40 - 21;
	}
	newcfg |= offset >> bitsize;

	if (bar_base != NULL)
		*bar_base = offset;

	if (bar_config != NULL)
		*bar_config = newcfg;

	return 0;
}

static int
nfp_bar_write(struct nfp_pcie_user *nfp,
		struct nfp_bar *bar,
		uint32_t newcfg)
{
	uint32_t xbar;

	xbar = NFP_PCIE_P2C_EXPBAR_OFFSET(bar->index);

	if (nfp->csr != NULL) {
		rte_write32(newcfg, nfp->csr + xbar);
		/* Readback to ensure BAR is flushed */
		rte_read32(nfp->csr + xbar);
	} else {
		xbar += nfp->dev_info->pcie_cfg_expbar_offset;
		if (rte_pci_write_config(nfp->pci_dev, &newcfg, sizeof(uint32_t),
				xbar) < 0)
			return -EIO;
	}

	bar->barcfg = newcfg;

	return 0;
}

static int
nfp_reconfigure_bar(struct nfp_pcie_user *nfp,
		struct nfp_bar *bar,
		int target,
		int action,
		int token,
		uint64_t offset,
		size_t size,
		int width)
{
	int err;
	uint32_t newcfg;
	uint64_t newbase;

	err = nfp_compute_bar(bar, &newcfg, &newbase, target, action,
			token, offset, size, width);
	if (err != 0)
		return err;

	bar->base = newbase;

	return nfp_bar_write(nfp, bar, newcfg);
}

static uint32_t
nfp_bitsize_calc(uint64_t mask)
{
	uint64_t tmp = mask;
	uint32_t bit_size = 0;

	if (tmp == 0)
		return 0;

	for (; tmp != 0; tmp >>= 1)
		bit_size++;

	return bit_size;
}

static int
nfp_cmp_bars(const void *ptr_a,
		const void *ptr_b)
{
	const struct nfp_bar *a = ptr_a;
	const struct nfp_bar *b = ptr_b;

	if (a->bitsize == b->bitsize)
		return a->index - b->index;
	else
		return a->bitsize - b->bitsize;
}

static bool
nfp_bars_for_secondary(uint32_t index)
{
	uint8_t tmp = index & 0x07;

	if (tmp == 0x06 || tmp == 0x07)
		return true;
	else
		return false;
}

/**
 * Map all PCI bars and fetch the actual BAR configurations from the board.
 * We assume that the BAR with the PCIe config block is already mapped.
 *
 * BAR0.0: Reserved for General Mapping (for MSI-X access to PCIe SRAM)
 * BAR0.1: --
 * BAR0.2: --
 * BAR0.3: --
 * BAR0.4: --
 * BAR0.5: --
 * BAR0.6: --
 * BAR0.7: --
 *
 * BAR1.0-BAR1.7: --
 * BAR2.0-BAR2.7: --
 */
static int
nfp_enable_bars(struct nfp_pcie_user *nfp)
{
	int pf;
	uint32_t i;
	uint8_t min_bars;
	struct nfp_bar *bar;
	enum rte_proc_type_t type;
	struct rte_mem_resource *res;
	const uint32_t barcfg_msix_general = NFP_PCIE_BAR_PCIE2CPP_MAPTYPE
			(NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_GENERAL) |
			NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_32BIT;

	type = rte_eal_process_type();
	if (type == RTE_PROC_PRIMARY)
		min_bars = 12;
	else
		min_bars = 4;

	for (i = 0; i < RTE_DIM(nfp->bar); i++) {
		if (i != 0) {
			if (type == RTE_PROC_PRIMARY) {
				if (nfp_bars_for_secondary(i))
					continue;
			} else {
				if (!nfp_bars_for_secondary(i))
					continue;
			}
		}

		/* 24 NFP bars mapping into BAR0, BAR2 and BAR4 */
		res = &nfp->pci_dev->mem_resource[(i >> 3) * 2];

		/* Skip over BARs that are not mapped */
		if (res->addr != NULL) {
			bar = &nfp->bar[i];
			bar->resource = res;
			bar->barcfg = 0;

			bar->nfp = nfp;
			bar->index = i;
			/* The resource shared by 8 bars */
			bar->mask = (res->len >> 3) - 1;
			bar->bitsize = nfp_bitsize_calc(bar->mask);
			bar->base = 0;
			bar->lock = false;
			bar->iomem = (char *)res->addr +
					((bar->index & 7) << bar->bitsize);

			nfp->bars++;
		}
	}

	if (nfp->bars < min_bars) {
		PMD_DRV_LOG(ERR, "Not enough usable BARs found.");
		return -EINVAL;
	}

	switch (nfp->pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
		pf = nfp->pci_dev->addr.function & 0x07;
		nfp->csr = nfp->bar[0].iomem + NFP_PCIE_BAR(pf);
		break;
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
		nfp->csr = nfp->bar[0].iomem + NFP_PCIE_BAR(0);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported device ID: %04hx!",
				nfp->pci_dev->id.device_id);
		return -EINVAL;
	}

	/* Configure, and lock, BAR0.0 for General Target use (MSI-X SRAM) */
	bar = &nfp->bar[0];
	bar->lock = true;

	if (nfp_bar_write(nfp, bar, barcfg_msix_general) < 0)
		return -EIO;

	/* Sort bars by bit size - use the smallest possible first. */
	qsort(&nfp->bar[0], nfp->bars, sizeof(nfp->bar[0]), nfp_cmp_bars);

	return 0;
}

/* Check if BAR can be used with the given parameters. */
static bool
matching_bar_exist(struct nfp_bar *bar,
		int target,
		int action,
		int token,
		uint64_t offset,
		size_t size,
		int width)
{
	int bar_width;
	int bar_token;
	int bar_target;
	int bar_action;
	uint32_t map_type;

	bar_width = NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_OF(bar->barcfg);
	switch (bar_width) {
	case NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_32BIT:
		bar_width = 4;
		break;
	case NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_64BIT:
		bar_width = 8;
		break;
	case NFP_PCIE_BAR_PCIE2CPP_LENGTHSELECT_0BYTE:
		bar_width = 0;
		break;
	default:
		bar_width = -1;
		break;
	}

	/* Make sure to match up the width */
	if (bar_width != width)
		return false;

	bar_token = NFP_PCIE_BAR_PCIE2CPP_TOKEN_BASEADDRESS_OF(bar->barcfg);
	bar_action = NFP_PCIE_BAR_PCIE2CPP_ACTION_BASEADDRESS_OF(bar->barcfg);
	map_type = NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_OF(bar->barcfg);
	switch (map_type) {
	case NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_TARGET:
		bar_token = -1;
		/* FALLTHROUGH */
	case NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_BULK:
		bar_action = NFP_CPP_ACTION_RW;
		if (action == 0)
			action = NFP_CPP_ACTION_RW;
		/* FALLTHROUGH */
	case NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_FIXED:
		break;
	default:
		/* We don't match explicit bars through the area interface */
		return false;
	}

	bar_target = NFP_PCIE_BAR_PCIE2CPP_TARGET_BASEADDRESS_OF(bar->barcfg);
	if ((bar_target < 0 || bar_target == target) &&
			(bar_token < 0 || bar_token == token) &&
			bar_action == action &&
			bar->base <= offset &&
			(bar->base + (1 << bar->bitsize)) >= (offset + size))
		return true;

	/* No match */
	return false;
}

static int
find_matching_bar(struct nfp_pcie_user *nfp,
		int target,
		int action,
		int token,
		uint64_t offset,
		size_t size,
		int width)
{
	uint32_t n;

	for (n = 0; n < nfp->bars; n++) {
		struct nfp_bar *bar = &nfp->bar[n];

		if (bar->lock)
			continue;

		if (matching_bar_exist(bar, target, action, token,
				offset, size, width))
			return n;
	}

	return -1;
}

/* Return EAGAIN if no resource is available */
static int
find_unused_bar_noblock(struct nfp_pcie_user *nfp,
		int target,
		int action,
		int token,
		uint64_t offset,
		size_t size,
		int width)
{
	int ret;
	uint32_t n;
	const struct nfp_bar *bar;

	for (n = 0; n < nfp->bars; n++) {
		bar = &nfp->bar[n];

		if (bar->bitsize == 0)
			continue;

		/* Just check to see if we can make it fit... */
		ret = nfp_compute_bar(bar, NULL, NULL, target, action,
				token, offset, size, width);
		if (ret != 0)
			continue;

		if (!bar->lock)
			return n;
	}

	return -EAGAIN;
}

static int
nfp_alloc_bar(struct nfp_pcie_user *nfp,
		struct nfp6000_area_priv *priv)
{
	int ret;
	int bar_num;
	size_t size = priv->size;
	int token = priv->token;
	int target = priv->target;
	int action = priv->action;
	int width = priv->width.bar;
	uint64_t offset = priv->offset;

	/* Bar size should small than 16MB */
	if (size > (1 << 24))
		return -EINVAL;

	bar_num = find_matching_bar(nfp, target, action, token,
			offset, size, width);
	if (bar_num >= 0) {
		/* Found a perfect match. */
		nfp->bar[bar_num].lock = true;
		return bar_num;
	}

	bar_num = find_unused_bar_noblock(nfp, target, action, token,
			offset, size, width);
	if (bar_num < 0)
		return bar_num;

	nfp->bar[bar_num].lock = true;
	ret = nfp_reconfigure_bar(nfp, &nfp->bar[bar_num],
			target, action, token, offset, size, width);
	if (ret < 0) {
		nfp->bar[bar_num].lock = false;
		return ret;
	}

	return bar_num;
}

static void
nfp_disable_bars(struct nfp_pcie_user *nfp)
{
	uint32_t i;
	struct nfp_bar *bar;

	for (i = 0; i < nfp->bars; i++) {
		bar = &nfp->bar[i];
		if (bar->iomem != NULL) {
			bar->iomem = NULL;
			bar->lock = false;
		}
	}
}

static int
nfp6000_area_init(struct nfp_cpp_area *area,
		uint32_t dest,
		uint64_t address,
		size_t size)
{
	int pp;
	int ret = 0;
	uint32_t token = NFP_CPP_ID_TOKEN_of(dest);
	uint32_t target = NFP_CPP_ID_TARGET_of(dest);
	uint32_t action = NFP_CPP_ID_ACTION_of(dest);
	struct nfp6000_area_priv *priv = nfp_cpp_area_priv(area);

	pp = nfp_target_pushpull(NFP_CPP_ID(target, action, token), address);
	if (pp < 0)
		return pp;

	priv->width.read = PUSH_WIDTH(pp);
	priv->width.write = PULL_WIDTH(pp);

	if (priv->width.read > 0 &&
			priv->width.write > 0 &&
			priv->width.read != priv->width.write)
		return -EINVAL;

	if (priv->width.read > 0)
		priv->width.bar = priv->width.read;
	else
		priv->width.bar = priv->width.write;

	priv->bar = NULL;

	priv->target = target;
	priv->action = action;
	priv->token = token;
	priv->offset = address;
	priv->size = size;

	return ret;
}

static int
nfp6000_area_acquire(struct nfp_cpp_area *area)
{
	int bar_num;
	struct nfp6000_area_priv *priv = nfp_cpp_area_priv(area);
	struct nfp_pcie_user *nfp = nfp_cpp_priv(nfp_cpp_area_cpp(area));

	/* Already allocated. */
	if (priv->bar != NULL)
		return 0;

	bar_num = nfp_alloc_bar(nfp, priv);
	if (bar_num < 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate bar %d:%d:%d:%#lx: %d",
				priv->target, priv->action, priv->token,
				priv->offset, bar_num);
		return bar_num;
	}

	priv->bar = &nfp->bar[bar_num];

	/* Calculate offset into BAR. */
	if (nfp_bar_maptype(priv->bar) ==
			NFP_PCIE_BAR_PCIE2CPP_MAPTYPE_GENERAL) {
		priv->bar_offset = priv->offset &
				(NFP_PCIE_P2C_GENERAL_SIZE(priv->bar) - 1);
		priv->bar_offset += NFP_PCIE_P2C_GENERAL_TARGET_OFFSET(priv->bar,
				priv->target);
		priv->bar_offset += NFP_PCIE_P2C_GENERAL_TOKEN_OFFSET(priv->bar,
				priv->token);
	} else {
		priv->bar_offset = priv->offset & priv->bar->mask;
	}

	/* Must have been too big. Sub-allocate. */
	if (priv->bar->iomem == NULL)
		return -ENOMEM;

	priv->iomem = priv->bar->iomem + priv->bar_offset;

	return 0;
}

static void
nfp6000_area_release(struct nfp_cpp_area *area)
{
	struct nfp6000_area_priv *priv = nfp_cpp_area_priv(area);

	priv->bar->lock = false;
	priv->bar = NULL;
	priv->iomem = NULL;
}

static void *
nfp6000_area_iomem(struct nfp_cpp_area *area)
{
	struct nfp6000_area_priv *priv = nfp_cpp_area_priv(area);
	return priv->iomem;
}

static int
nfp6000_area_read(struct nfp_cpp_area *area,
		void *address,
		uint32_t offset,
		size_t length)
{
	int ret;
	size_t n;
	int width;
	uint32_t *wrptr32 = address;
	uint64_t *wrptr64 = address;
	struct nfp6000_area_priv *priv;
	const volatile uint32_t *rdptr32;
	const volatile uint64_t *rdptr64;

	priv = nfp_cpp_area_priv(area);
	rdptr64 = (uint64_t *)(priv->iomem + offset);
	rdptr32 = (uint32_t *)(priv->iomem + offset);

	if (offset + length > priv->size)
		return -EFAULT;

	width = priv->width.read;
	if (width <= 0)
		return -EINVAL;

	/* MU reads via a PCIe2CPP BAR support 32bit (and other) lengths */
	if (priv->target == (NFP_CPP_TARGET_MU & NFP_CPP_TARGET_ID_MASK) &&
			priv->action == NFP_CPP_ACTION_RW &&
			(offset % sizeof(uint64_t) == 4 ||
			length % sizeof(uint64_t) == 4))
		width = TARGET_WIDTH_32;

	/* Unaligned? Translate to an explicit access */
	if (((priv->offset + offset) & (width - 1)) != 0) {
		PMD_DRV_LOG(ERR, "aread_read unaligned!!!");
		return -EINVAL;
	}

	if (priv->bar == NULL)
		return -EFAULT;

	switch (width) {
	case TARGET_WIDTH_32:
		if (offset % sizeof(uint32_t) != 0 ||
				length % sizeof(uint32_t) != 0)
			return -EINVAL;

		for (n = 0; n < length; n += sizeof(uint32_t)) {
			*wrptr32 = *rdptr32;
			wrptr32++;
			rdptr32++;
		}

		ret = n;
		break;
	case TARGET_WIDTH_64:
		if (offset % sizeof(uint64_t) != 0 ||
				length % sizeof(uint64_t) != 0)
			return -EINVAL;

		for (n = 0; n < length; n += sizeof(uint64_t)) {
			*wrptr64 = *rdptr64;
			wrptr64++;
			rdptr64++;
		}

		ret = n;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int
nfp6000_area_write(struct nfp_cpp_area *area,
		const void *address,
		uint32_t offset,
		size_t length)
{
	int ret;
	size_t n;
	int width;
	uint32_t *wrptr32;
	uint64_t *wrptr64;
	struct nfp6000_area_priv *priv;
	const uint32_t *rdptr32 = address;
	const uint64_t *rdptr64 = address;

	priv = nfp_cpp_area_priv(area);
	wrptr64 = (uint64_t *)(priv->iomem + offset);
	wrptr32 = (uint32_t *)(priv->iomem + offset);

	if (offset + length > priv->size)
		return -EFAULT;

	width = priv->width.write;
	if (width <= 0)
		return -EINVAL;

	/* MU reads via a PCIe2CPP BAR support 32bit (and other) lengths */
	if (priv->target == (NFP_CPP_TARGET_MU & NFP_CPP_TARGET_ID_MASK) &&
			priv->action == NFP_CPP_ACTION_RW &&
			(offset % sizeof(uint64_t) == 4 ||
			length % sizeof(uint64_t) == 4))
		width = TARGET_WIDTH_32;

	/* Unaligned? Translate to an explicit access */
	if (((priv->offset + offset) & (width - 1)) != 0)
		return -EINVAL;

	if (priv->bar == NULL)
		return -EFAULT;

	switch (width) {
	case TARGET_WIDTH_32:
		if (offset % sizeof(uint32_t) != 0 ||
				length % sizeof(uint32_t) != 0)
			return -EINVAL;

		for (n = 0; n < length; n += sizeof(uint32_t)) {
			*wrptr32 = *rdptr32;
			wrptr32++;
			rdptr32++;
		}

		ret = n;
		break;
	case TARGET_WIDTH_64:
		if (offset % sizeof(uint64_t) != 0 ||
				length % sizeof(uint64_t) != 0)
			return -EINVAL;

		for (n = 0; n < length; n += sizeof(uint64_t)) {
			*wrptr64 = *rdptr64;
			wrptr64++;
			rdptr64++;
		}

		ret = n;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int
nfp_acquire_process_lock(struct nfp_pcie_user *desc)
{
	int rc;
	struct flock lock;
	char lockname[30];

	memset(&lock, 0, sizeof(lock));

	snprintf(lockname, sizeof(lockname), "/var/lock/nfp_%s",
			desc->pci_dev->device.name);
	desc->lock = open(lockname, O_RDWR | O_CREAT, 0666);
	if (desc->lock < 0)
		return desc->lock;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	rc = -1;
	while (rc != 0) {
		rc = fcntl(desc->lock, F_SETLKW, &lock);
		if (rc < 0) {
			if (errno != EAGAIN && errno != EACCES) {
				close(desc->lock);
				return rc;
			}
		}
	}

	return 0;
}

static int
nfp6000_get_dsn(struct rte_pci_device *pci_dev,
		uint64_t *dsn)
{
	off_t pos;
	size_t len;
	uint64_t tmp = 0;

	pos = rte_pci_find_ext_capability(pci_dev, RTE_PCI_EXT_CAP_ID_DSN);
	if (pos <= 0) {
		PMD_DRV_LOG(ERR, "PCI_EXT_CAP_ID_DSN not found");
		return -ENODEV;
	}

	pos += 4;
	len = sizeof(tmp);

	if (rte_pci_read_config(pci_dev, &tmp, len, pos) < 0) {
		PMD_DRV_LOG(ERR, "nfp get device serial number failed");
		return -ENOENT;
	}

	*dsn = tmp;

	return 0;
}

static int
nfp6000_get_interface(struct rte_pci_device *dev,
		uint16_t *interface)
{
	int ret;
	uint64_t dsn = 0;

	ret = nfp6000_get_dsn(dev, &dsn);
	if (ret != 0)
		return ret;

	*interface = dsn & 0xffff;

	return 0;
}

static int
nfp6000_get_serial(struct rte_pci_device *dev,
		uint8_t *serial,
		size_t length)
{
	int ret;
	uint64_t dsn = 0;

	if (length < NFP_SERIAL_LEN)
		return -ENOMEM;

	ret = nfp6000_get_dsn(dev, &dsn);
	if (ret != 0)
		return ret;

	serial[0] = (dsn >> 56) & 0xff;
	serial[1] = (dsn >> 48) & 0xff;
	serial[2] = (dsn >> 40) & 0xff;
	serial[3] = (dsn >> 32) & 0xff;
	serial[4] = (dsn >> 24) & 0xff;
	serial[5] = (dsn >> 16) & 0xff;

	return 0;
}

static int
nfp6000_init(struct nfp_cpp *cpp)
{
	int ret = 0;
	struct nfp_pcie_user *desc = nfp_cpp_priv(cpp);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
			nfp_cpp_driver_need_lock(cpp)) {
		ret = nfp_acquire_process_lock(desc);
		if (ret != 0)
			return -1;
	}

	ret = nfp_enable_bars(desc);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Enable bars failed");
		return -1;
	}

	return 0;
}

static void
nfp6000_free(struct nfp_cpp *cpp)
{
	struct nfp_pcie_user *desc = nfp_cpp_priv(cpp);

	nfp_disable_bars(desc);
	if (nfp_cpp_driver_need_lock(cpp))
		close(desc->lock);
	free(desc);
}

static const struct nfp_cpp_operations nfp6000_pcie_ops = {
	.init = nfp6000_init,
	.free = nfp6000_free,

	.area_priv_size = sizeof(struct nfp6000_area_priv),

	.get_interface = nfp6000_get_interface,
	.get_serial = nfp6000_get_serial,

	.area_init = nfp6000_area_init,
	.area_acquire = nfp6000_area_acquire,
	.area_release = nfp6000_area_release,
	.area_read = nfp6000_area_read,
	.area_write = nfp6000_area_write,
	.area_iomem = nfp6000_area_iomem,
};

const struct
nfp_cpp_operations *nfp_cpp_transport_operations(void)
{
	return &nfp6000_pcie_ops;
}

/**
 * Build a NFP CPP bus from a NFP6000 PCI device
 *
 * @param pdev
 *   NFP6000 PCI device
 * @param driver_lock_needed
 *   driver lock flag
 *
 * @return
 *   NFP CPP handle or NULL
 */
struct nfp_cpp *
nfp_cpp_from_nfp6000_pcie(struct rte_pci_device *pci_dev,
		const struct nfp_dev_info *dev_info,
		bool driver_lock_needed)
{
	int ret;
	struct nfp_cpp *cpp;
	uint16_t interface = 0;
	struct nfp_pcie_user *nfp;

	nfp = malloc(sizeof(*nfp));
	if (nfp == NULL)
		return NULL;

	memset(nfp, 0, sizeof(*nfp));
	nfp->pci_dev = pci_dev;
	nfp->dev_info = dev_info;

	ret = nfp6000_get_interface(pci_dev, &interface);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Get interface failed.");
		free(nfp);
		return NULL;
	}

	if (NFP_CPP_INTERFACE_TYPE_of(interface) != NFP_CPP_INTERFACE_TYPE_PCI) {
		PMD_DRV_LOG(ERR, "Interface type is not right.");
		free(nfp);
		return NULL;
	}

	if (NFP_CPP_INTERFACE_CHANNEL_of(interface) !=
			NFP_CPP_INTERFACE_CHANNEL_PEROPENER) {
		PMD_DRV_LOG(ERR, "Interface channel is not right");
		free(nfp);
		return NULL;
	}

	/* Probe for all the common NFP devices */
	cpp = nfp_cpp_from_device_name(pci_dev, nfp, driver_lock_needed);
	if (cpp == NULL) {
		PMD_DRV_LOG(ERR, "Get cpp from operation failed");
		free(nfp);
		return NULL;
	}

	return cpp;
}
