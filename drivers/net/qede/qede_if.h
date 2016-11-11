/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _QEDE_IF_H
#define _QEDE_IF_H

#include "qede_ethdev.h"

/* forward */
struct ecore_dev;
struct qed_sb_info;
struct qed_pf_params;
enum ecore_int_mode;

struct qed_dev_info {
	uint8_t num_hwfns;
	uint8_t hw_mac[ETHER_ADDR_LEN];
	bool is_mf_default;

	/* FW version */
	uint16_t fw_major;
	uint16_t fw_minor;
	uint16_t fw_rev;
	uint16_t fw_eng;

	/* MFW version */
	uint32_t mfw_rev;

	uint32_t flash_size;
	uint8_t mf_mode;
	bool tx_switching;
	/* To be added... */
};

enum qed_sb_type {
	QED_SB_TYPE_L2_QUEUE,
	QED_SB_TYPE_STORAGE,
	QED_SB_TYPE_CNQ,
};

enum qed_protocol {
	QED_PROTOCOL_ETH,
};

struct qed_link_params {
	bool link_up;

#define QED_LINK_OVERRIDE_SPEED_AUTONEG         (1 << 0)
#define QED_LINK_OVERRIDE_SPEED_ADV_SPEEDS      (1 << 1)
#define QED_LINK_OVERRIDE_SPEED_FORCED_SPEED    (1 << 2)
#define QED_LINK_OVERRIDE_PAUSE_CONFIG          (1 << 3)
	uint32_t override_flags;
	bool autoneg;
	uint32_t adv_speeds;
	uint32_t forced_speed;
#define QED_LINK_PAUSE_AUTONEG_ENABLE           (1 << 0)
#define QED_LINK_PAUSE_RX_ENABLE                (1 << 1)
#define QED_LINK_PAUSE_TX_ENABLE                (1 << 2)
	uint32_t pause_config;
};

struct qed_link_output {
	bool link_up;
	uint32_t supported_caps;	/* In SUPPORTED defs */
	uint32_t advertised_caps;	/* In ADVERTISED defs */
	uint32_t lp_caps;	/* In ADVERTISED defs */
	uint32_t speed;		/* In Mb/s */
	uint32_t adv_speed;	/* Speed mask */
	uint8_t duplex;		/* In DUPLEX defs */
	uint8_t port;		/* In PORT defs */
	bool autoneg;
	uint32_t pause_config;
};

struct qed_slowpath_params {
	uint32_t int_mode;
	uint8_t drv_major;
	uint8_t drv_minor;
	uint8_t drv_rev;
	uint8_t drv_eng;
	uint8_t name[NAME_SIZE];
};

#define ILT_PAGE_SIZE_TCFC 0x8000	/* 32KB */

struct qed_common_cb_ops {
	void (*link_update)(void *dev, struct qed_link_output *link);
};

struct qed_selftest_ops {
/**
 * @brief registers - Perform register tests
 *
 * @param edev
 *
 * @return 0 on success, error otherwise.
 */
	int (*registers)(struct ecore_dev *edev);
};

struct qed_common_ops {
	int (*probe)(struct ecore_dev *edev,
		     struct rte_pci_device *pci_dev,
		     enum qed_protocol protocol,
		     uint32_t dp_module, uint8_t dp_level, bool is_vf);
	void (*set_id)(struct ecore_dev *edev,
		char name[], const char ver_str[]);
	enum _ecore_status_t (*chain_alloc)(struct ecore_dev *edev,
					    enum ecore_chain_use_mode
					    intended_use,
					    enum ecore_chain_mode mode,
					    enum ecore_chain_cnt_type cnt_type,
					    uint32_t num_elems,
					    osal_size_t elem_size,
					    struct ecore_chain *p_chain);

	void (*chain_free)(struct ecore_dev *edev,
			   struct ecore_chain *p_chain);

	void (*get_link)(struct ecore_dev *edev,
			 struct qed_link_output *if_link);
	int (*set_link)(struct ecore_dev *edev,
			struct qed_link_params *params);

	int (*drain)(struct ecore_dev *edev);

	void (*remove)(struct ecore_dev *edev);

	int (*slowpath_stop)(struct ecore_dev *edev);

	void (*update_pf_params)(struct ecore_dev *edev,
				 struct ecore_pf_params *params);

	int (*slowpath_start)(struct ecore_dev *edev,
			      struct qed_slowpath_params *params);

	int (*set_fp_int)(struct ecore_dev *edev, uint16_t cnt);

	uint32_t (*sb_init)(struct ecore_dev *edev,
			    struct ecore_sb_info *sb_info,
			    void *sb_virt_addr,
			    dma_addr_t sb_phy_addr,
			    uint16_t sb_id, enum qed_sb_type type);

	bool (*can_link_change)(struct ecore_dev *edev);
	void (*update_msglvl)(struct ecore_dev *edev,
			      uint32_t dp_module, uint8_t dp_level);
};

#endif /* _QEDE_IF_H */
