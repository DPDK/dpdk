/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Intel Corporation
 */

#ifndef BBDEV_TRACE_H
#define BBDEV_TRACE_H

/**
 * @file
 *
 * API for bbdev trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

#include "rte_bbdev.h"

RTE_TRACE_POINT(
	rte_bbdev_trace_setup_queues,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t num_queues, int socket_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(num_queues);
	rte_trace_point_emit_int(socket_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_configure,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id, const char *op_str, uint8_t pri),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
	rte_trace_point_emit_string(op_str);
	rte_trace_point_emit_u8(pri);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_close,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
)
RTE_TRACE_POINT(
	rte_bbdev_trace_queue_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_id);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_ldpc_dec,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_ldpc_dec ldpc_dec),
	rte_trace_point_emit_u8(ldpc_dec.code_block_mode);
	rte_trace_point_emit_u8(ldpc_dec.tb_params.c);
	rte_trace_point_emit_u8(ldpc_dec.tb_params.cab);
	rte_trace_point_emit_u32(ldpc_dec.tb_params.ea);
	rte_trace_point_emit_u32(ldpc_dec.tb_params.eb);
	rte_trace_point_emit_u8(ldpc_dec.tb_params.r);
	rte_trace_point_emit_u32(ldpc_dec.cb_params.e);
	rte_trace_point_emit_u32(ldpc_dec.op_flags);
	rte_trace_point_emit_u8(ldpc_dec.basegraph);
	rte_trace_point_emit_u16(ldpc_dec.z_c);
	rte_trace_point_emit_u16(ldpc_dec.n_cb);
	rte_trace_point_emit_u8(ldpc_dec.q_m);
	rte_trace_point_emit_u16(ldpc_dec.n_filler);
	rte_trace_point_emit_u8(ldpc_dec.rv_index);
	rte_trace_point_emit_u8(ldpc_dec.iter_max);
	rte_trace_point_emit_u8(ldpc_dec.iter_count);
	rte_trace_point_emit_u32(ldpc_dec.harq_combined_input.length);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_ldpc_enc,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_ldpc_enc ldpc_enc),
	rte_trace_point_emit_u8(ldpc_enc.code_block_mode);
	rte_trace_point_emit_u8(ldpc_enc.tb_params.c);
	rte_trace_point_emit_u8(ldpc_enc.tb_params.cab);
	rte_trace_point_emit_u32(ldpc_enc.tb_params.ea);
	rte_trace_point_emit_u32(ldpc_enc.tb_params.eb);
	rte_trace_point_emit_u8(ldpc_enc.tb_params.r);
	rte_trace_point_emit_u32(ldpc_enc.cb_params.e);
	rte_trace_point_emit_u32(ldpc_enc.op_flags);
	rte_trace_point_emit_u8(ldpc_enc.basegraph);
	rte_trace_point_emit_u16(ldpc_enc.z_c);
	rte_trace_point_emit_u16(ldpc_enc.n_cb);
	rte_trace_point_emit_u8(ldpc_enc.q_m);
	rte_trace_point_emit_u16(ldpc_enc.n_filler);
	rte_trace_point_emit_u8(ldpc_enc.rv_index);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_turbo_enc,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_turbo_enc turbo_enc),
	rte_trace_point_emit_u8(turbo_enc.code_block_mode);
	rte_trace_point_emit_u32(turbo_enc.op_flags);
	rte_trace_point_emit_u8(turbo_enc.rv_index);
	rte_trace_point_emit_u16(turbo_enc.tb_params.k_neg);
	rte_trace_point_emit_u16(turbo_enc.tb_params.k_pos);
	rte_trace_point_emit_u8(turbo_enc.tb_params.c_neg);
	rte_trace_point_emit_u8(turbo_enc.tb_params.c);
	rte_trace_point_emit_u8(turbo_enc.tb_params.cab);
	rte_trace_point_emit_u32(turbo_enc.tb_params.ea);
	rte_trace_point_emit_u32(turbo_enc.tb_params.eb);
	rte_trace_point_emit_u8(turbo_enc.tb_params.r);
	rte_trace_point_emit_u32(turbo_enc.cb_params.e);
	rte_trace_point_emit_u16(turbo_enc.cb_params.k);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_turbo_dec,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_turbo_dec turbo_dec),
	rte_trace_point_emit_u8(turbo_dec.code_block_mode);
	rte_trace_point_emit_u32(turbo_dec.op_flags);
	rte_trace_point_emit_u8(turbo_dec.rv_index);
	rte_trace_point_emit_u8(turbo_dec.iter_count);
	rte_trace_point_emit_u8(turbo_dec.ext_scale);
	rte_trace_point_emit_u8(turbo_dec.num_maps);
	rte_trace_point_emit_u16(turbo_dec.tb_params.k_neg);
	rte_trace_point_emit_u16(turbo_dec.tb_params.k_pos);
	rte_trace_point_emit_u8(turbo_dec.tb_params.c_neg);
	rte_trace_point_emit_u8(turbo_dec.tb_params.c);
	rte_trace_point_emit_u8(turbo_dec.tb_params.cab);
	rte_trace_point_emit_u32(turbo_dec.tb_params.ea);
	rte_trace_point_emit_u32(turbo_dec.tb_params.eb);
	rte_trace_point_emit_u8(turbo_dec.tb_params.r);
	rte_trace_point_emit_u32(turbo_dec.cb_params.e);
	rte_trace_point_emit_u16(turbo_dec.cb_params.k);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_fft,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_fft fft),
	rte_trace_point_emit_u32(fft.op_flags);
	rte_trace_point_emit_u16(fft.input_sequence_size);
	rte_trace_point_emit_u16(fft.input_leading_padding);
	rte_trace_point_emit_u16(fft.output_sequence_size);
	rte_trace_point_emit_u16(fft.output_leading_depadding);
	rte_trace_point_emit_u16(fft.cs_bitmap);
	rte_trace_point_emit_u8(fft.num_antennas_log2);
	rte_trace_point_emit_u8(fft.idft_log2);
	rte_trace_point_emit_u8(fft.dft_log2);
	rte_trace_point_emit_u8(fft.cs_time_adjustment);
	rte_trace_point_emit_u8(fft.idft_shift);
	rte_trace_point_emit_u8(fft.dft_shift);
	rte_trace_point_emit_u16(fft.ncs_reciprocal);
	rte_trace_point_emit_u16(fft.power_shift);
	rte_trace_point_emit_u16(fft.fp16_exp_adjust);
	rte_trace_point_emit_u8(fft.freq_resample_mode);
	rte_trace_point_emit_u16(fft.output_depadded_size);
)

RTE_TRACE_POINT(
	rte_bbdev_trace_op_mldts,
	RTE_TRACE_POINT_ARGS(const struct rte_bbdev_op_mldts mldts),
	rte_trace_point_emit_u32(mldts.op_flags);
	rte_trace_point_emit_u16(mldts.num_rbs);
	rte_trace_point_emit_u16(mldts.num_layers);
	rte_trace_point_emit_u8(mldts.r_rep);
	rte_trace_point_emit_u8(mldts.c_rep);
)

#ifdef __cplusplus
}
#endif

#endif /* BBDEV_TRACE_H */
