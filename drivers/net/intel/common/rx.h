/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _COMMON_INTEL_RX_H_
#define _COMMON_INTEL_RX_H_

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_vect.h>

#include "desc.h"

#define CI_RX_MAX_BURST 32
#define CI_RX_MAX_NSEG 2
#define CI_VPMD_RX_BURST            32
#define CI_VPMD_DESCS_PER_LOOP      4
#define CI_VPMD_DESCS_PER_LOOP_WIDE 8
#define CI_VPMD_RX_REARM_THRESH     64

struct ci_rx_queue;

struct ci_rx_entry {
	struct rte_mbuf *mbuf; /* mbuf associated with RX descriptor. */
};

struct ci_rx_entry_sc {
	struct rte_mbuf *fbuf; /* First segment of the fragmented packet.*/
};

typedef void (*ci_rx_release_mbufs_t)(struct ci_rx_queue *rxq);

/**
 * Structure associated with each RX queue.
 */
struct ci_rx_queue {
	struct rte_mempool  *mp; /**< mbuf pool to populate RX ring. */
	union { /* RX ring virtual address */
		volatile union ixgbe_adv_rx_desc *ixgbe_rx_ring;
		volatile union ci_rx_desc *rx_ring;
		volatile union ci_rx_flex_desc *rx_flex_ring;
	};
	volatile uint8_t *qrx_tail;   /**< register address of tail */
	struct ci_rx_entry *sw_ring; /**< address of RX software ring. */
	struct ci_rx_entry_sc *sw_sc_ring; /**< address of scattered Rx software ring. */
	rte_iova_t rx_ring_phys_addr; /**< RX ring DMA address. */
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg; /**< Last segment of current packet. */
	/** hold packets to return to application */
	struct rte_mbuf *rx_stage[CI_RX_MAX_BURST * 2];
	uint16_t nb_rx_desc; /**< number of RX descriptors. */
	uint16_t rx_tail;  /**< current value of tail register. */
	uint16_t rx_nb_avail; /**< nr of staged pkts ready to ret to app */
	uint16_t nb_rx_hold; /**< number of held free RX desc. */
	uint16_t rx_next_avail; /**< idx of next staged pkt to ret to app */
	uint16_t rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
	uint16_t rxrearm_nb;     /**< number of remaining to be re-armed */
	uint16_t rxrearm_start;  /**< the idx we start the re-arming from */
	uint16_t queue_id; /**< RX queue index. */
	uint16_t port_id;  /**< Device port identifier. */
	uint16_t reg_idx;  /**< RX queue register index. */
	uint16_t rx_buf_len; /* The packet buffer size */
	uint16_t rx_hdr_len; /* The header buffer size */
	uint16_t max_pkt_len; /* Maximum packet length */
	uint8_t crc_len;  /**< 0 if CRC stripped, 4 otherwise. */
	bool q_set; /**< indicate if rx queue has been configured */
	bool rx_deferred_start; /**< queue is not started on dev start. */
	bool fdir_enabled; /* 0 if FDIR disabled, 1 when enabled */
	bool vector_rx; /**< indicates that vector RX is in use */
	bool drop_en;  /**< if 1, drop packets if no descriptors are available. */
	uint64_t mbuf_initializer; /**< value to init mbufs */
	uint64_t offloads; /**< Rx offloads with RTE_ETH_RX_OFFLOAD_* */
	uint32_t rxdid; /**< RX descriptor format ID. */
	uint32_t proto_xtr; /* protocol extraction type */
	uint64_t xtr_ol_flag; /* flexible descriptor metadata extraction offload flag */
	ptrdiff_t xtr_field_offs; /* Protocol extraction matedata offset*/
	uint64_t hw_time_update; /**< Last time HW timestamp was updated */
	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	union { /* the VSI this queue belongs to */
		struct i40e_vsi *i40e_vsi;
		struct ice_vsi *ice_vsi;
		struct iavf_vsi *iavf_vsi;
	};
	const struct rte_memzone *mz;
	union {
		struct { /* ixgbe specific values */
			/** flags to set in mbuf when a vlan is detected. */
			uint64_t vlan_flags;
			/** Packet type mask for different NICs. */
			uint16_t pkt_type_mask;
			/** indicates that IPsec RX feature is in use */
			uint8_t using_ipsec;
			/** UDP frames with a 0 checksum can be marked as checksum errors. */
			uint8_t rx_udp_csum_zero_err;
		};
		struct { /* i40e specific values */
			uint8_t hs_mode; /**< Header Split mode */
			uint8_t dcb_tc; /**< Traffic class of rx queue */
		};
		struct { /* ice specific values */
			ci_rx_release_mbufs_t rx_rel_mbufs; /**< release mbuf function */
			/** holds buffer split information */
			struct rte_eth_rxseg_split rxseg[CI_RX_MAX_NSEG];
			struct ci_rx_entry *sw_split_buf; /**< Buffer split SW ring */
			uint32_t rxseg_nb; /**< number of buffer split segments */
			uint32_t time_high; /* high 32 bits of hardware timestamp register */
			uint32_t hw_time_high; /* high 32 bits of timestamp */
			uint32_t hw_time_low; /* low 32 bits of timestamp */
			int ts_offset; /* dynamic mbuf timestamp field offset */
			uint64_t ts_flag; /* dynamic mbuf timestamp flag */
		};
		struct { /* iavf specific values */
			const struct iavf_rxq_ops *ops; /**< queue ops */
			struct iavf_rx_queue_stats *stats; /**< per-queue stats */
			uint64_t phc_time; /**< HW timestamp */
			uint8_t rel_mbufs_type; /**< type of release mbuf function */
			uint8_t rx_flags; /**< Rx VLAN tag location flags */
#define IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG1     BIT(0)
#define IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2   BIT(1)
		};
	};
};

struct ci_rx_path_features_extra {
	bool scattered;
	bool flex_desc;
	bool bulk_alloc;
	bool disabled;
	bool single_queue;
};

struct ci_rx_path_features {
	uint32_t rx_offloads;
	enum rte_vect_max_simd simd_width;
	struct ci_rx_path_features_extra extra;
};

struct ci_rx_path_info {
	eth_rx_burst_t pkt_burst;
	const char *info;
	struct ci_rx_path_features features;
};

static inline uint16_t
ci_rx_reassemble_packets(struct rte_mbuf **rx_bufs, uint16_t nb_bufs, uint8_t *split_flags,
		struct rte_mbuf **pkt_first_seg, struct rte_mbuf **pkt_last_seg,
		const uint8_t crc_len)
{
	struct rte_mbuf *pkts[CI_RX_MAX_BURST] = {0}; /*finished pkts*/
	struct rte_mbuf *start = *pkt_first_seg;
	struct rte_mbuf *end = *pkt_last_seg;
	unsigned int pkt_idx, buf_idx;

	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += crc_len;

			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
				start->hash = end->hash;
				start->vlan_tci = end->vlan_tci;
				start->ol_flags = end->ol_flags;
				/* we need to strip crc for the whole packet */
				start->pkt_len -= crc_len;
				if (end->data_len > crc_len) {
					end->data_len -= crc_len;
				} else {
					/* free up last mbuf */
					struct rte_mbuf *secondlast = start;

					start->nb_segs--;
					while (secondlast->next != end)
						secondlast = secondlast->next;
					secondlast->data_len -= (crc_len - end->data_len);
					secondlast->next = NULL;
					rte_pktmbuf_free_seg(end);
				}
				pkts[pkt_idx++] = start;
				start = NULL;
				end = NULL;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			start = rx_bufs[buf_idx];
			end = start;
			rx_bufs[buf_idx]->data_len += crc_len;
			rx_bufs[buf_idx]->pkt_len += crc_len;
		}
	}

	/* save the partial packet for next time */
	*pkt_first_seg = start;
	*pkt_last_seg = end;
	memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static inline uint64_t
ci_rxq_mbuf_initializer(uint16_t port_id)
{
	struct rte_mbuf mb_def = {
		.nb_segs = 1,
		.data_off = RTE_PKTMBUF_HEADROOM,
		.port = port_id,
	};
	rte_mbuf_refcnt_set(&mb_def, 1);

	return mb_def.rearm_data[0];
}

/* basic checks for a vector-driver capable queue.
 * Individual drivers may have other further tests beyond this.
 */
static inline bool
ci_rxq_vec_capable(uint16_t nb_desc, uint16_t rx_free_thresh)
{
	if (!rte_is_power_of_2(nb_desc) ||
			rx_free_thresh < CI_RX_MAX_BURST ||
			(nb_desc % rx_free_thresh) != 0)
		return false;

	return true;
}

/**
 * Select the best matching Rx path based on features
 *
 * @param req_features
 *   The requested features for the Rx path
 * @param infos
 *   Array of information about the available Rx paths
 * @param num_paths
 *   Number of available paths in the infos array
 * @param default_path
 *   Index of the default path to use if no suitable path is found
 *
 * @return
 *   The packet burst function index that best matches the requested features,
 *   or default_path if no suitable path is found
 */
static inline int
ci_rx_path_select(struct ci_rx_path_features req_features,
			const struct ci_rx_path_info *infos,
			int num_paths,
			int default_path)
{
	int i, idx = default_path;
	const struct ci_rx_path_features *current_features = NULL;

	for (i = 0; i < num_paths; i++) {
		const struct ci_rx_path_features *path_features = &infos[i].features;

		/* Do not select a disabled rx path. */
		if (path_features->extra.disabled)
			continue;

		/* If requested, ensure the path uses the flexible descriptor. */
		if (path_features->extra.flex_desc != req_features.extra.flex_desc)
			continue;

		/* If requested, ensure the path supports single queue RX. */
		if (path_features->extra.single_queue != req_features.extra.single_queue)
			continue;

		/* If requested, ensure the path supports scattered RX. */
		if (path_features->extra.scattered != req_features.extra.scattered)
			continue;

		/* Do not use a bulk alloc path if not requested. */
		if (path_features->extra.bulk_alloc && !req_features.extra.bulk_alloc)
			continue;

		/* Ensure the path supports the requested RX offloads. */
		if ((path_features->rx_offloads & req_features.rx_offloads) !=
				req_features.rx_offloads)
			continue;

		/* Ensure the path's SIMD width is compatible with the requested width. */
		if (path_features->simd_width > req_features.simd_width)
			continue;

		/* Do not select the path if it is less suitable than the current path. */
		if (current_features != NULL) {
			/* Do not select paths with lower SIMD width than the current path. */
			if (path_features->simd_width < current_features->simd_width)
				continue;
			/* Do not select paths with more offloads enabled than the current path. */
			if (rte_popcount32(path_features->rx_offloads) >
					rte_popcount32(current_features->rx_offloads))
				continue;
			/* Do not select paths without bulk alloc support if requested and the
			 * current path already meets this requirement.
			 */
			if (!path_features->extra.bulk_alloc && req_features.extra.bulk_alloc &&
					current_features->extra.bulk_alloc)
				continue;
		}

		/* Finally, select the path since it has met all the requirements. */
		idx = i;
		current_features = &infos[idx].features;
	}

	return idx;
}

#endif /* _COMMON_INTEL_RX_H_ */
