/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_io.h>
#include <rte_vfio.h>
#include <bus_pci_driver.h>
#include <bus_ifpga_driver.h>
#include <rte_rawdev.h>

#include "afu_pmd_core.h"
#include "afu_pmd_he_mem.h"

static int he_mem_tg_test(struct afu_rawdev *dev)
{
	struct he_mem_tg_priv *priv = NULL;
	struct rte_pmd_afu_he_mem_tg_cfg *cfg = NULL;
	struct he_mem_tg_ctx *ctx = NULL;
	uint64_t value = 0x12345678;
	uint64_t cap = 0;
	uint64_t channel_mask = 0;
	int i, t = 0;

	if (!dev)
		return -EINVAL;

	priv = (struct he_mem_tg_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	cfg = &priv->he_mem_tg_cfg;
	ctx = &priv->he_mem_tg_ctx;

	IFPGA_RAWDEV_PMD_DEBUG("Channel mask: 0x%x", cfg->channel_mask);

	rte_write64(value, ctx->addr + MEM_TG_SCRATCHPAD);
	cap = rte_read64(ctx->addr + MEM_TG_SCRATCHPAD);
	IFPGA_RAWDEV_PMD_DEBUG("Scratchpad value: 0x%"PRIx64, cap);
	if (cap != value) {
		IFPGA_RAWDEV_PMD_ERR("Test scratchpad register failed");
		return -EIO;
	}

	cap = rte_read64(ctx->addr + MEM_TG_CTRL);
	IFPGA_RAWDEV_PMD_DEBUG("Capability: 0x%"PRIx64, cap);

	channel_mask = cfg->channel_mask & cap;
	/* start traffic generators */
	rte_write64(channel_mask, ctx->addr + MEM_TG_CTRL);

	/* check test status */
	while (t < MEM_TG_TIMEOUT_MS) {
		value = rte_read64(ctx->addr + MEM_TG_STAT);
		for (i = 0; i < NUM_MEM_TG_CHANNELS; i++) {
			if (channel_mask & (1 << i)) {
				if (TGACTIVE(value, i))
					continue;
				printf("TG channel %d test %s\n", i,
					TGPASS(value, i) ? "pass" :
					TGTIMEOUT(value, i) ? "timeout" :
					TGFAIL(value, i) ? "fail" : "error");
				channel_mask &= ~(1 << i);
			}
		}
		if (!channel_mask)
			break;
		rte_delay_ms(MEM_TG_POLL_INTERVAL_MS);
		t += MEM_TG_POLL_INTERVAL_MS;
	}

	if (channel_mask) {
		IFPGA_RAWDEV_PMD_ERR("Timeout 0x%04lx", (unsigned long)value);
		return channel_mask;
	}

	return 0;
}

static int he_mem_tg_init(struct afu_rawdev *dev)
{
	struct he_mem_tg_priv *priv = NULL;
	struct he_mem_tg_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct he_mem_tg_priv *)dev->priv;
	if (!priv) {
		priv = rte_zmalloc(NULL, sizeof(struct he_mem_tg_priv), 0);
		if (!priv)
			return -ENOMEM;
		dev->priv = priv;
	}

	ctx = &priv->he_mem_tg_ctx;
	ctx->addr = (uint8_t *)dev->addr;

	return 0;
}

static int he_mem_tg_config(struct afu_rawdev *dev, void *config,
	size_t config_size)
{
	struct he_mem_tg_priv *priv = NULL;

	if (!dev || !config || !config_size)
		return -EINVAL;

	priv = (struct he_mem_tg_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (config_size != sizeof(struct rte_pmd_afu_he_mem_tg_cfg))
		return -EINVAL;

	rte_memcpy(&priv->he_mem_tg_cfg, config, sizeof(priv->he_mem_tg_cfg));

	return 0;
}

static int he_mem_tg_close(struct afu_rawdev *dev)
{
	if (!dev)
		return -EINVAL;

	rte_free(dev->priv);
	dev->priv = NULL;

	return 0;
}

static int he_mem_tg_dump(struct afu_rawdev *dev, FILE *f)
{
	struct he_mem_tg_priv *priv = NULL;
	struct he_mem_tg_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct he_mem_tg_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (!f)
		f = stdout;

	ctx = &priv->he_mem_tg_ctx;

	fprintf(f, "addr:\t\t%p\n", (void *)ctx->addr);

	return 0;
}

static struct afu_ops he_mem_tg_ops = {
	.init = he_mem_tg_init,
	.config = he_mem_tg_config,
	.start = NULL,
	.stop = NULL,
	.test = he_mem_tg_test,
	.close = he_mem_tg_close,
	.dump = he_mem_tg_dump,
	.reset = NULL
};

struct afu_rawdev_drv he_mem_tg_drv = {
	.uuid = { HE_MEM_TG_UUID_L, HE_MEM_TG_UUID_H },
	.ops = &he_mem_tg_ops
};

AFU_PMD_REGISTER(he_mem_tg_drv);
