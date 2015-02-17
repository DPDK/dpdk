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

#ifndef EAL_THREAD_H
#define EAL_THREAD_H

#include <rte_lcore.h>

/**
 * basic loop of thread, called for each thread by eal_init().
 *
 * @param arg
 *   opaque pointer
 */
__attribute__((noreturn)) void *eal_thread_loop(void *arg);

/**
 * Init per-lcore info for master thread
 *
 * @param lcore_id
 *   identifier of master lcore
 */
void eal_thread_init_master(unsigned lcore_id);

/**
 * Get the NUMA socket id from cpu id.
 * This function is private to EAL.
 *
 * @param cpu_id
 *   The logical process id.
 * @return
 *   socket_id or SOCKET_ID_ANY
 */
unsigned eal_cpu_socket_id(unsigned cpu_id);

/**
 * Get the NUMA socket id from cpuset.
 * This function is private to EAL.
 *
 * @param cpusetp
 *   The point to a valid cpu set.
 * @return
 *   socket_id or SOCKET_ID_ANY
 */
int eal_cpuset_socket_id(rte_cpuset_t *cpusetp);

/**
 * Default buffer size to use with eal_thread_dump_affinity()
 */
#define RTE_CPU_AFFINITY_STR_LEN            256

/**
 * Dump the current pthread cpuset.
 * This function is private to EAL.
 *
 * Note:
 *   If the dump size is greater than the size of given buffer,
 *   the string will be truncated and with '\0' at the end.
 *
 * @param str
 *   The string buffer the cpuset will dump to.
 * @param size
 *   The string buffer size.
 * @return
 *   0 for success, -1 if truncation happens.
 */
int
eal_thread_dump_affinity(char *str, unsigned size);

#endif /* EAL_THREAD_H */
