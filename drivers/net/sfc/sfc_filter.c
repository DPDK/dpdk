/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_common.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"

boolean_t
sfc_filter_is_match_supported(struct sfc_adapter *sa, uint32_t match)
{
	struct sfc_filter *filter = &sa->filter;
	size_t i;

	for (i = 0; i < filter->supported_match_num; ++i) {
		if (match == filter->supported_match[i])
			return B_TRUE;
	}

	return B_FALSE;
}

static int
sfc_filter_cache_match_supported(struct sfc_adapter *sa)
{
	struct sfc_filter *filter = &sa->filter;
	size_t num = filter->supported_match_num;
	uint32_t *buf = filter->supported_match;
	unsigned int retry;
	int rc;

	/* Just a guess of possibly sufficient entries */
	if (num == 0)
		num = 16;

	for (retry = 0; retry < 2; ++retry) {
		if (num != filter->supported_match_num) {
			rc = ENOMEM;
			buf = rte_realloc(buf, num * sizeof(*buf), 0);
			if (buf == NULL)
				goto fail_realloc;
		}

		rc = efx_filter_supported_filters(sa->nic, buf, num, &num);
		if (rc == 0) {
			filter->supported_match_num = num;
			filter->supported_match = buf;

			return 0;
		} else if (rc != ENOSPC) {
			goto fail_efx_filter_supported_filters;
		}
	}

	SFC_ASSERT(rc == ENOSPC);

fail_efx_filter_supported_filters:
fail_realloc:
	/* Original pointer is not freed by rte_realloc() on failure */
	rte_free(buf);
	filter->supported_match = NULL;
	filter->supported_match_num = 0;
	return rc;
}

int
sfc_filter_attach(struct sfc_adapter *sa)
{
	int rc;
	unsigned int i;

	sfc_log_init(sa, "entry");

	rc = efx_filter_init(sa->nic);
	if (rc != 0)
		goto fail_filter_init;

	rc = sfc_filter_cache_match_supported(sa);
	if (rc != 0)
		goto fail_cache_match_supported;

	efx_filter_fini(sa->nic);

	sa->filter.supports_ip_proto_or_addr_filter = B_FALSE;
	sa->filter.supports_rem_or_local_port_filter = B_FALSE;
	for (i = 0; i < sa->filter.supported_match_num; ++i) {
		if (sa->filter.supported_match[i] &
		    (EFX_FILTER_MATCH_IP_PROTO | EFX_FILTER_MATCH_LOC_HOST |
		     EFX_FILTER_MATCH_REM_HOST))
			sa->filter.supports_ip_proto_or_addr_filter = B_TRUE;

		if (sa->filter.supported_match[i] &
		    (EFX_FILTER_MATCH_LOC_PORT | EFX_FILTER_MATCH_REM_PORT))
			sa->filter.supports_rem_or_local_port_filter = B_TRUE;
	}

	sfc_log_init(sa, "done");

	return 0;

fail_cache_match_supported:
	efx_filter_fini(sa->nic);

fail_filter_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_filter_detach(struct sfc_adapter *sa)
{
	struct sfc_filter *filter = &sa->filter;

	sfc_log_init(sa, "entry");

	rte_free(filter->supported_match);
	filter->supported_match = NULL;
	filter->supported_match_num = 0;

	sfc_log_init(sa, "done");
}
