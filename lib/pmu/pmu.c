/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <eal_export.h>
#include <rte_bitops.h>
#include <rte_tailq.h>
#include <rte_log.h>

#include "rte_pmu.h"
#include "pmu_private.h"

#define EVENT_SOURCE_DEVICES_PATH "/sys/bus/event_source/devices"

#define FIELD_PREP(m, v) (((uint64_t)(v) << (rte_ffs64(m) - 1)) & (m))

RTE_LOG_REGISTER_DEFAULT(rte_pmu_logtype, INFO)
#define RTE_LOGTYPE_PMU rte_pmu_logtype

#define PMU_LOG(level, ...) \
	RTE_LOG_LINE(level, PMU, ## __VA_ARGS__)

/* A structure describing an event */
struct rte_pmu_event {
	char *name;
	unsigned int index;
	TAILQ_ENTRY(rte_pmu_event) next;
};

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmu)
struct rte_pmu rte_pmu;

/* Stubs for arch-specific functions */
#if !defined(RTE_PMU_SUPPORTED) || defined(RTE_ARCH_X86_64)
int
pmu_arch_init(void)
{
	return 0;
}
void
pmu_arch_fini(void)
{
}
void
pmu_arch_fixup_config(uint64_t __rte_unused config[3])
{
}
#endif

static int
get_term_format(const char *name, int *num, uint64_t *mask)
{
	char path[PATH_MAX];
	char *config = NULL;
	int high, low, ret;
	FILE *fp;

	*num = *mask = 0;
	snprintf(path, sizeof(path), EVENT_SOURCE_DEVICES_PATH "/%s/format/%s", rte_pmu.name, name);
	fp = fopen(path, "r");
	if (fp == NULL)
		return -errno;

	errno = 0;
	ret = fscanf(fp, "%m[^:]:%d-%d", &config, &low, &high);
	if (ret < 2) {
		ret = -ENODATA;
		goto out;
	}
	if (errno) {
		ret = -errno;
		goto out;
	}

	if (ret == 2)
		high = low;

	*mask = RTE_GENMASK64(high, low);
	/* Last digit should be [012]. If last digit is missing 0 is implied. */
	*num = config[strlen(config) - 1];
	*num = isdigit(*num) ? *num - '0' : 0;

	ret = 0;
out:
	free(config);
	fclose(fp);

	return ret;
}

static int
parse_event(char *buf, uint64_t config[3])
{
	char *token, *term;
	int num, ret, val;
	uint64_t mask;
	char *tmp;

	config[0] = config[1] = config[2] = 0;

	token = strtok_r(buf, ",", &tmp);
	while (token) {
		errno = 0;
		/* <term>=<value> */
		ret = sscanf(token, "%m[^=]=%i", &term, &val);
		if (ret < 1)
			return -ENODATA;
		if (errno)
			return -errno;
		if (ret == 1)
			val = 1;

		ret = get_term_format(term, &num, &mask);
		free(term);
		if (ret)
			return ret;

		config[num] |= FIELD_PREP(mask, val);
		token = strtok_r(NULL, ",", &tmp);
	}

	return 0;
}

static int
get_event_config(const char *name, uint64_t config[3])
{
	char path[PATH_MAX], buf[BUFSIZ];
	FILE *fp;
	int ret;

	snprintf(path, sizeof(path), EVENT_SOURCE_DEVICES_PATH "/%s/events/%s", rte_pmu.name, name);
	fp = fopen(path, "r");
	if (fp == NULL)
		return -errno;

	ret = fread(buf, 1, sizeof(buf), fp);
	if (ret == 0) {
		fclose(fp);

		return -EINVAL;
	}
	fclose(fp);
	buf[ret] = '\0';

	return parse_event(buf, config);
}

static int
do_perf_event_open(uint64_t config[3], int group_fd)
{
	struct perf_event_attr attr = {
		.size = sizeof(struct perf_event_attr),
		.type = PERF_TYPE_RAW,
		.exclude_kernel = 1,
		.exclude_hv = 1,
		.disabled = 1,
		.pinned = group_fd == -1,
	};

	pmu_arch_fixup_config(config);

	attr.config = config[0];
	attr.config1 = config[1];
	attr.config2 = config[2];

	return syscall(SYS_perf_event_open, &attr, 0, -1, group_fd, 0);
}

static int
open_events(struct rte_pmu_event_group *group)
{
	struct rte_pmu_event *event;
	uint64_t config[3];
	int num = 0, ret;

	/* group leader gets created first, with fd = -1 */
	group->fds[0] = -1;

	TAILQ_FOREACH(event, &rte_pmu.event_list, next) {
		ret = get_event_config(event->name, config);
		if (ret)
			continue;

		ret = do_perf_event_open(config, group->fds[0]);
		if (ret == -1) {
			ret = -errno;
			goto out;
		}

		group->fds[event->index] = ret;
		num++;
	}

	return 0;
out:
	for (--num; num >= 0; num--) {
		close(group->fds[num]);
		group->fds[num] = -1;
	}

	return ret;
}

/* Inspired by rte_mem_page_size() from /lib/eal/unix/eal_unix_memory.c */
static size_t
mem_page_size(void)
{
	static size_t page_size;

	if (unlikely(page_size == 0)) {
		/*
		 * When the sysconf value cannot be determined, sysconf()
		 * returns -1 without setting errno.
		 * To distinguish an indeterminate value from an error,
		 * clear errno before calling sysconf(), and check whether
		 * errno has been set if sysconf() returns -1.
		 */
		errno = 0;
		page_size = sysconf(_SC_PAGESIZE);
		if ((ssize_t)page_size < 0 && errno == 0)
			errno = ENOENT;
	}

	return page_size;
}

static int
mmap_events(struct rte_pmu_event_group *group)
{
	size_t page_size = mem_page_size();
	unsigned int i;
	void *addr;
	int ret;

	if ((ssize_t)page_size < 0)
		return -errno;

	for (i = 0; i < rte_pmu.num_group_events; i++) {
		addr = mmap(0, page_size, PROT_READ, MAP_SHARED, group->fds[i], 0);
		if (addr == MAP_FAILED) {
			ret = -errno;
			goto out;
		}

		group->mmap_pages[i] = addr;
	}

	return 0;
out:
	for (; i; i--) {
		munmap(group->mmap_pages[i - 1], page_size);
		group->mmap_pages[i - 1] = NULL;
	}

	return ret;
}

static void
cleanup_events(struct rte_pmu_event_group *group)
{
	size_t page_size = mem_page_size();
	unsigned int i;

	if (group->fds[0] != -1)
		ioctl(group->fds[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

	for (i = 0; i < rte_pmu.num_group_events; i++) {
		if (group->mmap_pages[i]) {
			__rte_assume((ssize_t)page_size >= 0);
			munmap(group->mmap_pages[i], page_size);
			group->mmap_pages[i] = NULL;
		}

		if (group->fds[i] != -1) {
			close(group->fds[i]);
			group->fds[i] = -1;
		}
	}

	group->enabled = false;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_pmu_enable_group, 25.07)
int
__rte_pmu_enable_group(struct rte_pmu_event_group *group)
{
	int ret;

	if (rte_pmu.num_group_events == 0)
		return -ENODEV;

	ret = open_events(group);
	if (ret)
		goto out;

	ret = mmap_events(group);
	if (ret)
		goto out;

	if (ioctl(group->fds[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1) {
		ret = -errno;
		goto out;
	}

	if (ioctl(group->fds[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
		ret = -errno;
		goto out;
	}

	group->enabled = true;

	return 0;
out:
	cleanup_events(group);

	return ret;
}

static int
scan_pmus(void)
{
	char path[PATH_MAX];
	struct dirent *dent;
	const char *name;
	DIR *dirp;

	dirp = opendir(EVENT_SOURCE_DEVICES_PATH);
	if (dirp == NULL)
		return -errno;

	while ((dent = readdir(dirp))) {
		name = dent->d_name;
		if (name[0] == '.')
			continue;

		/* sysfs entry should either contain cpus or be a cpu */
		if (!strcmp(name, "cpu"))
			break;

		snprintf(path, sizeof(path), EVENT_SOURCE_DEVICES_PATH "/%s/cpus", name);
		if (access(path, F_OK) == 0)
			break;
	}

	if (dent) {
		rte_pmu.name = strdup(name);
		if (rte_pmu.name == NULL) {
			closedir(dirp);

			return -ENOMEM;
		}
	}

	closedir(dirp);

	return rte_pmu.name ? 0 : -ENODEV;
}

static struct rte_pmu_event *
new_event(const char *name)
{
	struct rte_pmu_event *event;

	event = calloc(1, sizeof(*event));
	if (event == NULL)
		goto out;

	event->name = strdup(name);
	if (event->name == NULL) {
		free(event);
		event = NULL;
	}

out:
	return event;
}

static void
free_event(struct rte_pmu_event *event)
{
	free(event->name);
	free(event);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmu_add_event, 25.07)
int
rte_pmu_add_event(const char *name)
{
	struct rte_pmu_event *event;
	char path[PATH_MAX];

	if (!rte_pmu.initialized) {
		PMU_LOG(ERR, "PMU is not initialized");
		return -ENODEV;
	}

	if (rte_pmu.num_group_events + 1 >= RTE_MAX_NUM_GROUP_EVENTS) {
		PMU_LOG(ERR, "Excessive number of events in a group (%d > %d)",
			rte_pmu.num_group_events, RTE_MAX_NUM_GROUP_EVENTS);
		return -ENOSPC;
	}

	snprintf(path, sizeof(path), EVENT_SOURCE_DEVICES_PATH "/%s/events/%s", rte_pmu.name, name);
	if (access(path, R_OK)) {
		PMU_LOG(ERR, "Cannot access %s", path);
		return -ENODEV;
	}

	TAILQ_FOREACH(event, &rte_pmu.event_list, next) {
		if (strcmp(event->name, name))
			continue;

		return event->index;
	}

	event = new_event(name);
	if (event == NULL) {
		PMU_LOG(ERR, "Failed to create event %s", name);
		return -ENOMEM;
	}

	event->index = rte_pmu.num_group_events++;
	TAILQ_INSERT_TAIL(&rte_pmu.event_list, event, next);

	return event->index;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmu_init, 25.07)
int
rte_pmu_init(void)
{
	int ret;

	if (rte_pmu.initialized)
		return 0;

	ret = scan_pmus();
	if (ret) {
		PMU_LOG(ERR, "Failed to scan for event sources");
		goto out;
	}

	ret = pmu_arch_init();
	if (ret) {
		PMU_LOG(ERR, "Failed to setup arch internals");
		goto out;
	}

	TAILQ_INIT(&rte_pmu.event_list);
	rte_pmu.initialized = 1;
out:
	if (ret) {
		free(rte_pmu.name);
		rte_pmu.name = NULL;
	}

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmu_fini, 25.07)
void
rte_pmu_fini(void)
{
	struct rte_pmu_event *event, *tmp_event;
	struct rte_pmu_event_group *group;
	unsigned int i;

	if (!rte_pmu.initialized)
		return;

	RTE_TAILQ_FOREACH_SAFE(event, &rte_pmu.event_list, next, tmp_event) {
		TAILQ_REMOVE(&rte_pmu.event_list, event, next);
		free_event(event);
	}

	for (i = 0; i < RTE_DIM(rte_pmu.event_groups); i++) {
		group = &rte_pmu.event_groups[i];
		if (!group->enabled)
			continue;

		cleanup_events(group);
	}

	pmu_arch_fini();
	free(rte_pmu.name);
	rte_pmu.name = NULL;
	rte_pmu.num_group_events = 0;
}
