/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#include "ena_ethdev.h"
#include "ena_logs.h"

#include <ena_admin_defs.h>

#define TEST_BIT(val, bit_shift) ((val) & (1UL << (bit_shift)))

#define ENA_HF_RSS_ALL_L2	(ENA_ADMIN_RSS_L3_SA | ENA_ADMIN_RSS_L3_DA)
#define ENA_HF_RSS_ALL_L3	(ENA_ADMIN_RSS_L3_SA | ENA_ADMIN_RSS_L3_DA)
#define ENA_HF_RSS_ALL_L4	(ENA_ADMIN_RSS_L4_SP | ENA_ADMIN_RSS_L4_DP)
#define ENA_HF_RSS_ALL_L3_L4	(ENA_HF_RSS_ALL_L3 | ENA_HF_RSS_ALL_L4)
#define ENA_HF_RSS_ALL_L2_L3_L4	(ENA_HF_RSS_ALL_L2 | ENA_HF_RSS_ALL_L3_L4)

enum ena_rss_hash_fields {
	ENA_HF_RSS_TCP4		= ENA_HF_RSS_ALL_L3_L4,
	ENA_HF_RSS_UDP4		= ENA_HF_RSS_ALL_L3_L4,
	ENA_HF_RSS_TCP6		= ENA_HF_RSS_ALL_L3_L4,
	ENA_HF_RSS_UDP6		= ENA_HF_RSS_ALL_L3_L4,
	ENA_HF_RSS_IP4		= ENA_HF_RSS_ALL_L3,
	ENA_HF_RSS_IP6		= ENA_HF_RSS_ALL_L3,
	ENA_HF_RSS_IP4_FRAG	= ENA_HF_RSS_ALL_L3,
	ENA_HF_RSS_NOT_IP	= ENA_HF_RSS_ALL_L2,
	ENA_HF_RSS_TCP6_EX	= ENA_HF_RSS_ALL_L3_L4,
	ENA_HF_RSS_IP6_EX	= ENA_HF_RSS_ALL_L3,
};

static int ena_fill_indirect_table_default(struct ena_com_dev *ena_dev,
					   size_t tbl_size,
					   size_t queue_num);
static uint64_t ena_admin_hf_to_eth_hf(enum ena_admin_flow_hash_proto proto,
				       uint16_t field);
static uint16_t ena_eth_hf_to_admin_hf(enum ena_admin_flow_hash_proto proto,
				       uint64_t rss_hf);
static int ena_set_hash_fields(struct ena_com_dev *ena_dev, uint64_t rss_hf);
static int ena_rss_hash_set(struct ena_com_dev *ena_dev,
			    struct rte_eth_rss_conf *rss_conf,
			    bool default_allowed);
static void ena_reorder_rss_hash_key(uint8_t *reordered_key,
				     uint8_t *key,
				     size_t key_size);
static int ena_get_rss_hash_key(struct ena_com_dev *ena_dev, uint8_t *rss_key);

void ena_rss_key_fill(void *key, size_t size)
{
	static bool key_generated;
	static uint8_t default_key[ENA_HASH_KEY_SIZE];
	size_t i;

	RTE_ASSERT(size <= ENA_HASH_KEY_SIZE);

	if (!key_generated) {
		for (i = 0; i < ENA_HASH_KEY_SIZE; ++i)
			default_key[i] = rte_rand() & 0xff;
		key_generated = true;
	}

	rte_memcpy(key, default_key, size);
}

int ena_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc, i;
	u16 entry_value;
	int conf_idx;
	int idx;

	if (reta_size == 0 || reta_conf == NULL)
		return -EINVAL;

	if (!(dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		PMD_DRV_LOG(ERR,
			"RSS was not configured for the PMD\n");
		return -ENOTSUP;
	}

	if (reta_size > ENA_RX_RSS_TABLE_SIZE) {
		PMD_DRV_LOG(WARNING,
			"Requested indirection table size (%d) is bigger than supported: %d\n",
			reta_size, ENA_RX_RSS_TABLE_SIZE);
		return -EINVAL;
	}

	/* Prevent RETA table structure update races */
	rte_spinlock_lock(&adapter->admin_lock);
	for (i = 0 ; i < reta_size ; i++) {
		/* Each reta_conf is for 64 entries.
		 * To support 128 we use 2 conf of 64.
		 */
		conf_idx = i / RTE_ETH_RETA_GROUP_SIZE;
		idx = i % RTE_ETH_RETA_GROUP_SIZE;
		if (TEST_BIT(reta_conf[conf_idx].mask, idx)) {
			entry_value =
				ENA_IO_RXQ_IDX(reta_conf[conf_idx].reta[idx]);

			rc = ena_com_indirect_table_fill_entry(ena_dev, i,
				entry_value);
			if (unlikely(rc != 0)) {
				PMD_DRV_LOG(ERR,
					"Cannot fill indirection table\n");
				return rc;
			}
		}
	}

	rc = ena_mp_indirect_table_set(adapter);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR, "Cannot set the indirection table\n");
		return rc;
	}

	PMD_DRV_LOG(DEBUG, "RSS configured %d entries for port %d\n",
		reta_size, dev->data->port_id);

	return 0;
}

/* Query redirection table. */
int ena_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	uint32_t indirect_table[ENA_RX_RSS_TABLE_SIZE];
	struct ena_adapter *adapter = dev->data->dev_private;
	int rc;
	int i;
	int reta_conf_idx;
	int reta_idx;

	if (reta_size == 0 || reta_conf == NULL)
		return -EINVAL;

	if (!(dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		PMD_DRV_LOG(ERR,
			"RSS was not configured for the PMD\n");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_mp_indirect_table_get(adapter, indirect_table);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR, "Cannot get indirection table\n");
		return rc;
	}

	for (i = 0 ; i < reta_size ; i++) {
		reta_conf_idx = i / RTE_ETH_RETA_GROUP_SIZE;
		reta_idx = i % RTE_ETH_RETA_GROUP_SIZE;
		if (TEST_BIT(reta_conf[reta_conf_idx].mask, reta_idx))
			reta_conf[reta_conf_idx].reta[reta_idx] =
				ENA_IO_RXQ_IDX_REV(indirect_table[i]);
	}

	return 0;
}

static int ena_fill_indirect_table_default(struct ena_com_dev *ena_dev,
					   size_t tbl_size,
					   size_t queue_num)
{
	size_t i;
	int rc;
	uint16_t val;

	for (i = 0; i < tbl_size; ++i) {
		val = i % queue_num;
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
			ENA_IO_RXQ_IDX(val));
		if (unlikely(rc != 0)) {
			PMD_DRV_LOG(DEBUG,
				"Failed to set %zu indirection table entry with val %" PRIu16 "\n",
				i, val);
			return rc;
		}
	}

	return 0;
}

static uint64_t ena_admin_hf_to_eth_hf(enum ena_admin_flow_hash_proto proto,
				       uint16_t fields)
{
	uint64_t rss_hf = 0;

	/* If no fields are activated, then RSS is disabled for this proto */
	if ((fields & ENA_HF_RSS_ALL_L2_L3_L4) == 0)
		return 0;

	/* Convert proto to ETH flag */
	switch (proto) {
	case ENA_ADMIN_RSS_TCP4:
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
		break;
	case ENA_ADMIN_RSS_UDP4:
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
		break;
	case ENA_ADMIN_RSS_TCP6:
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;
		break;
	case ENA_ADMIN_RSS_UDP6:
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;
		break;
	case ENA_ADMIN_RSS_IP4:
		rss_hf |= RTE_ETH_RSS_IPV4;
		break;
	case ENA_ADMIN_RSS_IP6:
		rss_hf |= RTE_ETH_RSS_IPV6;
		break;
	case ENA_ADMIN_RSS_IP4_FRAG:
		rss_hf |= RTE_ETH_RSS_FRAG_IPV4;
		break;
	case ENA_ADMIN_RSS_NOT_IP:
		rss_hf |= RTE_ETH_RSS_L2_PAYLOAD;
		break;
	case ENA_ADMIN_RSS_TCP6_EX:
		rss_hf |= RTE_ETH_RSS_IPV6_TCP_EX;
		break;
	case ENA_ADMIN_RSS_IP6_EX:
		rss_hf |= RTE_ETH_RSS_IPV6_EX;
		break;
	default:
		break;
	};

	/* Check if only DA or SA is being used for L3. */
	switch (fields & ENA_HF_RSS_ALL_L3) {
	case ENA_ADMIN_RSS_L3_SA:
		rss_hf |= RTE_ETH_RSS_L3_SRC_ONLY;
		break;
	case ENA_ADMIN_RSS_L3_DA:
		rss_hf |= RTE_ETH_RSS_L3_DST_ONLY;
		break;
	default:
		break;
	};

	/* Check if only DA or SA is being used for L4. */
	switch (fields & ENA_HF_RSS_ALL_L4) {
	case ENA_ADMIN_RSS_L4_SP:
		rss_hf |= RTE_ETH_RSS_L4_SRC_ONLY;
		break;
	case ENA_ADMIN_RSS_L4_DP:
		rss_hf |= RTE_ETH_RSS_L4_DST_ONLY;
		break;
	default:
		break;
	};

	return rss_hf;
}

static uint16_t ena_eth_hf_to_admin_hf(enum ena_admin_flow_hash_proto proto,
				       uint64_t rss_hf)
{
	uint16_t fields_mask = 0;

	/* L2 always uses source and destination addresses. */
	fields_mask = ENA_ADMIN_RSS_L2_DA | ENA_ADMIN_RSS_L2_SA;

	/* Determine which fields of L3 should be used. */
	switch (rss_hf & (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY)) {
	case RTE_ETH_RSS_L3_DST_ONLY:
		fields_mask |= ENA_ADMIN_RSS_L3_DA;
		break;
	case RTE_ETH_RSS_L3_SRC_ONLY:
		fields_mask |= ENA_ADMIN_RSS_L3_SA;
		break;
	default:
		/*
		 * If SRC nor DST aren't set, it means both of them should be
		 * used.
		 */
		fields_mask |= ENA_HF_RSS_ALL_L3;
	}

	/* Determine which fields of L4 should be used. */
	switch (rss_hf & (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY)) {
	case RTE_ETH_RSS_L4_DST_ONLY:
		fields_mask |= ENA_ADMIN_RSS_L4_DP;
		break;
	case RTE_ETH_RSS_L4_SRC_ONLY:
		fields_mask |= ENA_ADMIN_RSS_L4_SP;
		break;
	default:
		/*
		 * If SRC nor DST aren't set, it means both of them should be
		 * used.
		 */
		fields_mask |= ENA_HF_RSS_ALL_L4;
	}

	/* Return appropriate hash fields. */
	switch (proto) {
	case ENA_ADMIN_RSS_TCP4:
		return ENA_HF_RSS_TCP4 & fields_mask;
	case ENA_ADMIN_RSS_UDP4:
		return ENA_HF_RSS_UDP4 & fields_mask;
	case ENA_ADMIN_RSS_TCP6:
		return ENA_HF_RSS_TCP6 & fields_mask;
	case ENA_ADMIN_RSS_UDP6:
		return ENA_HF_RSS_UDP6 & fields_mask;
	case ENA_ADMIN_RSS_IP4:
		return ENA_HF_RSS_IP4 & fields_mask;
	case ENA_ADMIN_RSS_IP6:
		return ENA_HF_RSS_IP6 & fields_mask;
	case ENA_ADMIN_RSS_IP4_FRAG:
		return ENA_HF_RSS_IP4_FRAG & fields_mask;
	case ENA_ADMIN_RSS_NOT_IP:
		return ENA_HF_RSS_NOT_IP & fields_mask;
	case ENA_ADMIN_RSS_TCP6_EX:
		return ENA_HF_RSS_TCP6_EX & fields_mask;
	case ENA_ADMIN_RSS_IP6_EX:
		return ENA_HF_RSS_IP6_EX & fields_mask;
	default:
		break;
	}

	return 0;
}

static int ena_set_hash_fields(struct ena_com_dev *ena_dev, uint64_t rss_hf)
{
	struct ena_admin_proto_input selected_fields[ENA_ADMIN_RSS_PROTO_NUM] = {};
	int rc, i;

	/* Turn on appropriate fields for each requested packet type */
	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) != 0)
		selected_fields[ENA_ADMIN_RSS_TCP4].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_TCP4, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) != 0)
		selected_fields[ENA_ADMIN_RSS_UDP4].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_UDP4, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) != 0)
		selected_fields[ENA_ADMIN_RSS_TCP6].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_TCP6, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) != 0)
		selected_fields[ENA_ADMIN_RSS_UDP6].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_UDP6, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_IPV4) != 0)
		selected_fields[ENA_ADMIN_RSS_IP4].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_IP4, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_IPV6) != 0)
		selected_fields[ENA_ADMIN_RSS_IP6].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_IP6, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_FRAG_IPV4) != 0)
		selected_fields[ENA_ADMIN_RSS_IP4_FRAG].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_IP4_FRAG, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_L2_PAYLOAD) != 0)
		selected_fields[ENA_ADMIN_RSS_NOT_IP].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_NOT_IP, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_IPV6_TCP_EX) != 0)
		selected_fields[ENA_ADMIN_RSS_TCP6_EX].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_TCP6_EX, rss_hf);

	if ((rss_hf & RTE_ETH_RSS_IPV6_EX) != 0)
		selected_fields[ENA_ADMIN_RSS_IP6_EX].fields =
			ena_eth_hf_to_admin_hf(ENA_ADMIN_RSS_IP6_EX, rss_hf);

	/* Try to write them to the device */
	for (i = 0; i < ENA_ADMIN_RSS_PROTO_NUM; i++) {
		rc = ena_com_fill_hash_ctrl(ena_dev,
			(enum ena_admin_flow_hash_proto)i,
			selected_fields[i].fields);
		if (unlikely(rc != 0)) {
			PMD_DRV_LOG(DEBUG,
				"Failed to set ENA HF %d with fields %" PRIu16 "\n",
				i, selected_fields[i].fields);
			return rc;
		}
	}

	return 0;
}

static int ena_rss_hash_set(struct ena_com_dev *ena_dev,
			    struct rte_eth_rss_conf *rss_conf,
			    bool default_allowed)
{
	uint8_t hw_rss_key[ENA_HASH_KEY_SIZE];
	uint8_t *rss_key;
	int rc;

	if (rss_conf->rss_key != NULL) {
		/* Reorder the RSS key bytes for the hardware requirements. */
		ena_reorder_rss_hash_key(hw_rss_key, rss_conf->rss_key,
			ENA_HASH_KEY_SIZE);
		rss_key = hw_rss_key;
	} else {
		rss_key = NULL;
	}

	/* If the rss_key is NULL, then the randomized key will be used. */
	rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_TOEPLITZ,
		rss_key, ENA_HASH_KEY_SIZE, 0);
	if (rc != 0 && !(default_allowed && rc == ENA_COM_UNSUPPORTED)) {
		PMD_DRV_LOG(ERR,
			"Failed to set RSS hash function in the device\n");
		return rc;
	}

	rc = ena_set_hash_fields(ena_dev, rss_conf->rss_hf);
	if (rc == ENA_COM_UNSUPPORTED) {
		if (rss_conf->rss_key == NULL && !default_allowed) {
			PMD_DRV_LOG(ERR,
				"Setting RSS hash fields is not supported\n");
			return -ENOTSUP;
		}
		PMD_DRV_LOG(WARNING,
			"Setting RSS hash fields is not supported. Using default values: 0x%" PRIx64 "\n",
			(uint64_t)(ENA_ALL_RSS_HF));
	} else if (rc != 0)  {
		PMD_DRV_LOG(ERR, "Failed to set RSS hash fields\n");
		return rc;
	}

	return 0;
}

/* ENA HW interprets the RSS key in reverse bytes order. Because of that, the
 * key must be processed upon interaction with ena_com layer.
 */
static void ena_reorder_rss_hash_key(uint8_t *reordered_key,
				     uint8_t *key,
				     size_t key_size)
{
	size_t i, rev_i;

	for (i = 0, rev_i = key_size - 1; i < key_size; ++i, --rev_i)
		reordered_key[i] = key[rev_i];
}

static int ena_get_rss_hash_key(struct ena_com_dev *ena_dev, uint8_t *rss_key)
{
	uint8_t hw_rss_key[ENA_HASH_KEY_SIZE];
	int rc;

	/* The default RSS hash key cannot be retrieved from the HW. Unless it's
	 * explicitly set, this operation shouldn't be supported.
	 */
	if (ena_dev->rss.hash_key == NULL) {
		PMD_DRV_LOG(WARNING,
			"Retrieving default RSS hash key is not supported\n");
		return -ENOTSUP;
	}

	rc = ena_com_get_hash_key(ena_dev, hw_rss_key);
	if (rc != 0)
		return rc;

	ena_reorder_rss_hash_key(rss_key, hw_rss_key, ENA_HASH_KEY_SIZE);

	return 0;
}

int ena_rss_configure(struct ena_adapter *adapter)
{
	struct rte_eth_rss_conf *rss_conf;
	struct ena_com_dev *ena_dev;
	int rc;

	ena_dev = &adapter->ena_dev;
	rss_conf = &adapter->edev_data->dev_conf.rx_adv_conf.rss_conf;

	if (adapter->edev_data->nb_rx_queues == 0)
		return 0;

	/* Restart the indirection table. The number of queues could change
	 * between start/stop calls, so it must be reinitialized with default
	 * values.
	 */
	rc = ena_fill_indirect_table_default(ena_dev, ENA_RX_RSS_TABLE_SIZE,
		adapter->edev_data->nb_rx_queues);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR,
			"Failed to fill indirection table with default values\n");
		return rc;
	}

	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely(rc != 0 && rc != ENA_COM_UNSUPPORTED)) {
		PMD_DRV_LOG(ERR,
			"Failed to set indirection table in the device\n");
		return rc;
	}

	rc = ena_rss_hash_set(ena_dev, rss_conf, true);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR, "Failed to set RSS hash\n");
		return rc;
	}

	PMD_DRV_LOG(DEBUG, "RSS configured for port %d\n",
		adapter->edev_data->port_id);

	return 0;
}

int ena_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	int rc;

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_rss_hash_set(&adapter->ena_dev, rss_conf, false);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR, "Failed to set RSS hash\n");
		return rc;
	}

	return 0;
}

int ena_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	enum ena_admin_flow_hash_proto proto;
	uint64_t rss_hf = 0;
	int rc, i;
	uint16_t admin_hf;
	static bool warn_once;

	if (!(dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		PMD_DRV_LOG(ERR, "RSS was not configured for the PMD\n");
		return -ENOTSUP;
	}

	if (rss_conf->rss_key != NULL) {
		rc = ena_get_rss_hash_key(ena_dev, rss_conf->rss_key);
		if (unlikely(rc != 0)) {
			PMD_DRV_LOG(ERR,
				"Cannot retrieve RSS hash key, err: %d\n",
				rc);
			return rc;
		}
	}

	for (i = 0; i < ENA_ADMIN_RSS_PROTO_NUM; ++i) {
		proto = (enum ena_admin_flow_hash_proto)i;
		rte_spinlock_lock(&adapter->admin_lock);
		rc = ena_com_get_hash_ctrl(ena_dev, proto, &admin_hf);
		rte_spinlock_unlock(&adapter->admin_lock);
		if (rc == ENA_COM_UNSUPPORTED) {
			/* As some devices may support only reading rss hash
			 * key and not the hash ctrl, we want to notify the
			 * caller that this feature is only partially supported
			 * and do not return an error - the caller could be
			 * interested only in the key value.
			 */
			if (!warn_once) {
				PMD_DRV_LOG(WARNING,
					"Reading hash control from the device is not supported. .rss_hf will contain a default value.\n");
				warn_once = true;
			}
			rss_hf = ENA_ALL_RSS_HF;
			break;
		} else if (rc != 0) {
			PMD_DRV_LOG(ERR,
				"Failed to retrieve hash ctrl for proto: %d with err: %d\n",
				i, rc);
			return rc;
		}

		rss_hf |= ena_admin_hf_to_eth_hf(proto, admin_hf);
	}

	rss_conf->rss_hf = rss_hf;
	return 0;
}
