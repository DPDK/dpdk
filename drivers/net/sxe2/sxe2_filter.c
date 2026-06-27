/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_os.h>
#include <rte_tailq.h>
#include "sxe2_osal.h"
#include "sxe2_mac.h"
#include "sxe2_common_log.h"
#include "sxe2_ethdev.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_switchdev.h"

static struct sxe2_mac_filter *sxe2_uc_filter_find(struct sxe2_adapter *adapter,
			struct rte_ether_addr *macaddr)
{
	struct sxe2_mac_filter *filter      = NULL;
	struct sxe2_mac_filter *entry       = NULL;
	struct sxe2_mac_filter *next_entry  = NULL;

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	RTE_TAILQ_FOREACH_SAFE(entry, &adapter->filter_ctxt.uc_list, next, next_entry) {
		if (rte_is_same_ether_addr(macaddr, &entry->mac_addr)) {
			filter = entry;
			break;
		}
	}
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	return filter;
}

int32_t sxe2_uc_filter_add(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr, bool default_config)
{
	struct sxe2_mac_filter *filter = NULL;
	bool hw_config = false;
	int32_t ret = 0;

	filter = sxe2_uc_filter_find(adapter, mac_addr);
	if (filter) {
		if (default_config && !filter->default_config)
			filter->default_config = true;
		PMD_DEV_LOG_INFO(adapter, DRV, "This MAC filter already exists.");
		goto l_end;
	}

	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot add hw uc addr in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add hw uc addr in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add hw uc addr in switchdev mode");
	} else {
		ret = sxe2_drv_uc_config(adapter, mac_addr, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add uc rule");
			ret = -EINVAL;
			goto l_end;
		}
		hw_config = true;
	}

	filter = rte_zmalloc("sxe2_uc_filter",
			     sizeof(struct sxe2_mac_filter), 0);
	if (!filter) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to allocate memory");
		ret = -ENOMEM;
		goto l_end;
	}
	filter->hw_config = hw_config;
	filter->default_config = default_config;
	rte_ether_addr_copy(mac_addr, &filter->mac_addr);
	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_INSERT_TAIL(&adapter->filter_ctxt.uc_list, filter, next);
	adapter->filter_ctxt.uc_num++;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	PMD_DEV_LOG_INFO(adapter, DRV, "add mac rule, mac num %u.", adapter->filter_ctxt.uc_num);
	ret = 0;

l_end:
	return ret;
}

int32_t sxe2_uc_filter_del(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr)
{
	struct sxe2_mac_filter *filter = NULL;
	int32_t ret                         = -1;

	filter = sxe2_uc_filter_find(adapter, mac_addr);
	if (!filter) {
		PMD_DEV_LOG_INFO(adapter, DRV, "This MAC filter not exists.");
		ret = 0;
		goto l_end;
	}
	if (filter->hw_config) {
		ret = sxe2_drv_uc_config(adapter, mac_addr, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete mac rule");
			if (ret == -EPERM)
				goto l_free;
			ret = -EINVAL;
			goto l_end;
		}
	}
	PMD_DEV_LOG_INFO(adapter, DRV, "remove mac rule, uc num %u.", adapter->filter_ctxt.uc_num);
	ret = 0;

l_free:

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_REMOVE(&adapter->filter_ctxt.uc_list, filter, next);
	adapter->filter_ctxt.uc_num--;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);
	rte_free(filter);
	filter = NULL;
l_end:
	return ret;
}

void sxe2_uc_filter_clear(struct sxe2_adapter *adapter, bool default_config)
{
	struct sxe2_mac_filter *entry;
	struct sxe2_mac_filter *next_entry;

	RTE_TAILQ_FOREACH_SAFE(entry, &adapter->filter_ctxt.uc_list, next, next_entry) {
		if (entry->default_config && !default_config)
			continue;

		if (sxe2_uc_filter_del(adapter, &entry->mac_addr))
			PMD_DEV_LOG_ERR(adapter, DRV, "This MAC filter delete fail.");
	}
}

static struct sxe2_mac_filter *sxe2_mc_filter_find(struct sxe2_adapter *adapter,
			struct rte_ether_addr *macaddr)
{
	struct sxe2_mac_filter *filter      = NULL;
	struct sxe2_mac_filter *entry       = NULL;
	struct sxe2_mac_filter *next_entry  = NULL;

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	RTE_TAILQ_FOREACH_SAFE(entry, &adapter->filter_ctxt.mc_list, next, next_entry) {
		if (rte_is_same_ether_addr(macaddr, &entry->mac_addr)) {
			filter = entry;
			break;
		}
	}
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	return filter;
}

int32_t sxe2_mc_filter_add(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr, bool default_config)
{
	struct sxe2_mac_filter *filter = NULL;
	bool hw_config = false;
	int32_t ret = 0;

	filter = sxe2_mc_filter_find(adapter, mac_addr);
	if (filter) {
		if (default_config && !filter->default_config)
			filter->default_config = true;
		PMD_DEV_LOG_INFO(adapter, DRV, "This MAC filter already exists.");
		goto l_end;
	}

	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot add hw mc addr in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add hw mc addr in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add hw mc addr in switchdev mode");
	} else {
		ret = sxe2_drv_mc_config(adapter, mac_addr, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add mac rule");
			ret = -EINVAL;
			goto l_end;
		}
		hw_config = true;
	}

	filter = rte_zmalloc("sxe2_mc_filter",
			     sizeof(struct sxe2_mac_filter), 0);
	if (!filter) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to allocate memory");
		ret = -ENOMEM;
		goto l_end;
	}
	filter->hw_config = hw_config;
	filter->default_config = default_config;
	rte_ether_addr_copy(mac_addr, &filter->mac_addr);
	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_INSERT_TAIL(&adapter->filter_ctxt.mc_list, filter, next);
	adapter->filter_ctxt.mc_num++;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	PMD_DEV_LOG_INFO(adapter, DRV, "add mc rule, mc num %u.", adapter->filter_ctxt.mc_num);
	ret = 0;

l_end:
	return ret;
}

int32_t sxe2_mc_filter_del(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr)
{
	struct sxe2_mac_filter *filter = NULL;
	int32_t ret                         = -1;

	filter = sxe2_mc_filter_find(adapter, mac_addr);
	if (!filter) {
		PMD_DEV_LOG_INFO(adapter, DRV, "This MAC filter not exists.");
		ret = 0;
		goto l_end;
	}

	if (filter->hw_config) {
		ret = sxe2_drv_mc_config(adapter, mac_addr, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete mc rule");
			if (ret == -EPERM)
				goto l_free;
			ret = -EINVAL;
			goto l_end;
		}
	}
	PMD_DEV_LOG_INFO(adapter, DRV, "remove mc rule, mc num %u.", adapter->filter_ctxt.mc_num);
	ret = 0;

l_free:

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_REMOVE(&adapter->filter_ctxt.mc_list, filter, next);
	adapter->filter_ctxt.mc_num--;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);
	rte_free(filter);
	filter = NULL;
l_end:
	return ret;
}

void sxe2_mc_filter_clear(struct sxe2_adapter *adapter, bool default_config)
{
	struct sxe2_mac_filter *entry;
	struct sxe2_mac_filter *next_entry;

	RTE_TAILQ_FOREACH_SAFE(entry, &adapter->filter_ctxt.mc_list, next, next_entry) {
		if (entry->default_config && !default_config)
			continue;
		if (sxe2_mc_filter_del(adapter, &entry->mac_addr))
			PMD_DEV_LOG_ERR(adapter, DRV, "This MAC filter delete fail.");
	}
}

static struct sxe2_vlan_filter *sxe2_vlan_filter_find(struct sxe2_adapter *adapter,
			struct sxe2_vlan *vlan)
{
	struct sxe2_vlan_filter *f;
	struct sxe2_vlan_filter *save_f = NULL;

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_FOREACH(f, &adapter->filter_ctxt.vlan_list, next)
	{
		if (vlan->tpid == f->vlan_info.tpid &&
			vlan->vid == f->vlan_info.vid) {
			save_f = f;
			break;
		}
	}
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	return save_f;
}

int32_t sxe2_vlan_filter_add(struct sxe2_adapter *adapter,
			     struct sxe2_vlan *vlan, bool default_config)
{
	struct sxe2_vlan_filter *filter = NULL;
	bool hw_config                 = false;
	int32_t ret                    = 0;

	if (!vlan || vlan->vid > RTE_ETHER_MAX_VLAN_ID) {
		PMD_DEV_LOG_ERR(adapter, DRV, "This vlan filter is invalid.");
		ret = -EINVAL;
		goto l_end;
	}

	filter = sxe2_vlan_filter_find(adapter, vlan);
	if (filter) {
		PMD_DEV_LOG_INFO(adapter, DRV, "This vlan filter already exists.");
		ret = 0;
		goto l_end;
	}
	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot add vlan in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add vlan in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add vlan in switchdev mode");
	} else {
		ret = sxe2_drv_vlan_filter_id_config(adapter, vlan, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add vlan rule");
			ret = -EINVAL;
			goto l_end;
		}
		hw_config = true;
	}

	filter = rte_zmalloc("sxe2_vlan_filter", sizeof(*filter), 0);
	if (!filter) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to allocate memory");
		ret = -ENOMEM;
		goto l_end;
	}

	filter->hw_config = hw_config;
	filter->default_config = default_config;

	filter->vlan_info.tpid = vlan->tpid;
	filter->vlan_info.vid = vlan->vid;
	filter->vlan_info.prio = vlan->prio;

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_INSERT_TAIL(&adapter->filter_ctxt.vlan_list, filter, next);
	adapter->filter_ctxt.vlan_num++;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);

	ret = 0;

l_end:
	return ret;
}

int32_t sxe2_vlan_filter_del(struct sxe2_adapter *adapter, struct sxe2_vlan *vlan)
{
	struct sxe2_vlan_filter *filter = NULL;
	int32_t ret                         = -1;

	if (!vlan || vlan->vid > RTE_ETHER_MAX_VLAN_ID) {
		PMD_DEV_LOG_INFO(adapter, DRV, "This vlan filter is invalid.");
		ret = -EINVAL;
		goto l_end;
	}

	filter = sxe2_vlan_filter_find(adapter, vlan);
	if (!filter) {
		PMD_DEV_LOG_INFO(adapter, DRV, "This vlan filter not exists.");
		ret = 0;
		goto l_end;
	}

	if (filter->hw_config) {
		ret = sxe2_drv_vlan_filter_id_config(adapter, vlan, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete vlan rule");
			if (ret == -EPERM)
				goto l_free;
			ret = -EINVAL;
			goto l_end;
		}
	}
	ret = 0;

l_free:

	rte_spinlock_lock(&adapter->filter_ctxt.filter_lock);
	TAILQ_REMOVE(&adapter->filter_ctxt.vlan_list, filter, next);
	adapter->filter_ctxt.vlan_num--;
	rte_spinlock_unlock(&adapter->filter_ctxt.filter_lock);
	rte_free(filter);
	filter = NULL;
l_end:
	return ret;
}

void sxe2_vlan_filters_clear(struct sxe2_adapter *adapter, bool default_config)
{
	int32_t ret = 0;
	struct sxe2_vlan_filter *v_f;
	void *temp;

	if (adapter->filter_ctxt.vlan_num == 0)
		return;

	RTE_TAILQ_FOREACH_SAFE(v_f, &adapter->filter_ctxt.vlan_list, next, temp)
	{
		if (v_f->default_config && !default_config)
			continue;
		ret = sxe2_vlan_filter_del(adapter, &v_f->vlan_info);
		if (ret)
			PMD_DEV_LOG_ERR(adapter, DRV, "This vlan filter delete fail.");
	}
}

int32_t sxe2_vlan_filter_ctrl(struct sxe2_adapter *adapter, bool flag)
{
	struct sxe2_vlan_info *vlan_info = &adapter->filter_ctxt.vlan_info;
	int32_t ret = 0;

	if (vlan_info->filter_on == flag)
		goto l_end;
	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot add vlan filter ctrl in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add vlan filter ctrl in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot add vlan filter ctrl in switchdev mode");
	} else {
		ret = sxe2_drv_vlan_filter_switch(adapter, flag);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add vlan filter ctrl");
			goto l_end;
		}
		vlan_info->hw_filter_on = flag;
	}
	vlan_info->filter_on = flag;

l_end:
	return ret;
}

int32_t sxe2_promisc_add(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot enable promiscuous in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot enable promiscuous in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot enable promiscuous in switchdev mode");
	} else if (!(adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC)) {
		ret = sxe2_drv_promisc_config(adapter, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg promiscuous, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags |= SXE2_PROMISC;
	}
	adapter->filter_ctxt.cur_promisc_flags |= SXE2_PROMISC;

l_end:
	return ret;
}

int32_t sxe2_promisc_del(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->flow_isolated &&
	    (adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC)) {
		ret = sxe2_drv_promisc_config(adapter, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg promiscuous, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags &= ~SXE2_PROMISC;
	}

	adapter->filter_ctxt.cur_promisc_flags &= ~SXE2_PROMISC;

l_end:
	return ret;
}

int32_t sxe2_allmulti_add(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->rule_started) {
		PMD_DEV_LOG_DEBUG(adapter, DRV, "cannot enable allmulticast in port stop status");
	} else if (adapter->flow_isolated) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot enable allmulticast in flow isolation mode");
	} else if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_WARN(adapter, DRV, "cannot enable allmulticast in switchdev mode");
	} else if (!(adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC_MULTICAST)) {
		ret = sxe2_drv_allmulti_config(adapter, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg allmulticast, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags |= SXE2_PROMISC_MULTICAST;
	}
	adapter->filter_ctxt.cur_promisc_flags |= SXE2_PROMISC_MULTICAST;

l_end:
	return ret;
}

int32_t sxe2_allmulti_del(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->flow_isolated &&
	    (adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC_MULTICAST)) {
		ret = sxe2_drv_allmulti_config(adapter, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg allmulticast, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags &= ~SXE2_PROMISC_MULTICAST;
	}

	adapter->filter_ctxt.cur_promisc_flags &= ~SXE2_PROMISC_MULTICAST;
l_end:
	return ret;
}

static int32_t sxe2_all_filter_hw_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_mac_filter *mac_entry;
	struct sxe2_mac_filter *next_mac_entry;
	struct sxe2_vlan_filter *vlan_entry;
	struct sxe2_vlan_filter *next_vlan_entry;

	if (adapter->filter_ctxt.uc_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.uc_list, next,
			    next_mac_entry) {
			if (mac_entry->hw_config) {
				ret = sxe2_drv_uc_config(adapter, &mac_entry->mac_addr, false);
				if (ret) {
					PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete mac rule");
					ret = -EINVAL;
					goto l_end;
				}
				mac_entry->hw_config = false;
			}
		}
	}

	if (adapter->filter_ctxt.mc_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.mc_list, next,
			    next_mac_entry) {
			if (mac_entry->hw_config) {
				ret = sxe2_drv_mc_config(adapter, &mac_entry->mac_addr, false);
				if (ret) {
					PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete mc rule");
					ret = -EINVAL;
					goto l_end;
				}
				mac_entry->hw_config = false;
			}
		}
	}

	if (adapter->filter_ctxt.vlan_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(vlan_entry, &adapter->filter_ctxt.vlan_list, next,
			    next_vlan_entry) {
			if (vlan_entry->hw_config) {
				ret = sxe2_drv_vlan_filter_id_config(adapter,
				    &vlan_entry->vlan_info, false);
				if (ret) {
					PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete vlan rule");
					ret = -EINVAL;
					goto l_end;
				}
				vlan_entry->hw_config = false;
			}
		}
	}

	if (adapter->filter_ctxt.vlan_info.hw_filter_on) {
		ret = sxe2_drv_vlan_filter_switch(adapter, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to delete vlan rule");
			ret = -EINVAL;
			goto l_end;
		}
		adapter->filter_ctxt.vlan_info.hw_filter_on = false;
	}

	if (adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC) {
		ret = sxe2_drv_promisc_config(adapter, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg promiscuous, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags &= ~SXE2_PROMISC;
	}

	if (adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC_MULTICAST) {
		ret = sxe2_drv_allmulti_config(adapter, false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "failed to cfg allmulticast, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags &= ~SXE2_PROMISC_MULTICAST;
	}
l_end:
	return ret;
}

static int32_t sxe2_all_filter_hw_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_mac_filter *mac_entry;
	struct sxe2_mac_filter *next_mac_entry;
	struct sxe2_vlan_filter *vlan_entry;
	struct sxe2_vlan_filter *next_vlan_entry;

	if (adapter->filter_ctxt.uc_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.uc_list, next,
				       next_mac_entry) {
			if (!mac_entry->hw_config) {
				ret = sxe2_drv_uc_config(adapter, &mac_entry->mac_addr,
							 true);
				if (ret && ret != -EEXIST) {
					PMD_DEV_LOG_ERR(adapter, DRV,
							"Failed to add uc rule, ret:%d", ret);
					ret = -EINVAL;
					goto l_end;
				}
				mac_entry->hw_config = true;
				ret = 0;
			}
		}
	}

	if (adapter->filter_ctxt.mc_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.mc_list, next,
				       next_mac_entry) {
			if (!mac_entry->hw_config) {
				ret = sxe2_drv_mc_config(adapter, &mac_entry->mac_addr, true);
				if (ret && ret != -EEXIST) {
					PMD_DEV_LOG_ERR(adapter, DRV,
							"Failed to add mc rule, ret:%d", ret);
					ret = -EINVAL;
					goto l_end;
				}
				mac_entry->hw_config = true;
				ret = 0;
			}
		}
	}

	if (adapter->filter_ctxt.vlan_num > 0) {
		RTE_TAILQ_FOREACH_SAFE(vlan_entry, &adapter->filter_ctxt.vlan_list, next,
				       next_vlan_entry) {
			if (!vlan_entry->hw_config) {
				ret = sxe2_drv_vlan_filter_id_config(adapter,
				    &vlan_entry->vlan_info, true);
				if (ret && ret != -EEXIST) {
					PMD_DEV_LOG_ERR(adapter, DRV,
							"Failed to add vlan rule, ret:%d", ret);
					ret = -EINVAL;
					goto l_end;
				}
				vlan_entry->hw_config = true;
				ret = 0;
			}
		}
	}

	if (adapter->filter_ctxt.vlan_info.filter_on) {
		if (!(adapter->filter_ctxt.vlan_info.hw_filter_on)) {
			ret = sxe2_drv_vlan_filter_switch(adapter, true);
			if (ret && ret != -EEXIST) {
				PMD_DEV_LOG_ERR(adapter, DRV,
						"Failed to add vlan ctrl, ret:%d", ret);
				ret = -EINVAL;
				goto l_end;
			}
			adapter->filter_ctxt.vlan_info.hw_filter_on = true;
			ret = 0;
		}
	}

	if ((adapter->filter_ctxt.cur_promisc_flags & SXE2_PROMISC) &&
	    (!(adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC))) {
		ret = sxe2_drv_promisc_config(adapter, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV,
					"Failed to set promisc, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags |= SXE2_PROMISC;
		ret = 0;
	}

	if ((adapter->filter_ctxt.cur_promisc_flags & SXE2_PROMISC_MULTICAST) &&
	    (!(adapter->filter_ctxt.hw_promisc_flags & SXE2_PROMISC_MULTICAST))) {
		ret = sxe2_drv_allmulti_config(adapter, true);
		if (ret && ret != -EEXIST) {
			PMD_DEV_LOG_ERR(adapter, DRV,
					"Failed to set allmulti, ret:%d", ret);
			goto l_end;
		}
		adapter->filter_ctxt.hw_promisc_flags |= SXE2_PROMISC_MULTICAST;
		ret = 0;
	}
l_end:
	return ret;
}

static int32_t sxe2_uplink_hw_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (adapter->filter_ctxt.hw_uplink_config) {
		if (adapter->dev_type == SXE2_DEV_T_PF) {
			ret = sxe2_uplink_clear(adapter);
			if (ret) {
				PMD_DEV_LOG_ERR(adapter, DRV,
						"Failed to clear uplink, ret:%d", ret);
				goto l_end;
			}
			adapter->filter_ctxt.hw_uplink_config = false;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_uplink_hw_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->filter_ctxt.hw_uplink_config) {
		if (adapter->dev_type == SXE2_DEV_T_PF) {
			ret = sxe2_uplink_set(adapter);
			if (ret && ret != -EEXIST) {
				PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set uplink, ret:%d", ret);
				goto l_end;
			}
			adapter->filter_ctxt.hw_uplink_config = true;
			ret = 0;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_repr_hw_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (adapter->filter_ctxt.hw_repr_config) {
		if (adapter->dev_type == SXE2_DEV_T_PF ||
			adapter->dev_type == SXE2_DEV_T_PF_BOND) {
			ret = sxe2_repr_clear(adapter);
			if (ret) {
				PMD_DEV_LOG_ERR(adapter, DRV, "Failed to clear repr, ret:%d", ret);
				goto l_end;
			}
			adapter->filter_ctxt.hw_repr_config = false;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_repr_hw_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->filter_ctxt.hw_repr_config) {
		if (adapter->dev_type == SXE2_DEV_T_PF ||
			adapter->dev_type == SXE2_DEV_T_PF_BOND) {
			ret = sxe2_repr_set(adapter);
			if (ret && ret != -EEXIST) {
				PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set repr, ret:%d", ret);
				goto l_end;
			}
			adapter->filter_ctxt.hw_repr_config = true;
			ret = 0;
		}
	}
l_end:
	return ret;
}

int32_t sxe2_l2_rule_update(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->flow_isolated &&
	    !adapter->switchdev_info.is_switchdev &&
	    adapter->rule_started)
		adapter->filter_ctxt.cur_l2_config = true;
	else
		adapter->filter_ctxt.cur_l2_config = false;

	if (adapter->filter_ctxt.cur_l2_config !=
	    adapter->filter_ctxt.hw_l2_config) {
		if (adapter->filter_ctxt.cur_l2_config) {
			ret = sxe2_all_filter_hw_set(adapter);
			if (!ret)
				adapter->filter_ctxt.hw_l2_config = true;
		} else {
			ret = sxe2_all_filter_hw_clear(adapter);
			if (!ret)
				adapter->filter_ctxt.hw_l2_config = false;
		}
	}
	return ret;
}

int32_t sxe2_switchdev_rule_update(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->flow_isolated &&
	    adapter->switchdev_info.is_switchdev) {
		adapter->filter_ctxt.cur_uplink_config = true;
		adapter->filter_ctxt.cur_repr_config = true;
	} else {
		adapter->filter_ctxt.cur_uplink_config = false;
		adapter->filter_ctxt.cur_repr_config = false;
	}

	if (adapter->filter_ctxt.cur_uplink_config !=
	    adapter->filter_ctxt.hw_uplink_config) {
		if (adapter->filter_ctxt.cur_uplink_config)
			ret = sxe2_uplink_hw_set(adapter);
		else
			ret = sxe2_uplink_hw_clear(adapter);
	}

	if (adapter->filter_ctxt.cur_repr_config !=
	    adapter->filter_ctxt.hw_repr_config) {
		if (adapter->filter_ctxt.cur_repr_config)
			ret = sxe2_repr_hw_set(adapter);
		else
			ret = sxe2_repr_hw_clear(adapter);
	}

	return ret;
}

int32_t sxe2_filter_rule_stop(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	adapter->rule_started = 0;

	ret = sxe2_l2_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update l2 rule");

	return ret;
}

int32_t sxe2_filter_rule_start(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	adapter->rule_started = 1;

	ret = sxe2_l2_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update l2 rule");

	return ret;
}

int32_t sxe2_filter_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	rte_spinlock_init(&adapter->filter_ctxt.filter_lock);

	TAILQ_INIT(&adapter->filter_ctxt.uc_list);
	adapter->filter_ctxt.uc_num = 0;

	TAILQ_INIT(&adapter->filter_ctxt.mc_list);
	adapter->filter_ctxt.mc_num = 0;

	TAILQ_INIT(&adapter->filter_ctxt.vlan_list);
	adapter->filter_ctxt.vlan_num = 0;
	return 0;
}

int32_t sxe2_filter_uinit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	sxe2_uc_filter_clear(adapter, true);
	adapter->filter_ctxt.uc_num = 0;

	sxe2_mc_filter_clear(adapter, true);
	adapter->filter_ctxt.mc_num = 0;

	sxe2_vlan_filters_clear(adapter, true);
	adapter->filter_ctxt.vlan_num = 0;
	return 0;
}
