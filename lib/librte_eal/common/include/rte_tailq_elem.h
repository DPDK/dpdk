/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *
 * This file contains the type of the tailq elem recognised by DPDK, which
 * can be used to fill out an array of structures describing the tailq.
 *
 * In order to populate an array, the user of this file must define this macro:
 * rte_tailq_elem(idx, name). For example:
 *
 * @code
 * enum rte_tailq_t {
 * #define rte_tailq_elem(idx, name)     idx,
 * #define rte_tailq_end(idx)            idx
 * #include <rte_tailq_elem.h>
 * };
 *
 * const char* rte_tailq_names[RTE_MAX_TAILQ] = {
 * #define rte_tailq_elem(idx, name)     name,
 * #include <rte_tailq_elem.h>
 * };
 * @endcode
 *
 * Note that this file can be included multiple times within the same file.
 */

#ifndef rte_tailq_elem
#define rte_tailq_elem(idx, name)
#endif /* rte_tailq_elem */

#ifndef rte_tailq_end
#define rte_tailq_end(idx)
#endif /* rte_tailq_end */

rte_tailq_elem(RTE_TAILQ_PCI, "PCI_RESOURCE_LIST")

rte_tailq_elem(RTE_TAILQ_MEMPOOL, "RTE_MEMPOOL")

rte_tailq_elem(RTE_TAILQ_RING, "RTE_RING")

rte_tailq_elem(RTE_TAILQ_HASH, "RTE_HASH")

rte_tailq_elem(RTE_TAILQ_FBK_HASH, "RTE_FBK_HASH")

rte_tailq_elem(RTE_TAILQ_LPM, "RTE_LPM")

rte_tailq_elem(RTE_TAILQ_LPM6, "RTE_LPM6")

rte_tailq_elem(RTE_TAILQ_PM, "RTE_PM")

rte_tailq_elem(RTE_TAILQ_ACL, "RTE_ACL")

rte_tailq_elem(RTE_TAILQ_DISTRIBUTOR, "RTE_DISTRIBUTOR")

rte_tailq_end(RTE_TAILQ_NUM)

#undef rte_tailq_elem
#undef rte_tailq_end
