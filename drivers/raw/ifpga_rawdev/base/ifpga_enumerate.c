/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "opae_hw_api.h"
#include "ifpga_api.h"

#include "ifpga_hw.h"
#include "ifpga_enumerate.h"
#include "ifpga_feature_dev.h"

struct build_feature_devs_info {
	struct opae_adapter_data_pci *pci_data;

	struct ifpga_afu_info *acc_info;

	void *fiu;
	enum fpga_id_type current_type;
	int current_port_id;

	void *ioaddr;
	void *ioend;
	uint64_t phys_addr;
	int current_bar;

	void *pfme_hdr;

	struct ifpga_hw *hw;
};

struct feature_info {
	const char *name;
	u32 resource_size;
	int feature_index;
	int revision_id;
	unsigned int vec_start;
	unsigned int vec_cnt;

	struct feature_ops *ops;
};

/* indexed by fme feature IDs which are defined in 'enum fme_feature_id'. */
static struct feature_info fme_features[] = {
	{
		.name = FME_FEATURE_HEADER,
		.resource_size = sizeof(struct feature_fme_header),
		.feature_index = FME_FEATURE_ID_HEADER,
		.revision_id = FME_HEADER_REVISION,
		.ops = &fme_hdr_ops,
	},
	{
		.name = FME_FEATURE_THERMAL_MGMT,
		.resource_size = sizeof(struct feature_fme_thermal),
		.feature_index = FME_FEATURE_ID_THERMAL_MGMT,
		.revision_id = FME_THERMAL_MGMT_REVISION,
		.ops = &fme_thermal_mgmt_ops,
	},
	{
		.name = FME_FEATURE_POWER_MGMT,
		.resource_size = sizeof(struct feature_fme_power),
		.feature_index = FME_FEATURE_ID_POWER_MGMT,
		.revision_id = FME_POWER_MGMT_REVISION,
		.ops = &fme_power_mgmt_ops,
	},
	{
		.name = FME_FEATURE_GLOBAL_IPERF,
		.resource_size = sizeof(struct feature_fme_iperf),
		.feature_index = FME_FEATURE_ID_GLOBAL_IPERF,
		.revision_id = FME_GLOBAL_IPERF_REVISION,
		.ops = &fme_global_iperf_ops,
	},
	{
		.name = FME_FEATURE_GLOBAL_ERR,
		.resource_size = sizeof(struct feature_fme_err),
		.feature_index = FME_FEATURE_ID_GLOBAL_ERR,
		.revision_id = FME_GLOBAL_ERR_REVISION,
		.ops = &fme_global_err_ops,
	},
	{
		.name = FME_FEATURE_PR_MGMT,
		.resource_size = sizeof(struct feature_fme_pr),
		.feature_index = FME_FEATURE_ID_PR_MGMT,
		.revision_id = FME_PR_MGMT_REVISION,
		.ops = &fme_pr_mgmt_ops,
	},
	{
		.name = FME_FEATURE_HSSI_ETH,
		.resource_size = sizeof(struct feature_fme_hssi),
		.feature_index = FME_FEATURE_ID_HSSI_ETH,
		.revision_id = FME_HSSI_ETH_REVISION
	},
	{
		.name = FME_FEATURE_GLOBAL_DPERF,
		.resource_size = sizeof(struct feature_fme_dperf),
		.feature_index = FME_FEATURE_ID_GLOBAL_DPERF,
		.revision_id = FME_GLOBAL_DPERF_REVISION,
		.ops = &fme_global_dperf_ops,
	}
};

static struct feature_info port_features[] = {
	{
		.name = PORT_FEATURE_HEADER,
		.resource_size = sizeof(struct feature_port_header),
		.feature_index = PORT_FEATURE_ID_HEADER,
		.revision_id = PORT_HEADER_REVISION,
		.ops = &port_hdr_ops,
	},
	{
		.name = PORT_FEATURE_ERR,
		.resource_size = sizeof(struct feature_port_error),
		.feature_index = PORT_FEATURE_ID_ERROR,
		.revision_id = PORT_ERR_REVISION,
		.ops = &port_error_ops,
	},
	{
		.name = PORT_FEATURE_UMSG,
		.resource_size = sizeof(struct feature_port_umsg),
		.feature_index = PORT_FEATURE_ID_UMSG,
		.revision_id = PORT_UMSG_REVISION,
	},
	{
		.name = PORT_FEATURE_UINT,
		.resource_size = sizeof(struct feature_port_uint),
		.feature_index = PORT_FEATURE_ID_UINT,
		.revision_id = PORT_UINT_REVISION,
		.ops = &port_uint_ops,
	},
	{
		.name = PORT_FEATURE_STP,
		.resource_size = PORT_FEATURE_STP_REGION_SIZE,
		.feature_index = PORT_FEATURE_ID_STP,
		.revision_id = PORT_STP_REVISION,
		.ops = &port_stp_ops,
	},
	{
		.name = PORT_FEATURE_UAFU,
		/* UAFU feature size should be read from PORT_CAP.MMIOSIZE.
		 * Will set uafu feature size while parse port device.
		 */
		.resource_size = 0,
		.feature_index = PORT_FEATURE_ID_UAFU,
		.revision_id = PORT_UAFU_REVISION
	},
};

static u64 feature_id(void __iomem *start)
{
	struct feature_header header;

	header.csr = readq(start);

	switch (header.type) {
	case FEATURE_TYPE_FIU:
		return FEATURE_ID_HEADER;
	case FEATURE_TYPE_PRIVATE:
		return header.id;
	case FEATURE_TYPE_AFU:
		return FEATURE_ID_AFU;
	}

	WARN_ON(1);
	return 0;
}

static int
build_info_add_sub_feature(struct build_feature_devs_info *binfo,
			   struct feature_info *finfo, void __iomem *start)
{
	struct ifpga_hw *hw = binfo->hw;
	struct feature *feature = NULL;
	int feature_idx = finfo->feature_index;
	unsigned int vec_start = finfo->vec_start;
	unsigned int vec_cnt = finfo->vec_cnt;
	struct feature_irq_ctx *ctx = NULL;
	int port_id, ret = 0;
	unsigned int i;

	if (binfo->current_type == FME_ID) {
		feature = &hw->fme.sub_feature[feature_idx];
		feature->parent = &hw->fme;
	} else if (binfo->current_type == PORT_ID) {
		port_id = binfo->current_port_id;
		feature = &hw->port[port_id].sub_feature[feature_idx];
		feature->parent = &hw->port[port_id];
	} else {
		return -EFAULT;
	}

	feature->state = IFPGA_FEATURE_ATTACHED;
	feature->addr = start;
	feature->id = feature_id(start);
	feature->size = finfo->resource_size;
	feature->name = finfo->name;
	feature->revision = finfo->revision_id;
	feature->ops = finfo->ops;
	feature->phys_addr = binfo->phys_addr +
				((u8 *)start - (u8 *)binfo->ioaddr);

	if (vec_cnt) {
		if (vec_start + vec_cnt <= vec_start)
			return -EINVAL;

		ctx = zmalloc(sizeof(*ctx) * vec_cnt);
		if (!ctx)
			return -ENOMEM;

		for (i = 0; i < vec_cnt; i++) {
			ctx[i].eventfd = -1;
			ctx[i].idx = vec_start + i;
		}
	}

	feature->ctx = ctx;
	feature->ctx_num = vec_cnt;
	feature->vfio_dev_fd = binfo->pci_data->vfio_dev_fd;

	return ret;
}

static int
create_feature_instance(struct build_feature_devs_info *binfo,
			void __iomem *start, struct feature_info *finfo)
{
	struct feature_header *hdr = start;

	if (finfo->revision_id != SKIP_REVISION_CHECK &&
	    hdr->revision > finfo->revision_id) {
		dev_err(binfo, "feature %s revision :default:%x, now at:%x, mis-match.\n",
			finfo->name, finfo->revision_id, hdr->revision);
	}

	return build_info_add_sub_feature(binfo, finfo, start);
}

/*
 * UAFU GUID is dynamic as it can be changed after FME downloads different
 * Green Bitstream to the port, so we treat the unknown GUIDs which are
 * attached on port's feature list as UAFU.
 */
static bool feature_is_UAFU(struct build_feature_devs_info *binfo)
{
	if (binfo->current_type != PORT_ID)
		return false;

	return true;
}

static int parse_feature_port_uafu(struct build_feature_devs_info *binfo,
				   struct feature_header *hdr)
{
	enum port_feature_id id = PORT_FEATURE_ID_UAFU;
	struct ifpga_afu_info *info;
	void *start = (void *)hdr;
	int ret;

	if (port_features[id].resource_size) {
		ret = create_feature_instance(binfo, hdr, &port_features[id]);
	} else {
		dev_err(binfo, "the uafu feature header is mis-configured.\n");
		ret = -EINVAL;
	}

	if (ret)
		return ret;

	/* FIXME: need to figure out a better name */
	info = malloc(sizeof(*info));
	if (!info)
		return -ENOMEM;

	info->region[0].addr = start;
	info->region[0].phys_addr = binfo->phys_addr +
			(uint8_t *)start - (uint8_t *)binfo->ioaddr;
	info->region[0].len = port_features[id].resource_size;
	port_features[id].resource_size = 0;
	info->num_regions = 1;

	binfo->acc_info = info;

	return ret;
}

static int parse_feature_afus(struct build_feature_devs_info *binfo,
			      struct feature_header *hdr)
{
	int ret;
	struct feature_afu_header *afu_hdr, header;
	u8 __iomem *start;
	u8 __iomem *end = binfo->ioend;

	start = (u8 __iomem *)hdr;
	for (; start < end; start += header.next_afu) {
		if ((unsigned int)(end - start) <
			(unsigned int)(sizeof(*afu_hdr) + sizeof(*hdr)))
			return -EINVAL;

		hdr = (struct feature_header *)start;
		afu_hdr = (struct feature_afu_header *)(hdr + 1);
		header.csr = readq(&afu_hdr->csr);

		if (feature_is_UAFU(binfo)) {
			ret = parse_feature_port_uafu(binfo, hdr);
			if (ret)
				return ret;
		}

		if (!header.next_afu)
			break;
	}

	return 0;
}

/* create and register proper private data */
static int build_info_commit_dev(struct build_feature_devs_info *binfo)
{
	struct ifpga_afu_info *info = binfo->acc_info;
	struct ifpga_hw *hw = binfo->hw;
	struct opae_manager *mgr;
	struct opae_bridge *br;
	struct opae_accelerator *acc;

	if (!binfo->fiu)
		return 0;

	if (binfo->current_type == PORT_ID) {
		/* return error if no valid acc info data structure */
		if (!info)
			return -EFAULT;

		br = opae_bridge_alloc(hw->adapter->name, &ifpga_br_ops,
				       binfo->fiu);
		if (!br)
			return -ENOMEM;

		br->id = binfo->current_port_id;

		/* update irq info */
		info->num_irqs = port_features[PORT_FEATURE_ID_UINT].vec_cnt;

		acc = opae_accelerator_alloc(hw->adapter->name,
					     &ifpga_acc_ops, info);
		if (!acc) {
			opae_bridge_free(br);
			return -ENOMEM;
		}

		acc->br = br;
		acc->index = br->id;

		opae_adapter_add_acc(hw->adapter, acc);

	} else if (binfo->current_type == FME_ID) {
		mgr = opae_manager_alloc(hw->adapter->name, &ifpga_mgr_ops,
					 binfo->fiu);
		if (!mgr)
			return -ENOMEM;

		mgr->adapter = hw->adapter;
		hw->adapter->mgr = mgr;
	}

	binfo->fiu = NULL;

	return 0;
}

static int
build_info_create_dev(struct build_feature_devs_info *binfo,
		      enum fpga_id_type type, unsigned int index)
{
	int ret;

	ret = build_info_commit_dev(binfo);
	if (ret)
		return ret;

	binfo->current_type = type;

	if (type == FME_ID) {
		binfo->fiu = &binfo->hw->fme;
	} else if (type == PORT_ID) {
		binfo->fiu = &binfo->hw->port[index];
		binfo->current_port_id = index;
	}

	return 0;
}

static int parse_feature_fme(struct build_feature_devs_info *binfo,
			     struct feature_header *start)
{
	struct ifpga_hw *hw = binfo->hw;
	struct ifpga_fme_hw *fme = &hw->fme;
	int ret;

	ret = build_info_create_dev(binfo, FME_ID, 0);
	if (ret)
		return ret;

	/* Update FME states */
	fme->state = IFPGA_FME_IMPLEMENTED;
	fme->parent = hw;
	spinlock_init(&fme->lock);

	return create_feature_instance(binfo, start,
				       &fme_features[FME_FEATURE_ID_HEADER]);
}

static int parse_feature_port(struct build_feature_devs_info *binfo,
			      void __iomem *start)
{
	struct feature_port_header *port_hdr;
	struct feature_port_capability capability;
	struct ifpga_hw *hw = binfo->hw;
	struct ifpga_port_hw *port;
	unsigned int port_id;
	int ret;

	/* Get current port's id */
	port_hdr = (struct feature_port_header *)start;
	capability.csr = readq(&port_hdr->capability);
	port_id = capability.port_number;

	ret = build_info_create_dev(binfo, PORT_ID, port_id);
	if (ret)
		return ret;

	/*found a Port device*/
	port = &hw->port[port_id];
	port->port_id = binfo->current_port_id;
	port->parent = hw;
	port->state = IFPGA_PORT_ATTACHED;
	spinlock_init(&port->lock);

	return create_feature_instance(binfo, start,
				      &port_features[PORT_FEATURE_ID_HEADER]);
}

static void enable_port_uafu(struct build_feature_devs_info *binfo,
			     void __iomem *start)
{
	enum port_feature_id id = PORT_FEATURE_ID_UAFU;
	struct feature_port_header *port_hdr;
	struct feature_port_capability capability;
	struct ifpga_port_hw *port = &binfo->hw->port[binfo->current_port_id];

	port_hdr = (struct feature_port_header *)start;
	capability.csr = readq(&port_hdr->capability);
	port_features[id].resource_size = (capability.mmio_size << 10);

	/*
	 * From spec, to Enable UAFU, we should reset related port,
	 * or the whole mmio space in this UAFU will be invalid
	 */
	if (port_features[id].resource_size)
		fpga_port_reset(port);
}

static int parse_feature_fiu(struct build_feature_devs_info *binfo,
			     struct feature_header *hdr)
{
	struct feature_header header;
	struct feature_fiu_header *fiu_hdr, fiu_header;
	u8 __iomem *start = (u8 __iomem *)hdr;
	int ret;

	header.csr = readq(hdr);

	switch (header.id) {
	case FEATURE_FIU_ID_FME:
		ret = parse_feature_fme(binfo, hdr);
		binfo->pfme_hdr = hdr;
		if (ret)
			return ret;
		break;
	case FEATURE_FIU_ID_PORT:
		ret = parse_feature_port(binfo, hdr);
		enable_port_uafu(binfo, hdr);
		if (ret)
			return ret;

		/* Check Port FIU's next_afu pointer to User AFU DFH */
		fiu_hdr = (struct feature_fiu_header *)(hdr + 1);
		fiu_header.csr = readq(&fiu_hdr->csr);

		if (fiu_header.next_afu) {
			start += fiu_header.next_afu;
			ret = parse_feature_afus(binfo,
						(struct feature_header *)start);
			if (ret)
				return ret;
		} else {
			dev_info(binfo, "No AFUs detected on Port\n");
		}

		break;
	default:
		dev_info(binfo, "FIU TYPE %d is not supported yet.\n",
			 header.id);
	}

	return 0;
}

static void parse_feature_irqs(struct build_feature_devs_info *binfo,
			       void __iomem *start, struct feature_info *finfo)
{
	finfo->vec_start = 0;
	finfo->vec_cnt = 0;

	UNUSED(binfo);

	if (!strcmp(finfo->name, PORT_FEATURE_UINT)) {
		struct feature_port_uint *port_uint = start;
		struct feature_port_uint_cap uint_cap;

		uint_cap.csr = readq(&port_uint->capability);
		if (uint_cap.intr_num) {
			finfo->vec_start = uint_cap.first_vec_num;
			finfo->vec_cnt = uint_cap.intr_num;
		} else {
			dev_debug(binfo, "UAFU doesn't support interrupt\n");
		}
	} else if (!strcmp(finfo->name, PORT_FEATURE_ERR)) {
		struct feature_port_error *port_err = start;
		struct feature_port_err_capability port_err_cap;

		port_err_cap.csr = readq(&port_err->error_capability);
		if (port_err_cap.support_intr) {
			finfo->vec_start = port_err_cap.intr_vector_num;
			finfo->vec_cnt = 1;
		} else {
			dev_debug(&binfo, "Port error doesn't support interrupt\n");
		}

	} else if (!strcmp(finfo->name, FME_FEATURE_GLOBAL_ERR)) {
		struct feature_fme_err *fme_err = start;
		struct feature_fme_error_capability fme_err_cap;

		fme_err_cap.csr = readq(&fme_err->fme_err_capability);
		if (fme_err_cap.support_intr) {
			finfo->vec_start = fme_err_cap.intr_vector_num;
			finfo->vec_cnt = 1;
		} else {
			dev_debug(&binfo, "FME error doesn't support interrupt\n");
		}
	}
}

static int parse_feature_fme_private(struct build_feature_devs_info *binfo,
				     struct feature_header *hdr)
{
	struct feature_header header;

	header.csr = readq(hdr);

	if (header.id >= ARRAY_SIZE(fme_features)) {
		dev_err(binfo, "FME feature id %x is not supported yet.\n",
			header.id);
		return 0;
	}

	parse_feature_irqs(binfo, hdr, &fme_features[header.id]);

	return create_feature_instance(binfo, hdr, &fme_features[header.id]);
}

static int parse_feature_port_private(struct build_feature_devs_info *binfo,
				      struct feature_header *hdr)
{
	struct feature_header header;
	enum port_feature_id id;

	header.csr = readq(hdr);
	/*
	 * the region of port feature id is [0x10, 0x13], + 1 to reserve 0
	 * which is dedicated for port-hdr.
	 */
	id = (header.id & 0x000f) + 1;

	if (id >= ARRAY_SIZE(port_features)) {
		dev_err(binfo, "Port feature id %x is not supported yet.\n",
			header.id);
		return 0;
	}

	parse_feature_irqs(binfo, hdr, &port_features[id]);

	return create_feature_instance(binfo, hdr, &port_features[id]);
}

static int parse_feature_private(struct build_feature_devs_info *binfo,
				 struct feature_header *hdr)
{
	struct feature_header header;

	header.csr = readq(hdr);

	switch (binfo->current_type) {
	case FME_ID:
		return parse_feature_fme_private(binfo, hdr);
	case PORT_ID:
		return parse_feature_port_private(binfo, hdr);
	default:
		dev_err(binfo, "private feature %x belonging to AFU %d (unknown_type) is not supported yet.\n",
			header.id, binfo->current_type);
	}
	return 0;
}

static int parse_feature(struct build_feature_devs_info *binfo,
			 struct feature_header *hdr)
{
	struct feature_header header;
	int ret = 0;

	header.csr = readq(hdr);

	switch (header.type) {
	case FEATURE_TYPE_AFU:
		ret = parse_feature_afus(binfo, hdr);
		break;
	case FEATURE_TYPE_PRIVATE:
		ret = parse_feature_private(binfo, hdr);
		break;
	case FEATURE_TYPE_FIU:
		ret = parse_feature_fiu(binfo, hdr);
		break;
	default:
		dev_err(binfo, "Feature Type %x is not supported.\n",
			hdr->type);
	};

	return ret;
}

static int
parse_feature_list(struct build_feature_devs_info *binfo, u8 __iomem *start)
{
	struct feature_header *hdr, header;
	u8 __iomem *end = (u8 __iomem *)binfo->ioend;
	int ret = 0;

	for (; start < end; start += header.next_header_offset) {
		if ((unsigned int)(end - start) < (unsigned int)sizeof(*hdr)) {
			dev_err(binfo, "The region is too small to contain a feature.\n");
			ret =  -EINVAL;
			break;
		}

		hdr = (struct feature_header *)start;
		ret = parse_feature(binfo, hdr);
		if (ret)
			return ret;

		header.csr = readq(hdr);
		if (!header.next_header_offset)
			break;
	}

	return build_info_commit_dev(binfo);
}

/* switch the memory mapping to BAR# @bar */
static int parse_switch_to(struct build_feature_devs_info *binfo, int bar)
{
	struct opae_adapter_data_pci *pci_data = binfo->pci_data;

	if (!pci_data->region[bar].addr)
		return -ENOMEM;

	binfo->ioaddr = pci_data->region[bar].addr;
	binfo->ioend = (u8 __iomem *)binfo->ioaddr + pci_data->region[bar].len;
	binfo->phys_addr = pci_data->region[bar].phys_addr;
	binfo->current_bar = bar;

	return 0;
}

static int parse_ports_from_fme(struct build_feature_devs_info *binfo)
{
	struct feature_fme_header *fme_hdr;
	struct feature_fme_port port;
	int i = 0, ret = 0;

	if (!binfo->pfme_hdr) {
		dev_info(binfo,  "VF is detected.\n");
		return ret;
	}

	fme_hdr = binfo->pfme_hdr;

	do {
		port.csr = readq(&fme_hdr->port[i]);
		if (!port.port_implemented)
			break;

		/* skip port which only could be accessed via VF */
		if (port.afu_access_control == FME_AFU_ACCESS_VF)
			continue;

		ret = parse_switch_to(binfo, port.port_bar);
		if (ret)
			break;

		ret = parse_feature_list(binfo,
					 (u8 __iomem *)binfo->ioaddr +
					  port.port_offset);
		if (ret)
			break;
	} while (++i < MAX_FPGA_PORT_NUM);

	return ret;
}

static struct build_feature_devs_info *
build_info_alloc_and_init(struct ifpga_hw *hw)
{
	struct build_feature_devs_info *binfo;

	binfo = zmalloc(sizeof(*binfo));
	if (!binfo)
		return binfo;

	binfo->hw = hw;
	binfo->pci_data = hw->pci_data;

	/* fpga feature list starts from BAR 0 */
	if (parse_switch_to(binfo, 0)) {
		free(binfo);
		return NULL;
	}

	return binfo;
}

static void build_info_free(struct build_feature_devs_info *binfo)
{
	free(binfo);
}

static void ifpga_print_device_feature_list(struct ifpga_hw *hw)
{
	struct ifpga_fme_hw *fme = &hw->fme;
	struct ifpga_port_hw *port;
	struct feature *feature;
	int i, j;

	dev_info(hw, "found fme_device, is in PF: %s\n",
		 is_ifpga_hw_pf(hw) ? "yes" : "no");

	for (i = 0; i < FME_FEATURE_ID_MAX; i++) {
		feature = &fme->sub_feature[i];
		if (feature->state != IFPGA_FEATURE_ATTACHED)
			continue;

		dev_info(hw, "%12s:	0x%p - 0x%p  - paddr: 0x%lx\n",
			 feature->name, feature->addr,
			 feature->addr + feature->size - 1,
			 (unsigned long)feature->phys_addr);
	}

	for (i = 0; i < MAX_FPGA_PORT_NUM; i++) {
		port = &hw->port[i];

		if (port->state != IFPGA_PORT_ATTACHED)
			continue;

		dev_info(hw, "port device: %d\n", port->port_id);

		for (j = 0; j < PORT_FEATURE_ID_MAX; j++) {
			feature = &port->sub_feature[j];
			if (feature->state != IFPGA_FEATURE_ATTACHED)
				continue;

			dev_info(hw, "%12s:	0x%p - 0x%p  - paddr:0x%lx\n",
				 feature->name,
				 feature->addr,
				 feature->addr +
				 feature->size - 1,
				 (unsigned long)feature->phys_addr);
		}
	}
}

int ifpga_bus_enumerate(struct ifpga_hw *hw)
{
	struct build_feature_devs_info *binfo;
	int ret;

	binfo = build_info_alloc_and_init(hw);
	if (!binfo)
		return -ENOMEM;

	ret = parse_feature_list(binfo, binfo->ioaddr);
	if (ret)
		goto exit;

	ret = parse_ports_from_fme(binfo);
	if (ret)
		goto exit;

	ifpga_print_device_feature_list(hw);

exit:
	build_info_free(binfo);
	return ret;
}

int ifpga_bus_init(struct ifpga_hw *hw)
{
	int i;

	fme_hw_init(&hw->fme);
	for (i = 0; i < MAX_FPGA_PORT_NUM; i++)
		port_hw_init(&hw->port[i]);

	return 0;
}
