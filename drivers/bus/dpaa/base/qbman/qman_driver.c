/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include <fsl_usd.h>
#include <process.h>
#include "qman_priv.h"
#include <sys/ioctl.h>
#include <rte_branch_prediction.h>

/* Global variable containing revision id (even on non-control plane systems
 * where CCSR isn't available).
 */
u16 qman_ip_rev;
u16 qm_channel_pool1 = QMAN_CHANNEL_POOL1;
u16 qm_channel_caam = QMAN_CHANNEL_CAAM;
u16 qm_channel_pme = QMAN_CHANNEL_PME;

/* Ccsr map address to access ccsrbased register */
static void *qman_ccsr_map;
/* The qman clock frequency */
static u32 qman_clk;

static __thread int qmfd = -1;
static __thread struct qm_portal_config qpcfg;
static __thread struct dpaa_ioctl_portal_map map = {
	.type = dpaa_portal_qman
};

static int fsl_qman_portal_init(uint32_t index, int is_shared)
{
	cpu_set_t cpuset;
	struct qman_portal *portal;
	int loop, ret;
	struct dpaa_ioctl_irq_map irq_map;

	/* Verify the thread's cpu-affinity */
	ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t),
				     &cpuset);
	if (ret) {
		error(0, ret, "pthread_getaffinity_np()");
		return ret;
	}
	qpcfg.cpu = -1;
	for (loop = 0; loop < CPU_SETSIZE; loop++)
		if (CPU_ISSET(loop, &cpuset)) {
			if (qpcfg.cpu != -1) {
				pr_err("Thread is not affine to 1 cpu\n");
				return -EINVAL;
			}
			qpcfg.cpu = loop;
		}
	if (qpcfg.cpu == -1) {
		pr_err("Bug in getaffinity handling!\n");
		return -EINVAL;
	}

	/* Allocate and map a qman portal */
	map.index = index;
	ret = process_portal_map(&map);
	if (ret) {
		error(0, ret, "process_portal_map()");
		return ret;
	}
	qpcfg.channel = map.channel;
	qpcfg.pools = map.pools;
	qpcfg.index = map.index;

	/* Make the portal's cache-[enabled|inhibited] regions */
	qpcfg.addr_virt[DPAA_PORTAL_CE] = map.addr.cena;
	qpcfg.addr_virt[DPAA_PORTAL_CI] = map.addr.cinh;

	qmfd = open(QMAN_PORTAL_IRQ_PATH, O_RDONLY);
	if (qmfd == -1) {
		pr_err("QMan irq init failed\n");
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}

	qpcfg.is_shared = is_shared;
	qpcfg.node = NULL;
	qpcfg.irq = qmfd;

	portal = qman_create_affine_portal(&qpcfg, NULL, 0);
	if (!portal) {
		pr_err("Qman portal initialisation failed (%d)\n",
		       qpcfg.cpu);
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}

	irq_map.type = dpaa_portal_qman;
	irq_map.portal_cinh = map.addr.cinh;
	process_portal_irq_map(qmfd, &irq_map);
	return 0;
}

static int fsl_qman_portal_finish(void)
{
	__maybe_unused const struct qm_portal_config *cfg;
	int ret;

	process_portal_irq_unmap(qmfd);

	cfg = qman_destroy_affine_portal(NULL);
	DPAA_BUG_ON(cfg != &qpcfg);
	ret = process_portal_unmap(&map.addr);
	if (ret)
		error(0, ret, "process_portal_unmap()");
	return ret;
}

int qman_thread_fd(void)
{
	return qmfd;
}

int qman_thread_init(void)
{
	/* Convert from contiguous/virtual cpu numbering to real cpu when
	 * calling into the code that is dependent on the device naming.
	 */
	return fsl_qman_portal_init(QBMAN_ANY_PORTAL_IDX, 0);
}

int qman_thread_finish(void)
{
	return fsl_qman_portal_finish();
}

void qman_thread_irq(void)
{
	qbman_invoke_irq(qpcfg.irq);

	/* Now we need to uninhibit interrupts. This is the only code outside
	 * the regular portal driver that manipulates any portal register, so
	 * rather than breaking that encapsulation I am simply hard-coding the
	 * offset to the inhibit register here.
	 */
	out_be32(qpcfg.addr_virt[DPAA_PORTAL_CI] + 0x36C0, 0);
}

struct qman_portal *fsl_qman_portal_create(void)
{
	cpu_set_t cpuset;
	struct qman_portal *res;

	struct qm_portal_config *q_pcfg;
	int loop, ret;
	struct dpaa_ioctl_irq_map irq_map;
	struct dpaa_ioctl_portal_map q_map = {0};
	int q_fd;

	q_pcfg = kzalloc((sizeof(struct qm_portal_config)), 0);
	if (!q_pcfg) {
		error(0, -1, "q_pcfg kzalloc failed");
		return NULL;
	}

	/* Verify the thread's cpu-affinity */
	ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t),
				     &cpuset);
	if (ret) {
		error(0, ret, "pthread_getaffinity_np()");
		kfree(q_pcfg);
		return NULL;
	}

	q_pcfg->cpu = -1;
	for (loop = 0; loop < CPU_SETSIZE; loop++)
		if (CPU_ISSET(loop, &cpuset)) {
			if (q_pcfg->cpu != -1) {
				pr_err("Thread is not affine to 1 cpu\n");
				kfree(q_pcfg);
				return NULL;
			}
			q_pcfg->cpu = loop;
		}
	if (q_pcfg->cpu == -1) {
		pr_err("Bug in getaffinity handling!\n");
		kfree(q_pcfg);
		return NULL;
	}

	/* Allocate and map a qman portal */
	q_map.type = dpaa_portal_qman;
	q_map.index = QBMAN_ANY_PORTAL_IDX;
	ret = process_portal_map(&q_map);
	if (ret) {
		error(0, ret, "process_portal_map()");
		kfree(q_pcfg);
		return NULL;
	}
	q_pcfg->channel = q_map.channel;
	q_pcfg->pools = q_map.pools;
	q_pcfg->index = q_map.index;

	/* Make the portal's cache-[enabled|inhibited] regions */
	q_pcfg->addr_virt[DPAA_PORTAL_CE] = q_map.addr.cena;
	q_pcfg->addr_virt[DPAA_PORTAL_CI] = q_map.addr.cinh;

	q_fd = open(QMAN_PORTAL_IRQ_PATH, O_RDONLY);
	if (q_fd == -1) {
		pr_err("QMan irq init failed\n");
		goto err1;
	}

	q_pcfg->irq = q_fd;

	res = qman_create_affine_portal(q_pcfg, NULL, true);
	if (!res) {
		pr_err("Qman portal initialisation failed (%d)\n",
		       q_pcfg->cpu);
		goto err2;
	}

	irq_map.type = dpaa_portal_qman;
	irq_map.portal_cinh = q_map.addr.cinh;
	process_portal_irq_map(q_fd, &irq_map);

	return res;
err2:
	close(q_fd);
err1:
	process_portal_unmap(&q_map.addr);
	kfree(q_pcfg);
	return NULL;
}

int fsl_qman_portal_destroy(struct qman_portal *qp)
{
	const struct qm_portal_config *cfg;
	struct dpaa_portal_map addr;
	int ret;

	cfg = qman_destroy_affine_portal(qp);
	kfree(qp);

	process_portal_irq_unmap(cfg->irq);

	addr.cena = cfg->addr_virt[DPAA_PORTAL_CE];
	addr.cinh = cfg->addr_virt[DPAA_PORTAL_CI];

	ret = process_portal_unmap(&addr);
	if (ret)
		pr_err("process_portal_unmap() (%d)\n", ret);

	kfree((void *)cfg);

	return ret;
}

int qman_global_init(void)
{
	const struct device_node *dt_node;
	size_t lenp;
	const u32 *chanid;
	static int ccsr_map_fd;
	const uint32_t *qman_addr;
	uint64_t phys_addr;
	uint64_t regs_size;
	const u32 *clk;

	static int done;

	if (done)
		return -EBUSY;

	/* Use the device-tree to determine IP revision until something better
	 * is devised.
	 */
	dt_node = of_find_compatible_node(NULL, NULL, "fsl,qman-portal");
	if (!dt_node) {
		pr_err("No qman portals available for any CPU\n");
		return -ENODEV;
	}
	if (of_device_is_compatible(dt_node, "fsl,qman-portal-1.0") ||
	    of_device_is_compatible(dt_node, "fsl,qman-portal-1.0.0"))
		pr_err("QMan rev1.0 on P4080 rev1 is not supported!\n");
	else if (of_device_is_compatible(dt_node, "fsl,qman-portal-1.1") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-1.1.0"))
		qman_ip_rev = QMAN_REV11;
	else if	(of_device_is_compatible(dt_node, "fsl,qman-portal-1.2") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-1.2.0"))
		qman_ip_rev = QMAN_REV12;
	else if (of_device_is_compatible(dt_node, "fsl,qman-portal-2.0") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-2.0.0"))
		qman_ip_rev = QMAN_REV20;
	else if (of_device_is_compatible(dt_node, "fsl,qman-portal-3.0.0") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-3.0.1"))
		qman_ip_rev = QMAN_REV30;
	else if (of_device_is_compatible(dt_node, "fsl,qman-portal-3.1.0") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-3.1.1") ||
		of_device_is_compatible(dt_node, "fsl,qman-portal-3.1.2") ||
		of_device_is_compatible(dt_node, "fsl,qman-portal-3.1.3"))
		qman_ip_rev = QMAN_REV31;
	else if (of_device_is_compatible(dt_node, "fsl,qman-portal-3.2.0") ||
		 of_device_is_compatible(dt_node, "fsl,qman-portal-3.2.1"))
		qman_ip_rev = QMAN_REV32;
	else
		qman_ip_rev = QMAN_REV11;

	if (!qman_ip_rev) {
		pr_err("Unknown qman portal version\n");
		return -ENODEV;
	}
	if ((qman_ip_rev & 0xFF00) >= QMAN_REV30) {
		qm_channel_pool1 = QMAN_CHANNEL_POOL1_REV3;
		qm_channel_caam = QMAN_CHANNEL_CAAM_REV3;
		qm_channel_pme = QMAN_CHANNEL_PME_REV3;
	}

	dt_node = of_find_compatible_node(NULL, NULL, "fsl,pool-channel-range");
	if (!dt_node) {
		pr_err("No qman pool channel range available\n");
		return -ENODEV;
	}
	chanid = of_get_property(dt_node, "fsl,pool-channel-range", &lenp);
	if (!chanid) {
		pr_err("Can not get pool-channel-range property\n");
		return -EINVAL;
	}

	/* get ccsr base */
	dt_node = of_find_compatible_node(NULL, NULL, "fsl,qman");
	if (!dt_node) {
		pr_err("No qman device node available\n");
		return -ENODEV;
	}
	qman_addr = of_get_address(dt_node, 0, &regs_size, NULL);
	if (!qman_addr) {
		pr_err("of_get_address cannot return qman address\n");
		return -EINVAL;
	}
	phys_addr = of_translate_address(dt_node, qman_addr);
	if (!phys_addr) {
		pr_err("of_translate_address failed\n");
		return -EINVAL;
	}

	ccsr_map_fd = open("/dev/mem", O_RDWR);
	if (unlikely(ccsr_map_fd < 0)) {
		pr_err("Can not open /dev/mem for qman ccsr map\n");
		return ccsr_map_fd;
	}

	qman_ccsr_map = mmap(NULL, regs_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, ccsr_map_fd, phys_addr);
	if (qman_ccsr_map == MAP_FAILED) {
		pr_err("Can not map qman ccsr base\n");
		return -EINVAL;
	}

	clk = of_get_property(dt_node, "clock-frequency", NULL);
	if (!clk)
		pr_warn("Can't find Qman clock frequency\n");
	else
		qman_clk = be32_to_cpu(*clk);

#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
	return qman_setup_fq_lookup_table(CONFIG_FSL_QMAN_FQ_LOOKUP_MAX);
#endif
	return 0;
}
