/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

struct roc_model *roc_model;

/* RoC and CPU IDs and revisions */
#define VENDOR_ARM    0x41 /* 'A' */
#define VENDOR_CAVIUM 0x43 /* 'C' */

#define SOC_PART_CN10K 0xD49

#define PART_106XX  0xB9
#define PART_105XX  0xBA
#define PART_105XXN 0xBC
#define PART_98XX   0xB1
#define PART_96XX   0xB2
#define PART_95XX   0xB3
#define PART_95XXN  0xB4
#define PART_95XXMM 0xB5
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

static const struct model_db {
	uint32_t impl;
	uint32_t part;
	uint32_t major;
	uint32_t minor;
	uint64_t flag;
	char name[ROC_MODEL_STR_LEN_MAX];
} model_db[] = {
	{VENDOR_ARM, PART_106XX, 0, 0, ROC_MODEL_CN106XX, "cn10ka"},
	{VENDOR_ARM, PART_105XX, 0, 0, ROC_MODEL_CNF105XX, "cnf10ka"},
	{VENDOR_ARM, PART_105XXN, 0, 0, ROC_MODEL_CNF105XXN, "cnf10kb"},
	{VENDOR_CAVIUM, PART_98XX, 0, 0, ROC_MODEL_CN98xx_A0, "cn98xx_a0"},
	{VENDOR_CAVIUM, PART_96XX, 0, 0, ROC_MODEL_CN96xx_A0, "cn96xx_a0"},
	{VENDOR_CAVIUM, PART_96XX, 0, 1, ROC_MODEL_CN96xx_B0, "cn96xx_b0"},
	{VENDOR_CAVIUM, PART_96XX, 2, 0, ROC_MODEL_CN96xx_C0, "cn96xx_c0"},
	{VENDOR_CAVIUM, PART_95XX, 0, 0, ROC_MODEL_CNF95xx_A0, "cnf95xx_a0"},
	{VENDOR_CAVIUM, PART_95XX, 1, 0, ROC_MODEL_CNF95xx_B0, "cnf95xx_b0"},
	{VENDOR_CAVIUM, PART_95XXN, 0, 0, ROC_MODEL_CNF95XXN_A0, "cnf95xxn_a0"},
	{VENDOR_CAVIUM, PART_95O, 0, 0, ROC_MODEL_CNF95XXO_A0, "cnf95O_a0"},
	{VENDOR_CAVIUM, PART_95XXMM, 0, 0, ROC_MODEL_CNF95XXMM_A0,
	 "cnf95xxmm_a0"}
};

static uint32_t
cn10k_part_get(void)
{
	uint32_t soc = 0x0;
	char buf[BUFSIZ];
	char *ptr;
	FILE *fd;

	/* Read the CPU compatible variant */
	fd = fopen("/proc/device-tree/compatible", "r");
	if (!fd) {
		plt_err("Failed to open /proc/device-tree/compatible");
		goto err;
	}

	if (fgets(buf, sizeof(buf), fd) == NULL) {
		plt_err("Failed to read from /proc/device-tree/compatible");
		goto fclose;
	}
	ptr = strchr(buf, ',');
	if (!ptr) {
		plt_err("Malformed 'CPU compatible': <%s>", buf);
		goto fclose;
	}
	ptr++;
	if (strcmp("cn10ka", ptr) == 0) {
		soc = PART_106XX;
	} else if (strcmp("cnf10ka", ptr) == 0) {
		soc = PART_105XX;
	} else if (strcmp("cnf10kb", ptr) == 0) {
		soc = PART_105XXN;
	} else {
		plt_err("Unidentified 'CPU compatible': <%s>", ptr);
		goto fclose;
	}

fclose:
	fclose(fd);

err:
	return soc;
}

static bool
populate_model(struct roc_model *model, uint32_t midr)
{
	uint32_t impl, major, part, minor;
	bool found = false;
	size_t i;

	impl = (midr >> MODEL_IMPL_SHIFT) & MODEL_IMPL_MASK;
	part = (midr >> MODEL_PART_SHIFT) & MODEL_PART_MASK;
	major = (midr >> MODEL_MAJOR_SHIFT) & MODEL_MAJOR_MASK;
	minor = (midr >> MODEL_MINOR_SHIFT) & MODEL_MINOR_MASK;

	/* Update part number for cn10k from device-tree */
	if (part == SOC_PART_CN10K)
		part = cn10k_part_get();

	for (i = 0; i < PLT_DIM(model_db); i++)
		if (model_db[i].impl == impl && model_db[i].part == part &&
		    model_db[i].major == major && model_db[i].minor == minor) {
			model->flag = model_db[i].flag;
			strncpy(model->name, model_db[i].name,
				ROC_MODEL_STR_LEN_MAX - 1);
			found = true;
			break;
		}

	if (!found) {
		model->flag = 0;
		strncpy(model->name, "unknown", ROC_MODEL_STR_LEN_MAX - 1);
		plt_err("Invalid RoC model (impl=0x%x, part=0x%x)", impl, part);
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

	rc = 0;
	plt_info("RoC Model: %s", model->name);
	roc_model = model;
err:
	return rc;
}
