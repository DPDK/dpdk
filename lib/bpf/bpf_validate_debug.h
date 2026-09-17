/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _BPF_VALIDATE_DEBUG_H_
#define _BPF_VALIDATE_DEBUG_H_

/**
 * @file bpf_validate_debug.h
 *
 * Internal-use headers for eBPF validation debug notifications.
 */

#include "rte_bpf_validate_debug.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_bpf_prm_ex;
struct rte_bpf_validate_debug;
struct bpf_verifier;

/* Type alias for validation event enum. */
typedef enum rte_bpf_validate_debug_event debug_event_t;

/*
 * Signal beginning of evaluation process.
 *
 * Immediately return 0 if debug is NULL.
 *
 * @param debug
 *   Validate debug instance configured by user, can be NULL.
 * @param verifier
 *   Opaque pointer that can be used for calling bpf_validate.h API.
 * @param bpf_prm
 *   Parameters struct of the validated eBPF program, including code with all
 *   patches and relocations applied.
 * @return
 *   Non-negative value on success, negative errno on failure.
 */
int
__rte_bpf_validate_debug_evaluate_start(struct rte_bpf_validate_debug *debug,
	const struct bpf_verifier *verifier, const struct rte_bpf_prm_ex *bpf_prm);

/*
 * Signal evaluation events.
 *
 * Immediately return 0 if debug is NULL.
 *
 * @param debug
 *   Validate debug instance configured by user, can be NULL.
 * @param pc
 *   Current value of the program counter, or next after last instruction.
 * @param events
 *   Bitmask of events.
 *   Step bit is always implied and should not be set.
 * @return
 *   Non-negative value: evaluation should continue;
 *   -ECANCELED: evaluation should fail without calling this API again;
 *   Other negative value: evaluation should fail signalling failure;
 */
int
__rte_bpf_validate_debug_evaluate_update(struct rte_bpf_validate_debug *debug,
	uint32_t pc, uint64_t events);

/*
 * Signal end of evaluation process.
 *
 * Immediately return 0 if debug is NULL.
 *
 * @param debug
 *   Validate debug instance configured by user, can be NULL.
 * @return
 *   Non-negative value on success, negative errno on failure.
 */
int
__rte_bpf_validate_debug_evaluate_finish(struct rte_bpf_validate_debug *debug,
	int result);

#ifdef __cplusplus
}
#endif

#endif /* _BPF_VALIDATE_DEBUG_H_ */
