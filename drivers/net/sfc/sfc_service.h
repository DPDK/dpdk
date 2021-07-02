/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_SERVICE_H
#define _SFC_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t sfc_get_service_lcore(int socket_id);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_SERVICE_H */
