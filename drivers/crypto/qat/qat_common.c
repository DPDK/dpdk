/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_common.h"
#include "qat_logs.h"

int
qat_sgl_fill_array(struct rte_mbuf *buf, uint64_t buf_start,
		struct qat_sgl *list, uint32_t data_len)
{
	int nr = 1;

	uint32_t buf_len = rte_pktmbuf_iova(buf) -
			buf_start + rte_pktmbuf_data_len(buf);

	list->buffers[0].addr = buf_start;
	list->buffers[0].resrvd = 0;
	list->buffers[0].len = buf_len;

	if (data_len <= buf_len) {
		list->num_bufs = nr;
		list->buffers[0].len = data_len;
		return 0;
	}

	buf = buf->next;
	while (buf) {
		if (unlikely(nr == QAT_SGL_MAX_NUMBER)) {
			PMD_DRV_LOG(ERR,
				"QAT PMD exceeded size of QAT SGL entry(%u)",
					QAT_SGL_MAX_NUMBER);
			return -EINVAL;
		}

		list->buffers[nr].len = rte_pktmbuf_data_len(buf);
		list->buffers[nr].resrvd = 0;
		list->buffers[nr].addr = rte_pktmbuf_iova(buf);

		buf_len += list->buffers[nr].len;
		buf = buf->next;

		if (buf_len > data_len) {
			list->buffers[nr].len -=
				buf_len - data_len;
			buf = NULL;
		}
		++nr;
	}
	list->num_bufs = nr;

	return 0;
}
