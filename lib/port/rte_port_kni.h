/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Ethan Zhuang <zhuangwj@gmail.com>.
 * Copyright(c) 2016 Intel Corporation.
 */

#ifndef __INCLUDE_RTE_PORT_KNI_H__
#define __INCLUDE_RTE_PORT_KNI_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port KNI Interface
 *
 * kni_reader: input port built on top of pre-initialized KNI interface
 * kni_writer: output port built on top of pre-initialized KNI interface
 *
 ***/

#include <stdint.h>


#include "rte_port.h"

/** kni_reader port parameters */
struct rte_port_kni_reader_params {
	/** KNI interface reference */
	struct rte_kni *kni;
};

/** kni_reader port operations */
extern struct rte_port_in_ops rte_port_kni_reader_ops;


/** kni_writer port parameters */
struct rte_port_kni_writer_params {
	/** KNI interface reference */
	struct rte_kni *kni;
	/** Burst size to KNI interface. */
	uint32_t tx_burst_sz;
};

/** kni_writer port operations */
extern struct rte_port_out_ops rte_port_kni_writer_ops;

/** kni_writer_nodrop port parameters */
struct rte_port_kni_writer_nodrop_params {
	/** KNI interface reference */
	struct rte_kni *kni;
	/** Burst size to KNI interface. */
	uint32_t tx_burst_sz;
	/** Maximum number of retries, 0 for no limit */
	uint32_t n_retries;
};

/** kni_writer_nodrop port operations */
extern struct rte_port_out_ops rte_port_kni_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
