/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <rte_pipeline.h>
#include <rte_log.h>
#include <inttypes.h>
#include <rte_hexdump.h>
#include "test_table.h"
#include "test_table_pipeline.h"

#if 0

static rte_pipeline_port_out_action_handler port_action_0x00
	(struct rte_mbuf **pkts, uint32_t n, uint64_t *pkts_mask, void *arg);
static rte_pipeline_port_out_action_handler port_action_0xFF
	(struct rte_mbuf **pkts, uint32_t n, uint64_t *pkts_mask, void *arg);
static rte_pipeline_port_out_action_handler port_action_stub
	(struct rte_mbuf **pkts, uint32_t n, uint64_t *pkts_mask, void *arg);


rte_pipeline_port_out_action_handler port_action_0x00(struct rte_mbuf **pkts,
	uint32_t n,
	uint64_t *pkts_mask,
	void *arg)
{
	RTE_SET_USED(pkts);
	RTE_SET_USED(n);
	RTE_SET_USED(arg);
	printf("Port Action 0x00\n");
	*pkts_mask = 0x00;
	return 0;
}

rte_pipeline_port_out_action_handler port_action_0xFF(struct rte_mbuf **pkts,
	uint32_t n,
	uint64_t *pkts_mask,
	void *arg)
{
	RTE_SET_USED(pkts);
	RTE_SET_USED(n);
	RTE_SET_USED(arg);
	printf("Port Action 0xFF\n");
	*pkts_mask = 0xFF;
	return 0;
}

rte_pipeline_port_out_action_handler port_action_stub(struct rte_mbuf **pkts,
	uint32_t n,
	uint64_t *pkts_mask,
	void *arg)
{
	RTE_SET_USED(pkts);
	RTE_SET_USED(n);
	RTE_SET_USED(pkts_mask);
	RTE_SET_USED(arg);
	printf("Port Action stub\n");
	return 0;
}

#endif

rte_pipeline_table_action_handler_hit
table_action_0x00(struct rte_pipeline *p, struct rte_mbuf **pkts,
	uint64_t pkts_mask, struct rte_pipeline_table_entry **entry, void *arg);

rte_pipeline_table_action_handler_hit
table_action_stub_hit(struct rte_pipeline *p, struct rte_mbuf **pkts,
	uint64_t pkts_mask, struct rte_pipeline_table_entry **entry, void *arg);

rte_pipeline_table_action_handler_miss
table_action_stub_miss(struct rte_pipeline *p, struct rte_mbuf **pkts,
	uint64_t pkts_mask, struct rte_pipeline_table_entry **entry, void *arg);

rte_pipeline_table_action_handler_hit
table_action_0x00(__attribute__((unused)) struct rte_pipeline *p,
	__attribute__((unused)) struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	__attribute__((unused)) struct rte_pipeline_table_entry **entry,
	__attribute__((unused)) void *arg)
{
	printf("Table Action, setting pkts_mask to 0x00\n");
	pkts_mask = ~0x00;
	rte_pipeline_ah_packet_drop(p, pkts_mask);
	return 0;
}

rte_pipeline_table_action_handler_hit
table_action_stub_hit(__attribute__((unused)) struct rte_pipeline *p,
	__attribute__((unused)) struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	__attribute__((unused)) struct rte_pipeline_table_entry **entry,
	__attribute__((unused)) void *arg)
{
	printf("STUB Table Action Hit - doing nothing\n");
	printf("STUB Table Action Hit - setting mask to 0x%"PRIx64"\n",
		override_hit_mask);
	pkts_mask = (~override_hit_mask) & 0x3;
	rte_pipeline_ah_packet_drop(p, pkts_mask);
	return 0;
}

rte_pipeline_table_action_handler_miss
table_action_stub_miss(struct rte_pipeline *p,
	__attribute__((unused)) struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	__attribute__((unused)) struct rte_pipeline_table_entry **entry,
	__attribute__((unused)) void *arg)
{
	printf("STUB Table Action Miss - setting mask to 0x%"PRIx64"\n",
		override_miss_mask);
	pkts_mask = (~override_miss_mask) & 0x3;
	rte_pipeline_ah_packet_drop(p, pkts_mask);
	return 0;
}

enum e_test_type {
	e_TEST_STUB = 0,
	e_TEST_LPM,
	e_TEST_LPM6,
	e_TEST_HASH_LRU_8,
	e_TEST_HASH_LRU_16,
	e_TEST_HASH_LRU_32,
	e_TEST_HASH_EXT_8,
	e_TEST_HASH_EXT_16,
	e_TEST_HASH_EXT_32
};

char pipeline_test_names[][64] = {
	"Stub",
	"LPM",
	"LPMv6",
	"8-bit LRU Hash",
	"16-bit LRU Hash",
	"32-bit LRU Hash",
	"16-bit Ext Hash",
	"8-bit Ext Hash",
	"32-bit Ext Hash",
	""
};


static int
cleanup_pipeline(void)
{

	rte_pipeline_free(p);

	return 0;
}


static int check_pipeline_invalid_params(void);

static int
check_pipeline_invalid_params(void)
{
	struct rte_pipeline_params pipeline_params_1 = {
		.name = NULL,
		.socket_id = 0,
	};
	struct rte_pipeline_params pipeline_params_2 = {
		.name = "PIPELINE",
		.socket_id = -1,
	};
	struct rte_pipeline_params pipeline_params_3 = {
		.name = "PIPELINE",
		.socket_id = 127,
	};

	p = rte_pipeline_create(NULL);
	if (p != NULL) {
		RTE_LOG(INFO, PIPELINE,
			"%s: configured pipeline with null params\n",
			__func__);
		goto fail;
	}
	p = rte_pipeline_create(&pipeline_params_1);
	if (p != NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Configure pipeline with NULL "
			"name\n", __func__);
		goto fail;
	}

	p = rte_pipeline_create(&pipeline_params_2);
	if (p != NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Configure pipeline with invalid "
			"socket\n", __func__);
		goto fail;
	}

	p = rte_pipeline_create(&pipeline_params_3);
	if (p != NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Configure pipeline with invalid "
			"socket\n", __func__);
		goto fail;
	}

	/* Check pipeline consistency */
	if (!rte_pipeline_check(p)) {
		rte_panic("Pipeline consistency reported as OK\n");
		goto fail;
	}


	return 0;
fail:
	return -1;
}


static int
setup_pipeline(int test_type)
{
	int ret;
	int i;
	struct rte_pipeline_params pipeline_params = {
		.name = "PIPELINE",
		.socket_id = 0,
	};

	RTE_LOG(INFO, PIPELINE, "%s: **** Setting up %s test\n",
		__func__, pipeline_test_names[test_type]);

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Failed to configure pipeline\n",
			__func__);
		goto fail;
	}

	ret = rte_pipeline_free(p);
	if (ret != 0) {
		RTE_LOG(INFO, PIPELINE, "%s: Failed to free pipeline\n",
			__func__);
		goto fail;
	}

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Failed to configure pipeline\n",
			__func__);
		goto fail;
	}


	/* Input port configuration */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = rings_rx[i],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.burst_size = BURST_SIZE,
		};

		/* Put in action for some ports */
		if (i)
			port_params.f_action = NULL;

		ret = rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i]);
		if (ret) {
			rte_panic("Unable to configure input port %d, ret:%d\n",
				i, ret);
			goto fail;
		}
	}

	/* output Port configuration */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = rings_tx[i],
			.tx_burst_sz = BURST_SIZE,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
		};

		if (i)
			port_params.f_action = port_out_action;

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i])) {
			rte_panic("Unable to configure output port %d\n", i);
			goto fail;
		}
	}

	/* Table configuration  */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_pipeline_table_params table_params = {
				.ops = &rte_table_stub_ops,
				.arg_create = NULL,
				.f_action_hit = action_handler_hit,
				.f_action_miss = action_handler_miss,
				.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id[i])) {
			rte_panic("Unable to configure table %u\n", i);
			goto fail;
		}

		if (connect_miss_action_to_table)
			if (rte_pipeline_table_create(p, &table_params,
				&table_id[i+2])) {
				rte_panic("Unable to configure table %u\n", i);
				goto fail;
			}
	}

	for (i = 0; i < N_PORTS; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id[i])) {
			rte_panic("Unable to connect input port %u to "
				"table %u\n", port_in_id[i],  table_id[i]);
			goto fail;
		}

	/* Add entries to tables */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_pipeline_table_entry default_entry = {
			.action = (enum rte_pipeline_action)
				table_entry_default_action,
			{.port_id = port_out_id[i^1]},
		};
		struct rte_pipeline_table_entry *default_entry_ptr;

		if (connect_miss_action_to_table) {
			printf("Setting first table to output to next table\n");
			default_entry.action = RTE_PIPELINE_ACTION_TABLE;
			default_entry.table_id = table_id[i+2];
		}

		/* Add the default action for the table. */
		ret = rte_pipeline_table_default_entry_add(p, table_id[i],
			&default_entry, &default_entry_ptr);
		if (ret < 0) {
			rte_panic("Unable to add default entry to table %u "
				"code %d\n", table_id[i], ret);
			goto fail;
		} else
			printf("Added default entry to table id %d with "
				"action %x\n",
				table_id[i], default_entry.action);

		if (connect_miss_action_to_table) {
			/* We create a second table so the first can pass
			traffic into it */
			struct rte_pipeline_table_entry default_entry = {
				.action = RTE_PIPELINE_ACTION_PORT,
				{.port_id = port_out_id[i^1]},
			};
			printf("Setting secont table to output to port\n");

			/* Add the default action for the table. */
			ret = rte_pipeline_table_default_entry_add(p,
				table_id[i+2],
				&default_entry, &default_entry_ptr);
			if (ret < 0) {
				rte_panic("Unable to add default entry to "
					"table %u code %d\n",
					table_id[i], ret);
				goto fail;
			} else
				printf("Added default entry to table id %d "
					"with action %x\n",
					table_id[i], default_entry.action);
		}
	}

	/* Enable input ports */
	for (i = 0; i < N_PORTS ; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0) {
		rte_panic("Pipeline consistency check failed\n");
		goto fail;
	} else
		printf("Pipeline Consistency OK!\n");

	return 0;
fail:

	return -1;
}

static int
test_pipeline_single_filter(int test_type, int expected_count)
{
	int i;
	int j;
	int ret;
	int tx_count;

	RTE_LOG(INFO, PIPELINE, "%s: **** Running %s test\n",
		__func__, pipeline_test_names[test_type]);
	/* Run pipeline once */
	for (i = 0; i < N_PORTS; i++)
		rte_pipeline_run(p);


	ret = rte_pipeline_flush(NULL);
	if (ret != -EINVAL) {
		RTE_LOG(INFO, PIPELINE,
			"%s: No pipeline flush error NULL pipeline (%d)\n",
			__func__, ret);
		goto fail;
	}

	/*
	 * Allocate a few mbufs and manually insert into the rings. */
	for (i = 0; i < N_PORTS; i++)
		for (j = 0; j < N_PORTS; j++) {
			struct rte_mbuf *m;
			uint8_t *key;
			uint32_t *k32;

			m = rte_pktmbuf_alloc(pool);
			if (m == NULL) {
				rte_panic("Failed to alloc mbuf from pool\n");
				return -1;
			}
			key = RTE_MBUF_METADATA_UINT8_PTR(m,
					APP_METADATA_OFFSET(32));

			k32 = (uint32_t *) key;
			k32[0] = 0xadadadad >> (j % 2);

			RTE_LOG(INFO, PIPELINE, "%s: Enqueue onto ring %d\n",
				__func__, i);
			rte_ring_enqueue(rings_rx[i], m);
		}

	/* Run pipeline once */
	for (i = 0; i < N_PORTS; i++)
		rte_pipeline_run(p);

   /*
	* need to flush the pipeline, as there may be less hits than the burst
	size and they will not have been flushed to the tx rings. */
	rte_pipeline_flush(p);

   /*
	* Now we'll see what we got back on the tx rings. We should see whatever
	* packets we had hits on that were destined for the output ports.
	*/
	tx_count = 0;

	for (i = 0; i < N_PORTS; i++) {
		void *objs[RING_TX_SIZE];
		struct rte_mbuf *mbuf;

		ret = rte_ring_sc_dequeue_burst(rings_tx[i], objs, 10, NULL);
		if (ret <= 0)
			printf("Got no objects from ring %d - error code %d\n",
				i, ret);
		else {
			printf("Got %d object(s) from ring %d!\n", ret, i);
			for (j = 0; j < ret; j++) {
				mbuf = objs[j];
				rte_hexdump(stdout, "Object:",
					rte_pktmbuf_mtod(mbuf, char *),
					mbuf->data_len);
				rte_pktmbuf_free(mbuf);
			}
			tx_count += ret;
		}
	}

	if (tx_count != expected_count) {
		RTE_LOG(INFO, PIPELINE,
			"%s: Unexpected packets out for %s test, expected %d, "
			"got %d\n", __func__, pipeline_test_names[test_type],
			expected_count, tx_count);
		goto fail;
	}

	cleanup_pipeline();

	return 0;
fail:
	return -1;

}

int
test_table_pipeline(void)
{
	/* TEST - All packets dropped */
	action_handler_hit = NULL;
	action_handler_miss = NULL;
	table_entry_default_action = RTE_PIPELINE_ACTION_DROP;
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 0) < 0)
		return -1;

	/* TEST - All packets passed through */
	table_entry_default_action = RTE_PIPELINE_ACTION_PORT;
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 4) < 0)
		return -1;

	/* TEST - one packet per port */
	action_handler_hit = NULL;
	action_handler_miss =
		(rte_pipeline_table_action_handler_miss) table_action_stub_miss;
	table_entry_default_action = RTE_PIPELINE_ACTION_PORT;
	override_miss_mask = 0x01; /* one packet per port */
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 2) < 0)
		return -1;

	/* TEST - one packet per port */
	override_miss_mask = 0x02; /*all per port */
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 2) < 0)
		return -1;

	/* TEST - all packets per port */
	override_miss_mask = 0x03; /*all per port */
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 4) < 0)
		return -1;

   /*
	* This test will set up two tables in the pipeline. the first table
	* will forward to another table on miss, and the second table will
	* forward to port.
	*/
	connect_miss_action_to_table = 1;
	table_entry_default_action = RTE_PIPELINE_ACTION_TABLE;
	action_handler_hit = NULL;  /* not for stub, hitmask always zero */
	action_handler_miss = NULL;
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 4) < 0)
		return -1;
	connect_miss_action_to_table = 0;

	printf("TEST - two tables, hitmask override to 0x01\n");
	connect_miss_action_to_table = 1;
	action_handler_miss =
		(rte_pipeline_table_action_handler_miss)table_action_stub_miss;
	override_miss_mask = 0x01;
	setup_pipeline(e_TEST_STUB);
	if (test_pipeline_single_filter(e_TEST_STUB, 2) < 0)
		return -1;
	connect_miss_action_to_table = 0;

	if (check_pipeline_invalid_params()) {
		RTE_LOG(INFO, PIPELINE, "%s: Check pipeline invalid params "
			"failed.\n", __func__);
		return -1;
	}

	return 0;
}
