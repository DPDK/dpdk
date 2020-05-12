/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#ifdef RTE_IBVERBS_LINK_DLOPEN
#include <dlfcn.h>
#endif

#include <rte_errno.h>

#include "mlx5_common.h"
#include "mlx5_common_utils.h"
#include "mlx5_glue.h"


int mlx5_common_logtype;

#ifdef MLX5_GLUE
const struct mlx5_glue *mlx5_glue;
#endif

uint8_t haswell_broadwell_cpu;

/**
 * Get PCI information by sysfs device path.
 *
 * @param dev_path
 *   Pointer to device sysfs folder name.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_to_pci_addr(const char *dev_path,
		     struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", dev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

static int
mlx5_class_check_handler(__rte_unused const char *key, const char *value,
			 void *opaque)
{
	enum mlx5_class *ret = opaque;

	if (strcmp(value, "vdpa") == 0) {
		*ret = MLX5_CLASS_VDPA;
	} else if (strcmp(value, "net") == 0) {
		*ret = MLX5_CLASS_NET;
	} else {
		DRV_LOG(ERR, "Invalid mlx5 class %s. Maybe typo in device"
			" class argument setting?", value);
		*ret = MLX5_CLASS_INVALID;
	}
	return 0;
}

enum mlx5_class
mlx5_class_get(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *key = MLX5_CLASS_ARG_NAME;
	enum mlx5_class ret = MLX5_CLASS_NET;

	if (devargs == NULL)
		return ret;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return ret;
	if (rte_kvargs_count(kvlist, key))
		rte_kvargs_process(kvlist, key, mlx5_class_check_handler, &ret);
	rte_kvargs_free(kvlist);
	return ret;
}

/**
 * Extract port name, as a number, from sysfs or netlink information.
 *
 * @param[in] port_name_in
 *   String representing the port name.
 * @param[out] port_info_out
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   port_name field set according to recognized name format.
 */
void
mlx5_translate_port_name(const char *port_name_in,
			 struct mlx5_switch_info *port_info_out)
{
	char pf_c1, pf_c2, vf_c1, vf_c2;
	char *end;
	int sc_items;

	/*
	 * Check for port-name as a string of the form pf0vf0
	 * (support kernel ver >= 5.0 or OFED ver >= 4.6).
	 */
	sc_items = sscanf(port_name_in, "%c%c%d%c%c%d",
			  &pf_c1, &pf_c2, &port_info_out->pf_num,
			  &vf_c1, &vf_c2, &port_info_out->port_name);
	if (sc_items == 6 &&
	    pf_c1 == 'p' && pf_c2 == 'f' &&
	    vf_c1 == 'v' && vf_c2 == 'f') {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_PFVF;
		return;
	}
	/*
	 * Check for port-name as a string of the form p0
	 * (support kernel ver >= 5.0, or OFED ver >= 4.6).
	 */
	sc_items = sscanf(port_name_in, "%c%d",
			  &pf_c1, &port_info_out->port_name);
	if (sc_items == 2 && pf_c1 == 'p') {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_UPLINK;
		return;
	}
	/* Check for port-name as a number (support kernel ver < 5.0 */
	errno = 0;
	port_info_out->port_name = strtol(port_name_in, &end, 0);
	if (!errno &&
	    (size_t)(end - port_name_in) == strlen(port_name_in)) {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_LEGACY;
		return;
	}
	port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN;
	return;
}

#ifdef MLX5_GLUE

/**
 * Suffix RTE_EAL_PMD_PATH with "-glue".
 *
 * This function performs a sanity check on RTE_EAL_PMD_PATH before
 * suffixing its last component.
 *
 * @param buf[out]
 *   Output buffer, should be large enough otherwise NULL is returned.
 * @param size
 *   Size of @p out.
 *
 * @return
 *   Pointer to @p buf or @p NULL in case suffix cannot be appended.
 */
static char *
mlx5_glue_path(char *buf, size_t size)
{
	static const char *const bad[] = { "/", ".", "..", NULL };
	const char *path = RTE_EAL_PMD_PATH;
	size_t len = strlen(path);
	size_t off;
	int i;

	while (len && path[len - 1] == '/')
		--len;
	for (off = len; off && path[off - 1] != '/'; --off)
		;
	for (i = 0; bad[i]; ++i)
		if (!strncmp(path + off, bad[i], (int)(len - off)))
			goto error;
	i = snprintf(buf, size, "%.*s-glue", (int)len, path);
	if (i == -1 || (size_t)i >= size)
		goto error;
	return buf;
error:
	RTE_LOG(ERR, PMD, "unable to append \"-glue\" to last component of"
		" RTE_EAL_PMD_PATH (\"" RTE_EAL_PMD_PATH "\"), please"
		" re-configure DPDK");
	return NULL;
}

static int
mlx5_glue_dlopen(void)
{
	char glue_path[sizeof(RTE_EAL_PMD_PATH) - 1 + sizeof("-glue")];
	void *handle = NULL;

	const char *path[] = {
		/*
		 * A basic security check is necessary before trusting
		 * MLX5_GLUE_PATH, which may override RTE_EAL_PMD_PATH.
		 */
		(geteuid() == getuid() && getegid() == getgid() ?
		 getenv("MLX5_GLUE_PATH") : NULL),
		/*
		 * When RTE_EAL_PMD_PATH is set, use its glue-suffixed
		 * variant, otherwise let dlopen() look up libraries on its
		 * own.
		 */
		(*RTE_EAL_PMD_PATH ?
		 mlx5_glue_path(glue_path, sizeof(glue_path)) : ""),
	};
	unsigned int i = 0;
	void **sym;
	const char *dlmsg;

	while (!handle && i != RTE_DIM(path)) {
		const char *end;
		size_t len;
		int ret;

		if (!path[i]) {
			++i;
			continue;
		}
		end = strpbrk(path[i], ":;");
		if (!end)
			end = path[i] + strlen(path[i]);
		len = end - path[i];
		ret = 0;
		do {
			char name[ret + 1];

			ret = snprintf(name, sizeof(name), "%.*s%s" MLX5_GLUE,
				       (int)len, path[i],
				       (!len || *(end - 1) == '/') ? "" : "/");
			if (ret == -1)
				break;
			if (sizeof(name) != (size_t)ret + 1)
				continue;
			DRV_LOG(DEBUG, "Looking for rdma-core glue as "
				"\"%s\"", name);
			handle = dlopen(name, RTLD_LAZY);
			break;
		} while (1);
		path[i] = end + 1;
		if (!*end)
			++i;
	}
	if (!handle) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			DRV_LOG(WARNING, "Cannot load glue library: %s", dlmsg);
		goto glue_error;
	}
	sym = dlsym(handle, "mlx5_glue");
	if (!sym || !*sym) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			DRV_LOG(ERR, "Cannot resolve glue symbol: %s", dlmsg);
		goto glue_error;
	}
	mlx5_glue = *sym;
	return 0;

glue_error:
	if (handle)
		dlclose(handle);
	return -1;
}

#endif

/* In case this is an x86_64 intel processor to check if
 * we should use relaxed ordering.
 */
#ifdef RTE_ARCH_X86_64
/**
 * This function returns processor identification and feature information
 * into the registers.
 *
 * @param eax, ebx, ecx, edx
 *		Pointers to the registers that will hold cpu information.
 * @param level
 *		The main category of information returned.
 */
static inline void mlx5_cpu_id(unsigned int level,
				unsigned int *eax, unsigned int *ebx,
				unsigned int *ecx, unsigned int *edx)
{
	__asm__("cpuid\n\t"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (level));
}
#endif

RTE_INIT_PRIO(mlx5_log_init, LOG)
{
	mlx5_common_logtype = rte_log_register("pmd.common.mlx5");
	if (mlx5_common_logtype >= 0)
		rte_log_set_level(mlx5_common_logtype, RTE_LOG_NOTICE);
}

/**
 * Initialization routine for run-time dependency on rdma-core.
 */
RTE_INIT_PRIO(mlx5_glue_init, CLASS)
{
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	/* Match the size of Rx completion entry to the size of a cacheline. */
	if (RTE_CACHE_LINE_SIZE == 128)
		setenv("MLX5_CQE_SIZE", "128", 0);
	/*
	 * MLX5_DEVICE_FATAL_CLEANUP tells ibv_destroy functions to
	 * cleanup all the Verbs resources even when the device was removed.
	 */
	setenv("MLX5_DEVICE_FATAL_CLEANUP", "1", 1);

#ifdef MLX5_GLUE
	if (mlx5_glue_dlopen() != 0)
		goto glue_error;
#endif

#ifdef RTE_LIBRTE_MLX5_DEBUG
	/* Glue structure must not contain any NULL pointers. */
	{
		unsigned int i;

		for (i = 0; i != sizeof(*mlx5_glue) / sizeof(void *); ++i)
			MLX5_ASSERT(((const void *const *)mlx5_glue)[i]);
	}
#endif
	if (strcmp(mlx5_glue->version, MLX5_GLUE_VERSION)) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "rdma-core glue \"%s\" mismatch: \"%s\" is "
			"required", mlx5_glue->version, MLX5_GLUE_VERSION);
		goto glue_error;
	}
	mlx5_glue->fork_init();
	return;

glue_error:
	DRV_LOG(WARNING, "Cannot initialize MLX5 common due to missing"
		" run-time dependency on rdma-core libraries (libibverbs,"
		" libmlx5)");
	mlx5_glue = NULL;
	return;
}

/**
 * This function is responsible of initializing the variable
 *  haswell_broadwell_cpu by checking if the cpu is intel
 *  and reading the data returned from mlx5_cpu_id().
 *  since haswell and broadwell cpus don't have improved performance
 *  when using relaxed ordering we want to check the cpu type before
 *  before deciding whether to enable RO or not.
 *  if the cpu is haswell or broadwell the variable will be set to 1
 *  otherwise it will be 0.
 */
RTE_INIT_PRIO(mlx5_is_haswell_broadwell_cpu, LOG)
{
#ifdef RTE_ARCH_X86_64
	unsigned int broadwell_models[4] = {0x3d, 0x47, 0x4F, 0x56};
	unsigned int haswell_models[4] = {0x3c, 0x3f, 0x45, 0x46};
	unsigned int i, model, family, brand_id, vendor;
	unsigned int signature_intel_ebx = 0x756e6547;
	unsigned int extended_model;
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	int max_level;

	mlx5_cpu_id(0, &eax, &ebx, &ecx, &edx);
	vendor = ebx;
	max_level = eax;
	if (max_level < 1) {
		haswell_broadwell_cpu = 0;
		return;
	}
	mlx5_cpu_id(1, &eax, &ebx, &ecx, &edx);
	model = (eax >> 4) & 0x0f;
	family = (eax >> 8) & 0x0f;
	brand_id = ebx & 0xff;
	extended_model = (eax >> 12) & 0xf0;
	/* Check if the processor is Haswell or Broadwell */
	if (vendor == signature_intel_ebx) {
		if (family == 0x06)
			model += extended_model;
		if (brand_id == 0 && family == 0x6) {
			for (i = 0; i < RTE_DIM(broadwell_models); i++)
				if (model == broadwell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
			for (i = 0; i < RTE_DIM(haswell_models); i++)
				if (model == haswell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
		}
	}
#endif
	haswell_broadwell_cpu = 0;
}
