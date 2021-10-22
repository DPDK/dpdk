/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <inttypes.h>
#include <math.h>

#include "cnxk_ethdev.h"

static int
parse_outb_nb_desc(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_outb_nb_crypto_qs(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val < 1 || val > 64)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_ipsec_in_max_spi(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_ipsec_out_max_sa(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_flow_max_priority(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint16_t val;

	val = atoi(value);

	/* Limit the max priority to 32 */
	if (val < 1 || val > 32)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_flow_prealloc_size(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint16_t val;

	val = atoi(value);

	/* Limit the prealloc size to 32 */
	if (val < 1 || val > 32)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_reta_size(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val <= RTE_ETH_RSS_RETA_SIZE_64)
		val = ROC_NIX_RSS_RETA_SZ_64;
	else if (val > RTE_ETH_RSS_RETA_SIZE_64 && val <= RTE_ETH_RSS_RETA_SIZE_128)
		val = ROC_NIX_RSS_RETA_SZ_128;
	else if (val > RTE_ETH_RSS_RETA_SIZE_128 && val <= RTE_ETH_RSS_RETA_SIZE_256)
		val = ROC_NIX_RSS_RETA_SZ_256;
	else
		val = ROC_NIX_RSS_RETA_SZ_64;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_flag(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);

	*(uint16_t *)extra_args = atoi(value);

	return 0;
}

static int
parse_sqb_count(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_switch_header_type(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);

	if (strcmp(value, "higig2") == 0)
		*(uint16_t *)extra_args = ROC_PRIV_FLAGS_HIGIG;

	if (strcmp(value, "dsa") == 0)
		*(uint16_t *)extra_args = ROC_PRIV_FLAGS_EDSA;

	if (strcmp(value, "chlen90b") == 0)
		*(uint16_t *)extra_args = ROC_PRIV_FLAGS_LEN_90B;

	if (strcmp(value, "exdsa") == 0)
		*(uint16_t *)extra_args = ROC_PRIV_FLAGS_EXDSA;

	if (strcmp(value, "vlan_exdsa") == 0)
		*(uint16_t *)extra_args = ROC_PRIV_FLAGS_VLAN_EXDSA;

	return 0;
}

#define CNXK_RSS_RETA_SIZE	"reta_size"
#define CNXK_SCL_ENABLE		"scalar_enable"
#define CNXK_MAX_SQB_COUNT	"max_sqb_count"
#define CNXK_FLOW_PREALLOC_SIZE "flow_prealloc_size"
#define CNXK_FLOW_MAX_PRIORITY	"flow_max_priority"
#define CNXK_SWITCH_HEADER_TYPE "switch_header"
#define CNXK_RSS_TAG_AS_XOR	"tag_as_xor"
#define CNXK_LOCK_RX_CTX	"lock_rx_ctx"
#define CNXK_IPSEC_IN_MAX_SPI	"ipsec_in_max_spi"
#define CNXK_IPSEC_OUT_MAX_SA	"ipsec_out_max_sa"
#define CNXK_OUTB_NB_DESC	"outb_nb_desc"
#define CNXK_FORCE_INB_INL_DEV	"force_inb_inl_dev"
#define CNXK_OUTB_NB_CRYPTO_QS	"outb_nb_crypto_qs"

int
cnxk_ethdev_parse_devargs(struct rte_devargs *devargs, struct cnxk_eth_dev *dev)
{
	uint16_t reta_sz = ROC_NIX_RSS_RETA_SZ_64;
	uint16_t sqb_count = CNXK_NIX_TX_MAX_SQB;
	uint16_t ipsec_in_max_spi = BIT(8) - 1;
	uint16_t ipsec_out_max_sa = BIT(12);
	uint16_t flow_prealloc_size = 1;
	uint16_t switch_header_type = 0;
	uint16_t flow_max_priority = 3;
	uint16_t force_inb_inl_dev = 0;
	uint16_t outb_nb_crypto_qs = 1;
	uint16_t outb_nb_desc = 8200;
	uint16_t rss_tag_as_xor = 0;
	uint16_t scalar_enable = 0;
	uint8_t lock_rx_ctx = 0;
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		goto null_devargs;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rte_kvargs_process(kvlist, CNXK_RSS_RETA_SIZE, &parse_reta_size,
			   &reta_sz);
	rte_kvargs_process(kvlist, CNXK_SCL_ENABLE, &parse_flag,
			   &scalar_enable);
	rte_kvargs_process(kvlist, CNXK_MAX_SQB_COUNT, &parse_sqb_count,
			   &sqb_count);
	rte_kvargs_process(kvlist, CNXK_FLOW_PREALLOC_SIZE,
			   &parse_flow_prealloc_size, &flow_prealloc_size);
	rte_kvargs_process(kvlist, CNXK_FLOW_MAX_PRIORITY,
			   &parse_flow_max_priority, &flow_max_priority);
	rte_kvargs_process(kvlist, CNXK_SWITCH_HEADER_TYPE,
			   &parse_switch_header_type, &switch_header_type);
	rte_kvargs_process(kvlist, CNXK_RSS_TAG_AS_XOR, &parse_flag,
			   &rss_tag_as_xor);
	rte_kvargs_process(kvlist, CNXK_LOCK_RX_CTX, &parse_flag, &lock_rx_ctx);
	rte_kvargs_process(kvlist, CNXK_IPSEC_IN_MAX_SPI,
			   &parse_ipsec_in_max_spi, &ipsec_in_max_spi);
	rte_kvargs_process(kvlist, CNXK_IPSEC_OUT_MAX_SA,
			   &parse_ipsec_out_max_sa, &ipsec_out_max_sa);
	rte_kvargs_process(kvlist, CNXK_OUTB_NB_DESC, &parse_outb_nb_desc,
			   &outb_nb_desc);
	rte_kvargs_process(kvlist, CNXK_OUTB_NB_CRYPTO_QS,
			   &parse_outb_nb_crypto_qs, &outb_nb_crypto_qs);
	rte_kvargs_process(kvlist, CNXK_FORCE_INB_INL_DEV, &parse_flag,
			   &force_inb_inl_dev);
	rte_kvargs_free(kvlist);

null_devargs:
	dev->scalar_ena = !!scalar_enable;
	dev->inb.force_inl_dev = !!force_inb_inl_dev;
	dev->inb.max_spi = ipsec_in_max_spi;
	dev->outb.max_sa = ipsec_out_max_sa;
	dev->outb.nb_desc = outb_nb_desc;
	dev->outb.nb_crypto_qs = outb_nb_crypto_qs;
	dev->nix.ipsec_in_max_spi = ipsec_in_max_spi;
	dev->nix.ipsec_out_max_sa = ipsec_out_max_sa;
	dev->nix.rss_tag_as_xor = !!rss_tag_as_xor;
	dev->nix.max_sqb_count = sqb_count;
	dev->nix.reta_sz = reta_sz;
	dev->nix.lock_rx_ctx = lock_rx_ctx;
	dev->npc.flow_prealloc_size = flow_prealloc_size;
	dev->npc.flow_max_priority = flow_max_priority;
	dev->npc.switch_header_type = switch_header_type;
	return 0;

exit:
	return -EINVAL;
}

RTE_PMD_REGISTER_PARAM_STRING(net_cnxk,
			      CNXK_RSS_RETA_SIZE "=<64|128|256>"
			      CNXK_SCL_ENABLE "=1"
			      CNXK_MAX_SQB_COUNT "=<8-512>"
			      CNXK_FLOW_PREALLOC_SIZE "=<1-32>"
			      CNXK_FLOW_MAX_PRIORITY "=<1-32>"
			      CNXK_SWITCH_HEADER_TYPE "=<higig2|dsa|chlen90b>"
			      CNXK_RSS_TAG_AS_XOR "=1"
			      CNXK_IPSEC_IN_MAX_SPI "=<1-65535>"
			      CNXK_OUTB_NB_DESC "=<1-65535>"
			      CNXK_OUTB_NB_CRYPTO_QS "=<1-64>"
			      CNXK_FORCE_INB_INL_DEV "=1");
