/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2017 NXP
 *
 */

#ifndef __FSL_FMAN_H
#define __FSL_FMAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Status field in FD is updated on Rx side by FMAN with following information.
 * Refer to field description in FM BG.
 */
struct fm_status_t {
	unsigned int reserved0:3;
	unsigned int dcl4c:1; /* Don't Check L4 Checksum */
	unsigned int reserved1:1;
	unsigned int ufd:1; /* Unsupported Format */
	unsigned int lge:1; /* Length Error */
	unsigned int dme:1; /* DMA Error */

	unsigned int reserved2:4;
	unsigned int fpe:1; /* Frame physical Error */
	unsigned int fse:1; /* Frame Size Error */
	unsigned int dis:1; /* Discard by Classification */
	unsigned int reserved3:1;

	unsigned int eof:1; /* Key Extraction goes out of frame */
	unsigned int nss:1; /* No Scheme selected */
	unsigned int kso:1; /* Key Size Overflow */
	unsigned int reserved4:1;
	unsigned int fcl:2; /* Frame Color */
	unsigned int ipp:1; /* Illegal Policer Profile Selected */
	unsigned int flm:1; /* Frame Length Mismatch */
	unsigned int pte:1; /* Parser Timeout */
	unsigned int isp:1; /* Invalid Soft Parser Instruction */
	unsigned int phe:1; /* Header Error during parsing */
	unsigned int frdr:1; /* Frame Dropped by disabled port */
	unsigned int reserved5:4;
} __attribute__ ((__packed__));

/* Set MAC address for a particular interface */
int fman_if_add_mac_addr(struct fman_if *p, uint8_t *eth, uint8_t addr_num);

/* Remove a MAC address for a particular interface */
void fman_if_clear_mac_addr(struct fman_if *p, uint8_t addr_num);

/* Get the FMAN statistics */
void fman_if_stats_get(struct fman_if *p, struct rte_eth_stats *stats);

/* Reset the FMAN statistics */
void fman_if_stats_reset(struct fman_if *p);

/* Get all of the FMAN statistics */
void fman_if_stats_get_all(struct fman_if *p, uint64_t *value, int n);

/* Set ignore pause option for a specific interface */
void fman_if_set_rx_ignore_pause_frames(struct fman_if *p, bool enable);

/* Set max frame length */
void fman_if_conf_max_frame_len(struct fman_if *p, unsigned int max_frame_len);

/* Enable/disable Rx promiscuous mode on specified interface */
void fman_if_promiscuous_enable(struct fman_if *p);
void fman_if_promiscuous_disable(struct fman_if *p);

/* Enable/disable Rx on specific interfaces */
void fman_if_enable_rx(struct fman_if *p);
void fman_if_disable_rx(struct fman_if *p);

/* Enable/disable loopback on specific interfaces */
void fman_if_loopback_enable(struct fman_if *p);
void fman_if_loopback_disable(struct fman_if *p);

/* Set buffer pool on specific interface */
void fman_if_set_bp(struct fman_if *fm_if, unsigned int num, int bpid,
		    size_t bufsize);

/* Get Flow Control threshold parameters on specific interface */
int fman_if_get_fc_threshold(struct fman_if *fm_if);

/* Enable and Set Flow Control threshold parameters on specific interface */
int fman_if_set_fc_threshold(struct fman_if *fm_if,
			u32 high_water, u32 low_water, u32 bpid);

/* Get Flow Control pause quanta on specific interface */
int fman_if_get_fc_quanta(struct fman_if *fm_if);

/* Set Flow Control pause quanta on specific interface */
int fman_if_set_fc_quanta(struct fman_if *fm_if, u16 pause_quanta);

/* Set default error fqid on specific interface */
void fman_if_set_err_fqid(struct fman_if *fm_if, uint32_t err_fqid);

/* Get IC transfer params */
int fman_if_get_ic_params(struct fman_if *fm_if, struct fman_if_ic_params *icp);

/* Set IC transfer params */
int fman_if_set_ic_params(struct fman_if *fm_if,
			  const struct fman_if_ic_params *icp);

/* Get interface fd->offset value */
int fman_if_get_fdoff(struct fman_if *fm_if);

/* Set interface fd->offset value */
void fman_if_set_fdoff(struct fman_if *fm_if, uint32_t fd_offset);

/* Get interface Max Frame length (MTU) */
uint16_t fman_if_get_maxfrm(struct fman_if *fm_if);

/* Set interface  Max Frame length (MTU) */
void fman_if_set_maxfrm(struct fman_if *fm_if, uint16_t max_frm);

/* Set interface next invoked action for dequeue operation */
void fman_if_set_dnia(struct fman_if *fm_if, uint32_t nia);

/* discard error packets on rx */
void fman_if_discard_rx_errors(struct fman_if *fm_if);

void fman_if_set_mcast_filter_table(struct fman_if *p);

void fman_if_reset_mcast_filter_table(struct fman_if *p);

int fman_if_add_hash_mac_addr(struct fman_if *p, uint8_t *eth);

int fman_if_get_primary_mac_addr(struct fman_if *p, uint8_t *eth);


/* Enable/disable Rx on all interfaces */
static inline void fman_if_enable_all_rx(void)
{
	struct fman_if *__if;

	list_for_each_entry(__if, fman_if_list, node)
		fman_if_enable_rx(__if);
}

static inline void fman_if_disable_all_rx(void)
{
	struct fman_if *__if;

	list_for_each_entry(__if, fman_if_list, node)
		fman_if_disable_rx(__if);
}
#endif /* __FSL_FMAN_H */
