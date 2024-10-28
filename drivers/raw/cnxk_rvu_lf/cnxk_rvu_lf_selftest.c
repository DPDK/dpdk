/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <stdlib.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_eal.h>
#include <rte_lcore.h>

#include <rte_rawdev.h>
#include <rte_cycles.h>

#include "cnxk_rvu_lf.h"
#include "cnxk_rvu_lf_driver.h"

#define PF		0
#define VF		0
#define RSP_LEN		64
#define REQ_LEN		64
#define MSG_ID_FROM	0x3000
#define MSG_ID_TO	0x4000
#define MAX_BAR		6

static int
msg_process_notify_cb(uint16_t vf, uint16_t msg_id,
		      void *req, uint16_t req_len, void **rsp, uint16_t *rsp_len)
{
	uint8_t *resp;
	int i;

	printf("\nReceived message(0x%x) from VF0x%x\n", msg_id, vf);
	rte_hexdump(stdout, "req_data received", req, req_len);

	resp = malloc(RSP_LEN);
	if (resp == NULL)
		return -ENOMEM;
	for (i = 0; i < RSP_LEN; i++)
		resp[i] = 0xB0;
	*rsp = resp;
	*rsp_len = RSP_LEN;
	rte_hexdump(stdout, "rsp_data_filled", *rsp, RSP_LEN);

	return 0;
}

int
rvu_lf_rawdev_selftest(uint16_t dev_id)
{
	char *dev_name = rte_rawdevs[dev_id].name;
	uint8_t req[REQ_LEN] = {0};
	uint8_t rsp[RSP_LEN] = {0};
	size_t bar_mask = 0;
	size_t bar_va = 0;
	unsigned int i, j;
	uint16_t pf_func;
	char *token[2];
	int func, ret;

	token[0] = strtok_r(dev_name, ".", &dev_name);
	token[1] = strtok_r(dev_name, ".", &dev_name);
	func = atoi(token[1]);

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	pf_func = rte_pmd_rvu_lf_npa_pf_func_get();
	if (pf_func == 0)
		CNXK_RVU_LF_LOG(WARNING, "NPA pf_func is invalid");

	pf_func = rte_pmd_rvu_lf_sso_pf_func_get();
	if (pf_func == 0)
		CNXK_RVU_LF_LOG(WARNING, "SSO pf_func is invalid");

	pf_func = rte_pmd_rvu_lf_pf_func_get(dev_id);
	if (pf_func == 0)
		CNXK_RVU_LF_LOG(WARNING, "RVU-LF pf_func is invalid");

	for (i = 0; i < MAX_BAR; i++) {
		if (!rte_pmd_rvu_lf_bar_get(dev_id, i, &bar_va, &bar_mask))
			printf("\n BAR[%d]: addr: 0x%" PRIx64 ", mask: 0x%" PRIx64 "\n",
					i, bar_va, bar_mask);
	}

	ret = rte_pmd_rvu_lf_msg_id_range_set(dev_id, MSG_ID_FROM, MSG_ID_TO);
	if (ret) {
		CNXK_RVU_LF_LOG(ERR, "RVU message ID range invalid");
		goto out;
	}

	ret = rte_pmd_rvu_lf_msg_handler_register(dev_id, msg_process_notify_cb);
	if (ret) {
		CNXK_RVU_LF_LOG(ERR, "RVU message handler register failed, ret: %d", ret);
		goto out;
	}

	if (func == 0) {
		j = 50;
		printf("\n");
		while (j--) {
		/* PF will wait for RVU message callbacks to be called */
			rte_delay_ms(1000);
			printf("PF waiting for VF messages for %d sec.\r", j);
		}
		/* PF will send the messages and receive responses. */
		for (i = 0; i < REQ_LEN; i++)
			req[i] = 0xC0;
		/*
		 * Range is set as between MSG_ID_FROM and MSG_ID_TO.
		 * Messages sent with this id will be serviced by VF..
		 */
		ret = rte_pmd_rvu_lf_msg_process(dev_id,
					     VF /* Send to VF0 */,
					     MSG_ID_FROM + 0x2,
					     req, REQ_LEN, rsp, RSP_LEN);
		if (ret) {
			CNXK_RVU_LF_LOG(ERR, "rvu lf PF->VF message send failed");
			goto unregister;
		}
		CNXK_RVU_LF_LOG(INFO, "RVU PF->VF message processed");
		rte_hexdump(stdout, "rsp_data received", rsp, RSP_LEN);
		j = 50;
		printf("\n");
		while (j--) {
			rte_delay_ms(1000);
			printf("PF waiting for VF to exit for %d sec.\r", j);
		}

	} else {
		/* VF will send the messages and receive responses. */
		for (i = 0; i < REQ_LEN; i++)
			req[i] = 0xA0;
		/*
		 * Range is set as between MSG_ID_FROM and MSG_ID_TO
		 * Messages sent with this id will be serviced by PF and will
		 * not be forwarded to AF.
		 */
		ret = rte_pmd_rvu_lf_msg_process(dev_id,
					     PF /* Send to PF */,
					     MSG_ID_FROM + 0x1,
					     req, REQ_LEN, rsp, RSP_LEN);
		if (ret) {
			CNXK_RVU_LF_LOG(ERR, "rvu lf VF->PF message send failed");
			goto unregister;
		}
		CNXK_RVU_LF_LOG(INFO, "RVU VF->PF message processed");
		rte_hexdump(stdout, "rsp_data received", rsp, RSP_LEN);
		j = 50;
		printf("\n");
		while (j--) {
			rte_delay_ms(1000);
			printf("VF waiting for PF to send msg for %d sec.\r", j);
		}
	}
unregister:
	rte_pmd_rvu_lf_msg_handler_unregister(dev_id);
out:
	rte_rawdev_stop(dev_id);

	return ret;
}


