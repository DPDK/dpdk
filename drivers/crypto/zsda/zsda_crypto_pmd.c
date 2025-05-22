/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#include <rte_cryptodev.h>

#include "zsda_crypto_pmd.h"
#include "zsda_crypto_session.h"
#include "zsda_crypto.h"

uint8_t zsda_crypto_driver_id;

static int
zsda_dev_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return ZSDA_SUCCESS;
}

static int
zsda_dev_start(struct rte_cryptodev *dev)
{
	struct zsda_crypto_dev_private *crypto_dev = dev->data->dev_private;
	int ret;

	ret = zsda_queue_start(crypto_dev->zsda_pci_dev->pci_dev);

	return ret;
}

static void
zsda_dev_stop(struct rte_cryptodev *dev)
{
	struct zsda_crypto_dev_private *crypto_dev = dev->data->dev_private;

	zsda_queue_stop(crypto_dev->zsda_pci_dev->pci_dev);
}

static int
zsda_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	return zsda_queue_pair_release(
		(struct zsda_qp **)&(dev->data->queue_pairs[queue_pair_id]));
}

static int
zsda_dev_close(struct rte_cryptodev *dev)
{
	int ret = ZSDA_SUCCESS;
	uint16_t i;

	for (i = 0; i < dev->data->nb_queue_pairs; i++)
		ret |= zsda_qp_release(dev, i);
	return ret;
}

static uint16_t
zsda_crypto_max_nb_qps(void)
{
	uint16_t encrypt = zsda_nb_qps.encrypt;
	uint16_t decrypt = zsda_nb_qps.decrypt;
	uint16_t hash = zsda_nb_qps.hash;
	uint16_t min = 0;

	if ((encrypt == MAX_QPS_ON_FUNCTION) ||
	    (decrypt == MAX_QPS_ON_FUNCTION) ||
	    (hash == MAX_QPS_ON_FUNCTION))
		min = MAX_QPS_ON_FUNCTION;
	else {
		min = (encrypt < decrypt) ? encrypt : decrypt;
		min = (min < hash) ? min : hash;
	}

	if (min == 0)
		return MAX_QPS_ON_FUNCTION;
	return min;
}

static void
zsda_dev_info_get(struct rte_cryptodev *dev,
		  struct rte_cryptodev_info *info)
{
	struct zsda_crypto_dev_private *crypto_dev_priv = dev->data->dev_private;

	if (info != NULL) {
		info->max_nb_queue_pairs = zsda_crypto_max_nb_qps();
		info->feature_flags = dev->feature_flags;
		info->capabilities = crypto_dev_priv->zsda_crypto_capabilities;
		info->driver_id = zsda_crypto_driver_id;
		info->sym.max_nb_sessions = 0;
	}
}

static void
zsda_crypto_stats_get(struct rte_cryptodev *dev, struct rte_cryptodev_stats *stats)
{
	struct zsda_qp_stat comm = {0};

	zsda_stats_get(dev->data->queue_pairs, dev->data->nb_queue_pairs,
		       &comm);
	stats->enqueued_count = comm.enqueued_count;
	stats->dequeued_count = comm.dequeued_count;
	stats->enqueue_err_count = comm.enqueue_err_count;
	stats->dequeue_err_count = comm.dequeue_err_count;
}

static void
zsda_crypto_stats_reset(struct rte_cryptodev *dev)
{
	zsda_stats_reset(dev->data->queue_pairs, dev->data->nb_queue_pairs);
}


static int
zsda_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		  const struct rte_cryptodev_qp_conf *qp_conf,
		  int socket_id)
{
	int ret = ZSDA_SUCCESS;
	struct zsda_qp *qp_new;
	struct zsda_qp **qp_addr =
		(struct zsda_qp **)&(dev->data->queue_pairs[qp_id]);
	struct zsda_crypto_dev_private *crypto_dev_priv = dev->data->dev_private;
	struct zsda_pci_device *zsda_pci_dev = crypto_dev_priv->zsda_pci_dev;
	uint32_t nb_des = qp_conf->nb_descriptors;
	struct task_queue_info task_q_info;

	nb_des = (nb_des == NB_DES) ? nb_des : NB_DES;

	if (*qp_addr != NULL) {
		ret = zsda_qp_release(dev, qp_id);
		if (ret)
			return ret;
	}

	qp_new = rte_zmalloc_socket("zsda PMD qp metadata", sizeof(*qp_new),
				    RTE_CACHE_LINE_SIZE, socket_id);
	if (qp_new == NULL) {
		ZSDA_LOG(ERR, "Failed to alloc mem for qp struct");
		return -ENOMEM;
	}

	task_q_info.nb_des = nb_des;
	task_q_info.socket_id = socket_id;
	task_q_info.qp_id = qp_id;
	task_q_info.rx_cb = NULL;

	task_q_info.type = ZSDA_SERVICE_CRYPTO_ENCRY;
	task_q_info.service_str = "encry";
	task_q_info.tx_cb = zsda_crypto_wqe_build;
	task_q_info.match = zsda_encry_match;
	ret = zsda_task_queue_setup(zsda_pci_dev, qp_new, &task_q_info);

	task_q_info.type = ZSDA_SERVICE_CRYPTO_DECRY;
	task_q_info.service_str = "decry";
	task_q_info.tx_cb = zsda_crypto_wqe_build;
	task_q_info.match = zsda_decry_match;
	ret |= zsda_task_queue_setup(zsda_pci_dev, qp_new, &task_q_info);

	task_q_info.type = ZSDA_SERVICE_HASH_ENCODE;
	task_q_info.service_str = "hash";
	task_q_info.tx_cb = zsda_hash_wqe_build;
	task_q_info.match = zsda_hash_match;
	ret |= zsda_task_queue_setup(zsda_pci_dev, qp_new, &task_q_info);

	if (ret) {
		ZSDA_LOG(ERR, "zsda_task_queue_setup crypto is failed!");
		rte_free(qp_new);
		return ret;
	}

	*qp_addr = qp_new;

	return ret;
}

static unsigned int
zsda_sym_session_private_size_get(struct rte_cryptodev *dev __rte_unused)
{
	return RTE_ALIGN_CEIL(sizeof(struct zsda_sym_session), 8);
}

static int
zsda_sym_session_configure(struct rte_cryptodev *dev __rte_unused,
			   struct rte_crypto_sym_xform *xform,
			   struct rte_cryptodev_sym_session *sess)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		ZSDA_LOG(ERR, "Invalid session struct");
		return -EINVAL;
	}

	sess_private_data = CRYPTODEV_GET_SYM_SESS_PRIV(sess);

	ret = zsda_crypto_session_parameters_set(
			sess_private_data, xform);

	if (ret != ZSDA_SUCCESS)
		ZSDA_LOG(ERR, "Failed configure session parameters");

	return ret;
}

static void
zsda_sym_session_clear(struct rte_cryptodev *dev __rte_unused,
			struct rte_cryptodev_sym_session  *sess)
{
	struct zsda_sym_session *sess_priv = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	memset(sess_priv, 0, sizeof(struct zsda_sym_session));
}

static struct rte_cryptodev_ops crypto_zsda_ops = {
	.dev_configure = zsda_dev_config,
	.dev_start = zsda_dev_start,
	.dev_stop = zsda_dev_stop,
	.dev_close = zsda_dev_close,
	.dev_infos_get = zsda_dev_info_get,

	.stats_get = zsda_crypto_stats_get,
	.stats_reset = zsda_crypto_stats_reset,
	.queue_pair_setup = zsda_qp_setup,
	.queue_pair_release = zsda_qp_release,

	.sym_session_get_size = zsda_sym_session_private_size_get,
	.sym_session_configure = zsda_sym_session_configure,
	.sym_session_clear = zsda_sym_session_clear,
};

static const char zsda_crypto_drv_name[] = RTE_STR(CRYPTODEV_NAME_ZSDA_PMD);
static const struct rte_driver cryptodev_zsda_crypto_driver = {
	.name = zsda_crypto_drv_name,
	.alias = zsda_crypto_drv_name
};

static uint16_t
zsda_crypto_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
			      uint16_t nb_ops)
{
	return zsda_enqueue_burst((struct zsda_qp *)qp, (void **)ops,
				     nb_ops);
}

int
zsda_crypto_dev_create(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_device_info *dev_info =
		&zsda_devs[zsda_pci_dev->zsda_dev_id];

	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = (int)rte_socket_id(),
		.private_data_size = sizeof(struct zsda_crypto_dev_private)
	};

	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct zsda_crypto_dev_private *crypto_dev_priv;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return ZSDA_SUCCESS;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s", zsda_pci_dev->name,
		 "crypto");
	ZSDA_LOG(DEBUG, "Creating ZSDA crypto device %s", name);

	dev_info->crypto_rte_dev.driver = &cryptodev_zsda_crypto_driver;
	dev_info->crypto_rte_dev.numa_node = dev_info->pci_dev->device.numa_node;

	cryptodev = rte_cryptodev_pmd_create(name, &(dev_info->crypto_rte_dev),
					     &init_params);

	if (cryptodev == NULL) {
		ZSDA_LOG(ERR, "Failed! rte_cryptodev_pmd_create");
		goto error;
	}

	dev_info->crypto_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = zsda_crypto_driver_id;

	cryptodev->dev_ops = &crypto_zsda_ops;

	cryptodev->enqueue_burst = zsda_crypto_enqueue_op_burst;
	cryptodev->dequeue_burst = NULL;
	cryptodev->feature_flags = 0;

	crypto_dev_priv = cryptodev->data->dev_private;
	crypto_dev_priv->zsda_pci_dev = zsda_pci_dev;
	crypto_dev_priv->cryptodev = cryptodev;

	zsda_pci_dev->crypto_dev_priv = crypto_dev_priv;

	return ZSDA_SUCCESS;

error:

	rte_cryptodev_pmd_destroy(cryptodev);
	memset(&dev_info->crypto_rte_dev, 0, sizeof(dev_info->crypto_rte_dev));

	return -EFAULT;
}

void
zsda_crypto_dev_destroy(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_crypto_dev_private *crypto_dev_priv;

	if (zsda_pci_dev == NULL)
		return;

	crypto_dev_priv = zsda_pci_dev->crypto_dev_priv;
	if (crypto_dev_priv == NULL)
		return;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(crypto_dev_priv->capa_mz);

	zsda_dev_close(crypto_dev_priv->cryptodev);

	rte_cryptodev_pmd_destroy(crypto_dev_priv->cryptodev);
	zsda_devs[zsda_pci_dev->zsda_dev_id].crypto_rte_dev.name = NULL;
	zsda_pci_dev->crypto_dev_priv = NULL;
}

static struct cryptodev_driver zsda_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(zsda_crypto_drv, cryptodev_zsda_crypto_driver,
			       zsda_crypto_driver_id);
