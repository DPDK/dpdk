/*-
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <rte_dev.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_log.h>
#include <rte_common.h>

#include "rte_crypto.h"
#include "rte_cryptodev.h"

struct rte_cryptodev_session {
	RTE_STD_C11
	struct {
		uint8_t dev_id;
		enum rte_cryptodev_type type;
		struct rte_mempool *mp;
	} __rte_aligned(8);

	__extension__ char _private[0];
};

struct rte_cryptodev_driver;

/**
 * Initialisation function of a crypto driver invoked for each matching
 * crypto PCI device detected during the PCI probing phase.
 *
 * @param	drv	The pointer to the [matching] crypto driver structure
 *			supplied by the PMD when it registered itself.
 * @param	dev	The dev pointer is the address of the *rte_cryptodev*
 *			structure associated with the matching device and which
 *			has been [automatically] allocated in the
 *			*rte_crypto_devices* array.
 *
 * @return
 *   - 0: Success, the device is properly initialised by the driver.
 *        In particular, the driver MUST have set up the *dev_ops* pointer
 *        of the *dev* structure.
 *   - <0: Error code of the device initialisation failure.
 */
typedef int (*cryptodev_init_t)(struct rte_cryptodev_driver *drv,
		struct rte_cryptodev *dev);

/**
 * Finalisation function of a driver invoked for each matching
 * PCI device detected during the PCI closing phase.
 *
 * @param	drv	The pointer to the [matching] driver structure supplied
 *			by the PMD when it registered itself.
 * @param	dev	The dev pointer is the address of the *rte_cryptodev*
 *			structure associated with the matching device and which
 *			has been [automatically] allocated in the
 *			*rte_crypto_devices* array.
 *
 *  * @return
 *   - 0: Success, the device is properly finalised by the driver.
 *        In particular, the driver MUST free the *dev_ops* pointer
 *        of the *dev* structure.
 *   - <0: Error code of the device initialisation failure.
 */
typedef int (*cryptodev_uninit_t)(const struct rte_cryptodev_driver  *drv,
				struct rte_cryptodev *dev);

/**
 * The structure associated with a PMD driver.
 *
 * Each driver acts as a PCI driver and is represented by a generic
 * *crypto_driver* structure that holds:
 *
 * - An *rte_pci_driver* structure (which must be the first field).
 *
 * - The *cryptodev_init* function invoked for each matching PCI device.
 *
 * - The size of the private data to allocate for each matching device.
 */
struct rte_cryptodev_driver {
	struct rte_pci_driver pci_drv;	/**< The PMD is also a PCI driver. */
	unsigned dev_private_size;	/**< Size of device private data. */

	cryptodev_init_t cryptodev_init;	/**< Device init function. */
	cryptodev_uninit_t cryptodev_uninit;	/**< Device uninit function. */
};


/** Global structure used for maintaining state of allocated crypto devices */
struct rte_cryptodev_global {
	struct rte_cryptodev *devs;	/**< Device information array */
	struct rte_cryptodev_data *data[RTE_CRYPTO_MAX_DEVS];
	/**< Device private data */
	uint8_t nb_devs;		/**< Number of devices found */
	uint8_t max_devs;		/**< Max number of devices */
};

/** pointer to global crypto devices data structure. */
extern struct rte_cryptodev_global *rte_cryptodev_globals;

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
 * Start queue pair of a device.
 *
 * @param	dev	Crypto device pointer
 * @param	qp_id	Queue Pair Index
 *
 * @return	Returns 0 on success.
 */
typedef int (*cryptodev_queue_pair_start_t)(struct rte_cryptodev *dev,
				uint16_t qp_id);

/**
 * Stop queue pair of a device.
 *
 * @param	dev	Crypto device pointer
 * @param	qp_id	Queue Pair Index
 *
 * @return	Returns 0 on success.
 */
typedef int (*cryptodev_queue_pair_stop_t)(struct rte_cryptodev *dev,
				uint16_t qp_id);

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
 * Initialize a Crypto session on a device.
 *
 * @param	dev		Crypto device pointer
 * @param	xform		Single or chain of crypto xforms
 * @param	priv_sess	Pointer to cryptodev's private session structure
 *
 * @return
 *  - Returns private session structure on success.
 *  - Returns NULL on failure.
 */
typedef void (*cryptodev_sym_initialize_session_t)(struct rte_mempool *mempool,
		void *session_private);

/**
 * Configure a Crypto session on a device.
 *
 * @param	dev		Crypto device pointer
 * @param	xform		Single or chain of crypto xforms
 * @param	priv_sess	Pointer to cryptodev's private session structure
 *
 * @return
 *  - Returns private session structure on success.
 *  - Returns NULL on failure.
 */
typedef void * (*cryptodev_sym_configure_session_t)(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private);

/**
 * Free Crypto session.
 * @param	session		Cryptodev session structure to free
 */
typedef void (*cryptodev_sym_free_session_t)(struct rte_cryptodev *dev,
		void *session_private);

/**
 * Optional API for drivers to attach sessions with queue pair.
 * @param	dev		Crypto device pointer
 * @param	qp_id		queue pair id for attaching session
 * @param	priv_sess       Pointer to cryptodev's private session structure
 * @return
 *  - Return 0 on success
 */
typedef int (*cryptodev_sym_queue_pair_attach_session_t)(
		  struct rte_cryptodev *dev,
		  uint16_t qp_id,
		  void *session_private);

/**
 * Optional API for drivers to detach sessions from queue pair.
 * @param	dev		Crypto device pointer
 * @param	qp_id		queue pair id for detaching session
 * @param	priv_sess       Pointer to cryptodev's private session structure
 * @return
 *  - Return 0 on success
 */
typedef int (*cryptodev_sym_queue_pair_detach_session_t)(
		  struct rte_cryptodev *dev,
		  uint16_t qp_id,
		  void *session_private);

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
	cryptodev_queue_pair_start_t queue_pair_start;
	/**< Start a queue pair. */
	cryptodev_queue_pair_stop_t queue_pair_stop;
	/**< Stop a queue pair. */
	cryptodev_queue_pair_count_t queue_pair_count;
	/**< Get count of the queue pairs. */

	cryptodev_sym_get_session_private_size_t session_get_size;
	/**< Return private session. */
	cryptodev_sym_initialize_session_t session_initialize;
	/**< Initialization function for private session data */
	cryptodev_sym_configure_session_t session_configure;
	/**< Configure a Crypto session. */
	cryptodev_sym_free_session_t session_clear;
	/**< Clear a Crypto sessions private data. */
	cryptodev_sym_queue_pair_attach_session_t qp_attach_session;
	/**< Attach session to queue pair. */
	cryptodev_sym_queue_pair_attach_session_t qp_detach_session;
	/**< Detach session from queue pair. */
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
 * Creates a new virtual crypto device and returns the pointer
 * to that device.
 *
 * @param	name			PMD type name
 * @param	dev_private_size	Size of crypto PMDs private data
 * @param	socket_id		Socket to allocate resources on.
 *
 * @return
 *   - Cryptodev pointer if device is successfully created.
 *   - NULL if device cannot be created.
 */
struct rte_cryptodev *
rte_cryptodev_pmd_virtual_dev_init(const char *name, size_t dev_private_size,
		int socket_id);


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
 * Wrapper for use by pci drivers as a .probe function to attach to a crypto
 * interface.
 */
int rte_cryptodev_pci_probe(struct rte_pci_driver *pci_drv,
			    struct rte_pci_device *pci_dev);

/**
 * Wrapper for use by pci drivers as a .remove function to detach a crypto
 * interface.
 */
int rte_cryptodev_pci_remove(struct rte_pci_device *pci_dev);

/**
 * @internal
 * Create unique device name
 */
int
rte_cryptodev_pmd_create_dev_name(char *name, const char *dev_name_prefix);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_PMD_H_ */
