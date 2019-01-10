/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation.
 */

#ifndef _RTE_CRYPTODEV_PMD_H_
#define _RTE_CRYPTODEV_PMD_H_

/** @file
 * RTE Crypto PMD APIs
 *
 * @note
 * These API are from crypto PMD only and user applications should not call
 * them directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_config.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
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
	RTE_CRYPTODEV_PMD_SOCKET_ID_ARG
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

/** Global structure used for maintaining state of allocated crypto devices */
struct rte_cryptodev_global {
	struct rte_cryptodev *devs;	/**< Device information array */
	struct rte_cryptodev_data *data[RTE_CRYPTO_MAX_DEVS];
	/**< Device private data */
	uint8_t nb_devs;		/**< Number of devices found */
	uint8_t max_devs;		/**< Max number of devices */
};

/* Cryptodev driver, containing the driver ID */
struct cryptodev_driver {
	TAILQ_ENTRY(cryptodev_driver) next; /**< Next in list. */
	const struct rte_driver *driver;
	uint8_t id;
};

/**
 * Get the rte_cryptodev structure device pointer for the device. Assumes a
 * valid device index.
 *
 * @param	dev_id	Device ID value to select the device structure.
 *
 * @return
 *   - The rte_cryptodev structure pointer for the given device ID.
 */
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
struct rte_cryptodev *
rte_cryptodev_pmd_get_named_dev(const char *name);

/**
 * Validate if the crypto device index is valid attached crypto device.
 *
 * @param	dev_id	Crypto device index.
 *
 * @return
 *   - If the device index is valid (1) or not (0).
 */
unsigned int
rte_cryptodev_pmd_is_valid_dev(uint8_t dev_id);

/**
 * The pool of rte_cryptodev structures.
 */
extern struct rte_cryptodev *rte_cryptodevs;


/**
 * Definitions of all functions exported by a driver through the
 * the generic structure of type *crypto_dev_ops* supplied in the
 * *rte_cryptodev* structure associated with a device.
 */

/**
 *	Function used to configure device.
 *
 * @param	dev	Crypto device pointer
 *		config	Crypto device configurations
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
 * @param	dev	Crypto device pointer
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
 * Get number of available queue pairs of a device.
 *
 * @param	dev	Crypto device pointer
 *
 * @return	Returns number of queue pairs on success.
 */
typedef uint32_t (*cryptodev_queue_pair_count_t)(struct rte_cryptodev *dev);

/**
 * Create a session mempool to allocate sessions from
 *
 * @param	dev		Crypto device pointer
 * @param	nb_objs		number of sessions objects in mempool
 * @param	obj_cache	l-core object cache size, see *rte_ring_create*
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
 * @param	priv_sess	Pointer to cryptodev's private session structure
 * @param	mp		Mempool where the private session is allocated
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 *  - Returns -ENOMEM if the private session could not be allocated.
 */
typedef int (*cryptodev_sym_configure_session_t)(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *session,
		struct rte_mempool *mp);
/**
 * Configure a Crypto asymmetric session on a device.
 *
 * @param	dev		Crypto device pointer
 * @param	xform		Single or chain of crypto xforms
 * @param	priv_sess	Pointer to cryptodev's private session structure
 * @param	mp		Mempool where the private session is allocated
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 *  - Returns -ENOMEM if the private session could not be allocated.
 */
typedef int (*cryptodev_asym_configure_session_t)(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *session,
		struct rte_mempool *mp);
/**
 * Free driver private session data.
 *
 * @param	dev		Crypto device pointer
 * @param	sess		Cryptodev session structure
 */
typedef void (*cryptodev_sym_free_session_t)(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess);
/**
 * Free asymmetric session private data.
 *
 * @param	dev		Crypto device pointer
 * @param	sess		Cryptodev session structure
 */
typedef void (*cryptodev_asym_free_session_t)(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess);

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
	cryptodev_queue_pair_count_t queue_pair_count;
	/**< Get count of the queue pairs. */

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
	cryptodev_asym_free_session_t asym_session_clear;
	/**< Clear a Crypto sessions private data. */
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
extern int
rte_cryptodev_pmd_release_device(struct rte_cryptodev *cryptodev);


/**
 * @internal
 *
 * PMD assist function to parse initialisation arguments for crypto driver
 * when creating a new crypto PMD device instance.
 *
 * PMD driver should set default values for that PMD before calling function,
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
void rte_cryptodev_pmd_callback_process(struct rte_cryptodev *dev,
				enum rte_cryptodev_event_type event);

/**
 * @internal
 * Create unique device name
 */
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
uint8_t rte_cryptodev_allocate_driver(struct cryptodev_driver *crypto_drv,
		const struct rte_driver *drv);


#define RTE_PMD_REGISTER_CRYPTO_DRIVER(crypto_drv, drv, driver_id)\
RTE_INIT(init_ ##driver_id)\
{\
	driver_id = rte_cryptodev_allocate_driver(&crypto_drv, &(drv));\
}

static inline void *
get_sym_session_private_data(const struct rte_cryptodev_sym_session *sess,
		uint8_t driver_id) {
	if (unlikely(sess->nb_drivers <= driver_id))
		return NULL;

	return sess->sess_data[driver_id].data;
}

static inline void
set_sym_session_private_data(struct rte_cryptodev_sym_session *sess,
		uint8_t driver_id, void *private_data)
{
	if (unlikely(sess->nb_drivers <= driver_id)) {
		CDEV_LOG_ERR("Set private data for driver %u not allowed\n",
				driver_id);
		return;
	}

	sess->sess_data[driver_id].data = private_data;
}

static inline void *
get_asym_session_private_data(const struct rte_cryptodev_asym_session *sess,
		uint8_t driver_id) {
	return sess->sess_private_data[driver_id];
}

static inline void
set_asym_session_private_data(struct rte_cryptodev_asym_session *sess,
		uint8_t driver_id, void *private_data)
{
	sess->sess_private_data[driver_id] = private_data;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_PMD_H_ */
