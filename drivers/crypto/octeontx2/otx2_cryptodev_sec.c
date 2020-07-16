/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "otx2_cryptodev_sec.h"

static struct rte_security_ops otx2_crypto_sec_ops = {
	.session_create		= NULL,
	.session_destroy	= NULL,
	.session_get_size	= NULL,
	.set_pkt_metadata	= NULL,
	.get_userdata		= NULL,
	.capabilities_get	= NULL
};

int
otx2_crypto_sec_ctx_create(struct rte_cryptodev *cdev)
{
	struct rte_security_ctx *ctx;

	ctx = rte_malloc("otx2_cpt_dev_sec_ctx",
			 sizeof(struct rte_security_ctx), 0);

	if (ctx == NULL)
		return -ENOMEM;

	/* Populate ctx */
	ctx->device = cdev;
	ctx->ops = &otx2_crypto_sec_ops;
	ctx->sess_cnt = 0;

	cdev->security_ctx = ctx;

	return 0;
}

void
otx2_crypto_sec_ctx_destroy(struct rte_cryptodev *cdev)
{
	rte_free(cdev->security_ctx);
}
