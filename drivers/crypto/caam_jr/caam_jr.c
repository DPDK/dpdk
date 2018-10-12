/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <net/if.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_security_driver.h>
#include <rte_hexdump.h>

#include <caam_jr_config.h>
#include <caam_jr_hw_specific.h>
#include <caam_jr_pvt.h>
#include <caam_jr_log.h>

/* RTA header files */
#include <hw/desc/common.h>
#include <hw/desc/algo.h>
#include <hw/desc/ipsec.h>
#include <of.h>

#define CAAM_JR_DBG	0
#define CRYPTODEV_NAME_CAAM_JR_PMD	crypto_caam_jr
static uint8_t cryptodev_driver_id;
int caam_jr_logtype;

enum rta_sec_era rta_sec_era;

/* Lists the states possible for the SEC user space driver. */
enum sec_driver_state_e {
	SEC_DRIVER_STATE_IDLE,		/* Driver not initialized */
	SEC_DRIVER_STATE_STARTED,	/* Driver initialized and can be used*/
	SEC_DRIVER_STATE_RELEASE,	/* Driver release is in progress */
};

/* Job rings used for communication with SEC HW */
static struct sec_job_ring_t g_job_rings[MAX_SEC_JOB_RINGS];

/* The current state of SEC user space driver */
static enum sec_driver_state_e g_driver_state = SEC_DRIVER_STATE_IDLE;

/* The number of job rings used by SEC user space driver */
static int g_job_rings_no;
static int g_job_rings_max;

/* @brief Poll the HW for already processed jobs in the JR
 * and silently discard the available jobs or notify them to UA
 * with indicated error code.
 *
 * @param [in,out]  job_ring        The job ring to poll.
 * @param [in]  do_notify           Can be #TRUE or #FALSE. Indicates if
 *				    descriptors are to be discarded
 *                                  or notified to UA with given error_code.
 * @param [out] notified_descs    Number of notified descriptors. Can be NULL
 *					if do_notify is #FALSE
 */
static void
hw_flush_job_ring(struct sec_job_ring_t *job_ring,
		  uint32_t do_notify,
		  uint32_t *notified_descs)
{
	int32_t jobs_no_to_discard = 0;
	int32_t discarded_descs_no = 0;

	PMD_INIT_FUNC_TRACE();
	CAAM_JR_DEBUG("Jr[%p] pi[%d] ci[%d].Flushing jr notify desc=[%d]",
		job_ring, job_ring->pidx, job_ring->cidx, do_notify);

	jobs_no_to_discard = hw_get_no_finished_jobs(job_ring);

	/* Discard all jobs */
	CAAM_JR_DEBUG("Jr[%p] pi[%d] ci[%d].Discarding %d descs",
		  job_ring, job_ring->pidx, job_ring->cidx,
		  jobs_no_to_discard);

	while (jobs_no_to_discard > discarded_descs_no) {
		discarded_descs_no++;
		/* Now increment the consumer index for the current job ring,
		 * AFTER saving job in temporary location!
		 * Increment the consumer index for the current job ring
		 */
		job_ring->cidx = SEC_CIRCULAR_COUNTER(job_ring->cidx,
					 SEC_JOB_RING_SIZE);

		hw_remove_entries(job_ring, 1);
	}

	if (do_notify == true) {
		ASSERT(notified_descs != NULL);
		*notified_descs = discarded_descs_no;
	}
}

/* Release queue pair */
static int
caam_jr_queue_pair_release(struct rte_cryptodev *dev,
			   uint16_t qp_id)
{
	struct sec_job_ring_t *internals;
	struct caam_jr_qp *qp = NULL;

	PMD_INIT_FUNC_TRACE();
	CAAM_JR_DEBUG("dev =%p, queue =%d", dev, qp_id);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		CAAM_JR_ERR("Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->ring = NULL;
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/* Setup a queue pair */
static int
caam_jr_queue_pair_setup(
		struct rte_cryptodev *dev, uint16_t qp_id,
		__rte_unused const struct rte_cryptodev_qp_conf *qp_conf,
		__rte_unused int socket_id,
		__rte_unused struct rte_mempool *session_pool)
{
	struct sec_job_ring_t *internals;
	struct caam_jr_qp *qp = NULL;

	PMD_INIT_FUNC_TRACE();
	CAAM_JR_DEBUG("dev =%p, queue =%d, conf =%p", dev, qp_id, qp_conf);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		CAAM_JR_ERR("Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->ring = internals;
	dev->data->queue_pairs[qp_id] = qp;

	return 0;
}

/* Return the number of allocated queue pairs */
static uint32_t
caam_jr_queue_pair_count(struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();

	return dev->data->nb_queue_pairs;
}

/* Returns the size of the aesni gcm session structure */
static unsigned int
caam_jr_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return sizeof(struct caam_jr_session);
}

static int
caam_jr_cipher_init(struct rte_cryptodev *dev __rte_unused,
		    struct rte_crypto_sym_xform *xform,
		    struct caam_jr_session *session)
{
	PMD_INIT_FUNC_TRACE();
	session->cipher_alg = xform->cipher.algo;
	session->iv.length = xform->cipher.iv.length;
	session->iv.offset = xform->cipher.iv.offset;
	session->cipher_key.data = rte_zmalloc(NULL, xform->cipher.key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL && xform->cipher.key.length > 0) {
		CAAM_JR_ERR("No Memory for cipher key\n");
		return -ENOMEM;
	}
	session->cipher_key.length = xform->cipher.key.length;

	memcpy(session->cipher_key.data, xform->cipher.key.data,
	       xform->cipher.key.length);
	session->dir = (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
caam_jr_auth_init(struct rte_cryptodev *dev __rte_unused,
		  struct rte_crypto_sym_xform *xform,
		  struct caam_jr_session *session)
{
	PMD_INIT_FUNC_TRACE();
	session->auth_alg = xform->auth.algo;
	session->auth_key.data = rte_zmalloc(NULL, xform->auth.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->auth_key.data == NULL && xform->auth.key.length > 0) {
		CAAM_JR_ERR("No Memory for auth key\n");
		return -ENOMEM;
	}
	session->auth_key.length = xform->auth.key.length;
	session->digest_length = xform->auth.digest_length;

	memcpy(session->auth_key.data, xform->auth.key.data,
	       xform->auth.key.length);
	session->dir = (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
caam_jr_aead_init(struct rte_cryptodev *dev __rte_unused,
		  struct rte_crypto_sym_xform *xform,
		  struct caam_jr_session *session)
{
	PMD_INIT_FUNC_TRACE();
	session->aead_alg = xform->aead.algo;
	session->iv.length = xform->aead.iv.length;
	session->iv.offset = xform->aead.iv.offset;
	session->auth_only_len = xform->aead.aad_length;
	session->aead_key.data = rte_zmalloc(NULL, xform->aead.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->aead_key.data == NULL && xform->aead.key.length > 0) {
		CAAM_JR_ERR("No Memory for aead key\n");
		return -ENOMEM;
	}
	session->aead_key.length = xform->aead.key.length;
	session->digest_length = xform->aead.digest_length;

	memcpy(session->aead_key.data, xform->aead.key.data,
	       xform->aead.key.length);
	session->dir = (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
caam_jr_set_session_parameters(struct rte_cryptodev *dev,
			       struct rte_crypto_sym_xform *xform, void *sess)
{
	struct sec_job_ring_t *internals = dev->data->dev_private;
	struct caam_jr_session *session = sess;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(sess == NULL)) {
		CAAM_JR_ERR("invalid session struct");
		return -EINVAL;
	}

	/* Default IV length = 0 */
	session->iv.length = 0;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL) {
		session->auth_alg = RTE_CRYPTO_AUTH_NULL;
		caam_jr_cipher_init(dev, xform, session);

	/* Authentication Only */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next == NULL) {
		session->cipher_alg = RTE_CRYPTO_CIPHER_NULL;
		caam_jr_auth_init(dev, xform, session);

	/* Cipher then Authenticate */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			caam_jr_cipher_init(dev, xform, session);
			caam_jr_auth_init(dev, xform->next, session);
		} else {
			CAAM_JR_ERR("Not supported: Auth then Cipher");
			goto err1;
		}

	/* Authenticate then Cipher */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			caam_jr_auth_init(dev, xform, session);
			caam_jr_cipher_init(dev, xform->next, session);
		} else {
			CAAM_JR_ERR("Not supported: Auth then Cipher");
			goto err1;
		}

	/* AEAD operation for AES-GCM kind of Algorithms */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
		   xform->next == NULL) {
		caam_jr_aead_init(dev, xform, session);

	} else {
		CAAM_JR_ERR("Invalid crypto type");
		return -EINVAL;
	}
	session->ctx_pool = internals->ctx_pool;

	return 0;

err1:
	rte_free(session->cipher_key.data);
	rte_free(session->auth_key.data);
	memset(session, 0, sizeof(struct caam_jr_session));

	return -EINVAL;
}

static int
caam_jr_sym_session_configure(struct rte_cryptodev *dev,
			      struct rte_crypto_sym_xform *xform,
			      struct rte_cryptodev_sym_session *sess,
			      struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CAAM_JR_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}

	memset(sess_private_data, 0, sizeof(struct caam_jr_session));
	ret = caam_jr_set_session_parameters(dev, xform, sess_private_data);
	if (ret != 0) {
		CAAM_JR_ERR("failed to configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id, sess_private_data);

	return 0;
}

/* Clear the memory of session so it doesn't leave key material behind */
static void
caam_jr_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);
	struct caam_jr_session *s = (struct caam_jr_session *)sess_priv;

	PMD_INIT_FUNC_TRACE();

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(s, 0, sizeof(struct caam_jr_session));
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

static int
caam_jr_dev_configure(struct rte_cryptodev *dev,
		       struct rte_cryptodev_config *config __rte_unused)
{
	char str[20];
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();

	internals = dev->data->dev_private;
	sprintf(str, "ctx_pool_%d", dev->data->dev_id);
	if (!internals->ctx_pool) {
		internals->ctx_pool = rte_mempool_create((const char *)str,
						CTX_POOL_NUM_BUFS,
						sizeof(struct caam_jr_op_ctx),
						CTX_POOL_CACHE_SIZE, 0,
						NULL, NULL, NULL, NULL,
						SOCKET_ID_ANY, 0);
		if (!internals->ctx_pool) {
			CAAM_JR_ERR("%s create failed\n", str);
			return -ENOMEM;
		}
	} else
		CAAM_JR_INFO("mempool already created for dev_id : %d",
				dev->data->dev_id);

	return 0;
}

static int
caam_jr_dev_start(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

static void
caam_jr_dev_stop(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static int
caam_jr_dev_close(struct rte_cryptodev *dev)
{
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();

	if (dev == NULL)
		return -ENOMEM;

	internals = dev->data->dev_private;
	rte_mempool_free(internals->ctx_pool);
	internals->ctx_pool = NULL;

	return 0;
}

static void
caam_jr_dev_infos_get(struct rte_cryptodev *dev,
		       struct rte_cryptodev_info *info)
{
	struct sec_job_ring_t *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		info->feature_flags = dev->feature_flags;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->driver_id = cryptodev_driver_id;
	}
}

static struct rte_cryptodev_ops caam_jr_ops = {
	.dev_configure	      = caam_jr_dev_configure,
	.dev_start	      = caam_jr_dev_start,
	.dev_stop	      = caam_jr_dev_stop,
	.dev_close	      = caam_jr_dev_close,
	.dev_infos_get        = caam_jr_dev_infos_get,
	.queue_pair_setup     = caam_jr_queue_pair_setup,
	.queue_pair_release   = caam_jr_queue_pair_release,
	.queue_pair_count     = caam_jr_queue_pair_count,
	.sym_session_get_size = caam_jr_sym_session_get_size,
	.sym_session_configure = caam_jr_sym_session_configure,
	.sym_session_clear    = caam_jr_sym_session_clear
};


/* @brief Flush job rings of any processed descs.
 * The processed descs are silently dropped,
 * WITHOUT being notified to UA.
 */
static void
close_job_ring(struct sec_job_ring_t *job_ring)
{
	PMD_INIT_FUNC_TRACE();
	if (job_ring->irq_fd) {
		/* Producer index is frozen. If consumer index is not equal
		 * with producer index, then we have descs to flush.
		 */
		while (job_ring->pidx != job_ring->cidx)
			hw_flush_job_ring(job_ring, false, NULL);

		/* free the uio job ring */
		free_job_ring(job_ring->irq_fd);
		job_ring->irq_fd = 0;
		caam_jr_dma_free(job_ring->input_ring);
		caam_jr_dma_free(job_ring->output_ring);
		g_job_rings_no--;
	}
}

/** @brief Release the software and hardware resources tied to a job ring.
 * @param [in] job_ring The job ring
 *
 * @retval  0 for success
 * @retval  -1 for error
 */
static int
shutdown_job_ring(struct sec_job_ring_t *job_ring)
{
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	ASSERT(job_ring != NULL);
	ret = hw_shutdown_job_ring(job_ring);
	SEC_ASSERT(ret == 0, ret,
		"Failed to shutdown hardware job ring %p",
		job_ring);

	if (job_ring->coalescing_en)
		hw_job_ring_disable_coalescing(job_ring);

	if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL) {
		ret = caam_jr_disable_irqs(job_ring->irq_fd);
		SEC_ASSERT(ret == 0, ret,
		"Failed to disable irqs for job ring %p",
		job_ring);
	}

	return ret;
}

/*
 * @brief Release the resources used by the SEC user space driver.
 *
 * Reset and release SEC's job rings indicated by the User Application at
 * init_job_ring() and free any memory allocated internally.
 * Call once during application tear down.
 *
 * @note In case there are any descriptors in-flight (descriptors received by
 * SEC driver for processing and for which no response was yet provided to UA),
 * the descriptors are discarded without any notifications to User Application.
 *
 * @retval ::0			is returned for a successful execution
 * @retval ::-1		is returned if SEC driver release is in progress
 */
static int
caam_jr_dev_uninit(struct rte_cryptodev *dev)
{
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();
	if (dev == NULL)
		return -ENODEV;

	internals = dev->data->dev_private;
	rte_free(dev->security_ctx);

	/* If any descriptors in flight , poll and wait
	 * until all descriptors are received and silently discarded.
	 */
	if (internals) {
		shutdown_job_ring(internals);
		close_job_ring(internals);
		rte_mempool_free(internals->ctx_pool);
	}

	CAAM_JR_INFO("Closing crypto device %s", dev->data->name);

	/* last caam jr instance) */
	if (g_job_rings_no == 0)
		g_driver_state = SEC_DRIVER_STATE_IDLE;

	return SEC_SUCCESS;
}

/* @brief Initialize the software and hardware resources tied to a job ring.
 * @param [in] jr_mode;		Model to be used by SEC Driver to receive
 *				notifications from SEC.  Can be either
 *				of the three: #SEC_NOTIFICATION_TYPE_NAPI
 *				#SEC_NOTIFICATION_TYPE_IRQ or
 *				#SEC_NOTIFICATION_TYPE_POLL
 * @param [in] NAPI_mode	The NAPI work mode to configure a job ring at
 *				startup. Used only when #SEC_NOTIFICATION_TYPE
 *				is set to #SEC_NOTIFICATION_TYPE_NAPI.
 * @param [in] irq_coalescing_timer This value determines the maximum
 *					amount of time after processing a
 *					descriptor before raising an interrupt.
 * @param [in] irq_coalescing_count This value determines how many
 *					descriptors are completed before
 *					raising an interrupt.
 * @param [in] reg_base_addr,	The job ring base address register
 * @param [in] irq_id		The job ring interrupt identification number.
 * @retval  job_ring_handle for successful job ring configuration
 * @retval  NULL on error
 *
 */
static void *
init_job_ring(void *reg_base_addr, uint32_t irq_id)
{
	struct sec_job_ring_t *job_ring = NULL;
	int i, ret = 0;
	int jr_mode = SEC_NOTIFICATION_TYPE_POLL;
	int napi_mode = 0;
	int irq_coalescing_timer = 0;
	int irq_coalescing_count = 0;

	for (i = 0; i < MAX_SEC_JOB_RINGS; i++) {
		if (g_job_rings[i].irq_fd == 0) {
			job_ring = &g_job_rings[i];
			g_job_rings_no++;
			break;
		}
	}
	if (job_ring == NULL) {
		CAAM_JR_ERR("No free job ring\n");
		return NULL;
	}

	job_ring->register_base_addr = reg_base_addr;
	job_ring->jr_mode = jr_mode;
	job_ring->napi_mode = 0;
	job_ring->irq_fd = irq_id;

	/* Allocate mem for input and output ring */

	/* Allocate memory for input ring */
	job_ring->input_ring = caam_jr_dma_mem_alloc(L1_CACHE_BYTES,
				SEC_DMA_MEM_INPUT_RING_SIZE);
	memset(job_ring->input_ring, 0, SEC_DMA_MEM_INPUT_RING_SIZE);

	/* Allocate memory for output ring */
	job_ring->output_ring = caam_jr_dma_mem_alloc(L1_CACHE_BYTES,
				SEC_DMA_MEM_OUTPUT_RING_SIZE);
	memset(job_ring->output_ring, 0, SEC_DMA_MEM_OUTPUT_RING_SIZE);

	/* Reset job ring in SEC hw and configure job ring registers */
	ret = hw_reset_job_ring(job_ring);
	if (ret != 0) {
		CAAM_JR_ERR("Failed to reset hardware job ring");
		goto cleanup;
	}

	if (jr_mode == SEC_NOTIFICATION_TYPE_NAPI) {
	/* When SEC US driver works in NAPI mode, the UA can select
	 * if the driver starts with IRQs on or off.
	 */
		if (napi_mode == SEC_STARTUP_INTERRUPT_MODE) {
			CAAM_JR_INFO("Enabling DONE IRQ generationon job ring - %p",
				job_ring);
			ret = caam_jr_enable_irqs(job_ring->irq_fd);
			if (ret != 0) {
				CAAM_JR_ERR("Failed to enable irqs for job ring");
				goto cleanup;
			}
		}
	} else if (jr_mode == SEC_NOTIFICATION_TYPE_IRQ) {
	/* When SEC US driver works in pure interrupt mode,
	 * IRQ's are always enabled.
	 */
		CAAM_JR_INFO("Enabling DONE IRQ generation on job ring - %p",
			 job_ring);
		ret = caam_jr_enable_irqs(job_ring->irq_fd);
		if (ret != 0) {
			CAAM_JR_ERR("Failed to enable irqs for job ring");
			goto cleanup;
		}
	}
	if (irq_coalescing_timer || irq_coalescing_count) {
		hw_job_ring_set_coalescing_param(job_ring,
			 irq_coalescing_timer,
			 irq_coalescing_count);

		hw_job_ring_enable_coalescing(job_ring);
		job_ring->coalescing_en = 1;
	}

	job_ring->jr_state = SEC_JOB_RING_STATE_STARTED;
	job_ring->max_nb_queue_pairs = RTE_CAAM_MAX_NB_SEC_QPS;
	job_ring->max_nb_sessions = RTE_CAAM_JR_PMD_MAX_NB_SESSIONS;

	return job_ring;
cleanup:
	caam_jr_dma_free(job_ring->output_ring);
	caam_jr_dma_free(job_ring->input_ring);
	return NULL;
}


static int
caam_jr_dev_init(const char *name,
		 struct rte_vdev_device *vdev,
		 struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct uio_job_ring *job_ring;
	char str[RTE_CRYPTODEV_NAME_MAX_LEN];

	PMD_INIT_FUNC_TRACE();

	/* Validate driver state */
	if (g_driver_state == SEC_DRIVER_STATE_IDLE) {
		g_job_rings_max = sec_configure();
		if (!g_job_rings_max) {
			CAAM_JR_ERR("No job ring detected on UIO !!!!");
			return -1;
		}
		/* Update driver state */
		g_driver_state = SEC_DRIVER_STATE_STARTED;
	}

	if (g_job_rings_no >= g_job_rings_max) {
		CAAM_JR_ERR("No more job rings available max=%d!!!!",
				g_job_rings_max);
		return -1;
	}

	job_ring = config_job_ring();
	if (job_ring == NULL) {
		CAAM_JR_ERR("failed to create job ring");
		goto init_error;
	}

	snprintf(str, sizeof(str), "caam_jr%d", job_ring->jr_id);

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		CAAM_JR_ERR("failed to create cryptodev vdev");
		goto cleanup;
	}
	/*TODO free it during teardown*/
	dev->data->dev_private = init_job_ring(job_ring->register_base_addr,
						job_ring->uio_fd);

	if (!dev->data->dev_private) {
		CAAM_JR_ERR("Ring memory allocation failed\n");
		goto cleanup2;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = &caam_jr_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = NULL;
	dev->enqueue_burst = NULL;
	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_SECURITY |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		CAAM_JR_WARN("Device already init by primary process");
		return 0;
	}

	RTE_LOG(INFO, PMD, "%s cryptodev init\n", dev->data->name);

	return 0;

cleanup2:
	caam_jr_dev_uninit(dev);
	rte_cryptodev_pmd_release_device(dev);
cleanup:
	free_job_ring(job_ring->uio_fd);
init_error:
	CAAM_JR_ERR("driver %s: cryptodev_caam_jr_create failed",
			init_params->name);

	return -ENXIO;
}

/** Initialise CAAM JR crypto device */
static int
cryptodev_caam_jr_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct sec_job_ring_t),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

	/* if sec device version is not configured */
	if (!rta_get_sec_era()) {
		const struct device_node *caam_node;

		for_each_compatible_node(caam_node, NULL, "fsl,sec-v4.0") {
			const uint32_t *prop = of_get_property(caam_node,
					"fsl,sec-era",
					NULL);
			if (prop) {
				rta_set_sec_era(
					INTL_SEC_ERA(cpu_to_caam32(*prop)));
				break;
			}
		}
	}
#ifdef RTE_LIBRTE_PMD_CAAM_JR_BE
	if (rta_get_sec_era() > RTA_SEC_ERA_8) {
		RTE_LOG(ERR, PMD,
		"CAAM is compiled in BE mode for device with sec era > 8???\n");
		return -EINVAL;
	}
#endif

	return caam_jr_dev_init(name, vdev, &init_params);
}

/** Uninitialise CAAM JR crypto device */
static int
cryptodev_caam_jr_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	caam_jr_dev_uninit(cryptodev);

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_caam_jr_drv = {
	.probe = cryptodev_caam_jr_probe,
	.remove = cryptodev_caam_jr_remove
};

static struct cryptodev_driver caam_jr_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CAAM_JR_PMD, cryptodev_caam_jr_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CAAM_JR_PMD,
	"max_nb_queue_pairs=<int>"
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(caam_jr_crypto_drv, cryptodev_caam_jr_drv.driver,
		cryptodev_driver_id);

RTE_INIT(caam_jr_init_log)
{
	caam_jr_logtype = rte_log_register("pmd.crypto.caam");
	if (caam_jr_logtype >= 0)
		rte_log_set_level(caam_jr_logtype, RTE_LOG_NOTICE);
}
