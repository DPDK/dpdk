/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 RehiveTech. All rights reserved.
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
 *     * Neither the name of RehiveTech nor the names of its
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

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

/**
 * @file
 *
 * Test Resource API
 *
 * Each test can require and use some external resources. Usually, an external
 * resource is a file or a filesystem sub-hierarchy. A resource is included
 * inside the test executable.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <stddef.h>

#include <rte_eal.h>
#include <rte_common.h>

TAILQ_HEAD(resource_list, resource);
extern struct resource_list resource_list;

/**
 * Representation of a resource. It points to the resource's binary data.
 * The semantics of the binary data are defined by the target test.
 */
struct resource {
	const char *name;  /**< Unique name of the resource */
	const char *begin; /**< Start of resource data */
	const char *end;   /**< End of resource data */
	TAILQ_ENTRY(resource) next;
};

/**
 * @return size of the given resource
 */
size_t resource_size(const struct resource *r);

/**
 * Find a resource by name in the global list of resources.
 */
const struct resource *resource_find(const char *name);

/**
 * Write the raw data of the resource to the given file.
 * @return 0 on success
 */
int resource_fwrite(const struct resource *r, FILE *f);

/**
 * Write the raw data of the resource to the given file given by name.
 * The name is relative to the current working directory.
 * @return 0 on success
 */
int resource_fwrite_file(const struct resource *r, const char *fname);

/**
 * Treat the given resource as a tar archive. Extract
 * the archive to the current directory.
 */
int resource_untar(const struct resource *res);

/**
 * Treat the given resource as a tar archive. Remove
 * all files (related to the current directory) listed
 * in the tar archive.
 */
int resource_rm_by_tar(const struct resource *res);

/**
 * Register a resource in the global list of resources.
 * Not intended for direct use, please check the REGISTER_RESOURCE
 * macro.
 */
void resource_register(struct resource *r);

/**
 * Definition of a resource linked externally (by means of the used toolchain).
 * Only the base name of the resource is expected. The name refers to the
 * linked pointers beg_<name> and end_<name> provided externally.
 */
#define REGISTER_LINKED_RESOURCE(n) \
extern const char beg_ ##n;         \
extern const char end_ ##n;         \
REGISTER_RESOURCE(n, &beg_ ##n, &end_ ##n) \

/**
 * Definition of a resource described by its name, and pointers begin, end.
 */
#define REGISTER_RESOURCE(n, b, e) \
static struct resource linkres_ ##n = {       \
	.name = RTE_STR(n),     \
	.begin = b,             \
	.end = e,               \
};                              \
static void __attribute__((constructor, used)) resinitfn_ ##n(void) \
{                               \
	resource_register(&linkres_ ##n);  \
}

#endif
