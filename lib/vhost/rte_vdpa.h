/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_VDPA_H_
#define _RTE_VDPA_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Device specific vhost lib
 */

#include <stdint.h>

/** Maximum name length for statistics counters */
#define RTE_VDPA_STATS_NAME_SIZE 64

struct rte_vdpa_device;

/**
 * A vDPA device statistic structure
 *
 * This structure is used by rte_vdpa_stats_get() to provide
 * statistics from the HW vDPA device.
 *
 * It maps a name id, corresponding to an index in the array returned
 * by rte_vdpa_get_stats_names, to a statistic value.
 */
struct rte_vdpa_stat {
	uint64_t id;        /**< The index in stats name array */
	uint64_t value;     /**< The statistic counter value */
};

/**
 * A name element for statistics
 *
 * An array of this structure is returned by rte_vdpa_get_stats_names
 * It lists the names of extended statistics for a PMD. The rte_vdpa_stat
 * structure references these names by their array index
 */
struct rte_vdpa_stat_name {
	char name[RTE_VDPA_STATS_NAME_SIZE]; /**< The statistic name */
};

/**
 * Find the device id of a vdpa device from its name
 *
 * @param name
 *  the vdpa device name
 * @return
 *  vDPA device pointer on success, NULL on failure
 */
struct rte_vdpa_device *
rte_vdpa_find_device_by_name(const char *name);

/**
 * Get the generic device from the vdpa device
 *
 * @param vdpa_dev
 *  the vdpa device pointer
 * @return
 *  generic device pointer on success, NULL on failure
 */
struct rte_device *
rte_vdpa_get_rte_device(struct rte_vdpa_device *vdpa_dev);

/**
 * Get number of queue pairs supported by the vDPA device
 *
 * @param dev
 *  vDP device pointer
 * @param queue_num
 *  pointer on where the number of queue is stored
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vdpa_get_queue_num(struct rte_vdpa_device *dev, uint32_t *queue_num);

/**
 * Get the Virtio features supported by the vDPA device
 *
 * @param dev
 *  vDP device pointer
 * @param features
 *  pointer on where the supported features are stored
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vdpa_get_features(struct rte_vdpa_device *dev, uint64_t *features);

/**
 * Get the Vhost-user protocol features supported by the vDPA device
 *
 * @param dev
 *  vDP device pointer
 * @param features
 *  pointer on where the supported protocol features are stored
 * @return
 *  0 on success, -1 on failure
 */
int
rte_vdpa_get_protocol_features(struct rte_vdpa_device *dev, uint64_t *features);

/**
 * Synchronize the used ring from mediated ring to guest, log dirty
 * page for each writeable buffer, caller should handle the used
 * ring logging before device stop.
 *
 * @param vid
 *  vhost device id
 * @param qid
 *  vhost queue id
 * @param vring_m
 *  mediated virtio ring pointer
 * @return
 *  number of synced used entries on success, -1 on failure
 */
int
rte_vdpa_relay_vring_used(int vid, uint16_t qid, void *vring_m);

/**
 * Retrieve names of statistics of a vDPA device.
 *
 * There is an assumption that 'stat_names' and 'stats' arrays are matched
 * by array index: stats_names[i].name => stats[i].value
 *
 * And the array index is same with id field of 'struct rte_vdpa_stat':
 * stats[i].id == i
 *
 * @param dev
 *  vDPA device pointer
 * @param stats_names
 *   array of at least size elements to be filled.
 *   If set to NULL, the function returns the required number of elements.
 * @param size
 *   The number of elements in stats_names array.
 * @return
 *   A negative value on error, otherwise the number of entries filled in the
 *   stats name array.
 */
int
rte_vdpa_get_stats_names(struct rte_vdpa_device *dev,
		struct rte_vdpa_stat_name *stats_names,
		unsigned int size);

/**
 * Retrieve statistics of a vDPA device.
 *
 * There is an assumption that 'stat_names' and 'stats' arrays are matched
 * by array index: stats_names[i].name => stats[i].value
 *
 * And the array index is same with id field of 'struct rte_vdpa_stat':
 * stats[i].id == i
 *
 * @param dev
 *  vDPA device pointer
 * @param qid
 *  queue id
 * @param stats
 *   A pointer to a table of structure of type rte_vdpa_stat to be filled with
 *   device statistics ids and values.
 * @param n
 *   The number of elements in stats array.
 * @return
 *   A negative value on error, otherwise the number of entries filled in the
 *   stats table.
 */
int
rte_vdpa_get_stats(struct rte_vdpa_device *dev, uint16_t qid,
		struct rte_vdpa_stat *stats, unsigned int n);
/**
 * Reset statistics of a vDPA device.
 *
 * @param dev
 *  vDPA device pointer
 * @param qid
 *  queue id
 * @return
 *   0 on success, a negative value on error.
 */
int
rte_vdpa_reset_stats(struct rte_vdpa_device *dev, uint16_t qid);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VDPA_H_ */
