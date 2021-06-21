/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BPHY_IRQ_
#define _ROC_BPHY_IRQ_

struct roc_bphy_irq_vec {
	int fd;
	int handler_cpu;
	void (*handler)(int irq_num, void *isr_data);
	void *isr_data;
};

struct roc_bphy_irq_chip {
	struct roc_bphy_irq_vec *irq_vecs;
	uint64_t max_irq;
	uint64_t avail_irq_bmask;
	int intfd;
	int n_handlers;
	char *mz_name;
};

__roc_api struct roc_bphy_irq_chip *roc_bphy_intr_init(void);
__roc_api void roc_bphy_intr_fini(struct roc_bphy_irq_chip *irq_chip);

#endif /* _ROC_BPHY_IRQ_ */
