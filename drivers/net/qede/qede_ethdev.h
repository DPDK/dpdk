/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */


#ifndef _QEDE_ETHDEV_H_
#define _QEDE_ETHDEV_H_

#include <sys/queue.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_dev.h>
#include <rte_ip.h>

/* ecore includes */
#include "base/bcm_osal.h"
#include "base/ecore.h"
#include "base/ecore_dev_api.h"
#include "base/ecore_l2_api.h"
#include "base/ecore_vf_api.h"
#include "base/ecore_hsi_common.h"
#include "base/ecore_int_api.h"
#include "base/ecore_chain.h"
#include "base/ecore_status.h"
#include "base/ecore_hsi_eth.h"
#include "base/ecore_dev_api.h"
#include "base/ecore_iov_api.h"
#include "base/ecore_cxt.h"
#include "base/nvm_cfg.h"
#include "base/ecore_iov_api.h"
#include "base/ecore_sp_commands.h"
#include "base/ecore_l2.h"
#include "base/ecore_dev_api.h"
#include "base/ecore_l2.h"

#include "qede_logs.h"
#include "qede_if.h"
#include "qede_rxtx.h"

#define qede_stringify1(x...)		#x
#define qede_stringify(x...)		qede_stringify1(x)

/* Driver versions */
#define QEDE_PMD_VER_PREFIX		"QEDE PMD"
#define QEDE_PMD_VERSION_MAJOR		2
#define QEDE_PMD_VERSION_MINOR	        5
#define QEDE_PMD_VERSION_REVISION       0
#define QEDE_PMD_VERSION_PATCH	        1

#define QEDE_PMD_VERSION qede_stringify(QEDE_PMD_VERSION_MAJOR) "."     \
			 qede_stringify(QEDE_PMD_VERSION_MINOR) "."     \
			 qede_stringify(QEDE_PMD_VERSION_REVISION) "."  \
			 qede_stringify(QEDE_PMD_VERSION_PATCH)

#define QEDE_PMD_DRV_VER_STR_SIZE NAME_SIZE
#define QEDE_PMD_VER_PREFIX "QEDE PMD"


#define QEDE_RSS_INDIR_INITED     (1 << 0)
#define QEDE_RSS_KEY_INITED       (1 << 1)
#define QEDE_RSS_CAPS_INITED      (1 << 2)

#define QEDE_MAX_RSS_CNT(edev)  ((edev)->dev_info.num_queues)
#define QEDE_MAX_TSS_CNT(edev)  ((edev)->dev_info.num_queues * \
					(edev)->dev_info.num_tc)

#define QEDE_QUEUE_CNT(qdev) ((qdev)->num_queues)
#define QEDE_RSS_COUNT(qdev) ((qdev)->num_rx_queues)
#define QEDE_TSS_COUNT(qdev) ((qdev)->num_tx_queues)

#define QEDE_DUPLEX_FULL	1
#define QEDE_DUPLEX_HALF	2
#define QEDE_DUPLEX_UNKNOWN     0xff

#define QEDE_SUPPORTED_AUTONEG (1 << 6)
#define QEDE_SUPPORTED_PAUSE   (1 << 13)

#define QEDE_INIT_QDEV(eth_dev) (eth_dev->data->dev_private)

#define QEDE_INIT_EDEV(adapter) (&((struct qede_dev *)adapter)->edev)

#define QEDE_INIT(eth_dev) {					\
	struct qede_dev *qdev = eth_dev->data->dev_private;	\
	struct ecore_dev *edev = &qdev->edev;			\
}

/************* QLogic 10G/25G/40G/50G/100G vendor/devices ids *************/
#define PCI_VENDOR_ID_QLOGIC                   0x1077

#define CHIP_NUM_57980E                        0x1634
#define CHIP_NUM_57980S                        0x1629
#define CHIP_NUM_VF                            0x1630
#define CHIP_NUM_57980S_40                     0x1634
#define CHIP_NUM_57980S_25                     0x1656
#define CHIP_NUM_57980S_IOV                    0x1664
#define CHIP_NUM_57980S_100                    0x1644
#define CHIP_NUM_57980S_50                     0x1654
#define CHIP_NUM_AH_50G	                       0x8070
#define CHIP_NUM_AH_10G                        0x8071
#define CHIP_NUM_AH_40G			       0x8072
#define CHIP_NUM_AH_25G			       0x8073
#define CHIP_NUM_AH_IOV			       0x8090

#define PCI_DEVICE_ID_QLOGIC_NX2_57980E        CHIP_NUM_57980E
#define PCI_DEVICE_ID_QLOGIC_NX2_57980S        CHIP_NUM_57980S
#define PCI_DEVICE_ID_QLOGIC_NX2_VF            CHIP_NUM_VF
#define PCI_DEVICE_ID_QLOGIC_57980S_40         CHIP_NUM_57980S_40
#define PCI_DEVICE_ID_QLOGIC_57980S_25         CHIP_NUM_57980S_25
#define PCI_DEVICE_ID_QLOGIC_57980S_IOV        CHIP_NUM_57980S_IOV
#define PCI_DEVICE_ID_QLOGIC_57980S_100        CHIP_NUM_57980S_100
#define PCI_DEVICE_ID_QLOGIC_57980S_50         CHIP_NUM_57980S_50
#define PCI_DEVICE_ID_QLOGIC_AH_50G            CHIP_NUM_AH_50G
#define PCI_DEVICE_ID_QLOGIC_AH_10G            CHIP_NUM_AH_10G
#define PCI_DEVICE_ID_QLOGIC_AH_40G            CHIP_NUM_AH_40G
#define PCI_DEVICE_ID_QLOGIC_AH_25G            CHIP_NUM_AH_25G
#define PCI_DEVICE_ID_QLOGIC_AH_IOV            CHIP_NUM_AH_IOV


#define QEDE_VXLAN_DEF_PORT		8472

extern char fw_file[];

/* Number of PF connections - 32 RX + 32 TX */
#define QEDE_PF_NUM_CONNS		(64)

/* Maximum number of flowdir filters */
#define QEDE_RFS_MAX_FLTR		(256)

#define QEDE_MAX_MCAST_FILTERS		(64)

enum qed_filter_rx_mode_type {
	QED_FILTER_RX_MODE_TYPE_REGULAR,
	QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC,
	QED_FILTER_RX_MODE_TYPE_PROMISC,
};

struct qede_vlan_entry {
	SLIST_ENTRY(qede_vlan_entry) list;
	uint16_t vid;
};

struct qede_mcast_entry {
	struct ether_addr mac;
	SLIST_ENTRY(qede_mcast_entry) list;
};

struct qede_ucast_entry {
	struct ether_addr mac;
	uint16_t vlan;
	uint16_t vni;
	SLIST_ENTRY(qede_ucast_entry) list;
};

struct qede_fdir_entry {
	uint32_t soft_id; /* unused for now */
	uint16_t pkt_len; /* actual packet length to match */
	uint16_t rx_queue; /* queue to be steered to */
	const struct rte_memzone *mz; /* mz used to hold L2 frame */
	SLIST_ENTRY(qede_fdir_entry) list;
};

struct qede_fdir_info {
	struct ecore_arfs_config_params arfs;
	uint16_t filter_count;
	SLIST_HEAD(fdir_list_head, qede_fdir_entry)fdir_list_head;
};


/*
 *  Structure to store private data for each port.
 */
struct qede_dev {
	struct ecore_dev edev;
	const struct qed_eth_ops *ops;
	struct qed_dev_eth_info dev_info;
	struct ecore_sb_info *sb_array;
	struct qede_fastpath *fp_array;
	uint16_t mtu;
	uint16_t new_mtu;
	bool rss_enable;
	struct rte_eth_rss_conf rss_conf;
	uint16_t rss_ind_table[ECORE_RSS_IND_TABLE_SIZE];
	uint64_t rss_hf;
	uint8_t rss_key_len;
	bool enable_lro;
	uint8_t num_rx_queues;
	uint8_t num_tx_queues;
	SLIST_HEAD(vlan_list_head, qede_vlan_entry)vlan_list_head;
	uint16_t configured_vlans;
	bool accept_any_vlan;
	struct ether_addr primary_mac;
	SLIST_HEAD(mc_list_head, qede_mcast_entry) mc_list_head;
	uint16_t num_mc_addr;
	SLIST_HEAD(uc_list_head, qede_ucast_entry) uc_list_head;
	uint16_t num_uc_addr;
	bool handle_hw_err;
	uint16_t num_tunn_filters;
	uint16_t vxlan_filter_type;
	struct qede_fdir_info fdir_info;
	bool vlan_strip_flg;
	char drv_ver[QEDE_PMD_DRV_VER_STR_SIZE];
};

/* Non-static functions */
int qede_config_rss(struct rte_eth_dev *eth_dev);

int qede_rss_hash_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_conf *rss_conf);

int qede_rss_reta_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);

int qed_fill_eth_dev_info(struct ecore_dev *edev,
				 struct qed_dev_eth_info *info);
int qede_dev_set_link_state(struct rte_eth_dev *eth_dev, bool link_up);

int qede_dev_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type type,
			 enum rte_filter_op op, void *arg);

int qede_fdir_filter_conf(struct rte_eth_dev *eth_dev,
			  enum rte_filter_op filter_op, void *arg);

int qede_ntuple_filter_conf(struct rte_eth_dev *eth_dev,
			    enum rte_filter_op filter_op, void *arg);

int qede_check_fdir_support(struct rte_eth_dev *eth_dev);

uint16_t qede_fdir_construct_pkt(struct rte_eth_dev *eth_dev,
				 struct rte_eth_fdir_filter *fdir,
				 void *buff,
				 struct ecore_arfs_config_params *params);

void qede_fdir_dealloc_resc(struct rte_eth_dev *eth_dev);

int qede_activate_vport(struct rte_eth_dev *eth_dev, bool flg);

int qede_update_mtu(struct rte_eth_dev *eth_dev, uint16_t mtu);

int qede_enable_tpa(struct rte_eth_dev *eth_dev, bool flg);

#endif /* _QEDE_ETHDEV_H_ */
