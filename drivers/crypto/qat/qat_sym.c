/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2023 Intel Corporation
 */

#include <openssl/evp.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_crypto_sym.h>
#include <bus_pci_driver.h>
#include <rte_byteorder.h>
#include <rte_security_driver.h>

#include "qat_sym.h"
#include "qat_crypto.h"
#include "qat_qp.h"

uint8_t qat_sym_driver_id;
int qat_legacy_capa;

struct qat_crypto_gen_dev_ops qat_sym_gen_dev_ops[QAT_N_GENS];

/* An rte_driver is needed in the registration of both the device and the driver
 * with cryptodev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the crypto part of the pci device.
 */
static const char qat_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD);
static const struct rte_driver cryptodev_qat_sym_driver = {
	.name = qat_sym_drv_name,
	.alias = qat_sym_drv_name
};

void
qat_sym_init_op_cookie(void *op_cookie)
{
	struct qat_sym_op_cookie *cookie = op_cookie;

	cookie->qat_sgl_src_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_src);

	cookie->qat_sgl_dst_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_dst);

	cookie->opt.spc_gmac.cd_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			opt.spc_gmac.cd_cipher);

	cookie->digest_null_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			digest_null);
}

static __rte_always_inline int
qat_sym_build_request(void *in_op, uint8_t *out_msg,
		void *op_cookie, uint64_t *opaque, enum qat_device_gen dev_gen)
{
	struct rte_crypto_op *op = (struct rte_crypto_op *)in_op;
	uintptr_t sess = (uintptr_t)opaque[0];
	uintptr_t build_request_p = (uintptr_t)opaque[1];
	qat_sym_build_request_t build_request = (void *)build_request_p;
	struct qat_sym_session *ctx = NULL;
	enum rte_proc_type_t proc_type = rte_eal_process_type();

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	if (likely(op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)) {
		ctx = (void *)CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
		if (sess != (uintptr_t)ctx) {
			struct rte_cryptodev *cdev;
			struct qat_cryptodev_private *internals;

			cdev = rte_cryptodev_pmd_get_dev(ctx->dev_id);
			internals = cdev->data->dev_private;

			if (internals->qat_dev->qat_dev_gen != dev_gen) {
				op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
				return -EINVAL;
			}

			if (unlikely(ctx->build_request[proc_type] == NULL)) {
				int ret =
				qat_sym_gen_dev_ops[dev_gen].set_session(
					(void *)cdev, (void *)ctx);
				if (ret < 0) {
					op->status =
						RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
					return -EINVAL;
				}
			}

			build_request = ctx->build_request[proc_type];
			opaque[0] = (uintptr_t)ctx;
			opaque[1] = (uintptr_t)build_request;
		}
	} else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		ctx = SECURITY_GET_SESS_PRIV(op->sym->session);
		if (unlikely(!ctx)) {
			QAT_DP_LOG(ERR, "No session for this device");
			return -EINVAL;
		}
		if (sess != (uintptr_t)ctx) {
			struct rte_cryptodev *cdev;
			struct qat_cryptodev_private *internals;

#ifdef RTE_QAT_OPENSSL
			if (unlikely(ctx->bpi_ctx == NULL)) {
#else
			if (unlikely(ctx->mb_mgr == NULL)) {
#endif
				QAT_DP_LOG(ERR, "QAT PMD only supports security"
						" operation requests for"
						" DOCSIS, op (%p) is not for"
						" DOCSIS.", op);
				return -EINVAL;
			} else if (unlikely(((op->sym->m_dst != NULL) &&
					(op->sym->m_dst != op->sym->m_src)) ||
					op->sym->m_src->nb_segs > 1)) {
				QAT_DP_LOG(ERR, "OOP and/or multi-segment"
						" buffers not supported for"
						" DOCSIS security.");
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				return -EINVAL;
			}
			cdev = rte_cryptodev_pmd_get_dev(ctx->dev_id);
			internals = cdev->data->dev_private;

			if (internals->qat_dev->qat_dev_gen != dev_gen) {
				op->status =
					RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
				return -EINVAL;
			}

			if (unlikely(ctx->build_request[proc_type] == NULL)) {
				int ret =
				qat_sym_gen_dev_ops[dev_gen].set_session(
					(void *)cdev, (void *)sess);
				if (ret < 0) {
					op->status =
						RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
					return -EINVAL;
				}
			}

			sess = (uintptr_t)op->sym->session;
			build_request = ctx->build_request[proc_type];
			opaque[0] = sess;
			opaque[1] = (uintptr_t)build_request;
		}
	} else { /* RTE_CRYPTO_OP_SESSIONLESS */
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		QAT_LOG(DEBUG, "QAT does not support sessionless operation");
		return -1;
	}

	return build_request(op, (void *)ctx, out_msg, op_cookie);
}

uint16_t
qat_sym_enqueue_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, qat_sym_build_request,
			(void **)ops, nb_ops);
}

uint16_t
qat_sym_dequeue_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops,
				qat_sym_process_response, nb_ops);
}

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param)
{
	int i = 0, ret = 0;
	uint16_t slice_map = 0;
	struct qat_device_info *qat_dev_instance =
			&qat_pci_devs[qat_pci_dev->qat_dev_id];
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_cryptodev_private)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_cryptodev_private *internals;
	const struct qat_crypto_gen_dev_ops *gen_dev_ops =
		&qat_sym_gen_dev_ops[qat_pci_dev->qat_dev_gen];

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "sym");
	QAT_LOG(DEBUG, "Creating QAT SYM device %s", name);

	if (gen_dev_ops->cryptodev_ops == NULL) {
		QAT_LOG(ERR, "Device %s does not support symmetric crypto",
				name);
		return -(EFAULT);
	}

	/*
	 * All processes must use same driver id so they can share sessions.
	 * Store driver_id so we can validate that all processes have the same
	 * value, typically they have, but could differ if binaries built
	 * separately.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		qat_pci_dev->qat_sym_driver_id =
				qat_sym_driver_id;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (qat_pci_dev->qat_sym_driver_id !=
				qat_sym_driver_id) {
			QAT_LOG(ERR,
				"Device %s have different driver id than corresponding device in primary process",
				name);
			return -(EFAULT);
		}
	}

	/* Populate subset device to use in cryptodev device creation */
	qat_dev_instance->sym_rte_dev.driver = &cryptodev_qat_sym_driver;
	qat_dev_instance->sym_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->sym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_dev_instance->sym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_dev_instance->sym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = qat_sym_driver_id;
	cryptodev->dev_ops = gen_dev_ops->cryptodev_ops;

	cryptodev->enqueue_burst = qat_sym_enqueue_burst;
	cryptodev->dequeue_burst = qat_sym_dequeue_burst;

	cryptodev->feature_flags = gen_dev_ops->get_feature_flags(qat_pci_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (gen_dev_ops->create_security_ctx) {
		cryptodev->security_ctx =
			gen_dev_ops->create_security_ctx((void *)cryptodev);
		if (cryptodev->security_ctx == NULL) {
			QAT_LOG(ERR, "rte_security_ctx memory alloc failed");
			ret = -ENOMEM;
			goto error;
		}

		cryptodev->feature_flags |= RTE_CRYPTODEV_FF_SECURITY;
		QAT_LOG(INFO, "Device %s rte_security support ensabled", name);
	} else {
		QAT_LOG(INFO, "Device %s rte_security support disabled", name);
	}
	snprintf(capa_memz_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"QAT_SYM_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;

	internals->dev_id = cryptodev->data->dev_id;

	while (qat_dev_cmd_param[i].name != NULL) {
		if (!strcmp(qat_dev_cmd_param[i].name, SYM_ENQ_THRESHOLD_NAME))
			internals->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		if (!strcmp(qat_dev_cmd_param[i].name,
				SYM_CIPHER_CRC_ENABLE_NAME))
			internals->cipher_crc_offload_enable =
					qat_dev_cmd_param[i].val;
		if (!strcmp(qat_dev_cmd_param[i].name, QAT_LEGACY_CAPA))
			qat_legacy_capa = qat_dev_cmd_param[i].val;
		if (!strcmp(qat_dev_cmd_param[i].name, QAT_CMD_SLICE_MAP))
			slice_map = qat_dev_cmd_param[i].val;
		i++;
	}

	if (gen_dev_ops->get_capabilities(internals,
			capa_memz_name, slice_map) < 0) {
		QAT_LOG(ERR,
			"Device cannot obtain capabilities, destroying PMD for %s",
			name);
		ret = -1;
		goto error;
	}
	internals->service_type = QAT_SERVICE_SYMMETRIC;
	qat_pci_dev->sym_dev = internals;
	QAT_LOG(DEBUG, "Created QAT SYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);

	return 0;

error:
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
	rte_cryptodev_pmd_destroy(cryptodev);
	memset(&qat_dev_instance->sym_rte_dev, 0,
		sizeof(qat_dev_instance->sym_rte_dev));

	return ret;
}

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->sym_dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->sym_dev->capa_mz);

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(qat_pci_dev->sym_dev->dev_id);
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].sym_rte_dev.name = NULL;
	qat_pci_dev->sym_dev = NULL;

	return 0;
}

int
qat_sym_configure_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	struct qat_cryptodev_private *internals = dev->data->dev_private;
	enum qat_device_gen qat_dev_gen = internals->qat_dev->qat_dev_gen;
	struct qat_crypto_gen_dev_ops *gen_dev_ops =
			&qat_sym_gen_dev_ops[qat_dev_gen];
	struct qat_qp *qp;
	struct qat_sym_session *ctx;
	struct qat_sym_dp_ctx *dp_ctx;

	if (!gen_dev_ops->set_raw_dp_ctx) {
		QAT_LOG(ERR, "Device GEN %u does not support raw data path",
				qat_dev_gen);
		return -ENOTSUP;
	}

	qp = dev->data->queue_pairs[qp_id];
	dp_ctx = (struct qat_sym_dp_ctx *)raw_dp_ctx->drv_ctx_data;

	if (!is_update) {
		memset(raw_dp_ctx, 0, sizeof(*raw_dp_ctx) +
				sizeof(struct qat_sym_dp_ctx));
		raw_dp_ctx->qp_data = dev->data->queue_pairs[qp_id];
		dp_ctx->tail = qp->tx_q.tail;
		dp_ctx->head = qp->rx_q.head;
		dp_ctx->cached_enqueue = dp_ctx->cached_dequeue = 0;
	}

	if (sess_type != RTE_CRYPTO_OP_WITH_SESSION)
		return -EINVAL;

	ctx = CRYPTODEV_GET_SYM_SESS_PRIV(session_ctx.crypto_sess);

	dp_ctx->session = ctx;

	return gen_dev_ops->set_raw_dp_ctx(raw_dp_ctx, ctx);
}

int
qat_sym_get_dp_ctx_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct qat_sym_dp_ctx);
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_sym_driver,
		qat_sym_driver_id);
