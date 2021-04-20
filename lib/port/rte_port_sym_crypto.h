/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_SYM_CRYPTO_H__
#define __INCLUDE_RTE_PORT_SYM_CRYPTO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Port sym crypto Interface
 *
 * crypto_reader: input port built on top of pre-initialized crypto interface
 * crypto_writer: output port built on top of pre-initialized crypto interface
 *
 **/

#include <stdint.h>

#include <rte_cryptodev.h>

#include "rte_port.h"

/** Function prototype for reader post action. */
typedef void (*rte_port_sym_crypto_reader_callback_fn)(struct rte_mbuf **pkts,
		uint16_t n_pkts, void *arg);

/** Crypto_reader port parameters */
struct rte_port_sym_crypto_reader_params {
	/** Target cryptodev ID. */
	uint8_t cryptodev_id;

	/** Target cryptodev Queue Pair ID. */
	uint16_t queue_id;

	/** Crypto reader post callback function. */
	rte_port_sym_crypto_reader_callback_fn f_callback;

	/** Crypto reader post callback function arguments. */
	void *arg_callback;
};

/** Crypto_reader port operations. */
extern struct rte_port_in_ops rte_port_sym_crypto_reader_ops;


/** Crypto_writer port parameters. */
struct rte_port_sym_crypto_writer_params {
	/** Target cryptodev ID. */
	uint8_t cryptodev_id;

	/** Target cryptodev Queue Pair ID. */
	uint16_t queue_id;

	/** offset to rte_crypto_op in the mbufs. */
	uint16_t crypto_op_offset;

	/** Burst size to crypto interface. */
	uint32_t tx_burst_sz;
};

/** Crypto_writer port operations. */
extern struct rte_port_out_ops rte_port_sym_crypto_writer_ops;

/** Crypto_writer_nodrop port parameters. */
struct rte_port_sym_crypto_writer_nodrop_params {
	/** Target cryptodev ID. */
	uint8_t cryptodev_id;

	/** Target cryptodev queue pair id. */
	uint16_t queue_id;

	/** Offset to rte_crypto_op in the mbufs. */
	uint16_t crypto_op_offset;

	/** Burst size to crypto interface. */
	uint32_t tx_burst_sz;

	/** Maximum number of retries, 0 for no limit. */
	uint32_t n_retries;
};

/** Crypto_writer_nodrop port operations. */
extern struct rte_port_out_ops rte_port_sym_crypto_writer_nodrop_ops;

#ifdef __cplusplus
}
#endif

#endif
