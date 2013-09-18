/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <cmdline_parse.h>

#include "test.h"

#ifdef RTE_LIBRTE_KNI
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_kni.h>

#define NB_MBUF          (8192 * 16)
#define MAX_PACKET_SZ    2048
#define MBUF_SZ \
	(MAX_PACKET_SZ + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define PKT_BURST_SZ     32
#define MEMPOOL_CACHE_SZ PKT_BURST_SZ
#define SOCKET           0
#define NB_RXD           128
#define NB_TXD           512
#define KNI_TIMEOUT_MS   5000 /* ms */

#define IFCONFIG "/sbin/ifconfig"

/* The threshold number of mbufs to be transmitted or received. */
#define KNI_NUM_MBUF_THRESHOLD 100
static int kni_pkt_mtu = 0;

struct test_kni_stats {
	volatile uint64_t ingress;
	volatile uint64_t egress;
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
	.rx_free_thresh = 0,
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = 36,
		.hthresh = 0,
		.wthresh = 0,
	},
	.tx_free_thresh = 0,
	.tx_rs_thresh = 0,
};

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.header_split = 0,
		.hw_ip_checksum = 0,
		.hw_vlan_filter = 0,
		.jumbo_frame = 0,
		.hw_strip_crc = 0,
	},
	.txmode = {
		.mq_mode = ETH_DCB_NONE,
	},
};

static struct rte_kni_ops kni_ops = {
	.change_mtu = NULL,
	.config_network_if = NULL,
};

static unsigned lcore_master, lcore_ingress, lcore_egress;
static struct rte_kni *test_kni_ctx;
static struct test_kni_stats stats;

static volatile uint32_t test_kni_processing_flag;

static struct rte_mempool *
test_kni_create_mempool(void)
{
	struct rte_mempool * mp;

	mp = rte_mempool_lookup("kni_mempool");
	if (!mp)
		mp = rte_mempool_create("kni_mempool",
				NB_MBUF,
				MBUF_SZ,
				MEMPOOL_CACHE_SZ,
				sizeof(struct rte_pktmbuf_pool_private),
				rte_pktmbuf_pool_init,
				NULL,
				rte_pktmbuf_init,
				NULL,
				SOCKET,
				0);

	return mp;
}

static struct rte_mempool *
test_kni_lookup_mempool(void)
{
	return rte_mempool_lookup("kni_mempool");
}
/* Callback for request of changing MTU */
static int
kni_change_mtu(uint8_t port_id, unsigned new_mtu)
{
	printf("Change MTU of port %d to %u\n", port_id, new_mtu);
	kni_pkt_mtu = new_mtu;	
	printf("Change MTU of port %d to %i successfully.\n",
					 port_id, kni_pkt_mtu);
	return 0;
}
/**
 * This loop fully tests the basic functions of KNI. e.g. transmitting,
 * receiving to, from kernel space, and kernel requests.
 *
 * This is the loop to transmit/receive mbufs to/from kernel interface with
 * supported by KNI kernel module. The ingress lcore will allocate mbufs and
 * transmit them to kernel space; while the egress lcore will receive the mbufs
 * from kernel space and free them.
 * On the master lcore, several commands will be run to check handling the
 * kernel requests. And it will finally set the flag to exit the KNI
 * transmitting/receiving to/from the kernel space.
 *
 * Note: To support this testing, the KNI kernel module needs to be insmodded
 * in one of its loopback modes.
 */
static int
test_kni_loop(__rte_unused void *arg)
{
	int ret = 0;
	unsigned nb_rx, nb_tx, num, i;
	const unsigned lcore_id = rte_lcore_id();
	struct rte_mbuf *pkts_burst[PKT_BURST_SZ];

	if (lcore_id == lcore_master) {
		rte_delay_ms(KNI_TIMEOUT_MS);
		/* tests of handling kernel request */
		if (system(IFCONFIG " vEth0 up") == -1)
			ret = -1;
		if (system(IFCONFIG " vEth0 mtu 1400") == -1)
			ret = -1;
		if (system(IFCONFIG " vEth0 down") == -1)
			ret = -1;
		rte_delay_ms(KNI_TIMEOUT_MS);
		test_kni_processing_flag = 1;
	} else if (lcore_id == lcore_ingress) {
		struct rte_mempool *mp = test_kni_lookup_mempool();

		if (mp == NULL)
			return -1;

		while (1) {
			if (test_kni_processing_flag)
				break;

			for (nb_rx = 0; nb_rx < PKT_BURST_SZ; nb_rx++) {
				pkts_burst[nb_rx] = rte_pktmbuf_alloc(mp);
				if (!pkts_burst[nb_rx])
					break;
			}

			num = rte_kni_tx_burst(test_kni_ctx, pkts_burst,
								nb_rx);
			stats.ingress += num;
			rte_kni_handle_request(test_kni_ctx);
			if (num < nb_rx) {
				for (i = num; i < nb_rx; i++) {
					rte_pktmbuf_free(pkts_burst[i]);
				}
			}
		}
	} else if (lcore_id == lcore_egress) {
		while (1) {
			if (test_kni_processing_flag)
				break;
			num = rte_kni_rx_burst(test_kni_ctx, pkts_burst,
							PKT_BURST_SZ);
			stats.egress += num;
			for (nb_tx = 0; nb_tx < num; nb_tx++)
				rte_pktmbuf_free(pkts_burst[nb_tx]);
		}
	}

	return ret;
}

static int
test_kni_allocate_lcores(void)
{
	unsigned i, count = 0;

	lcore_master = rte_get_master_lcore();
	printf("master lcore: %u\n", lcore_master);
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (count >=2 )
			break;
		if (rte_lcore_is_enabled(i) && i != lcore_master) {
			count ++;
			if (count == 1)
				lcore_ingress = i;
			else if (count == 2)
				lcore_egress = i;
		}
	}
	printf("count: %u\n", count);

	return (count == 2 ? 0 : -1);
}

static int
test_kni_processing(uint8_t pid, struct rte_mempool *mp)
{
	int ret = 0;
	unsigned i;
	struct rte_kni *kni;
	int p_id,p_ret;
	int status;

	if (!mp)
		return -1;

	/* basic test of kni processing */
	kni = rte_kni_create(pid, MAX_PACKET_SZ, mp, &kni_ops);
	if (!kni) {
		printf("fail to create kni\n");
		return -1;
	}
	if (rte_kni_get_port_id(kni) != pid) {
		printf("fail to get port id\n");
		ret = -1;
		goto fail_kni;
	}

	test_kni_ctx = kni;
	test_kni_processing_flag = 0;
	stats.ingress = 0;
	stats.egress = 0;

	/* create a subprocess to test the APIs of supporting multi-process */
	p_id = fork();
	if (p_id == 0) {
		struct rte_kni *kni_test;
	#define TEST_MTU_SIZE  1450
		kni_test = rte_kni_info_get(RTE_MAX_ETHPORTS);
		if (kni_test) {
			printf("unexpectedly gets kni successfully with an invalid "
									"port id\n");
			exit(-1);
		}
		kni_test = rte_kni_info_get(pid);
		if (NULL == kni_test) {
			printf("Failed to get KNI info of the port %d\n",pid);
			exit(-1);
		}
		struct rte_kni_ops kni_ops_test = {
			.change_mtu = kni_change_mtu,
			.config_network_if = NULL,
		};
		/* test of registering kni with NULL ops */	
		if (rte_kni_register_handlers(kni_test,NULL) == 0) {
			printf("unexpectedly register kni successfully"
						"with NULL ops\n");
			exit(-1);
		}
		if (rte_kni_register_handlers(kni_test,&kni_ops_test) < 0) {
			printf("Failed to register KNI request handler"
						"of the port %d\n",pid);
			exit(-1);
		}
		if (system(IFCONFIG " vEth0 mtu 1450") == -1) 
			exit(-1);
		
		rte_kni_handle_request(kni_test);
		if (kni_pkt_mtu != TEST_MTU_SIZE) {
			printf("Failed to change kni MTU\n");
			exit(-1) ;
		}

		/* test of unregistering kni request */
		kni_pkt_mtu = 0;
		if (rte_kni_unregister_handlers(kni_test) < 0) {
			printf("Failed to unregister kni request handlers\n");
			exit(-1);
		}
		if (system(IFCONFIG " vEth0 mtu 1450") == -1)
			exit(-1);

		rte_kni_handle_request(kni_test);
		if (kni_pkt_mtu != 0) {
			printf("Failed to test kni unregister handlers\n");
			exit(-1);		
		}
		exit(0);
	}else if (p_id < 0) {
		printf("Failed to fork a process\n");
		return -1;
	}else {
		p_ret = wait(&status);
		if (WIFEXITED(status)) 
			printf("test of multi-process api passed.\n");
		else {
			printf("KNI test:The child process %d exit abnormally./n",p_ret);
			return -1;
		}
	}
	rte_eal_mp_remote_launch(test_kni_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(i) {
		if (rte_eal_wait_lcore(i) < 0) {
			ret = -1;
			goto fail_kni;
		}
	}
	/**
	 * Check if the number of mbufs received from kernel space is equal
	 * to that of transmitted to kernel space
	 */
	if (stats.ingress < KNI_NUM_MBUF_THRESHOLD ||
		stats.egress < KNI_NUM_MBUF_THRESHOLD) {
		printf("The ingress/egress number should not be "
			"less than %u\n", (unsigned)KNI_NUM_MBUF_THRESHOLD);
		ret = -1;
		goto fail_kni;
	}

	/* test of creating kni on a port which has been used for a kni */
	if (rte_kni_create(pid, MAX_PACKET_SZ, mp, &kni_ops) != NULL) {
		printf("should not create a kni successfully for a port which"
						"has been used for a kni\n");
		ret = -1;
		goto fail_kni;
	}

	if (rte_kni_release(kni) < 0) {
		printf("fail to release kni\n");
		return -1;
	}
	test_kni_ctx = NULL;
	
	/* test of releasing a released kni device */
	if (rte_kni_release(kni) == 0) {
		printf("should not release a released kni device\n");
		return -1;
	}

	/* test of reusing memzone */
	kni = rte_kni_create(pid, MAX_PACKET_SZ, mp, &kni_ops);
	if (!kni) {
		printf("fail to create kni\n");
		return -1;
	}

	/* Release the kni for following testing */
	if (rte_kni_release(kni) < 0) {
		printf("fail to release kni\n");
		return -1;
	}
	
	return ret;
fail_kni:
	if (rte_kni_release(kni) < 0) {
		printf("fail to release kni\n");
		ret = -1;
	}

	return ret;
}

int
test_kni(void)
{
	int ret = -1;
	uint8_t nb_ports, pid;
	struct rte_kni *kni;
	struct rte_mempool * mp;

	if (test_kni_allocate_lcores() < 0) {
		printf("No enough lcores for kni processing\n");
		return -1;
	}

	mp = test_kni_create_mempool();
	if (!mp) {
		printf("fail to create mempool for kni\n");
		return -1;
	}
	ret = rte_pmd_init_all();
	if (ret < 0) {
		printf("fail to initialize PMD\n");
		return -1;
	}
	ret = rte_eal_pci_probe();
	if (ret < 0) {
		printf("fail to probe PCI devices\n");
		return -1;
	}

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0) {
		printf("no supported nic port found\n");
		return -1;
	}

	/* configuring port 0 for the test is enough */
	pid = 0;
	ret = rte_eth_dev_configure(pid, 1, 1, &port_conf);
	if (ret < 0) {
		printf("fail to configure port %d\n", pid);
		return -1;
	}

	ret = rte_eth_rx_queue_setup(pid, 0, NB_RXD, SOCKET, &rx_conf, mp);
	if (ret < 0) {
		printf("fail to setup rx queue for port %d\n", pid);
		return -1;
	}

	ret = rte_eth_tx_queue_setup(pid, 0, NB_TXD, SOCKET, &tx_conf);
	if (ret < 0) {
		printf("fail to setup tx queue for port %d\n", pid);
		return -1;
	}

	ret = rte_eth_dev_start(pid);
	if (ret < 0) {
		printf("fail to start port %d\n", pid);
		return -1;
	}

	rte_eth_promiscuous_enable(pid);

	/* basic test of kni processing */
	ret = test_kni_processing(pid, mp);
	if (ret < 0)
		goto fail;

	/* test of creating kni with port id exceeds the maximum */
	kni = rte_kni_create(RTE_MAX_ETHPORTS, MAX_PACKET_SZ, mp, &kni_ops);
	if (kni) {
		printf("unexpectedly creates kni successfully with an invalid "
								"port id\n");
		goto fail;
	}

	/* test of creating kni with NULL mempool pointer */
	kni = rte_kni_create(pid, MAX_PACKET_SZ, NULL, &kni_ops);
	if (kni) {
		printf("unexpectedly creates kni successfully with NULL "
							"mempool pointer\n");
		goto fail;
	}

	/* test of creating kni with NULL ops */
	kni = rte_kni_create(pid, MAX_PACKET_SZ, mp, NULL);
	if (!kni) { 
		printf("unexpectedly creates kni falied with NULL ops\n");
		goto fail;
	}

	/* test of releasing kni with NULL ops */
	if (rte_kni_release(kni) < 0) {
		printf("fail to release kni\n");
		goto fail;
	}	

	/* test of getting port id according to NULL kni context */
	if (rte_kni_get_port_id(NULL) < RTE_MAX_ETHPORTS) {
		printf("unexpectedly get port id successfully by NULL kni "
								"pointer\n");
		goto fail;
	}

	/* test of releasing NULL kni context */
	ret = rte_kni_release(NULL);
	if (ret == 0) {
		printf("unexpectedly release kni successfully\n");
		goto fail;
	}

	ret = 0;

fail:
	rte_eth_dev_stop(pid);

	return ret;
}

#else /* RTE_LIBRTE_KNI */

int
test_kni(void)
{
	printf("The KNI library is not included in this build\n");
	return 0;
}

#endif /* RTE_LIBRTE_KNI */
