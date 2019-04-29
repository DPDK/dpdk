/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File hw_atl_b0.h: Declaration of abstract interface for Atlantic hardware
 * specific functions.
 */

#ifndef HW_ATL_B0_H
#define HW_ATL_B0_H

int hw_atl_b0_hw_reset(struct aq_hw_s *self);
int hw_atl_b0_hw_init(struct aq_hw_s *self, u8 *mac_addr);

int hw_atl_b0_set_fc(struct aq_hw_s *self, u32 fc, u32 tc);

int hw_atl_b0_hw_ring_tx_init(struct aq_hw_s *self, uint64_t base_addr,
		int index, int size, int cpu, int vec);
int hw_atl_b0_hw_ring_rx_init(struct aq_hw_s *self, uint64_t base_addr,
		int index, int size, int buff_size, int cpu, int vec);

int hw_atl_b0_hw_start(struct aq_hw_s *self);

int hw_atl_b0_hw_ring_rx_start(struct aq_hw_s *self, int index);
int hw_atl_b0_hw_ring_tx_start(struct aq_hw_s *self, int index);


int hw_atl_b0_hw_ring_tx_stop(struct aq_hw_s *self, int index);
int hw_atl_b0_hw_ring_rx_stop(struct aq_hw_s *self, int index);


int hw_atl_b0_hw_tx_ring_tail_update(struct aq_hw_s *self, int tail, int index);

int hw_atl_b0_hw_rss_hash_set(struct aq_hw_s *self,
				     struct aq_rss_parameters *rss_params);
int hw_atl_b0_hw_rss_set(struct aq_hw_s *self,
				struct aq_rss_parameters *rss_params);

int hw_atl_b0_hw_irq_enable(struct aq_hw_s *self, u64 mask);
int hw_atl_b0_hw_irq_disable(struct aq_hw_s *self, u64 mask);
int hw_atl_b0_hw_irq_read(struct aq_hw_s *self, u64 *mask);

#endif /* HW_ATL_B0_H */
