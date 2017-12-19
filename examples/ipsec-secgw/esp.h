/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */
#ifndef __RTE_IPSEC_XFORM_ESP_H__
#define __RTE_IPSEC_XFORM_ESP_H__

struct mbuf;


int
esp_inbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop);

int
esp_inbound_post(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop);

int
esp_outbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop);

int
esp_outbound_post(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop);

#endif /* __RTE_IPSEC_XFORM_ESP_H__ */
