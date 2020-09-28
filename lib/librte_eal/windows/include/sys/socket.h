/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _SYS_SOCKET_H_
#define _SYS_SOCKET_H_

/**
 * @file
 *
 * Compatibility header
 *
 * Although symbols declared here are present on Windows,
 * including <winsock2.h> would expose too much macros breaking common code.
 */

#include <stddef.h>

#define AF_INET  2
#define AF_INET6 23

typedef size_t socklen_t;

#endif /* _SYS_SOCKET_H_ */
