/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

/**
 * @file Header file containing DPDK compilation parameters
 *
 * Header file containing DPDK compilation parameters. Also include the
 * meson-generated header file containing the detected parameters that
 * are variable across builds or build environments.
 */
#ifndef _RTE_CONFIG_H_
#define _RTE_CONFIG_H_

#include <rte_build_config.h>

/* legacy defines */
#ifdef RTE_EXEC_ENV_LINUX
#define RTE_EXEC_ENV_LINUXAPP 1
#endif
#ifdef RTE_EXEC_ENV_FREEBSD
#define RTE_EXEC_ENV_BSDAPP 1
#endif

/* String that appears before the version number */
#define RTE_VER_PREFIX "DPDK"

/****** library defines ********/

/* EAL defines */
#define RTE_CACHE_GUARD_LINES 1
#define RTE_MAX_HEAPS 32
#define RTE_MAX_MEMSEG_LISTS 128
#define RTE_MAX_MEMSEG_PER_LIST 8192
#define RTE_MAX_MEM_MB_PER_LIST 32768
#define RTE_MAX_MEMSEG_PER_TYPE 32768
#define RTE_MAX_MEM_MB_PER_TYPE 65536
#define RTE_MAX_TAILQ 32
#define RTE_LOG_DP_LEVEL RTE_LOG_INFO
#define RTE_MAX_VFIO_CONTAINERS 64

/* bsd module defines */
#define RTE_CONTIGMEM_MAX_NUM_BUFS 64
#define RTE_CONTIGMEM_DEFAULT_NUM_BUFS 1
#define RTE_CONTIGMEM_DEFAULT_BUF_SIZE (512*1024*1024)

/* mempool defines */
#define RTE_MEMPOOL_CACHE_MAX_SIZE 512
/* RTE_LIBRTE_MEMPOOL_STATS is not set */
/* RTE_LIBRTE_MEMPOOL_DEBUG is not set */

/* mbuf defines */
#define RTE_MBUF_DEFAULT_MEMPOOL_OPS "ring_mp_mc"
#define RTE_PKTMBUF_HEADROOM 128

/* ether defines */
#define RTE_MAX_QUEUES_PER_PORT 1024
#define RTE_ETHDEV_QUEUE_STAT_CNTRS 16 /* max 256 */
#define RTE_ETHDEV_RXTX_CALLBACKS 1
#define RTE_MAX_MULTI_HOST_CTRLS 4

/* cryptodev defines */
#define RTE_CRYPTO_MAX_DEVS 64
#define RTE_CRYPTODEV_NAME_LEN 64
#define RTE_CRYPTO_CALLBACKS 1

/* compressdev defines */
#define RTE_COMPRESS_MAX_DEVS 64

/* regexdev defines */
#define RTE_MAX_REGEXDEV_DEVS 32

/* eventdev defines */
#define RTE_EVENT_MAX_DEVS 16
#define RTE_EVENT_MAX_PORTS_PER_DEV 255
#define RTE_EVENT_MAX_QUEUES_PER_DEV 255
#define RTE_EVENT_MAX_PROFILES_PER_PORT 8
#define RTE_EVENT_TIMER_ADAPTER_NUM_MAX 32
#define RTE_EVENT_ETH_INTR_RING_SIZE 1024
#define RTE_EVENT_CRYPTO_ADAPTER_MAX_INSTANCE 32
#define RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE 32
#define RTE_EVENT_DMA_ADAPTER_MAX_INSTANCE 32

/* rawdev defines */
#define RTE_RAWDEV_MAX_DEVS 64

/* ip_fragmentation defines */
#define RTE_LIBRTE_IP_FRAG_MAX_FRAG 8
// RTE_LIBRTE_IP_FRAG_TBL_STAT is not set

/* rte_power defines */
#define RTE_MAX_LCORE_FREQS 64
#define RTE_MAX_UNCORE_FREQS 64

/* rte_graph defines */
#define RTE_GRAPH_BURST_SIZE 256
#define RTE_LIBRTE_GRAPH_STATS 1

/****** driver defines ********/

/* Packet prefetching in PMDs */
#define RTE_PMD_PACKET_PREFETCH 1

/* QuickAssist device */
/* Max. number of QuickAssist devices which can be attached */
#define RTE_PMD_QAT_MAX_PCI_DEVICES 48
#define RTE_PMD_QAT_COMP_SGL_MAX_SEGMENTS 16
#define RTE_PMD_QAT_COMP_IM_BUFFER_SIZE 65536

/* virtio crypto defines */
#define RTE_MAX_VIRTIO_CRYPTO 32

/* DPAA SEC max cryptodev devices*/
#define RTE_LIBRTE_DPAA_MAX_CRYPTODEV	4

/* fm10k defines */
#define RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE 1

/* hns3 defines */
#define RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF 256

/* i40e defines */
#define RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC 1
// RTE_LIBRTE_I40E_16BYTE_RX_DESC is not set
#define RTE_LIBRTE_I40E_QUEUE_NUM_PER_PF 64
#define RTE_LIBRTE_I40E_QUEUE_NUM_PER_VF 4
#define RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM 4

/* Ring net PMD settings */
#define RTE_PMD_RING_MAX_RX_RINGS 16
#define RTE_PMD_RING_MAX_TX_RINGS 16

/* QEDE PMD defines */
#define RTE_LIBRTE_QEDE_FW ""

/* DLB2 defines */
// RTE_LIBRTE_PMD_DLB2_QUELL_STATS is not set

#endif /* _RTE_CONFIG_H_ */
