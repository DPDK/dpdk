/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <netinet/in.h>
#ifdef RTE_EXEC_ENV_LINUXAPP
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

#define TAP_DEV                                            "/dev/net/tun"

int
softnic_tap_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->tap_list);

	return 0;
}

void
softnic_tap_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_tap *tap;

		tap = TAILQ_FIRST(&p->tap_list);
		if (tap == NULL)
			break;

		TAILQ_REMOVE(&p->tap_list, tap, node);
		free(tap);
	}
}

struct softnic_tap *
softnic_tap_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_tap *tap;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(tap, &p->tap_list, node)
		if (strcmp(tap->name, name) == 0)
			return tap;

	return NULL;
}

#ifndef RTE_EXEC_ENV_LINUXAPP

struct softnic_tap *
softnic_tap_create(struct pmd_internals *p __rte_unused,
	const char *name __rte_unused)
{
	return NULL;
}

#else

struct softnic_tap *
softnic_tap_create(struct pmd_internals *p,
	const char *name)
{
	struct softnic_tap *tap;
	struct ifreq ifr;
	int fd, status;

	/* Check input params */
	if (name == NULL ||
		softnic_tap_find(p, name))
		return NULL;

	/* Resource create */
	fd = open(TAP_DEV, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* No packet information */
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);

	status = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if (status < 0) {
		close(fd);
		return NULL;
	}

	/* Node allocation */
	tap = calloc(1, sizeof(struct softnic_tap));
	if (tap == NULL) {
		close(fd);
		return NULL;
	}
	/* Node fill in */
	strlcpy(tap->name, name, sizeof(tap->name));
	tap->fd = fd;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->tap_list, tap, node);

	return tap;
}

#endif
