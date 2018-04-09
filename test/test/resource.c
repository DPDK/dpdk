/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
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

#ifdef RTE_APP_TEST_RESOURCE_TAR
#include <archive.h>
#include <archive_entry.h>

static int do_copy(struct archive *r, struct archive *w)
{
	const void *buf;
	size_t len;
#if ARCHIVE_VERSION_NUMBER >= 3000000
	int64_t off;
#else
	off_t off;
#endif
	int ret;

	while (1) {
		ret = archive_read_data_block(r, &buf, &len, &off);
		if (ret == ARCHIVE_RETRY)
			continue;

		if (ret == ARCHIVE_EOF)
			return 0;

		if (ret != ARCHIVE_OK)
			return ret;

		do {
			ret = archive_write_data_block(w, buf, len, off);
			if (ret != ARCHIVE_OK && ret != ARCHIVE_RETRY)
				return ret;
		} while (ret != ARCHIVE_OK);
	}
}

int resource_untar(const struct resource *res)
{
	struct archive *r;
	struct archive *w;
	struct archive_entry *e;
	void *p;
	int flags = 0;
	int ret;

	p = malloc(resource_size(res));
	if (p == NULL)
		rte_panic("Failed to malloc %zu B\n", resource_size(res));

	memcpy(p, res->begin, resource_size(res));

	r = archive_read_new();
	if (r == NULL) {
		free(p);
		return -1;
	}

	archive_read_support_format_all(r);
	archive_read_support_filter_all(r);

	w = archive_write_disk_new();
	if (w == NULL) {
		archive_read_free(r);
		free(p);
		return -1;
	}

	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_FFLAGS;
	archive_write_disk_set_options(w, flags);
	archive_write_disk_set_standard_lookup(w);

	ret = archive_read_open_memory(r, p, resource_size(res));
	if (ret != ARCHIVE_OK)
		goto fail;

	while (1) {
		ret = archive_read_next_header(r, &e);
		if (ret == ARCHIVE_EOF)
			break;
		if (ret != ARCHIVE_OK)
			goto fail;

		ret = archive_write_header(w, e);
		if (ret == ARCHIVE_EOF)
			break;
		if (ret != ARCHIVE_OK)
			goto fail;

		if (archive_entry_size(e) == 0)
			continue;

		ret = do_copy(r, w);
		if (ret != ARCHIVE_OK)
			goto fail;

		ret = archive_write_finish_entry(w);
		if (ret != ARCHIVE_OK)
			goto fail;
	}

	archive_write_free(w);
	archive_read_free(r);
	free(p);
	return 0;

fail:
	archive_write_free(w);
	archive_read_free(r);
	free(p);
	rte_panic("Failed: %s\n", archive_error_string(r));
	return -1;
}

int resource_rm_by_tar(const struct resource *res)
{
	struct archive *r;
	struct archive_entry *e;
	void *p;
	int try_again = 1;
	int attempts = 0;
	int ret;

	p = malloc(resource_size(res));
	if (p == NULL)
		rte_panic("Failed to malloc %zu B\n", resource_size(res));

	memcpy(p, res->begin, resource_size(res));

	/*
	 * If somebody creates a file somewhere inside the extracted TAR
	 * hierarchy during a test the resource_rm_by_tar might loop
	 * infinitely. We prevent this by adding the attempts counter there.
	 * In normal case, max N iteration is done where N is the depth of
	 * the file-hierarchy.
	 */
	while (try_again && attempts < 10000) {
		r = archive_read_new();
		if (r == NULL) {
			free(p);
			return -1;
		}

		archive_read_support_format_all(r);
		archive_read_support_filter_all(r);

		ret = archive_read_open_memory(r, p, resource_size(res));
		if (ret != ARCHIVE_OK) {
			fprintf(stderr, "Failed: %s\n",
					archive_error_string(r));
			goto fail;
		}

		try_again = 0;

		while (1) {
			ret = archive_read_next_header(r, &e);
			if (ret == ARCHIVE_EOF)
				break;
			if (ret != ARCHIVE_OK)
				goto fail;

			ret = remove(archive_entry_pathname(e));
			if (ret < 0) {
				switch (errno) {
				case ENOTEMPTY:
				case EEXIST:
					try_again = 1;
					break;

				/* should not usually happen: */
				case ENOENT:
				case ENOTDIR:
				case EROFS:
					attempts += 1;
					continue;
				default:
					perror("Failed to remove file");
					goto fail;
				}
			}
		}

		archive_read_free(r);
		attempts += 1;
	}

	if (attempts >= 10000) {
		fprintf(stderr, "Failed to remove archive\n");
		free(p);
		return -1;
	}

	free(p);
	return 0;

fail:
	archive_read_free(r);
	free(p);

	rte_panic("Failed: %s\n", archive_error_string(r));
	return -1;
}

#endif /* RTE_APP_TEST_RESOURCE_TAR */

void resource_register(struct resource *r)
{
	TAILQ_INSERT_TAIL(&resource_list, r, next);
}
