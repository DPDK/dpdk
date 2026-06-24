/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _BPF_VALIDATE_H_
#define _BPF_VALIDATE_H_

/**
 * @file bpf_validate.h
 *
 * Internal-use headers for eBPF validation observability.
 */

#include <bpf_def.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bpf_verifier;

/*
 * Return 1 if the verifier passes internal self-check,
 * 0 if it fails, or a negative error code.
 */
int
__rte_bpf_validate_state_is_valid(const struct bpf_verifier *verifier);

/*
 * Return 1 if the specified access instruction is valid,
 * 0 if it is invalid, or a negative error code.
 */
int
__rte_bpf_validate_can_access(const struct bpf_verifier *verifier,
	const struct ebpf_insn *access, uint64_t off64);

/* Get possible truth values of the specified jump condition. */
int
__rte_bpf_validate_may_jump(const struct bpf_verifier *verifier,
	const struct ebpf_insn *jump, uint64_t imm64);

/* Format known information about the register for the user. */
int
__rte_bpf_validate_format_register_info(const struct bpf_verifier *verifier,
	char *buffer, size_t bufsz, uint8_t reg);

/* Format known information about the frame location for the user. */
int
__rte_bpf_validate_format_frame_info(const struct bpf_verifier *verifier,
	char *buffer, size_t bufsz, int32_t offset);

/* Return frame size. */
int32_t
__rte_bpf_validate_get_frame_size(const struct bpf_verifier *verifier);

#ifdef __cplusplus
}
#endif

#endif /* _BPF_VALIDATE_H_ */
