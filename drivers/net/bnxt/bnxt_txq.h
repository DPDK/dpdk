/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
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
 *     * Neither the name of Broadcom Corporation nor the names of its
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

#ifndef _BNXT_TXQ_H_
#define _BNXT_TXQ_H_

struct bnxt_tx_ring_info;
struct bnxt_cp_ring_info;
struct bnxt_tx_queue {
	uint16_t		nb_tx_desc;    /* number of TX descriptors */
	uint16_t		tx_free_thresh;/* minimum TX before freeing */
	/** Index to last TX descriptor to have been cleaned. */
	uint16_t		last_desc_cleaned;
	/** Total number of TX descriptors ready to be allocated. */
	uint16_t		tx_next_dd; /* next desc to scan for DD bit */
	uint16_t		tx_next_rs; /* next desc to set RS bit */
	uint16_t		queue_id; /* TX queue index */
	uint16_t		reg_idx; /* TX queue register index */
	uint8_t			port_id; /* Device port identifier */
	uint8_t			pthresh; /* Prefetch threshold register */
	uint8_t			hthresh; /* Host threshold register */
	uint8_t			wthresh; /* Write-back threshold reg */
	uint32_t		txq_flags; /* Holds flags for this TXq */
	uint32_t		ctx_curr; /* Hardware context states */
	uint8_t			tx_deferred_start; /* not in global dev start */

	struct bnxt		*bp;
	int			index;
	int			tx_wake_thresh;
	struct bnxt_tx_ring_info	*tx_ring;

	unsigned int		cp_nr_rings;
	struct bnxt_cp_ring_info	*cp_ring;
};

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq);
void bnxt_free_tx_mbufs(struct bnxt *bp);
void bnxt_tx_queue_release_op(void *tx_queue);
int bnxt_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_txconf *tx_conf);

#endif
