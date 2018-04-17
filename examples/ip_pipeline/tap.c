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

#include "tap.h"

#define TAP_DEV                                            "/dev/net/tun"

static struct tap_list tap_list;

int
tap_init(void)
{
	TAILQ_INIT(&tap_list);

	return 0;
}

struct tap *
tap_find(const char *name)
{
	struct tap *tap;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(tap, &tap_list, node)
		if (strcmp(tap->name, name) == 0)
			return tap;

	return NULL;
}

#ifndef RTE_EXEC_ENV_LINUXAPP

struct tap *
tap_create(const char *name __rte_unused)
{
	return NULL;
}

#else

struct tap *
tap_create(const char *name)
{
	struct tap *tap;
	struct ifreq ifr;
	int fd, status;

	/* Check input params */
	if ((name == NULL) ||
		tap_find(name))
		return NULL;

	/* Resource create */
	fd = open(TAP_DEV, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* No packet information */
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);

	status = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (status < 0) {
		close(fd);
		return NULL;
	}

	/* Node allocation */
	tap = calloc(1, sizeof(struct tap));
	if (tap == NULL) {
		close(fd);
		return NULL;
	}
	/* Node fill in */
	strlcpy(tap->name, name, sizeof(tap->name));
	tap->fd = fd;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&tap_list, tap, node);

	return tap;
}

#endif
