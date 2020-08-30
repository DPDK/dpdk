/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#define FLOW_ITEM_MASK(_x) (UINT64_C(1) << _x)
#define FLOW_ACTION_MASK(_x) (UINT64_C(1) << _x)
#define FLOW_ATTR_MASK(_x) (UINT64_C(1) << _x)
#define GET_RSS_HF() (ETH_RSS_IP | ETH_RSS_TCP)

/* Configuration */
#define RXQ_NUM 4
#define TXQ_NUM 4
#define TOTAL_MBUF_NUM 32000
#define MBUF_SIZE 2048
#define MBUF_CACHE_SIZE 512
#define NR_RXD  256
#define NR_TXD  256

/* This is used for encap/decap & header modify actions.
 * When it's 1: it means all actions have fixed values.
 * When it's 0: it means all actions will have different values.
 */
#define FIXED_VALUES 1

/* Items/Actions parameters */
#define JUMP_ACTION_TABLE 2
#define VLAN_VALUE 1
#define VNI_VALUE 1
#define META_DATA 1
#define TAG_INDEX 0
#define PORT_ID_DST 1
#define TEID_VALUE 1

/* Flow items/acctions max size */
#define MAX_ITEMS_NUM 32
#define MAX_ACTIONS_NUM 32
#define MAX_ATTRS_NUM 16
