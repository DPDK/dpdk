/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_PRIV_H
#define APP_GRAPH_PRIV_H

#define MAX_GRAPH_USECASES 32

enum graph_model {
	GRAPH_MODEL_RTC = 0x01,
	GRAPH_MODEL_MCD = 0x02,
};

struct usecases {
	char name[32];
	bool enabled;
};

struct usecase_params {
	uint64_t coremask;
	uint32_t bsz;
	uint32_t tmo;
};

struct graph_config {
	struct usecases usecases[MAX_GRAPH_USECASES];
	struct usecase_params params;
	enum graph_model model;
	uint64_t num_pcap_pkts;
	char *pcap_file;
	uint8_t pcap_ena;
};

#endif
