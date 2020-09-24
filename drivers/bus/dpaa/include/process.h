/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright 2020 NXP
 *
 */

#ifndef __PROCESS_H
#define	__PROCESS_H

#include <compat.h>
#include <rte_ethdev.h>

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

struct dpaa_portal_map {
	void *cinh;
	void *cena;
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
	struct dpaa_portal_map addr;

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

struct usdpaa_ioctl_link_status {
	char            if_name[IF_NAME_MAX_LEN];
	uint32_t        efd;
};

__rte_internal
int dpaa_intr_enable(char *if_name, int efd);

__rte_internal
int dpaa_intr_disable(char *if_name);

struct usdpaa_ioctl_link_status_args_old {
	/* network device node name */
	char    if_name[IF_NAME_MAX_LEN];
	/* link status(ETH_LINK_UP/DOWN) */
	int     link_status;
};

struct usdpaa_ioctl_link_status_args {
	/* network device node name */
	char    if_name[IF_NAME_MAX_LEN];
	/* link status(ETH_LINK_UP/DOWN) */
	int     link_status;
	/* link speed (ETH_SPEED_NUM_)*/
	int     link_speed;
	/* link duplex (ETH_LINK_[HALF/FULL]_DUPLEX)*/
	int     link_duplex;
	/* link autoneg (ETH_LINK_AUTONEG/FIXED)*/
	int     link_autoneg;

};

struct usdpaa_ioctl_update_link_status_args {
	/* network device node name */
	char    if_name[IF_NAME_MAX_LEN];
	/* link status(ETH_LINK_UP/DOWN) */
	int     link_status;
};

struct usdpaa_ioctl_update_link_speed {
	/* network device node name*/
	char    if_name[IF_NAME_MAX_LEN];
	/* link speed (ETH_SPEED_NUM_)*/
	int     link_speed;
	/* link duplex (ETH_LINK_[HALF/FULL]_DUPLEX)*/
	int     link_duplex;
};

__rte_internal
int dpaa_get_link_status(char *if_name, struct rte_eth_link *link);
__rte_internal
int dpaa_update_link_status(char *if_name, int link_status);
__rte_internal
int dpaa_update_link_speed(char *if_name, int speed, int duplex);
__rte_internal
int dpaa_restart_link_autoneg(char *if_name);
__rte_internal
int dpaa_get_ioctl_version_number(void);

#endif	/*  __PROCESS_H */
