/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Red Hat, Inc.
 */

#ifndef _EAL_FIRMWARE_H_
#define _EAL_FIRMWARE_H_

#include <sys/types.h>

#include <rte_compat.h>

/**
 * Load a firmware in a dynamically allocated buffer, dealing with compressed
 * files if libarchive is available.
 *
 * @param[in] name
 *      Firmware filename to load.
 * @param[out] buf
 *      Buffer allocated by this function. If this function succeeds, the
 *      caller is responsible for calling free() on this buffer.
 * @param[out] bufsz
 *      Size of the data in the buffer.
 *
 * @return
 *      0 if successful.
 *      Negative otherwise, buf and bufsize contents are invalid.
 */
__rte_internal
int
rte_firmware_read(const char *name, void **buf, size_t *bufsz);

#endif /* _EAL_FIRMWARE_H_ */
