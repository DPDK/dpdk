/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_security.h>

#include "otx2_ethdev_sec.h"

int
otx2_eth_sec_ctx_create(struct rte_eth_dev *eth_dev)
{
	struct rte_security_ctx *ctx;

	ctx = rte_malloc("otx2_eth_sec_ctx",
			 sizeof(struct rte_security_ctx), 0);
	if (ctx == NULL)
		return -ENOMEM;

	/* Populate ctx */

	ctx->device = eth_dev;
	ctx->sess_cnt = 0;

	eth_dev->security_ctx = ctx;

	return 0;
}

void
otx2_eth_sec_ctx_destroy(struct rte_eth_dev *eth_dev)
{
	rte_free(eth_dev->security_ctx);
}
