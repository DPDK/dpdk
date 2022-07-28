/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
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
#include "afu_pmd_he_hssi.h"

static int he_hssi_indirect_write(struct he_hssi_ctx *ctx, uint32_t addr,
	uint32_t value)
{
	struct traffic_ctrl_cmd cmd;
	struct traffic_ctrl_data data;
	uint32_t i = 0;

	IFPGA_RAWDEV_PMD_DEBUG("Indirect write 0x%x, value 0x%08x", addr, value);

	if (!ctx)
		return -EINVAL;

	data.write_data = value;
	rte_write64(data.csr, ctx->addr + TRAFFIC_CTRL_DATA);

	cmd.csr = 0;
	cmd.write_cmd = 1;
	cmd.afu_cmd_addr = addr;
	rte_write64(cmd.csr, ctx->addr + TRAFFIC_CTRL_CMD);

	while (i < MAILBOX_TIMEOUT_MS) {
		rte_delay_ms(MAILBOX_POLL_INTERVAL_MS);
		cmd.csr = rte_read64(ctx->addr + TRAFFIC_CTRL_CMD);
		if (cmd.ack_trans)
			break;
		i += MAILBOX_POLL_INTERVAL_MS;
	}
	if (i >= MAILBOX_TIMEOUT_MS)
		return -ETIMEDOUT;

	i = 0;
	cmd.csr = 0;
	while (i < MAILBOX_TIMEOUT_MS) {
		cmd.ack_trans = 1;
		rte_write64(cmd.csr, ctx->addr + TRAFFIC_CTRL_CMD);
		rte_delay_ms(MAILBOX_POLL_INTERVAL_MS);
		cmd.csr = rte_read64(ctx->addr + TRAFFIC_CTRL_CMD);
		if (!cmd.ack_trans)
			break;
		i += MAILBOX_POLL_INTERVAL_MS;
	}
	if (i >= MAILBOX_TIMEOUT_MS)
		return -ETIMEDOUT;

	return 0;
}

static int he_hssi_indirect_read(struct he_hssi_ctx *ctx, uint32_t addr,
	uint32_t *value)
{
	struct traffic_ctrl_cmd cmd;
	struct traffic_ctrl_data data;
	uint32_t i = 0;

	if (!ctx)
		return -EINVAL;

	cmd.csr = 0;
	cmd.read_cmd = 1;
	cmd.afu_cmd_addr = addr;
	rte_write64(cmd.csr, ctx->addr + TRAFFIC_CTRL_CMD);

	while (i < MAILBOX_TIMEOUT_MS) {
		rte_delay_ms(MAILBOX_POLL_INTERVAL_MS);
		cmd.csr = rte_read64(ctx->addr + TRAFFIC_CTRL_CMD);
		if (cmd.ack_trans) {
			data.csr = rte_read64(ctx->addr + TRAFFIC_CTRL_DATA);
			*value = data.read_data;
			break;
		}
		i += MAILBOX_POLL_INTERVAL_MS;
	}
	if (i >= MAILBOX_TIMEOUT_MS)
		return -ETIMEDOUT;

	i = 0;
	cmd.csr = 0;
	while (i < MAILBOX_TIMEOUT_MS) {
		cmd.ack_trans = 1;
		rte_write64(cmd.csr, ctx->addr + TRAFFIC_CTRL_CMD);
		rte_delay_ms(MAILBOX_POLL_INTERVAL_MS);
		cmd.csr = rte_read64(ctx->addr + TRAFFIC_CTRL_CMD);
		if (!cmd.ack_trans)
			break;
		i += MAILBOX_POLL_INTERVAL_MS;
	}
	if (i >= MAILBOX_TIMEOUT_MS)
		return -ETIMEDOUT;

	IFPGA_RAWDEV_PMD_DEBUG("Indirect read 0x%x, value 0x%08x", addr, *value);
	return 0;
}

static void he_hssi_report(struct he_hssi_ctx *ctx)
{
	uint32_t val = 0;
	uint64_t v64 = 0;
	int ret = 0;

	ret = he_hssi_indirect_read(ctx, TM_PKT_GOOD, &val);
	if (ret)
		return;
	printf("Number of good packets received: %u\n", val);

	ret = he_hssi_indirect_read(ctx, TM_PKT_BAD, &val);
	if (ret)
		return;
	printf("Number of bad packets received: %u\n", val);

	ret = he_hssi_indirect_read(ctx, TM_BYTE_CNT1, &val);
	if (ret)
		return;
	v64 = val;
	ret = he_hssi_indirect_read(ctx, TM_BYTE_CNT0, &val);
	if (ret)
		return;
	v64 = (v64 << 32) | val;
	printf("Number of bytes received: %"PRIu64"\n", v64);

	ret = he_hssi_indirect_read(ctx, TM_AVST_RX_ERR, &val);
	if (ret)
		return;
	if (val & ERR_VALID) {
		printf("AVST rx error:");
		if (val & OVERFLOW_ERR)
			printf(" overflow");
		if (val & LENGTH_ERR)
			printf(" length");
		if (val & OVERSIZE_ERR)
			printf(" oversize");
		if (val & UNDERSIZE_ERR)
			printf(" undersize");
		if (val & MAC_CRC_ERR)
			printf(" crc");
		if (val & PHY_ERR)
			printf(" phy");
		printf("\n");
	}

	ret = he_hssi_indirect_read(ctx, LOOPBACK_FIFO_STATUS, &val);
	if (ret)
		return;
	if (val & (ALMOST_EMPTY | ALMOST_FULL)) {
		printf("FIFO status:");
		if (val & ALMOST_EMPTY)
			printf(" almost empty");
		if (val & ALMOST_FULL)
			printf(" almost full");
		printf("\n");
	}
}

static int he_hssi_test(struct afu_rawdev *dev)
{
	struct he_hssi_priv *priv = NULL;
	struct rte_pmd_afu_he_hssi_cfg *cfg = NULL;
	struct he_hssi_ctx *ctx = NULL;
	struct traffic_ctrl_ch_sel sel;
	uint32_t val = 0;
	uint32_t i = 0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	priv = (struct he_hssi_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	cfg = &priv->he_hssi_cfg;
	ctx = &priv->he_hssi_ctx;

	ret = he_hssi_indirect_write(ctx, TG_STOP_XFR, 0);
	if (ret)
		return ret;

	sel.channel_sel = cfg->port;
	rte_write64(sel.csr, ctx->addr + TRAFFIC_CTRL_CH_SEL);

	if (cfg->he_loopback >= 0) {
		val = cfg->he_loopback ? 1 : 0;
		IFPGA_RAWDEV_PMD_INFO("%s HE loopback on port %u",
			val ? "Enable" : "Disable", cfg->port);
		return he_hssi_indirect_write(ctx, LOOPBACK_EN, val);
	}

	ret = he_hssi_indirect_write(ctx, TG_NUM_PKT, cfg->num_packets);
	if (ret)
		return ret;

	ret = he_hssi_indirect_write(ctx, TG_PKT_LEN, cfg->packet_length);
	if (ret)
		return ret;

	val = cfg->src_addr & 0xffffffff;
	ret = he_hssi_indirect_write(ctx, TG_SRC_MAC_L, val);
	if (ret)
		return ret;
	val = (cfg->src_addr >> 32) & 0xffff;
	ret = he_hssi_indirect_write(ctx, TG_SRC_MAC_H, val);
	if (ret)
		return ret;

	val = cfg->dest_addr & 0xffffffff;
	ret = he_hssi_indirect_write(ctx, TG_DST_MAC_L, val);
	if (ret)
		return ret;
	val = (cfg->dest_addr >> 32) & 0xffff;
	ret = he_hssi_indirect_write(ctx, TG_DST_MAC_H, val);
	if (ret)
		return ret;

	val = cfg->random_length ? 1 : 0;
	ret = he_hssi_indirect_write(ctx, TG_PKT_LEN_TYPE, val);
	if (ret)
		return ret;

	val = cfg->random_payload ? 1 : 0;
	ret = he_hssi_indirect_write(ctx, TG_DATA_PATTERN, val);
	if (ret)
		return ret;

	for (i = 0; i < TG_NUM_RND_SEEDS; i++) {
		ret = he_hssi_indirect_write(ctx, TG_RANDOM_SEED(i),
			cfg->rnd_seed[i]);
		if (ret)
			return ret;
	}

	ret = he_hssi_indirect_write(ctx, TG_START_XFR, 1);
	if (ret)
		return ret;

	while (i++ < cfg->timeout) {
		ret = he_hssi_indirect_read(ctx, TG_PKT_XFRD, &val);
		if (ret)
			break;
		if (val == cfg->num_packets)
			break;
		sleep(1);
	}

	he_hssi_report(ctx);

	return ret;
}

static int he_hssi_init(struct afu_rawdev *dev)
{
	struct he_hssi_priv *priv = NULL;
	struct he_hssi_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct he_hssi_priv *)dev->priv;
	if (!priv) {
		priv = rte_zmalloc(NULL, sizeof(struct he_hssi_priv), 0);
		if (!priv)
			return -ENOMEM;
		dev->priv = priv;
	}

	ctx = &priv->he_hssi_ctx;
	ctx->addr = (uint8_t *)dev->addr;

	return 0;
}

static int he_hssi_config(struct afu_rawdev *dev, void *config,
	size_t config_size)
{
	struct he_hssi_priv *priv = NULL;
	struct rte_pmd_afu_he_hssi_cfg *cfg = NULL;

	if (!dev || !config || !config_size)
		return -EINVAL;

	priv = (struct he_hssi_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (config_size != sizeof(struct rte_pmd_afu_he_hssi_cfg))
		return -EINVAL;

	cfg = (struct rte_pmd_afu_he_hssi_cfg *)config;
	if (cfg->port >= NUM_HE_HSSI_PORTS)
		return -EINVAL;

	rte_memcpy(&priv->he_hssi_cfg, cfg, sizeof(priv->he_hssi_cfg));

	return 0;
}

static int he_hssi_close(struct afu_rawdev *dev)
{
	if (!dev)
		return -EINVAL;

	rte_free(dev->priv);
	dev->priv = NULL;

	return 0;
}

static int he_hssi_dump(struct afu_rawdev *dev, FILE *f)
{
	struct he_hssi_priv *priv = NULL;
	struct he_hssi_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct he_hssi_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (!f)
		f = stdout;

	ctx = &priv->he_hssi_ctx;

	fprintf(f, "addr:\t\t%p\n", (void *)ctx->addr);

	return 0;
}

static struct afu_ops he_hssi_ops = {
	.init = he_hssi_init,
	.config = he_hssi_config,
	.start = NULL,
	.stop = NULL,
	.test = he_hssi_test,
	.close = he_hssi_close,
	.dump = he_hssi_dump,
	.reset = NULL
};

struct afu_rawdev_drv he_hssi_drv = {
	.uuid = { HE_HSSI_UUID_L, HE_HSSI_UUID_H },
	.ops = &he_hssi_ops
};

AFU_PMD_REGISTER(he_hssi_drv);
