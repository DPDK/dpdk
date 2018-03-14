/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include <rte_branch_prediction.h>

#include <fsl_usd.h>
#include <process.h>
#include "bman_priv.h"
#include <sys/ioctl.h>

/*
 * Global variables of the max portal/pool number this bman version supported
 */
u16 bman_ip_rev;
u16 bman_pool_max;
void *bman_ccsr_map;

/*****************/
/* Portal driver */
/*****************/

static __thread int fd = -1;
static __thread struct bm_portal_config pcfg;
static __thread struct dpaa_ioctl_portal_map map = {
	.type = dpaa_portal_bman
};

static int fsl_bman_portal_init(uint32_t idx, int is_shared)
{
	cpu_set_t cpuset;
	struct bman_portal *portal;
	int loop, ret;
	struct dpaa_ioctl_irq_map irq_map;

	/* Verify the thread's cpu-affinity */
	ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t),
				     &cpuset);
	if (ret) {
		error(0, ret, "pthread_getaffinity_np()");
		return ret;
	}
	pcfg.cpu = -1;
	for (loop = 0; loop < CPU_SETSIZE; loop++)
		if (CPU_ISSET(loop, &cpuset)) {
			if (pcfg.cpu != -1) {
				pr_err("Thread is not affine to 1 cpu");
				return -EINVAL;
			}
			pcfg.cpu = loop;
		}
	if (pcfg.cpu == -1) {
		pr_err("Bug in getaffinity handling!");
		return -EINVAL;
	}
	/* Allocate and map a bman portal */
	map.index = idx;
	ret = process_portal_map(&map);
	if (ret) {
		error(0, ret, "process_portal_map()");
		return ret;
	}
	/* Make the portal's cache-[enabled|inhibited] regions */
	pcfg.addr_virt[DPAA_PORTAL_CE] = map.addr.cena;
	pcfg.addr_virt[DPAA_PORTAL_CI] = map.addr.cinh;
	pcfg.is_shared = is_shared;
	pcfg.index = map.index;
	bman_depletion_fill(&pcfg.mask);

	fd = open(BMAN_PORTAL_IRQ_PATH, O_RDONLY);
	if (fd == -1) {
		pr_err("BMan irq init failed");
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}
	/* Use the IRQ FD as a unique IRQ number */
	pcfg.irq = fd;

	portal = bman_create_affine_portal(&pcfg);
	if (!portal) {
		pr_err("Bman portal initialisation failed (%d)",
		       pcfg.cpu);
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}

	/* Set the IRQ number */
	irq_map.type = dpaa_portal_bman;
	irq_map.portal_cinh = map.addr.cinh;
	process_portal_irq_map(fd, &irq_map);
	return 0;
}

static int fsl_bman_portal_finish(void)
{
	__maybe_unused const struct bm_portal_config *cfg;
	int ret;

	process_portal_irq_unmap(fd);

	cfg = bman_destroy_affine_portal();
	DPAA_BUG_ON(cfg != &pcfg);
	ret = process_portal_unmap(&map.addr);
	if (ret)
		error(0, ret, "process_portal_unmap()");
	return ret;
}

int bman_thread_init(void)
{
	/* Convert from contiguous/virtual cpu numbering to real cpu when
	 * calling into the code that is dependent on the device naming.
	 */
	return fsl_bman_portal_init(QBMAN_ANY_PORTAL_IDX, 0);
}

int bman_thread_finish(void)
{
	return fsl_bman_portal_finish();
}

void bman_thread_irq(void)
{
	qbman_invoke_irq(pcfg.irq);
	/* Now we need to uninhibit interrupts. This is the only code outside
	 * the regular portal driver that manipulates any portal register, so
	 * rather than breaking that encapsulation I am simply hard-coding the
	 * offset to the inhibit register here.
	 */
	out_be32(pcfg.addr_virt[DPAA_PORTAL_CI] + 0xe0c, 0);
}

int bman_init_ccsr(const struct device_node *node)
{
	static int ccsr_map_fd;
	uint64_t phys_addr;
	const uint32_t *bman_addr;
	uint64_t regs_size;

	bman_addr = of_get_address(node, 0, &regs_size, NULL);
	if (!bman_addr) {
		pr_err("of_get_address cannot return BMan address");
		return -EINVAL;
	}
	phys_addr = of_translate_address(node, bman_addr);
	if (!phys_addr) {
		pr_err("of_translate_address failed");
		return -EINVAL;
	}

	ccsr_map_fd = open(BMAN_CCSR_MAP, O_RDWR);
	if (unlikely(ccsr_map_fd < 0)) {
		pr_err("Can not open /dev/mem for BMan CCSR map");
		return ccsr_map_fd;
	}

	bman_ccsr_map = mmap(NULL, regs_size, PROT_READ |
			     PROT_WRITE, MAP_SHARED, ccsr_map_fd, phys_addr);
	if (bman_ccsr_map == MAP_FAILED) {
		pr_err("Can not map BMan CCSR base Bman: "
		       "0x%x Phys: 0x%" PRIx64 " size 0x%" PRIu64,
		       *bman_addr, phys_addr, regs_size);
		return -EINVAL;
	}

	return 0;
}

int bman_global_init(void)
{
	const struct device_node *dt_node;
	static int done;

	if (done)
		return -EBUSY;
	/* Use the device-tree to determine IP revision until something better
	 * is devised.
	 */
	dt_node = of_find_compatible_node(NULL, NULL, "fsl,bman-portal");
	if (!dt_node) {
		pr_err("No bman portals available for any CPU\n");
		return -ENODEV;
	}
	if (of_device_is_compatible(dt_node, "fsl,bman-portal-1.0") ||
	    of_device_is_compatible(dt_node, "fsl,bman-portal-1.0.0")) {
		bman_ip_rev = BMAN_REV10;
		bman_pool_max = 64;
	} else if (of_device_is_compatible(dt_node, "fsl,bman-portal-2.0") ||
		of_device_is_compatible(dt_node, "fsl,bman-portal-2.0.8")) {
		bman_ip_rev = BMAN_REV20;
		bman_pool_max = 8;
	} else if (of_device_is_compatible(dt_node, "fsl,bman-portal-2.1.0") ||
		of_device_is_compatible(dt_node, "fsl,bman-portal-2.1.1") ||
		of_device_is_compatible(dt_node, "fsl,bman-portal-2.1.2") ||
		of_device_is_compatible(dt_node, "fsl,bman-portal-2.1.3")) {
		bman_ip_rev = BMAN_REV21;
		bman_pool_max = 64;
	} else {
		pr_warn("unknown BMan version in portal node,default "
			"to rev1.0");
		bman_ip_rev = BMAN_REV10;
		bman_pool_max = 64;
	}

	if (!bman_ip_rev) {
		pr_err("Unknown bman portal version\n");
		return -ENODEV;
	}
	{
		const struct device_node *dn = of_find_compatible_node(NULL,
							NULL, "fsl,bman");
		if (!dn)
			pr_err("No bman device node available");

		if (bman_init_ccsr(dn))
			pr_err("BMan CCSR map failed.");
	}

	done = 1;
	return 0;
}

#define BMAN_POOL_CONTENT(n) (0x0600 + ((n) * 0x04))
u32 bm_pool_free_buffers(u32 bpid)
{
	return in_be32(bman_ccsr_map + BMAN_POOL_CONTENT(bpid));
}

static u32 __generate_thresh(u32 val, int roundup)
{
	u32 e = 0;      /* co-efficient, exponent */
	int oddbit = 0;

	while (val > 0xff) {
		oddbit = val & 1;
		val >>= 1;
		e++;
		if (roundup && oddbit)
			val++;
	}
	DPAA_ASSERT(e < 0x10);
	return (val | (e << 8));
}

#define POOL_SWDET(n)       (0x0000 + ((n) * 0x04))
#define POOL_HWDET(n)       (0x0100 + ((n) * 0x04))
#define POOL_SWDXT(n)       (0x0200 + ((n) * 0x04))
#define POOL_HWDXT(n)       (0x0300 + ((n) * 0x04))
int bm_pool_set(u32 bpid, const u32 *thresholds)
{
	if (!bman_ccsr_map)
		return -ENODEV;
	if (bpid >= bman_pool_max)
		return -EINVAL;
	out_be32(bman_ccsr_map + POOL_SWDET(bpid),
		 __generate_thresh(thresholds[0], 0));
	out_be32(bman_ccsr_map + POOL_SWDXT(bpid),
		 __generate_thresh(thresholds[1], 1));
	out_be32(bman_ccsr_map + POOL_HWDET(bpid),
		 __generate_thresh(thresholds[2], 0));
	out_be32(bman_ccsr_map + POOL_HWDXT(bpid),
		 __generate_thresh(thresholds[3], 1));
	return 0;
}

#define BMAN_LOW_DEFAULT_THRESH		0x40
#define BMAN_HIGH_DEFAULT_THRESH		0x80
int bm_pool_set_hw_threshold(u32 bpid, const u32 low_thresh,
			     const u32 high_thresh)
{
	if (!bman_ccsr_map)
		return -ENODEV;
	if (bpid >= bman_pool_max)
		return -EINVAL;
	if (low_thresh && high_thresh) {
		out_be32(bman_ccsr_map + POOL_HWDET(bpid),
			 __generate_thresh(low_thresh, 0));
		out_be32(bman_ccsr_map + POOL_HWDXT(bpid),
			 __generate_thresh(high_thresh, 1));
	} else {
		out_be32(bman_ccsr_map + POOL_HWDET(bpid),
			 __generate_thresh(BMAN_LOW_DEFAULT_THRESH, 0));
		out_be32(bman_ccsr_map + POOL_HWDXT(bpid),
			 __generate_thresh(BMAN_HIGH_DEFAULT_THRESH, 1));
	}
	return 0;
}
