/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_BPHY_IRQ_
#define _CNXK_BPHY_IRQ_

#include <roc_api.h>

#include "rte_pmd_bphy.h"

struct bphy_irq_queue {
	/* queue holds up to one response */
	void *rsp;
};

struct bphy_device {
#define BPHY_QUEUE_CNT 1
	struct roc_bphy bphy;
	struct roc_bphy_irq_chip *irq_chip;
	struct cnxk_bphy_mem mem;
	/* bphy irq interface supports single queue only */
	struct bphy_irq_queue queues[BPHY_QUEUE_CNT];
};

int cnxk_bphy_intr_init(uint16_t dev_id);
void cnxk_bphy_intr_fini(uint16_t dev_id);
struct cnxk_bphy_mem *cnxk_bphy_mem_get(uint16_t dev_id);
int cnxk_bphy_intr_register(uint16_t dev_id, int irq_num,
			    cnxk_bphy_intr_handler_t handler, void *isr_data,
			    int cpu);
void cnxk_bphy_intr_unregister(uint16_t dev_id, int irq_num);
uint64_t cnxk_bphy_irq_max_get(uint16_t dev_id);

#endif /* _CNXK_BPHY_IRQ_ */
