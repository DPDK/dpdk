/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef _SFC_MAE_CONNTRACK_H
#define _SFC_MAE_CONNTRACK_H

#include <stdbool.h>

#include <rte_ip.h>

#include "efx.h"

#include "sfc_tbls.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sfc_mae_conntrack_key_s {
	uint8_t		ip_proto;
	uint16_t	ether_type_le;

	uint16_t	src_port_le;
	uint16_t	dst_port_le;

	uint8_t		src_addr_le[RTE_SIZEOF_FIELD(struct rte_ipv6_hdr, src_addr)];
	uint8_t		dst_addr_le[RTE_SIZEOF_FIELD(struct rte_ipv6_hdr, dst_addr)];
} sfc_mae_conntrack_key_t;

typedef struct sfc_mae_conntrack_nat_s {
	uint32_t	ip_le;
	uint16_t	port_le;
	bool		dir_is_dst;
} sfc_mae_conntrack_nat_t;

typedef struct sfc_mae_conntrack_response_s {
	uint32_t		ct_mark;
	sfc_mae_conntrack_nat_t	nat;
	uint32_t		counter_id;
} sfc_mae_conntrack_response_t;

struct sfc_adapter;

static inline bool
sfc_mae_conntrack_is_supported(struct sfc_adapter *sa)
{
	return sfc_tbls_id_is_supported(sa, EFX_TABLE_ID_CONNTRACK);
}

static inline const struct sfc_tbl_meta *
sfc_mae_conntrack_meta_lookup(struct sfc_adapter *sa)
{
	return sfc_tbl_meta_lookup(sa, EFX_TABLE_ID_CONNTRACK);
}

int sfc_mae_conntrack_insert(struct sfc_adapter *sa,
			     const sfc_mae_conntrack_key_t *key,
			     const sfc_mae_conntrack_response_t *response);

int sfc_mae_conntrack_delete(struct sfc_adapter *sa,
			     const sfc_mae_conntrack_key_t *key);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_MAE_CONNTRACK_H */
