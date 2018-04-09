/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 */

/* upt1_defs.h
 *
 *      Definitions for UPTv1
 *
 *      Some of the defs are duplicated in vmkapi_net_upt.h, because
 *      vmkapi_net_upt.h cannot distribute with OSS yet and vmkapi headers can
 *      only include vmkapi headers. Make sure they are kept in sync!
 */

#ifndef _UPT1_DEFS_H
#define _UPT1_DEFS_H

#define UPT1_MAX_TX_QUEUES  64
#define UPT1_MAX_RX_QUEUES  64

#define UPT1_MAX_INTRS  (UPT1_MAX_TX_QUEUES + UPT1_MAX_RX_QUEUES)

typedef
#include "vmware_pack_begin.h"
struct UPT1_TxStats {
   uint64 TSOPktsTxOK;  /* TSO pkts post-segmentation */
   uint64 TSOBytesTxOK;
   uint64 ucastPktsTxOK;
   uint64 ucastBytesTxOK;
   uint64 mcastPktsTxOK;
   uint64 mcastBytesTxOK;
   uint64 bcastPktsTxOK;
   uint64 bcastBytesTxOK;
   uint64 pktsTxError;
   uint64 pktsTxDiscard;
}
#include "vmware_pack_end.h"
UPT1_TxStats;

typedef
#include "vmware_pack_begin.h"
struct UPT1_RxStats {
   uint64 LROPktsRxOK;    /* LRO pkts */
   uint64 LROBytesRxOK;   /* bytes from LRO pkts */
   /* the following counters are for pkts from the wire, i.e., pre-LRO */
   uint64 ucastPktsRxOK;
   uint64 ucastBytesRxOK;
   uint64 mcastPktsRxOK;
   uint64 mcastBytesRxOK;
   uint64 bcastPktsRxOK;
   uint64 bcastBytesRxOK;
   uint64 pktsRxOutOfBuf;
   uint64 pktsRxError;
}
#include "vmware_pack_end.h"
UPT1_RxStats;

/* interrupt moderation level */
#define UPT1_IML_NONE     0 /* no interrupt moderation */
#define UPT1_IML_HIGHEST  7 /* least intr generated */
#define UPT1_IML_ADAPTIVE 8 /* adpative intr moderation */

/* values for UPT1_RSSConf.hashFunc */
#define UPT1_RSS_HASH_TYPE_NONE      0x0
#define UPT1_RSS_HASH_TYPE_IPV4      0x01
#define UPT1_RSS_HASH_TYPE_TCP_IPV4  0x02
#define UPT1_RSS_HASH_TYPE_IPV6      0x04
#define UPT1_RSS_HASH_TYPE_TCP_IPV6  0x08

#define UPT1_RSS_HASH_FUNC_NONE      0x0
#define UPT1_RSS_HASH_FUNC_TOEPLITZ  0x01

#define UPT1_RSS_MAX_KEY_SIZE        40
#define UPT1_RSS_MAX_IND_TABLE_SIZE  128

typedef
#include "vmware_pack_begin.h"
struct UPT1_RSSConf {
   uint16   hashType;
   uint16   hashFunc;
   uint16   hashKeySize;
   uint16   indTableSize;
   uint8    hashKey[UPT1_RSS_MAX_KEY_SIZE];
   uint8    indTable[UPT1_RSS_MAX_IND_TABLE_SIZE];
}
#include "vmware_pack_end.h"
UPT1_RSSConf;

/* features */
#define UPT1_F_RXCSUM      0x0001   /* rx csum verification */
#define UPT1_F_RSS         0x0002
#define UPT1_F_RXVLAN      0x0004   /* VLAN tag stripping */
#define UPT1_F_LRO         0x0008

#endif
