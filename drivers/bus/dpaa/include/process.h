/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PROCESS_H
#define	__PROCESS_H

#include <compat.h>

/* The process device underlies process-wide user/kernel interactions, such as
 * mapping dma_mem memory and providing accompanying ioctl()s. (This isn't used
 * for portals, which use one UIO device each.).
 */
#define PROCESS_PATH		"/dev/fsl-usdpaa"

/* Allocation of resource IDs uses a generic interface. This enum is used to
 * distinguish between the type of underlying object being manipulated.
 */
enum dpaa_id_type {
	dpaa_id_fqid,
	dpaa_id_bpid,
	dpaa_id_qpool,
	dpaa_id_cgrid,
	dpaa_id_max /* <-- not a valid type, represents the number of types */
};

int process_alloc(enum dpaa_id_type id_type, uint32_t *base, uint32_t num,
		  uint32_t align, int partial);
void process_release(enum dpaa_id_type id_type, uint32_t base, uint32_t num);

int process_reserve(enum dpaa_id_type id_type, uint32_t base, uint32_t num);

/* Mapping and using QMan/BMan portals */
enum dpaa_portal_type {
	dpaa_portal_qman,
	dpaa_portal_bman,
};

struct dpaa_ioctl_portal_map {
	/* Input parameter, is a qman or bman portal required. */
	enum dpaa_portal_type type;
	/* Specifes a specific portal index to map or 0xffffffff
	 * for don't care.
	 */
	uint32_t index;

	/* Return value if the map succeeds, this gives the mapped
	 * cache-inhibited (cinh) and cache-enabled (cena) addresses.
	 */
	struct dpaa_portal_map {
		void *cinh;
		void *cena;
	} addr;
	/* Qman-specific return values */
	u16 channel;
	uint32_t pools;
};

int process_portal_map(struct dpaa_ioctl_portal_map *params);
int process_portal_unmap(struct dpaa_portal_map *map);

struct dpaa_ioctl_irq_map {
	enum dpaa_portal_type type; /* Type of portal to map */
	int fd; /* File descriptor that contains the portal */
	void *portal_cinh; /* Cache inhibited area to identify the portal */
};

int process_portal_irq_map(int fd,  struct dpaa_ioctl_irq_map *irq);
int process_portal_irq_unmap(int fd);

#endif	/*  __PROCESS_H */
