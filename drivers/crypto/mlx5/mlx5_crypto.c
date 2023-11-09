/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <bus_pci_driver.h>
#include <rte_memory.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>

#include "mlx5_crypto_utils.h"
#include "mlx5_crypto.h"

#define MLX5_CRYPTO_DRIVER_NAME crypto_mlx5
#define MLX5_CRYPTO_LOG_NAME pmd.crypto.mlx5
#define MLX5_CRYPTO_MAX_QPS 128
#define MLX5_CRYPTO_MAX_SEGS 56

#define MLX5_CRYPTO_FEATURE_FLAGS(wrapped_mode) \
	(RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO | RTE_CRYPTODEV_FF_HW_ACCELERATED | \
	 RTE_CRYPTODEV_FF_IN_PLACE_SGL | RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT | \
	 RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT | \
	 RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT | \
	 RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT | \
	 (wrapped_mode ? RTE_CRYPTODEV_FF_CIPHER_WRAPPED_KEY : 0) | \
	 RTE_CRYPTODEV_FF_CIPHER_MULTIPLE_DATA_UNITS)

TAILQ_HEAD(mlx5_crypto_privs, mlx5_crypto_priv) mlx5_crypto_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_crypto_priv_list);
static pthread_mutex_t priv_list_lock;

int mlx5_crypto_logtype;

uint8_t mlx5_crypto_driver_id;

static const char mlx5_crypto_drv_name[] = RTE_STR(MLX5_CRYPTO_DRIVER_NAME);

static const struct rte_driver mlx5_drv = {
	.name = mlx5_crypto_drv_name,
	.alias = mlx5_crypto_drv_name
};

static struct cryptodev_driver mlx5_cryptodev_driver;

static void
mlx5_crypto_dev_infos_get(struct rte_cryptodev *dev,
			  struct rte_cryptodev_info *dev_info)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	RTE_SET_USED(dev);
	if (dev_info != NULL) {
		dev_info->driver_id = mlx5_crypto_driver_id;
		dev_info->feature_flags =
			MLX5_CRYPTO_FEATURE_FLAGS(priv->is_wrapped_mode);
		dev_info->capabilities = priv->caps;
		dev_info->max_nb_queue_pairs = MLX5_CRYPTO_MAX_QPS;
		if (priv->caps->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			dev_info->min_mbuf_headroom_req = MLX5_CRYPTO_GCM_MAX_AAD;
			dev_info->min_mbuf_tailroom_req = MLX5_CRYPTO_GCM_MAX_DIGEST;
		} else {
			dev_info->min_mbuf_headroom_req = 0;
			dev_info->min_mbuf_tailroom_req = 0;
		}
		dev_info->sym.max_nb_sessions = 0;
		/*
		 * If 0, the device does not have any limitation in number of
		 * sessions that can be used.
		 */
	}
}

void
mlx5_crypto_indirect_mkeys_release(struct mlx5_crypto_qp *qp,
				   uint16_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++)
		if (qp->mkey[i])
			claim_zero(mlx5_devx_cmd_destroy(qp->mkey[i]));
}

int
mlx5_crypto_indirect_mkeys_prepare(struct mlx5_crypto_priv *priv,
				   struct mlx5_crypto_qp *qp,
				   struct mlx5_devx_mkey_attr *attr,
				   mlx5_crypto_mkey_update_t update_cb)
{
	uint32_t i;

	for (i = 0; i < qp->entries_n; i++) {
		attr->klm_array = update_cb(priv, qp, i);
		qp->mkey[i] = mlx5_devx_cmd_mkey_create(priv->cdev->ctx, attr);
		if (!qp->mkey[i])
			goto error;
	}
	return 0;
error:
	DRV_LOG(ERR, "Failed to allocate indirect mkey.");
	mlx5_crypto_indirect_mkeys_release(qp, i);
	return -1;
}

static int
mlx5_crypto_dev_configure(struct rte_cryptodev *dev,
			  struct rte_cryptodev_config *config)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	if (config == NULL) {
		DRV_LOG(ERR, "Invalid crypto dev configure parameters.");
		return -EINVAL;
	}
	if ((config->ff_disable & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) != 0) {
		DRV_LOG(ERR,
			"Disabled symmetric crypto feature is not supported.");
		return -ENOTSUP;
	}
	if (mlx5_crypto_dek_setup(priv) != 0) {
		DRV_LOG(ERR, "Dek hash list creation has failed.");
		return -ENOMEM;
	}
	priv->dev_config = *config;
	DRV_LOG(DEBUG, "Device %u was configured.", dev->driver_id);
	return 0;
}

static void
mlx5_crypto_dev_stop(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);
}

static int
mlx5_crypto_dev_start(struct rte_cryptodev *dev)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	return mlx5_dev_mempool_subscribe(priv->cdev);
}

static int
mlx5_crypto_dev_close(struct rte_cryptodev *dev)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	mlx5_crypto_dek_unset(priv);
	DRV_LOG(DEBUG, "Device %u was closed.", dev->driver_id);
	return 0;
}

static unsigned int
mlx5_crypto_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct mlx5_crypto_session);
}

static void
mlx5_crypto_sym_session_clear(struct rte_cryptodev *dev,
			      struct rte_cryptodev_sym_session *sess)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_session *spriv = CRYPTODEV_GET_SYM_SESS_PRIV(sess);

	if (unlikely(spriv == NULL)) {
		DRV_LOG(ERR, "Failed to get session %p private data.", spriv);
		return;
	}
	mlx5_crypto_dek_destroy(priv, spriv->dek);
	DRV_LOG(DEBUG, "Session %p was cleared.", spriv);
}

static void
mlx5_crypto_stats_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void
mlx5_crypto_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static struct rte_cryptodev_ops mlx5_crypto_ops = {
	.dev_configure			= mlx5_crypto_dev_configure,
	.dev_start			= mlx5_crypto_dev_start,
	.dev_stop			= mlx5_crypto_dev_stop,
	.dev_close			= mlx5_crypto_dev_close,
	.dev_infos_get			= mlx5_crypto_dev_infos_get,
	.stats_get			= mlx5_crypto_stats_get,
	.stats_reset			= mlx5_crypto_stats_reset,
	.sym_session_get_size		= mlx5_crypto_sym_session_get_size,
	.sym_session_clear		= mlx5_crypto_sym_session_clear,
	.sym_get_raw_dp_ctx_size	= NULL,
	.sym_configure_raw_dp_ctx	= NULL,
};

static int
mlx5_crypto_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_crypto_devarg_params *devarg_prms = opaque;
	struct mlx5_devx_crypto_login_attr *attr = &devarg_prms->login_attr;
	unsigned long tmp;
	FILE *file;
	int ret;
	int i;

	if (strcmp(key, "wcs_file") == 0) {
		file = fopen(val, "rb");
		if (file == NULL) {
			rte_errno = ENOTSUP;
			return -rte_errno;
		}
		for (i = 0 ; i < MLX5_CRYPTO_CREDENTIAL_SIZE ; i++) {
			ret = fscanf(file, "%02hhX", &attr->credential[i]);
			if (ret <= 0) {
				fclose(file);
				DRV_LOG(ERR,
					"Failed to read credential from file.");
				rte_errno = EINVAL;
				return -rte_errno;
			}
		}
		fclose(file);
		devarg_prms->login_devarg = true;
		return 0;
	}
	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.", key, val);
		return -errno;
	}
	if (strcmp(key, "max_segs_num") == 0) {
		if (!tmp) {
			DRV_LOG(ERR, "max_segs_num must be greater than 0.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		devarg_prms->max_segs_num = (uint32_t)tmp;
	} else if (strcmp(key, "import_kek_id") == 0) {
		attr->session_import_kek_ptr = (uint32_t)tmp;
	} else if (strcmp(key, "credential_id") == 0) {
		attr->credential_pointer = (uint32_t)tmp;
	} else if (strcmp(key, "keytag") == 0) {
		devarg_prms->keytag = tmp;
	} else if (strcmp(key, "algo") == 0) {
		if (tmp == 1) {
			devarg_prms->is_aes_gcm = 1;
		} else if (tmp > 1) {
			DRV_LOG(ERR, "Invalid algo.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
	}
	return 0;
}

static int
mlx5_crypto_parse_devargs(struct mlx5_kvargs_ctrl *mkvlist,
			  struct mlx5_crypto_devarg_params *devarg_prms,
			  bool wrapped_mode)
{
	struct mlx5_devx_crypto_login_attr *attr = &devarg_prms->login_attr;
	const char **params = (const char *[]){
		"credential_id",
		"import_kek_id",
		"keytag",
		"max_segs_num",
		"wcs_file",
		"algo",
		NULL,
	};

	/* Default values. */
	attr->credential_pointer = 0;
	attr->session_import_kek_ptr = 0;
	devarg_prms->keytag = 0;
	devarg_prms->max_segs_num = 8;
	if (mkvlist == NULL) {
		if (!wrapped_mode)
			return 0;
		DRV_LOG(ERR,
			"No login devargs in order to enable crypto operations in the device.");
		rte_errno = EINVAL;
		return -1;
	}
	if (mlx5_kvargs_process(mkvlist, params, mlx5_crypto_args_check_handler,
				devarg_prms) != 0) {
		DRV_LOG(ERR, "Devargs handler function Failed.");
		rte_errno = EINVAL;
		return -1;
	}
	if (devarg_prms->login_devarg == false && wrapped_mode) {
		DRV_LOG(ERR,
			"No login credential devarg in order to enable crypto operations in the device while in wrapped import method.");
		rte_errno = EINVAL;
		return -1;
	}
	return 0;
}

static int
mlx5_crypto_dev_probe(struct mlx5_common_device *cdev,
		      struct mlx5_kvargs_ctrl *mkvlist)
{
	struct rte_cryptodev *crypto_dev;
	struct mlx5_devx_obj *login;
	struct mlx5_crypto_priv *priv;
	struct mlx5_crypto_devarg_params devarg_prms = { 0 };
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.private_data_size = sizeof(struct mlx5_crypto_priv),
		.socket_id = cdev->dev->numa_node,
		.max_nb_queue_pairs =
				RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS,
	};
	const char *ibdev_name = mlx5_os_get_ctx_device_name(cdev->ctx);
	int ret;
	bool wrapped_mode;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(ERR, "Non-primary process type is not supported.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (!cdev->config.hca_attr.crypto ||
	   (!cdev->config.hca_attr.aes_xts &&
	    !cdev->config.hca_attr.crypto_mmo.crypto_mmo_qp)) {
		DRV_LOG(ERR, "Not enough capabilities to support crypto "
			"operations, maybe old FW/OFED version?");
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	wrapped_mode = !!cdev->config.hca_attr.crypto_wrapped_import_method;
	ret = mlx5_crypto_parse_devargs(mkvlist, &devarg_prms, wrapped_mode);
	if (ret) {
		DRV_LOG(ERR, "Failed to parse devargs.");
		return -rte_errno;
	}
	crypto_dev = rte_cryptodev_pmd_create(ibdev_name, cdev->dev,
					      &init_params);
	if (crypto_dev == NULL) {
		DRV_LOG(ERR, "Failed to create device \"%s\".", ibdev_name);
		return -ENODEV;
	}
	DRV_LOG(INFO,
		"Crypto device %s was created successfully.", ibdev_name);
	crypto_dev->dev_ops = &mlx5_crypto_ops;
	crypto_dev->feature_flags = MLX5_CRYPTO_FEATURE_FLAGS(wrapped_mode);
	crypto_dev->driver_id = mlx5_crypto_driver_id;
	priv = crypto_dev->data->dev_private;
	priv->cdev = cdev;
	priv->crypto_dev = crypto_dev;
	priv->is_wrapped_mode = wrapped_mode;
	priv->max_segs_num = devarg_prms.max_segs_num;
	/* Init and override AES-GCM configuration. */
	if (devarg_prms.is_aes_gcm) {
		ret = mlx5_crypto_gcm_init(priv);
		if (ret) {
			rte_cryptodev_pmd_destroy(priv->crypto_dev);
			DRV_LOG(ERR, "Failed to init AES-GCM crypto.");
			return -ENOTSUP;
		}
	} else {
		ret = mlx5_crypto_xts_init(priv);
		if (ret) {
			rte_cryptodev_pmd_destroy(priv->crypto_dev);
			DRV_LOG(ERR, "Failed to init AES-XTS crypto.");
			return -ENOTSUP;
		}
	}
	if (mlx5_devx_uar_prepare(cdev, &priv->uar) != 0) {
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
		return -1;
	}
	if (wrapped_mode) {
		login = mlx5_devx_cmd_create_crypto_login_obj(cdev->ctx,
						      &devarg_prms.login_attr);
		if (login == NULL) {
			DRV_LOG(ERR, "Failed to configure login.");
			mlx5_devx_uar_release(&priv->uar);
			rte_cryptodev_pmd_destroy(priv->crypto_dev);
			return -rte_errno;
		}
		priv->login_obj = login;
	}
	priv->keytag = rte_cpu_to_be_64(devarg_prms.keytag);
	DRV_LOG(INFO, "Max number of segments: %u.",
		(unsigned int)RTE_MIN(
			MLX5_CRYPTO_KLM_SEGS_NUM(priv->umr_wqe_size),
			(uint16_t)(priv->max_rdmar_ds - 2)));
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_crypto_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);

	rte_cryptodev_pmd_probing_finish(crypto_dev);

	return 0;
}

static int
mlx5_crypto_dev_remove(struct mlx5_common_device *cdev)
{
	struct mlx5_crypto_priv *priv = NULL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &mlx5_crypto_priv_list, next)
		if (priv->crypto_dev->device == cdev->dev)
			break;
	if (priv)
		TAILQ_REMOVE(&mlx5_crypto_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (priv) {
		claim_zero(mlx5_devx_cmd_destroy(priv->login_obj));
		mlx5_devx_uar_release(&priv->uar);
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
	}
	return 0;
}

static const struct rte_pci_id mlx5_crypto_pci_id_map[] = {
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX6)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX6DX)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_BLUEFIELD2)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX7)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_BLUEFIELD3)
		},
		{
			.vendor_id = 0
		}
};

static struct mlx5_class_driver mlx5_crypto_driver = {
	.drv_class = MLX5_CLASS_CRYPTO,
	.name = RTE_STR(MLX5_CRYPTO_DRIVER_NAME),
	.id_table = mlx5_crypto_pci_id_map,
	.probe = mlx5_crypto_dev_probe,
	.remove = mlx5_crypto_dev_remove,
};

RTE_INIT(rte_mlx5_crypto_init)
{
	pthread_mutex_init(&priv_list_lock, NULL);
	mlx5_common_init();
	if (mlx5_glue != NULL)
		mlx5_class_driver_register(&mlx5_crypto_driver);
}

RTE_PMD_REGISTER_CRYPTO_DRIVER(mlx5_cryptodev_driver, mlx5_drv,
			       mlx5_crypto_driver_id);

RTE_LOG_REGISTER_DEFAULT(mlx5_crypto_logtype, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_CRYPTO_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_CRYPTO_DRIVER_NAME, mlx5_crypto_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_CRYPTO_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
