/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#ifndef _VDUSE_H
#define _VDUSE_H

#include "vhost.h"

#ifdef VHOST_HAS_VDUSE

int vduse_device_create(const char *path);
int vduse_device_destroy(const char *path);

#else

static inline int
vduse_device_create(const char *path)
{
	VHOST_LOG_CONFIG(path, ERR, "VDUSE support disabled at build time\n");
	return -1;
}

static inline int
vduse_device_destroy(const char *path)
{
	VHOST_LOG_CONFIG(path, ERR, "VDUSE support disabled at build time\n");
	return -1;
}

#endif /* VHOST_HAS_VDUSE */

#endif /* _VDUSE_H */
