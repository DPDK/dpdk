#ifndef _HSI_FUNC_COMMON_H
#define _HSI_FUNC_COMMON_H

/* Physical memory descriptor */
struct phys_mem_desc {
	dma_addr_t phys_addr;
	void *virt_addr;
	u32 size; /* In bytes */
};

/* Virtual memory descriptor */
struct virt_mem_desc {
	void *ptr;
	u32 size; /* In bytes */
};

#endif
