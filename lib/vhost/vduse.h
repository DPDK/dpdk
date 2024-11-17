/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#ifndef _VDUSE_H
#define _VDUSE_H

#include "vhost.h"

#define VDUSE_NET_SUPPORTED_FEATURES VIRTIO_NET_SUPPORTED_FEATURES

#ifdef VHOST_HAS_VDUSE

int vduse_device_create(const char *path, bool compliant_ol_flags);
int vduse_device_destroy(const char *path);

#else

static inline int
vduse_device_create(const char *path, bool compliant_ol_flags)
{
	RTE_SET_USED(compliant_ol_flags);

	VHOST_CONFIG_LOG(path, ERR, "VDUSE support disabled at build time");
	return -1;
}

static inline int
vduse_device_destroy(const char *path)
{
	VHOST_CONFIG_LOG(path, ERR, "VDUSE support disabled at build time");
	return -1;
}

#endif /* VHOST_HAS_VDUSE */

#endif /* _VDUSE_H */
