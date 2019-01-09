/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_TSO_H
#define _SFC_TSO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Standard TSO header length */
#define SFC_TSOH_STD_LEN	256

/** The number of TSO option descriptors that precede the packet descriptors */
#define SFC_TSO_OPT_DESCS_NUM	2

/**
 * The number of DMA descriptors for TSO header that may or may not precede the
 * packet's payload descriptors
 */
#define SFC_TSO_HDR_DESCS_NUM	1

unsigned int sfc_tso_prepare_header(uint8_t *tsoh, size_t header_len,
				    struct rte_mbuf **in_seg, size_t *in_off);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_TSO_H */
