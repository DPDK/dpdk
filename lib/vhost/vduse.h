/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#ifndef _VDUSE_H
#define _VDUSE_H

#include "vhost.h"

#define VDUSE_NET_SUPPORTED_FEATURES VIRTIO_NET_SUPPORTED_FEATURES

int vduse_device_create(const char *path, bool compliant_ol_flags);
int vduse_device_destroy(const char *path);

#endif /* _VDUSE_H */
