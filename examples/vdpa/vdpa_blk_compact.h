/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _VDPA_BLK_COMPACT_H_
#define _VDPA_BLK_COMPACT_H_

#include <rte_vhost.h>

/* Feature bits */
#define VIRTIO_BLK_F_SIZE_MAX     1    /* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX      2    /* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY     4    /* Legacy geometry available  */
#define VIRTIO_BLK_F_BLK_SIZE     6    /* Block size of disk is available */
#define VIRTIO_BLK_F_TOPOLOGY     10   /* Topology information is available */
#define VIRTIO_BLK_F_MQ           12   /* support more than one vq */

/* Legacy feature bits */
#define VIRTIO_BLK_F_BARRIER      0    /* Does host support barriers? */
#define VIRTIO_BLK_F_SCSI         7    /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE   11   /* Writeback mode available in config */

#define VHOST_BLK_FEATURES_BASE ((1ULL << VHOST_F_LOG_ALL) | \
	(1ULL << VIRTIO_F_NOTIFY_ON_EMPTY) | \
	(1ULL << VIRTIO_RING_F_INDIRECT_DESC) | \
	(1ULL << VIRTIO_RING_F_EVENT_IDX) | \
	(1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
	(1ULL << VIRTIO_F_VERSION_1))

#define VHOST_BLK_DISABLED_FEATURES_BASE ((1ULL << VIRTIO_F_NOTIFY_ON_EMPTY) | \
	(1ULL << VIRTIO_RING_F_EVENT_IDX))

#define VHOST_BLK_FEATURES (VHOST_BLK_FEATURES_BASE | \
	(1ULL << VIRTIO_BLK_F_SIZE_MAX) | (1ULL << VIRTIO_BLK_F_SEG_MAX) | \
	(1ULL << VIRTIO_BLK_F_GEOMETRY) | (1ULL << VIRTIO_BLK_F_BLK_SIZE) | \
	(1ULL << VIRTIO_BLK_F_TOPOLOGY) | (1ULL << VIRTIO_BLK_F_BARRIER)  | \
	(1ULL << VIRTIO_BLK_F_SCSI)     | (1ULL << VIRTIO_BLK_F_CONFIG_WCE) | \
	(1ULL << VIRTIO_BLK_F_MQ))

/* Not supported features */
#define VHOST_BLK_DISABLED_FEATURES (VHOST_BLK_DISABLED_FEATURES_BASE | \
	(1ULL << VIRTIO_BLK_F_GEOMETRY) | (1ULL << VIRTIO_BLK_F_BARRIER) | \
	(1ULL << VIRTIO_BLK_F_SCSI)  | (1ULL << VIRTIO_BLK_F_CONFIG_WCE))

/* Vhost-blk support protocol features */
#define VHOST_BLK_PROTOCOL_FEATURES \
	((1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
	(1ULL << VHOST_USER_PROTOCOL_F_CONFIG))

#endif /* _VDPA_BLK_COMPACT_H_ */
