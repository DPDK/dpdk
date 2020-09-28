/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

/**
 * @file
 *
 * Compatibility header
 *
 * Although symbols declared here are present on Windows,
 * including <winsock2.h> would expose too much macros breaking common code.
 */

#include <netinet/in.h>
#include <sys/socket.h>

/* defined in ws2_32.dll */
__attribute__((stdcall))
int
inet_pton(int af, const char *src, void *dst);

/* defined in ws2_32.dll */
__attribute__((stdcall))
const char *
inet_ntop(int af, const void *src, char *dst, socklen_t size);

#endif /* _ARPA_INET_H_ */
