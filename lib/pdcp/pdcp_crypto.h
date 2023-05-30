/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_CRYPTO_H
#define PDCP_CRYPTO_H

#include <rte_pdcp.h>

int pdcp_crypto_sess_create(struct rte_pdcp_entity *entity,
			    const struct rte_pdcp_entity_conf *conf);

void pdcp_crypto_sess_destroy(struct rte_pdcp_entity *entity);

#endif /* PDCP_CRYPTO_H */
