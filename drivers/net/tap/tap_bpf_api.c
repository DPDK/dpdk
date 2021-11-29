/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_eth_tap.h>
#include <tap_flow.h>
#include <tap_autoconf.h>
#include <tap_tcmsgs.h>
#include <tap_bpf.h>
#include <tap_bpf_insns.h>

/**
 * Load BPF program (section cls_q) into the kernel and return a bpf fd
 *
 * @param queue_idx
 *   Queue index matching packet cb
 *
 * @return
 *   -1 if the BPF program couldn't be loaded. An fd (int) otherwise.
 */
int tap_flow_bpf_cls_q(__u32 queue_idx)
{
	cls_q_insns[1].imm = queue_idx;

	return bpf_load(BPF_PROG_TYPE_SCHED_CLS,
		(struct bpf_insn *)cls_q_insns,
		RTE_DIM(cls_q_insns),
		"Dual BSD/GPL");
}

/**
 * Load BPF program (section l3_l4) into the kernel and return a bpf fd.
 *
 * @param[in] key_idx
 *   RSS MAP key index
 *
 * @param[in] map_fd
 *   BPF RSS map file descriptor
 *
 * @return
 *   -1 if the BPF program couldn't be loaded. An fd (int) otherwise.
 */
int tap_flow_bpf_calc_l3_l4_hash(__u32 key_idx, int map_fd)
{
	l3_l4_hash_insns[4].imm = key_idx;
	l3_l4_hash_insns[9].imm = map_fd;

	return bpf_load(BPF_PROG_TYPE_SCHED_ACT,
		(struct bpf_insn *)l3_l4_hash_insns,
		RTE_DIM(l3_l4_hash_insns),
		"Dual BSD/GPL");
}

/**
 * Helper function to convert a pointer to unsigned 64 bits
 *
 * @param[in] ptr
 *   pointer to address
 *
 * @return
 *   64 bit unsigned long type of pointer address
 */
static inline __u64 ptr_to_u64(const void *ptr)
{
	return (__u64)(unsigned long)ptr;
}

/**
 * Call BPF system call
 *
 * @param[in] cmd
 *   BPF command for program loading, map creation, map entry update, etc
 *
 * @param[in] attr
 *   System call attributes relevant to system call command
 *
 * @param[in] size
 *   size of attr parameter
 *
 * @return
 *   -1 if BPF system call failed, 0 otherwise
 */
static inline int sys_bpf(enum bpf_cmd cmd, union bpf_attr *attr,
			unsigned int size)
{
	return syscall(__NR_bpf, cmd, attr, size);
}

/**
 * Load BPF instructions to kernel
 *
 * @param[in] type
 *   BPF program type: classifier or action
 *
 * @param[in] insns
 *   Array of BPF instructions (equivalent to BPF instructions)
 *
 * @param[in] insns_cnt
 *   Number of BPF instructions (size of array)
 *
 * @param[in] license
 *   License string that must be acknowledged by the kernel
 *
 * @return
 *   -1 if the BPF program couldn't be loaded, fd (file descriptor) otherwise
 */
static int bpf_load(enum bpf_prog_type type,
		  const struct bpf_insn *insns,
		  size_t insns_cnt,
		  const char *license)
{
	union bpf_attr attr = {};

	bzero(&attr, sizeof(attr));
	attr.prog_type = type;
	attr.insn_cnt = (__u32)insns_cnt;
	attr.insns = ptr_to_u64(insns);
	attr.license = ptr_to_u64(license);
	attr.log_buf = ptr_to_u64(NULL);
	attr.log_level = 0;
	attr.kern_version = 0;

	return sys_bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
}

/**
 * Create BPF map for RSS rules
 *
 * @param[in] key_size
 *   map RSS key size
 *
 * @param[in] value_size
 *   Map RSS value size
 *
 * @param[in] max_entries
 *   Map max number of RSS entries (limit on max RSS rules)
 *
 * @return
 *   -1 if BPF map couldn't be created, map fd otherwise
 */
int tap_flow_bpf_rss_map_create(unsigned int key_size,
		unsigned int value_size,
		unsigned int max_entries)
{
	union bpf_attr attr = {};

	bzero(&attr, sizeof(attr));
	attr.map_type    = BPF_MAP_TYPE_HASH;
	attr.key_size    = key_size;
	attr.value_size  = value_size;
	attr.max_entries = max_entries;

	return sys_bpf(BPF_MAP_CREATE, &attr, sizeof(attr));
}

/**
 * Update RSS entry in BPF map
 *
 * @param[in] fd
 *   RSS map fd
 *
 * @param[in] key
 *   Pointer to RSS key whose entry is updated
 *
 * @param[in] value
 *   Pointer to RSS new updated value
 *
 * @return
 *   -1 if RSS entry failed to be updated, 0 otherwise
 */
int tap_flow_bpf_update_rss_elem(int fd, void *key, void *value)
{
	union bpf_attr attr = {};

	bzero(&attr, sizeof(attr));

	attr.map_type = BPF_MAP_TYPE_HASH;
	attr.map_fd = fd;
	attr.key = ptr_to_u64(key);
	attr.value = ptr_to_u64(value);
	attr.flags = BPF_ANY;

	return sys_bpf(BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr));
}
