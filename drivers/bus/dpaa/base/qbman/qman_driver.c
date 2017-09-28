/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
void *qman_ccsr_map;
/* The qman clock frequency */
u32 qman_clk;

static __thread int fd = -1;
static __thread struct qm_portal_config pcfg;
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
	pcfg.cpu = -1;
	for (loop = 0; loop < CPU_SETSIZE; loop++)
		if (CPU_ISSET(loop, &cpuset)) {
			if (pcfg.cpu != -1) {
				pr_err("Thread is not affine to 1 cpu\n");
				return -EINVAL;
			}
			pcfg.cpu = loop;
		}
	if (pcfg.cpu == -1) {
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
	pcfg.channel = map.channel;
	pcfg.pools = map.pools;
	pcfg.index = map.index;

	/* Make the portal's cache-[enabled|inhibited] regions */
	pcfg.addr_virt[DPAA_PORTAL_CE] = map.addr.cena;
	pcfg.addr_virt[DPAA_PORTAL_CI] = map.addr.cinh;

	fd = open(QMAN_PORTAL_IRQ_PATH, O_RDONLY);
	if (fd == -1) {
		pr_err("QMan irq init failed\n");
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}

	pcfg.is_shared = is_shared;
	pcfg.node = NULL;
	pcfg.irq = fd;

	portal = qman_create_affine_portal(&pcfg, NULL);
	if (!portal) {
		pr_err("Qman portal initialisation failed (%d)\n",
		       pcfg.cpu);
		process_portal_unmap(&map.addr);
		return -EBUSY;
	}

	irq_map.type = dpaa_portal_qman;
	irq_map.portal_cinh = map.addr.cinh;
	process_portal_irq_map(fd, &irq_map);
	return 0;
}

static int fsl_qman_portal_finish(void)
{
	__maybe_unused const struct qm_portal_config *cfg;
	int ret;

	process_portal_irq_unmap(fd);

	cfg = qman_destroy_affine_portal();
	DPAA_BUG_ON(cfg != &pcfg);
	ret = process_portal_unmap(&map.addr);
	if (ret)
		error(0, ret, "process_portal_unmap()");
	return ret;
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
	qbman_invoke_irq(pcfg.irq);

	/* Now we need to uninhibit interrupts. This is the only code outside
	 * the regular portal driver that manipulates any portal register, so
	 * rather than breaking that encapsulation I am simply hard-coding the
	 * offset to the inhibit register here.
	 */
	out_be32(pcfg.addr_virt[DPAA_PORTAL_CI] + 0xe0c, 0);
}

int qman_global_init(void)
{
	const struct device_node *dt_node;
	int ret = 0;
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
	ret = qman_setup_fq_lookup_table(CONFIG_FSL_QMAN_FQ_LOOKUP_MAX);
	if (ret)
		return ret;
#endif
	return 0;
}
