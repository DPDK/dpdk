/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Red Hat, Inc.
 */

#ifdef RTE_HAS_LIBARCHIVE
#include <archive.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>

#include "eal_firmware.h"

#ifdef RTE_HAS_LIBARCHIVE

struct firmware_read_ctx {
	struct archive *a;
};

static int
firmware_open(struct firmware_read_ctx *ctx, const char *name, size_t blocksize)
{
	struct archive_entry *e;

	ctx->a = archive_read_new();
	if (ctx->a == NULL)
		return -1;
	if (archive_read_support_format_raw(ctx->a) != ARCHIVE_OK ||
			archive_read_support_filter_xz(ctx->a) != ARCHIVE_OK ||
			archive_read_open_filename(ctx->a, name, blocksize) != ARCHIVE_OK ||
			archive_read_next_header(ctx->a, &e) != ARCHIVE_OK) {
		archive_read_free(ctx->a);
		ctx->a = NULL;
		return -1;
	}
	return 0;
}

static ssize_t
firmware_read_block(struct firmware_read_ctx *ctx, void *buf, size_t count)
{
	return archive_read_data(ctx->a, buf, count);
}

static void
firmware_close(struct firmware_read_ctx *ctx)
{
	archive_read_free(ctx->a);
	ctx->a = NULL;
}

#else /* !RTE_HAS_LIBARCHIVE */

struct firmware_read_ctx {
	int fd;
};

static int
firmware_open(struct firmware_read_ctx *ctx, const char *name,
	__rte_unused size_t blocksize)
{
	ctx->fd = open(name, O_RDONLY);
	if (ctx->fd < 0)
		return -1;
	return 0;
}

static ssize_t
firmware_read_block(struct firmware_read_ctx *ctx, void *buf, size_t count)
{
	return read(ctx->fd, buf, count);
}

static void
firmware_close(struct firmware_read_ctx *ctx)
{
	close(ctx->fd);
	ctx->fd = -1;
}

#endif /* !RTE_HAS_LIBARCHIVE */

static int
firmware_read(const char *name, void **buf, size_t *bufsz)
{
	const size_t blocksize = 4096;
	struct firmware_read_ctx ctx;
	int ret = -1;
	int err;

	*buf = NULL;
	*bufsz = 0;

	if (firmware_open(&ctx, name, blocksize) < 0)
		return -1;

	do {
		void *tmp;

		tmp = realloc(*buf, *bufsz + blocksize);
		if (tmp == NULL) {
			free(*buf);
			*buf = NULL;
			*bufsz = 0;
			goto out;
		}
		*buf = tmp;

		err = firmware_read_block(&ctx, RTE_PTR_ADD(*buf, *bufsz), blocksize);
		if (err < 0) {
			free(*buf);
			*buf = NULL;
			*bufsz = 0;
			goto out;
		}
		*bufsz += err;

	} while (err != 0);

	ret = 0;
out:
	firmware_close(&ctx);
	return ret;
}

int
rte_firmware_read(const char *name, void **buf, size_t *bufsz)
{
	char path[PATH_MAX];
	int ret;

	ret = firmware_read(name, buf, bufsz);
	if (ret < 0) {
		snprintf(path, sizeof(path), "%s.xz", name);
		path[PATH_MAX - 1] = '\0';
#ifndef RTE_HAS_LIBARCHIVE
		if (access(path, F_OK) == 0) {
			RTE_LOG(WARNING, EAL, "libarchive not linked, %s cannot be decompressed\n",
				path);
		}
#else
		ret = firmware_read(path, buf, bufsz);
#endif
	}
	return ret;
}
