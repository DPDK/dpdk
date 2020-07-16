/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_capabilities.h"
#include "otx2_cryptodev_sec.h"
#include "otx2_security.h"

static unsigned int
otx2_crypto_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct otx2_sec_session);
}

static int
otx2_crypto_sec_set_pkt_mdata(void *device __rte_unused,
			      struct rte_security_session *session,
			      struct rte_mbuf *m, void *params __rte_unused)
{
	/* Set security session as the pkt metadata */
	m->udata64 = (uint64_t)session;

	return 0;
}

static int
otx2_crypto_sec_get_userdata(void *device __rte_unused, uint64_t md,
			     void **userdata)
{
	/* Retrieve userdata  */
	*userdata = (void *)md;

	return 0;
}

static struct rte_security_ops otx2_crypto_sec_ops = {
	.session_create		= NULL,
	.session_destroy	= NULL,
	.session_get_size	= otx2_crypto_sec_session_get_size,
	.set_pkt_metadata	= otx2_crypto_sec_set_pkt_mdata,
	.get_userdata		= otx2_crypto_sec_get_userdata,
	.capabilities_get	= otx2_crypto_sec_capabilities_get
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
