/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_byteorder.h>
#include <rte_mbuf.h>
#include <rte_pdcp_hdr.h>

#include "pdcp_ctrl_pdu.h"
#include "pdcp_entity.h"

static __rte_always_inline void
pdcp_hdr_fill(struct rte_pdcp_up_ctrl_pdu_hdr *pdu_hdr, uint32_t rx_deliv)
{
	pdu_hdr->d_c = RTE_PDCP_PDU_TYPE_CTRL;
	pdu_hdr->pdu_type = RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT;
	pdu_hdr->r = 0;
	pdu_hdr->fmc = rte_cpu_to_be_32(rx_deliv);
}

int
pdcp_ctrl_pdu_status_gen(struct entity_priv *en_priv, struct rte_mbuf *m)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *pdu_hdr;
	uint32_t rx_deliv;
	int pdu_sz;

	if (!en_priv->flags.is_status_report_required)
		return -EINVAL;

	pdu_sz = sizeof(struct rte_pdcp_up_ctrl_pdu_hdr);

	rx_deliv = en_priv->state.rx_deliv;

	/* Zero missing PDUs - status report contains only FMC */
	if (rx_deliv >= en_priv->state.rx_next) {
		pdu_hdr = (struct rte_pdcp_up_ctrl_pdu_hdr *)rte_pktmbuf_append(m, pdu_sz);
		if (pdu_hdr == NULL)
			return -ENOMEM;
		pdcp_hdr_fill(pdu_hdr, rx_deliv);

		return 0;
	}

	return -ENOTSUP;
}
