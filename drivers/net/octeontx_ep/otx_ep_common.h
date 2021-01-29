/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX_EP_COMMON_H_
#define _OTX_EP_COMMON_H_

#define OTX_EP_MAX_RINGS_PER_VF        (8)
#define OTX_EP_CFG_IO_QUEUES        OTX_EP_MAX_RINGS_PER_VF
#define OTX_EP_64BYTE_INSTR         (64)
#define OTX_EP_MIN_IQ_DESCRIPTORS   (128)
#define OTX_EP_MIN_OQ_DESCRIPTORS   (128)
#define OTX_EP_MAX_IQ_DESCRIPTORS   (8192)
#define OTX_EP_MAX_OQ_DESCRIPTORS   (8192)
#define OTX_EP_OQ_BUF_SIZE          (2048)
#define OTX_EP_MIN_RX_BUF_SIZE      (64)

#define OTX_EP_OQ_INFOPTR_MODE      (0)
#define OTX_EP_OQ_REFIL_THRESHOLD   (16)

#define otx_ep_info(fmt, args...)				\
	rte_log(RTE_LOG_INFO, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

#define otx_ep_err(fmt, args...)				\
	rte_log(RTE_LOG_ERR, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

#define otx_ep_dbg(fmt, args...)				\
	rte_log(RTE_LOG_DEBUG, otx_net_ep_logtype,		\
		"%s():%u " fmt "\n",				\
		__func__, __LINE__, ##args)

#define otx_ep_write64(value, base_addr, reg_off) \
	{\
	typeof(value) val = (value); \
	typeof(reg_off) off = (reg_off); \
	otx_ep_dbg("octeon_write_csr64: reg: 0x%08lx val: 0x%016llx\n", \
		   (unsigned long)off, (unsigned long long)val); \
	rte_write64(val, ((base_addr) + off)); \
	}

struct otx_ep_device;

/* Structure to define the configuration attributes for each Input queue. */
struct otx_ep_iq_config {
	/* Max number of IQs available */
	uint16_t max_iqs;

	/* Command size - 32 or 64 bytes */
	uint16_t instr_type;

	/* Pending list size, usually set to the sum of the size of all IQs */
	uint32_t pending_list_size;
};

/* Structure to define the configuration attributes for each Output queue. */
struct otx_ep_oq_config {
	/* Max number of OQs available */
	uint16_t max_oqs;

	/* If set, the Output queue uses info-pointer mode. (Default: 1 ) */
	uint16_t info_ptr;

	/** The number of buffers that were consumed during packet processing by
	 *  the driver on this Output queue before the driver attempts to
	 *  replenish the descriptor ring with new buffers.
	 */
	uint32_t refill_threshold;
};

/* Structure to define the configuration. */
struct otx_ep_config {
	/* Input Queue attributes. */
	struct otx_ep_iq_config iq;

	/* Output Queue attributes. */
	struct otx_ep_oq_config oq;

	/* Num of desc for IQ rings */
	uint32_t num_iqdef_descs;

	/* Num of desc for OQ rings */
	uint32_t num_oqdef_descs;

	/* OQ buffer size */
	uint32_t oqdef_buf_size;
};

/* SRIOV information */
struct otx_ep_sriov_info {
	/* Number of rings assigned to VF */
	uint32_t rings_per_vf;

	/* Number of VF devices enabled */
	uint32_t num_vfs;
};

/* Required functions for each VF device */
struct otx_ep_fn_list {
	void (*setup_device_regs)(struct otx_ep_device *otx_ep);
};

/* OTX_EP EP VF device data structure */
struct otx_ep_device {
	/* PCI device pointer */
	struct rte_pci_device *pdev;

	uint16_t chip_id;

	struct rte_eth_dev *eth_dev;

	int port_id;

	/* Memory mapped h/w address */
	uint8_t *hw_addr;

	struct otx_ep_fn_list fn_list;

	uint32_t max_tx_queues;

	uint32_t max_rx_queues;

	/* SR-IOV info */
	struct otx_ep_sriov_info sriov_info;

	/* Device configuration */
	const struct otx_ep_config *conf;

	uint64_t rx_offloads;

	uint64_t tx_offloads;
};

#define OTX_EP_MAX_PKT_SZ 64000U
#define OTX_EP_MAX_MAC_ADDRS 1

extern int otx_net_ep_logtype;
#endif  /* _OTX_EP_COMMON_H_ */
