/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_CRYPTODEV_SEC_H__
#define __CNXK_CRYPTODEV_SEC_H__

#include <rte_cryptodev.h>

int cnxk_crypto_sec_ctx_create(struct rte_cryptodev *crypto_dev);

void cnxk_crypto_sec_ctx_destroy(struct rte_cryptodev *crypto_dev);

#endif /* __CNXK_CRYPTODEV_SEC_H__ */
