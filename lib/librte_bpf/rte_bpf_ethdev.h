/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_BPF_ETHDEV_H_
#define _RTE_BPF_ETHDEV_H_

/**
 * @file rte_bpf_ethdev.h
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * API to install BPF filter as RX/TX callbacks for eth devices.
 * Note that right now:
 * - it is not MT safe, i.e. it is not allowed to do load/unload for the
 *   same port/queue from different threads in parallel.
 * - though it allows to do load/unload at runtime
 *   (while RX/TX is ongoing on given port/queue).
 * - allows only one BPF program per port/queue,
 * i.e. new load will replace previously loaded for that port/queue BPF program.
 * Filter behaviour - if BPF program returns zero value for a given packet,
 * then it will be dropped inside callback and no further processing
 *   on RX - it will be dropped inside callback and no further processing
 *   for that packet will happen.
 *   on TX - packet will remain unsent, and it is responsibility of the user
 *   to handle such situation (drop, try to send again, etc.).
 */

#include <rte_bpf.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	RTE_BPF_ETH_F_NONE = 0,
	RTE_BPF_ETH_F_JIT  = 0x1, /*< use compiled into native ISA code */
};

/**
 * Unload previously loaded BPF program (if any) from given RX port/queue
 * and remove appropriate RX port/queue callback.
 *
 * @param port
 *   The identifier of the ethernet port
 * @param queue
 *   The identifier of the RX queue on the given port
 */
void __rte_experimental
rte_bpf_eth_rx_unload(uint16_t port, uint16_t queue);

/**
 * Unload previously loaded BPF program (if any) from given TX port/queue
 * and remove appropriate TX port/queue callback.
 *
 * @param port
 *   The identifier of the ethernet port
 * @param queue
 *   The identifier of the TX queue on the given port
 */
void __rte_experimental
rte_bpf_eth_tx_unload(uint16_t port, uint16_t queue);

/**
 * Load BPF program from the ELF file and install callback to execute it
 * on given RX port/queue.
 *
 * @param port
 *   The identifier of the ethernet port
 * @param queue
 *   The identifier of the RX queue on the given port
 * @param fname
 *  Pathname for a ELF file.
 * @param sname
 *  Name of the executable section within the file to load.
 * @param prm
 *  Parameters used to create and initialise the BPF exeution context.
 * @param flags
 *  Flags that define expected expected behavior of the loaded filter
 *  (i.e. jited/non-jited version to use).
 * @return
 *   Zero on successful completion or negative error code otherwise.
 */
int __rte_experimental
rte_bpf_eth_rx_elf_load(uint16_t port, uint16_t queue,
	const struct rte_bpf_prm *prm, const char *fname, const char *sname,
	uint32_t flags);

/**
 * Load BPF program from the ELF file and install callback to execute it
 * on given TX port/queue.
 *
 * @param port
 *   The identifier of the ethernet port
 * @param queue
 *   The identifier of the TX queue on the given port
 * @param fname
 *  Pathname for a ELF file.
 * @param sname
 *  Name of the executable section within the file to load.
 * @param prm
 *  Parameters used to create and initialise the BPF exeution context.
 * @param flags
 *  Flags that define expected expected behavior of the loaded filter
 *  (i.e. jited/non-jited version to use).
 * @return
 *   Zero on successful completion or negative error code otherwise.
 */
int __rte_experimental
rte_bpf_eth_tx_elf_load(uint16_t port, uint16_t queue,
	const struct rte_bpf_prm *prm, const char *fname, const char *sname,
	uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BPF_ETHDEV_H_ */
