/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 * Copyright(c) 2021 Intel Corporation
 */

#include <inttypes.h>

#include <rte_dmadev.h>
#include <rte_bus_vdev.h>
#include <rte_dmadev_pmd.h>

#include "test.h"
#include "test_dmadev_api.h"

#define ERR_RETURN(...) do { print_err(__func__, __LINE__, __VA_ARGS__); return -1; } while (0)

static void
__rte_format_printf(3, 4)
print_err(const char *func, int lineno, const char *format, ...)
{
	va_list ap;

	fprintf(stderr, "In %s:%d - ", func, lineno);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

static int
test_dmadev_instance(int16_t dev_id)
{
#define TEST_RINGSIZE 512
	struct rte_dma_stats stats;
	struct rte_dma_info info;
	const struct rte_dma_conf conf = { .nb_vchans = 1};
	const struct rte_dma_vchan_conf qconf = {
			.direction = RTE_DMA_DIR_MEM_TO_MEM,
			.nb_desc = TEST_RINGSIZE,
	};
	const int vchan = 0;

	printf("\n### Test dmadev instance %u [%s]\n",
			dev_id, rte_dma_devices[dev_id].data->dev_name);

	rte_dma_info_get(dev_id, &info);
	if (info.max_vchans < 1)
		ERR_RETURN("Error, no channels available on device id %u\n", dev_id);

	if (rte_dma_configure(dev_id, &conf) != 0)
		ERR_RETURN("Error with rte_dma_configure()\n");

	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) < 0)
		ERR_RETURN("Error with queue configuration\n");

	rte_dma_info_get(dev_id, &info);
	if (info.nb_vchans != 1)
		ERR_RETURN("Error, no configured queues reported on device id %u\n", dev_id);

	if (rte_dma_start(dev_id) != 0)
		ERR_RETURN("Error with rte_dma_start()\n");

	if (rte_dma_stats_get(dev_id, vchan, &stats) != 0)
		ERR_RETURN("Error with rte_dma_stats_get()\n");

	if (stats.completed != 0 || stats.submitted != 0 || stats.errors != 0)
		ERR_RETURN("Error device stats are not all zero: completed = %"PRIu64", "
				"submitted = %"PRIu64", errors = %"PRIu64"\n",
				stats.completed, stats.submitted, stats.errors);

	rte_dma_stop(dev_id);
	rte_dma_stats_reset(dev_id, vchan);
	return 0;
}

static int
test_apis(void)
{
	const char *pmd = "dma_skeleton";
	int id;
	int ret;

	if (rte_vdev_init(pmd, NULL) < 0)
		return TEST_SKIPPED;
	id = rte_dma_get_dev_id_by_name(pmd);
	if (id < 0)
		return TEST_SKIPPED;
	printf("\n### Test dmadev infrastructure using skeleton driver\n");
	ret = test_dma_api(id);
	rte_vdev_uninit(pmd);

	return ret;
}

static int
test_dma(void)
{
	int i;

	/* basic sanity on dmadev infrastructure */
	if (test_apis() < 0)
		ERR_RETURN("Error performing API tests\n");

	if (rte_dma_count_avail() == 0)
		return TEST_SKIPPED;

	RTE_DMA_FOREACH_DEV(i)
		if (test_dmadev_instance(i) < 0)
			ERR_RETURN("Error, test failure for device %d\n", i);

	return 0;
}

REGISTER_TEST_COMMAND(dmadev_autotest, test_dma);
