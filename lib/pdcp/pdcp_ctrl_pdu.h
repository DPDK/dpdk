/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_CTRL_PDU_H
#define PDCP_CTRL_PDU_H

#include <rte_mbuf.h>

#include "pdcp_entity.h"

int
pdcp_ctrl_pdu_status_gen(struct entity_priv *en_priv, struct entity_priv_dl_part *dl,
			 struct rte_mbuf *m);

#endif /* PDCP_CTRL_PDU_H */
