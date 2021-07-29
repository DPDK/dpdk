/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_cpp_bridge.h
 *
 * Netronome vNIC DPDK Poll-Mode Driver: CPP Bridge header file
 */

#ifndef _NFP_CPP_BRIDGE_H_
#define _NFP_CPP_BRIDGE_H_

#define NFP_CPP_MEMIO_BOUNDARY	(1 << 20)
#define NFP_BRIDGE_OP_READ	20
#define NFP_BRIDGE_OP_WRITE	30
#define NFP_BRIDGE_OP_IOCTL	40

#define NFP_IOCTL 'n'
#define NFP_IOCTL_CPP_IDENTIFICATION _IOW(NFP_IOCTL, 0x8f, uint32_t)

void nfp_register_cpp_service(struct nfp_cpp *cpp);
int32_t nfp_cpp_bridge_service_func(void *args);

#endif /* _NFP_CPP_BRIDGE_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
