/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation.
 * Copyright(c) 2012-2014 6WIND S.A.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/file.h>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if defined(RTE_ARCH_X86)
#include <sys/io.h>
#endif
#include <linux/version.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_service_component.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_bus.h>
#include <rte_version.h>
#include <malloc_heap.h>
#include <rte_vfio.h>

#include <telemetry_internal.h>
#include "eal_private.h"
#include "eal_thread.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_hugepages.h"
#include "eal_memcfg.h"
#include "eal_trace.h"
#include "eal_options.h"
#include "eal_vfio.h"
#include "hotplug_mp.h"
#include "log_internal.h"

#define MEMSIZE_IF_NO_HUGE_PAGE (64ULL * 1024ULL * 1024ULL)

#define SOCKET_MEM_STRLEN (RTE_MAX_NUMA_NODES * 10)

#define KERNEL_IOMMU_GROUPS_PATH "/sys/kernel/iommu_groups"

/* define fd variable here, because file needs to be kept open for the
 * duration of the program, as we hold a write lock on it in the primary proc */
static int mem_cfg_fd = -1;

static struct flock wr_lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = offsetof(struct rte_mem_config, memsegs),
		.l_len = RTE_SIZEOF_FIELD(struct rte_mem_config, memsegs),
};

/* internal configuration (per-core) */
struct lcore_config lcore_config[RTE_MAX_LCORE];

/* used by rte_rdtsc() */
int rte_cycles_vmware_tsc_map;


int
eal_clean_runtime_dir(void)
{
	const char *runtime_dir = rte_eal_get_runtime_dir();
	DIR *dir;
	struct dirent *dirent;
	int dir_fd, fd, lck_result;
	static const char * const filters[] = {
		"fbarray_*",
		"mp_socket_*"
	};

	/* open directory */
	dir = opendir(runtime_dir);
	if (!dir) {
		RTE_LOG(ERR, EAL, "Unable to open runtime directory %s\n",
				runtime_dir);
		goto error;
	}
	dir_fd = dirfd(dir);

	/* lock the directory before doing anything, to avoid races */
	if (flock(dir_fd, LOCK_EX) < 0) {
		RTE_LOG(ERR, EAL, "Unable to lock runtime directory %s\n",
			runtime_dir);
		goto error;
	}

	dirent = readdir(dir);
	if (!dirent) {
		RTE_LOG(ERR, EAL, "Unable to read runtime directory %s\n",
				runtime_dir);
		goto error;
	}

	while (dirent != NULL) {
		unsigned int f_idx;
		bool skip = true;

		/* skip files that don't match the patterns */
		for (f_idx = 0; f_idx < RTE_DIM(filters); f_idx++) {
			const char *filter = filters[f_idx];

			if (fnmatch(filter, dirent->d_name, 0) == 0) {
				skip = false;
				break;
			}
		}
		if (skip) {
			dirent = readdir(dir);
			continue;
		}

		/* try and lock the file */
		fd = openat(dir_fd, dirent->d_name, O_RDONLY);

		/* skip to next file */
		if (fd == -1) {
			dirent = readdir(dir);
			continue;
		}

		/* non-blocking lock */
		lck_result = flock(fd, LOCK_EX | LOCK_NB);

		/* if lock succeeds, remove the file */
		if (lck_result != -1)
			unlinkat(dir_fd, dirent->d_name, 0);
		close(fd);
		dirent = readdir(dir);
	}

	/* closedir closes dir_fd and drops the lock */
	closedir(dir);
	return 0;

error:
	if (dir)
		closedir(dir);

	RTE_LOG(ERR, EAL, "Error while clearing runtime dir: %s\n",
		strerror(errno));

	return -1;
}


/* create memory configuration in shared/mmap memory. Take out
 * a write lock on the memsegs, so we can auto-detect primary/secondary.
 * This means we never close the file while running (auto-close on exit).
 * We also don't lock the whole file, so that in future we can use read-locks
 * on other parts, e.g. memzones, to detect if there are running secondary
 * processes. */
static int
rte_eal_config_create(void)
{
	struct rte_config *config = rte_eal_get_configuration();
	size_t page_sz = sysconf(_SC_PAGE_SIZE);
	size_t cfg_len = sizeof(*config->mem_config);
	size_t cfg_len_aligned = RTE_ALIGN(cfg_len, page_sz);
	void *rte_mem_cfg_addr, *mapped_mem_cfg_addr;
	int retval;
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	const char *pathname = eal_runtime_config_path();

	if (internal_conf->no_shconf)
		return 0;

	/* map the config before hugepage address so that we don't waste a page */
	if (internal_conf->base_virtaddr != 0)
		rte_mem_cfg_addr = (void *)
			RTE_ALIGN_FLOOR(internal_conf->base_virtaddr -
			sizeof(struct rte_mem_config), page_sz);
	else
		rte_mem_cfg_addr = NULL;

	if (mem_cfg_fd < 0){
		mem_cfg_fd = open(pathname, O_RDWR | O_CREAT, 0600);
		if (mem_cfg_fd < 0) {
			RTE_LOG(ERR, EAL, "Cannot open '%s' for rte_mem_config\n",
				pathname);
			return -1;
		}
	}

	retval = ftruncate(mem_cfg_fd, cfg_len);
	if (retval < 0){
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		RTE_LOG(ERR, EAL, "Cannot resize '%s' for rte_mem_config\n",
			pathname);
		return -1;
	}

	retval = fcntl(mem_cfg_fd, F_SETLK, &wr_lock);
	if (retval < 0){
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		RTE_LOG(ERR, EAL, "Cannot create lock on '%s'. Is another primary "
			"process running?\n", pathname);
		return -1;
	}

	/* reserve space for config */
	rte_mem_cfg_addr = eal_get_virtual_area(rte_mem_cfg_addr,
			&cfg_len_aligned, page_sz, 0, 0);
	if (rte_mem_cfg_addr == NULL) {
		RTE_LOG(ERR, EAL, "Cannot mmap memory for rte_config\n");
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		return -1;
	}

	/* remap the actual file into the space we've just reserved */
	mapped_mem_cfg_addr = mmap(rte_mem_cfg_addr,
			cfg_len_aligned, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, mem_cfg_fd, 0);
	if (mapped_mem_cfg_addr == MAP_FAILED) {
		munmap(rte_mem_cfg_addr, cfg_len);
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		RTE_LOG(ERR, EAL, "Cannot remap memory for rte_config\n");
		return -1;
	}

	memcpy(rte_mem_cfg_addr, config->mem_config, sizeof(struct rte_mem_config));
	config->mem_config = rte_mem_cfg_addr;

	/* store address of the config in the config itself so that secondary
	 * processes could later map the config into this exact location
	 */
	config->mem_config->mem_cfg_addr = (uintptr_t) rte_mem_cfg_addr;
	config->mem_config->dma_maskbits = 0;

	return 0;
}

/* attach to an existing shared memory config */
static int
rte_eal_config_attach(void)
{
	struct rte_config *config = rte_eal_get_configuration();
	struct rte_mem_config *mem_config;
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	const char *pathname = eal_runtime_config_path();

	if (internal_conf->no_shconf)
		return 0;

	if (mem_cfg_fd < 0){
		mem_cfg_fd = open(pathname, O_RDWR);
		if (mem_cfg_fd < 0) {
			RTE_LOG(ERR, EAL, "Cannot open '%s' for rte_mem_config\n",
				pathname);
			return -1;
		}
	}

	/* map it as read-only first */
	mem_config = (struct rte_mem_config *) mmap(NULL, sizeof(*mem_config),
			PROT_READ, MAP_SHARED, mem_cfg_fd, 0);
	if (mem_config == MAP_FAILED) {
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		RTE_LOG(ERR, EAL, "Cannot mmap memory for rte_config! error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	config->mem_config = mem_config;

	return 0;
}

/* reattach the shared config at exact memory location primary process has it */
static int
rte_eal_config_reattach(void)
{
	struct rte_config *config = rte_eal_get_configuration();
	struct rte_mem_config *mem_config;
	void *rte_mem_cfg_addr;
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (internal_conf->no_shconf)
		return 0;

	/* save the address primary process has mapped shared config to */
	rte_mem_cfg_addr =
		(void *) (uintptr_t) config->mem_config->mem_cfg_addr;

	/* unmap original config */
	munmap(config->mem_config, sizeof(struct rte_mem_config));

	/* remap the config at proper address */
	mem_config = (struct rte_mem_config *) mmap(rte_mem_cfg_addr,
			sizeof(*mem_config), PROT_READ | PROT_WRITE, MAP_SHARED,
			mem_cfg_fd, 0);

	close(mem_cfg_fd);
	mem_cfg_fd = -1;

	if (mem_config == MAP_FAILED || mem_config != rte_mem_cfg_addr) {
		if (mem_config != MAP_FAILED) {
			/* errno is stale, don't use */
			RTE_LOG(ERR, EAL, "Cannot mmap memory for rte_config at [%p], got [%p]"
				" - please use '--" OPT_BASE_VIRTADDR
				"' option\n", rte_mem_cfg_addr, mem_config);
			munmap(mem_config, sizeof(struct rte_mem_config));
			return -1;
		}
		RTE_LOG(ERR, EAL, "Cannot mmap memory for rte_config! error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	config->mem_config = mem_config;

	return 0;
}

/* Detect if we are a primary or a secondary process */
enum rte_proc_type_t
eal_proc_type_detect(void)
{
	enum rte_proc_type_t ptype = RTE_PROC_PRIMARY;
	const char *pathname = eal_runtime_config_path();
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* if there no shared config, there can be no secondary processes */
	if (!internal_conf->no_shconf) {
		/* if we can open the file but not get a write-lock we are a
		 * secondary process. NOTE: if we get a file handle back, we
		 * keep that open and don't close it to prevent a race condition
		 * between multiple opens.
		 */
		if (((mem_cfg_fd = open(pathname, O_RDWR)) >= 0) &&
				(fcntl(mem_cfg_fd, F_SETLK, &wr_lock) < 0))
			ptype = RTE_PROC_SECONDARY;
	}

	RTE_LOG(INFO, EAL, "Auto-detected process type: %s\n",
			ptype == RTE_PROC_PRIMARY ? "PRIMARY" : "SECONDARY");

	return ptype;
}

/* Sets up rte_config structure with the pointer to shared memory config.*/
static int
rte_config_init(void)
{
	struct rte_config *config = rte_eal_get_configuration();
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	config->process_type = internal_conf->process_type;

	switch (config->process_type) {
	case RTE_PROC_PRIMARY:
		if (rte_eal_config_create() < 0)
			return -1;
		eal_mcfg_update_from_internal();
		break;
	case RTE_PROC_SECONDARY:
		if (rte_eal_config_attach() < 0)
			return -1;
		eal_mcfg_wait_complete();
		if (eal_mcfg_check_version() < 0) {
			RTE_LOG(ERR, EAL, "Primary and secondary process DPDK version mismatch\n");
			return -1;
		}
		if (rte_eal_config_reattach() < 0)
			return -1;
		if (!__rte_mp_enable()) {
			RTE_LOG(ERR, EAL, "Primary process refused secondary attachment\n");
			return -1;
		}
		eal_mcfg_update_internal();
		break;
	case RTE_PROC_AUTO:
	case RTE_PROC_INVALID:
		RTE_LOG(ERR, EAL, "Invalid process type %d\n",
			config->process_type);
		return -1;
	}

	return 0;
}

/* Unlocks hugepage directories that were locked by eal_hugepage_info_init */
static void
eal_hugedirs_unlock(void)
{
	int i;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++)
	{
		/* skip uninitialized */
		if (internal_conf->hugepage_info[i].lock_descriptor < 0)
			continue;
		/* unlock hugepage file */
		flock(internal_conf->hugepage_info[i].lock_descriptor, LOCK_UN);
		close(internal_conf->hugepage_info[i].lock_descriptor);
		/* reset the field */
		internal_conf->hugepage_info[i].lock_descriptor = -1;
	}
}

/* display usage */
static void
eal_usage(const char *prgname)
{
	rte_usage_hook_t hook = eal_get_application_usage_hook();

	printf("\nUsage: %s ", prgname);
	eal_common_usage();
	printf("EAL Linux options:\n"
	       "  --"OPT_SOCKET_MEM"        Memory to allocate on sockets (comma separated values)\n"
	       "  --"OPT_SOCKET_LIMIT"      Limit memory allocation on sockets (comma separated values)\n"
	       "  --"OPT_HUGE_DIR"          Directory where hugetlbfs is mounted\n"
	       "  --"OPT_FILE_PREFIX"       Prefix for hugepage filenames\n"
	       "  --"OPT_CREATE_UIO_DEV"    Create /dev/uioX (usually done by hotplug)\n"
	       "  --"OPT_VFIO_INTR"         Interrupt mode for VFIO (legacy|msi|msix)\n"
	       "  --"OPT_VFIO_VF_TOKEN"     VF token (UUID) shared between SR-IOV PF and VFs\n"
	       "  --"OPT_LEGACY_MEM"        Legacy memory mode (no dynamic allocation, contiguous segments)\n"
	       "  --"OPT_SINGLE_FILE_SEGMENTS" Put all hugepage memory in single files\n"
	       "  --"OPT_MATCH_ALLOCATIONS" Free hugepages exactly as allocated\n"
	       "  --"OPT_HUGE_WORKER_STACK"[=size]\n"
	       "                      Allocate worker thread stacks from hugepage memory.\n"
	       "                      Size is in units of kbytes and defaults to system\n"
	       "                      thread stack size if not specified.\n"
	       "\n");
	/* Allow the application to print its usage message too if hook is set */
	if (hook) {
		printf("===== Application Usage =====\n\n");
		(hook)(prgname);
	}
}

static int
eal_parse_socket_arg(char *strval, volatile uint64_t *socket_arg)
{
	char * arg[RTE_MAX_NUMA_NODES];
	char *end;
	int arg_num, i, len;

	len = strnlen(strval, SOCKET_MEM_STRLEN);
	if (len == SOCKET_MEM_STRLEN) {
		RTE_LOG(ERR, EAL, "--socket-mem is too long\n");
		return -1;
	}

	/* all other error cases will be caught later */
	if (!isdigit(strval[len-1]))
		return -1;

	/* split the optarg into separate socket values */
	arg_num = rte_strsplit(strval, len,
			arg, RTE_MAX_NUMA_NODES, ',');

	/* if split failed, or 0 arguments */
	if (arg_num <= 0)
		return -1;

	/* parse each defined socket option */
	errno = 0;
	for (i = 0; i < arg_num; i++) {
		uint64_t val;
		end = NULL;
		val = strtoull(arg[i], &end, 10);

		/* check for invalid input */
		if ((errno != 0)  ||
				(arg[i][0] == '\0') || (end == NULL) || (*end != '\0'))
			return -1;
		val <<= 20;
		socket_arg[i] = val;
	}

	return 0;
}

static int
eal_parse_vfio_intr(const char *mode)
{
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	unsigned i;
	static struct {
		const char *name;
		enum rte_intr_mode value;
	} map[] = {
		{ "legacy", RTE_INTR_MODE_LEGACY },
		{ "msi", RTE_INTR_MODE_MSI },
		{ "msix", RTE_INTR_MODE_MSIX },
	};

	for (i = 0; i < RTE_DIM(map); i++) {
		if (!strcmp(mode, map[i].name)) {
			internal_conf->vfio_intr_mode = map[i].value;
			return 0;
		}
	}
	return -1;
}

static int
eal_parse_vfio_vf_token(const char *vf_token)
{
	struct internal_config *cfg = eal_get_internal_configuration();
	rte_uuid_t uuid;

	if (!rte_uuid_parse(vf_token, uuid)) {
		rte_uuid_copy(cfg->vfio_vf_token, uuid);
		return 0;
	}

	return -1;
}

/* Parse the arguments for --log-level only */
static void
eal_log_level_parse(int argc, char **argv)
{
	int opt;
	char **argvopt;
	int option_index;
	const int old_optind = optind;
	const int old_optopt = optopt;
	char * const old_optarg = optarg;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	argvopt = argv;
	optind = 1;

	while ((opt = getopt_long(argc, argvopt, eal_short_options,
				  eal_long_options, &option_index)) != EOF) {

		int ret;

		/* getopt is not happy, stop right now */
		if (opt == '?')
			break;

		ret = (opt == OPT_LOG_LEVEL_NUM) ?
			eal_parse_common_option(opt, optarg, internal_conf) : 0;

		/* common parser is not happy */
		if (ret < 0)
			break;
	}

	/* restore getopt lib */
	optind = old_optind;
	optopt = old_optopt;
	optarg = old_optarg;
}

static int
eal_parse_huge_worker_stack(const char *arg)
{
	struct internal_config *cfg = eal_get_internal_configuration();

	if (arg == NULL || arg[0] == '\0') {
		pthread_attr_t attr;
		int ret;

		if (pthread_attr_init(&attr) != 0) {
			RTE_LOG(ERR, EAL, "Could not retrieve default stack size\n");
			return -1;
		}
		ret = pthread_attr_getstacksize(&attr, &cfg->huge_worker_stack_size);
		pthread_attr_destroy(&attr);
		if (ret != 0) {
			RTE_LOG(ERR, EAL, "Could not retrieve default stack size\n");
			return -1;
		}
	} else {
		unsigned long stack_size;
		char *end;

		errno = 0;
		stack_size = strtoul(arg, &end, 10);
		if (errno || end == NULL || stack_size == 0 ||
				stack_size >= (size_t)-1 / 1024)
			return -1;

		cfg->huge_worker_stack_size = stack_size * 1024;
	}

	RTE_LOG(DEBUG, EAL, "Each worker thread will use %zu kB of DPDK memory as stack\n",
		cfg->huge_worker_stack_size / 1024);
	return 0;
}

/* Parse the argument given in the command line of the application */
static int
eal_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	const int old_optind = optind;
	const int old_optopt = optopt;
	char * const old_optarg = optarg;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	argvopt = argv;
	optind = 1;

	while ((opt = getopt_long(argc, argvopt, eal_short_options,
				  eal_long_options, &option_index)) != EOF) {

		/* getopt didn't recognise the option */
		if (opt == '?') {
			eal_usage(prgname);
			ret = -1;
			goto out;
		}

		/* eal_log_level_parse() already handled this option */
		if (opt == OPT_LOG_LEVEL_NUM)
			continue;

		ret = eal_parse_common_option(opt, optarg, internal_conf);
		/* common parser is not happy */
		if (ret < 0) {
			eal_usage(prgname);
			ret = -1;
			goto out;
		}
		/* common parser handled this option */
		if (ret == 0)
			continue;

		switch (opt) {
		case OPT_HELP_NUM:
			eal_usage(prgname);
			exit(EXIT_SUCCESS);

		case OPT_HUGE_DIR_NUM:
		{
			char *hdir = strdup(optarg);
			if (hdir == NULL)
				RTE_LOG(ERR, EAL, "Could not store hugepage directory\n");
			else {
				/* free old hugepage dir */
				free(internal_conf->hugepage_dir);
				internal_conf->hugepage_dir = hdir;
			}
			break;
		}
		case OPT_FILE_PREFIX_NUM:
		{
			char *prefix = strdup(optarg);
			if (prefix == NULL)
				RTE_LOG(ERR, EAL, "Could not store file prefix\n");
			else {
				/* free old prefix */
				free(internal_conf->hugefile_prefix);
				internal_conf->hugefile_prefix = prefix;
			}
			break;
		}
		case OPT_SOCKET_MEM_NUM:
			if (eal_parse_socket_arg(optarg,
					internal_conf->socket_mem) < 0) {
				RTE_LOG(ERR, EAL, "invalid parameters for --"
						OPT_SOCKET_MEM "\n");
				eal_usage(prgname);
				ret = -1;
				goto out;
			}
			internal_conf->force_sockets = 1;
			break;

		case OPT_SOCKET_LIMIT_NUM:
			if (eal_parse_socket_arg(optarg,
					internal_conf->socket_limit) < 0) {
				RTE_LOG(ERR, EAL, "invalid parameters for --"
						OPT_SOCKET_LIMIT "\n");
				eal_usage(prgname);
				ret = -1;
				goto out;
			}
			internal_conf->force_socket_limits = 1;
			break;

		case OPT_VFIO_INTR_NUM:
			if (eal_parse_vfio_intr(optarg) < 0) {
				RTE_LOG(ERR, EAL, "invalid parameters for --"
						OPT_VFIO_INTR "\n");
				eal_usage(prgname);
				ret = -1;
				goto out;
			}
			break;

		case OPT_VFIO_VF_TOKEN_NUM:
			if (eal_parse_vfio_vf_token(optarg) < 0) {
				RTE_LOG(ERR, EAL, "invalid parameters for --"
						OPT_VFIO_VF_TOKEN "\n");
				eal_usage(prgname);
				ret = -1;
				goto out;
			}
			break;

		case OPT_CREATE_UIO_DEV_NUM:
			internal_conf->create_uio_dev = 1;
			break;

		case OPT_MBUF_POOL_OPS_NAME_NUM:
		{
			char *ops_name = strdup(optarg);
			if (ops_name == NULL)
				RTE_LOG(ERR, EAL, "Could not store mbuf pool ops name\n");
			else {
				/* free old ops name */
				free(internal_conf->user_mbuf_pool_ops_name);

				internal_conf->user_mbuf_pool_ops_name =
						ops_name;
			}
			break;
		}
		case OPT_MATCH_ALLOCATIONS_NUM:
			internal_conf->match_allocations = 1;
			break;

		case OPT_HUGE_WORKER_STACK_NUM:
			if (eal_parse_huge_worker_stack(optarg) < 0) {
				RTE_LOG(ERR, EAL, "invalid parameter for --"
					OPT_HUGE_WORKER_STACK"\n");
				eal_usage(prgname);
				ret = -1;
				goto out;
			}
			break;

		default:
			if (opt < OPT_LONG_MIN_NUM && isprint(opt)) {
				RTE_LOG(ERR, EAL, "Option %c is not supported "
					"on Linux\n", opt);
			} else if (opt >= OPT_LONG_MIN_NUM &&
				   opt < OPT_LONG_MAX_NUM) {
				RTE_LOG(ERR, EAL, "Option %s is not supported "
					"on Linux\n",
					eal_long_options[option_index].name);
			} else {
				RTE_LOG(ERR, EAL, "Option %d is not supported "
					"on Linux\n", opt);
			}
			eal_usage(prgname);
			ret = -1;
			goto out;
		}
	}

	/* create runtime data directory. In no_shconf mode, skip any errors */
	if (eal_create_runtime_dir() < 0) {
		if (internal_conf->no_shconf == 0) {
			RTE_LOG(ERR, EAL, "Cannot create runtime directory\n");
			ret = -1;
			goto out;
		} else
			RTE_LOG(WARNING, EAL, "No DPDK runtime directory created\n");
	}

	if (eal_adjust_config(internal_conf) != 0) {
		ret = -1;
		goto out;
	}

	/* sanity checks */
	if (eal_check_common_options(internal_conf) != 0) {
		eal_usage(prgname);
		ret = -1;
		goto out;
	}

	if (optind >= 0)
		argv[optind-1] = prgname;
	ret = optind-1;

out:
	/* restore getopt lib */
	optind = old_optind;
	optopt = old_optopt;
	optarg = old_optarg;

	return ret;
}

static int
check_socket(const struct rte_memseg_list *msl, void *arg)
{
	int *socket_id = arg;

	if (msl->external)
		return 0;

	return *socket_id == msl->socket_id;
}

static void
eal_check_mem_on_local_socket(void)
{
	int socket_id;
	const struct rte_config *config = rte_eal_get_configuration();

	socket_id = rte_lcore_to_socket_id(config->main_lcore);

	if (rte_memseg_list_walk(check_socket, &socket_id) == 0)
		RTE_LOG(WARNING, EAL, "WARNING: Main core has no memory on local socket!\n");
}

static int
sync_func(__rte_unused void *arg)
{
	return 0;
}

/*
 * Request iopl privilege for all RPL, returns 0 on success
 * iopl() call is mostly for the i386 architecture. For other architectures,
 * return -1 to indicate IO privilege can't be changed in this way.
 */
int
rte_eal_iopl_init(void)
{
#if defined(RTE_ARCH_X86)
	if (iopl(3) != 0)
		return -1;
#endif
	return 0;
}

#ifdef VFIO_PRESENT
static int rte_eal_vfio_setup(void)
{
	if (rte_vfio_enable("vfio"))
		return -1;

	return 0;
}
#endif

static void rte_eal_init_alert(const char *msg)
{
	fprintf(stderr, "EAL: FATAL: %s\n", msg);
	RTE_LOG(ERR, EAL, "%s\n", msg);
}

/*
 * On Linux 3.6+, even if VFIO is not loaded, whenever IOMMU is enabled in the
 * BIOS and in the kernel, /sys/kernel/iommu_groups path will contain kernel
 * IOMMU groups. If IOMMU is not enabled, that path would be empty.
 * Therefore, checking if the path is empty will tell us if IOMMU is enabled.
 */
static bool
is_iommu_enabled(void)
{
	DIR *dir = opendir(KERNEL_IOMMU_GROUPS_PATH);
	struct dirent *d;
	int n = 0;

	/* if directory doesn't exist, assume IOMMU is not enabled */
	if (dir == NULL)
		return false;

	while ((d = readdir(dir)) != NULL) {
		/* skip dot and dot-dot */
		if (++n > 2)
			break;
	}
	closedir(dir);

	return n > 2;
}

static __rte_noreturn void *
eal_worker_thread_loop(void *arg)
{
	eal_thread_loop(arg);
}

static int
eal_worker_thread_create(unsigned int lcore_id)
{
	pthread_attr_t *attrp = NULL;
	void *stack_ptr = NULL;
	pthread_attr_t attr;
	size_t stack_size;
	int ret = -1;

	stack_size = eal_get_internal_configuration()->huge_worker_stack_size;
	if (stack_size != 0) {
		/* Allocate NUMA aware stack memory and set pthread attributes */
		stack_ptr = rte_zmalloc_socket("lcore_stack", stack_size,
			RTE_CACHE_LINE_SIZE, rte_lcore_to_socket_id(lcore_id));
		if (stack_ptr == NULL) {
			rte_eal_init_alert("Cannot allocate worker lcore stack memory");
			rte_errno = ENOMEM;
			goto out;
		}

		if (pthread_attr_init(&attr) != 0) {
			rte_eal_init_alert("Cannot init pthread attributes");
			rte_errno = EFAULT;
			goto out;
		}
		attrp = &attr;

		if (pthread_attr_setstack(attrp, stack_ptr, stack_size) != 0) {
			rte_eal_init_alert("Cannot set pthread stack attributes");
			rte_errno = EFAULT;
			goto out;
		}
	}

	if (pthread_create((pthread_t *)&lcore_config[lcore_id].thread_id.opaque_id,
			attrp, eal_worker_thread_loop, (void *)(uintptr_t)lcore_id) == 0)
		ret = 0;

out:
	if (ret != 0)
		rte_free(stack_ptr);
	if (attrp != NULL)
		pthread_attr_destroy(attrp);
	return ret;
}

/* Launch threads, called at application init(). */
int
rte_eal_init(int argc, char **argv)
{
	int i, fctret, ret;
	static RTE_ATOMIC(uint32_t) run_once;
	uint32_t has_run = 0;
	char cpuset[RTE_CPU_AFFINITY_STR_LEN];
	char thread_name[RTE_THREAD_NAME_SIZE];
	bool phys_addrs;
	const struct rte_config *config = rte_eal_get_configuration();
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* checks if the machine is adequate */
	if (!rte_cpu_is_supported()) {
		rte_eal_init_alert("unsupported cpu type.");
		rte_errno = ENOTSUP;
		return -1;
	}

	if (!rte_atomic_compare_exchange_strong_explicit(&run_once, &has_run, 1,
					rte_memory_order_relaxed, rte_memory_order_relaxed)) {
		rte_eal_init_alert("already called initialization.");
		rte_errno = EALREADY;
		return -1;
	}

	eal_reset_internal_config(internal_conf);

	/* set log level as early as possible */
	eal_log_level_parse(argc, argv);

	/* clone argv to report out later in telemetry */
	eal_save_args(argc, argv);

	if (rte_eal_cpu_init() < 0) {
		rte_eal_init_alert("Cannot detect lcores.");
		rte_errno = ENOTSUP;
		return -1;
	}

	fctret = eal_parse_args(argc, argv);
	if (fctret < 0) {
		rte_eal_init_alert("Invalid 'command line' arguments.");
		rte_errno = EINVAL;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}

	if (eal_plugins_init() < 0) {
		rte_eal_init_alert("Cannot init plugins");
		rte_errno = EINVAL;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}

	if (eal_trace_init() < 0) {
		rte_eal_init_alert("Cannot init trace");
		rte_errno = EFAULT;
		return -1;
	}

	if (eal_option_device_parse()) {
		rte_errno = ENODEV;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}

	if (rte_config_init() < 0) {
		rte_eal_init_alert("Cannot init config");
		return -1;
	}

	if (rte_eal_intr_init() < 0) {
		rte_eal_init_alert("Cannot init interrupt-handling thread");
		return -1;
	}

	if (rte_eal_alarm_init() < 0) {
		rte_eal_init_alert("Cannot init alarm");
		/* rte_eal_alarm_init sets rte_errno on failure. */
		return -1;
	}

	/* Put mp channel init before bus scan so that we can init the vdev
	 * bus through mp channel in the secondary process before the bus scan.
	 */
	if (rte_mp_channel_init() < 0 && rte_errno != ENOTSUP) {
		rte_eal_init_alert("failed to init mp channel");
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			rte_errno = EFAULT;
			return -1;
		}
	}

	if (rte_bus_scan()) {
		rte_eal_init_alert("Cannot scan the buses for devices");
		rte_errno = ENODEV;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}

	phys_addrs = rte_eal_using_phys_addrs() != 0;

	/* if no EAL option "--iova-mode=<pa|va>", use bus IOVA scheme */
	if (internal_conf->iova_mode == RTE_IOVA_DC) {
		/* autodetect the IOVA mapping mode */
		enum rte_iova_mode iova_mode = rte_bus_get_iommu_class();

		if (iova_mode == RTE_IOVA_DC) {
			RTE_LOG(DEBUG, EAL, "Buses did not request a specific IOVA mode.\n");

			if (!RTE_IOVA_IN_MBUF) {
				iova_mode = RTE_IOVA_VA;
				RTE_LOG(DEBUG, EAL, "IOVA as VA mode is forced by build option.\n");
			} else if (!phys_addrs) {
				/* if we have no access to physical addresses,
				 * pick IOVA as VA mode.
				 */
				iova_mode = RTE_IOVA_VA;
				RTE_LOG(DEBUG, EAL, "Physical addresses are unavailable, selecting IOVA as VA mode.\n");
			} else if (is_iommu_enabled()) {
				/* we have an IOMMU, pick IOVA as VA mode */
				iova_mode = RTE_IOVA_VA;
				RTE_LOG(DEBUG, EAL, "IOMMU is available, selecting IOVA as VA mode.\n");
			} else {
				/* physical addresses available, and no IOMMU
				 * found, so pick IOVA as PA.
				 */
				iova_mode = RTE_IOVA_PA;
				RTE_LOG(DEBUG, EAL, "IOMMU is not available, selecting IOVA as PA mode.\n");
			}
		}
		rte_eal_get_configuration()->iova_mode = iova_mode;
	} else {
		rte_eal_get_configuration()->iova_mode =
			internal_conf->iova_mode;
	}

	if (rte_eal_iova_mode() == RTE_IOVA_PA && !phys_addrs) {
		rte_eal_init_alert("Cannot use IOVA as 'PA' since physical addresses are not available");
		rte_errno = EINVAL;
		return -1;
	}

	if (rte_eal_iova_mode() == RTE_IOVA_PA && !RTE_IOVA_IN_MBUF) {
		rte_eal_init_alert("Cannot use IOVA as 'PA' as it is disabled during build");
		rte_errno = EINVAL;
		return -1;
	}

	RTE_LOG(INFO, EAL, "Selected IOVA mode '%s'\n",
		rte_eal_iova_mode() == RTE_IOVA_PA ? "PA" : "VA");

	if (internal_conf->no_hugetlbfs == 0) {
		/* rte_config isn't initialized yet */
		ret = internal_conf->process_type == RTE_PROC_PRIMARY ?
				eal_hugepage_info_init() :
				eal_hugepage_info_read();
		if (ret < 0) {
			rte_eal_init_alert("Cannot get hugepage information.");
			rte_errno = EACCES;
			rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
			return -1;
		}
	}

	if (internal_conf->memory == 0 && internal_conf->force_sockets == 0) {
		if (internal_conf->no_hugetlbfs)
			internal_conf->memory = MEMSIZE_IF_NO_HUGE_PAGE;
	}

	if (internal_conf->vmware_tsc_map == 1) {
#ifdef RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT
		rte_cycles_vmware_tsc_map = 1;
		RTE_LOG (DEBUG, EAL, "Using VMWARE TSC MAP, "
				"you must have monitor_control.pseudo_perfctr = TRUE\n");
#else
		RTE_LOG (WARNING, EAL, "Ignoring --vmware-tsc-map because "
				"RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT is not set\n");
#endif
	}

	if (eal_log_init(program_invocation_short_name,
			 internal_conf->syslog_facility) < 0) {
		rte_eal_init_alert("Cannot init logging.");
		rte_errno = ENOMEM;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}

#ifdef VFIO_PRESENT
	if (rte_eal_vfio_setup() < 0) {
		rte_eal_init_alert("Cannot init VFIO");
		rte_errno = EAGAIN;
		rte_atomic_store_explicit(&run_once, 0, rte_memory_order_relaxed);
		return -1;
	}
#endif
	/* in secondary processes, memory init may allocate additional fbarrays
	 * not present in primary processes, so to avoid any potential issues,
	 * initialize memzones first.
	 */
	if (rte_eal_memzone_init() < 0) {
		rte_eal_init_alert("Cannot init memzone");
		rte_errno = ENODEV;
		return -1;
	}

	rte_mcfg_mem_read_lock();

	if (rte_eal_memory_init() < 0) {
		rte_mcfg_mem_read_unlock();
		rte_eal_init_alert("Cannot init memory");
		rte_errno = ENOMEM;
		return -1;
	}

	/* the directories are locked during eal_hugepage_info_init */
	eal_hugedirs_unlock();

	if (rte_eal_malloc_heap_init() < 0) {
		rte_mcfg_mem_read_unlock();
		rte_eal_init_alert("Cannot init malloc heap");
		rte_errno = ENODEV;
		return -1;
	}

	rte_mcfg_mem_read_unlock();

	if (rte_eal_malloc_heap_populate() < 0) {
		rte_eal_init_alert("Cannot init malloc heap");
		rte_errno = ENODEV;
		return -1;
	}

	/* register multi-process action callbacks for hotplug after memory init */
	if (eal_mp_dev_hotplug_init() < 0) {
		rte_eal_init_alert("failed to register mp callback for hotplug");
		return -1;
	}

	if (rte_eal_tailqs_init() < 0) {
		rte_eal_init_alert("Cannot init tail queues for objects");
		rte_errno = EFAULT;
		return -1;
	}

	if (rte_eal_timer_init() < 0) {
		rte_eal_init_alert("Cannot init HPET or TSC timers");
		rte_errno = ENOTSUP;
		return -1;
	}

	eal_check_mem_on_local_socket();

	if (rte_thread_set_affinity_by_id(rte_thread_self(),
			&lcore_config[config->main_lcore].cpuset) != 0) {
		rte_eal_init_alert("Cannot set affinity");
		rte_errno = EINVAL;
		return -1;
	}
	__rte_thread_init(config->main_lcore,
		&lcore_config[config->main_lcore].cpuset);

	ret = eal_thread_dump_current_affinity(cpuset, sizeof(cpuset));
	RTE_LOG(DEBUG, EAL, "Main lcore %u is ready (tid=%zx;cpuset=[%s%s])\n",
		config->main_lcore, (uintptr_t)pthread_self(), cpuset,
		ret == 0 ? "" : "...");

	RTE_LCORE_FOREACH_WORKER(i) {

		/*
		 * create communication pipes between main thread
		 * and children
		 */
		if (pipe(lcore_config[i].pipe_main2worker) < 0)
			rte_panic("Cannot create pipe\n");
		if (pipe(lcore_config[i].pipe_worker2main) < 0)
			rte_panic("Cannot create pipe\n");

		lcore_config[i].state = WAIT;

		/* create a thread for each lcore */
		ret = eal_worker_thread_create(i);
		if (ret != 0)
			rte_panic("Cannot create thread\n");

		/* Set thread_name for aid in debugging. */
		snprintf(thread_name, sizeof(thread_name),
			"dpdk-worker%d", i);
		rte_thread_set_name(lcore_config[i].thread_id, thread_name);

		ret = rte_thread_set_affinity_by_id(lcore_config[i].thread_id,
			&lcore_config[i].cpuset);
		if (ret != 0)
			rte_panic("Cannot set affinity\n");
	}

	/*
	 * Launch a dummy function on all worker lcores, so that main lcore
	 * knows they are all ready when this function returns.
	 */
	rte_eal_mp_remote_launch(sync_func, NULL, SKIP_MAIN);
	rte_eal_mp_wait_lcore();

	/* initialize services so vdevs register service during bus_probe. */
	ret = rte_service_init();
	if (ret) {
		rte_eal_init_alert("rte_service_init() failed");
		rte_errno = -ret;
		return -1;
	}

	/* Probe all the buses and devices/drivers on them */
	if (rte_bus_probe()) {
		rte_eal_init_alert("Cannot probe devices");
		rte_errno = ENOTSUP;
		return -1;
	}

#ifdef VFIO_PRESENT
	/* Register mp action after probe() so that we got enough info */
	if (rte_vfio_is_enabled("vfio") && vfio_mp_sync_setup() < 0)
		return -1;
#endif

	/* initialize default service/lcore mappings and start running. Ignore
	 * -ENOTSUP, as it indicates no service coremask passed to EAL.
	 */
	ret = rte_service_start_with_defaults();
	if (ret < 0 && ret != -ENOTSUP) {
		rte_errno = -ret;
		return -1;
	}

	/*
	 * Clean up unused files in runtime directory. We do this at the end of
	 * init and not at the beginning because we want to clean stuff up
	 * whether we are primary or secondary process, but we cannot remove
	 * primary process' files because secondary should be able to run even
	 * if primary process is dead.
	 *
	 * In no_shconf mode, no runtime directory is created in the first
	 * place, so no cleanup needed.
	 */
	if (!internal_conf->no_shconf && eal_clean_runtime_dir() < 0) {
		rte_eal_init_alert("Cannot clear runtime directory");
		return -1;
	}
	if (rte_eal_process_type() == RTE_PROC_PRIMARY && !internal_conf->no_telemetry) {
		if (rte_telemetry_init(rte_eal_get_runtime_dir(),
				rte_version(),
				&internal_conf->ctrl_cpuset) != 0)
			return -1;
	}

	eal_mcfg_complete();

	return fctret;
}

static int
mark_freeable(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg __rte_unused)
{
	/* ms is const, so find this memseg */
	struct rte_memseg *found;

	if (msl->external)
		return 0;

	found = rte_mem_virt2memseg(ms->addr, msl);

	found->flags &= ~RTE_MEMSEG_FLAG_DO_NOT_FREE;

	return 0;
}

int
rte_eal_cleanup(void)
{
	static RTE_ATOMIC(uint32_t) run_once;
	uint32_t has_run = 0;

	if (!rte_atomic_compare_exchange_strong_explicit(&run_once, &has_run, 1,
					rte_memory_order_relaxed, rte_memory_order_relaxed)) {
		RTE_LOG(WARNING, EAL, "Already called cleanup\n");
		rte_errno = EALREADY;
		return -1;
	}

	/* if we're in a primary process, we need to mark hugepages as freeable
	 * so that finalization can release them back to the system.
	 */
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
			internal_conf->hugepage_file.unlink_existing)
		rte_memseg_walk(mark_freeable, NULL);

	rte_service_finalize();
#ifdef VFIO_PRESENT
	vfio_mp_sync_cleanup();
#endif
	rte_mp_channel_cleanup();
	eal_bus_cleanup();
	rte_trace_save();
	eal_trace_fini();
	eal_mp_dev_hotplug_cleanup();
	rte_eal_alarm_cleanup();
	/* after this point, any DPDK pointers will become dangling */
	rte_eal_memory_detach();
	rte_eal_malloc_heap_cleanup();
	eal_cleanup_config(internal_conf);
	rte_eal_log_cleanup();
	return 0;
}

int rte_eal_create_uio_dev(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	return internal_conf->create_uio_dev;
}

enum rte_intr_mode
rte_eal_vfio_intr_mode(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	return internal_conf->vfio_intr_mode;
}

void
rte_eal_vfio_get_vf_token(rte_uuid_t vf_token)
{
	struct internal_config *cfg = eal_get_internal_configuration();

	rte_uuid_copy(vf_token, cfg->vfio_vf_token);
}

int
rte_eal_check_module(const char *module_name)
{
	char sysfs_mod_name[PATH_MAX];
	struct stat st;
	int n;

	if (NULL == module_name)
		return -1;

	/* Check if there is sysfs mounted */
	if (stat("/sys/module", &st) != 0) {
		RTE_LOG(DEBUG, EAL, "sysfs is not mounted! error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	/* A module might be built-in, therefore try sysfs */
	n = snprintf(sysfs_mod_name, PATH_MAX, "/sys/module/%s", module_name);
	if (n < 0 || n > PATH_MAX) {
		RTE_LOG(DEBUG, EAL, "Could not format module path\n");
		return -1;
	}

	if (stat(sysfs_mod_name, &st) != 0) {
		RTE_LOG(DEBUG, EAL, "Module %s not found! error %i (%s)\n",
		        sysfs_mod_name, errno, strerror(errno));
		return 0;
	}

	/* Module has been found */
	return 1;
}
