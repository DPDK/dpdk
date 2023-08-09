/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <getopt.h>
#include <sys/file.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_service_component.h>
#include <rte_log.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_interrupts.h>
#include <rte_bus.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_version.h>
#include <rte_vfio.h>
#include <malloc_heap.h>
#include <telemetry_internal.h>

#include "eal_private.h"
#include "eal_thread.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"
#include "eal_hugepages.h"
#include "eal_options.h"
#include "eal_memcfg.h"
#include "eal_trace.h"

#define MEMSIZE_IF_NO_HUGE_PAGE (64ULL * 1024ULL * 1024ULL)

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
	/* FreeBSD doesn't need this implemented for now, because, unlike Linux,
	 * FreeBSD doesn't create per-process files, so no need to clean up.
	 */
	return 0;
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
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();
	size_t page_sz = sysconf(_SC_PAGE_SIZE);
	size_t cfg_len = sizeof(struct rte_mem_config);
	size_t cfg_len_aligned = RTE_ALIGN(cfg_len, page_sz);
	void *rte_mem_cfg_addr, *mapped_mem_cfg_addr;
	int retval;

	const char *pathname = eal_runtime_config_path();

	if (internal_conf->no_shconf)
		return 0;

	/* map the config before base address so that we don't waste a page */
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
		RTE_LOG(ERR, EAL, "Cannot remap memory for rte_config\n");
		munmap(rte_mem_cfg_addr, cfg_len);
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		return -1;
	}

	memcpy(rte_mem_cfg_addr, config->mem_config, sizeof(struct rte_mem_config));
	config->mem_config = rte_mem_cfg_addr;

	/* store address of the config in the config itself so that secondary
	 * processes could later map the config into this exact location
	 */
	config->mem_config->mem_cfg_addr = (uintptr_t) rte_mem_cfg_addr;
	return 0;
}

/* attach to an existing shared memory config */
static int
rte_eal_config_attach(void)
{
	void *rte_mem_cfg_addr;
	const char *pathname = eal_runtime_config_path();
	struct rte_config *config = rte_eal_get_configuration();
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();


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

	rte_mem_cfg_addr = mmap(NULL, sizeof(*config->mem_config),
				PROT_READ, MAP_SHARED, mem_cfg_fd, 0);
	/* don't close the fd here, it will be closed on reattach */
	if (rte_mem_cfg_addr == MAP_FAILED) {
		close(mem_cfg_fd);
		mem_cfg_fd = -1;
		RTE_LOG(ERR, EAL, "Cannot mmap memory for rte_config! error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	config->mem_config = rte_mem_cfg_addr;

	return 0;
}

/* reattach the shared config at exact memory location primary process has it */
static int
rte_eal_config_reattach(void)
{
	struct rte_mem_config *mem_config;
	void *rte_mem_cfg_addr;
	struct rte_config *config = rte_eal_get_configuration();
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (internal_conf->no_shconf)
		return 0;

	/* save the address primary process has mapped shared config to */
	rte_mem_cfg_addr =
			(void *)(uintptr_t)config->mem_config->mem_cfg_addr;

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
					  "' option\n",
				rte_mem_cfg_addr, mem_config);
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

/* display usage */
static void
eal_usage(const char *prgname)
{
	rte_usage_hook_t hook = eal_get_application_usage_hook();

	printf("\nUsage: %s ", prgname);
	eal_common_usage();
	/* Allow the application to print its usage message too if hook is set */
	if (hook) {
		printf("===== Application Usage =====\n\n");
		(hook)(prgname);
	}
}

static inline size_t
eal_get_hugepage_mem_size(void)
{
	uint64_t size = 0;
	unsigned i, j;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	for (i = 0; i < internal_conf->num_hugepage_sizes; i++) {
		struct hugepage_info *hpi = &internal_conf->hugepage_info[i];
		if (strnlen(hpi->hugedir, sizeof(hpi->hugedir)) != 0) {
			for (j = 0; j < RTE_MAX_NUMA_NODES; j++) {
				size += hpi->hugepage_sz * hpi->num_pages[j];
			}
		}
	}

	return (size < SIZE_MAX) ? (size_t)(size) : SIZE_MAX;
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
	const int old_optreset = optreset;
	char * const old_optarg = optarg;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	argvopt = argv;
	optind = 1;
	optreset = 1;

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
	optreset = old_optreset;
	optarg = old_optarg;
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
	const int old_optreset = optreset;
	char * const old_optarg = optarg;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	argvopt = argv;
	optind = 1;
	optreset = 1;

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
		case OPT_HELP_NUM:
			eal_usage(prgname);
			exit(EXIT_SUCCESS);
		default:
			if (opt < OPT_LONG_MIN_NUM && isprint(opt)) {
				RTE_LOG(ERR, EAL, "Option %c is not supported "
					"on FreeBSD\n", opt);
			} else if (opt >= OPT_LONG_MIN_NUM &&
				   opt < OPT_LONG_MAX_NUM) {
				RTE_LOG(ERR, EAL, "Option %s is not supported "
					"on FreeBSD\n",
					eal_long_options[option_index].name);
			} else {
				RTE_LOG(ERR, EAL, "Option %d is not supported "
					"on FreeBSD\n", opt);
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
	optreset = old_optreset;
	optarg = old_optarg;

	return ret;
}

static int
check_socket(const struct rte_memseg_list *msl, void *arg)
{
	int *socket_id = arg;

	if (msl->external)
		return 0;

	if (msl->socket_id == *socket_id && msl->memseg_arr.count != 0)
		return 1;

	return 0;
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
/* Abstraction for port I/0 privilege */
int
rte_eal_iopl_init(void)
{
	static int fd = -1;

	if (fd < 0)
		fd = open("/dev/io", O_RDWR);

	if (fd < 0)
		return -1;
	/* keep fd open for iopl */
	return 0;
}

static void rte_eal_init_alert(const char *msg)
{
	fprintf(stderr, "EAL: FATAL: %s\n", msg);
	RTE_LOG(ERR, EAL, "%s\n", msg);
}

/* Launch threads, called at application init(). */
int
rte_eal_init(int argc, char **argv)
{
	int i, fctret, ret;
	static uint32_t run_once;
	uint32_t has_run = 0;
	char cpuset[RTE_CPU_AFFINITY_STR_LEN];
	char thread_name[RTE_MAX_THREAD_NAME_LEN];
	const struct rte_config *config = rte_eal_get_configuration();
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	bool has_phys_addr;
	enum rte_iova_mode iova_mode;

	/* checks if the machine is adequate */
	if (!rte_cpu_is_supported()) {
		rte_eal_init_alert("unsupported cpu type.");
		rte_errno = ENOTSUP;
		return -1;
	}

	if (!__atomic_compare_exchange_n(&run_once, &has_run, 1, 0,
					__ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
		rte_eal_init_alert("already called initialization.");
		rte_errno = EALREADY;
		return -1;
	}

	eal_reset_internal_config(internal_conf);

	/* clone argv to report out later in telemetry */
	eal_save_args(argc, argv);

	/* set log level as early as possible */
	eal_log_level_parse(argc, argv);

	if (rte_eal_cpu_init() < 0) {
		rte_eal_init_alert("Cannot detect lcores.");
		rte_errno = ENOTSUP;
		return -1;
	}

	fctret = eal_parse_args(argc, argv);
	if (fctret < 0) {
		rte_eal_init_alert("Invalid 'command line' arguments.");
		rte_errno = EINVAL;
		__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
		return -1;
	}

	/* FreeBSD always uses legacy memory model */
	internal_conf->legacy_mem = true;
	if (internal_conf->in_memory) {
		RTE_LOG(WARNING, EAL, "Warning: ignoring unsupported flag, '%s'\n", OPT_IN_MEMORY);
		internal_conf->in_memory = false;
	}

	if (eal_plugins_init() < 0) {
		rte_eal_init_alert("Cannot init plugins");
		rte_errno = EINVAL;
		__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
		return -1;
	}

	if (eal_trace_init() < 0) {
		rte_eal_init_alert("Cannot init trace");
		rte_errno = EFAULT;
		__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
		return -1;
	}

	if (eal_option_device_parse()) {
		rte_errno = ENODEV;
		__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
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
		__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
		return -1;
	}

	/*
	 * PA are only available for hugepages via contigmem.
	 * If contigmem is inaccessible, rte_eal_hugepage_init() will fail
	 * with a message describing the cause.
	 */
	has_phys_addr = internal_conf->no_hugetlbfs == 0;
	iova_mode = internal_conf->iova_mode;
	if (iova_mode == RTE_IOVA_DC) {
		RTE_LOG(DEBUG, EAL, "Specific IOVA mode is not requested, autodetecting\n");
		if (has_phys_addr) {
			RTE_LOG(DEBUG, EAL, "Selecting IOVA mode according to bus requests\n");
			iova_mode = rte_bus_get_iommu_class();
			if (iova_mode == RTE_IOVA_DC) {
				if (!RTE_IOVA_IN_MBUF) {
					iova_mode = RTE_IOVA_VA;
					RTE_LOG(DEBUG, EAL, "IOVA as VA mode is forced by build option.\n");
				} else	{
					iova_mode = RTE_IOVA_PA;
				}
			}
		} else {
			iova_mode = RTE_IOVA_VA;
		}
	}

	if (iova_mode == RTE_IOVA_PA && !has_phys_addr) {
		rte_eal_init_alert("Cannot use IOVA as 'PA' since physical addresses are not available");
		rte_errno = EINVAL;
		return -1;
	}

	if (iova_mode == RTE_IOVA_PA && !RTE_IOVA_IN_MBUF) {
		rte_eal_init_alert("Cannot use IOVA as 'PA' as it is disabled during build");
		rte_errno = EINVAL;
		return -1;
	}

	rte_eal_get_configuration()->iova_mode = iova_mode;
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
			__atomic_store_n(&run_once, 0, __ATOMIC_RELAXED);
			return -1;
		}
	}

	if (internal_conf->memory == 0 && internal_conf->force_sockets == 0) {
		if (internal_conf->no_hugetlbfs)
			internal_conf->memory = MEMSIZE_IF_NO_HUGE_PAGE;
		else
			internal_conf->memory = eal_get_hugepage_mem_size();
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
		ret = rte_thread_create(&lcore_config[i].thread_id, NULL,
				     eal_thread_loop, (void *)(uintptr_t)i);
		if (ret != 0)
			rte_panic("Cannot create thread\n");

		/* Set thread_name for aid in debugging. */
		snprintf(thread_name, sizeof(thread_name),
				"rte-worker-%d", i);
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

int
rte_eal_cleanup(void)
{
	static uint32_t run_once;
	uint32_t has_run = 0;

	if (!__atomic_compare_exchange_n(&run_once, &has_run, 1, 0,
			__ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
		RTE_LOG(WARNING, EAL, "Already called cleanup\n");
		rte_errno = EALREADY;
		return -1;
	}

	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	rte_service_finalize();
	rte_mp_channel_cleanup();
	eal_bus_cleanup();
	rte_trace_save();
	eal_trace_fini();
	rte_eal_alarm_cleanup();
	/* after this point, any DPDK pointers will become dangling */
	rte_eal_memory_detach();
	eal_cleanup_config(internal_conf);
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
	return RTE_INTR_MODE_NONE;
}

void
rte_eal_vfio_get_vf_token(__rte_unused rte_uuid_t vf_token)
{
}

int rte_vfio_setup_device(__rte_unused const char *sysfs_base,
		      __rte_unused const char *dev_addr,
		      __rte_unused int *vfio_dev_fd,
		      __rte_unused struct vfio_device_info *device_info)
{
	rte_errno = ENOTSUP;
	return -1;
}

int rte_vfio_release_device(__rte_unused const char *sysfs_base,
			__rte_unused const char *dev_addr,
			__rte_unused int fd)
{
	rte_errno = ENOTSUP;
	return -1;
}

int rte_vfio_enable(__rte_unused const char *modname)
{
	rte_errno = ENOTSUP;
	return -1;
}

int rte_vfio_is_enabled(__rte_unused const char *modname)
{
	return 0;
}

int rte_vfio_noiommu_is_enabled(void)
{
	return 0;
}

int rte_vfio_clear_group(__rte_unused int vfio_group_fd)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_get_group_num(__rte_unused const char *sysfs_base,
		       __rte_unused const char *dev_addr,
		       __rte_unused int *iommu_group_num)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_get_container_fd(void)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_get_group_fd(__rte_unused int iommu_group_num)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_create(void)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_destroy(__rte_unused int container_fd)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_group_bind(__rte_unused int container_fd,
		__rte_unused int iommu_group_num)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_group_unbind(__rte_unused int container_fd,
		__rte_unused int iommu_group_num)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_dma_map(__rte_unused int container_fd,
			__rte_unused uint64_t vaddr,
			__rte_unused uint64_t iova,
			__rte_unused uint64_t len)
{
	rte_errno = ENOTSUP;
	return -1;
}

int
rte_vfio_container_dma_unmap(__rte_unused int container_fd,
			__rte_unused uint64_t vaddr,
			__rte_unused uint64_t iova,
			__rte_unused uint64_t len)
{
	rte_errno = ENOTSUP;
	return -1;
}
