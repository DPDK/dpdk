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

#ifndef _RTE_CRYPTODEV_H_
#define _RTE_CRYPTODEV_H_

/**
 * @file rte_cryptodev.h
 *
 * RTE Cryptographic Device APIs
 *
 * Defines RTE Crypto Device APIs for the provisioning of cipher and
 * authentication operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_kvargs.h"
#include "rte_crypto.h"
#include "rte_dev.h"
#include <rte_common.h>

#define CRYPTODEV_NAME_NULL_PMD		crypto_null
/**< Null crypto PMD device name */
#define CRYPTODEV_NAME_AESNI_MB_PMD	crypto_aesni_mb
/**< AES-NI Multi buffer PMD device name */
#define CRYPTODEV_NAME_AESNI_GCM_PMD	crypto_aesni_gcm
/**< AES-NI GCM PMD device name */
#define CRYPTODEV_NAME_OPENSSL_PMD	crypto_openssl
/**< Open SSL Crypto PMD device name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat
/**< Intel QAT Symmetric Crypto PMD device name */
#define CRYPTODEV_NAME_SNOW3G_PMD	crypto_snow3g
/**< SNOW 3G PMD device name */
#define CRYPTODEV_NAME_KASUMI_PMD	crypto_kasumi
/**< KASUMI PMD device name */
#define CRYPTODEV_NAME_ZUC_PMD		crypto_zuc
/**< KASUMI PMD device name */

/** Crypto device type */
enum rte_cryptodev_type {
	RTE_CRYPTODEV_NULL_PMD = 1,	/**< Null crypto PMD */
	RTE_CRYPTODEV_AESNI_GCM_PMD,	/**< AES-NI GCM PMD */
	RTE_CRYPTODEV_AESNI_MB_PMD,	/**< AES-NI multi buffer PMD */
	RTE_CRYPTODEV_QAT_SYM_PMD,	/**< QAT PMD Symmetric Crypto */
	RTE_CRYPTODEV_SNOW3G_PMD,	/**< SNOW 3G PMD */
	RTE_CRYPTODEV_KASUMI_PMD,	/**< KASUMI PMD */
	RTE_CRYPTODEV_ZUC_PMD,		/**< ZUC PMD */
	RTE_CRYPTODEV_OPENSSL_PMD,    /**<  OpenSSL PMD */
};

extern const char **rte_cyptodev_names;

/* Logging Macros */

#define CDEV_LOG_ERR(...) \
	RTE_LOG(ERR, CRYPTODEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define CDEV_PMD_LOG_ERR(dev, ...) \
	RTE_LOG(ERR, CRYPTODEV, \
		RTE_FMT("[%s] %s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			dev, __func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#ifdef RTE_LIBRTE_CRYPTODEV_DEBUG
#define CDEV_LOG_DEBUG(...) \
	RTE_LOG(DEBUG, CRYPTODEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define CDEV_PMD_TRACE(...) \
	RTE_LOG(DEBUG, CRYPTODEV, \
		RTE_FMT("[%s] %s: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			dev, __func__, RTE_FMT_TAIL(__VA_ARGS__,)))

#else
#define CDEV_LOG_DEBUG(...) (void)0
#define CDEV_PMD_TRACE(...) (void)0
#endif

/**
 * Symmetric Crypto Capability
 */
struct rte_cryptodev_symmetric_capability {
	enum rte_crypto_sym_xform_type xform_type;
	/**< Transform type : Authentication / Cipher */
	RTE_STD_C11
	union {
		struct {
			enum rte_crypto_auth_algorithm algo;
			/**< authentication algorithm */
			uint16_t block_size;
			/**< algorithm block size */
			struct {
				uint16_t min;	/**< minimum key size */
				uint16_t max;	/**< maximum key size */
				uint16_t increment;
				/**< if a range of sizes are supported,
				 * this parameter is used to indicate
				 * increments in byte size that are supported
				 * between the minimum and maximum */
			} key_size;
			/**< auth key size range */
			struct {
				uint16_t min;	/**< minimum digest size */
				uint16_t max;	/**< maximum digest size */
				uint16_t increment;
				/**< if a range of sizes are supported,
				 * this parameter is used to indicate
				 * increments in byte size that are supported
				 * between the minimum and maximum */
			} digest_size;
			/**< digest size range */
			struct {
				uint16_t min;	/**< minimum aad size */
				uint16_t max;	/**< maximum aad size */
				uint16_t increment;
				/**< if a range of sizes are supported,
				 * this parameter is used to indicate
				 * increments in byte size that are supported
				 * between the minimum and maximum */
			} aad_size;
			/**< Additional authentication data size range */
		} auth;
		/**< Symmetric Authentication transform capabilities */
		struct {
			enum rte_crypto_cipher_algorithm algo;
			/**< cipher algorithm */
			uint16_t block_size;
			/**< algorithm block size */
			struct {
				uint16_t min;	/**< minimum key size */
				uint16_t max;	/**< maximum key size */
				uint16_t increment;
				/**< if a range of sizes are supported,
				 * this parameter is used to indicate
				 * increments in byte size that are supported
				 * between the minimum and maximum */
			} key_size;
			/**< cipher key size range */
			struct {
				uint16_t min;	/**< minimum iv size */
				uint16_t max;	/**< maximum iv size */
				uint16_t increment;
				/**< if a range of sizes are supported,
				 * this parameter is used to indicate
				 * increments in byte size that are supported
				 * between the minimum and maximum */
			} iv_size;
			/**< Initialisation vector data size range */
		} cipher;
		/**< Symmetric Cipher transform capabilities */
	};
};

/** Structure used to capture a capability of a crypto device */
struct rte_cryptodev_capabilities {
	enum rte_crypto_op_type op;
	/**< Operation type */

	RTE_STD_C11
	union {
		struct rte_cryptodev_symmetric_capability sym;
		/**< Symmetric operation capability parameters */
	};
};

/** Macro used at end of crypto PMD list */
#define RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST() \
	{ RTE_CRYPTO_OP_TYPE_UNDEFINED }


/**
 * Crypto device supported feature flags
 *
 * Note:
 * New features flags should be added to the end of the list
 *
 * Keep these flags synchronised with rte_cryptodev_get_feature_name()
 */
#define	RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO	(1ULL << 0)
/**< Symmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO	(1ULL << 1)
/**< Asymmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING	(1ULL << 2)
/**< Chaining symmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_CPU_SSE		(1ULL << 3)
/**< Utilises CPU SIMD SSE instructions */
#define	RTE_CRYPTODEV_FF_CPU_AVX		(1ULL << 4)
/**< Utilises CPU SIMD AVX instructions */
#define	RTE_CRYPTODEV_FF_CPU_AVX2		(1ULL << 5)
/**< Utilises CPU SIMD AVX2 instructions */
#define	RTE_CRYPTODEV_FF_CPU_AESNI		(1ULL << 6)
/**< Utilises CPU AES-NI instructions */
#define	RTE_CRYPTODEV_FF_HW_ACCELERATED		(1ULL << 7)
/**< Operations are off-loaded to an external hardware accelerator */


/**
 * Get the name of a crypto device feature flag
 *
 * @param	flag	The mask describing the flag.
 *
 * @return
 *   The name of this flag, or NULL if it's not a valid feature flag.
 */

extern const char *
rte_cryptodev_get_feature_name(uint64_t flag);

/**  Crypto device information */
struct rte_cryptodev_info {
	const char *driver_name;		/**< Driver name. */
	enum rte_cryptodev_type dev_type;	/**< Device type */
	struct rte_pci_device *pci_dev;		/**< PCI information. */

	uint64_t feature_flags;			/**< Feature flags */

	const struct rte_cryptodev_capabilities *capabilities;
	/**< Array of devices supported capabilities */

	unsigned max_nb_queue_pairs;
	/**< Maximum number of queues pairs supported by device. */

	struct {
		unsigned max_nb_sessions;
		/**< Maximum number of sessions supported by device. */
	} sym;
};

#define RTE_CRYPTODEV_DETACHED  (0)
#define RTE_CRYPTODEV_ATTACHED  (1)

/** Definitions of Crypto device event types */
enum rte_cryptodev_event_type {
	RTE_CRYPTODEV_EVENT_UNKNOWN,	/**< unknown event type */
	RTE_CRYPTODEV_EVENT_ERROR,	/**< error interrupt event */
	RTE_CRYPTODEV_EVENT_MAX		/**< max value of this enum */
};

/** Crypto device queue pair configuration structure. */
struct rte_cryptodev_qp_conf {
	uint32_t nb_descriptors; /**< Number of descriptors per queue pair */
};

/**
 * Typedef for application callback function to be registered by application
 * software for notification of device events
 *
 * @param	dev_id	Crypto device identifier
 * @param	event	Crypto device event to register for notification of.
 * @param	cb_arg	User specified parameter to be passed as to passed to
 *			users callback function.
 */
typedef void (*rte_cryptodev_cb_fn)(uint8_t dev_id,
		enum rte_cryptodev_event_type event, void *cb_arg);


/** Crypto Device statistics */
struct rte_cryptodev_stats {
	uint64_t enqueued_count;
	/**< Count of all operations enqueued */
	uint64_t dequeued_count;
	/**< Count of all operations dequeued */

	uint64_t enqueue_err_count;
	/**< Total error count on operations enqueued */
	uint64_t dequeue_err_count;
	/**< Total error count on operations dequeued */
};

#define RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS	8
#define RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS	2048

/**
 * @internal
 * Initialisation parameters for virtual crypto devices
 */
struct rte_crypto_vdev_init_params {
	unsigned max_nb_queue_pairs;
	unsigned max_nb_sessions;
	uint8_t socket_id;
};

/**
 * Parse virtual device initialisation parameters input arguments
 * @internal
 *
 * @params	params		Initialisation parameters with defaults set.
 * @params	input_args	Command line arguments
 *
 * @return
 * 0 on successful parse
 * <0 on failure to parse
 */
int
rte_cryptodev_parse_vdev_init_params(
		struct rte_crypto_vdev_init_params *params,
		const char *input_args);

/**
 * Create a virtual crypto device
 *
 * @param	name	Cryptodev PMD name of device to be created.
 * @param	args	Options arguments for device.
 *
 * @return
 * - On successful creation of the cryptodev the device index is returned,
 *   which will be between 0 and rte_cryptodev_count().
 * - In the case of a failure, returns -1.
 */
extern int
rte_cryptodev_create_vdev(const char *name, const char *args);

/**
 * Get the device identifier for the named crypto device.
 *
 * @param	name	device name to select the device structure.
 *
 * @return
 *   - Returns crypto device identifier on success.
 *   - Return -1 on failure to find named crypto device.
 */
extern int
rte_cryptodev_get_dev_id(const char *name);

/**
 * Get the total number of crypto devices that have been successfully
 * initialised.
 *
 * @return
 *   - The total number of usable crypto devices.
 */
extern uint8_t
rte_cryptodev_count(void);

extern uint8_t
rte_cryptodev_count_devtype(enum rte_cryptodev_type type);
/*
 * Return the NUMA socket to which a device is connected
 *
 * @param dev_id
 *   The identifier of the device
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 if returned is the dev_id value is out of range.
 */
extern int
rte_cryptodev_socket_id(uint8_t dev_id);

/** Crypto device configuration structure */
struct rte_cryptodev_config {
	int socket_id;			/**< Socket to allocate resources on */
	uint16_t nb_queue_pairs;
	/**< Number of queue pairs to configure on device */

	struct {
		uint32_t nb_objs;	/**< Number of objects in mempool */
		uint32_t cache_size;	/**< l-core object cache size */
	} session_mp;		/**< Session mempool configuration */
};

/**
 * Configure a device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param	dev_id		The identifier of the device to configure.
 * @param	config		The crypto device configuration structure.
 *
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
extern int
rte_cryptodev_configure(uint8_t dev_id, struct rte_cryptodev_config *config);

/**
 * Start an device.
 *
 * The device start step is the last one and consists of setting the configured
 * offload features and in starting the transmit and the receive units of the
 * device.
 * On success, all basic functions exported by the API (link status,
 * receive/transmit, and so on) can be invoked.
 *
 * @param dev_id
 *   The identifier of the device.
 * @return
 *   - 0: Success, device started.
 *   - <0: Error code of the driver device start function.
 */
extern int
rte_cryptodev_start(uint8_t dev_id);

/**
 * Stop an device. The device can be restarted with a call to
 * rte_cryptodev_start()
 *
 * @param	dev_id		The identifier of the device.
 */
extern void
rte_cryptodev_stop(uint8_t dev_id);

/**
 * Close an device. The device cannot be restarted!
 *
 * @param	dev_id		The identifier of the device.
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 */
extern int
rte_cryptodev_close(uint8_t dev_id);

/**
 * Allocate and set up a receive queue pair for a device.
 *
 *
 * @param	dev_id		The identifier of the device.
 * @param	queue_pair_id	The index of the queue pairs to set up. The
 *				value must be in the range [0, nb_queue_pair
 *				- 1] previously supplied to
 *				rte_cryptodev_configure().
 * @param	qp_conf		The pointer to the configuration data to be
 *				used for the queue pair. NULL value is
 *				allowed, in which case default configuration
 *				will be used.
 * @param	socket_id	The *socket_id* argument is the socket
 *				identifier in case of NUMA. The value can be
 *				*SOCKET_ID_ANY* if there is no NUMA constraint
 *				for the DMA memory allocated for the receive
 *				queue pair.
 *
 * @return
 *   - 0: Success, queue pair correctly set up.
 *   - <0: Queue pair configuration failed
 */
extern int
rte_cryptodev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf, int socket_id);

/**
 * Start a specified queue pair of a device. It is used
 * when deferred_start flag of the specified queue is true.
 *
 * @param	dev_id		The identifier of the device
 * @param	queue_pair_id	The index of the queue pair to start. The value
 *				must be in the range [0, nb_queue_pair - 1]
 *				previously supplied to
 *				rte_crypto_dev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The dev_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int
rte_cryptodev_queue_pair_start(uint8_t dev_id, uint16_t queue_pair_id);

/**
 * Stop specified queue pair of a device
 *
 * @param	dev_id		The identifier of the device
 * @param	queue_pair_id	The index of the queue pair to stop. The value
 *				must be in the range [0, nb_queue_pair - 1]
 *				previously supplied to
 *				rte_cryptodev_configure().
 * @return
 *   - 0: Success, the transmit queue is correctly set up.
 *   - -EINVAL: The dev_id or the queue_id out of range.
 *   - -ENOTSUP: The function not supported in PMD driver.
 */
extern int
rte_cryptodev_queue_pair_stop(uint8_t dev_id, uint16_t queue_pair_id);

/**
 * Get the number of queue pairs on a specific crypto device
 *
 * @param	dev_id		Crypto device identifier.
 * @return
 *   - The number of configured queue pairs.
 */
extern uint16_t
rte_cryptodev_queue_pair_count(uint8_t dev_id);


/**
 * Retrieve the general I/O statistics of a device.
 *
 * @param	dev_id		The identifier of the device.
 * @param	stats		A pointer to a structure of type
 *				*rte_cryptodev_stats* to be filled with the
 *				values of device counters.
 * @return
 *   - Zero if successful.
 *   - Non-zero otherwise.
 */
extern int
rte_cryptodev_stats_get(uint8_t dev_id, struct rte_cryptodev_stats *stats);

/**
 * Reset the general I/O statistics of a device.
 *
 * @param	dev_id		The identifier of the device.
 */
extern void
rte_cryptodev_stats_reset(uint8_t dev_id);

/**
 * Retrieve the contextual information of a device.
 *
 * @param	dev_id		The identifier of the device.
 * @param	dev_info	A pointer to a structure of type
 *				*rte_cryptodev_info* to be filled with the
 *				contextual information of the device.
 */
extern void
rte_cryptodev_info_get(uint8_t dev_id, struct rte_cryptodev_info *dev_info);


/**
 * Register a callback function for specific device id.
 *
 * @param	dev_id		Device id.
 * @param	event		Event interested.
 * @param	cb_fn		User supplied callback function to be called.
 * @param	cb_arg		Pointer to the parameters for the registered
 *				callback.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
extern int
rte_cryptodev_callback_register(uint8_t dev_id,
		enum rte_cryptodev_event_type event,
		rte_cryptodev_cb_fn cb_fn, void *cb_arg);

/**
 * Unregister a callback function for specific device id.
 *
 * @param	dev_id		The device identifier.
 * @param	event		Event interested.
 * @param	cb_fn		User supplied callback function to be called.
 * @param	cb_arg		Pointer to the parameters for the registered
 *				callback.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
extern int
rte_cryptodev_callback_unregister(uint8_t dev_id,
		enum rte_cryptodev_event_type event,
		rte_cryptodev_cb_fn cb_fn, void *cb_arg);


typedef uint16_t (*dequeue_pkt_burst_t)(void *qp,
		struct rte_crypto_op **ops,	uint16_t nb_ops);
/**< Dequeue processed packets from queue pair of a device. */

typedef uint16_t (*enqueue_pkt_burst_t)(void *qp,
		struct rte_crypto_op **ops,	uint16_t nb_ops);
/**< Enqueue packets for processing on queue pair of a device. */




struct rte_cryptodev_callback;

/** Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_cryptodev_cb_list, rte_cryptodev_callback);

/** The data structure associated with each crypto device. */
struct rte_cryptodev {
	dequeue_pkt_burst_t dequeue_burst;
	/**< Pointer to PMD receive function. */
	enqueue_pkt_burst_t enqueue_burst;
	/**< Pointer to PMD transmit function. */

	const struct rte_cryptodev_driver *driver;
	/**< Driver for this device */
	struct rte_cryptodev_data *data;
	/**< Pointer to device data */
	struct rte_cryptodev_ops *dev_ops;
	/**< Functions exported by PMD */
	uint64_t feature_flags;
	/**< Supported features */
	struct rte_pci_device *pci_dev;
	/**< PCI info. supplied by probing */

	enum rte_cryptodev_type dev_type;
	/**< Crypto device type */

	struct rte_cryptodev_cb_list link_intr_cbs;
	/**< User application callback for interrupts if present */

	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */
} __rte_cache_aligned;


#define RTE_CRYPTODEV_NAME_MAX_LEN	(64)
/**< Max length of name of crypto PMD */

/**
 *
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_cryptodev_data {
	uint8_t dev_id;
	/**< Device ID for this instance */
	uint8_t socket_id;
	/**< Socket ID where memory is allocated */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	__extension__
	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	struct rte_mempool *session_pool;
	/**< Session memory pool */
	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs. */

	void *dev_private;
	/**< PMD-specific private data */
} __rte_cache_aligned;

extern struct rte_cryptodev *rte_cryptodevs;
/**
 *
 * Dequeue a burst of processed crypto operations from a queue on the crypto
 * device. The dequeued operation are stored in *rte_crypto_op* structures
 * whose pointers are supplied in the *ops* array.
 *
 * The rte_cryptodev_dequeue_burst() function returns the number of ops
 * actually dequeued, which is the number of *rte_crypto_op* data structures
 * effectively supplied into the *ops* array.
 *
 * A return value equal to *nb_ops* indicates that the queue contained
 * at least *nb_ops* operations, and this is likely to signify that other
 * processed operations remain in the devices output queue. Applications
 * implementing a "retrieve as many processed operations as possible" policy
 * can check this specific case and keep invoking the
 * rte_cryptodev_dequeue_burst() function until a value less than
 * *nb_ops* is returned.
 *
 * The rte_cryptodev_dequeue_burst() function does not provide any error
 * notification to avoid the corresponding overhead.
 *
 * @param	dev_id		The symmetric crypto device identifier
 * @param	qp_id		The index of the queue pair from which to
 *				retrieve processed packets. The value must be
 *				in the range [0, nb_queue_pair - 1] previously
 *				supplied to rte_cryptodev_configure().
 * @param	ops		The address of an array of pointers to
 *				*rte_crypto_op* structures that must be
 *				large enough to store *nb_ops* pointers in it.
 * @param	nb_ops		The maximum number of operations to dequeue.
 *
 * @return
 *   - The number of operations actually dequeued, which is the number
 *   of pointers to *rte_crypto_op* structures effectively supplied to the
 *   *ops* array.
 */
static inline uint16_t
rte_cryptodev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_cryptodev *dev = &rte_cryptodevs[dev_id];

	nb_ops = (*dev->dequeue_burst)
			(dev->data->queue_pairs[qp_id], ops, nb_ops);

	return nb_ops;
}

/**
 * Enqueue a burst of operations for processing on a crypto device.
 *
 * The rte_cryptodev_enqueue_burst() function is invoked to place
 * crypto operations on the queue *qp_id* of the device designated by
 * its *dev_id*.
 *
 * The *nb_ops* parameter is the number of operations to process which are
 * supplied in the *ops* array of *rte_crypto_op* structures.
 *
 * The rte_cryptodev_enqueue_burst() function returns the number of
 * operations it actually enqueued for processing. A return value equal to
 * *nb_ops* means that all packets have been enqueued.
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair which packets are
 *				to be enqueued for processing. The value
 *				must be in the range [0, nb_queue_pairs - 1]
 *				previously supplied to
 *				 *rte_cryptodev_configure*.
 * @param	ops		The address of an array of *nb_ops* pointers
 *				to *rte_crypto_op* structures which contain
 *				the crypto operations to be processed.
 * @param	nb_ops		The number of operations to process.
 *
 * @return
 * The number of operations actually enqueued on the crypto device. The return
 * value can be less than the value of the *nb_ops* parameter when the
 * crypto devices queue is full or if invalid parameters are specified in
 * a *rte_crypto_op*.
 */
static inline uint16_t
rte_cryptodev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_cryptodev *dev = &rte_cryptodevs[dev_id];

	return (*dev->enqueue_burst)(
			dev->data->queue_pairs[qp_id], ops, nb_ops);
}


/** Cryptodev symmetric crypto session */
struct rte_cryptodev_sym_session {
	RTE_STD_C11
	struct {
		uint8_t dev_id;
		/**< Device Id */
		enum rte_cryptodev_type dev_type;
		/** Crypto Device type session created on */
		struct rte_mempool *mp;
		/**< Mempool session allocated from */
	} __rte_aligned(8);
	/**< Public symmetric session details */

	__extension__ char _private[0];
	/**< Private session material */
};


/**
 * Initialise a session for symmetric cryptographic operations.
 *
 * This function is used by the client to initialize immutable
 * parameters of symmetric cryptographic operation.
 * To perform the operation the rte_cryptodev_enqueue_burst function is
 * used.  Each mbuf should contain a reference to the session
 * pointer returned from this function contained within it's crypto_op if a
 * session-based operation is being provisioned. Memory to contain the session
 * information is allocated from within mempool managed by the cryptodev.
 *
 * The rte_cryptodev_session_free must be called to free allocated
 * memory when the session is no longer required.
 *
 * @param	dev_id		The device identifier.
 * @param	xform		Crypto transform chain.

 *
 * @return
 *  Pointer to the created session or NULL
 */
extern struct rte_cryptodev_sym_session *
rte_cryptodev_sym_session_create(uint8_t dev_id,
		struct rte_crypto_sym_xform *xform);

/**
 * Free the memory associated with a previously allocated session.
 *
 * @param	dev_id		The device identifier.
 * @param	session		Session pointer previously allocated by
 *				*rte_cryptodev_sym_session_create*.
 *
 * @return
 *   NULL on successful freeing of session.
 *   Session pointer on failure to free session.
 */
extern struct rte_cryptodev_sym_session *
rte_cryptodev_sym_session_free(uint8_t dev_id,
		struct rte_cryptodev_sym_session *session);


#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_H_ */
