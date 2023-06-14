/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef _RTE_REGEX_CORE_H_
#define _RTE_REGEX_CORE_H_

/**
 * @file
 *
 * RTE RegEx Device internal header.
 *
 * This header contains internal data types, that are used by the RegEx devices
 * in order to expose their ops to the class.
 *
 * Applications should not use these API directly.
 */

struct rte_regexdev;

typedef int (*regexdev_info_get_t)(struct rte_regexdev *dev,
				   struct rte_regexdev_info *info);
/**< @internal Get the RegEx device info. */

typedef int (*regexdev_configure_t)(struct rte_regexdev *dev,
				    const struct rte_regexdev_config *cfg);
/**< @internal Configure the RegEx device. */

typedef int (*regexdev_qp_setup_t)(struct rte_regexdev *dev, uint16_t id,
				   const struct rte_regexdev_qp_conf *qp_conf);
/**< @internal Setup a queue pair.*/

typedef int (*regexdev_start_t)(struct rte_regexdev *dev);
/**< @internal Start the RegEx device. */

typedef int (*regexdev_stop_t)(struct rte_regexdev *dev);
/**< @internal Stop the RegEx device. */

typedef int (*regexdev_close_t)(struct rte_regexdev *dev);
/**< @internal Close the RegEx device. */

typedef int (*regexdev_attr_get_t)(struct rte_regexdev *dev,
				   enum rte_regexdev_attr_id id,
				   void *value);
/**< @internal Get selected attribute from RegEx device. */

typedef int (*regexdev_attr_set_t)(struct rte_regexdev *dev,
				   enum rte_regexdev_attr_id id,
				   const void *value);
/**< @internal Set selected attribute to RegEx device. */

typedef int (*regexdev_rule_db_update_t)(struct rte_regexdev *dev,
					 const struct rte_regexdev_rule *rules,
					 uint16_t nb_rules);
/**< @internal Update the rule database for the RegEx device. */

typedef int (*regexdev_rule_db_compile_activate_t)(struct rte_regexdev *dev);
/**< @internal Compile the rule database and activate it. */

typedef int (*regexdev_rule_db_import_t)(struct rte_regexdev *dev,
					 const  char *rule_db,
					 uint32_t rule_db_len);
/**< @internal Upload a pre created rule database to the RegEx device. */

typedef int (*regexdev_rule_db_export_t)(struct rte_regexdev *dev,
					 char *rule_db);
/**< @internal Export the current rule database from the RegEx device. */

typedef int (*regexdev_xstats_names_get_t)(struct rte_regexdev *dev,
					   struct rte_regexdev_xstats_map
					   *xstats_map);
/**< @internal Get xstats name map for the RegEx device. */

typedef int (*regexdev_xstats_get_t)(struct rte_regexdev *dev,
				     const uint16_t *ids, uint64_t *values,
				     uint16_t nb_values);
/**< @internal Get xstats values for the RegEx device. */

typedef int (*regexdev_xstats_by_name_get_t)(struct rte_regexdev *dev,
					     const char *name, uint16_t *id,
					     uint64_t *value);
/**< @internal Get xstat value for the RegEx device based on the xstats name. */

typedef int (*regexdev_xstats_reset_t)(struct rte_regexdev *dev,
				       const uint16_t *ids,
				       uint16_t nb_ids);
/**< @internal Reset xstats values for the RegEx device. */

typedef int (*regexdev_selftest_t)(struct rte_regexdev *dev);
/**< @internal Trigger RegEx self test. */

typedef int (*regexdev_dump_t)(struct rte_regexdev *dev, FILE *f);
/**< @internal Dump internal information about the RegEx device. */

typedef uint16_t (*regexdev_enqueue_t)(struct rte_regexdev *dev, uint16_t qp_id,
				       struct rte_regex_ops **ops,
				       uint16_t nb_ops);
/**< @internal Enqueue a burst of scan requests to a queue on RegEx device. */

typedef uint16_t (*regexdev_dequeue_t)(struct rte_regexdev *dev, uint16_t qp_id,
				       struct rte_regex_ops **ops,
				       uint16_t nb_ops);
/**< @internal Dequeue a burst of scan response from a queue on RegEx device. */

/**
 * RegEx device operations
 */
struct rte_regexdev_ops {
	regexdev_info_get_t dev_info_get;
	regexdev_configure_t dev_configure;
	regexdev_qp_setup_t dev_qp_setup;
	regexdev_start_t dev_start;
	regexdev_stop_t dev_stop;
	regexdev_close_t dev_close;
	regexdev_attr_get_t dev_attr_get;
	regexdev_attr_set_t dev_attr_set;
	regexdev_rule_db_update_t dev_rule_db_update;
	regexdev_rule_db_compile_activate_t dev_rule_db_compile_activate;
	regexdev_rule_db_import_t dev_db_import;
	regexdev_rule_db_export_t dev_db_export;
	regexdev_xstats_names_get_t dev_xstats_names_get;
	regexdev_xstats_get_t dev_xstats_get;
	regexdev_xstats_by_name_get_t dev_xstats_by_name_get;
	regexdev_xstats_reset_t dev_xstats_reset;
	regexdev_selftest_t dev_selftest;
	regexdev_dump_t dev_dump;
};

/**
 * Possible states of a RegEx device.
 */
enum rte_regexdev_state {
	RTE_REGEXDEV_UNUSED = 0, /**< Device is unused. */
	RTE_REGEXDEV_REGISTERED,
	/**< Device is registered, but not ready to be used. */
	RTE_REGEXDEV_READY,
	/**< Device is ready for use. This is set by the PMD. */
};

/**
 * @internal
 * The data part, with no function pointers, associated with each RegEx device.
 *
 * This structure is safe to place in shared memory to be common among different
 * processes in a multi-process configuration.
 */
struct rte_regexdev_data {
	void *dev_private; /**< PMD-specific private data. */
	char dev_name[RTE_REGEXDEV_NAME_MAX_LEN]; /**< Unique identifier name */
	uint16_t dev_id; /**< Device [external]  identifier. */
	struct rte_regexdev_config dev_conf; /**< RegEx configuration. */
	uint8_t dev_started : 1; /**< Device started to work. */
} __rte_cache_aligned;

/**
 * @internal
 * The generic data structure associated with each RegEx device.
 *
 * Pointers to burst-oriented packet receive and transmit functions are
 * located at the beginning of the structure, along with the pointer to
 * where all the data elements for the particular device are stored in shared
 * memory. This split allows the function pointer and driver data to be per-
 * process, while the actual configuration data for the device is shared.
 */
struct rte_regexdev {
	regexdev_enqueue_t enqueue;
	regexdev_dequeue_t dequeue;
	const struct rte_regexdev_ops *dev_ops;
	/**< Functions exported by PMD */
	struct rte_device *device; /**< Backing device */
	enum rte_regexdev_state state; /**< The device state. */
	struct rte_regexdev_data *data;  /**< Pointer to device data. */
} __rte_cache_aligned;

/**
 * @internal
 * The pool of *rte_regexdev* structures. The size of the pool
 * is configured at compile-time in the <rte_regexdev.c> file.
 */
extern struct rte_regexdev rte_regex_devices[];

#endif /* _RTE_REGEX_CORE_H_ */
