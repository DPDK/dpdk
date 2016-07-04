/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EAL_PCI_INIT_H_
#define EAL_PCI_INIT_H_

#include "eal_vfio.h"

/** IO resource type: */
#define IORESOURCE_IO         0x00000100
#define IORESOURCE_MEM        0x00000200

/*
 * Helper function to map PCI resources right after hugepages in virtual memory
 */
extern void *pci_map_addr;
void *pci_find_max_end_va(void);

/* parse one line of the "resource" sysfs file (note that the 'line'
 * string is modified)
 */
int pci_parse_one_sysfs_resource(char *line, size_t len, uint64_t *phys_addr,
	uint64_t *end_addr, uint64_t *flags);

int pci_uio_alloc_resource(struct rte_pci_device *dev,
		struct mapped_pci_resource **uio_res);
void pci_uio_free_resource(struct rte_pci_device *dev,
		struct mapped_pci_resource *uio_res);
int pci_uio_map_resource_by_index(struct rte_pci_device *dev, int res_idx,
		struct mapped_pci_resource *uio_res, int map_idx);

int pci_uio_read_config(const struct rte_intr_handle *intr_handle,
			void *buf, size_t len, off_t offs);
int pci_uio_write_config(const struct rte_intr_handle *intr_handle,
			 const void *buf, size_t len, off_t offs);

int pci_uio_ioport_map(struct rte_pci_device *dev, int bar,
		       struct rte_pci_ioport *p);
void pci_uio_ioport_read(struct rte_pci_ioport *p,
			 void *data, size_t len, off_t offset);
void pci_uio_ioport_write(struct rte_pci_ioport *p,
			  const void *data, size_t len, off_t offset);
int pci_uio_ioport_unmap(struct rte_pci_ioport *p);

#ifdef VFIO_PRESENT

/* access config space */
int pci_vfio_read_config(const struct rte_intr_handle *intr_handle,
			 void *buf, size_t len, off_t offs);
int pci_vfio_write_config(const struct rte_intr_handle *intr_handle,
			  const void *buf, size_t len, off_t offs);

int pci_vfio_ioport_map(struct rte_pci_device *dev, int bar,
		        struct rte_pci_ioport *p);
void pci_vfio_ioport_read(struct rte_pci_ioport *p,
			  void *data, size_t len, off_t offset);
void pci_vfio_ioport_write(struct rte_pci_ioport *p,
			   const void *data, size_t len, off_t offset);
int pci_vfio_ioport_unmap(struct rte_pci_ioport *p);

/* map VFIO resource prototype */
int pci_vfio_map_resource(struct rte_pci_device *dev);

#endif

#endif /* EAL_PCI_INIT_H_ */
