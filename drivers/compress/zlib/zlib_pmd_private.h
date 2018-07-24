/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef _RTE_ZLIB_PMD_PRIVATE_H_
#define _RTE_ZLIB_PMD_PRIVATE_H_

#include <zlib.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#define COMPRESSDEV_NAME_ZLIB_PMD	compress_zlib
/**< ZLIB PMD device name */

#define DEF_MEM_LEVEL			8

int zlib_logtype_driver;
#define ZLIB_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, zlib_logtype_driver, "%s(): "fmt "\n", \
			__func__, ##args)

#define ZLIB_PMD_INFO(fmt, args...) \
	ZLIB_PMD_LOG(INFO, fmt, ## args)
#define ZLIB_PMD_ERR(fmt, args...) \
	ZLIB_PMD_LOG(ERR, fmt, ## args)
#define ZLIB_PMD_WARN(fmt, args...) \
	ZLIB_PMD_LOG(WARNING, fmt, ## args)

struct zlib_private {
	struct rte_mempool *mp;
};

struct zlib_qp {
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_compressdev_stats qp_stats;
	/**< Queue pair statistics */
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
} __rte_cache_aligned;

/* Algorithm handler function prototype */
typedef void (*comp_func_t)(struct rte_comp_op *op, z_stream *strm);

typedef int (*comp_free_t)(z_stream *strm);

/** ZLIB Stream structure */
struct zlib_stream {
	z_stream strm;
	/**< zlib stream structure */
	comp_func_t comp;
	/**< Operation (compression/decompression) */
	comp_free_t free;
	/**< Free Operation (compression/decompression) */
} __rte_cache_aligned;

/** ZLIB private xform structure */
struct zlib_priv_xform {
	struct zlib_stream stream;
} __rte_cache_aligned;

int
zlib_set_stream_parameters(const struct rte_comp_xform *xform,
		struct zlib_stream *stream);

/** Device specific operations function pointer structure */
extern struct rte_compressdev_ops *rte_zlib_pmd_ops;

#endif /* _RTE_ZLIB_PMD_PRIVATE_H_ */
