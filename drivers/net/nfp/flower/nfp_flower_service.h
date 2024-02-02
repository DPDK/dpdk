/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_FLOWER_SERVICE_H__
#define __NFP_FLOWER_SERVICE_H__

struct nfp_flower_service;

int nfp_flower_service_start(void *app_fw_flower);
void nfp_flower_service_stop(void *app_fw_flower);

int nfp_flower_service_sync_alloc(void *app_fw_flower);
void nfp_flower_service_sync_free(void *app_fw_flower);

#endif /* __NFP_FLOWER_SERVICE_H__ */
