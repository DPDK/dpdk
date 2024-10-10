/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTOSS_VIRT_QUEUE_H__
#define __NTOSS_VIRT_QUEUE_H__

#include <stdint.h>
#include <stdalign.h>

#include <rte_memory.h>

struct nthw_virt_queue;

#define SPLIT_RING        0
#define IN_ORDER          1

struct nthw_cvirtq_desc;

struct nthw_received_packets;

#endif /* __NTOSS_VIRT_QUEUE_H__ */
