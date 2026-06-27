/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_rss.h"
#include "sxe2_common_log.h"
#include "sxe2_ethdev.h"
#include "sxe2_cmd_chnl.h"

void sxe2_sw_rss_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_rss_hash_caps *rss_caps)
{
	adapter->rss_ctxt.rss_key_size = rss_caps->hash_key_size;
	adapter->rss_ctxt.rss_lut_size = rss_caps->lut_key_size;
}

int32_t sxe2_rss_hash_key_init(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf *rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;
	uint16_t i = 0;

	if (rss_ctxt->rss_key == NULL) {
		rss_ctxt->rss_key = (uint8_t *)rte_zmalloc("rss_key", rss_ctxt->rss_key_size, 0);
		if (rss_ctxt->rss_key == NULL) {
			PMD_LOG_ERR(INIT, "Failed to allocate rss key");
			ret = -ENOMEM;
			goto l_end;
		}
	}

	if (!rss_conf->rss_key) {
		for (i = 0; i < rss_ctxt->rss_key_size; i++)
			rss_ctxt->rss_key[i] = (uint8_t)rte_rand();
	} else {
		rte_memcpy(rss_ctxt->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len, rss_ctxt->rss_key_size));
	}

	ret = sxe2_drv_rss_key_set(adapter, rss_ctxt->rss_key,
				   rss_ctxt->rss_key_size);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set rss key, ret:%d", ret);
		rte_free(rss_ctxt->rss_key);
		rss_ctxt->rss_key = NULL;
		goto l_end;
	}

l_end:
	return ret;
}

void sxe2_rss_hash_key_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;

	if (rss_ctxt->rss_key) {
		rte_free(rss_ctxt->rss_key);
		rss_ctxt->rss_key = NULL;
	}
}

int32_t sxe2_rss_lut_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;
	uint16_t i;

	if (rss_ctxt->rss_lut == NULL) {
		rss_ctxt->rss_lut = (uint8_t *)rte_zmalloc("rss_lut", rss_ctxt->rss_lut_size, 0);
		if (rss_ctxt->rss_lut == NULL) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to allocate rss lut");
			ret = -ENOMEM;
			goto l_end;
		}
	}

	for (i = 0; i < rss_ctxt->rss_lut_size; i++)
		rss_ctxt->rss_lut[i] = (uint8_t)(i % dev->data->nb_rx_queues);

	ret = sxe2_drv_rss_lut_set(adapter, rss_ctxt->rss_lut, rss_ctxt->rss_lut_size);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set rss lut, ret:%d", ret);
		rte_free(rss_ctxt->rss_lut);
		rss_ctxt->rss_lut = NULL;
		goto l_end;
	}

l_end:
	return ret;
}

void sxe2_rss_lut_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;

	if (rss_ctxt->rss_lut) {
		rte_free(rss_ctxt->rss_lut);
		rss_ctxt->rss_lut = NULL;
	}
}

static struct sxe2_rss_hf_config sxe2_rss_default_hf_config[] = {
	{
		.rss_hf = RTE_ETH_RSS_L2_PAYLOAD,
		.hdrs = {SXE2_FLOW_HDR_ETH,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_ETH_TYPE,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_IPV4,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_IPV6,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_FRAG_IPV4,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_HDR_IPV_FRAG,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_FRAG_IPV6,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_HDR_IPV_FRAG,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_HDR_IPV_OTHER,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_HDR_IPV_OTHER,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV4_UDP,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_HDR_UDP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_FLD_ID_UDP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_UDP_DST_PORT,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV6_UDP,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_HDR_UDP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_FLD_ID_UDP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_UDP_DST_PORT,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV4_TCP,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_HDR_TCP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_FLD_ID_TCP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_TCP_DST_PORT,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV6_TCP,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_HDR_TCP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_FLD_ID_TCP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_TCP_DST_PORT,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV4_SCTP,
		.hdrs = {SXE2_FLOW_HDR_IPV4,
				 SXE2_FLOW_HDR_SCTP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV4_SA,
				 SXE2_FLOW_FLD_ID_IPV4_DA,
				 SXE2_FLOW_FLD_ID_SCTP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_SCTP_DST_PORT,
				 SXE2_FLOW_END},
	},
	{
		.rss_hf = RTE_ETH_RSS_NONFRAG_IPV6_SCTP,
		.hdrs = {SXE2_FLOW_HDR_IPV6,
				 SXE2_FLOW_HDR_SCTP,
				 SXE2_FLOW_END},
		.flds = {SXE2_FLOW_FLD_ID_IPV6_SA,
				 SXE2_FLOW_FLD_ID_IPV6_DA,
				 SXE2_FLOW_FLD_ID_SCTP_SRC_PORT,
				 SXE2_FLOW_FLD_ID_SCTP_DST_PORT,
				 SXE2_FLOW_END},
	},
};

int32_t sxe2_rss_hf_type_set(struct rte_eth_dev *dev, uint64_t rss_hf)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;
	uint32_t i;
	uint8_t symm = 0;

	if (0 == (rss_hf & SXE2_RSS_HF_SUPPORT_ALL) && rss_hf != 0) {
		PMD_DEV_LOG_ERR(adapter, DRV,
			"Failed to set unsupported rss_hf:0x%016" PRIx64,
			rss_hf);
		ret = -EINVAL;
		goto l_end;
	}

	for (i = 0; i < RTE_DIM(sxe2_rss_default_hf_config); i++) {
		if (rss_ctxt->rss_hf & sxe2_rss_default_hf_config[i].rss_hf) {
			sxe2_rss_default_hf_config[i].symm = rss_ctxt->symm;
			ret = sxe2_drv_rss_hf_del(adapter, &sxe2_rss_default_hf_config[i]);
			if (ret) {
				PMD_DEV_LOG_ERR(adapter, INIT,
					"Failed to del rss hf cfg[%d], ret:%d", i, ret);
				goto l_end;
			}
		}
	}

	if (rss_ctxt->hash_func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
		symm = 1;

	for (i = 0; i < RTE_DIM(sxe2_rss_default_hf_config); i++) {
		if (rss_hf & sxe2_rss_default_hf_config[i].rss_hf) {
			sxe2_rss_default_hf_config[i].symm = symm;
			ret = sxe2_drv_rss_hf_add(adapter, &sxe2_rss_default_hf_config[i]);
			if (ret) {
				PMD_DEV_LOG_ERR(adapter, INIT,
					"Failed to add rss hf cfg[%d], ret:%d", i, ret);
				goto l_end;
			}
		}
	}

	rss_ctxt->rss_hf = rss_hf & SXE2_RSS_HF_SUPPORT_ALL;
	rss_ctxt->symm = symm;
l_end:
	return ret;
}

int32_t sxe2_rss_hash_function_set(struct rte_eth_dev *dev, enum rte_eth_hash_function func)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	enum sxe2_rss_hash_key_func hash_func = SXE2_RSS_HASH_FUNC_SYM_TOEPLITZ;
	int32_t ret = 0;

	switch (func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
		hash_func = SXE2_RSS_HASH_FUNC_SYM_TOEPLITZ;
		break;
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		hash_func = SXE2_RSS_HASH_FUNC_XOR;
		break;
	default:
		PMD_DEV_LOG_ERR(adapter, DRV, "RSS hash function[%d] not support.", func);
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_drv_rss_hash_ctrl_func(adapter, hash_func);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set rss hash function, ret=[%d]", ret);
		goto l_end;
	}

l_end:
	return ret;
}

int32_t sxe2_rss_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_eth_rss_conf *rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	enum rte_eth_hash_function rss_func = RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;
	int32_t ret = 0;

	adapter->rss_ctxt.inited = false;

	if (dev->data->nb_rx_queues <= 1) {
		PMD_DEV_LOG_DEBUG(adapter, INIT, "No need to init rss, rx queues %d.",
				dev->data->nb_rx_queues);
		goto l_end;
	}

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_RSS) == 0) {
		PMD_DEV_LOG_WARN(adapter, INIT, "RSS not supported");
		goto l_end;
	}

	ret = sxe2_rss_hash_key_init(dev);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to init rss key");
		goto l_end;
	}

	ret = sxe2_rss_lut_init(dev);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to init rss lut");
		goto l_err_key;
	}

	rss_func = rss_conf->algorithm;
	ret = sxe2_rss_hash_function_set(dev, rss_func);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to init rss hash function");
		goto l_err_lut;
	}
	ret = sxe2_rss_hf_type_set(dev, rss_conf->rss_hf);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set rss hf type");
		goto l_err_lut;
	}
	adapter->rss_ctxt.inited = true;
	goto l_end;

l_err_lut:
	sxe2_rss_lut_uninit(dev);
l_err_key:
	sxe2_rss_hash_key_uninit(dev);
l_end:
	return ret;
}

int32_t sxe2_rss_disable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	if ((adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_RSS) == 0)
		goto l_end;

	ret = sxe2_drv_rss_hf_clear(adapter);
	if (ret)
		PMD_LOG_ERR(INIT, "Failed to clear rss hf");

	sxe2_rss_hash_key_uninit(dev);

	sxe2_rss_lut_uninit(dev);

l_end:
	return ret;
}

int32_t sxe2_dev_rss_reta_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	uint8_t *lut_tmp = NULL;
	int32_t ret = 0;
	uint16_t i;
	uint16_t shift;
	uint16_t idx;

	if (!adapter->rss_ctxt.inited) {
		PMD_DEV_LOG_INFO(adapter, DRV, "RSS not inited.");
		ret = -ENOTSUP;
		goto l_end;
	}

	if (reta_size != rss_ctxt->rss_lut_size) {
		PMD_DEV_LOG_ERR(adapter, DRV, "The size of hash lookup table configured "
				"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, rss_ctxt->rss_lut_size);
		ret = -EINVAL;
		goto l_end;
	}

	lut_tmp = rte_zmalloc("rss_lut_temp", reta_size, 0);
	if (!lut_tmp) {
		PMD_DEV_LOG_ERR(adapter, DRV, "No memory can be allocated");
		ret = -ENOMEM;
		goto l_end;
	}
	rte_memcpy(lut_tmp, rss_ctxt->rss_lut, reta_size);

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut_tmp[i] = reta_conf[idx].reta[shift];
	}

	ret = sxe2_drv_rss_lut_set(adapter, lut_tmp, reta_size);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set rss lut");
		goto l_end;
	}

	rte_memcpy(rss_ctxt->rss_lut, lut_tmp, reta_size);

l_end:
	if (lut_tmp)
		rte_free(lut_tmp);
	return ret;
}

int32_t sxe2_dev_rss_reta_query(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;
	uint16_t i;
	uint16_t shift;
	uint16_t idx;

	if (!adapter->rss_ctxt.inited) {
		PMD_DEV_LOG_INFO(adapter, DRV, "RSS not inited.");
		ret = -ENOTSUP;
		goto l_end;
	}

	if (reta_size != rss_ctxt->rss_lut_size) {
		PMD_LOG_ERR(INIT, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, rss_ctxt->rss_lut_size);
		ret = -EINVAL;
		goto l_end;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = rss_ctxt->rss_lut[i];
	}

l_end:
	return ret;
}

static int32_t sxe2_rss_hash_key_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;
	uint16_t i;

	if (rss_conf->rss_key_len == 0 || rss_conf->rss_key == NULL)
		goto l_end;

	if (rss_conf->rss_key_len != rss_ctxt->rss_key_size) {
		PMD_DEV_LOG_ERR(adapter, DRV, "The size of hash key configured "
			"(%d) doesn't match the size of hardware can "
			"support (%d)", rss_conf->rss_key_len,
			rss_ctxt->rss_key_size);
		ret = -EINVAL;
		goto l_end;
	}

	for (i = 0; i < rss_conf->rss_key_len; i++) {
		if (rss_conf->rss_key[i] != rss_ctxt->rss_key[i])
			break;
	}
	if (i == rss_conf->rss_key_len)
		goto l_end;

	ret = sxe2_drv_rss_key_set(adapter, rss_conf->rss_key,
				   rss_conf->rss_key_len);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set rss key");
		goto l_end;
	}

	rte_memcpy(rss_ctxt->rss_key, rss_conf->rss_key, rss_conf->rss_key_len);
l_end:
	return ret;
}

int32_t sxe2_dev_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = -1;

	if (!adapter->rss_ctxt.inited) {
		PMD_DEV_LOG_INFO(adapter, DRV, "RSS not inited.");
		ret = -ENOTSUP;
		goto l_end;
	}

	ret = sxe2_rss_hash_key_update(dev, rss_conf);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set rss hash key");
		goto l_end;
	}

	if (rss_conf->algorithm != rss_ctxt->hash_func) {
		ret = sxe2_rss_hash_function_set(dev, rss_conf->algorithm);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set rss hash function");
			goto l_end;
		}
		rss_ctxt->hash_func = rss_conf->algorithm;
	}

	if ((rss_conf->rss_hf & SXE2_RSS_HF_SUPPORT_ALL)
			!= rss_ctxt->rss_hf) {
		ret = sxe2_rss_hf_type_set(dev, rss_conf->rss_hf);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set rss hf type");
			goto l_end;
		}
	}
	ret = 0;
l_end:
	return ret;
}

int32_t sxe2_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_rss_context *rss_ctxt = &adapter->rss_ctxt;
	int32_t ret = 0;

	if (adapter->rss_ctxt.inited == 0) {
		PMD_DEV_LOG_INFO(adapter, DRV, "RSS not inited.");
		ret = -ENOTSUP;
		goto l_end;
	}

	if (rss_conf->rss_key) {
		rss_conf->rss_key_len = rss_ctxt->rss_key_size;
		rte_memcpy(rss_conf->rss_key, rss_ctxt->rss_key, rss_ctxt->rss_key_size);
	}
	rss_conf->rss_hf = rss_ctxt->rss_hf;
	rss_conf->algorithm = rss_ctxt->hash_func;
l_end:
	return ret;
}
