/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "test_common.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int
ml_read_file(char *file, size_t *size, char **buffer)
{
	char *file_buffer = NULL;
	struct stat file_stat;
	char *file_map;
	int ret;
	int fd;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		ml_err("Failed to open file: %s\n", file);
		return -errno;
	}

	if (fstat(fd, &file_stat) != 0) {
		ml_err("fstat failed for file: %s\n", file);
		return -errno;
	}

	file_buffer = malloc(file_stat.st_size);
	if (file_buffer == NULL) {
		ml_err("Failed to allocate memory: %s\n", file);
		ret = -ENOMEM;
		goto error;
	}

	file_map = mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (file_map == MAP_FAILED) {
		ml_err("Failed to map file: %s\n", file);
		ret = -errno;
		goto error;
	}

	rte_memcpy(file_buffer, file_map, file_stat.st_size);
	munmap(file_map, file_stat.st_size);
	close(fd);

	*size = file_stat.st_size;
	*buffer = file_buffer;

	return 0;

error:
	free(file_buffer);
	close(fd);

	return ret;
}

bool
ml_test_cap_check(struct ml_options *opt)
{
	struct rte_ml_dev_info dev_info;

	rte_ml_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_models == 0) {
		ml_err("Not enough mldev models supported = %u", dev_info.max_models);
		return false;
	}

	return true;
}

int
ml_test_opt_check(struct ml_options *opt)
{
	uint16_t dev_count;
	int socket_id;

	RTE_SET_USED(opt);

	dev_count = rte_ml_dev_count();
	if (dev_count == 0) {
		ml_err("No ML devices found");
		return -ENODEV;
	}

	if ((opt->dev_id >= dev_count) || (opt->dev_id < 0)) {
		ml_err("Invalid option, dev_id = %d", opt->dev_id);
		return -EINVAL;
	}

	socket_id = rte_ml_dev_socket_id(opt->dev_id);
	if ((opt->socket_id != SOCKET_ID_ANY) && (opt->socket_id != socket_id)) {
		ml_err("Invalid option, socket_id = %d\n", opt->socket_id);
		return -EINVAL;
	}

	return 0;
}

void
ml_test_opt_dump(struct ml_options *opt)
{
	ml_options_dump(opt);
}

int
ml_test_device_configure(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	struct rte_ml_dev_config dev_config;
	int ret;

	ret = rte_ml_dev_info_get(opt->dev_id, &t->dev_info);
	if (ret != 0) {
		ml_err("Failed to get mldev info, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	/* configure device */
	dev_config.socket_id = opt->socket_id;
	dev_config.nb_models = t->dev_info.max_models;
	dev_config.nb_queue_pairs = opt->queue_pairs;
	ret = rte_ml_dev_configure(opt->dev_id, &dev_config);
	if (ret != 0) {
		ml_err("Failed to configure ml device, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	return 0;
}

int
ml_test_device_close(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret = 0;

	RTE_SET_USED(t);

	/* close device */
	ret = rte_ml_dev_close(opt->dev_id);
	if (ret != 0)
		ml_err("Failed to close ML device, dev_id = %d\n", opt->dev_id);

	return ret;
}

int
ml_test_device_start(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	/* start device */
	ret = rte_ml_dev_start(opt->dev_id);
	if (ret != 0) {
		ml_err("Failed to start ml device, dev_id = %d\n", opt->dev_id);
		return ret;
	}

	return 0;
}

int
ml_test_device_stop(struct ml_test *test, struct ml_options *opt)
{
	struct test_common *t = ml_test_priv(test);
	int ret = 0;

	RTE_SET_USED(t);

	/* stop device */
	ret = rte_ml_dev_stop(opt->dev_id);
	if (ret != 0)
		ml_err("Failed to stop ML device, dev_id = %d\n", opt->dev_id);

	return ret;
}
