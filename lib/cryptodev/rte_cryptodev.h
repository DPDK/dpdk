/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Intel Corporation.
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
#include <rte_config.h>
#include <rte_rcu_qsbr.h>

#include "rte_cryptodev_trace_fp.h"

extern const char **rte_cyptodev_names;

/* Logging Macros */

#define CDEV_LOG_ERR(...) \
	RTE_LOG(ERR, CRYPTODEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define CDEV_LOG_INFO(...) \
	RTE_LOG(INFO, CRYPTODEV, \
		RTE_FMT(RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			RTE_FMT_TAIL(__VA_ARGS__,)))

#define CDEV_LOG_DEBUG(...) \
	RTE_LOG(DEBUG, CRYPTODEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define CDEV_PMD_TRACE(...) \
	RTE_LOG(DEBUG, CRYPTODEV, \
		RTE_FMT("[%s] %s: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			dev, __func__, RTE_FMT_TAIL(__VA_ARGS__,)))

/**
 * A macro that points to an offset from the start
 * of the crypto operation structure (rte_crypto_op)
 *
 * The returned pointer is cast to type t.
 *
 * @param c
 *   The crypto operation.
 * @param o
 *   The offset from the start of the crypto operation.
 * @param t
 *   The type to cast the result into.
 */
#define rte_crypto_op_ctod_offset(c, t, o)	\
	((t)((char *)(c) + (o)))

/**
 * A macro that returns the physical address that points
 * to an offset from the start of the crypto operation
 * (rte_crypto_op)
 *
 * @param c
 *   The crypto operation.
 * @param o
 *   The offset from the start of the crypto operation
 *   to calculate address from.
 */
#define rte_crypto_op_ctophys_offset(c, o)	\
	(rte_iova_t)((c)->phys_addr + (o))

/**
 * Crypto parameters range description
 */
struct rte_crypto_param_range {
	uint16_t min;	/**< minimum size */
	uint16_t max;	/**< maximum size */
	uint16_t increment;
	/**< if a range of sizes are supported,
	 * this parameter is used to indicate
	 * increments in byte size that are supported
	 * between the minimum and maximum
	 */
};

/**
 * Data-unit supported lengths of cipher algorithms.
 * A bit can represent any set of data-unit sizes
 * (single size, multiple size, range, etc).
 */
#define RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES             RTE_BIT32(0)
#define RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_4096_BYTES            RTE_BIT32(1)
#define RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_1_MEGABYTES           RTE_BIT32(2)

/**
 * Symmetric Crypto Capability
 */
struct rte_cryptodev_symmetric_capability {
	enum rte_crypto_sym_xform_type xform_type;
	/**< Transform type : Authentication / Cipher / AEAD */
	RTE_STD_C11
	union {
		struct {
			enum rte_crypto_auth_algorithm algo;
			/**< authentication algorithm */
			uint16_t block_size;
			/**< algorithm block size */
			struct rte_crypto_param_range key_size;
			/**< auth key size range */
			struct rte_crypto_param_range digest_size;
			/**< digest size range */
			struct rte_crypto_param_range aad_size;
			/**< Additional authentication data size range */
			struct rte_crypto_param_range iv_size;
			/**< Initialisation vector data size range */
		} auth;
		/**< Symmetric Authentication transform capabilities */
		struct {
			enum rte_crypto_cipher_algorithm algo;
			/**< cipher algorithm */
			uint16_t block_size;
			/**< algorithm block size */
			struct rte_crypto_param_range key_size;
			/**< cipher key size range */
			struct rte_crypto_param_range iv_size;
			/**< Initialisation vector data size range */
			uint32_t dataunit_set;
			/**<
			 * Supported data-unit lengths:
			 * RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_* bits
			 * or 0 for lengths defined in the algorithm standard.
			 */
		} cipher;
		/**< Symmetric Cipher transform capabilities */
		struct {
			enum rte_crypto_aead_algorithm algo;
			/**< AEAD algorithm */
			uint16_t block_size;
			/**< algorithm block size */
			struct rte_crypto_param_range key_size;
			/**< AEAD key size range */
			struct rte_crypto_param_range digest_size;
			/**< digest size range */
			struct rte_crypto_param_range aad_size;
			/**< Additional authentication data size range */
			struct rte_crypto_param_range iv_size;
			/**< Initialisation vector data size range */
		} aead;
	};
};

/**
 * Asymmetric Xform Crypto Capability
 *
 */
struct rte_cryptodev_asymmetric_xform_capability {
	enum rte_crypto_asym_xform_type xform_type;
	/**< Transform type: RSA/MODEXP/DH/DSA/MODINV */

	uint32_t op_types;
	/**< bitmask for supported rte_crypto_asym_op_type */

	__extension__
	union {
		struct rte_crypto_param_range modlen;
		/**< Range of modulus length supported by modulus based xform.
		 * Value 0 mean implementation default
		 */
	};
};

/**
 * Asymmetric Crypto Capability
 *
 */
struct rte_cryptodev_asymmetric_capability {
	struct rte_cryptodev_asymmetric_xform_capability xform_capa;
};


/** Structure used to capture a capability of a crypto device */
struct rte_cryptodev_capabilities {
	enum rte_crypto_op_type op;
	/**< Operation type */

	RTE_STD_C11
	union {
		struct rte_cryptodev_symmetric_capability sym;
		/**< Symmetric operation capability parameters */
		struct rte_cryptodev_asymmetric_capability asym;
		/**< Asymmetric operation capability parameters */
	};
};

/** Structure used to describe crypto algorithms */
struct rte_cryptodev_sym_capability_idx {
	enum rte_crypto_sym_xform_type type;
	union {
		enum rte_crypto_cipher_algorithm cipher;
		enum rte_crypto_auth_algorithm auth;
		enum rte_crypto_aead_algorithm aead;
	} algo;
};

/**
 * Structure used to describe asymmetric crypto xforms
 * Each xform maps to one asym algorithm.
 *
 */
struct rte_cryptodev_asym_capability_idx {
	enum rte_crypto_asym_xform_type type;
	/**< Asymmetric xform (algo) type */
};

/**
 * Provide capabilities available for defined device and algorithm
 *
 * @param	dev_id		The identifier of the device.
 * @param	idx		Description of crypto algorithms.
 *
 * @return
 *   - Return description of the symmetric crypto capability if exist.
 *   - Return NULL if the capability not exist.
 */
const struct rte_cryptodev_symmetric_capability *
rte_cryptodev_sym_capability_get(uint8_t dev_id,
		const struct rte_cryptodev_sym_capability_idx *idx);

/**
 *  Provide capabilities available for defined device and xform
 *
 * @param	dev_id		The identifier of the device.
 * @param	idx		Description of asym crypto xform.
 *
 * @return
 *   - Return description of the asymmetric crypto capability if exist.
 *   - Return NULL if the capability not exist.
 */
__rte_experimental
const struct rte_cryptodev_asymmetric_xform_capability *
rte_cryptodev_asym_capability_get(uint8_t dev_id,
		const struct rte_cryptodev_asym_capability_idx *idx);

/**
 * Check if key size and initial vector are supported
 * in crypto cipher capability
 *
 * @param	capability	Description of the symmetric crypto capability.
 * @param	key_size	Cipher key size.
 * @param	iv_size		Cipher initial vector size.
 *
 * @return
 *   - Return 0 if the parameters are in range of the capability.
 *   - Return -1 if the parameters are out of range of the capability.
 */
int
rte_cryptodev_sym_capability_check_cipher(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t iv_size);

/**
 * Check if key size and initial vector are supported
 * in crypto auth capability
 *
 * @param	capability	Description of the symmetric crypto capability.
 * @param	key_size	Auth key size.
 * @param	digest_size	Auth digest size.
 * @param	iv_size		Auth initial vector size.
 *
 * @return
 *   - Return 0 if the parameters are in range of the capability.
 *   - Return -1 if the parameters are out of range of the capability.
 */
int
rte_cryptodev_sym_capability_check_auth(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t iv_size);

/**
 * Check if key, digest, AAD and initial vector sizes are supported
 * in crypto AEAD capability
 *
 * @param	capability	Description of the symmetric crypto capability.
 * @param	key_size	AEAD key size.
 * @param	digest_size	AEAD digest size.
 * @param	aad_size	AEAD AAD size.
 * @param	iv_size		AEAD IV size.
 *
 * @return
 *   - Return 0 if the parameters are in range of the capability.
 *   - Return -1 if the parameters are out of range of the capability.
 */
int
rte_cryptodev_sym_capability_check_aead(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t aad_size,
		uint16_t iv_size);

/**
 * Check if op type is supported
 *
 * @param	capability	Description of the asymmetric crypto capability.
 * @param	op_type		op type
 *
 * @return
 *   - Return 1 if the op type is supported
 *   - Return 0 if unsupported
 */
__rte_experimental
int
rte_cryptodev_asym_xform_capability_check_optype(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
		enum rte_crypto_asym_op_type op_type);

/**
 * Check if modulus length is in supported range
 *
 * @param	capability	Description of the asymmetric crypto capability.
 * @param	modlen		modulus length.
 *
 * @return
 *   - Return 0 if the parameters are in range of the capability.
 *   - Return -1 if the parameters are out of range of the capability.
 */
__rte_experimental
int
rte_cryptodev_asym_xform_capability_check_modlen(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
		uint16_t modlen);

/**
 * Provide the cipher algorithm enum, given an algorithm string
 *
 * @param	algo_enum	A pointer to the cipher algorithm
 *				enum to be filled
 * @param	algo_string	Authentication algo string
 *
 * @return
 * - Return -1 if string is not valid
 * - Return 0 is the string is valid
 */
int
rte_cryptodev_get_cipher_algo_enum(enum rte_crypto_cipher_algorithm *algo_enum,
		const char *algo_string);

/**
 * Provide the authentication algorithm enum, given an algorithm string
 *
 * @param	algo_enum	A pointer to the authentication algorithm
 *				enum to be filled
 * @param	algo_string	Authentication algo string
 *
 * @return
 * - Return -1 if string is not valid
 * - Return 0 is the string is valid
 */
int
rte_cryptodev_get_auth_algo_enum(enum rte_crypto_auth_algorithm *algo_enum,
		const char *algo_string);

/**
 * Provide the AEAD algorithm enum, given an algorithm string
 *
 * @param	algo_enum	A pointer to the AEAD algorithm
 *				enum to be filled
 * @param	algo_string	AEAD algorithm string
 *
 * @return
 * - Return -1 if string is not valid
 * - Return 0 is the string is valid
 */
int
rte_cryptodev_get_aead_algo_enum(enum rte_crypto_aead_algorithm *algo_enum,
		const char *algo_string);

/**
 * Provide the Asymmetric xform enum, given an xform string
 *
 * @param	xform_enum	A pointer to the xform type
 *				enum to be filled
 * @param	xform_string	xform string
 *
 * @return
 * - Return -1 if string is not valid
 * - Return 0 if the string is valid
 */
__rte_experimental
int
rte_cryptodev_asym_get_xform_enum(enum rte_crypto_asym_xform_type *xform_enum,
		const char *xform_string);


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
#define	RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO		(1ULL << 0)
/**< Symmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO		(1ULL << 1)
/**< Asymmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING		(1ULL << 2)
/**< Chaining symmetric crypto operations are supported */
#define	RTE_CRYPTODEV_FF_CPU_SSE			(1ULL << 3)
/**< Utilises CPU SIMD SSE instructions */
#define	RTE_CRYPTODEV_FF_CPU_AVX			(1ULL << 4)
/**< Utilises CPU SIMD AVX instructions */
#define	RTE_CRYPTODEV_FF_CPU_AVX2			(1ULL << 5)
/**< Utilises CPU SIMD AVX2 instructions */
#define	RTE_CRYPTODEV_FF_CPU_AESNI			(1ULL << 6)
/**< Utilises CPU AES-NI instructions */
#define	RTE_CRYPTODEV_FF_HW_ACCELERATED			(1ULL << 7)
/**< Operations are off-loaded to an
 * external hardware accelerator
 */
#define	RTE_CRYPTODEV_FF_CPU_AVX512			(1ULL << 8)
/**< Utilises CPU SIMD AVX512 instructions */
#define	RTE_CRYPTODEV_FF_IN_PLACE_SGL			(1ULL << 9)
/**< In-place Scatter-gather (SGL) buffers, with multiple segments,
 * are supported
 */
#define RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT		(1ULL << 10)
/**< Out-of-place Scatter-gather (SGL) buffers are
 * supported in input and output
 */
#define RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT		(1ULL << 11)
/**< Out-of-place Scatter-gather (SGL) buffers are supported
 * in input, combined with linear buffers (LB), with a
 * single segment in output
 */
#define RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT		(1ULL << 12)
/**< Out-of-place Scatter-gather (SGL) buffers are supported
 * in output, combined with linear buffers (LB) in input
 */
#define RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT		(1ULL << 13)
/**< Out-of-place linear buffers (LB) are supported in input and output */
#define	RTE_CRYPTODEV_FF_CPU_NEON			(1ULL << 14)
/**< Utilises CPU NEON instructions */
#define	RTE_CRYPTODEV_FF_CPU_ARM_CE			(1ULL << 15)
/**< Utilises ARM CPU Cryptographic Extensions */
#define	RTE_CRYPTODEV_FF_SECURITY			(1ULL << 16)
/**< Support Security Protocol Processing */
#define RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP		(1ULL << 17)
/**< Support RSA Private Key OP with exponent */
#define RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT		(1ULL << 18)
/**< Support RSA Private Key OP with CRT (quintuple) Keys */
#define RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED		(1ULL << 19)
/**< Support encrypted-digest operations where digest is appended to data */
#define RTE_CRYPTODEV_FF_ASYM_SESSIONLESS		(1ULL << 20)
/**< Support asymmetric session-less operations */
#define	RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO			(1ULL << 21)
/**< Support symmetric cpu-crypto processing */
#define RTE_CRYPTODEV_FF_SYM_SESSIONLESS		(1ULL << 22)
/**< Support symmetric session-less operations */
#define RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA		(1ULL << 23)
/**< Support operations on data which is not byte aligned */
#define RTE_CRYPTODEV_FF_SYM_RAW_DP			(1ULL << 24)
/**< Support accelerator specific symmetric raw data-path APIs */
#define RTE_CRYPTODEV_FF_CIPHER_MULTIPLE_DATA_UNITS	(1ULL << 25)
/**< Support operations on multiple data-units message */
#define RTE_CRYPTODEV_FF_CIPHER_WRAPPED_KEY		(1ULL << 26)
/**< Support wrapped key in cipher xform  */
#define RTE_CRYPTODEV_FF_SECURITY_INNER_CSUM		(1ULL << 27)
/**< Support inner checksum computation/verification */

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
	const char *driver_name;	/**< Driver name. */
	uint8_t driver_id;		/**< Driver identifier */
	struct rte_device *device;	/**< Generic device information. */

	uint64_t feature_flags;
	/**< Feature flags exposes HW/SW features for the given device */

	const struct rte_cryptodev_capabilities *capabilities;
	/**< Array of devices supported capabilities */

	unsigned max_nb_queue_pairs;
	/**< Maximum number of queues pairs supported by device. */

	uint16_t min_mbuf_headroom_req;
	/**< Minimum mbuf headroom required by device */

	uint16_t min_mbuf_tailroom_req;
	/**< Minimum mbuf tailroom required by device */

	struct {
		unsigned max_nb_sessions;
		/**< Maximum number of sessions supported by device.
		 * If 0, the device does not have any limitation in
		 * number of sessions that can be used.
		 */
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
	struct rte_mempool *mp_session;
	/**< The mempool for creating session in sessionless mode */
	struct rte_mempool *mp_session_private;
	/**< The mempool for creating sess private data in sessionless mode */
};

/**
 * Function type used for processing crypto ops when enqueue/dequeue burst is
 * called.
 *
 * The callback function is called on enqueue/dequeue burst immediately.
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair on which ops are
 *				enqueued/dequeued. The value must be in the
 *				range [0, nb_queue_pairs - 1] previously
 *				supplied to *rte_cryptodev_configure*.
 * @param	ops		The address of an array of *nb_ops* pointers
 *				to *rte_crypto_op* structures which contain
 *				the crypto operations to be processed.
 * @param	nb_ops		The number of operations to process.
 * @param	user_param	The arbitrary user parameter passed in by the
 *				application when the callback was originally
 *				registered.
 * @return			The number of ops to be enqueued to the
 *				crypto device.
 */
typedef uint16_t (*rte_cryptodev_callback_fn)(uint16_t dev_id, uint16_t qp_id,
		struct rte_crypto_op **ops, uint16_t nb_ops, void *user_param);

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

#define RTE_CRYPTODEV_NAME_MAX_LEN	(64)
/**< Max length of name of crypto PMD */

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
 * Get the crypto device name given a device identifier.
 *
 * @param dev_id
 *   The identifier of the device
 *
 * @return
 *   - Returns crypto device name.
 *   - Returns NULL if crypto device is not present.
 */
extern const char *
rte_cryptodev_name_get(uint8_t dev_id);

/**
 * Get the total number of crypto devices that have been successfully
 * initialised.
 *
 * @return
 *   - The total number of usable crypto devices.
 */
extern uint8_t
rte_cryptodev_count(void);

/**
 * Get number of crypto device defined type.
 *
 * @param	driver_id	driver identifier.
 *
 * @return
 *   Returns number of crypto device.
 */
extern uint8_t
rte_cryptodev_device_count_by_driver(uint8_t driver_id);

/**
 * Get number and identifiers of attached crypto devices that
 * use the same crypto driver.
 *
 * @param	driver_name	driver name.
 * @param	devices		output devices identifiers.
 * @param	nb_devices	maximal number of devices.
 *
 * @return
 *   Returns number of attached crypto device.
 */
uint8_t
rte_cryptodev_devices_get(const char *driver_name, uint8_t *devices,
		uint8_t nb_devices);
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
	uint64_t ff_disable;
	/**< Feature flags to be disabled. Only the following features are
	 * allowed to be disabled,
	 *  - RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO
	 *  - RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO
	 *  - RTE_CRYTPODEV_FF_SECURITY
	 */
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
 *				used for the queue pair.
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
 * Get the status of queue pairs setup on a specific crypto device
 *
 * @param	dev_id		Crypto device identifier.
 * @param	queue_pair_id	The index of the queue pairs to set up. The
 *				value must be in the range [0, nb_queue_pair
 *				- 1] previously supplied to
 *				rte_cryptodev_configure().
 * @return
 *   - 0: qp was not configured
 *	 - 1: qp was configured
 *	 - -EINVAL: device was not configured
 */
__rte_experimental
int
rte_cryptodev_get_qp_status(uint8_t dev_id, uint16_t queue_pair_id);

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
 *
 * @note The capabilities field of dev_info is set to point to the first
 * element of an array of struct rte_cryptodev_capabilities. The element after
 * the last valid element has it's op field set to
 * RTE_CRYPTO_OP_TYPE_UNDEFINED.
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

struct rte_cryptodev_callback;

/** Structure to keep track of registered callbacks */
RTE_TAILQ_HEAD(rte_cryptodev_cb_list, rte_cryptodev_callback);

/**
 * Structure used to hold information about the callbacks to be called for a
 * queue pair on enqueue/dequeue.
 */
struct rte_cryptodev_cb {
	struct rte_cryptodev_cb *next;
	/**< Pointer to next callback */
	rte_cryptodev_callback_fn fn;
	/**< Pointer to callback function */
	void *arg;
	/**< Pointer to argument */
};

/**
 * @internal
 * Structure used to hold information about the RCU for a queue pair.
 */
struct rte_cryptodev_cb_rcu {
	struct rte_cryptodev_cb *next;
	/**< Pointer to next callback */
	struct rte_rcu_qsbr *qsbr;
	/**< RCU QSBR variable per queue pair */
};

void *
rte_cryptodev_get_sec_ctx(uint8_t dev_id);

/** Cryptodev symmetric crypto session
 * Each session is derived from a fixed xform chain. Therefore each session
 * has a fixed algo, key, op-type, digest_len etc.
 */
struct rte_cryptodev_sym_session {
	uint64_t opaque_data;
	/**< Can be used for external metadata */
	uint16_t nb_drivers;
	/**< number of elements in sess_data array */
	uint16_t user_data_sz;
	/**< session user data will be placed after sess_data */
	__extension__ struct {
		void *data;
		uint16_t refcnt;
	} sess_data[0];
	/**< Driver specific session material, variable size */
};

/** Cryptodev asymmetric crypto session */
struct rte_cryptodev_asym_session {
	__extension__ void *sess_private_data[0];
	/**< Private asymmetric session material */
};

/**
 * Create a symmetric session mempool.
 *
 * @param name
 *   The unique mempool name.
 * @param nb_elts
 *   The number of elements in the mempool.
 * @param elt_size
 *   The size of the element. This value will be ignored if it is smaller than
 *   the minimum session header size required for the system. For the user who
 *   want to use the same mempool for sym session and session private data it
 *   can be the maximum value of all existing devices' private data and session
 *   header sizes.
 * @param cache_size
 *   The number of per-lcore cache elements
 * @param priv_size
 *   The private data size of each session.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 *
 * @return
 *  - On success return size of the session
 *  - On failure returns 0
 */
__rte_experimental
struct rte_mempool *
rte_cryptodev_sym_session_pool_create(const char *name, uint32_t nb_elts,
	uint32_t elt_size, uint32_t cache_size, uint16_t priv_size,
	int socket_id);

/**
 * Create symmetric crypto session header (generic with no private data)
 *
 * @param   mempool    Symmetric session mempool to allocate session
 *                     objects from
 * @return
 *  - On success return pointer to sym-session
 *  - On failure returns NULL
 */
struct rte_cryptodev_sym_session *
rte_cryptodev_sym_session_create(struct rte_mempool *mempool);

/**
 * Create asymmetric crypto session header (generic with no private data)
 *
 * @param   mempool    mempool to allocate asymmetric session
 *                     objects from
 * @return
 *  - On success return pointer to asym-session
 *  - On failure returns NULL
 */
__rte_experimental
struct rte_cryptodev_asym_session *
rte_cryptodev_asym_session_create(struct rte_mempool *mempool);

/**
 * Frees symmetric crypto session header, after checking that all
 * the device private data has been freed, returning it
 * to its original mempool.
 *
 * @param   sess     Session header to be freed.
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if session is NULL.
 *  - -EBUSY if not all device private data has been freed.
 */
int
rte_cryptodev_sym_session_free(struct rte_cryptodev_sym_session *sess);

/**
 * Frees asymmetric crypto session header, after checking that all
 * the device private data has been freed, returning it
 * to its original mempool.
 *
 * @param   sess     Session header to be freed.
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if session is NULL.
 *  - -EBUSY if not all device private data has been freed.
 */
__rte_experimental
int
rte_cryptodev_asym_session_free(struct rte_cryptodev_asym_session *sess);

/**
 * Fill out private data for the device id, based on its device type.
 *
 * @param   dev_id   ID of device that we want the session to be used on
 * @param   sess     Session where the private data will be attached to
 * @param   xforms   Symmetric crypto transform operations to apply on flow
 *                   processed with this session
 * @param   mempool  Mempool where the private data is allocated.
 *
 * @return
 *  - On success, zero.
 *  - -EINVAL if input parameters are invalid.
 *  - -ENOTSUP if crypto device does not support the crypto transform or
 *    does not support symmetric operations.
 *  - -ENOMEM if the private session could not be allocated.
 */
int
rte_cryptodev_sym_session_init(uint8_t dev_id,
			struct rte_cryptodev_sym_session *sess,
			struct rte_crypto_sym_xform *xforms,
			struct rte_mempool *mempool);

/**
 * Initialize asymmetric session on a device with specific asymmetric xform
 *
 * @param   dev_id   ID of device that we want the session to be used on
 * @param   sess     Session to be set up on a device
 * @param   xforms   Asymmetric crypto transform operations to apply on flow
 *                   processed with this session
 * @param   mempool  Mempool to be used for internal allocation.
 *
 * @return
 *  - On success, zero.
 *  - -EINVAL if input parameters are invalid.
 *  - -ENOTSUP if crypto device does not support the crypto transform.
 *  - -ENOMEM if the private session could not be allocated.
 */
__rte_experimental
int
rte_cryptodev_asym_session_init(uint8_t dev_id,
			struct rte_cryptodev_asym_session *sess,
			struct rte_crypto_asym_xform *xforms,
			struct rte_mempool *mempool);

/**
 * Frees private data for the device id, based on its device type,
 * returning it to its mempool. It is the application's responsibility
 * to ensure that private session data is not cleared while there are
 * still in-flight operations using it.
 *
 * @param   dev_id   ID of device that uses the session.
 * @param   sess     Session containing the reference to the private data
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if device is invalid or session is NULL.
 *  - -ENOTSUP if crypto device does not support symmetric operations.
 */
int
rte_cryptodev_sym_session_clear(uint8_t dev_id,
			struct rte_cryptodev_sym_session *sess);

/**
 * Frees resources held by asymmetric session during rte_cryptodev_session_init
 *
 * @param   dev_id   ID of device that uses the asymmetric session.
 * @param   sess     Asymmetric session setup on device using
 *					 rte_cryptodev_session_init
 * @return
 *  - 0 if successful.
 *  - -EINVAL if device is invalid or session is NULL.
 */
__rte_experimental
int
rte_cryptodev_asym_session_clear(uint8_t dev_id,
			struct rte_cryptodev_asym_session *sess);

/**
 * Get the size of the header session, for all registered drivers excluding
 * the user data size.
 *
 * @return
 *   Size of the symmetric header session.
 */
unsigned int
rte_cryptodev_sym_get_header_session_size(void);

/**
 * Get the size of the header session from created session.
 *
 * @param sess
 *   The sym cryptodev session pointer
 *
 * @return
 *   - If sess is not NULL, return the size of the header session including
 *   the private data size defined within sess.
 *   - If sess is NULL, return 0.
 */
__rte_experimental
unsigned int
rte_cryptodev_sym_get_existing_header_session_size(
		struct rte_cryptodev_sym_session *sess);

/**
 * Get the size of the asymmetric session header, for all registered drivers.
 *
 * @return
 *   Size of the asymmetric header session.
 */
__rte_experimental
unsigned int
rte_cryptodev_asym_get_header_session_size(void);

/**
 * Get the size of the private symmetric session data
 * for a device.
 *
 * @param	dev_id		The device identifier.
 *
 * @return
 *   - Size of the private data, if successful
 *   - 0 if device is invalid or does not have private
 *   symmetric session
 */
unsigned int
rte_cryptodev_sym_get_private_session_size(uint8_t dev_id);

/**
 * Get the size of the private data for asymmetric session
 * on device
 *
 * @param	dev_id		The device identifier.
 *
 * @return
 *   - Size of the asymmetric private data, if successful
 *   - 0 if device is invalid or does not have private session
 */
__rte_experimental
unsigned int
rte_cryptodev_asym_get_private_session_size(uint8_t dev_id);

/**
 * Validate if the crypto device index is valid attached crypto device.
 *
 * @param	dev_id	Crypto device index.
 *
 * @return
 *   - If the device index is valid (1) or not (0).
 */
unsigned int
rte_cryptodev_is_valid_dev(uint8_t dev_id);

/**
 * Provide driver identifier.
 *
 * @param name
 *   The pointer to a driver name.
 * @return
 *  The driver type identifier or -1 if no driver found
 */
int rte_cryptodev_driver_id_get(const char *name);

/**
 * Provide driver name.
 *
 * @param driver_id
 *   The driver identifier.
 * @return
 *  The driver name or null if no driver found
 */
const char *rte_cryptodev_driver_name_get(uint8_t driver_id);

/**
 * Store user data in a session.
 *
 * @param	sess		Session pointer allocated by
 *				*rte_cryptodev_sym_session_create*.
 * @param	data		Pointer to the user data.
 * @param	size		Size of the user data.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
__rte_experimental
int
rte_cryptodev_sym_session_set_user_data(
					struct rte_cryptodev_sym_session *sess,
					void *data,
					uint16_t size);

/**
 * Get user data stored in a session.
 *
 * @param	sess		Session pointer allocated by
 *				*rte_cryptodev_sym_session_create*.
 *
 * @return
 *  - On success return pointer to user data.
 *  - On failure returns NULL.
 */
__rte_experimental
void *
rte_cryptodev_sym_session_get_user_data(
					struct rte_cryptodev_sym_session *sess);

/**
 * Perform actual crypto processing (encrypt/digest or auth/decrypt)
 * on user provided data.
 *
 * @param	dev_id	The device identifier.
 * @param	sess	Cryptodev session structure
 * @param	ofs	Start and stop offsets for auth and cipher operations
 * @param	vec	Vectorized operation descriptor
 *
 * @return
 *  - Returns number of successfully processed packets.
 */
__rte_experimental
uint32_t
rte_cryptodev_sym_cpu_crypto_process(uint8_t dev_id,
	struct rte_cryptodev_sym_session *sess, union rte_crypto_sym_ofs ofs,
	struct rte_crypto_sym_vec *vec);

/**
 * Get the size of the raw data-path context buffer.
 *
 * @param	dev_id		The device identifier.
 *
 * @return
 *   - If the device supports raw data-path APIs, return the context size.
 *   - If the device does not support the APIs, return -1.
 */
__rte_experimental
int
rte_cryptodev_get_raw_dp_ctx_size(uint8_t dev_id);

/**
 * Union of different crypto session types, including session-less xform
 * pointer.
 */
union rte_cryptodev_session_ctx {
	struct rte_cryptodev_sym_session *crypto_sess;
	struct rte_crypto_sym_xform *xform;
	struct rte_security_session *sec_sess;
};

/**
 * Enqueue a vectorized operation descriptor into the device queue but the
 * driver may or may not start processing until rte_cryptodev_raw_enqueue_done()
 * is called.
 *
 * @param	qp		Driver specific queue pair data.
 * @param	drv_ctx		Driver specific context data.
 * @param	vec		Vectorized operation descriptor.
 * @param	ofs		Start and stop offsets for auth and cipher
 *				operations.
 * @param	user_data	The array of user data for dequeue later.
 * @param	enqueue_status	Driver written value to specify the
 *				enqueue status. Possible values:
 *				- 1: The number of operations returned are
 *				     enqueued successfully.
 *				- 0: The number of operations returned are
 *				     cached into the queue but are not processed
 *				     until rte_cryptodev_raw_enqueue_done() is
 *				     called.
 *				- negative integer: Error occurred.
 * @return
 *   - The number of operations in the descriptor successfully enqueued or
 *     cached into the queue but not enqueued yet, depends on the
 *     "enqueue_status" value.
 */
typedef uint32_t (*cryptodev_sym_raw_enqueue_burst_t)(
	void *qp, uint8_t *drv_ctx, struct rte_crypto_sym_vec *vec,
	union rte_crypto_sym_ofs ofs, void *user_data[], int *enqueue_status);

/**
 * Enqueue single raw data vector into the device queue but the driver may or
 * may not start processing until rte_cryptodev_raw_enqueue_done() is called.
 *
 * @param	qp		Driver specific queue pair data.
 * @param	drv_ctx		Driver specific context data.
 * @param	data_vec	The buffer data vector.
 * @param	n_data_vecs	Number of buffer data vectors.
 * @param	ofs		Start and stop offsets for auth and cipher
 *				operations.
 * @param	iv		IV virtual and IOVA addresses
 * @param	digest		digest virtual and IOVA addresses
 * @param	aad_or_auth_iv	AAD or auth IV virtual and IOVA addresses,
 *				depends on the algorithm used.
 * @param	user_data	The user data.
 * @return
 *   - 1: The data vector is enqueued successfully.
 *   - 0: The data vector is cached into the queue but is not processed
 *        until rte_cryptodev_raw_enqueue_done() is called.
 *   - negative integer: failure.
 */
typedef int (*cryptodev_sym_raw_enqueue_t)(
	void *qp, uint8_t *drv_ctx, struct rte_crypto_vec *data_vec,
	uint16_t n_data_vecs, union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad_or_auth_iv,
	void *user_data);

/**
 * Inform the cryptodev queue pair to start processing or finish dequeuing all
 * enqueued/dequeued operations.
 *
 * @param	qp		Driver specific queue pair data.
 * @param	drv_ctx		Driver specific context data.
 * @param	n		The total number of processed operations.
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
typedef int (*cryptodev_sym_raw_operation_done_t)(void *qp, uint8_t *drv_ctx,
	uint32_t n);

/**
 * Typedef that the user provided for the driver to get the dequeue count.
 * The function may return a fixed number or the number parsed from the user
 * data stored in the first processed operation.
 *
 * @param	user_data	Dequeued user data.
 * @return
 *  - The number of operations to be dequeued.
 **/
typedef uint32_t (*rte_cryptodev_raw_get_dequeue_count_t)(void *user_data);

/**
 * Typedef that the user provided to deal with post dequeue operation, such
 * as filling status.
 *
 * @param	user_data	Dequeued user data.
 * @param	index		Index number of the processed descriptor.
 * @param	is_op_success	Operation status provided by the driver.
 **/
typedef void (*rte_cryptodev_raw_post_dequeue_t)(void *user_data,
	uint32_t index, uint8_t is_op_success);

/**
 * Dequeue a burst of symmetric crypto processing.
 *
 * @param	qp			Driver specific queue pair data.
 * @param	drv_ctx			Driver specific context data.
 * @param	get_dequeue_count	User provided callback function to
 *					obtain dequeue operation count.
 * @param	max_nb_to_dequeue	When get_dequeue_count is NULL this
 *					value is used to pass the maximum
 *					number of operations to be dequeued.
 * @param	post_dequeue		User provided callback function to
 *					post-process a dequeued operation.
 * @param	out_user_data		User data pointer array to be retrieve
 *					from device queue. In case of
 *					*is_user_data_array* is set there
 *					should be enough room to store all
 *					user data.
 * @param	is_user_data_array	Set 1 if every dequeued user data will
 *					be written into out_user_data array.
 *					Set 0 if only the first user data will
 *					be written into out_user_data array.
 * @param	n_success		Driver written value to specific the
 *					total successful operations count.
 * @param	dequeue_status		Driver written value to specify the
 *					dequeue status. Possible values:
 *					- 1: Successfully dequeued the number
 *					     of operations returned. The user
 *					     data previously set during enqueue
 *					     is stored in the "out_user_data".
 *					- 0: The number of operations returned
 *					     are completed and the user data is
 *					     stored in the "out_user_data", but
 *					     they are not freed from the queue
 *					     until
 *					     rte_cryptodev_raw_dequeue_done()
 *					     is called.
 *					- negative integer: Error occurred.
 * @return
 *   - The number of operations dequeued or completed but not freed from the
 *     queue, depends on "dequeue_status" value.
 */
typedef uint32_t (*cryptodev_sym_raw_dequeue_burst_t)(void *qp,
	uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success, int *dequeue_status);

/**
 * Dequeue a symmetric crypto processing.
 *
 * @param	qp			Driver specific queue pair data.
 * @param	drv_ctx			Driver specific context data.
 * @param	dequeue_status		Driver written value to specify the
 *					dequeue status. Possible values:
 *					- 1: Successfully dequeued a operation.
 *					     The user data is returned.
 *					- 0: The first operation in the queue
 *					     is completed and the user data
 *					     previously set during enqueue is
 *					     returned, but it is not freed from
 *					     the queue until
 *					     rte_cryptodev_raw_dequeue_done() is
 *					     called.
 *					- negative integer: Error occurred.
 * @param	op_status		Driver written value to specify
 *					operation status.
 * @return
 *   - The user data pointer retrieved from device queue or NULL if no
 *     operation is ready for dequeue.
 */
typedef void * (*cryptodev_sym_raw_dequeue_t)(
		void *qp, uint8_t *drv_ctx, int *dequeue_status,
		enum rte_crypto_op_status *op_status);

/**
 * Context data for raw data-path API crypto process. The buffer of this
 * structure is to be allocated by the user application with the size equal
 * or bigger than rte_cryptodev_get_raw_dp_ctx_size() returned value.
 */
struct rte_crypto_raw_dp_ctx {
	void *qp_data;

	cryptodev_sym_raw_enqueue_t enqueue;
	cryptodev_sym_raw_enqueue_burst_t enqueue_burst;
	cryptodev_sym_raw_operation_done_t enqueue_done;
	cryptodev_sym_raw_dequeue_t dequeue;
	cryptodev_sym_raw_dequeue_burst_t dequeue_burst;
	cryptodev_sym_raw_operation_done_t dequeue_done;

	/* Driver specific context data */
	__extension__ uint8_t drv_ctx_data[];
};

/**
 * Configure raw data-path context data.
 *
 * NOTE:
 * After the context data is configured, the user should call
 * rte_cryptodev_raw_attach_session() before using it in
 * rte_cryptodev_raw_enqueue/dequeue function call.
 *
 * @param	dev_id		The device identifier.
 * @param	qp_id		The index of the queue pair from which to
 *				retrieve processed packets. The value must be
 *				in the range [0, nb_queue_pair - 1] previously
 *				supplied to rte_cryptodev_configure().
 * @param	ctx		The raw data-path context data.
 * @param	sess_type	session type.
 * @param	session_ctx	Session context data.
 * @param	is_update	Set 0 if it is to initialize the ctx.
 *				Set 1 if ctx is initialized and only to update
 *				session context data.
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
__rte_experimental
int
rte_cryptodev_configure_raw_dp_ctx(uint8_t dev_id, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx,
	uint8_t is_update);

/**
 * Enqueue a vectorized operation descriptor into the device queue but the
 * driver may or may not start processing until rte_cryptodev_raw_enqueue_done()
 * is called.
 *
 * @param	ctx		The initialized raw data-path context data.
 * @param	vec		Vectorized operation descriptor.
 * @param	ofs		Start and stop offsets for auth and cipher
 *				operations.
 * @param	user_data	The array of user data for dequeue later.
 * @param	enqueue_status	Driver written value to specify the
 *				enqueue status. Possible values:
 *				- 1: The number of operations returned are
 *				     enqueued successfully.
 *				- 0: The number of operations returned are
 *				     cached into the queue but are not processed
 *				     until rte_cryptodev_raw_enqueue_done() is
 *				     called.
 *				- negative integer: Error occurred.
 * @return
 *   - The number of operations in the descriptor successfully enqueued or
 *     cached into the queue but not enqueued yet, depends on the
 *     "enqueue_status" value.
 */
__rte_experimental
uint32_t
rte_cryptodev_raw_enqueue_burst(struct rte_crypto_raw_dp_ctx *ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void **user_data, int *enqueue_status);

/**
 * Enqueue single raw data vector into the device queue but the driver may or
 * may not start processing until rte_cryptodev_raw_enqueue_done() is called.
 *
 * @param	ctx		The initialized raw data-path context data.
 * @param	data_vec	The buffer data vector.
 * @param	n_data_vecs	Number of buffer data vectors.
 * @param	ofs		Start and stop offsets for auth and cipher
 *				operations.
 * @param	iv		IV virtual and IOVA addresses
 * @param	digest		digest virtual and IOVA addresses
 * @param	aad_or_auth_iv	AAD or auth IV virtual and IOVA addresses,
 *				depends on the algorithm used.
 * @param	user_data	The user data.
 * @return
 *   - 1: The data vector is enqueued successfully.
 *   - 0: The data vector is cached into the queue but is not processed
 *        until rte_cryptodev_raw_enqueue_done() is called.
 *   - negative integer: failure.
 */
__rte_experimental
static __rte_always_inline int
rte_cryptodev_raw_enqueue(struct rte_crypto_raw_dp_ctx *ctx,
	struct rte_crypto_vec *data_vec, uint16_t n_data_vecs,
	union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad_or_auth_iv,
	void *user_data)
{
	return (*ctx->enqueue)(ctx->qp_data, ctx->drv_ctx_data, data_vec,
		n_data_vecs, ofs, iv, digest, aad_or_auth_iv, user_data);
}

/**
 * Start processing all enqueued operations from last
 * rte_cryptodev_configure_raw_dp_ctx() call.
 *
 * @param	ctx	The initialized raw data-path context data.
 * @param	n	The number of operations cached.
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
__rte_experimental
int
rte_cryptodev_raw_enqueue_done(struct rte_crypto_raw_dp_ctx *ctx,
		uint32_t n);

/**
 * Dequeue a burst of symmetric crypto processing.
 *
 * @param	ctx			The initialized raw data-path context
 *					data.
 * @param	get_dequeue_count	User provided callback function to
 *					obtain dequeue operation count.
 * @param	max_nb_to_dequeue	When get_dequeue_count is NULL this
 *					value is used to pass the maximum
 *					number of operations to be dequeued.
 * @param	post_dequeue		User provided callback function to
 *					post-process a dequeued operation.
 * @param	out_user_data		User data pointer array to be retrieve
 *					from device queue. In case of
 *					*is_user_data_array* is set there
 *					should be enough room to store all
 *					user data.
 * @param	is_user_data_array	Set 1 if every dequeued user data will
 *					be written into out_user_data array.
 *					Set 0 if only the first user data will
 *					be written into out_user_data array.
 * @param	n_success		Driver written value to specific the
 *					total successful operations count.
 * @param	dequeue_status		Driver written value to specify the
 *					dequeue status. Possible values:
 *					- 1: Successfully dequeued the number
 *					     of operations returned. The user
 *					     data previously set during enqueue
 *					     is stored in the "out_user_data".
 *					- 0: The number of operations returned
 *					     are completed and the user data is
 *					     stored in the "out_user_data", but
 *					     they are not freed from the queue
 *					     until
 *					     rte_cryptodev_raw_dequeue_done()
 *					     is called.
 *					- negative integer: Error occurred.
 * @return
 *   - The number of operations dequeued or completed but not freed from the
 *     queue, depends on "dequeue_status" value.
 */
__rte_experimental
uint32_t
rte_cryptodev_raw_dequeue_burst(struct rte_crypto_raw_dp_ctx *ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success, int *dequeue_status);

/**
 * Dequeue a symmetric crypto processing.
 *
 * @param	ctx			The initialized raw data-path context
 *					data.
 * @param	dequeue_status		Driver written value to specify the
 *					dequeue status. Possible values:
 *					- 1: Successfully dequeued a operation.
 *					     The user data is returned.
 *					- 0: The first operation in the queue
 *					     is completed and the user data
 *					     previously set during enqueue is
 *					     returned, but it is not freed from
 *					     the queue until
 *					     rte_cryptodev_raw_dequeue_done() is
 *					     called.
 *					- negative integer: Error occurred.
 * @param	op_status		Driver written value to specify
 *					operation status.
 * @return
 *   - The user data pointer retrieved from device queue or NULL if no
 *     operation is ready for dequeue.
 */
__rte_experimental
static __rte_always_inline void *
rte_cryptodev_raw_dequeue(struct rte_crypto_raw_dp_ctx *ctx,
		int *dequeue_status, enum rte_crypto_op_status *op_status)
{
	return (*ctx->dequeue)(ctx->qp_data, ctx->drv_ctx_data, dequeue_status,
			op_status);
}

/**
 * Inform the queue pair dequeue operations is finished.
 *
 * @param	ctx	The initialized raw data-path context data.
 * @param	n	The number of operations.
 * @return
 *   - On success return 0.
 *   - On failure return negative integer.
 */
__rte_experimental
int
rte_cryptodev_raw_dequeue_done(struct rte_crypto_raw_dp_ctx *ctx,
		uint32_t n);

/**
 * Add a user callback for a given crypto device and queue pair which will be
 * called on crypto ops enqueue.
 *
 * This API configures a function to be called for each burst of crypto ops
 * received on a given crypto device queue pair. The return value is a pointer
 * that can be used later to remove the callback using
 * rte_cryptodev_remove_enq_callback().
 *
 * Callbacks registered by application would not survive
 * rte_cryptodev_configure() as it reinitializes the callback list.
 * It is user responsibility to remove all installed callbacks before
 * calling rte_cryptodev_configure() to avoid possible memory leakage.
 * Application is expected to call add API after rte_cryptodev_configure().
 *
 * Multiple functions can be registered per queue pair & they are called
 * in the order they were added. The API does not restrict on maximum number
 * of callbacks.
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair on which ops are
 *				to be enqueued for processing. The value
 *				must be in the range [0, nb_queue_pairs - 1]
 *				previously supplied to
 *				*rte_cryptodev_configure*.
 * @param	cb_fn		The callback function
 * @param	cb_arg		A generic pointer parameter which will be passed
 *				to each invocation of the callback function on
 *				this crypto device and queue pair.
 *
 * @return
 *  - NULL on error & rte_errno will contain the error code.
 *  - On success, a pointer value which can later be used to remove the
 *    callback.
 */

__rte_experimental
struct rte_cryptodev_cb *
rte_cryptodev_add_enq_callback(uint8_t dev_id,
			       uint16_t qp_id,
			       rte_cryptodev_callback_fn cb_fn,
			       void *cb_arg);

/**
 * Remove a user callback function for given crypto device and queue pair.
 *
 * This function is used to remove enqueue callbacks that were added to a
 * crypto device queue pair using rte_cryptodev_add_enq_callback().
 *
 *
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair on which ops are
 *				to be enqueued. The value must be in the
 *				range [0, nb_queue_pairs - 1] previously
 *				supplied to *rte_cryptodev_configure*.
 * @param	cb		Pointer to user supplied callback created via
 *				rte_cryptodev_add_enq_callback().
 *
 * @return
 *   -  0: Success. Callback was removed.
 *   - <0: The dev_id or the qp_id is out of range, or the callback
 *         is NULL or not found for the crypto device queue pair.
 */

__rte_experimental
int rte_cryptodev_remove_enq_callback(uint8_t dev_id,
				      uint16_t qp_id,
				      struct rte_cryptodev_cb *cb);

/**
 * Add a user callback for a given crypto device and queue pair which will be
 * called on crypto ops dequeue.
 *
 * This API configures a function to be called for each burst of crypto ops
 * received on a given crypto device queue pair. The return value is a pointer
 * that can be used later to remove the callback using
 * rte_cryptodev_remove_deq_callback().
 *
 * Callbacks registered by application would not survive
 * rte_cryptodev_configure() as it reinitializes the callback list.
 * It is user responsibility to remove all installed callbacks before
 * calling rte_cryptodev_configure() to avoid possible memory leakage.
 * Application is expected to call add API after rte_cryptodev_configure().
 *
 * Multiple functions can be registered per queue pair & they are called
 * in the order they were added. The API does not restrict on maximum number
 * of callbacks.
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair on which ops are
 *				to be dequeued. The value must be in the
 *				range [0, nb_queue_pairs - 1] previously
 *				supplied to *rte_cryptodev_configure*.
 * @param	cb_fn		The callback function
 * @param	cb_arg		A generic pointer parameter which will be passed
 *				to each invocation of the callback function on
 *				this crypto device and queue pair.
 *
 * @return
 *   - NULL on error & rte_errno will contain the error code.
 *   - On success, a pointer value which can later be used to remove the
 *     callback.
 */

__rte_experimental
struct rte_cryptodev_cb *
rte_cryptodev_add_deq_callback(uint8_t dev_id,
			       uint16_t qp_id,
			       rte_cryptodev_callback_fn cb_fn,
			       void *cb_arg);

/**
 * Remove a user callback function for given crypto device and queue pair.
 *
 * This function is used to remove dequeue callbacks that were added to a
 * crypto device queue pair using rte_cryptodev_add_deq_callback().
 *
 *
 *
 * @param	dev_id		The identifier of the device.
 * @param	qp_id		The index of the queue pair on which ops are
 *				to be dequeued. The value must be in the
 *				range [0, nb_queue_pairs - 1] previously
 *				supplied to *rte_cryptodev_configure*.
 * @param	cb		Pointer to user supplied callback created via
 *				rte_cryptodev_add_deq_callback().
 *
 * @return
 *   -  0: Success. Callback was removed.
 *   - <0: The dev_id or the qp_id is out of range, or the callback
 *         is NULL or not found for the crypto device queue pair.
 */
__rte_experimental
int rte_cryptodev_remove_deq_callback(uint8_t dev_id,
				      uint16_t qp_id,
				      struct rte_cryptodev_cb *cb);

#include <rte_cryptodev_core.h>
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
	const struct rte_crypto_fp_ops *fp_ops;
	void *qp;

	rte_cryptodev_trace_dequeue_burst(dev_id, qp_id, (void **)ops, nb_ops);

	fp_ops = &rte_crypto_fp_ops[dev_id];
	qp = fp_ops->qp.data[qp_id];

	nb_ops = fp_ops->dequeue_burst(qp, ops, nb_ops);

#ifdef RTE_CRYPTO_CALLBACKS
	if (unlikely(fp_ops->qp.deq_cb != NULL)) {
		struct rte_cryptodev_cb_rcu *list;
		struct rte_cryptodev_cb *cb;

		/* __ATOMIC_RELEASE memory order was used when the
		 * call back was inserted into the list.
		 * Since there is a clear dependency between loading
		 * cb and cb->fn/cb->next, __ATOMIC_ACQUIRE memory order is
		 * not required.
		 */
		list = &fp_ops->qp.deq_cb[qp_id];
		rte_rcu_qsbr_thread_online(list->qsbr, 0);
		cb = __atomic_load_n(&list->next, __ATOMIC_RELAXED);

		while (cb != NULL) {
			nb_ops = cb->fn(dev_id, qp_id, ops, nb_ops,
					cb->arg);
			cb = cb->next;
		};

		rte_rcu_qsbr_thread_offline(list->qsbr, 0);
	}
#endif
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
	const struct rte_crypto_fp_ops *fp_ops;
	void *qp;

	fp_ops = &rte_crypto_fp_ops[dev_id];
	qp = fp_ops->qp.data[qp_id];
#ifdef RTE_CRYPTO_CALLBACKS
	if (unlikely(fp_ops->qp.enq_cb != NULL)) {
		struct rte_cryptodev_cb_rcu *list;
		struct rte_cryptodev_cb *cb;

		/* __ATOMIC_RELEASE memory order was used when the
		 * call back was inserted into the list.
		 * Since there is a clear dependency between loading
		 * cb and cb->fn/cb->next, __ATOMIC_ACQUIRE memory order is
		 * not required.
		 */
		list = &fp_ops->qp.enq_cb[qp_id];
		rte_rcu_qsbr_thread_online(list->qsbr, 0);
		cb = __atomic_load_n(&list->next, __ATOMIC_RELAXED);

		while (cb != NULL) {
			nb_ops = cb->fn(dev_id, qp_id, ops, nb_ops,
					cb->arg);
			cb = cb->next;
		};

		rte_rcu_qsbr_thread_offline(list->qsbr, 0);
	}
#endif

	rte_cryptodev_trace_enqueue_burst(dev_id, qp_id, (void **)ops, nb_ops);
	return fp_ops->enqueue_burst(qp, ops, nb_ops);
}



#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_H_ */
