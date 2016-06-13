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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_debug.h>

#include "resource.h"

struct resource_list resource_list = TAILQ_HEAD_INITIALIZER(resource_list);

size_t resource_size(const struct resource *r)
{
	return r->end - r->begin;
}

const struct resource *resource_find(const char *name)
{
	struct resource *r;

	TAILQ_FOREACH(r, &resource_list, next) {
		RTE_VERIFY(r->name);

		if (!strcmp(r->name, name))
			return r;
	}

	return NULL;
}

int resource_fwrite(const struct resource *r, FILE *f)
{
	const size_t goal = resource_size(r);
	size_t total = 0;

	while (total < goal) {
		size_t wlen = fwrite(r->begin + total, 1, goal - total, f);
		if (wlen == 0) {
			perror(__func__);
			return -1;
		}

		total += wlen;
	}

	return 0;
}

int resource_fwrite_file(const struct resource *r, const char *fname)
{
	FILE *f;
	int ret;

	f = fopen(fname, "w");
	if (f == NULL) {
		perror(__func__);
		return -1;
	}

	ret = resource_fwrite(r, f);
	fclose(f);
	return ret;
}

void resource_register(struct resource *r)
{
	TAILQ_INSERT_TAIL(&resource_list, r, next);
}
