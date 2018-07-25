/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <string.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cpuflags.h>
#include <rte_malloc.h>

#include "otx_zip.h"

static const struct rte_compressdev_capabilities
				octtx_zip_pmd_capabilities[] = {
	{	.algo = RTE_COMP_ALGO_DEFLATE,
		/* Deflate */
		.comp_feature_flags =	RTE_COMP_FF_HUFFMAN_FIXED |
					RTE_COMP_FF_HUFFMAN_DYNAMIC,
		/* Non sharable Priv XFORM and Stateless */
		.window_size = {
				.min = 1,
				.max = 14,
				.increment = 1
				/* size supported 2^1 to 2^14 */
		},
	},
	RTE_COMP_END_OF_CAPABILITIES_LIST()
};

/** Parse xform parameters and setup a stream */
static int
zip_set_stream_parameters(struct rte_compressdev *dev,
			const struct rte_comp_xform *xform,
			struct zip_stream *z_stream)
{
	int ret;
	union zip_inst_s *inst;
	struct zip_vf *vf = (struct zip_vf *)dev->data->dev_private;
	void *res;

	/* Allocate resources required by a stream */
	ret = rte_mempool_get_bulk(vf->zip_mp,
			z_stream->bufs, MAX_BUFS_PER_STREAM);
	if (ret < 0)
		return -1;

	/* get one command buffer from pool and set up */
	inst = (union zip_inst_s *)z_stream->bufs[CMD_BUF];
	res = z_stream->bufs[RES_BUF];

	memset(inst->u, 0, sizeof(inst->u));

	/* set bf for only first ops of stream */
	inst->s.bf = 1;

	if (xform->type == RTE_COMP_COMPRESS) {
		inst->s.op = ZIP_OP_E_COMP;

		switch (xform->compress.deflate.huffman) {
		case RTE_COMP_HUFFMAN_DEFAULT:
			inst->s.cc = ZIP_CC_DEFAULT;
			break;
		case RTE_COMP_HUFFMAN_FIXED:
			inst->s.cc = ZIP_CC_FIXED_HUFF;
			break;
		case RTE_COMP_HUFFMAN_DYNAMIC:
			inst->s.cc = ZIP_CC_DYN_HUFF;
			break;
		default:
			ret = -1;
			goto err;
		}

		switch (xform->compress.level) {
		case RTE_COMP_LEVEL_MIN:
			inst->s.ss = ZIP_COMP_E_LEVEL_MIN;
			break;
		case RTE_COMP_LEVEL_MAX:
			inst->s.ss = ZIP_COMP_E_LEVEL_MAX;
			break;
		case RTE_COMP_LEVEL_NONE:
			ZIP_PMD_ERR("Compression level not supported");
			ret = -1;
			goto err;
		default:
			/* for any value between min and max , choose
			 * PMD default.
			 */
			inst->s.ss = ZIP_COMP_E_LEVEL_MED; /** PMD default **/
			break;
		}
	} else if (xform->type == RTE_COMP_DECOMPRESS) {
		inst->s.op = ZIP_OP_E_DECOMP;
		/* from HRM,
		 * For DEFLATE decompression, [CC] must be 0x0.
		 * For decompression, [SS] must be 0x0
		 */
		inst->s.cc = 0;
		/* Speed bit should not be set for decompression */
		inst->s.ss = 0;
		/* decompression context is supported only for STATEFUL
		 * operations. Currently we support STATELESS ONLY so
		 * skip setting of ctx pointer
		 */

	} else {
		ZIP_PMD_ERR("\nxform type not supported");
		ret = -1;
		goto err;
	}

	inst->s.res_ptr_addr.s.addr = rte_mempool_virt2iova(res);
	inst->s.res_ptr_ctl.s.length = 0;

	z_stream->inst = inst;

	return 0;

err:
	rte_mempool_put_bulk(vf->zip_mp,
			     (void *)&(z_stream->bufs[0]),
			     MAX_BUFS_PER_STREAM);

	return ret;
}

/** Configure device */
static int
zip_pmd_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	int nb_streams;
	char res_pool[RTE_MEMZONE_NAMESIZE];
	struct zip_vf *vf;
	struct rte_mempool *zip_buf_mp;

	if (!config || !dev)
		return -EIO;

	vf = (struct zip_vf *)(dev->data->dev_private);

	/* create pool with maximum numbers of resources
	 * required by streams
	 */

	/* use common pool for non-shareable priv_xform and stream */
	nb_streams = config->max_nb_priv_xforms + config->max_nb_streams;

	snprintf(res_pool, RTE_MEMZONE_NAMESIZE, "octtx_zip_res_pool%u",
		 dev->data->dev_id);

	/** TBD Should we use the per core object cache for stream resources */
	zip_buf_mp = rte_mempool_create(
			res_pool,
			nb_streams * MAX_BUFS_PER_STREAM,
			ZIP_BUF_SIZE,
			0,
			0,
			NULL,
			NULL,
			NULL,
			NULL,
			SOCKET_ID_ANY,
			0);

	if (zip_buf_mp == NULL) {
		ZIP_PMD_ERR(
			"Failed to create buf mempool octtx_zip_res_pool%u",
			dev->data->dev_id);
		return -1;
	}

	vf->zip_mp = zip_buf_mp;

	return 0;
}

/** Start device */
static int
zip_pmd_start(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Stop device */
static void
zip_pmd_stop(__rte_unused struct rte_compressdev *dev)
{

}

/** Close device */
static int
zip_pmd_close(struct rte_compressdev *dev)
{
	if (dev == NULL)
		return -1;

	struct zip_vf *vf = (struct zip_vf *)dev->data->dev_private;
	rte_mempool_free(vf->zip_mp);

	return 0;
}

/** Get device statistics */
static void
zip_pmd_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zipvf_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
zip_pmd_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zipvf_qp *qp = dev->data->queue_pairs[qp_id];
		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

/** Get device info */
static void
zip_pmd_info_get(struct rte_compressdev *dev,
		struct rte_compressdev_info *dev_info)
{
	struct zip_vf *vf = (struct zip_vf *)dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_name = dev->device->driver->name;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = octtx_zip_pmd_capabilities;
		dev_info->max_nb_queue_pairs = vf->max_nb_queue_pairs;
	}
}

/** Release queue pair */
static int
zip_pmd_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct zipvf_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp != NULL) {
		zipvf_q_term(qp);

		if (qp->processed_pkts)
			rte_ring_free(qp->processed_pkts);

		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
zip_pmd_qp_create_processed_pkts_ring(struct zipvf_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			ZIP_PMD_INFO("Reusing existing ring %s for processed"
					" packets", qp->name);
			return r;
		}

		ZIP_PMD_ERR("Unable to reuse existing ring %s for processed"
				" packets", qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
						RING_F_EXACT_SZ);
}

/** Setup a queue pair */
static int
zip_pmd_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct zipvf_qp *qp = NULL;
	struct zip_vf *vf;
	char *name;
	int ret;

	if (!dev)
		return -1;

	vf = (struct zip_vf *) (dev->data->dev_private);

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL) {
		ZIP_PMD_INFO("Using existing queue pair %d ", qp_id);
		return 0;
	}

	name =  rte_malloc(NULL, RTE_COMPRESSDEV_NAME_MAX_LEN, 0);
	snprintf(name, RTE_COMPRESSDEV_NAME_MAX_LEN,
		 "zip_pmd_%u_qp_%u",
		 dev->data->dev_id, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket(name, sizeof(*qp),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->name = name;

	/* Create completion queue upto max_inflight_ops */
	qp->processed_pkts = zip_pmd_qp_create_processed_pkts_ring(qp,
						max_inflight_ops, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	qp->id = qp_id;
	qp->vf = vf;

	ret = zipvf_q_init(qp);
	if (ret < 0)
		goto qp_setup_cleanup;

	dev->data->queue_pairs[qp_id] = qp;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	return 0;

qp_setup_cleanup:
	if (qp->processed_pkts)
		rte_ring_free(qp->processed_pkts);
	if (qp)
		rte_free(qp);
	return -1;
}

static int
zip_pmd_stream_create(struct rte_compressdev *dev,
		const struct rte_comp_xform *xform, void **stream)
{
	int ret;
	struct zip_stream *strm = NULL;

	strm = rte_malloc(NULL,
			sizeof(struct zip_stream), 0);

	if (strm == NULL)
		return (-ENOMEM);

	ret = zip_set_stream_parameters(dev, xform, strm);
	if (ret < 0) {
		ZIP_PMD_ERR("failed configure xform parameters");
		rte_free(strm);
		return ret;
	}
	*stream = strm;
	return 0;
}

static int
zip_pmd_stream_free(struct rte_compressdev *dev, void *stream)
{
	struct zip_vf *vf = (struct zip_vf *) (dev->data->dev_private);
	struct zip_stream *z_stream;

	if (stream == NULL)
		return 0;

	z_stream = (struct zip_stream *)stream;

	/* Free resources back to pool */
	rte_mempool_put_bulk(vf->zip_mp,
				(void *)&(z_stream->bufs[0]),
				MAX_BUFS_PER_STREAM);

	/* Zero out the whole structure */
	memset(stream, 0, sizeof(struct zip_stream));
	rte_free(stream);

	return 0;
}


struct rte_compressdev_ops octtx_zip_pmd_ops = {
		.dev_configure		= zip_pmd_config,
		.dev_start		= zip_pmd_start,
		.dev_stop		= zip_pmd_stop,
		.dev_close		= zip_pmd_close,

		.stats_get		= zip_pmd_stats_get,
		.stats_reset		= zip_pmd_stats_reset,

		.dev_infos_get		= zip_pmd_info_get,

		.queue_pair_setup	= zip_pmd_qp_setup,
		.queue_pair_release	= zip_pmd_qp_release,

		.private_xform_create	= zip_pmd_stream_create,
		.private_xform_free	= zip_pmd_stream_free,
		.stream_create		= NULL,
		.stream_free		= NULL
};

static int
zip_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int ret = 0;
	char compressdev_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};

	ZIP_PMD_INFO("vendor_id=0x%x device_id=0x%x",
			(unsigned int)pci_dev->id.vendor_id,
			(unsigned int)pci_dev->id.device_id);

	rte_pci_device_name(&pci_dev->addr, compressdev_name,
			    sizeof(compressdev_name));

	compressdev = rte_compressdev_pmd_create(compressdev_name,
		&pci_dev->device, sizeof(struct zip_vf), &init_params);
	if (compressdev == NULL) {
		ZIP_PMD_ERR("driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	/*
	 * create only if proc_type is primary.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/*  create vf dev with given pmd dev id */
		ret = zipvf_create(compressdev);
		if (ret < 0) {
			ZIP_PMD_ERR("Device creation failed");
			rte_compressdev_pmd_destroy(compressdev);
			return ret;
		}
	}

	compressdev->dev_ops = &octtx_zip_pmd_ops;
	/* register rx/tx burst functions for data path */
	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
	return ret;
}

static int
zip_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_compressdev *compressdev;
	char compressdev_name[RTE_COMPRESSDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		ZIP_PMD_ERR(" Invalid PCI Device\n");
		return -EINVAL;
	}
	rte_pci_device_name(&pci_dev->addr, compressdev_name,
			sizeof(compressdev_name));

	compressdev = rte_compressdev_pmd_get_named_dev(compressdev_name);
	if (compressdev == NULL)
		return -ENODEV;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (zipvf_destroy(compressdev) < 0)
			return -ENODEV;
	}
	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_pci_id pci_id_octtx_zipvf_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			PCI_DEVICE_ID_OCTEONTX_ZIPVF),
	},
	{
		.device_id = 0
	},
};

/**
 * Structure that represents a PCI driver
 */
static struct rte_pci_driver octtx_zip_pmd = {
	.id_table    = pci_id_octtx_zipvf_table,
	.drv_flags   = RTE_PCI_DRV_NEED_MAPPING,
	.probe       = zip_pci_probe,
	.remove      = zip_pci_remove,
};

RTE_PMD_REGISTER_PCI(COMPRESSDEV_NAME_ZIP_PMD, octtx_zip_pmd);
RTE_PMD_REGISTER_PCI_TABLE(COMPRESSDEV_NAME_ZIP_PMD, pci_id_octtx_zipvf_table);

RTE_INIT(octtx_zip_init_log);

static void
octtx_zip_init_log(void)
{
	octtx_zip_logtype_driver = rte_log_register("pmd.compress.octeontx");
	if (octtx_zip_logtype_driver >= 0)
		rte_log_set_level(octtx_zip_logtype_driver, RTE_LOG_INFO);
}
