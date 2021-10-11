/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_REPR_PROXY_H
#define _SFC_REPR_PROXY_H

#include <stdint.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of supported RxQs with different mbuf memory pools */
#define SFC_REPR_PROXY_NB_RXQ_MIN	(1)
#define SFC_REPR_PROXY_NB_RXQ_MAX	(1)

/* One TxQ is required and sufficient for port representors support */
#define SFC_REPR_PROXY_NB_TXQ_MIN	(1)
#define SFC_REPR_PROXY_NB_TXQ_MAX	(1)

struct sfc_repr_proxy_port {
	TAILQ_ENTRY(sfc_repr_proxy_port)	entries;
	uint16_t				repr_id;
	uint16_t				rte_port_id;
	efx_mport_id_t				egress_mport;
};

enum sfc_repr_proxy_mbox_op {
	SFC_REPR_PROXY_MBOX_ADD_PORT,
	SFC_REPR_PROXY_MBOX_DEL_PORT,
};

struct sfc_repr_proxy_mbox {
	struct sfc_repr_proxy_port	*port;
	enum sfc_repr_proxy_mbox_op	op;

	bool				write_marker;
	bool				ack;
};

TAILQ_HEAD(sfc_repr_proxy_ports, sfc_repr_proxy_port);

struct sfc_repr_proxy {
	uint32_t			service_core_id;
	uint32_t			service_id;
	efx_mport_id_t			mport_alias;
	struct sfc_repr_proxy_ports	ports;
	bool				started;

	struct sfc_repr_proxy_mbox	mbox;
};

struct sfc_adapter;

int sfc_repr_proxy_attach(struct sfc_adapter *sa);
void sfc_repr_proxy_pre_detach(struct sfc_adapter *sa);
void sfc_repr_proxy_detach(struct sfc_adapter *sa);
int sfc_repr_proxy_start(struct sfc_adapter *sa);
void sfc_repr_proxy_stop(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_REPR_PROXY_H */
