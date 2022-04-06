/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#ifdef RTE_IBVERBS_LINK_DLOPEN
#include <dlfcn.h>
#endif
#include <dirent.h>
#include <net/if.h>

#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_bus_pci.h>
#include <rte_bus_auxiliary.h>

#include "mlx5_common.h"
#include "mlx5_nl.h"
#include "mlx5_common_log.h"
#include "mlx5_common_private.h"
#include "mlx5_common_defs.h"
#include "mlx5_common_os.h"
#include "mlx5_glue.h"

#ifdef MLX5_GLUE
const struct mlx5_glue *mlx5_glue;
#endif

int
mlx5_get_pci_addr(const char *dev_path, struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	int rc = -ENOENT;
	MKSTR(path, "%s/device/uevent", dev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1)) {
			while (line[(len - 1)] != '\n') {
				int ret = fgetc(file);

				if (ret == EOF)
					goto exit;
				line[(len - 1)] = ret;
			}
			/* No match for long lines. */
			continue;
		}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			rc = 0;
			break;
		}
	}
exit:
	fclose(file);
	if (rc)
		rte_errno = -rc;
	return rc;
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
	char ctrl = 0, pf_c1, pf_c2, vf_c1, vf_c2, eol;
	char *end;
	int sc_items;

	sc_items = sscanf(port_name_in, "%c%d",
			  &ctrl, &port_info_out->ctrl_num);
	if (sc_items == 2 && ctrl == 'c') {
		port_name_in++; /* 'c' */
		port_name_in += snprintf(NULL, 0, "%d",
					  port_info_out->ctrl_num);
	}
	/* Check for port-name as a string of the form pf0vf0 or pf0sf0 */
	sc_items = sscanf(port_name_in, "%c%c%d%c%c%d%c",
			  &pf_c1, &pf_c2, &port_info_out->pf_num,
			  &vf_c1, &vf_c2, &port_info_out->port_name, &eol);
	if (sc_items == 6 && pf_c1 == 'p' && pf_c2 == 'f') {
		if (vf_c1 == 'v' && vf_c2 == 'f') {
			/* Kernel ver >= 5.0 or OFED ver >= 4.6 */
			port_info_out->name_type =
					MLX5_PHYS_PORT_NAME_TYPE_PFVF;
			return;
		}
		if (vf_c1 == 's' && vf_c2 == 'f') {
			/* Kernel ver >= 5.11 or OFED ver >= 5.1 */
			port_info_out->name_type =
					MLX5_PHYS_PORT_NAME_TYPE_PFSF;
			return;
		}
	}
	/*
	 * Check for port-name as a string of the form p0
	 * (support kernel ver >= 5.0, or OFED ver >= 4.6).
	 */
	sc_items = sscanf(port_name_in, "%c%d%c",
			  &pf_c1, &port_info_out->port_name, &eol);
	if (sc_items == 2 && pf_c1 == 'p') {
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_UPLINK;
		return;
	}
	/*
	 * Check for port-name as a string of the form pf0
	 * (support kernel ver >= 5.7 for HPF representor on BF).
	 */
	sc_items = sscanf(port_name_in, "%c%c%d%c",
			  &pf_c1, &pf_c2, &port_info_out->pf_num, &eol);
	if (sc_items == 3 && pf_c1 == 'p' && pf_c2 == 'f') {
		port_info_out->port_name = -1;
		port_info_out->name_type = MLX5_PHYS_PORT_NAME_TYPE_PFHPF;
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
}

int
mlx5_get_ifname_sysfs(const char *ibdev_path, char *ifname)
{
	DIR *dir;
	struct dirent *dent;
	unsigned int dev_type = 0;
	unsigned int dev_port_prev = ~0u;
	char match[IF_NAMESIZE] = "";

	MLX5_ASSERT(ibdev_path);
	{
		MKSTR(path, "%s/device/net", ibdev_path);

		dir = opendir(path);
		if (dir == NULL) {
			rte_errno = errno;
			return -rte_errno;
		}
	}
	while ((dent = readdir(dir)) != NULL) {
		char *name = dent->d_name;
		FILE *file;
		unsigned int dev_port;
		int r;

		if ((name[0] == '.') &&
		    ((name[1] == '\0') ||
		     ((name[1] == '.') && (name[2] == '\0'))))
			continue;

		MKSTR(path, "%s/device/net/%s/%s",
		      ibdev_path, name,
		      (dev_type ? "dev_id" : "dev_port"));

		file = fopen(path, "rb");
		if (file == NULL) {
			if (errno != ENOENT)
				continue;
			/*
			 * Switch to dev_id when dev_port does not exist as
			 * is the case with Linux kernel versions < 3.15.
			 */
try_dev_id:
			match[0] = '\0';
			if (dev_type)
				break;
			dev_type = 1;
			dev_port_prev = ~0u;
			rewinddir(dir);
			continue;
		}
		r = fscanf(file, (dev_type ? "%x" : "%u"), &dev_port);
		fclose(file);
		if (r != 1)
			continue;
		/*
		 * Switch to dev_id when dev_port returns the same value for
		 * all ports. May happen when using a MOFED release older than
		 * 3.0 with a Linux kernel >= 3.15.
		 */
		if (dev_port == dev_port_prev)
			goto try_dev_id;
		dev_port_prev = dev_port;
		if (dev_port == 0)
			strlcpy(match, name, IF_NAMESIZE);
	}
	closedir(dir);
	if (match[0] == '\0') {
		rte_errno = ENOENT;
		return -rte_errno;
	}
	strncpy(ifname, match, IF_NAMESIZE);
	return 0;
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

	char const *path[] = {
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

/**
 * Initialization routine for run-time dependency on rdma-core.
 */
void
mlx5_glue_constructor(void)
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
}

/**
 * Allocate Protection Domain object and extract its pdn using DV API.
 *
 * @param[out] cdev
 *   Pointer to the mlx5 device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_pd_create(struct mlx5_common_device *cdev)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret;
#endif

	cdev->pd = mlx5_glue->alloc_pd(cdev->ctx);
	if (cdev->pd == NULL) {
		DRV_LOG(ERR, "Failed to allocate PD.");
		return errno ? -errno : -ENOMEM;
	}
	if (cdev->config.devx == 0)
		return 0;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	obj.pd.in = cdev->pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret != 0) {
		DRV_LOG(ERR, "Fail to get PD object info.");
		mlx5_glue->dealloc_pd(cdev->pd);
		cdev->pd = NULL;
		return -errno;
	}
	cdev->pdn = pd_info.pdn;
	return 0;
#else
	DRV_LOG(ERR, "Cannot get pdn - no DV support.");
	return -ENOTSUP;
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
}

static struct ibv_device *
mlx5_os_get_ibv_device(const struct rte_pci_addr *addr)
{
	int n;
	struct ibv_device **ibv_list = mlx5_glue->get_device_list(&n);
	struct ibv_device *ibv_match = NULL;
	uint8_t guid1[32] = {0};
	uint8_t guid2[32] = {0};
	int ret1, ret2 = -1;
	struct rte_pci_addr paddr;

	if (ibv_list == NULL || !n) {
		rte_errno = ENOSYS;
		if (ibv_list)
			mlx5_glue->free_device_list(ibv_list);
		return NULL;
	}
	ret1 = mlx5_get_device_guid(addr, guid1, sizeof(guid1));
	while (n-- > 0) {
		DRV_LOG(DEBUG, "Checking device \"%s\"..", ibv_list[n]->name);
		if (mlx5_get_pci_addr(ibv_list[n]->ibdev_path, &paddr) != 0)
			continue;
		if (ret1 > 0)
			ret2 = mlx5_get_device_guid(&paddr, guid2, sizeof(guid2));
		/* Bond device can bond secondary PCIe */
		if ((strstr(ibv_list[n]->name, "bond") &&
		    ((ret1 > 0 && ret2 > 0 && !memcmp(guid1, guid2, sizeof(guid1))) ||
		    (addr->domain == paddr.domain && addr->bus == paddr.bus &&
		     addr->devid == paddr.devid))) ||
		     !rte_pci_addr_cmp(addr, &paddr)) {
			ibv_match = ibv_list[n];
			break;
		}
	}
	if (ibv_match == NULL) {
		DRV_LOG(WARNING,
			"No Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			addr->domain, addr->bus, addr->devid, addr->function);
		rte_errno = ENOENT;
	}
	mlx5_glue->free_device_list(ibv_list);
	return ibv_match;
}

/* Try to disable ROCE by Netlink\Devlink. */
static int
mlx5_nl_roce_disable(const char *addr)
{
	int nlsk_fd = mlx5_nl_init(NETLINK_GENERIC, 0);
	int devlink_id;
	int enable;
	int ret;

	if (nlsk_fd < 0)
		return nlsk_fd;
	devlink_id = mlx5_nl_devlink_family_id_get(nlsk_fd);
	if (devlink_id < 0) {
		ret = devlink_id;
		DRV_LOG(DEBUG,
			"Failed to get devlink id for ROCE operations by Netlink.");
		goto close;
	}
	ret = mlx5_nl_enable_roce_get(nlsk_fd, devlink_id, addr, &enable);
	if (ret) {
		DRV_LOG(DEBUG, "Failed to get ROCE enable by Netlink: %d.",
			ret);
		goto close;
	} else if (!enable) {
		DRV_LOG(INFO, "ROCE has already disabled(Netlink).");
		goto close;
	}
	ret = mlx5_nl_enable_roce_set(nlsk_fd, devlink_id, addr, 0);
	if (ret)
		DRV_LOG(DEBUG, "Failed to disable ROCE by Netlink: %d.", ret);
	else
		DRV_LOG(INFO, "ROCE is disabled by Netlink successfully.");
close:
	close(nlsk_fd);
	return ret;
}

/* Try to disable ROCE by sysfs. */
static int
mlx5_sys_roce_disable(const char *addr)
{
	FILE *file_o;
	int enable;
	int ret;

	MKSTR(file_p, "/sys/bus/pci/devices/%s/roce_enable", addr);
	file_o = fopen(file_p, "rb");
	if (!file_o) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	ret = fscanf(file_o, "%d", &enable);
	if (ret != 1) {
		rte_errno = EINVAL;
		ret = EINVAL;
		goto close;
	} else if (!enable) {
		ret = 0;
		DRV_LOG(INFO, "ROCE has already disabled(sysfs).");
		goto close;
	}
	fclose(file_o);
	file_o = fopen(file_p, "wb");
	if (!file_o) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	fprintf(file_o, "0\n");
	ret = 0;
close:
	if (ret)
		DRV_LOG(DEBUG, "Failed to disable ROCE by sysfs: %d.", ret);
	else
		DRV_LOG(INFO, "ROCE is disabled by sysfs successfully.");
	fclose(file_o);
	return ret;
}

static int
mlx5_roce_disable(const struct rte_device *dev)
{
	char pci_addr[PCI_PRI_STR_SIZE] = { 0 };

	if (mlx5_dev_to_pci_str(dev, pci_addr, sizeof(pci_addr)) < 0)
		return -rte_errno;
	/* Firstly try to disable ROCE by Netlink and fallback to sysfs. */
	if (mlx5_nl_roce_disable(pci_addr) != 0 &&
	    mlx5_sys_roce_disable(pci_addr) != 0)
		return -rte_errno;
	return 0;
}

static struct ibv_device *
mlx5_os_get_ibv_dev(const struct rte_device *dev)
{
	struct ibv_device *ibv;

	if (mlx5_dev_is_pci(dev))
		ibv = mlx5_os_get_ibv_device(&RTE_DEV_TO_PCI_CONST(dev)->addr);
	else
		ibv = mlx5_get_aux_ibv_device(RTE_DEV_TO_AUXILIARY_CONST(dev));
	if (ibv == NULL) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "Verbs device not found: %s", dev->name);
	}
	return ibv;
}

static struct ibv_device *
mlx5_vdpa_get_ibv_dev(const struct rte_device *dev)
{
	struct ibv_device *ibv;
	int retry;

	if (mlx5_roce_disable(dev) != 0) {
		DRV_LOG(WARNING, "Failed to disable ROCE for \"%s\".",
			dev->name);
		return NULL;
	}
	/* Wait for the IB device to appear again after reload. */
	for (retry = MLX5_VDPA_MAX_RETRIES; retry > 0; --retry) {
		ibv = mlx5_os_get_ibv_dev(dev);
		if (ibv != NULL)
			return ibv;
		usleep(MLX5_VDPA_USEC);
	}
	DRV_LOG(ERR,
		"Cannot get IB device after disabling RoCE for \"%s\", retries exceed %d.",
		dev->name, MLX5_VDPA_MAX_RETRIES);
	rte_errno = EAGAIN;
	return NULL;
}

static int
mlx5_config_doorbell_mapping_env(int dbnc)
{
	char *env;
	int value;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Get environment variable to store. */
	env = getenv(MLX5_SHUT_UP_BF);
	value = env ? !!strcmp(env, "0") : MLX5_ARG_UNSET;
	if (dbnc == MLX5_ARG_UNSET)
		setenv(MLX5_SHUT_UP_BF, MLX5_SHUT_UP_BF_DEFAULT, 1);
	else
		setenv(MLX5_SHUT_UP_BF,
		       dbnc == MLX5_TXDB_NCACHED ? "1" : "0", 1);
	return value;
}

static void
mlx5_restore_doorbell_mapping_env(int value)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Restore the original environment variable state. */
	if (value == MLX5_ARG_UNSET)
		unsetenv(MLX5_SHUT_UP_BF);
	else
		setenv(MLX5_SHUT_UP_BF, value ? "1" : "0", 1);
}

/**
 * Function API to open IB device.
 *
 *
 * @param cdev
 *   Pointer to the mlx5 device.
 * @param classes
 *   Chosen classes come from device arguments.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_open_device(struct mlx5_common_device *cdev, uint32_t classes)
{
	struct ibv_device *ibv;
	struct ibv_context *ctx = NULL;
	int dbmap_env;

	if (classes & MLX5_CLASS_VDPA)
		ibv = mlx5_vdpa_get_ibv_dev(cdev->dev);
	else
		ibv = mlx5_os_get_ibv_dev(cdev->dev);
	if (!ibv)
		return -rte_errno;
	DRV_LOG(INFO, "Dev information matches for device \"%s\".", ibv->name);
	/*
	 * Configure environment variable "MLX5_BF_SHUT_UP" before the device
	 * creation. The rdma_core library checks the variable at device
	 * creation and stores the result internally.
	 */
	dbmap_env = mlx5_config_doorbell_mapping_env(cdev->config.dbnc);
	/* Try to open IB device with DV first, then usual Verbs. */
	errno = 0;
	ctx = mlx5_glue->dv_open_device(ibv);
	if (ctx) {
		cdev->config.devx = 1;
		DRV_LOG(DEBUG, "DevX is supported.");
	} else if (classes == MLX5_CLASS_ETH) {
		/* The environment variable is still configured. */
		ctx = mlx5_glue->open_device(ibv);
		if (ctx == NULL)
			goto error;
		DRV_LOG(DEBUG, "DevX is NOT supported.");
	} else {
		goto error;
	}
	/* The device is created, no need for environment. */
	mlx5_restore_doorbell_mapping_env(dbmap_env);
	/* Hint libmlx5 to use PMD allocator for data plane resources */
	mlx5_set_context_attr(cdev->dev, ctx);
	cdev->ctx = ctx;
	return 0;
error:
	rte_errno = errno ? errno : ENODEV;
	/* The device creation is failed, no need for environment. */
	mlx5_restore_doorbell_mapping_env(dbmap_env);
	DRV_LOG(ERR, "Failed to open IB device \"%s\".", ibv->name);
	return -rte_errno;
}
int
mlx5_get_device_guid(const struct rte_pci_addr *dev, uint8_t *guid, size_t len)
{
	char tmp[512];
	char cur_ifname[IF_NAMESIZE + 1];
	FILE *id_file;
	DIR *dir;
	struct dirent *ptr;
	int ret;

	if (guid == NULL || len < sizeof(u_int64_t) + 1)
		return -1;
	memset(guid, 0, len);
	snprintf(tmp, sizeof(tmp), "/sys/bus/pci/devices/%04x:%02x:%02x.%x/net",
			dev->domain, dev->bus, dev->devid, dev->function);
	dir = opendir(tmp);
	if (dir == NULL)
		return -1;
	/* Traverse to identify PF interface */
	do {
		ptr = readdir(dir);
		if (ptr == NULL || ptr->d_type != DT_DIR) {
			closedir(dir);
			return -1;
		}
	} while (strchr(ptr->d_name, '.') || strchr(ptr->d_name, '_') ||
		 strchr(ptr->d_name, 'v'));
	snprintf(cur_ifname, sizeof(cur_ifname), "%s", ptr->d_name);
	closedir(dir);
	snprintf(tmp + strlen(tmp), sizeof(tmp) - strlen(tmp),
			"/%s/phys_switch_id", cur_ifname);
	/* Older OFED like 5.3 doesn't support read */
	id_file = fopen(tmp, "r");
	if (!id_file)
		return 0;
	ret = fscanf(id_file, "%16s", guid);
	fclose(id_file);
	return ret;
}

/*
 * Create direct mkey using the kernel ibv_reg_mr API and wrap it with a new
 * indirect mkey created by the DevX API.
 * This mkey should be used for DevX commands requesting mkey as a parameter.
 */
int
mlx5_os_wrapped_mkey_create(void *ctx, void *pd, uint32_t pdn, void *addr,
			    size_t length, struct mlx5_pmd_wrapped_mr *pmd_mr)
{
	struct mlx5_klm klm = {
		.byte_count = length,
		.address = (uintptr_t)addr,
	};
	struct mlx5_devx_mkey_attr mkey_attr = {
		.pd = pdn,
		.klm_array = &klm,
		.klm_num = 1,
	};
	struct mlx5_devx_obj *mkey;
	struct ibv_mr *ibv_mr = mlx5_glue->reg_mr(pd, addr, length,
						  IBV_ACCESS_LOCAL_WRITE |
						  (haswell_broadwell_cpu ? 0 :
						  IBV_ACCESS_RELAXED_ORDERING));

	if (!ibv_mr) {
		rte_errno = errno;
		return -rte_errno;
	}
	klm.mkey = ibv_mr->lkey;
	mkey_attr.addr = (uintptr_t)addr;
	mkey_attr.size = length;
	mkey = mlx5_devx_cmd_mkey_create(ctx, &mkey_attr);
	if (!mkey) {
		claim_zero(mlx5_glue->dereg_mr(ibv_mr));
		return -rte_errno;
	}
	pmd_mr->addr = addr;
	pmd_mr->len = length;
	pmd_mr->obj = (void *)ibv_mr;
	pmd_mr->imkey = mkey;
	pmd_mr->lkey = mkey->id;
	return 0;
}

void
mlx5_os_wrapped_mkey_destroy(struct mlx5_pmd_wrapped_mr *pmd_mr)
{
	if (!pmd_mr)
		return;
	if (pmd_mr->imkey)
		claim_zero(mlx5_devx_cmd_destroy(pmd_mr->imkey));
	if (pmd_mr->obj)
		claim_zero(mlx5_glue->dereg_mr(pmd_mr->obj));
	memset(pmd_mr, 0, sizeof(*pmd_mr));
}
