/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <sys/queue.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <dlfcn.h>
#include <libgen.h>
#endif
#include <sys/stat.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <dirent.h>
#endif

#include <rte_string_fns.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_version.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <rte_telemetry.h>
#endif
#include <rte_vect.h>

#include <rte_argparse.h>
#include <eal_export.h>
#include "eal_internal_cfg.h"
#include "eal_options.h"
#include "eal_filesystem.h"
#include "eal_private.h"
#include "log_internal.h"
#ifndef RTE_EXEC_ENV_WINDOWS
#include "eal_trace.h"
#endif

#define BITS_PER_HEX 4
#define NUMA_MEM_STRLEN (RTE_MAX_NUMA_NODES * 10)

/* Allow the application to print its usage message too if set */
static rte_usage_hook_t rte_application_usage_hook;

struct arg_list_elem {
	TAILQ_ENTRY(arg_list_elem) next;
	char *arg;
};
TAILQ_HEAD(arg_list, arg_list_elem);

struct eal_init_args {
	/* define a struct member for each EAL option, member name is the same as option name.
	 * Parameters that take an argument e.g. -l, are char *,
	 * parameters that take no options e.g. --no-huge, are bool.
	 * parameters that can be given multiple times e.g. -a, are arg_lists,
	 * parameters that are optional e.g. --huge-unlink,
	 *   are char * but are set to (void *)1 if the parameter is not given.
	 * for aliases, i.e. options under different names, no field needs to be output
	 */
#define LIST_ARG(long, short, help_str, fieldname) struct arg_list fieldname;
#define STR_ARG(long, short, help_str, fieldname) char *fieldname;
#define OPT_STR_ARG(long, short, help_str, fieldname) char *fieldname;
#define BOOL_ARG(long, short, help_str, fieldname) bool fieldname;
#define STR_ALIAS(long, short, help_str, fieldname)

#define INCLUDE_ALL_ARG 1  /* for struct definition, include even unsupported values */
#include "eal_option_list.h"
};

/* define the structure itself, with initializers. Only the LIST_ARGS need init */
#define LIST_ARG(long, short, help_str, fieldname) \
	.fieldname = TAILQ_HEAD_INITIALIZER(args.fieldname),
#define STR_ARG(long, short, help_str, fieldname)
#define OPT_STR_ARG(long, short, help_str, fieldname)
#define BOOL_ARG(long, short, help_str, fieldname)
#define STR_ALIAS(long, short, help_str, fieldname)

struct eal_init_args args = {
	#include "eal_option_list.h"
};
#undef INCLUDE_ALL_ARG

/* an rte_argparse callback to append the argument to an arg_list
 * in args. The index is the offset into the struct of the list.
 */
static int
arg_list_callback(uint32_t index, const char *arg, void *init_args)
{
	struct arg_list *list = RTE_PTR_ADD(init_args, index);
	struct arg_list_elem *elem;

	elem = malloc(sizeof(*elem));
	if (elem == NULL)
		return -1;

	elem->arg = strdup(arg);
	if (elem->arg == NULL) {
		free(elem);
		return -1;
	}

	TAILQ_INSERT_TAIL(list, elem, next);
	return 0;
}

static void
eal_usage(const struct rte_argparse *obj)
{
	rte_argparse_print_help(stdout, obj);
	if (rte_application_usage_hook != NULL)
		rte_application_usage_hook(obj->prog_name);
}

/* For arguments which have an arg_list type, they use callback (no val_saver),
 * require a value, and have the SUPPORT_MULTI flag.
 */
#define LIST_ARG(long, short, help_str, fieldname) { \
	.name_long = long, \
	.name_short = short, \
	.help = help_str, \
	.val_set = (void *)offsetof(struct eal_init_args, fieldname), \
	.value_required = RTE_ARGPARSE_VALUE_REQUIRED, \
	.flags = RTE_ARGPARSE_FLAG_SUPPORT_MULTI, \
},
/* For arguments which have a string type, they use val_saver (no callback),
 * and normally REQUIRED_VALUE.
 */
#define STR_ARG(long, short, help_str, fieldname) { \
	.name_long = long, \
	.name_short = short, \
	.help = help_str, \
	.val_saver = &args.fieldname, \
	.value_required = RTE_ARGPARSE_VALUE_REQUIRED, \
	.value_type = RTE_ARGPARSE_VALUE_TYPE_STR, \
},
/* For flags which have optional arguments, they use both val_saver and val_set,
 * but still have a string type.
 */
#define OPT_STR_ARG(long, short, help_str, fieldname) { \
	.name_long = long, \
	.name_short = short, \
	.help = help_str, \
	.val_saver = &args.fieldname, \
	.val_set = (void *)1, \
	.value_required = RTE_ARGPARSE_VALUE_OPTIONAL, \
	.value_type = RTE_ARGPARSE_VALUE_TYPE_STR, \
},
/* For boolean arguments, they use val_saver and val_set, with NO_VALUE flag.
 */
#define BOOL_ARG(long, short, help_str, fieldname) { \
	.name_long = long, \
	.name_short = short, \
	.help = help_str, \
	.val_saver = &args.fieldname, \
	.val_set = (void *)1, \
	.value_required = RTE_ARGPARSE_VALUE_NONE, \
	.value_type = RTE_ARGPARSE_VALUE_TYPE_BOOL, \
},
#define STR_ALIAS STR_ARG

#if RTE_VER_RELEASE == 99
#define GUIDES_PATH "https://doc.dpdk.org/guides-" RTE_STR(RTE_VER_YEAR) "." RTE_STR(RTE_VER_MONTH)
#else
#define GUIDES_PATH "https://doc.dpdk.org/guides"
#endif

struct rte_argparse eal_argparse  = {
	.prog_name = "",
	.usage = "<DPDK EAL options> -- <App options>",
	.epilog = "For more information on EAL options, see the DPDK documentation at:\n"
			"\t" GUIDES_PATH "/" RTE_EXEC_ENV_NAME "_gsg/",
	.exit_on_error = true,
	.ignore_non_flag_args = true,
	.callback = arg_list_callback,
	.print_help = eal_usage,
	.opaque = &args,
	.args = {
		#include "eal_option_list.h"
		ARGPARSE_ARG_END(),
	}
};

/* function to call into argparse library to parse the passed argc/argv parameters
 * to the eal_init_args structure.
 */
int
eal_collate_args(int argc, char **argv)
{
	if (argc < 1 || argv == NULL || argv[0] == NULL)
		return -EINVAL;

	/* parse the arguments */
	eal_argparse.prog_name = argv[0];
	int retval = rte_argparse_parse(&eal_argparse, argc, argv);
	if (retval < 0)
		return retval;

	argv[retval - 1] = argv[0];
	return retval - 1;
}

TAILQ_HEAD(shared_driver_list, shared_driver);

/* Definition for shared object drivers. */
struct shared_driver {
	TAILQ_ENTRY(shared_driver) next;

	char    name[PATH_MAX];
	void*   lib_handle;
};

/* List of external loadable drivers */
static struct shared_driver_list solib_list =
TAILQ_HEAD_INITIALIZER(solib_list);

#ifndef RTE_EXEC_ENV_WINDOWS
/* Default path of external loadable drivers */
static const char *default_solib_dir = RTE_EAL_PMD_PATH;
#endif

/*
 * Stringified version of solib path used by dpdk-pmdinfo.py
 * Note: PLEASE DO NOT ALTER THIS without making a corresponding
 * change to usertools/dpdk-pmdinfo.py
 */
RTE_PMD_EXPORT_SYMBOL(const char, dpdk_solib_path)[] =
"DPDK_PLUGIN_PATH=" RTE_EAL_PMD_PATH;

TAILQ_HEAD(device_option_list, device_option);

struct device_option {
	TAILQ_ENTRY(device_option) next;

	enum rte_devtype type;
	char arg[];
};

static struct device_option_list devopt_list =
TAILQ_HEAD_INITIALIZER(devopt_list);

static int main_lcore_parsed;
static int core_parsed;

/* Returns rte_usage_hook_t */
rte_usage_hook_t
eal_get_application_usage_hook(void)
{
	return rte_application_usage_hook;
}

/* Set a per-application usage message */
RTE_EXPORT_SYMBOL(rte_set_application_usage_hook)
rte_usage_hook_t
rte_set_application_usage_hook(rte_usage_hook_t usage_func)
{
	rte_usage_hook_t old_func;

	/* Will be NULL on the first call to denote the last usage routine. */
	old_func = rte_application_usage_hook;
	rte_application_usage_hook = usage_func;

	return old_func;
}

#ifdef RTE_EXEC_ENV_WINDOWS
int
eal_save_args(__rte_unused int argc, __rte_unused char **argv)
{
	return 0;
}
#else /* RTE_EXEC_ENV_WINDOWS */
static char **eal_args;
static char **eal_app_args;

#define EAL_PARAM_REQ "/eal/params"
#define EAL_APP_PARAM_REQ "/eal/app_params"

/* callback handler for telemetry library to report out EAL flags */
int
handle_eal_info_request(const char *cmd, const char *params __rte_unused,
		struct rte_tel_data *d)
{
	char **args;
	int used = 0;
	int i = 0;

	if (strcmp(cmd, EAL_PARAM_REQ) == 0)
		args = eal_args;
	else
		args = eal_app_args;

	rte_tel_data_start_array(d, RTE_TEL_STRING_VAL);
	if (args == NULL || args[0] == NULL)
		return 0;

	for ( ; args[i] != NULL; i++)
		used = rte_tel_data_add_array_string(d, args[i]);
	return used;
}

int
eal_save_args(int argc, char **argv)
{
	int i, j;

	rte_telemetry_register_cmd(EAL_PARAM_REQ, handle_eal_info_request,
			"Returns EAL commandline parameters used. Takes no parameters");
	rte_telemetry_register_cmd(EAL_APP_PARAM_REQ, handle_eal_info_request,
			"Returns app commandline parameters used. Takes no parameters");

	/* clone argv to report out later. We overprovision, but
	 * this does not waste huge amounts of memory
	 */
	eal_args = calloc(argc + 1, sizeof(*eal_args));
	if (eal_args == NULL)
		return -1;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0)
			break;
		eal_args[i] = strdup(argv[i]);
		if (eal_args[i] == NULL)
			goto error;
	}
	eal_args[i++] = NULL; /* always finish with NULL */

	/* allow reporting of any app args we know about too */
	if (i >= argc)
		return 0;

	eal_app_args = calloc(argc - i + 1, sizeof(*eal_args));
	if (eal_app_args == NULL)
		goto error;

	for (j = 0; i < argc; j++, i++) {
		eal_app_args[j] = strdup(argv[i]);
		if (eal_app_args[j] == NULL)
			goto error;
	}
	eal_app_args[j] = NULL;

	return 0;

error:
	if (eal_app_args != NULL) {
		i = 0;
		while (eal_app_args[i] != NULL)
			free(eal_app_args[i++]);
		free(eal_app_args);
		eal_app_args = NULL;
	}
	i = 0;
	while (eal_args[i] != NULL)
		free(eal_args[i++]);
	free(eal_args);
	eal_args = NULL;
	return -1;
}
#endif /* !RTE_EXEC_ENV_WINDOWS */

static int
eal_option_device_add(enum rte_devtype type, const char *optarg)
{
	struct device_option *devopt;
	size_t optlen;
	int ret;

	optlen = strlen(optarg) + 1;
	devopt = calloc(1, sizeof(*devopt) + optlen);
	if (devopt == NULL) {
		EAL_LOG(ERR, "Unable to allocate device option");
		return -ENOMEM;
	}

	devopt->type = type;
	ret = strlcpy(devopt->arg, optarg, optlen);
	if (ret < 0) {
		EAL_LOG(ERR, "Unable to copy device option");
		free(devopt);
		return -EINVAL;
	}
	TAILQ_INSERT_TAIL(&devopt_list, devopt, next);
	return 0;
}

int
eal_option_device_parse(void)
{
	struct device_option *devopt;
	void *tmp;
	int ret = 0;

	RTE_TAILQ_FOREACH_SAFE(devopt, &devopt_list, next, tmp) {
		if (ret == 0) {
			ret = rte_devargs_add(devopt->type, devopt->arg);
			if (ret)
				EAL_LOG(ERR, "Unable to parse device '%s'",
					devopt->arg);
		}
		TAILQ_REMOVE(&devopt_list, devopt, next);
		free(devopt);
	}
	return ret;
}

const char *
eal_get_hugefile_prefix(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (internal_conf->hugefile_prefix != NULL)
		return internal_conf->hugefile_prefix;
	return HUGEFILE_PREFIX_DEFAULT;
}

void
eal_reset_internal_config(struct internal_config *internal_cfg)
{
	int i;

	internal_cfg->memory = 0;
	internal_cfg->force_nrank = 0;
	internal_cfg->force_nchannel = 0;
	internal_cfg->hugefile_prefix = NULL;
	internal_cfg->hugepage_dir = NULL;
	internal_cfg->hugepage_file.unlink_before_mapping = false;
	internal_cfg->hugepage_file.unlink_existing = true;
	internal_cfg->force_numa = 0;
	/* zero out the NUMA config */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		internal_cfg->numa_mem[i] = 0;
	internal_cfg->force_numa_limits = 0;
	/* zero out the NUMA limits config */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		internal_cfg->numa_limit[i] = 0;
	/* zero out hugedir descriptors */
	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++) {
		memset(&internal_cfg->hugepage_info[i], 0,
				sizeof(internal_cfg->hugepage_info[0]));
		internal_cfg->hugepage_info[i].lock_descriptor = -1;
	}
	internal_cfg->base_virtaddr = 0;

	/* if set to NONE, interrupt mode is determined automatically */
	internal_cfg->vfio_intr_mode = RTE_INTR_MODE_NONE;
	memset(internal_cfg->vfio_vf_token, 0,
			sizeof(internal_cfg->vfio_vf_token));

#ifdef RTE_LIBEAL_USE_HPET
	internal_cfg->no_hpet = 0;
#else
	internal_cfg->no_hpet = 1;
#endif
	internal_cfg->vmware_tsc_map = 0;
	internal_cfg->create_uio_dev = 0;
	internal_cfg->iova_mode = RTE_IOVA_DC;
	internal_cfg->user_mbuf_pool_ops_name = NULL;
	CPU_ZERO(&internal_cfg->ctrl_cpuset);
	internal_cfg->init_complete = 0;
	internal_cfg->max_simd_bitwidth.bitwidth = RTE_VECT_DEFAULT_SIMD_BITWIDTH;
	internal_cfg->max_simd_bitwidth.forced = 0;
}

static int
eal_plugin_add(const char *path)
{
	struct shared_driver *solib;

	solib = malloc(sizeof(*solib));
	if (solib == NULL) {
		EAL_LOG(ERR, "malloc(solib) failed");
		return -1;
	}
	memset(solib, 0, sizeof(*solib));
	strlcpy(solib->name, path, PATH_MAX);
	TAILQ_INSERT_TAIL(&solib_list, solib, next);

	return 0;
}

#ifdef RTE_EXEC_ENV_WINDOWS
int
eal_plugins_init(void)
{
	return 0;
}
#else

static bool
ends_with(const char *str, const char *tail)
{
	size_t tail_len = strlen(tail);
	size_t str_len = strlen(str);

	return str_len >= tail_len && strcmp(&str[str_len - tail_len], tail) == 0;
}

static int
eal_plugindir_init(const char *path)
{
	struct dirent *dent = NULL;
	char sopath[PATH_MAX];
	DIR *d = NULL;

	if (path == NULL || *path == '\0')
		return 0;

	d = opendir(path);
	if (d == NULL) {
		EAL_LOG(ERR, "failed to open directory %s: %s",
			path, strerror(errno));
		return -1;
	}

	while ((dent = readdir(d)) != NULL) {
		struct stat sb;

		if (!ends_with(dent->d_name, ".so") && !ends_with(dent->d_name, ".so."ABI_VERSION))
			continue;

		snprintf(sopath, sizeof(sopath), "%s/%s", path, dent->d_name);

		/* if a regular file, add to list to load */
		if (!(stat(sopath, &sb) == 0 && S_ISREG(sb.st_mode)))
			continue;

		if (eal_plugin_add(sopath) == -1)
			break;
	}

	closedir(d);
	/* XXX this ignores failures from readdir() itself */
	return (dent == NULL) ? 0 : -1;
}

static int
verify_perms(const char *dirpath)
{
	struct stat st;

	/* if not root, check down one level first */
	if (strcmp(dirpath, "/") != 0) {
		static __thread char last_dir_checked[PATH_MAX];
		char copy[PATH_MAX];
		const char *dir;

		strlcpy(copy, dirpath, PATH_MAX);
		dir = dirname(copy);
		if (strncmp(dir, last_dir_checked, PATH_MAX) != 0) {
			if (verify_perms(dir) != 0)
				return -1;
			strlcpy(last_dir_checked, dir, PATH_MAX);
		}
	}

	/* call stat to check for permissions and ensure not world writable */
	if (stat(dirpath, &st) != 0) {
		EAL_LOG(ERR, "Error with stat on %s, %s",
				dirpath, strerror(errno));
		return -1;
	}
	if (st.st_mode & S_IWOTH) {
		EAL_LOG(ERR,
				"Error, directory path %s is world-writable and insecure",
				dirpath);
		return -1;
	}

	return 0;
}

static void *
eal_dlopen(const char *pathname)
{
	void *retval = NULL;
	char *realp = realpath(pathname, NULL);

	if (realp == NULL && errno == ENOENT) {
		/* not a full or relative path, try a load from system dirs */
		retval = dlopen(pathname, RTLD_NOW);
		if (retval == NULL)
			EAL_LOG(ERR, "%s", dlerror());
		return retval;
	}
	if (realp == NULL) {
		EAL_LOG(ERR, "Error with realpath for %s, %s",
				pathname, strerror(errno));
		goto out;
	}
	if (strnlen(realp, PATH_MAX) == PATH_MAX) {
		EAL_LOG(ERR, "Error, driver path greater than PATH_MAX");
		goto out;
	}

	/* do permissions checks */
	if (verify_perms(realp) != 0)
		goto out;

	retval = dlopen(realp, RTLD_NOW);
	if (retval == NULL)
		EAL_LOG(ERR, "%s", dlerror());
out:
	free(realp);
	return retval;
}

static int
is_shared_build(void)
{
#define EAL_SO "librte_eal.so"
	char soname[32];
	size_t len, minlen = strlen(EAL_SO);

	len = strlcpy(soname, EAL_SO"."ABI_VERSION, sizeof(soname));
	if (len > sizeof(soname)) {
		EAL_LOG(ERR, "Shared lib name too long in shared build check");
		len = sizeof(soname) - 1;
	}

	while (len >= minlen) {
		void *handle;

		/* check if we have this .so loaded, if so - shared build */
		EAL_LOG(DEBUG, "Checking presence of .so '%s'", soname);
		handle = dlopen(soname, RTLD_LAZY | RTLD_NOLOAD);
		if (handle != NULL) {
			EAL_LOG(INFO, "Detected shared linkage of DPDK");
			dlclose(handle);
			return 1;
		}

		/* remove any version numbers off the end to retry */
		while (len-- > 0)
			if (soname[len] == '.') {
				soname[len] = '\0';
				break;
			}
	}

	EAL_LOG(INFO, "Detected static linkage of DPDK");
	return 0;
}

int
eal_plugins_init(void)
{
	struct shared_driver *solib = NULL;
	struct stat sb;

	/* If we are not statically linked, add default driver loading
	 * path if it exists as a directory.
	 * (Using dlopen with NOLOAD flag on EAL, will return NULL if the EAL
	 * shared library is not already loaded i.e. it's statically linked.)
	 */
	if (is_shared_build() &&
			*default_solib_dir != '\0' &&
			stat(default_solib_dir, &sb) == 0 &&
			S_ISDIR(sb.st_mode))
		eal_plugin_add(default_solib_dir);

	TAILQ_FOREACH(solib, &solib_list, next) {

		if (stat(solib->name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
			if (eal_plugindir_init(solib->name) == -1) {
				EAL_LOG(ERR,
					"Cannot init plugin directory %s",
					solib->name);
				return -1;
			}
		} else {
			EAL_LOG(DEBUG, "open shared lib %s",
				solib->name);
			solib->lib_handle = eal_dlopen(solib->name);
			if (solib->lib_handle == NULL)
				return -1;
		}

	}
	return 0;
}
#endif

/*
 * Parse the coremask given as argument (hexadecimal string) and fill
 * the global configuration (core role and core count) with the parsed
 * value.
 */
static int xdigit2val(unsigned char c)
{
	int val;

	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;
	return val;
}

static int
eal_parse_service_coremask(const char *coremask)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, j, idx = 0;
	unsigned int count = 0;
	char c;
	int val;
	uint32_t taken_lcore_count = 0;

	EAL_LOG(WARNING, "'-s <service-coremask>' is deprecated, and will be removed in a future release.");
	EAL_LOG(WARNING, "\tUse '-S <service-corelist>' option instead.");

	if (coremask == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;

	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < RTE_MAX_LCORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX && idx < RTE_MAX_LCORE;
				j++, idx++) {
			if ((1 << j) & val) {
				/* handle main lcore already parsed */
				uint32_t lcore = idx;
				if (main_lcore_parsed &&
						cfg->main_lcore == lcore) {
					EAL_LOG(ERR,
						"lcore %u is main lcore, cannot use as service core",
						idx);
					return -1;
				}

				if (eal_cpu_detected(idx) == 0) {
					EAL_LOG(ERR,
						"lcore %u unavailable", idx);
					return -1;
				}

				if (cfg->lcore_role[idx] == ROLE_RTE)
					taken_lcore_count++;

				lcore_config[idx].core_role = ROLE_SERVICE;
				count++;
			}
		}
	}

	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;

	for (; idx < RTE_MAX_LCORE; idx++)
		lcore_config[idx].core_index = -1;

	if (count == 0)
		return -1;

	if (core_parsed && taken_lcore_count != count) {
		EAL_LOG(WARNING,
			"Not all service cores are in the coremask. "
			"Please ensure -c or -l includes service cores");
	}

	cfg->service_lcore_count = count;
	return 0;
}

static int
update_lcore_config(int *cores)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	unsigned int count = 0;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (cores[i] != -1) {
			if (eal_cpu_detected(i) == 0) {
				EAL_LOG(ERR, "lcore %u unavailable", i);
				ret = -1;
				continue;
			}
			cfg->lcore_role[i] = ROLE_RTE;
			count++;
		} else {
			cfg->lcore_role[i] = ROLE_OFF;
		}
		lcore_config[i].core_index = cores[i];
	}
	if (!ret)
		cfg->lcore_count = count;
	return ret;
}

static int
check_core_list(int *lcores, unsigned int count)
{
	char lcorestr[RTE_MAX_LCORE * 10];
	bool overflow = false;
	int len = 0, ret;
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (lcores[i] < RTE_MAX_LCORE)
			continue;

		EAL_LOG(ERR, "lcore %d >= RTE_MAX_LCORE (%d)",
			lcores[i], RTE_MAX_LCORE);
		overflow = true;
	}
	if (!overflow)
		return 0;

	/*
	 * We've encountered a core that's greater than RTE_MAX_LCORE,
	 * suggest using --lcores option to map lcores onto physical cores
	 * greater than RTE_MAX_LCORE.
	 */
	for (i = 0; i < count; i++) {
		ret = snprintf(&lcorestr[len], sizeof(lcorestr) - len,
			"%d@%d,", i, lcores[i]);
		if (ret > 0)
			len = len + ret;
	}
	if (len > 0)
		lcorestr[len - 1] = 0;
	EAL_LOG(ERR, "To use high physical core ids, "
		"please use --lcores to map them to lcore ids below RTE_MAX_LCORE, "
		"e.g. --lcores %s", lcorestr);
	return -1;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eal_parse_coremask)
int
rte_eal_parse_coremask(const char *coremask, int *cores)
{
	const char *coremask_orig = coremask;
	int lcores[RTE_MAX_LCORE];
	unsigned int count = 0;
	int i, j, idx;
	int val;
	char c;

	for (idx = 0; idx < RTE_MAX_LCORE; idx++)
		cores[idx] = -1;
	idx = 0;

	EAL_LOG(WARNING, "'-c <coremask>' option is deprecated, and will be removed in a future release");
	EAL_LOG(WARNING, "\tUse '-l <corelist>' or '--lcores=<corelist>' option instead");

	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;
	if (i == 0) {
		EAL_LOG(ERR, "No lcores in coremask: [%s]",
			coremask_orig);
		return -1;
	}

	for (i = i - 1; i >= 0; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			EAL_LOG(ERR, "invalid characters in coremask: [%s]",
				coremask_orig);
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX; j++, idx++)
		{
			if ((1 << j) & val) {
				if (count >= RTE_MAX_LCORE) {
					EAL_LOG(ERR, "Too many lcores provided. Cannot exceed RTE_MAX_LCORE (%d)",
						RTE_MAX_LCORE);
					return -1;
				}
				lcores[count++] = idx;
			}
		}
	}
	if (count == 0) {
		EAL_LOG(ERR, "No lcores in coremask: [%s]",
			coremask_orig);
		return -1;
	}

	if (check_core_list(lcores, count))
		return -1;

	/*
	 * Now that we've got a list of cores no longer than RTE_MAX_LCORE,
	 * and no lcore in that list is greater than RTE_MAX_LCORE, populate
	 * the cores array.
	 */
	do {
		count--;
		cores[lcores[count]] = count;
	} while (count != 0);

	return 0;
}

static int
eal_parse_service_corelist(const char *corelist)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i;
	unsigned count = 0;
	char *end = NULL;
	uint32_t min, max, idx;
	uint32_t taken_lcore_count = 0;

	if (corelist == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*corelist))
		corelist++;
	i = strlen(corelist);
	while ((i > 0) && isblank(corelist[i - 1]))
		i--;

	/* Get list of cores */
	min = RTE_MAX_LCORE;
	do {
		while (isblank(*corelist))
			corelist++;
		if (*corelist == '\0')
			return -1;
		errno = 0;
		idx = strtoul(corelist, &end, 10);
		if (errno || end == NULL)
			return -1;
		if (idx >= RTE_MAX_LCORE)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = min; idx <= max; idx++) {
				if (cfg->lcore_role[idx] != ROLE_SERVICE) {
					/* handle main lcore already parsed */
					uint32_t lcore = idx;
					if (cfg->main_lcore == lcore &&
							main_lcore_parsed) {
						EAL_LOG(ERR,
							"Error: lcore %u is main lcore, cannot use as service core",
							idx);
						return -1;
					}
					if (cfg->lcore_role[idx] == ROLE_RTE)
						taken_lcore_count++;

					lcore_config[idx].core_role =
							ROLE_SERVICE;
					count++;
				}
			}
			min = RTE_MAX_LCORE;
		} else
			return -1;
		corelist = end + 1;
	} while (*end != '\0');

	if (count == 0)
		return -1;

	if (core_parsed && taken_lcore_count != count) {
		EAL_LOG(WARNING,
			"Not all service cores were in the coremask. "
			"Please ensure -c or -l includes service cores");
	}

	return 0;
}

/* Changes the lcore id of the main thread */
static int
eal_parse_main_lcore(const char *arg)
{
	char *parsing_end;
	struct rte_config *cfg = rte_eal_get_configuration();

	errno = 0;
	cfg->main_lcore = (uint32_t) strtol(arg, &parsing_end, 0);
	if (errno || parsing_end[0] != 0)
		return -1;
	if (cfg->main_lcore >= RTE_MAX_LCORE)
		return -1;
	main_lcore_parsed = 1;

	/* ensure main core is not used as service core */
	if (lcore_config[cfg->main_lcore].core_role == ROLE_SERVICE) {
		EAL_LOG(ERR,
			"Error: Main lcore is used as a service core");
		return -1;
	}

	return 0;
}

/*
 * Parse elem, the elem could be single number/range or '(' ')' group
 * 1) A single number elem, it's just a simple digit. e.g. 9
 * 2) A single range elem, two digits with a '-' between. e.g. 2-6
 * 3) A group elem, combines multiple 1) or 2) with '( )'. e.g (0,2-4,6)
 *    Within group elem, '-' used for a range separator;
 *                       ',' used for a single number.
 */
static int
eal_parse_set(const char *input, rte_cpuset_t *set)
{
	unsigned idx;
	const char *str = input;
	char *end = NULL;
	unsigned min, max;

	CPU_ZERO(set);

	while (isblank(*str))
		str++;

	/* only digit or left bracket is qualify for start point */
	if ((!isdigit(*str) && *str != '(') || *str == '\0')
		return -1;

	/* process single number or single range of number */
	if (*str != '(') {
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= CPU_SETSIZE)
			return -1;
		else {
			while (isblank(*end))
				end++;

			min = idx;
			max = idx;
			if (*end == '-') {
				/* process single <number>-<number> */
				end++;
				while (isblank(*end))
					end++;
				if (!isdigit(*end))
					return -1;

				errno = 0;
				idx = strtoul(end, &end, 10);
				if (errno || end == NULL || idx >= CPU_SETSIZE)
					return -1;
				max = idx;
				while (isblank(*end))
					end++;
				if (*end != ',' && *end != '\0')
					return -1;
			}

			if (*end != ',' && *end != '\0' &&
			    *end != '@')
				return -1;

			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				CPU_SET(idx, set);

			return end - input;
		}
	}

	/* process set within bracket */
	str++;
	while (isblank(*str))
		str++;
	if (*str == '\0')
		return -1;

	min = RTE_MAX_LCORE;
	do {

		/* go ahead to the first digit */
		while (isblank(*str))
			str++;
		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= CPU_SETSIZE)
			return -1;

		/* go ahead to separator '-',',' and ')' */
		while (isblank(*end))
			end++;
		if (*end == '-') {
			if (min == RTE_MAX_LCORE)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if ((*end == ',') || (*end == ')')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				CPU_SET(idx, set);

			min = RTE_MAX_LCORE;
		} else
			return -1;

		str = end + 1;
	} while (*end != '\0' && *end != ')');

	/*
	 * to avoid failure that tail blank makes end character check fail
	 * in eal_parse_lcores( )
	 */
	while (isblank(*str))
		str++;

	return str - input;
}

static int
check_cpuset(rte_cpuset_t *set)
{
	unsigned int idx;

	for (idx = 0; idx < CPU_SETSIZE; idx++) {
		if (!CPU_ISSET(idx, set))
			continue;

		if (eal_cpu_detected(idx) == 0) {
			EAL_LOG(ERR, "core %u "
				"unavailable", idx);
			return -1;
		}
	}
	return 0;
}

/*
 * The format pattern: --lcores='<lcores[@cpus]>[<,lcores[@cpus]>...]'
 * lcores, cpus could be a single digit/range or a group.
 * '(' and ')' are necessary if it's a group.
 * If not supply '@cpus', the value of cpus uses the same as lcores.
 * e.g. '1,2@(5-7),(3-5)@(0,2),(0,6),7-8' means start 9 EAL thread as below
 *   lcore 0 runs on cpuset 0x41 (cpu 0,6)
 *   lcore 1 runs on cpuset 0x2 (cpu 1)
 *   lcore 2 runs on cpuset 0xe0 (cpu 5,6,7)
 *   lcore 3,4,5 runs on cpuset 0x5 (cpu 0,2)
 *   lcore 6 runs on cpuset 0x41 (cpu 0,6)
 *   lcore 7 runs on cpuset 0x80 (cpu 7)
 *   lcore 8 runs on cpuset 0x100 (cpu 8)
 */
static int
eal_parse_lcores(const char *lcores)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	rte_cpuset_t lcore_set;
	unsigned int set_count;
	unsigned idx = 0;
	unsigned count = 0;
	const char *lcore_start = NULL;
	const char *end = NULL;
	int offset;
	rte_cpuset_t cpuset;
	int lflags;
	int ret = -1;

	if (lcores == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*lcores))
		lcores++;

	CPU_ZERO(&cpuset);

	/* Reset lcore config */
	for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
		cfg->lcore_role[idx] = ROLE_OFF;
		lcore_config[idx].core_index = -1;
		CPU_ZERO(&lcore_config[idx].cpuset);
	}

	/* Get list of cores */
	do {
		while (isblank(*lcores))
			lcores++;
		if (*lcores == '\0')
			goto err;

		lflags = 0;

		/* record lcore_set start point */
		lcore_start = lcores;

		/* go across a complete bracket */
		if (*lcore_start == '(') {
			lcores += strcspn(lcores, ")");
			if (*lcores++ == '\0')
				goto err;
		}

		/* scan the separator '@', ','(next) or '\0'(finish) */
		lcores += strcspn(lcores, "@,");

		if (*lcores == '@') {
			/* explicit assign cpuset and update the end cursor */
			offset = eal_parse_set(lcores + 1, &cpuset);
			if (offset < 0)
				goto err;
			end = lcores + 1 + offset;
		} else { /* ',' or '\0' */
			/* haven't given cpuset, current loop done */
			end = lcores;

			/* go back to check <number>-<number> */
			offset = strcspn(lcore_start, "(-");
			if (offset < (end - lcore_start) &&
			    *(lcore_start + offset) != '(')
				lflags = 1;
		}

		if (*end != ',' && *end != '\0')
			goto err;

		/* parse lcore_set from start point */
		if (eal_parse_set(lcore_start, &lcore_set) < 0)
			goto err;

		/* without '@', by default using lcore_set as cpuset */
		if (*lcores != '@')
			rte_memcpy(&cpuset, &lcore_set, sizeof(cpuset));

		set_count = CPU_COUNT(&lcore_set);
		/* start to update lcore_set */
		for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
			if (!CPU_ISSET(idx, &lcore_set))
				continue;
			set_count--;

			if (cfg->lcore_role[idx] != ROLE_RTE) {
				lcore_config[idx].core_index = count;
				cfg->lcore_role[idx] = ROLE_RTE;
				count++;
			}

			if (lflags) {
				CPU_ZERO(&cpuset);
				CPU_SET(idx, &cpuset);
			}

			if (check_cpuset(&cpuset) < 0)
				goto err;
			rte_memcpy(&lcore_config[idx].cpuset, &cpuset,
				   sizeof(rte_cpuset_t));
		}

		/* some cores from the lcore_set can't be handled by EAL */
		if (set_count != 0)
			goto err;

		lcores = end + 1;
	} while (*end != '\0');

	if (count == 0)
		goto err;

	cfg->lcore_count = count;
	ret = 0;

err:

	return ret;
}

static void
eal_log_usage(void)
{
	unsigned int level;

	printf("Log type is a pattern matching items of this list"
			" (plugins may be missing):\n");
	rte_log_list_types(stdout, "\t");
	printf("\n");
	printf("Syntax using globbing pattern:     ");
	printf("--"OPT_LOG_LEVEL" pattern:level\n");
	printf("Syntax using regular expression:   ");
	printf("--"OPT_LOG_LEVEL" regexp,level\n");
	printf("Syntax for the global level:       ");
	printf("--"OPT_LOG_LEVEL" level\n");
	printf("Logs are emitted if allowed by both global and specific levels.\n");
	printf("\n");
	printf("Log level can be a number or the first letters of its name:\n");
	for (level = 1; level <= RTE_LOG_MAX; level++)
		printf("\t%d   %s\n", level, eal_log_level2str(level));
}

static int
eal_parse_log_priority(const char *level)
{
	size_t len = strlen(level);
	unsigned long tmp;
	char *end;
	unsigned int i;

	if (len == 0)
		return -1;

	/* look for named values, skip 0 which is not a valid level */
	for (i = 1; i <= RTE_LOG_MAX; i++) {
		if (strncmp(eal_log_level2str(i), level, len) == 0)
			return i;
	}

	/* not a string, maybe it is numeric */
	errno = 0;
	tmp = strtoul(level, &end, 0);

	/* check for errors */
	if (errno != 0 || end == NULL || *end != '\0' ||
	    tmp >= UINT32_MAX)
		return -1;

	return tmp;
}

static int
eal_parse_log_level(const char *arg)
{
	const char *pattern = NULL;
	const char *regex = NULL;
	char *str, *level;
	int priority;

	if (strcmp(arg, "help") == 0) {
		eal_log_usage();
		exit(EXIT_SUCCESS);
	}

	str = strdup(arg);
	if (str == NULL)
		return -1;

	if ((level = strchr(str, ','))) {
		regex = str;
		*level++ = '\0';
	} else if ((level = strchr(str, ':'))) {
		pattern = str;
		*level++ = '\0';
	} else {
		level = str;
	}

	priority = eal_parse_log_priority(level);
	if (priority <= 0) {
		fprintf(stderr, "Invalid log level: %s\n", level);
		goto fail;
	}
	if (priority > (int)RTE_LOG_MAX) {
		fprintf(stderr, "Log level %d higher than maximum (%d)\n",
				priority, RTE_LOG_MAX);
		priority = RTE_LOG_MAX;
	}

	if (regex) {
		if (rte_log_set_level_regexp(regex, priority) < 0) {
			fprintf(stderr, "cannot set log level %s,%d\n",
				regex, priority);
			goto fail;
		}
		if (eal_log_save_regexp(regex, priority) < 0)
			goto fail;
	} else if (pattern) {
		if (rte_log_set_level_pattern(pattern, priority) < 0) {
			fprintf(stderr, "cannot set log level %s:%d\n",
				pattern, priority);
			goto fail;
		}
		if (eal_log_save_pattern(pattern, priority) < 0)
			goto fail;
	} else {
		rte_log_set_global_level(priority);
	}

	free(str);
	return 0;

fail:
	free(str);
	return -1;
}

static enum rte_proc_type_t
eal_parse_proc_type(const char *arg)
{
	if (strncasecmp(arg, "primary", sizeof("primary")) == 0)
		return RTE_PROC_PRIMARY;
	if (strncasecmp(arg, "secondary", sizeof("secondary")) == 0)
		return RTE_PROC_SECONDARY;
	if (strncasecmp(arg, "auto", sizeof("auto")) == 0)
		return RTE_PROC_AUTO;

	return RTE_PROC_INVALID;
}

static int
eal_parse_iova_mode(const char *name)
{
	int mode;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (name == NULL)
		return -1;

	if (!strcmp("pa", name))
		mode = RTE_IOVA_PA;
	else if (!strcmp("va", name))
		mode = RTE_IOVA_VA;
	else
		return -1;

	internal_conf->iova_mode = mode;
	return 0;
}

static int
eal_parse_simd_bitwidth(const char *arg)
{
	char *end;
	unsigned long bitwidth;
	int ret;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (arg == NULL || arg[0] == '\0')
		return -1;

	errno = 0;
	bitwidth = strtoul(arg, &end, 0);

	/* check for errors */
	if (errno != 0 || end == NULL || *end != '\0' || bitwidth > RTE_VECT_SIMD_MAX)
		return -1;

	if (bitwidth == 0)
		bitwidth = (unsigned long) RTE_VECT_SIMD_MAX;
	ret = rte_vect_set_max_simd_bitwidth(bitwidth);
	if (ret < 0)
		return -1;
	internal_conf->max_simd_bitwidth.forced = 1;
	return 0;
}

static int
eal_parse_base_virtaddr(const char *arg)
{
	char *end;
	uint64_t addr;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	errno = 0;
	addr = strtoull(arg, &end, 16);

	/* check for errors */
	if ((errno != 0) || (arg[0] == '\0') || end == NULL || (*end != '\0'))
		return -1;

	/* make sure we don't exceed 32-bit boundary on 32-bit target */
#ifndef RTE_ARCH_64
	if (addr >= UINTPTR_MAX)
		return -1;
#endif

	/* align the addr on 16M boundary, 16MB is the minimum huge page
	 * size on IBM Power architecture. If the addr is aligned to 16MB,
	 * it can align to 2MB for x86. So this alignment can also be used
	 * on x86 and other architectures.
	 */
	internal_conf->base_virtaddr =
		RTE_PTR_ALIGN_CEIL((uintptr_t)addr, (size_t)RTE_PGSIZE_16M);

	return 0;
}

/* caller is responsible for freeing the returned string */
static char *
available_cores(void)
{
	char *str = NULL;
	int previous;
	int sequence;
	char *tmp;
	int idx;

	/* find the first available cpu */
	for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
		if (eal_cpu_detected(idx) == 0)
			continue;
		break;
	}
	if (idx >= RTE_MAX_LCORE)
		return NULL;

	/* first sequence */
	if (asprintf(&str, "%d", idx) < 0)
		return NULL;
	previous = idx;
	sequence = 0;

	for (idx++ ; idx < RTE_MAX_LCORE; idx++) {
		if (eal_cpu_detected(idx) == 0)
			continue;

		if (idx == previous + 1) {
			previous = idx;
			sequence = 1;
			continue;
		}

		/* finish current sequence */
		if (sequence) {
			if (asprintf(&tmp, "%s-%d", str, previous) < 0) {
				free(str);
				return NULL;
			}
			free(str);
			str = tmp;
		}

		/* new sequence */
		if (asprintf(&tmp, "%s,%d", str, idx) < 0) {
			free(str);
			return NULL;
		}
		free(str);
		str = tmp;
		previous = idx;
		sequence = 0;
	}

	/* finish last sequence */
	if (sequence) {
		if (asprintf(&tmp, "%s-%d", str, previous) < 0) {
			free(str);
			return NULL;
		}
		free(str);
		str = tmp;
	}

	return str;
}

#define HUGE_UNLINK_NEVER "never"

static int
eal_parse_huge_unlink(const char *arg, struct hugepage_file_discipline *out)
{
	if (arg == NULL || strcmp(arg, "always") == 0) {
		out->unlink_before_mapping = true;
		return 0;
	}
	if (strcmp(arg, "existing") == 0) {
		/* same as not specifying the option */
		return 0;
	}
	if (strcmp(arg, HUGE_UNLINK_NEVER) == 0) {
		EAL_LOG(WARNING, "Using --"OPT_HUGE_UNLINK"="
			HUGE_UNLINK_NEVER" may create data leaks.");
		out->unlink_existing = false;
		return 0;
	}
	return -1;
}

/* Parse all arguments looking for log related ones */
int
eal_parse_log_options(void)
{
	struct arg_list_elem *arg;
	TAILQ_FOREACH(arg, &args.log_level, next) {
		if (eal_parse_log_level(arg->arg) < 0) {
			EAL_LOG(ERR, "invalid log-level parameter");
			return -1;
		}
	}
	if (args.log_color != NULL) {
		/* if value is 1, no argument specified, so pass NULL */
		if (args.log_color == (void *)1)
			args.log_color = NULL;
		if (eal_log_color(args.log_color) < 0) {
			EAL_LOG(ERR, "invalid log-color parameter");
			return -1;
		}
	}
	if (args.log_timestamp != NULL) {
		/* similarly log_timestamp may be 1 */
		if (args.log_timestamp == (void *)1)
			args.log_timestamp = NULL;
		if (eal_log_timestamp(args.log_timestamp) < 0) {
			EAL_LOG(ERR, "invalid log-timestamp parameter");
			return -1;
		}
	}
	if (args.syslog != NULL) {
#ifdef RTE_EXEC_ENV_WINDOWS
		EAL_LOG(WARNING, "syslog is not supported on Windows, ignoring parameter");
#else
		/* also syslog parameter may be 1 */
		if (args.syslog == (void *)1)
			args.syslog = NULL;
		if (eal_log_syslog(args.syslog) < 0) {
			EAL_LOG(ERR, "invalid syslog parameter");
			return -1;
		}
#endif
	}
	return 0;
}

static int
eal_parse_socket_arg(char *strval, volatile uint64_t *socket_arg)
{
	char *arg[RTE_MAX_NUMA_NODES];
	char *end;
	int arg_num, i, len;

	len = strnlen(strval, NUMA_MEM_STRLEN);
	if (len == NUMA_MEM_STRLEN) {
		EAL_LOG(ERR, "--numa-mem/--socket-mem parameter is too long");
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
	static struct {
		const char *name;
		enum rte_intr_mode value;
	} map[] = {
		{ "legacy", RTE_INTR_MODE_LEGACY },
		{ "msi", RTE_INTR_MODE_MSI },
		{ "msix", RTE_INTR_MODE_MSIX },
	};

	for (size_t i = 0; i < RTE_DIM(map); i++) {
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

static int
eal_parse_huge_worker_stack(const char *arg)
{
#ifdef RTE_EXEC_ENV_WINDOWS
	EAL_LOG(WARNING, "Cannot set worker stack size on Windows, parameter ignored");
	RTE_SET_USED(arg);
#else
	struct internal_config *cfg = eal_get_internal_configuration();

	if (arg == NULL || arg[0] == '\0') {
		pthread_attr_t attr;
		int ret;

		if (pthread_attr_init(&attr) != 0) {
			EAL_LOG(ERR, "Could not retrieve default stack size");
			return -1;
		}
		ret = pthread_attr_getstacksize(&attr, &cfg->huge_worker_stack_size);
		pthread_attr_destroy(&attr);
		if (ret != 0) {
			EAL_LOG(ERR, "Could not retrieve default stack size");
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

	EAL_LOG(DEBUG, "Each worker thread will use %zu kB of DPDK memory as stack",
		cfg->huge_worker_stack_size / 1024);
#endif
	return 0;
}

/* Parse the arguments given in the command line of the application */
int
eal_parse_args(void)
{
	struct internal_config *int_cfg = eal_get_internal_configuration();
	struct arg_list_elem *arg;

	/* check for conflicting options */
	/* both -a and -b cannot be used together (one list must be empty at least) */
	if (!TAILQ_EMPTY(&args.allow) && !TAILQ_EMPTY(&args.block)) {
		EAL_LOG(ERR, "Options allow (-a) and block (-b) can't be used at the same time");
		return -1;
	}
	/* both -l and -c cannot be used at the same time */
	if (args.coremask != NULL && args.lcores != NULL) {
		EAL_LOG(ERR, "Options coremask (-c) and core list (-l) can't be used at the same time");
		return -1;
	}
	/* both -s and -S cannot be used at the same time */
	if (args.service_coremask != NULL && args.service_corelist != NULL) {
		EAL_LOG(ERR, "Options service coremask (-s) and service core list (-S) can't be used at the same time");
		return -1;
	}
	/* can't have both telemetry and no-telemetry */
	if (args.no_telemetry && args.telemetry) {
		EAL_LOG(ERR, "Options telemetry and no-telemetry can't be used at the same time");
		return -1;
	}
	/* can't have both -m and --numa-mem */
	if (args.memory_size != NULL && args.numa_mem != NULL) {
		EAL_LOG(ERR, "Options -m and --numa-mem can't be used at the same time");
		return -1;
	}

	/* parse options */
	/* print version before anything else */
	if (args.version) {
		/* since message is explicitly requested by user, we write message
		 * at highest log level so it can always be seen even if info or
		 * warning messages are disabled
		 */
		EAL_LOG(CRIT, "RTE Version: '%s'", rte_version());
	}

	/* parse the process type */
	if (args.proc_type != NULL) {
		int_cfg->process_type = eal_parse_proc_type(args.proc_type);
		if (int_cfg->process_type == RTE_PROC_INVALID) {
			EAL_LOG(ERR, "invalid process type: %s", args.proc_type);
			return -1;
		}
	}

	/* device -a/-b/-vdev options*/
	TAILQ_FOREACH(arg, &args.allow, next)
		if (eal_option_device_add(RTE_DEVTYPE_ALLOWED, arg->arg) < 0)
			return -1;
	TAILQ_FOREACH(arg, &args.block, next)
		if (eal_option_device_add(RTE_DEVTYPE_BLOCKED, arg->arg) < 0)
			return -1;
	TAILQ_FOREACH(arg, &args.vdev, next)
		if (eal_option_device_add(RTE_DEVTYPE_VIRTUAL, arg->arg) < 0)
			return -1;
	/* driver loading options */
	TAILQ_FOREACH(arg, &args.driver_path, next)
		if (eal_plugin_add(arg->arg) < 0)
			return -1;

	/* parse the coremask /core-list */
	if (args.coremask != NULL) {
		int lcore_indexes[RTE_MAX_LCORE];

		if (rte_eal_parse_coremask(args.coremask, lcore_indexes) < 0) {
			EAL_LOG(ERR, "invalid coremask syntax");
			return -1;
		}
		if (update_lcore_config(lcore_indexes) < 0) {
			char *available = available_cores();

			EAL_LOG(ERR, "invalid coremask '%s', please check specified cores are part of %s",
					args.coremask, available);
			free(available);
			return -1;
		}
		core_parsed = 1;
	} else if (args.lcores != NULL) {
		if (eal_parse_lcores(args.lcores) < 0) {
			EAL_LOG(ERR, "invalid lcore list: '%s'", args.lcores);
			return -1;
		}
		core_parsed = 1;
	}
	if (args.main_lcore != NULL) {
		if (eal_parse_main_lcore(args.main_lcore) < 0) {
			EAL_LOG(ERR, "invalid main-lcore parameter");
			return -1;
		}
	}

	/* service core options */
	if (args.service_coremask != NULL) {
		if (eal_parse_service_coremask(args.service_coremask) < 0) {
			EAL_LOG(ERR, "invalid service coremask: '%s'",
					args.service_coremask);
			return -1;
		}
	} else if (args.service_corelist != NULL) {
		if (eal_parse_service_corelist(args.service_corelist) < 0) {
			EAL_LOG(ERR, "invalid service core list: '%s'",
					args.service_corelist);
			return -1;
		}
	}

	/* memory options */
	if (args.memory_size != NULL) {
		int_cfg->memory = atoi(args.memory_size);
		int_cfg->memory *= 1024ULL;
		int_cfg->memory *= 1024ULL;
	}
	if (args.memory_channels != NULL) {
		int_cfg->force_nchannel = atoi(args.memory_channels);
		if (int_cfg->force_nchannel == 0) {
			EAL_LOG(ERR, "invalid memory channel parameter");
			return -1;
		}
	}
	if (args.memory_ranks != NULL) {
		int_cfg->force_nrank = atoi(args.memory_ranks);
		if (int_cfg->force_nrank == 0 || int_cfg->force_nrank > 16) {
			EAL_LOG(ERR, "invalid memory rank parameter");
			return -1;
		}
	}
	if (args.huge_unlink != NULL) {
		if (args.huge_unlink == (void *)1)
			args.huge_unlink = NULL;
		if (eal_parse_huge_unlink(args.huge_unlink, &int_cfg->hugepage_file) < 0) {
			EAL_LOG(ERR, "invalid huge-unlink parameter");
			return -1;
		}
	}
	if (args.no_huge) {
		int_cfg->no_hugetlbfs = 1;
		/* no-huge is legacy mem */
		int_cfg->legacy_mem = 1;
	}
	if (args.in_memory) {
		int_cfg->in_memory = 1;
		/* in-memory is a superset of noshconf and huge-unlink */
		int_cfg->no_shconf = 1;
		int_cfg->hugepage_file.unlink_before_mapping = true;
	}
	if (args.legacy_mem)
		int_cfg->legacy_mem = 1;
	if (args.single_file_segments)
		int_cfg->single_file_segments = 1;
	if (args.huge_dir != NULL) {
		free(int_cfg->hugepage_dir);  /* free old hugepage dir */
		int_cfg->hugepage_dir = strdup(args.huge_dir);
		if (int_cfg->hugepage_dir == NULL) {
			EAL_LOG(ERR, "failed to allocate memory for hugepage dir parameter");
			return -1;
		}
	}
	if (args.file_prefix != NULL) {
		free(int_cfg->hugefile_prefix);  /* free old file prefix */
		int_cfg->hugefile_prefix = strdup(args.file_prefix);
		if (int_cfg->hugefile_prefix == NULL) {
			EAL_LOG(ERR, "failed to allocate memory for file prefix parameter");
			return -1;
		}
	}
	if (args.numa_mem != NULL) {
		if (eal_parse_socket_arg(args.numa_mem, int_cfg->numa_mem) < 0) {
			EAL_LOG(ERR, "invalid numa-mem parameter: '%s'", args.numa_mem);
			return -1;
		}
		int_cfg->force_numa = 1;
	}
	if (args.numa_limit != NULL) {
		if (eal_parse_socket_arg(args.numa_limit, int_cfg->numa_limit) < 0) {
			EAL_LOG(ERR, "invalid numa-limit parameter: '%s'", args.numa_limit);
			return -1;
		}
		int_cfg->force_numa_limits = 1;
	}

	/* tracing settings, not supported on windows */
#ifdef RTE_EXEC_ENV_WINDOWS
	if (!TAILQ_EMPTY(&args.trace) ||
			args.trace_dir != NULL ||
			args.trace_bufsz != NULL ||
			args.trace_mode != NULL)
		EAL_LOG(WARNING, "Tracing is not supported on Windows, ignoring tracing parameters");
#else
	TAILQ_FOREACH(arg, &args.trace, next) {
		if (eal_trace_args_save(arg->arg) < 0) {
			EAL_LOG(ERR, "invalid trace parameter, '%s'", arg->arg);
			return -1;
		}
	}
	if (args.trace_dir != NULL) {
		if (eal_trace_dir_args_save(args.trace_dir) < 0) {
			EAL_LOG(ERR, "invalid trace directory, '%s'", args.trace_dir);
			return -1;
		}
	}
	if (args.trace_bufsz != NULL) {
		if (eal_trace_bufsz_args_save(args.trace_bufsz) < 0) {
			EAL_LOG(ERR, "invalid trace buffer size, '%s'", args.trace_bufsz);
			return -1;
		}
	}
	if (args.trace_mode != NULL) {
		if (eal_trace_mode_args_save(args.trace_mode) < 0) {
			EAL_LOG(ERR, "invalid trace mode, '%s'", args.trace_mode);
			return -1;
		}
	}
#endif

	/* simple flag settings
	 * Only set these to 1, as we don't want to set them to 0 in case
	 * other options above have already set them.
	 */
	if (args.no_pci)
		int_cfg->no_pci = 1;
	if (args.no_hpet)
		int_cfg->no_hpet = 1;
	if (args.vmware_tsc_map)
		int_cfg->vmware_tsc_map = 1;
	if (args.no_shconf)
		int_cfg->no_shconf = 1;
	if (args.no_telemetry)
		int_cfg->no_telemetry = 1;
	if (args.match_allocations)
		int_cfg->match_allocations = 1;
	if (args.create_uio_dev)
		int_cfg->create_uio_dev = 1;

	/* other misc settings */
	if (args.iova_mode != NULL) {
		if (eal_parse_iova_mode(args.iova_mode) < 0) {
			EAL_LOG(ERR, "invalid iova mode parameter '%s'", args.iova_mode);
			return -1;
		}
	};
	if (args.base_virtaddr != NULL) {
		if (eal_parse_base_virtaddr(args.base_virtaddr) < 0) {
			EAL_LOG(ERR, "invalid base virtaddr '%s'", args.base_virtaddr);
			return -1;
		}
	}
	if (args.force_max_simd_bitwidth != NULL) {
		if (eal_parse_simd_bitwidth(args.force_max_simd_bitwidth) < 0) {
			EAL_LOG(ERR, "invalid SIMD bitwidth parameter '%s'",
					args.force_max_simd_bitwidth);
			return -1;
		}
	}
	if (args.vfio_intr != NULL) {
		if (eal_parse_vfio_intr(args.vfio_intr) < 0) {
			EAL_LOG(ERR, "invalid vfio interrupt parameter: '%s'", args.vfio_intr);
			return -1;
		}
	}
	if (args.vfio_vf_token != NULL) {
		if (eal_parse_vfio_vf_token(args.vfio_vf_token) < 0) {
			EAL_LOG(ERR, "invalid vfio vf token parameter: '%s'", args.vfio_vf_token);
			return -1;
		}
	}

	if (args.huge_worker_stack != NULL) {
		if (args.huge_worker_stack == (void *)1)
			args.huge_worker_stack = NULL;
		if (eal_parse_huge_worker_stack(args.huge_worker_stack) < 0) {
			EAL_LOG(ERR, "invalid huge worker stack parameter");
			return -1;
		}
	}
	if (args.mbuf_pool_ops_name != NULL) {
		free(int_cfg->user_mbuf_pool_ops_name); /* free old ops name */
		int_cfg->user_mbuf_pool_ops_name = strdup(args.mbuf_pool_ops_name);
		if (int_cfg->user_mbuf_pool_ops_name == NULL) {
			EAL_LOG(ERR, "failed to allocate memory for mbuf pool ops name parameter");
			return -1;
		}
	}

#ifndef RTE_EXEC_ENV_WINDOWS
	/* create runtime data directory. In no_shconf mode, skip any errors */
	if (eal_create_runtime_dir() < 0) {
		if (int_cfg->no_shconf == 0) {
			EAL_LOG(ERR, "Cannot create runtime directory");
			return -1;
		}
		EAL_LOG(WARNING, "No DPDK runtime directory created");
	}
#endif

	if (eal_adjust_config(int_cfg) != 0) {
		EAL_LOG(ERR, "Invalid configuration");
		return -1;
	}

	if (eal_check_common_options(int_cfg) != 0) {
		EAL_LOG(ERR, "Checking common options failed");
		return -1;
	}

	return 0;
}

static void
eal_auto_detect_cores(struct rte_config *cfg)
{
	unsigned int lcore_id;
	unsigned int removed = 0;
	rte_cpuset_t affinity_set;

	if (rte_thread_get_affinity_by_id(rte_thread_self(), &affinity_set) != 0)
		CPU_ZERO(&affinity_set);

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (cfg->lcore_role[lcore_id] == ROLE_RTE &&
		    !CPU_ISSET(lcore_id, &affinity_set)) {
			cfg->lcore_role[lcore_id] = ROLE_OFF;
			removed++;
		}
	}

	cfg->lcore_count -= removed;
}

static void
compute_ctrl_threads_cpuset(struct internal_config *internal_cfg)
{
	rte_cpuset_t *cpuset = &internal_cfg->ctrl_cpuset;
	rte_cpuset_t default_set;
	unsigned int lcore_id;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_has_role(lcore_id, ROLE_OFF))
			continue;
		RTE_CPU_OR(cpuset, cpuset, &lcore_config[lcore_id].cpuset);
	}
	RTE_CPU_NOT(cpuset, cpuset);

	if (rte_thread_get_affinity_by_id(rte_thread_self(), &default_set) != 0)
		CPU_ZERO(&default_set);

	RTE_CPU_AND(cpuset, cpuset, &default_set);

	/* if no remaining cpu, use main lcore cpu affinity */
	if (!CPU_COUNT(cpuset)) {
		memcpy(cpuset, &lcore_config[rte_get_main_lcore()].cpuset,
			sizeof(*cpuset));
	}
}

int
eal_cleanup_config(struct internal_config *internal_cfg)
{
	free(internal_cfg->hugefile_prefix);
	free(internal_cfg->hugepage_dir);
	free(internal_cfg->user_mbuf_pool_ops_name);

	return 0;
}

int
eal_adjust_config(struct internal_config *internal_cfg)
{
	int i;
	struct rte_config *cfg = rte_eal_get_configuration();
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (!core_parsed)
		eal_auto_detect_cores(cfg);

	if (cfg->lcore_count == 0) {
		EAL_LOG(ERR, "No detected lcore is enabled, please check the core list");
		return -1;
	}

	if (internal_conf->process_type == RTE_PROC_AUTO)
		internal_conf->process_type = eal_proc_type_detect();

	/* default main lcore is the first one */
	if (!main_lcore_parsed) {
		cfg->main_lcore = rte_get_next_lcore(-1, 0, 0);
		if (cfg->main_lcore >= RTE_MAX_LCORE)
			return -1;
		lcore_config[cfg->main_lcore].core_role = ROLE_RTE;
	}

	compute_ctrl_threads_cpuset(internal_cfg);

	/* if no memory amounts were requested, this will result in 0 and
	 * will be overridden later, right after eal_hugepage_info_init() */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		internal_cfg->memory += internal_cfg->numa_mem[i];

	return 0;
}

int
eal_check_common_options(struct internal_config *internal_cfg)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	if (cfg->lcore_role[cfg->main_lcore] != ROLE_RTE) {
		EAL_LOG(ERR, "Main lcore is not enabled for DPDK");
		return -1;
	}

	if (internal_cfg->process_type == RTE_PROC_INVALID) {
		EAL_LOG(ERR, "Invalid process type specified");
		return -1;
	}
	if (internal_cfg->hugefile_prefix != NULL &&
			strlen(internal_cfg->hugefile_prefix) < 1) {
		EAL_LOG(ERR, "Invalid length of --" OPT_FILE_PREFIX " option");
		return -1;
	}
	if (internal_cfg->hugepage_dir != NULL &&
			strlen(internal_cfg->hugepage_dir) < 1) {
		EAL_LOG(ERR, "Invalid length of --" OPT_HUGE_DIR" option");
		return -1;
	}
	if (internal_cfg->user_mbuf_pool_ops_name != NULL &&
			strlen(internal_cfg->user_mbuf_pool_ops_name) < 1) {
		EAL_LOG(ERR, "Invalid length of --" OPT_MBUF_POOL_OPS_NAME" option");
		return -1;
	}
	if (strchr(eal_get_hugefile_prefix(), '%') != NULL) {
		EAL_LOG(ERR, "Invalid char, '%%', in --"OPT_FILE_PREFIX" "
			"option");
		return -1;
	}
	if (internal_cfg->no_hugetlbfs && internal_cfg->force_numa == 1) {
		EAL_LOG(ERR, "Option --"OPT_NUMA_MEM" cannot "
			"be specified together with --"OPT_NO_HUGE);
		return -1;
	}
	if (internal_cfg->no_hugetlbfs &&
			internal_cfg->hugepage_file.unlink_before_mapping &&
			!internal_cfg->in_memory) {
		EAL_LOG(ERR, "Option --"OPT_HUGE_UNLINK" cannot "
			"be specified together with --"OPT_NO_HUGE);
		return -1;
	}
	if (internal_cfg->no_hugetlbfs &&
			internal_cfg->huge_worker_stack_size != 0) {
		EAL_LOG(ERR, "Option --"OPT_HUGE_WORKER_STACK" cannot "
			"be specified together with --"OPT_NO_HUGE);
		return -1;
	}
	if (internal_conf->force_numa_limits && internal_conf->legacy_mem) {
		EAL_LOG(ERR, "Option --"OPT_NUMA_LIMIT
			" is only supported in non-legacy memory mode");
	}
	if (internal_cfg->single_file_segments &&
			internal_cfg->hugepage_file.unlink_before_mapping &&
			!internal_cfg->in_memory) {
		EAL_LOG(ERR, "Option --"OPT_SINGLE_FILE_SEGMENTS" is "
			"not compatible with --"OPT_HUGE_UNLINK);
		return -1;
	}
	if (!internal_cfg->hugepage_file.unlink_existing &&
			internal_cfg->in_memory) {
		EAL_LOG(ERR, "Option --"OPT_IN_MEMORY" is not compatible "
			"with --"OPT_HUGE_UNLINK"="HUGE_UNLINK_NEVER);
		return -1;
	}
	if (internal_cfg->legacy_mem &&
			internal_cfg->in_memory) {
		EAL_LOG(ERR, "Option --"OPT_LEGACY_MEM" is not compatible "
				"with --"OPT_IN_MEMORY);
		return -1;
	}
	if (internal_cfg->legacy_mem && internal_cfg->match_allocations) {
		EAL_LOG(ERR, "Option --"OPT_LEGACY_MEM" is not compatible "
				"with --"OPT_MATCH_ALLOCATIONS);
		return -1;
	}
	if (internal_cfg->no_hugetlbfs && internal_cfg->match_allocations) {
		EAL_LOG(ERR, "Option --"OPT_NO_HUGE" is not compatible "
				"with --"OPT_MATCH_ALLOCATIONS);
		return -1;
	}
	if (internal_cfg->legacy_mem && internal_cfg->memory == 0) {
		EAL_LOG(NOTICE, "Static memory layout is selected, "
			"amount of reserved memory can be adjusted with "
			"-m or --"OPT_NUMA_MEM);
	}

	return 0;
}

RTE_EXPORT_SYMBOL(rte_vect_get_max_simd_bitwidth)
uint16_t
rte_vect_get_max_simd_bitwidth(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();
	return internal_conf->max_simd_bitwidth.bitwidth;
}

RTE_EXPORT_SYMBOL(rte_vect_set_max_simd_bitwidth)
int
rte_vect_set_max_simd_bitwidth(uint16_t bitwidth)
{
	struct internal_config *internal_conf =
		eal_get_internal_configuration();
	if (internal_conf->max_simd_bitwidth.forced) {
		EAL_LOG(NOTICE, "Cannot set max SIMD bitwidth - user runtime override enabled");
		return -EPERM;
	}

	if (bitwidth < RTE_VECT_SIMD_DISABLED || !rte_is_power_of_2(bitwidth)) {
		EAL_LOG(ERR, "Invalid bitwidth value!");
		return -EINVAL;
	}
	internal_conf->max_simd_bitwidth.bitwidth = bitwidth;
	return 0;
}
