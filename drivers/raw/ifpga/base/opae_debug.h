/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _OPAE_DEBUG_H_
#define _OPAE_DEBUG_H_

#ifdef OPAE_HW_DEBUG
#define opae_log(fmt, args...) printf(fmt, ## args)
#else
#define opae_log(fme, args...) do {} while (0)
#endif

void opae_manager_dump(struct opae_manager *mgr);
void opae_bridge_dump(struct opae_bridge *br);
void opae_accelerator_dump(struct opae_accelerator *acc);
void opae_adapter_dump(struct opae_adapter *adapter, int verbose);

#endif /* _OPAE_DEBUG_H_ */
