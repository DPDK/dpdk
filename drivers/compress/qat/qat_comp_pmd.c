/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Intel Corporation
 */

#include <rte_malloc.h>

#include "qat_comp.h"
#include "qat_comp_pmd.h"

#define QAT_PMD_COMP_SGL_DEF_SEGMENTS 16

struct qat_comp_gen_dev_ops qat_comp_gen_dev_ops[QAT_N_GENS];

struct stream_create_info {
	struct qat_comp_dev_private *comp_dev;
	int socket_id;
	int error;
};

static struct
qat_comp_capabilities_info qat_comp_get_capa_info(
		enum qat_device_gen qat_dev_gen, struct qat_pci_device *qat_dev)
{
	struct qat_comp_capabilities_info ret = { .data = NULL, .size = 0 };

	if (qat_dev_gen >= QAT_N_GENS)
		return ret;
	RTE_FUNC_PTR_OR_ERR_RET(qat_comp_gen_dev_ops[qat_dev_gen]
			.qat_comp_get_capabilities, ret);
	return qat_comp_gen_dev_ops[qat_dev_gen]
			.qat_comp_get_capabilities(qat_dev);
}

void
qat_comp_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	struct qat_common_stats qat_stats = {0};
	struct qat_comp_dev_private *qat_priv;

	if (stats == NULL || dev == NULL) {
		QAT_LOG(ERR, "invalid ptr: stats %p, dev %p", stats, dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_get(qat_priv->qat_dev, &qat_stats, QAT_SERVICE_COMPRESSION);
	stats->enqueued_count = qat_stats.enqueued_count;
	stats->dequeued_count = qat_stats.dequeued_count;
	stats->enqueue_err_count = qat_stats.enqueue_err_count;
	stats->dequeue_err_count = qat_stats.dequeue_err_count;
}

void
qat_comp_stats_reset(struct rte_compressdev *dev)
{
	struct qat_comp_dev_private *qat_priv;

	if (dev == NULL) {
		QAT_LOG(ERR, "invalid compressdev ptr %p", dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_reset(qat_priv->qat_dev, QAT_SERVICE_COMPRESSION);

}

int
qat_comp_qp_release(struct rte_compressdev *dev, uint16_t queue_pair_id)
{
	struct qat_comp_dev_private *qat_private = dev->data->dev_private;
	struct qat_qp **qp_addr =
		(struct qat_qp **)&(dev->data->queue_pairs[queue_pair_id]);
	struct qat_qp *qp = (struct qat_qp *)*qp_addr;
	enum qat_device_gen qat_dev_gen = qat_private->qat_dev->qat_dev_gen;
	uint32_t i;

	QAT_LOG(DEBUG, "Release comp qp %u on device %d",
				queue_pair_id, dev->data->dev_id);

	qat_private->qat_dev->qps_in_use[QAT_SERVICE_COMPRESSION][queue_pair_id]
						= NULL;

	if (qp != NULL)
		for (i = 0; i < qp->nb_descriptors; i++) {
			struct qat_comp_op_cookie *cookie = qp->op_cookies[i];

			rte_free(cookie->qat_sgl_src_d);
			rte_free(cookie->qat_sgl_dst_d);
		}

	return qat_qp_release(qat_dev_gen, (struct qat_qp **)
			&(dev->data->queue_pairs[queue_pair_id]));
}

int
qat_comp_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct qat_qp_config qat_qp_conf = {0};
	struct qat_qp **qp_addr =
			(struct qat_qp **)&(dev->data->queue_pairs[qp_id]);
	struct qat_comp_dev_private *qat_private = dev->data->dev_private;
	struct qat_pci_device *qat_dev = qat_private->qat_dev;
	struct qat_qp *qp;
	uint32_t i;
	int ret;

	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = qat_comp_qp_release(dev, qp_id);
		if (ret < 0)
			return ret;
	}
	if (qp_id >= qat_qps_per_service(qat_dev,
					 QAT_SERVICE_COMPRESSION)) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}


	qat_qp_conf.hw = qat_qp_get_hw_data(qat_dev, QAT_SERVICE_COMPRESSION,
			qp_id);
	if (qat_qp_conf.hw == NULL) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}
	qat_qp_conf.cookie_size = sizeof(struct qat_comp_op_cookie);
	qat_qp_conf.nb_descriptors = max_inflight_ops;
	qat_qp_conf.socket_id = socket_id;
	qat_qp_conf.service_str = "comp";

	ret = qat_qp_setup(qat_private->qat_dev, qp_addr, qp_id, &qat_qp_conf);
	if (ret != 0)
		return ret;
	/* store a link to the qp in the qat_pci_device */
	qat_private->qat_dev->qps_in_use[QAT_SERVICE_COMPRESSION][qp_id]
								= *qp_addr;

	qp = (struct qat_qp *)*qp_addr;
	qp->min_enq_burst_threshold = qat_private->min_enq_burst_threshold;

	for (i = 0; i < qp->nb_descriptors; i++) {

		struct qat_comp_op_cookie *cookie =
				qp->op_cookies[i];

		cookie->qp = qp;
		cookie->cookie_index = i;

		cookie->qat_sgl_src_d = rte_zmalloc_socket(NULL,
					sizeof(struct qat_sgl) +
					sizeof(struct qat_flat_buf) *
					QAT_PMD_COMP_SGL_DEF_SEGMENTS,
					64, dev->data->socket_id);

		cookie->qat_sgl_dst_d = rte_zmalloc_socket(NULL,
					sizeof(struct qat_sgl) +
					sizeof(struct qat_flat_buf) *
					QAT_PMD_COMP_SGL_DEF_SEGMENTS,
					64, dev->data->socket_id);

		if (cookie->qat_sgl_src_d == NULL ||
				cookie->qat_sgl_dst_d == NULL) {
			QAT_LOG(ERR, "Can't allocate SGL"
				     " for device %s",
				     qat_private->qat_dev->name);
			return -ENOMEM;
		}

		cookie->qat_sgl_src_phys_addr =
				rte_malloc_virt2iova(cookie->qat_sgl_src_d);

		cookie->qat_sgl_dst_phys_addr =
				rte_malloc_virt2iova(cookie->qat_sgl_dst_d);

		cookie->dst_nb_elems = cookie->src_nb_elems =
				QAT_PMD_COMP_SGL_DEF_SEGMENTS;

		cookie->socket_id = dev->data->socket_id;

		cookie->error = 0;
	}

	return ret;
}


#define QAT_IM_BUFFER_DEBUG 0
const struct rte_memzone *
qat_comp_setup_inter_buffers(struct qat_comp_dev_private *comp_dev,
			      uint32_t buff_size)
{
	char inter_buff_mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *memzone;
	uint8_t *mz_start = NULL;
	rte_iova_t mz_start_phys = 0;
	struct array_of_ptrs *array_of_pointers;
	int size_of_ptr_array;
	uint32_t full_size;
	uint32_t offset_of_flat_buffs;
	int i;
	int num_im_sgls = qat_comp_get_num_im_bufs_required(
			comp_dev->qat_dev->qat_dev_gen);

	QAT_LOG(DEBUG, "QAT COMP device %s needs %d sgls",
				comp_dev->qat_dev->name, num_im_sgls);
	snprintf(inter_buff_mz_name, RTE_MEMZONE_NAMESIZE,
				"%s_inter_buff", comp_dev->qat_dev->name);
	memzone = rte_memzone_lookup(inter_buff_mz_name);
	if (memzone != NULL) {
		QAT_LOG(DEBUG, "QAT COMP im buffer memzone created already");
		return memzone;
	}

	/* Create multiple memzones to hold intermediate buffers and associated
	 * meta-data needed by the firmware.
	 * The first memzone contains:
	 *  - a list of num_im_sgls physical pointers to sgls
	 * All other memzones contain:
	 *  - the sgl structure, pointing to QAT_NUM_BUFS_IN_IM_SGL flat buffers
	 *  - the flat buffers: QAT_NUM_BUFS_IN_IM_SGL buffers,
	 *    each of buff_size
	 * num_im_sgls depends on the hardware generation of the device
	 * buff_size comes from the user via the config file
	 */

	size_of_ptr_array = num_im_sgls * sizeof(phys_addr_t);
	offset_of_flat_buffs = sizeof(struct qat_inter_sgl);
	full_size = offset_of_flat_buffs +
			buff_size * QAT_NUM_BUFS_IN_IM_SGL;

	memzone = rte_memzone_reserve_aligned(inter_buff_mz_name,
			size_of_ptr_array,
			comp_dev->compressdev->data->socket_id,
			RTE_MEMZONE_IOVA_CONTIG, QAT_64_BYTE_ALIGN);
	if (memzone == NULL) {
		QAT_LOG(ERR,
				"Can't allocate intermediate buffers for device %s",
				comp_dev->qat_dev->name);
		return NULL;
	}

	mz_start = (uint8_t *)memzone->addr;
	mz_start_phys = memzone->iova;
	QAT_LOG(DEBUG, "Memzone %s: addr = %p, phys = 0x%"PRIx64
			", size required %d, size created %zu",
			inter_buff_mz_name, mz_start, mz_start_phys,
			size_of_ptr_array, memzone->len);

	array_of_pointers = (struct array_of_ptrs *)mz_start;
	for (i = 0; i < num_im_sgls; i++) {
		const struct rte_memzone *mz;
		struct qat_inter_sgl *sgl;
		int lb;

		snprintf(inter_buff_mz_name, RTE_MEMZONE_NAMESIZE,
				"%s_inter_buff_%d", comp_dev->qat_dev->name, i);
		mz = rte_memzone_lookup(inter_buff_mz_name);
		if (mz == NULL) {
			mz = rte_memzone_reserve_aligned(inter_buff_mz_name,
					full_size,
					comp_dev->compressdev->data->socket_id,
					RTE_MEMZONE_IOVA_CONTIG,
					QAT_64_BYTE_ALIGN);
			if (mz == NULL) {
				QAT_LOG(ERR,
						"Can't allocate intermediate buffers for device %s",
						comp_dev->qat_dev->name);
				while (--i >= 0) {
					snprintf(inter_buff_mz_name,
							RTE_MEMZONE_NAMESIZE,
							"%s_inter_buff_%d",
							comp_dev->qat_dev->name,
							i);
					rte_memzone_free(
							rte_memzone_lookup(
							inter_buff_mz_name));
				}
				rte_memzone_free(memzone);
				return NULL;
			}
		}

		QAT_LOG(DEBUG, "Memzone %s: addr = %p, phys = 0x%"PRIx64
				", size required %d, size created %zu",
				inter_buff_mz_name, mz->addr, mz->iova,
				full_size, mz->len);

		array_of_pointers->pointer[i] = mz->iova;

		sgl = (struct qat_inter_sgl *) mz->addr;
		sgl->num_bufs = QAT_NUM_BUFS_IN_IM_SGL;
		sgl->num_mapped_bufs = 0;
		sgl->resrvd = 0;

#if QAT_IM_BUFFER_DEBUG
		QAT_LOG(DEBUG, "  : phys addr of sgl[%i] in array_of_pointers"
			" = 0x%"PRIx64, i, array_of_pointers->pointer[i]);
		QAT_LOG(DEBUG, "  : virt address of sgl[%i] = %p", i, sgl);
#endif
		for (lb = 0; lb < QAT_NUM_BUFS_IN_IM_SGL; lb++) {
			sgl->buffers[lb].addr =
					mz->iova + offset_of_flat_buffs +
					lb * buff_size;
			sgl->buffers[lb].len = buff_size;
			sgl->buffers[lb].resrvd = 0;
#if QAT_IM_BUFFER_DEBUG
			QAT_LOG(DEBUG,
			  "  : sgl->buffers[%d].addr = 0x%"PRIx64", len=%d",
			  lb, sgl->buffers[lb].addr, sgl->buffers[lb].len);
#endif
		}
	}
#if QAT_IM_BUFFER_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG,  "IM buffer memzone start:",
			memzone->addr, size_of_ptr_array);
#endif
	return memzone;
}

static struct rte_mempool *
qat_comp_create_xform_pool(struct qat_comp_dev_private *comp_dev,
			   struct rte_compressdev_config *config,
			   uint32_t num_elements)
{
	char xform_pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(xform_pool_name, RTE_MEMPOOL_NAMESIZE,
			"%s_xforms", comp_dev->qat_dev->name);

	QAT_LOG(DEBUG, "xformpool: %s", xform_pool_name);
	mp = rte_mempool_lookup(xform_pool_name);

	if (mp != NULL) {
		QAT_LOG(DEBUG, "xformpool already created");
		if (mp->size != num_elements) {
			QAT_LOG(DEBUG, "xformpool wrong size - delete it");
			rte_mempool_free(mp);
			mp = NULL;
			comp_dev->xformpool = NULL;
		}
	}

	if (mp == NULL)
		mp = rte_mempool_create(xform_pool_name,
				num_elements,
				qat_comp_xform_size(), 0, 0,
				NULL, NULL, NULL, NULL, config->socket_id,
				0);
	if (mp == NULL) {
		QAT_LOG(ERR, "Err creating mempool %s w %d elements of size %d",
			xform_pool_name, num_elements, qat_comp_xform_size());
		return NULL;
	}

	return mp;
}

static void
qat_comp_stream_init(struct rte_mempool *mp __rte_unused, void *opaque,
		     void *obj, unsigned int obj_idx)
{
	struct stream_create_info *info = opaque;
	struct qat_comp_stream *stream = obj;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *memzone;
	struct qat_inter_sgl *ram_banks_desc;

	/* find a memzone for RAM banks */
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_%u_rambanks",
		 info->comp_dev->qat_dev->name, obj_idx);
	memzone = rte_memzone_lookup(mz_name);
	if (memzone == NULL) {
		/* allocate a memzone for compression state and RAM banks */
		memzone = rte_memzone_reserve_aligned(mz_name,
			QAT_STATE_REGISTERS_MAX_SIZE
				+ sizeof(struct qat_inter_sgl)
				+ QAT_INFLATE_CONTEXT_SIZE,
			info->socket_id,
			RTE_MEMZONE_IOVA_CONTIG, QAT_64_BYTE_ALIGN);
		if (memzone == NULL) {
			QAT_LOG(ERR,
			    "Can't allocate RAM banks for device %s, object %u",
				info->comp_dev->qat_dev->name, obj_idx);
			info->error = -ENOMEM;
			return;
		}
	}

	/* prepare the buffer list descriptor for RAM banks */
	ram_banks_desc = (struct qat_inter_sgl *)
		(((uint8_t *) memzone->addr) + QAT_STATE_REGISTERS_MAX_SIZE);
	ram_banks_desc->num_bufs = 1;
	ram_banks_desc->buffers[0].len = QAT_INFLATE_CONTEXT_SIZE;
	ram_banks_desc->buffers[0].addr = memzone->iova
			+ QAT_STATE_REGISTERS_MAX_SIZE
			+ sizeof(struct qat_inter_sgl);

	memset(stream, 0, qat_comp_stream_size());
	stream->memzone = memzone;
	stream->state_registers_decomp = memzone->addr;
	stream->state_registers_decomp_phys = memzone->iova;
	stream->inflate_context = ((uint8_t *) memzone->addr)
			+ QAT_STATE_REGISTERS_MAX_SIZE;
	stream->inflate_context_phys = memzone->iova
			+ QAT_STATE_REGISTERS_MAX_SIZE;
}

static void
qat_comp_stream_destroy(struct rte_mempool *mp __rte_unused,
			void *opaque __rte_unused, void *obj,
			unsigned obj_idx __rte_unused)
{
	struct qat_comp_stream *stream = obj;

	rte_memzone_free(stream->memzone);
}

static struct rte_mempool *
qat_comp_create_stream_pool(struct qat_comp_dev_private *comp_dev,
			    int socket_id,
			    uint32_t num_elements)
{
	char stream_pool_name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(stream_pool_name, RTE_MEMPOOL_NAMESIZE,
		 "%s_streams", comp_dev->qat_dev->name);

	QAT_LOG(DEBUG, "streampool: %s", stream_pool_name);
	mp = rte_mempool_lookup(stream_pool_name);

	if (mp != NULL) {
		QAT_LOG(DEBUG, "streampool already created");
		if (mp->size != num_elements) {
			QAT_LOG(DEBUG, "streampool wrong size - delete it");
			rte_mempool_obj_iter(mp, qat_comp_stream_destroy, NULL);
			rte_mempool_free(mp);
			mp = NULL;
			comp_dev->streampool = NULL;
		}
	}

	if (mp == NULL) {
		struct stream_create_info info = {
			.comp_dev = comp_dev,
			.socket_id = socket_id,
			.error = 0
		};
		mp = rte_mempool_create(stream_pool_name,
				num_elements,
				qat_comp_stream_size(), 0, 0,
				NULL, NULL, qat_comp_stream_init, &info,
				socket_id, 0);
		if (mp == NULL) {
			QAT_LOG(ERR,
			     "Err creating mempool %s w %d elements of size %d",
			     stream_pool_name, num_elements,
			     qat_comp_stream_size());
		} else if (info.error) {
			rte_mempool_obj_iter(mp, qat_comp_stream_destroy, NULL);
			QAT_LOG(ERR,
			     "Destroying mempool %s as at least one element failed initialisation",
			     stream_pool_name);
			rte_mempool_free(mp);
			mp = NULL;
		}
	}

	return mp;
}

static void
_qat_comp_dev_config_clear(struct qat_comp_dev_private *comp_dev)
{
	/* Free intermediate buffers */
	if (comp_dev->interm_buff_mz) {
		char mz_name[RTE_MEMZONE_NAMESIZE];
		int i = qat_comp_get_num_im_bufs_required(
				comp_dev->qat_dev->qat_dev_gen);

		while (--i >= 0) {
			snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
					"%s_inter_buff_%d",
					comp_dev->qat_dev->name, i);
			rte_memzone_free(rte_memzone_lookup(mz_name));
		}
		rte_memzone_free(comp_dev->interm_buff_mz);
		comp_dev->interm_buff_mz = NULL;
	}

	/* Free private_xform pool */
	if (comp_dev->xformpool) {
		/* Free internal mempool for private xforms */
		rte_mempool_free(comp_dev->xformpool);
		comp_dev->xformpool = NULL;
	}

	/* Free stream pool */
	if (comp_dev->streampool) {
		rte_mempool_obj_iter(comp_dev->streampool,
				     qat_comp_stream_destroy, NULL);
		rte_mempool_free(comp_dev->streampool);
		comp_dev->streampool = NULL;
	}
}

int
qat_comp_dev_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;
	int ret = 0;

	if (config->max_nb_priv_xforms) {
		comp_dev->xformpool = qat_comp_create_xform_pool(comp_dev,
					    config, config->max_nb_priv_xforms);
		if (comp_dev->xformpool == NULL) {
			ret = -ENOMEM;
			goto error_out;
		}
	} else
		comp_dev->xformpool = NULL;

	if (config->max_nb_streams) {
		comp_dev->streampool = qat_comp_create_stream_pool(comp_dev,
				     config->socket_id, config->max_nb_streams);
		if (comp_dev->streampool == NULL) {
			ret = -ENOMEM;
			goto error_out;
		}
	} else
		comp_dev->streampool = NULL;

	return 0;

error_out:
	_qat_comp_dev_config_clear(comp_dev);
	return ret;
}

int
qat_comp_dev_start(struct rte_compressdev *dev __rte_unused)
{
	return 0;
}

void
qat_comp_dev_stop(struct rte_compressdev *dev __rte_unused)
{

}

int
qat_comp_dev_close(struct rte_compressdev *dev)
{
	int i;
	int ret = 0;
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_comp_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	_qat_comp_dev_config_clear(comp_dev);

	return ret;
}

void
qat_comp_dev_info_get(struct rte_compressdev *dev,
			struct rte_compressdev_info *info)
{
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;
	struct qat_pci_device *qat_dev = comp_dev->qat_dev;

	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(qat_dev,
					    QAT_SERVICE_COMPRESSION);
		info->feature_flags = dev->feature_flags;
		info->capabilities = comp_dev->qat_dev_capabilities;
	}
}

static uint16_t
qat_comp_pmd_enq_deq_dummy_op_burst(void *qp __rte_unused,
				    struct rte_comp_op **ops __rte_unused,
				    uint16_t nb_ops __rte_unused)
{
	QAT_DP_LOG(ERR, "QAT PMD detected wrong FW version !");
	return 0;
}

static struct rte_compressdev_ops compress_qat_dummy_ops = {

	/* Device related operations */
	.dev_configure		= NULL,
	.dev_start		= NULL,
	.dev_stop		= qat_comp_dev_stop,
	.dev_close		= qat_comp_dev_close,
	.dev_infos_get		= NULL,

	.stats_get		= NULL,
	.stats_reset		= qat_comp_stats_reset,
	.queue_pair_setup	= NULL,
	.queue_pair_release	= qat_comp_qp_release,

	/* Compression related operations */
	.private_xform_create	= NULL,
	.private_xform_free	= qat_comp_private_xform_free
};

static uint16_t
qat_comp_pmd_dequeue_first_op_burst(void *qp, struct rte_comp_op **ops,
				   uint16_t nb_ops)
{
	uint16_t ret = qat_dequeue_op_burst(qp, (void **)ops, nb_ops);
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;

	if (ret) {
		if ((*ops)->debug_status ==
				(uint64_t)ERR_CODE_QAT_COMP_WRONG_FW) {
			tmp_qp->qat_dev->comp_dev->compressdev->enqueue_burst =
					qat_comp_pmd_enq_deq_dummy_op_burst;
			tmp_qp->qat_dev->comp_dev->compressdev->dequeue_burst =
					qat_comp_pmd_enq_deq_dummy_op_burst;

			tmp_qp->qat_dev->comp_dev->compressdev->dev_ops =
					&compress_qat_dummy_ops;
			QAT_LOG(ERR,
					"This QAT hardware doesn't support compression operation");

		} else {
			tmp_qp->qat_dev->comp_dev->compressdev->dequeue_burst =
					(compressdev_dequeue_pkt_burst_t)
					qat_dequeue_op_burst;
		}
	}
	return ret;
}

/* An rte_driver is needed in the registration of the device with compressdev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the compression part of the pci device.
 */
static const char qat_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_QAT_PMD);
static const struct rte_driver compdev_qat_driver = {
	.name = qat_comp_drv_name,
	.alias = qat_comp_drv_name
};

int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param)
{
	int i = 0;
	struct qat_device_info *qat_dev_instance =
			&qat_pci_devs[qat_pci_dev->qat_dev_id];
	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
	};
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct qat_comp_dev_private *comp_dev;
	struct qat_comp_capabilities_info capabilities_info;
	const struct rte_compressdev_capabilities *capabilities;
	const struct qat_comp_gen_dev_ops *qat_comp_gen_ops =
			&qat_comp_gen_dev_ops[qat_pci_dev->qat_dev_gen];
	uint64_t capa_size;

	snprintf(name, RTE_COMPRESSDEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "comp");
	QAT_LOG(DEBUG, "Creating QAT COMP device %s", name);

	if (qat_comp_gen_ops->compressdev_ops == NULL) {
		QAT_LOG(DEBUG, "Device %s does not support compression", name);
		return -ENOTSUP;
	}

	/* Populate subset device to use in compressdev device creation */
	qat_dev_instance->comp_rte_dev.driver = &compdev_qat_driver;
	qat_dev_instance->comp_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->comp_rte_dev.devargs = NULL;

	compressdev = rte_compressdev_pmd_create(name,
			&(qat_dev_instance->comp_rte_dev),
			sizeof(struct qat_comp_dev_private),
			&init_params);

	if (compressdev == NULL)
		return -ENODEV;

	compressdev->dev_ops = qat_comp_gen_ops->compressdev_ops;

	compressdev->enqueue_burst = (compressdev_enqueue_pkt_burst_t)
			qat_enqueue_comp_op_burst;
	compressdev->dequeue_burst = qat_comp_pmd_dequeue_first_op_burst;
	compressdev->feature_flags =
			qat_comp_gen_ops->qat_comp_get_feature_flags();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	snprintf(capa_memz_name, RTE_COMPRESSDEV_NAME_MAX_LEN,
			"QAT_COMP_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

	comp_dev = compressdev->data->dev_private;
	comp_dev->qat_dev = qat_pci_dev;
	comp_dev->compressdev = compressdev;

	capabilities_info = qat_comp_get_capa_info(qat_pci_dev->qat_dev_gen,
			qat_pci_dev);

	if (capabilities_info.data == NULL) {
		QAT_LOG(DEBUG,
			"QAT gen %d capabilities unknown, default to GEN1",
					qat_pci_dev->qat_dev_gen);
		capabilities_info = qat_comp_get_capa_info(QAT_GEN1,
				qat_pci_dev);
	}

	capabilities = capabilities_info.data;
	capa_size = capabilities_info.size;

	comp_dev->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (comp_dev->capa_mz == NULL) {
		comp_dev->capa_mz = rte_memzone_reserve(capa_memz_name,
			capa_size,
			rte_socket_id(), 0);
	}
	if (comp_dev->capa_mz == NULL) {
		QAT_LOG(DEBUG,
			"Error allocating memzone for capabilities, destroying PMD for %s",
			name);
		memset(&qat_dev_instance->comp_rte_dev, 0,
			sizeof(qat_dev_instance->comp_rte_dev));
		rte_compressdev_pmd_destroy(compressdev);
		return -EFAULT;
	}

	memcpy(comp_dev->capa_mz->addr, capabilities, capa_size);
	comp_dev->qat_dev_capabilities = comp_dev->capa_mz->addr;

	while (1) {
		if (qat_dev_cmd_param[i].name == NULL)
			break;
		if (!strcmp(qat_dev_cmd_param[i].name, COMP_ENQ_THRESHOLD_NAME))
			comp_dev->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		i++;
	}
	qat_pci_dev->comp_dev = comp_dev;

	QAT_LOG(DEBUG,
		    "Created QAT COMP device %s as compressdev instance %d",
			name, compressdev->data->dev_id);
	return 0;
}

int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct qat_comp_dev_private *comp_dev;

	if (qat_pci_dev == NULL)
		return -ENODEV;

	comp_dev = qat_pci_dev->comp_dev;
	if (comp_dev == NULL)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->comp_dev->capa_mz);

	/* clean up any resources used by the device */
	qat_comp_dev_close(comp_dev->compressdev);

	rte_compressdev_pmd_destroy(comp_dev->compressdev);
	qat_pci_dev->comp_dev = NULL;

	return 0;
}
