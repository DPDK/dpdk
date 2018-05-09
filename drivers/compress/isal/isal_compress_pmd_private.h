/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ISAL_COMP_PMD_PRIVATE_H_
#define _ISAL_COMP_PMD_PRIVATE_H_

#define COMPDEV_NAME_ISAL_PMD		compress_isal
/**< ISA-L comp PMD device name */

extern int isal_logtype_driver;
#define ISAL_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, isal_logtype_driver, "%s(): "fmt "\n", \
			__func__, ##args)

/* private data structure for each ISA-L compression device */
struct isal_comp_private {
	struct rte_mempool *priv_xform_mp;
};

/** ISA-L queue pair */
struct isal_comp_qp {
	/* Queue Pair Identifier */
	uint16_t id;
	/* Unique Queue Pair Name */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/* Ring for placing process packets */
	struct rte_ring *processed_pkts;
	/* Queue pair statistics */
	struct rte_compressdev_stats qp_stats;
	/* Compression stream information*/
	struct isal_zstream *stream;
	/* Decompression state information*/
	struct inflate_state *state;
	/* Number of free elements on ring */
	uint16_t num_free_elements;
} __rte_cache_aligned;

/** ISA-L private xform structure */
struct isal_priv_xform {
	enum rte_comp_xform_type type;
	union {
		struct rte_comp_compress_xform compress;
		struct rte_comp_decompress_xform decompress;
	};
	uint32_t level_buffer_size;
} __rte_cache_aligned;

/** Set and validate NULL comp private xform parameters */
extern int
isal_comp_set_priv_xform_parameters(struct isal_priv_xform *priv_xform,
			const struct rte_comp_xform *xform);

/** device specific operations function pointer structure */
extern struct rte_compressdev_ops *isal_compress_pmd_ops;

#endif /* _ISAL_COMP_PMD_PRIVATE_H_ */
