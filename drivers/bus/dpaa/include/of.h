/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2010-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __OF_H
#define	__OF_H

#include <compat.h>

#ifndef OF_INIT_DEFAULT_PATH
#define OF_INIT_DEFAULT_PATH "/proc/device-tree"
#endif

#define OF_DEFAULT_NA 1
#define OF_DEFAULT_NS 1

#define OF_FILE_BUF_MAX 256

/**
 * Layout of Device Tree:
 * dt_dir
 *  |- dt_dir
 *  |   |- dt_dir
 *  |   |  |- dt_dir
 *  |   |  |  |- dt_file
 *  |   |  |  ``- dt_file
 *  |   |  ``- dt_file
 *  |   `-dt_file`
 *  ``- dt_file
 *
 *  +------------------+
 *  |dt_dir            |
 *  |+----------------+|
 *  ||dt_node         ||
 *  ||+--------------+||
 *  |||device_node   |||
 *  ||+--------------+||
 *  || list_dt_nodes  ||
 *  |+----------------+|
 *  | list of subdir   |
 *  | list of files    |
 *  +------------------+
 */

/**
 * Device description on of a device node in device tree.
 */
struct device_node {
	char name[NAME_MAX];
	char full_name[PATH_MAX];
};

/**
 * List of device nodes available in a device tree layout
 */
struct dt_node {
	struct device_node node; /**< Property of node */
	int is_file; /**< FALSE==dir, TRUE==file */
	struct list_head list; /**< Nodes within a parent subdir */
};

/**
 * Types we use to represent directories and files
 */
struct dt_file;
struct dt_dir {
	struct dt_node node;
	struct list_head subdirs;
	struct list_head files;
	struct list_head linear;
	struct dt_dir *parent;
	struct dt_file *compatible;
	struct dt_file *status;
	struct dt_file *lphandle;
	struct dt_file *a_cells;
	struct dt_file *s_cells;
	struct dt_file *reg;
};

struct dt_file {
	struct dt_node node;
	struct dt_dir *parent;
	ssize_t len;
	uint64_t buf[OF_FILE_BUF_MAX >> 3];
};

const struct device_node *of_find_compatible_node(
					const struct device_node *from,
					const char *type __always_unused,
					const char *compatible)
	__attribute__((nonnull(3)));

#define for_each_compatible_node(dev_node, type, compatible) \
	for (dev_node = of_find_compatible_node(NULL, type, compatible); \
		dev_node != NULL; \
		dev_node = of_find_compatible_node(dev_node, type, compatible))

const void *of_get_property(const struct device_node *from, const char *name,
			    size_t *lenp) __attribute__((nonnull(2)));
bool of_device_is_available(const struct device_node *dev_node);

const struct device_node *of_find_node_by_phandle(phandle ph);

const struct device_node *of_get_parent(const struct device_node *dev_node);

const struct device_node *of_get_next_child(const struct device_node *dev_node,
					    const struct device_node *prev);

#define for_each_child_node(parent, child) \
	for (child = of_get_next_child(parent, NULL); child != NULL; \
			child = of_get_next_child(parent, child))

uint32_t of_n_addr_cells(const struct device_node *dev_node);
uint32_t of_n_size_cells(const struct device_node *dev_node);

const uint32_t *of_get_address(const struct device_node *dev_node, size_t idx,
			       uint64_t *size, uint32_t *flags);

uint64_t of_translate_address(const struct device_node *dev_node,
			      const u32 *addr) __attribute__((nonnull));

bool of_device_is_compatible(const struct device_node *dev_node,
			     const char *compatible);

/* of_init() must be called prior to initialisation or use of any driver
 * subsystem that is device-tree-dependent. Eg. Qman/Bman, config layers, etc.
 * The path should usually be "/proc/device-tree".
 */
int of_init_path(const char *dt_path);

/* of_finish() allows a controlled tear-down of the device-tree layer, eg. if a
 * full reload is desired without a process exit.
 */
void of_finish(void);

/* Use of this wrapper is recommended. */
static inline int of_init(void)
{
	return of_init_path(OF_INIT_DEFAULT_PATH);
}

/* Read a numeric property according to its size and return it as a 64-bit
 * value.
 */
static inline uint64_t of_read_number(const __be32 *cell, int size)
{
	uint64_t r = 0;

	while (size--)
		r = (r << 32) | be32toh(*(cell++));
	return r;
}

#endif	/*  __OF_H */
