/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_byteorder.h>
#include <rte_mbuf.h>
#include <rte_pdcp_hdr.h>

#include "pdcp_ctrl_pdu.h"
#include "pdcp_entity.h"
#include "pdcp_cnt.h"

static inline uint16_t
round_up_bits(uint32_t bits)
{
	/* round up to the next multiple of 8 */
	return RTE_ALIGN_MUL_CEIL(bits, 8) / 8;
}

static __rte_always_inline void
pdcp_hdr_fill(struct rte_pdcp_up_ctrl_pdu_hdr *pdu_hdr, uint32_t rx_deliv)
{
	pdu_hdr->d_c = RTE_PDCP_PDU_TYPE_CTRL;
	pdu_hdr->pdu_type = RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT;
	pdu_hdr->r = 0;
	pdu_hdr->fmc = rte_cpu_to_be_32(rx_deliv);
}

int
pdcp_ctrl_pdu_status_gen(struct entity_priv *en_priv, struct entity_priv_dl_part *dl,
			 struct rte_mbuf *m)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *pdu_hdr;
	uint32_t rx_deliv, actual_sz;
	uint16_t pdu_sz, bitmap_sz;
	uint8_t *data;

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

	actual_sz = RTE_MIN(round_up_bits(en_priv->state.rx_next - rx_deliv - 1),
			RTE_PDCP_CTRL_PDU_SIZE_MAX - pdu_sz);
	bitmap_sz = pdcp_cnt_get_bitmap_size(actual_sz);

	data = (uint8_t *)rte_pktmbuf_append(m, pdu_sz + bitmap_sz);
	if (data == NULL)
		return -ENOMEM;

	m->pkt_len = pdu_sz + actual_sz;
	m->data_len = pdu_sz + actual_sz;

	pdcp_hdr_fill((struct rte_pdcp_up_ctrl_pdu_hdr *)data, rx_deliv);

	data = RTE_PTR_ADD(data, pdu_sz);
	pdcp_cnt_report_fill(dl->bitmap, en_priv->state, data, bitmap_sz);

	return 0;
}
