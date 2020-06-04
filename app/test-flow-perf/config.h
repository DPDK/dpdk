/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#define GET_RSS_HF() (ETH_RSS_IP | ETH_RSS_TCP)

/* Configuration */
#define RXQ_NUM 4
#define TXQ_NUM 4
#define TOTAL_MBUF_NUM 32000
#define MBUF_SIZE 2048
#define MBUF_CACHE_SIZE 512
#define NR_RXD  256
#define NR_TXD  256
