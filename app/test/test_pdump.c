/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>

#include <rte_ethdev_driver.h>
#include <rte_pdump.h>
#include "rte_eal.h"
#include "rte_lcore.h"
#include "rte_mempool.h"
#include "rte_ring.h"

#include "sample_packet_forward.h"
#include "test.h"
#include "process.h"
#include "test_pdump.h"

#define launch_p(ARGV) process_dup(ARGV, RTE_DIM(ARGV), __func__)

struct rte_ring *ring_server;
uint16_t portid;
uint16_t flag_for_send_pkts = 1;

int
test_pdump_init(void)
{
	int ret = 0;

	ret = rte_pdump_init();
	if (ret < 0) {
		printf("rte_pdump_init failed\n");
		return -1;
	}
	ret = test_ring_setup(&ring_server, &portid);
	if (ret < 0) {
		printf("test_ring_setup failed\n");
		return -1;
	}
	printf("pdump_init success\n");
	return ret;
}

int
run_pdump_client_tests(void)
{
	int flags = RTE_PDUMP_FLAG_TX, ret = 0, itr;
	char deviceid[] = "net_ring_net_ringa";
	struct rte_ring *ring_client;
	struct rte_mempool *mp = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	char poolname[] = "mbuf_pool_client";

	ret = test_get_mempool(&mp, poolname);
	if (ret < 0)
		return -1;
	mp->flags = 0x0000;
	ring_client = rte_ring_create("SR0", RING_SIZE, rte_socket_id(), 0);
	if (ring_client == NULL) {
		printf("rte_ring_create SR0 failed");
		return -1;
	}

	eth_dev = rte_eth_dev_attach_secondary(deviceid);
	if (!eth_dev) {
		printf("Failed to probe %s", deviceid);
		return -1;
	}
	rte_eth_dev_probing_finish(eth_dev);

	printf("\n***** flags = RTE_PDUMP_FLAG_TX *****\n");

	for (itr = 0; itr < NUM_ITR; itr++) {
		ret = rte_pdump_enable(portid, QUEUE_ID, flags, ring_client,
				       mp, NULL);
		if (ret < 0) {
			printf("rte_pdump_enable failed\n");
			return -1;
		}
		printf("pdump_enable success\n");

		ret = rte_pdump_disable(portid, QUEUE_ID, flags);
		if (ret < 0) {
			printf("rte_pdump_disable failed\n");
			return -1;
		}
		printf("pdump_disable success\n");

		ret = rte_pdump_enable_by_deviceid(deviceid, QUEUE_ID, flags,
						   ring_client, mp, NULL);
		if (ret < 0) {
			printf("rte_pdump_enable_by_deviceid failed\n");
			return -1;
		}
		printf("pdump_enable_by_deviceid success\n");

		ret = rte_pdump_disable_by_deviceid(deviceid, QUEUE_ID, flags);
		if (ret < 0) {
			printf("rte_pdump_disable_by_deviceid failed\n");
			return -1;
		}
		printf("pdump_disable_by_deviceid success\n");

		if (itr == 0) {
			flags = RTE_PDUMP_FLAG_RX;
			printf("\n***** flags = RTE_PDUMP_FLAG_RX *****\n");
		} else if (itr == 1) {
			flags = RTE_PDUMP_FLAG_RXTX;
			printf("\n***** flags = RTE_PDUMP_FLAG_RXTX *****\n");
		}
	}
	if (ring_client != NULL)
		test_ring_free(ring_client);
	if (mp != NULL)
		test_mp_free(mp);

	return ret;
}

int
test_pdump_uninit(void)
{
	int ret = 0;

	ret = rte_pdump_uninit();
	if (ret < 0) {
		printf("rte_pdump_uninit failed\n");
		return -1;
	}
	if (ring_server != NULL)
		test_ring_free(ring_server);
	printf("pdump_uninit success\n");
	test_vdev_uninit("net_ring_net_ringa");
	return ret;
}

void *
send_pkts(void *empty)
{
	int ret = 0;
	struct rte_mbuf *pbuf[NUM_PACKETS] = { };
	struct rte_mempool *mp;
	char poolname[] = "mbuf_pool_server";

	ret = test_get_mbuf_from_pool(&mp, pbuf, poolname);
	if (ret < 0)
		printf("get_mbuf_from_pool failed\n");

	ret = test_dev_start(portid, mp);
	if (ret < 0)
		printf("test_dev_start(%hu, %p) failed, error code: %d\n",
			portid, mp, ret);

	while (ret >= 0 && flag_for_send_pkts) {
		ret = test_packet_forward(pbuf, portid, QUEUE_ID);
		if (ret < 0)
			printf("send pkts Failed\n");
	};

	rte_eth_dev_stop(portid);
	test_put_mbuf_to_pool(mp, pbuf);
	return empty;
}

/*
 * This function is called in the primary i.e. main test, to spawn off secondary
 * processes to run actual mp tests. Uses fork() and exec pair
 */

int
run_pdump_server_tests(void)
{
	int ret = 0;
	char coremask[10];

#ifdef RTE_EXEC_ENV_LINUX
	char tmp[PATH_MAX] = { 0 };
	char prefix[PATH_MAX] = { 0 };

	get_current_prefix(tmp, sizeof(tmp));
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#else
	const char *prefix = "";
#endif

	/* good case, using secondary */
	const char *const argv1[] = {
		prgname, "-c", coremask, "--proc-type=secondary",
		prefix
	};

	snprintf(coremask, sizeof(coremask), "%x",
		 (1 << rte_get_main_lcore()));

	ret = test_pdump_init();
	ret |= launch_p(argv1);
	ret |= test_pdump_uninit();
	return ret;
}

int
test_pdump(void)
{
	int ret = 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		printf("IN PRIMARY PROCESS\n");
		ret = run_pdump_server_tests();
		if (ret < 0)
			return TEST_FAILED;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		printf("IN SECONDARY PROCESS\n");
		sleep(5);
		ret = run_pdump_client_tests();
		if (ret < 0)
			return TEST_FAILED;
	}
	return TEST_SUCCESS;
}

REGISTER_TEST_COMMAND(pdump_autotest, test_pdump);
