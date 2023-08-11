/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Intel Corporation.
 */

#ifndef _CRYPTODEV_PMD_H_
#define _CRYPTODEV_PMD_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * RTE Crypto PMD APIs
 *
 * @note
 * These API are from crypto PMD only and user applications should not call
 * them directly.
 */

#include <string.h>

#include <dev_driver.h>
#include <rte_compat.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_common.h>

#include "rte_crypto.h"
#include "rte_cryptodev.h"


#define RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS	8

#define RTE_CRYPTODEV_PMD_NAME_ARG			("name")
#define RTE_CRYPTODEV_PMD_MAX_NB_QP_ARG			("max_nb_queue_pairs")
#define RTE_CRYPTODEV_PMD_SOCKET_ID_ARG			("socket_id")


static const char * const cryptodev_pmd_valid_params[] = {
	RTE_CRYPTODEV_PMD_NAME_ARG,
	RTE_CRYPTODEV_PMD_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_PMD_SOCKET_ID_ARG,
	NULL
};

/**
 * @internal
 * Initialisation parameters for crypto devices
 */
struct rte_cryptodev_pmd_init_params {
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	size_t private_data_size;
	int socket_id;
	unsigned int max_nb_queue_pairs;
};

/**
 * @internal
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_cryptodev_data {
	/** Device ID for this instance */
	uint8_t dev_id;
	/** Socket ID where memory is allocated */
	int socket_id;
	/** Unique identifier name */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];

	__extension__
	/** Device state: STARTED(1)/STOPPED(0) */
	uint8_t dev_started : 1;

	/** Session memory pool */
	struct rte_mempool *session_pool;
	/** Array of pointers to queue pairs. */
	void **queue_pairs;
	/** Number of device queue pairs. */
	uint16_t nb_queue_pairs;

	/** PMD-specific private data */
	void *dev_private;
} __rte_cache_aligned;

/** @internal The data structure associated with each crypto device. */
struct rte_cryptodev {
	/** Pointer to PMD dequeue function. */
	dequeue_pkt_burst_t dequeue_burst;
	/** Pointer to PMD enqueue function. */
	enqueue_pkt_burst_t enqueue_burst;

	/** Pointer to device data */
	struct rte_cryptodev_data *data;
	/** Functions exported by PMD */
	struct rte_cryptodev_ops *dev_ops;
	/** Feature flags exposes HW/SW features for the given device */
	uint64_t feature_flags;
	/** Backing device */
	struct rte_device *device;

	/** Crypto driver identifier*/
	uint8_t driver_id;

	/** User application callback for interrupts if present */
	struct rte_cryptodev_cb_list link_intr_cbs;

	/** Context for security ops */
	void *security_ctx;

	__extension__
	/** Flag indicating the device is attached */
	uint8_t attached : 1;

	/** User application callback for pre enqueue processing */
	struct rte_cryptodev_cb_rcu *enq_cbs;
	/** User application callback for post dequeue processing */
	struct rte_cryptodev_cb_rcu *deq_cbs;
} __rte_cache_aligned;

/** Global structure used for maintaining state of allocated crypto devices */
struct rte_cryptodev_global {
	struct rte_cryptodev *devs;	/**< Device information array */
	struct rte_cryptodev_data *data[RTE_CRYPTO_MAX_DEVS];
	/**< Device private data */
	uint8_t nb_devs;		/**< Number of devices found */
};

/* Cryptodev driver, containing the driver ID */
struct cryptodev_driver {
	RTE_TAILQ_ENTRY(cryptodev_driver) next; /**< Next in list. */
	const struct rte_driver *driver;
	uint8_t id;
};

/** Cryptodev symmetric crypto session
 * Each session is derived from a fixed xform chain. Therefore each session
 * has a fixed algo, key, op-type, digest_len etc.
 */
struct rte_cryptodev_sym_session {
	RTE_MARKER cacheline0;
	uint64_t opaque_data;
	/**< Can be used for external metadata */
	uint32_t sess_data_sz;
	/**< Pointer to the user data stored after sess data */
	uint16_t user_data_sz;
	/**< Session user data will be placed after sess data */
	uint8_t driver_id;
	/**< Driver id to get the session priv */
	rte_iova_t driver_priv_data_iova;
	/**< Session driver data IOVA address */

	RTE_MARKER cacheline1 __rte_cache_min_aligned;
	/**< Second cache line - start of the driver session data */
	uint8_t driver_priv_data[0];
	/**< Driver specific session data, variable size */
};

/**
 * Helper macro to get driver private data
 */
#define CRYPTODEV_GET_SYM_SESS_PRIV(s) \
	((void *)(((struct rte_cryptodev_sym_session *)s)->driver_priv_data))
#define CRYPTODEV_GET_SYM_SESS_PRIV_IOVA(s) \
	(((struct rte_cryptodev_sym_session *)s)->driver_priv_data_iova)


/**
 * Get the rte_cryptodev structure device pointer for the device. Assumes a
 * valid device index.
 *
 * @param	dev_id	Device ID value to select the device structure.
 *
 * @return
 *   - The rte_cryptodev structure pointer for the given device ID.
 */
__rte_internal
struct rte_cryptodev *
rte_cryptodev_pmd_get_dev(uint8_t dev_id);

/**
 * Get the rte_cryptodev structure device pointer for the named device.
 *
 * @param	name	device name to select the device structure.
 *
 * @return
 *   - The rte_cryptodev structure pointer for the given device ID.
 */
__rte_internal
struct rte_cryptodev *
rte_cryptodev_pmd_get_named_dev(const char *name);

/**
 * Definitions of all functions exported by a driver through the
 * generic structure of type *crypto_dev_ops* supplied in the
 * *rte_cryptodev* structure associated with a device.
 */

/**
 *	Function used to configure device.
 *
 * @param	dev	Crypto device pointer
 * @param	config	Crypto device configurations
 *
 * @return	Returns 0 on success
 */
typedef int (*cryptodev_configure_t)(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);

/**
 * Function used to start a configured device.
 *
 * @param	dev	Crypto device pointer
 *
 * @return	Returns 0 on success
 */
typedef int (*cryptodev_start_t)(struct rte_cryptodev *dev);

/**
 * Function used to stop a configured device.
 *
 * @param	dev	Crypto device pointer
 */
typedef void (*cryptodev_stop_t)(struct rte_cryptodev *dev);

/**
 * Function used to close a configured device.
 *
 * @param	dev	Crypto device pointer
 * @return
 * - 0 on success.
 * - EAGAIN if can't close as device is busy
 */
typedef int (*cryptodev_close_t)(struct rte_cryptodev *dev);


/**
 * Function used to get statistics of a device.
 *
 * @param	dev	Crypto device pointer
 * @param	stats	Pointer to crypto device stats structure to populate
 */
typedef void (*cryptodev_stats_get_t)(struct rte_cryptodev *dev,
				struct rte_cryptodev_stats *stats);


/**
 * Function used to reset statistics of a device.
 *
 * @param	dev	Crypto device pointer
 */
typedef void (*cryptodev_stats_reset_t)(struct rte_cryptodev *dev);


/**
 * Function used to get specific information of a device.
 *
 * @param	dev		Crypto device pointer
 * @param	dev_info	Pointer to infos structure to populate
 */
typedef void (*cryptodev_info_get_t)(struct rte_cryptodev *dev,
				struct rte_cryptodev_info *dev_info);

/**
 * Setup a queue pair for a device.
 *
 * @param	dev		Crypto device pointer
 * @param	qp_id		Queue Pair Index
 * @param	qp_conf		Queue configuration structure
 * @param	socket_id	Socket Index
 *
 * @return	Returns 0 on success.
 */
typedef int (*cryptodev_queue_pair_setup_t)(struct rte_cryptodev *dev,
		uint16_t qp_id,	const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id);

/**
 * Release memory resources allocated by given queue pair.
 *
 * @param	dev	Crypto device pointer
 * @param	qp_id	Queue Pair Index
 *
 * @return
 * - 0 on success.
 * - EAGAIN if can't close as device is busy
 */
typedef int (*cryptodev_queue_pair_release_t)(struct rte_cryptodev *dev,
		uint16_t qp_id);

/**
 * Create a session mempool to allocate sessions from
 *
 * @param	dev		Crypto device pointer
 * @param	nb_objs		number of sessions objects in mempool
 * @param	obj_cache_size	l-core object cache size, see *rte_ring_create*
 * @param	socket_id	Socket Id to allocate  mempool on.
 *
 * @return
 * - On success returns a pointer to a rte_mempool
 * - On failure returns a NULL pointer
 */
typedef int (*cryptodev_sym_create_session_pool_t)(
		struct rte_cryptodev *dev, unsigned nb_objs,
		unsigned obj_cache_size, int socket_id);


/**
 * Get the size of a cryptodev session
 *
 * @param	dev		Crypto device pointer
 *
 * @return
 *  - On success returns the size of the session structure for device
 *  - On failure returns 0
 */
typedef unsigned (*cryptodev_sym_get_session_private_size_t)(
		struct rte_cryptodev *dev);
/**
 * Get the size of a asymmetric cryptodev session
 *
 * @param	dev		Crypto device pointer
 *
 * @return
 *  - On success returns the size of the session structure for device
 *  - On failure returns 0
 */
typedef unsigned int (*cryptodev_asym_get_session_private_size_t)(
		struct rte_cryptodev *dev);

/**
 * Configure a Crypto session on a device.
 *
 * @param	dev		Crypto device pointer
 * @param	xform		Single or chain of crypto xforms
 * @param	session		Pointer to cryptodev's private session structure
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 *  - Returns -ENOMEM if the private session could not be allocated.
 */
typedef int (*cryptodev_sym_configure_session_t)(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *session);

/**
 * Configure a Crypto asymmetric session on a device.
 *
 * @param	dev		Crypto device pointer
 * @param	xform		Single or chain of crypto xforms
 * @param	session		Pointer to cryptodev's private session structure
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 *  - Returns -ENOMEM if the private session could not be allocated.
 */
typedef int (*cryptodev_asym_configure_session_t)(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *session);
/**
 * Free driver private session data.
 *
 * @param	dev		Crypto device pointer
 * @param	sess		Cryptodev session structure
 */
typedef void (*cryptodev_sym_free_session_t)(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess);

/**
 * Clear asymmetric session private data.
 *
 * @param	dev		Crypto device pointer
 * @param	sess		Cryptodev session structure
 */
typedef void (*cryptodev_asym_clear_session_t)(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess);
/**
 * Perform actual crypto processing (encrypt/digest or auth/decrypt)
 * on user provided data.
 *
 * @param	dev	Crypto device pointer
 * @param	sess	Cryptodev session structure
 * @param	ofs	Start and stop offsets for auth and cipher operations
 * @param	vec	Vectorized operation descriptor
 *
 * @return
 *  - Returns number of successfully processed packets.
 */
typedef uint32_t (*cryptodev_sym_cpu_crypto_process_t)
	(struct rte_cryptodev *dev, struct rte_cryptodev_sym_session *sess,
	union rte_crypto_sym_ofs ofs, struct rte_crypto_sym_vec *vec);

/**
 * Typedef that the driver provided to get service context private date size.
 *
 * @param	dev	Crypto device pointer.
 *
 * @return
 *   - On success return the size of the device's service context private data.
 *   - On failure return negative integer.
 */
typedef int (*cryptodev_sym_get_raw_dp_ctx_size_t)(struct rte_cryptodev *dev);

/**
 * Typedef that the driver provided to configure raw data-path context.
 *
 * @param	dev		Crypto device pointer.
 * @param	qp_id		Crypto device queue pair index.
 * @param	ctx		The raw data-path context data.
 * @param	sess_type	session type.
 * @param	session_ctx	Session context data. If NULL the driver
 *				shall only configure the drv_ctx_data in
 *				ctx buffer. Otherwise the driver shall only
 *				parse the session_ctx to set appropriate
 *				function pointers in ctx.
 * @param	is_update	Set 0 if it is to initialize the ctx.
 *				Set 1 if ctx is initialized and only to update
 *				session context data.
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
typedef int (*cryptodev_sym_configure_raw_dp_ctx_t)(
	struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update);

/**
 * Typedef that the driver provided to set event crypto meta data.
 *
 * @param	dev		Crypto device pointer.
 * @param	sess		Crypto or security session.
 * @param	op_type		Operation type.
 * @param	sess_type	Session type.
 * @param	ev_mdata	Pointer to the event crypto meta data
 *				(aka *union rte_event_crypto_metadata*)
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
typedef int (*cryptodev_session_event_mdata_set_t)(
	struct rte_cryptodev *dev, void *sess,
	enum rte_crypto_op_type op_type,
	enum rte_crypto_op_sess_type sess_type,
	void *ev_mdata);

/**
 * @internal Query queue pair error interrupt event.
 * @see rte_cryptodev_queue_pair_event_error_query()
 */
typedef int (*cryptodev_queue_pair_event_error_query_t)(struct rte_cryptodev *dev,
					uint16_t qp_id);

/** Crypto device operations function pointer table */
struct rte_cryptodev_ops {
	cryptodev_configure_t dev_configure;	/**< Configure device. */
	cryptodev_start_t dev_start;		/**< Start device. */
	cryptodev_stop_t dev_stop;		/**< Stop device. */
	cryptodev_close_t dev_close;		/**< Close device. */

	cryptodev_info_get_t dev_infos_get;	/**< Get device info. */

	cryptodev_stats_get_t stats_get;
	/**< Get device statistics. */
	cryptodev_stats_reset_t stats_reset;
	/**< Reset device statistics. */

	cryptodev_queue_pair_setup_t queue_pair_setup;
	/**< Set up a device queue pair. */
	cryptodev_queue_pair_release_t queue_pair_release;
	/**< Release a queue pair. */

	cryptodev_sym_get_session_private_size_t sym_session_get_size;
	/**< Return private session. */
	cryptodev_asym_get_session_private_size_t asym_session_get_size;
	/**< Return asym session private size. */
	cryptodev_sym_configure_session_t sym_session_configure;
	/**< Configure a Crypto session. */
	cryptodev_asym_configure_session_t asym_session_configure;
	/**< Configure asymmetric Crypto session. */
	cryptodev_sym_free_session_t sym_session_clear;
	/**< Clear a Crypto sessions private data. */
	cryptodev_asym_clear_session_t asym_session_clear;
	/**< Clear a Crypto sessions private data. */
	union {
		cryptodev_sym_cpu_crypto_process_t sym_cpu_process;
		/**< process input data synchronously (cpu-crypto). */
		__extension__
		struct {
			cryptodev_sym_get_raw_dp_ctx_size_t
				sym_get_raw_dp_ctx_size;
			/**< Get raw data path service context data size. */
			cryptodev_sym_configure_raw_dp_ctx_t
				sym_configure_raw_dp_ctx;
			/**< Initialize raw data path context data. */
		};
	};
	cryptodev_session_event_mdata_set_t session_ev_mdata_set;
	/**< Set a Crypto or Security session even meta data. */
	cryptodev_queue_pair_event_error_query_t queue_pair_event_error_query;
	/**< Query queue error interrupt event */
};


/**
 * Function for internal use by dummy drivers primarily, e.g. ring-based
 * driver.
 * Allocates a new cryptodev slot for an crypto device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param	name		Unique identifier name for each device
 * @param	socket_id	Socket to allocate resources on.
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
__rte_internal
struct rte_cryptodev *
rte_cryptodev_pmd_allocate(const char *name, int socket_id);

/**
 * Function for internal use by dummy drivers primarily, e.g. ring-based
 * driver.
 * Release the specified cryptodev device.
 *
 * @param cryptodev
 * The *cryptodev* pointer is the address of the *rte_cryptodev* structure.
 * @return
 *   - 0 on success, negative on error
 */
__rte_internal
int
rte_cryptodev_pmd_release_device(struct rte_cryptodev *cryptodev);


/**
 * @internal
 *
 * PMD assist function to parse initialisation arguments for crypto driver
 * when creating a new crypto PMD device instance.
 *
 * PMD should set default values for that PMD before calling function,
 * these default values will be over-written with successfully parsed values
 * from args string.
 *
 * @param	params	parsed PMD initialisation parameters
 * @param	args	input argument string to parse
 *
 * @return
 *  - 0 on success
 *  - errno on failure
 */
__rte_internal
int
rte_cryptodev_pmd_parse_input_args(
		struct rte_cryptodev_pmd_init_params *params,
		const char *args);

/**
 * @internal
 *
 * PMD assist function to provide boiler plate code for crypto driver to create
 * and allocate resources for a new crypto PMD device instance.
 *
 * @param	name	crypto device name.
 * @param	device	base device instance
 * @param	params	PMD initialisation parameters
 *
 * @return
 *  - crypto device instance on success
 *  - NULL on creation failure
 */
__rte_internal
struct rte_cryptodev *
rte_cryptodev_pmd_create(const char *name,
		struct rte_device *device,
		struct rte_cryptodev_pmd_init_params *params);

/**
 * @internal
 *
 * PMD assist function to provide boiler plate code for crypto driver to
 * destroy and free resources associated with a crypto PMD device instance.
 *
 * @param	cryptodev	crypto device handle.
 *
 * @return
 *  - 0 on success
 *  - errno on failure
 */
__rte_internal
int
rte_cryptodev_pmd_destroy(struct rte_cryptodev *cryptodev);

/**
 * Executes all the user application registered callbacks for the specific
 * device.
 *  *
 * @param	dev	Pointer to cryptodev struct
 * @param	event	Crypto device interrupt event type.
 *
 * @return
 *  void
 */
__rte_internal
void rte_cryptodev_pmd_callback_process(struct rte_cryptodev *dev,
				enum rte_cryptodev_event_type event);

/**
 * @internal
 * Create unique device name
 */
__rte_internal
int
rte_cryptodev_pmd_create_dev_name(char *name, const char *dev_name_prefix);

/**
 * @internal
 * Allocate Cryptodev driver.
 *
 * @param crypto_drv
 *   Pointer to cryptodev_driver.
 * @param drv
 *   Pointer to rte_driver.
 *
 * @return
 *  The driver type identifier
 */
__rte_internal
uint8_t rte_cryptodev_allocate_driver(struct cryptodev_driver *crypto_drv,
		const struct rte_driver *drv);

/**
 * @internal
 * This is the last step of device probing. It must be called after a
 * cryptodev is allocated and initialized successfully.
 *
 * @param	dev	Pointer to cryptodev struct
 *
 * @return
 *  void
 */
__rte_internal
void
rte_cryptodev_pmd_probing_finish(struct rte_cryptodev *dev);

#define RTE_PMD_REGISTER_CRYPTO_DRIVER(crypto_drv, drv, driver_id)\
RTE_INIT(init_ ##driver_id)\
{\
	driver_id = rte_cryptodev_allocate_driver(&crypto_drv, &(drv));\
}

/* Reset crypto device fastpath APIs to dummy values. */
__rte_internal
void
cryptodev_fp_ops_reset(struct rte_crypto_fp_ops *fp_ops);

/* Setup crypto device fastpath APIs. */
__rte_internal
void
cryptodev_fp_ops_set(struct rte_crypto_fp_ops *fp_ops,
		     const struct rte_cryptodev *dev);

/**
 * Get session event meta data (aka *union rte_event_crypto_metadata*)
 *
 * @param	op            pointer to *rte_crypto_op* structure.
 *
 * @return
 *  - On success, pointer to event crypto metadata
 *  - On failure, NULL.
 */
__rte_internal
void *
rte_cryptodev_session_event_mdata_get(struct rte_crypto_op *op);

/**
 * @internal
 * Cryptodev asymmetric crypto session.
 */
struct rte_cryptodev_asym_session {
	uint8_t driver_id;
	/**< Session driver ID. */
	uint16_t max_priv_data_sz;
	/**< Size of private data used when creating mempool */
	uint16_t user_data_sz;
	/**< Session user data will be placed after sess_data */
	uint8_t padding[3];
	void *event_mdata;
	/**< Event metadata (aka *union rte_event_crypto_metadata*) */
	uint8_t sess_private_data[];
};

#ifdef __cplusplus
}
#endif

#endif /* _CRYPTODEV_PMD_H_ */
