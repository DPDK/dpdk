/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef _CN10K_CRYPTODEV_EVENT_DP_H_
#define _CN10K_CRYPTODEV_EVENT_DP_H_

__rte_internal
uintptr_t cn10k_cpt_crypto_adapter_dequeue(uintptr_t get_work1);
__rte_internal
uintptr_t cn10k_cpt_crypto_adapter_vector_dequeue(uintptr_t get_work1);

#endif /* _CN10K_CRYPTODEV_EVENT_DP_H_ */
