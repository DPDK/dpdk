/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_malloc.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "cnxk_cryptodev_capabilities.h"
#include "cnxk_cryptodev_sec.h"

/* Common security ops */
struct rte_security_ops cnxk_sec_ops = {
	.session_create = NULL,
	.session_destroy = NULL,
	.session_get_size = NULL,
	.set_pkt_metadata = NULL,
	.get_userdata = NULL,
	.capabilities_get = cnxk_crypto_sec_capabilities_get
};

int
cnxk_crypto_sec_ctx_create(struct rte_cryptodev *cdev)
{
	struct rte_security_ctx *ctx;

	ctx = rte_malloc("cnxk_cpt_dev_sec_ctx",
			 sizeof(struct rte_security_ctx), 0);

	if (ctx == NULL)
		return -ENOMEM;

	/* Populate ctx */
	ctx->device = cdev;
	ctx->ops = &cnxk_sec_ops;
	ctx->sess_cnt = 0;

	cdev->security_ctx = ctx;

	return 0;
}

void
cnxk_crypto_sec_ctx_destroy(struct rte_cryptodev *cdev)
{
	rte_free(cdev->security_ctx);
}
