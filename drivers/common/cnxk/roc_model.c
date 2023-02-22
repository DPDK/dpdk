/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_priv.h"

struct roc_model *roc_model;

/* RoC and CPU IDs and revisions */
#define VENDOR_ARM    0x41 /* 'A' */
#define VENDOR_CAVIUM 0x43 /* 'C' */

#define SOC_PART_CN10K 0xD49

#define PART_106xx  0xB9
#define PART_105xx  0xBA
#define PART_105xxN 0xBC
#define PART_103xx  0xBD
#define PART_98xx   0xB1
#define PART_96xx   0xB2
#define PART_95xx   0xB3
#define PART_95xxN  0xB4
#define PART_95xxMM 0xB5
#define PART_95O    0xB6

#define MODEL_IMPL_BITS	  8
#define MODEL_IMPL_SHIFT  24
#define MODEL_IMPL_MASK	  ((1 << MODEL_IMPL_BITS) - 1)
#define MODEL_PART_BITS	  12
#define MODEL_PART_SHIFT  4
#define MODEL_PART_MASK	  ((1 << MODEL_PART_BITS) - 1)
#define MODEL_MAJOR_BITS  4
#define MODEL_MAJOR_SHIFT 20
#define MODEL_MAJOR_MASK  ((1 << MODEL_MAJOR_BITS) - 1)
#define MODEL_MINOR_BITS  4
#define MODEL_MINOR_SHIFT 0
#define MODEL_MINOR_MASK  ((1 << MODEL_MINOR_BITS) - 1)

#define MODEL_CN10K_PART_SHIFT	8
#define MODEL_CN10K_PASS_BITS	4
#define MODEL_CN10K_PASS_MASK	((1 << MODEL_CN10K_PASS_BITS) - 1)
#define MODEL_CN10K_MAJOR_BITS	2
#define MODEL_CN10K_MAJOR_SHIFT 2
#define MODEL_CN10K_MAJOR_MASK	((1 << MODEL_CN10K_MAJOR_BITS) - 1)
#define MODEL_CN10K_MINOR_BITS	2
#define MODEL_CN10K_MINOR_SHIFT 0
#define MODEL_CN10K_MINOR_MASK	((1 << MODEL_CN10K_MINOR_BITS) - 1)

static const struct model_db {
	uint32_t impl;
	uint32_t part;
	uint32_t major;
	uint32_t minor;
	uint64_t flag;
	char name[ROC_MODEL_STR_LEN_MAX];
} model_db[] = {
	{VENDOR_ARM, PART_106xx, 0, 0, ROC_MODEL_CN106xx_A0, "cn10ka_a0"},
	{VENDOR_ARM, PART_106xx, 0, 1, ROC_MODEL_CN106xx_A1, "cn10ka_a1"},
	{VENDOR_ARM, PART_106xx, 1, 0, ROC_MODEL_CN106xx_B0, "cn10ka_b0"},
	{VENDOR_ARM, PART_105xx, 0, 0, ROC_MODEL_CNF105xx_A0, "cnf10ka_a0"},
	{VENDOR_ARM, PART_105xx, 0, 1, ROC_MODEL_CNF105xx_A1, "cnf10ka_a1"},
	{VENDOR_ARM, PART_103xx, 0, 0, ROC_MODEL_CN103xx_A0, "cn10kb_a0"},
	{VENDOR_ARM, PART_105xxN, 0, 0, ROC_MODEL_CNF105xxN_A0, "cnf10kb_a0"},
	{VENDOR_CAVIUM, PART_98xx, 0, 0, ROC_MODEL_CN98xx_A0, "cn98xx_a0"},
	{VENDOR_CAVIUM, PART_98xx, 0, 1, ROC_MODEL_CN98xx_A1, "cn98xx_a1"},
	{VENDOR_CAVIUM, PART_96xx, 0, 0, ROC_MODEL_CN96xx_A0, "cn96xx_a0"},
	{VENDOR_CAVIUM, PART_96xx, 0, 1, ROC_MODEL_CN96xx_B0, "cn96xx_b0"},
	{VENDOR_CAVIUM, PART_96xx, 2, 0, ROC_MODEL_CN96xx_C0, "cn96xx_c0"},
	{VENDOR_CAVIUM, PART_96xx, 2, 1, ROC_MODEL_CN96xx_C0, "cn96xx_c1"},
	{VENDOR_CAVIUM, PART_95xx, 0, 0, ROC_MODEL_CNF95xx_A0, "cnf95xx_a0"},
	{VENDOR_CAVIUM, PART_95xx, 1, 0, ROC_MODEL_CNF95xx_B0, "cnf95xx_b0"},
	{VENDOR_CAVIUM, PART_95xxN, 0, 0, ROC_MODEL_CNF95xxN_A0, "cnf95xxn_a0"},
	{VENDOR_CAVIUM, PART_95xxN, 0, 1, ROC_MODEL_CNF95xxN_A0, "cnf95xxn_a1"},
	{VENDOR_CAVIUM, PART_95xxN, 1, 0, ROC_MODEL_CNF95xxN_B0, "cnf95xxn_b0"},
	{VENDOR_CAVIUM, PART_95O, 0, 0, ROC_MODEL_CNF95xxO_A0, "cnf95O_a0"},
	{VENDOR_CAVIUM, PART_95xxMM, 0, 0, ROC_MODEL_CNF95xxMM_A0,
	 "cnf95xxmm_a0"}};

/* Detect if RVU device */
static bool
is_rvu_device(unsigned long val)
{
	return (val == PCI_DEVID_CNXK_RVU_PF || val == PCI_DEVID_CNXK_RVU_VF ||
		val == PCI_DEVID_CNXK_RVU_AF ||
		val == PCI_DEVID_CNXK_RVU_AF_VF ||
		val == PCI_DEVID_CNXK_RVU_NPA_PF ||
		val == PCI_DEVID_CNXK_RVU_NPA_VF ||
		val == PCI_DEVID_CNXK_RVU_SSO_TIM_PF ||
		val == PCI_DEVID_CNXK_RVU_SSO_TIM_VF ||
		val == PCI_DEVID_CN10K_RVU_CPT_PF ||
		val == PCI_DEVID_CN10K_RVU_CPT_VF);
}

static int
rvu_device_lookup(const char *dirname, uint32_t *part, uint32_t *pass)
{
	char filename[PATH_MAX];
	unsigned long val;

	/* Check if vendor id is cavium */
	snprintf(filename, sizeof(filename), "%s/vendor", dirname);
	if (plt_sysfs_value_parse(filename, &val) < 0)
		goto error;

	if (val != PCI_VENDOR_ID_CAVIUM)
		goto error;

	/* Get device id  */
	snprintf(filename, sizeof(filename), "%s/device", dirname);
	if (plt_sysfs_value_parse(filename, &val) < 0)
		goto error;

	/* Check if device ID belongs to any RVU device */
	if (!is_rvu_device(val))
		goto error;

	/* Get subsystem_device id */
	snprintf(filename, sizeof(filename), "%s/subsystem_device", dirname);
	if (plt_sysfs_value_parse(filename, &val) < 0)
		goto error;

	*part = val >> MODEL_CN10K_PART_SHIFT;

	/* Get revision for pass value*/
	snprintf(filename, sizeof(filename), "%s/revision", dirname);
	if (plt_sysfs_value_parse(filename, &val) < 0)
		goto error;

	*pass = val & MODEL_CN10K_PASS_MASK;

	return 0;
error:
	return -EINVAL;
}

/* Scans through all PCI devices, detects RVU device and returns
 * subsystem_device
 */
static int
cn10k_part_pass_get(uint32_t *part, uint32_t *pass)
{
#define SYSFS_PCI_DEVICES "/sys/bus/pci/devices"
	char dirname[PATH_MAX];
	struct dirent *e;
	DIR *dir;

	dir = opendir(SYSFS_PCI_DEVICES);
	if (dir == NULL) {
		plt_err("%s(): opendir failed: %s\n", __func__,
			strerror(errno));
		return -errno;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s", SYSFS_PCI_DEVICES,
			 e->d_name);

		/* Lookup for rvu device and get part pass information */
		if (!rvu_device_lookup(dirname, part, pass))
			break;
	}

	closedir(dir);
	return 0;
}

static bool
populate_model(struct roc_model *model, uint32_t midr)
{
	uint32_t impl, major, part, minor, pass;
	bool found = false;
	size_t i;

	impl = (midr >> MODEL_IMPL_SHIFT) & MODEL_IMPL_MASK;
	part = (midr >> MODEL_PART_SHIFT) & MODEL_PART_MASK;
	major = (midr >> MODEL_MAJOR_SHIFT) & MODEL_MAJOR_MASK;
	minor = (midr >> MODEL_MINOR_SHIFT) & MODEL_MINOR_MASK;

	/* Update part number for cn10k from device-tree */
	if (part == SOC_PART_CN10K) {
		if (cn10k_part_pass_get(&part, &pass))
			goto not_found;
		/*
		 * Pass value format:
		 * Bits 0..1: minor pass
		 * Bits 3..2: major pass
		 */
		minor = (pass >> MODEL_CN10K_MINOR_SHIFT) &
			MODEL_CN10K_MINOR_MASK;
		major = (pass >> MODEL_CN10K_MAJOR_SHIFT) &
			MODEL_CN10K_MAJOR_MASK;
	}

	for (i = 0; i < PLT_DIM(model_db); i++)
		if (model_db[i].impl == impl && model_db[i].part == part &&
		    model_db[i].major == major && model_db[i].minor == minor) {
			model->flag = model_db[i].flag;
			strncpy(model->name, model_db[i].name,
				ROC_MODEL_STR_LEN_MAX - 1);
			found = true;
			break;
		}
not_found:
	if (!found) {
		model->flag = 0;
		strncpy(model->name, "unknown", ROC_MODEL_STR_LEN_MAX - 1);
		plt_err("Invalid RoC model (impl=0x%x, part=0x%x, major=0x%x, minor=0x%x)",
			impl, part, major, minor);
	}

	return found;
}

static int
midr_get(unsigned long *val)
{
	const char *file =
		"/sys/devices/system/cpu/cpu0/regs/identification/midr_el1";
	int rc = UTIL_ERR_FS;
	char buf[BUFSIZ];
	char *end = NULL;
	FILE *f;

	if (val == NULL)
		goto err;
	f = fopen(file, "r");
	if (f == NULL)
		goto err;

	if (fgets(buf, sizeof(buf), f) == NULL)
		goto fclose;

	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n'))
		goto fclose;

	rc = 0;
fclose:
	fclose(f);
err:
	return rc;
}

static void
detect_invalid_config(void)
{
#ifdef ROC_PLATFORM_CN9K
#ifdef ROC_PLATFORM_CN10K
	PLT_STATIC_ASSERT(0);
#endif
#endif
}

static uint64_t
env_lookup_flag(const char *name)
{
	unsigned int i;
	struct {
		const char *name;
		uint64_t flag;
	} envs[] = {
		{"HW_PLATFORM", ROC_ENV_HW},
		{"EMUL_PLATFORM", ROC_ENV_EMUL},
		{"ASIM_PLATFORM", ROC_ENV_ASIM},
	};

	for (i = 0; i < PLT_DIM(envs); i++)
		if (!strncmp(envs[i].name, name, strlen(envs[i].name)))
			return envs[i].flag;

	return 0;
}

static void
of_env_get(struct roc_model *model)
{
	const char *const path = "/proc/device-tree/soc@0/runplatform";
	uint64_t flag;
	FILE *fp;

	if (access(path, F_OK) != 0) {
		strncpy(model->env, "HW_PLATFORM", ROC_MODEL_STR_LEN_MAX - 1);
		model->flag |= ROC_ENV_HW;
		return;
	}

	fp = fopen(path, "r");
	if (!fp) {
		plt_err("Failed to open %s", path);
		return;
	}

	if (!fgets(model->env, sizeof(model->env), fp)) {
		plt_err("Failed to read %s", path);
		goto err;
	}

	flag = env_lookup_flag(model->env);
	if (flag == 0) {
		plt_err("Unknown platform: %s", model->env);
		goto err;
	}

	model->flag |= flag;
err:
	fclose(fp);
}

int
roc_model_init(struct roc_model *model)
{
	int rc = UTIL_ERR_PARAM;
	unsigned long midr;

	detect_invalid_config();

	if (!model)
		goto err;

	rc = midr_get(&midr);
	if (rc)
		goto err;

	rc = UTIL_ERR_INVALID_MODEL;
	if (!populate_model(model, midr))
		goto err;

	of_env_get(model);

	rc = 0;
	plt_info("RoC Model: %s (%s)", model->name, model->env);
	roc_model = model;
err:
	return rc;
}
