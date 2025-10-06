/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef _ZLIB_PMD_PRIVATE_H_
#define _ZLIB_PMD_PRIVATE_H_

#include <zlib.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#define COMPRESSDEV_NAME_ZLIB_PMD	compress_zlib
/**< ZLIB PMD device name */

#define DEF_MEM_LEVEL			8

extern int zlib_logtype_driver;
#define RTE_LOGTYPE_ZLIB_DRIVER zlib_logtype_driver
#define ZLIB_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZLIB_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#define ZLIB_PMD_INFO(fmt, ...) \
	ZLIB_PMD_LOG(INFO, fmt, ## __VA_ARGS__)
#define ZLIB_PMD_ERR(fmt, ...) \
	ZLIB_PMD_LOG(ERR, fmt, ## __VA_ARGS__)
#define ZLIB_PMD_WARN(fmt, ...) \
	ZLIB_PMD_LOG(WARNING, fmt, ## __VA_ARGS__)

struct zlib_private {
	struct rte_mempool *mp;
};

struct __rte_cache_aligned zlib_qp {
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_compressdev_stats qp_stats;
	/**< Queue pair statistics */
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
};

/* Algorithm handler function prototype */
typedef void (*comp_func_t)(struct rte_comp_op *op, z_stream *strm);

typedef int (*comp_free_t)(z_stream *strm);

typedef void (*chksum_func_t)
		(struct rte_comp_op *op, z_stream *strm, enum rte_comp_checksum_type chksum);

/** ZLIB Stream structure */
struct __rte_cache_aligned zlib_stream {
	z_stream strm;
	/**< zlib stream structure */
	comp_func_t comp;
	/**< Operation (compression/decompression) */
	comp_free_t free;
	/**< Free Operation (compression/decompression) */
	chksum_func_t chksum;
	/**< Checksum Operation (compression/decompression) */
	enum rte_comp_checksum_type chksum_type;
	/**< Type of checksum to generate on the uncompressed data */
};

/** ZLIB private xform structure */
struct __rte_cache_aligned zlib_priv_xform {
	struct zlib_stream stream;
};

int
zlib_set_stream_parameters(const struct rte_comp_xform *xform,
		struct zlib_stream *stream);

/** Device specific operations function pointer structure */
extern struct rte_compressdev_ops *rte_zlib_pmd_ops;

#endif /* _ZLIB_PMD_PRIVATE_H_ */
