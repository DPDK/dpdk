/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#include <rte_bus_vdev.h>
#include <rte_common.h>

#include "zlib_pmd_private.h"

/** Parse comp xform and set private xform/stream parameters */
int
zlib_set_stream_parameters(const struct rte_comp_xform *xform,
		struct zlib_stream *stream)
{
	int strategy, level, wbits;
	z_stream *strm = &stream->strm;

	/* allocate deflate state */
	strm->zalloc = Z_NULL;
	strm->zfree = Z_NULL;
	strm->opaque = Z_NULL;

	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		/** Compression window bits */
		switch (xform->compress.algo) {
		case RTE_COMP_ALGO_DEFLATE:
			wbits = -(xform->compress.window_size);
			break;
		default:
			ZLIB_PMD_ERR("Compression algorithm not supported\n");
			return -1;
		}
		/** Compression Level */
		switch (xform->compress.level) {
		case RTE_COMP_LEVEL_PMD_DEFAULT:
			level = Z_DEFAULT_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_NONE:
			level = Z_NO_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_MIN:
			level = Z_BEST_SPEED;
			break;
		case RTE_COMP_LEVEL_MAX:
			level = Z_BEST_COMPRESSION;
			break;
		default:
			level = xform->compress.level;
			if (level < RTE_COMP_LEVEL_MIN ||
					level > RTE_COMP_LEVEL_MAX) {
				ZLIB_PMD_ERR("Compression level %d "
						"not supported\n",
						level);
				return -1;
			}
			break;
		}
		/** Compression strategy */
		switch (xform->compress.deflate.huffman) {
		case RTE_COMP_HUFFMAN_DEFAULT:
			strategy = Z_DEFAULT_STRATEGY;
			break;
		case RTE_COMP_HUFFMAN_FIXED:
			strategy = Z_FIXED;
			break;
		case RTE_COMP_HUFFMAN_DYNAMIC:
			strategy = Z_DEFAULT_STRATEGY;
			break;
		default:
			ZLIB_PMD_ERR("Compression strategy not supported\n");
			return -1;
		}
		if (deflateInit2(strm, level,
					Z_DEFLATED, wbits,
					DEF_MEM_LEVEL, strategy) != Z_OK) {
			ZLIB_PMD_ERR("Deflate init failed\n");
			return -1;
		}
		break;

	case RTE_COMP_DECOMPRESS:
		/** window bits */
		switch (xform->decompress.algo) {
		case RTE_COMP_ALGO_DEFLATE:
			wbits = -(xform->decompress.window_size);
			break;
		default:
			ZLIB_PMD_ERR("Compression algorithm not supported\n");
			return -1;
		}

		if (inflateInit2(strm, wbits) != Z_OK) {
			ZLIB_PMD_ERR("Inflate init failed\n");
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int
zlib_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct zlib_private), init_params);
	if (dev == NULL) {
		ZLIB_PMD_ERR("driver %s: create failed", init_params->name);
		return -ENODEV;
	}

	dev->dev_ops = rte_zlib_pmd_ops;

	return 0;
}

static int
zlib_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id()
	};
	const char *name;
	const char *input_args;
	int retval;

	name = rte_vdev_device_name(vdev);

	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);

	retval = rte_compressdev_pmd_parse_input_args(&init_params, input_args);
	if (retval < 0) {
		ZLIB_PMD_LOG(ERR,
			"Failed to parse initialisation arguments[%s]\n",
			input_args);
		return -EINVAL;
	}

	return zlib_create(name, vdev, &init_params);
}

static int
zlib_remove(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compressdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_get_named_dev(name);
	if (compressdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_vdev_driver zlib_pmd_drv = {
	.probe = zlib_probe,
	.remove = zlib_remove
};

RTE_PMD_REGISTER_VDEV(COMPRESSDEV_NAME_ZLIB_PMD, zlib_pmd_drv);
RTE_INIT(zlib_init_log);

static void
zlib_init_log(void)
{
	zlib_logtype_driver = rte_log_register("pmd.compress.zlib");
	if (zlib_logtype_driver >= 0)
		rte_log_set_level(zlib_logtype_driver, RTE_LOG_INFO);
}
