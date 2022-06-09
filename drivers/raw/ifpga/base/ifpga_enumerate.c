/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include "opae_hw_api.h"
#include "ifpga_api.h"

#include "ifpga_hw.h"
#include "ifpga_enumerate.h"
#include "ifpga_feature_dev.h"

struct dfl_fpga_enum_dfl {
	u64 start;
	u64 len;
	void *addr;
	TAILQ_ENTRY(dfl_fpga_enum_dfl) node;
};

TAILQ_HEAD(dfl_fpga_enum_dfls, dfl_fpga_enum_dfl);
struct dfl_fpga_enum_info {
	struct ifpga_hw *hw;
	struct dfl_fpga_enum_dfls dfls;
};

struct build_feature_devs_info {
	struct opae_adapter_data_pci *pci_data;

	struct ifpga_afu_info *acc_info;

	void *fiu;
	enum fpga_id_type current_type;
	int current_port_id;

	void *ioaddr;
	void *ioend;
	uint64_t phys_addr;

	void *pfme_hdr;

	struct ifpga_hw *hw;
};

static int feature_revision(void __iomem *start)
{
	struct feature_header header;

	header.csr = readq(start);

	return header.revision;
}

static u32 feature_size(void __iomem *start)
{
	struct feature_header header;

	header.csr = readq(start);

	/*the size of private feature is 4KB aligned*/
	return header.next_header_offset ? header.next_header_offset:4096;
}

static u64 feature_id(void __iomem *start)
{
	struct feature_header header;

	header.csr = readq(start);

	switch (header.type) {
	case FEATURE_TYPE_FIU:
		return FEATURE_ID_FIU_HEADER;
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
		void __iomem *start, u64 fid, unsigned int size,
		unsigned int vec_start,
		unsigned int vec_cnt)
{
	struct ifpga_hw *hw = binfo->hw;
	struct ifpga_feature *feature = NULL;
	struct feature_irq_ctx *ctx = NULL;
	int port_id, ret = 0;
	unsigned int i;

	fid = fid?fid:feature_id(start);
	size = size?size:feature_size(start);

	feature = opae_malloc(sizeof(struct ifpga_feature));
	if (!feature)
		return -ENOMEM;

	feature->state = IFPGA_FEATURE_ATTACHED;
	feature->addr = start;
	feature->id = fid;
	feature->size = size;
	feature->revision = feature_revision(start);
	feature->phys_addr = binfo->phys_addr +
				((u8 *)start - (u8 *)binfo->ioaddr);
	feature->vec_start = vec_start;
	feature->vec_cnt = vec_cnt;

	dev_debug(binfo, "%s: id=0x%llx, phys_addr=0x%llx, size=%u\n",
			__func__, (unsigned long long)feature->id,
			(unsigned long long)feature->phys_addr, size);

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

	if (binfo->current_type == FME_ID) {
		feature->parent = &hw->fme;
		feature->type = FEATURE_FME_TYPE;
		feature->name = get_fme_feature_name(fid);
		TAILQ_INSERT_TAIL(&hw->fme.feature_list, feature, next);
	} else if (binfo->current_type == PORT_ID) {
		port_id = binfo->current_port_id;
		feature->parent = &hw->port[port_id];
		feature->type = FEATURE_PORT_TYPE;
		feature->name = get_port_feature_name(fid);
		TAILQ_INSERT_TAIL(&hw->port[port_id].feature_list,
				feature, next);
	} else {
		return -EFAULT;
	}
	return ret;
}

static int
create_feature_instance(struct build_feature_devs_info *binfo,
			void __iomem *start, u64 fid,
			unsigned int size, unsigned int vec_start,
			unsigned int vec_cnt)
{
	if (binfo->current_type != AFU_ID)
		return build_info_add_sub_feature(binfo, start, fid, size,
			vec_start, vec_cnt);
	return 0;
}

/*
 * UAFU GUID is dynamic as it can be changed after FME downloads different
 * Green Bitstream to the port, so we treat the unknown GUIDs which are
 * attached on port's feature list as UAFU.
 */
static bool feature_is_UAFU(struct build_feature_devs_info *binfo)
{
	if ((binfo->current_type == PORT_ID) ||
		(binfo->current_type == AFU_ID))
		return true;

	return false;
}

static int parse_feature_uafu(struct build_feature_devs_info *binfo,
				   struct feature_header *hdr)
{
	u64 id = PORT_FEATURE_ID_UAFU;
	struct ifpga_afu_info *info;
	void *start = (void *)hdr;
	struct feature_port_header *port_hdr = binfo->ioaddr;
	struct feature_port_capability capability;
	int ret;
	int size;

	if (binfo->acc_info) {
		dev_info(binfo, "Sub AFU found @ %p.\n", start);
		return 0;
	}

	capability.csr = readq(&port_hdr->capability);

	if (binfo->current_type == AFU_ID) {
		size = AFU_REGION_SIZE;
	} else {
		capability.csr = readq(&port_hdr->capability);
		size = capability.mmio_size << 10;
	}

	ret = create_feature_instance(binfo, hdr, id, size, 0, 0);
	if (ret)
		return ret;

	info = opae_malloc(sizeof(*info));
	if (!info)
		return -ENOMEM;

	info->region[0].addr = start;
	info->region[0].phys_addr = binfo->phys_addr +
			(uint8_t *)start - (uint8_t *)binfo->ioaddr;
	info->region[0].len = size;
	info->num_regions = AFU_MAX_REGION;

	binfo->acc_info = info;

	return ret;
}

/* create and register proper private data */
static int build_info_commit_dev(struct build_feature_devs_info *binfo)
{
	struct ifpga_afu_info *info = binfo->acc_info;
	struct ifpga_hw *hw = binfo->hw;
	struct opae_manager *mgr;
	struct opae_bridge *br;
	struct opae_accelerator *acc;
	struct ifpga_port_hw *port;
	struct ifpga_fme_hw *fme;
	struct ifpga_feature *feature;

	if (binfo->current_type == PORT_ID) {
		if (!binfo->fiu)
			return 0;

		br = opae_bridge_alloc(hw->adapter->name, &ifpga_br_ops,
				       binfo->fiu);
		if (!br)
			return -ENOMEM;

		br->id = binfo->current_port_id;

		/* update irq info */
		port = &hw->port[binfo->current_port_id];
		feature = get_feature_by_id(&port->feature_list,
				PORT_FEATURE_ID_UINT);
		if (feature && info)
			info->num_irqs = feature->vec_cnt;

		acc = opae_accelerator_alloc(hw->adapter->name,
					     &ifpga_acc_ops, info);
		if (!acc) {
			opae_bridge_free(br);
			return -ENOMEM;
		}

		acc->adapter = hw->adapter;
		acc->br = br;
		if (hw->adapter->mgr)
			acc->mgr = hw->adapter->mgr;
		acc->index = br->id;

		fme = &hw->fme;
		fme->nums_acc_region = info ? info->num_regions : 0;

		opae_adapter_add_acc(hw->adapter, acc);

	} else if (binfo->current_type == FME_ID) {
		if (!binfo->fiu)
			return 0;

		mgr = opae_manager_alloc(hw->adapter->name, &ifpga_mgr_ops,
				&ifpga_mgr_network_ops, binfo->fiu);
		if (!mgr)
			return -ENOMEM;

		mgr->adapter = hw->adapter;
		hw->adapter->mgr = mgr;
	} else if (binfo->current_type == AFU_ID) {
		if (!info)
			return -EFAULT;

		info->num_irqs = 0;
		acc = opae_accelerator_alloc(hw->adapter->name,
					&ifpga_acc_ops, info);
		if (!acc)
			return -ENOMEM;

		acc->adapter = hw->adapter;
		acc->br = NULL;
		acc->mgr = NULL;
		acc->index = hw->num_afus++;

		opae_adapter_add_acc(hw->adapter, acc);
	}

	binfo->fiu = NULL;

	return 0;
}

static int
build_info_create_dev(struct build_feature_devs_info *binfo,
		      enum fpga_id_type type, unsigned int index)
{
	int ret;

	if ((type == AFU_ID) && (binfo->current_type == PORT_ID))
		return 0;

	ret = build_info_commit_dev(binfo);
	if (ret)
		return ret;

	binfo->current_type = type;
	binfo->acc_info = NULL;

	if (type == FME_ID) {
		binfo->fiu = &binfo->hw->fme;
	} else if (type == PORT_ID) {
		binfo->fiu = &binfo->hw->port[index];
		binfo->current_port_id = index;
	}

	return 0;
}

static int parse_feature_afus(struct build_feature_devs_info *binfo,
			      struct feature_header *hdr)
{
	int ret;
	struct feature_afu_header *afu_hdr, header;
	u8 __iomem *start;
	u8 __iomem *end = binfo->ioend;

	ret = build_info_create_dev(binfo, AFU_ID, 0);
	if (ret)
		return ret;

	start = (u8 __iomem *)hdr;
	for (; start < end; start += header.next_afu) {
		if ((unsigned int)(end - start) <
			(unsigned int)(sizeof(*afu_hdr) + sizeof(*hdr)))
			return -EINVAL;

		hdr = (struct feature_header *)start;
		afu_hdr = (struct feature_afu_header *)(hdr + 1);
		header.csr = readq(&afu_hdr->csr);

		if (feature_is_UAFU(binfo)) {
			ret = parse_feature_uafu(binfo, hdr);
			if (ret)
				return ret;
		}

		if (!header.next_afu)
			break;
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
	TAILQ_INIT(&fme->feature_list);
	spinlock_init(&fme->lock);

	return create_feature_instance(binfo, start, 0, 0, 0, 0);
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
	TAILQ_INIT(&port->feature_list);

	return create_feature_instance(binfo, start, 0, 0, 0, 0);
}

static void enable_port_uafu(struct build_feature_devs_info *binfo,
			     void __iomem *start)
{
	struct ifpga_port_hw *port = &binfo->hw->port[binfo->current_port_id];

	UNUSED(start);

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
			dev_info(binfo, "No AFU detected on Port\n");
		}

		break;
	default:
		dev_info(binfo, "FIU TYPE %d is not supported yet.\n",
			 header.id);
	}

	return 0;
}

static void parse_feature_irqs(struct build_feature_devs_info *binfo,
		void __iomem *start, unsigned int *vec_start,
		unsigned int *vec_cnt)
{
	UNUSED(binfo);
	u64 id;

	id = feature_id(start);

	if ((binfo->current_type == PORT_ID) && (id == PORT_FEATURE_ID_UINT)) {
		struct feature_port_uint *port_uint = start;
		struct feature_port_uint_cap uint_cap;

		uint_cap.csr = readq(&port_uint->capability);
		if (uint_cap.intr_num) {
			*vec_start = uint_cap.first_vec_num;
			*vec_cnt = uint_cap.intr_num;
		} else {
			dev_debug(binfo, "UAFU doesn't support interrupt\n");
		}
	} else if ((binfo->current_type == PORT_ID) &&
			(id == PORT_FEATURE_ID_ERROR)) {
		struct feature_port_error *port_err = start;
		struct feature_port_err_capability port_err_cap;

		port_err_cap.csr = readq(&port_err->error_capability);
		if (port_err_cap.support_intr) {
			*vec_start = port_err_cap.intr_vector_num;
			*vec_cnt = 1;
		} else {
			dev_debug(&binfo, "Port error doesn't support interrupt\n");
		}

	} else if ((binfo->current_type == FME_ID) &&
			(id == FME_FEATURE_ID_GLOBAL_ERR)) {
		struct feature_fme_err *fme_err = start;
		struct feature_fme_error_capability fme_err_cap;

		fme_err_cap.csr = readq(&fme_err->fme_err_capability);
		if (fme_err_cap.support_intr) {
			*vec_start = fme_err_cap.intr_vector_num;
			*vec_cnt = 1;
		} else {
			dev_debug(&binfo, "FME error doesn't support interrupt\n");
		}
	}
}

static int parse_feature_fme_private(struct build_feature_devs_info *binfo,
				     struct feature_header *hdr)
{
	unsigned int vec_start = 0;
	unsigned int vec_cnt = 0;

	parse_feature_irqs(binfo, hdr, &vec_start, &vec_cnt);

	return create_feature_instance(binfo, hdr, 0, 0, vec_start, vec_cnt);
}

static int parse_feature_port_private(struct build_feature_devs_info *binfo,
				      struct feature_header *hdr)
{
	unsigned int vec_start = 0;
	unsigned int vec_cnt = 0;

	parse_feature_irqs(binfo, hdr, &vec_start, &vec_cnt);

	return create_feature_instance(binfo, hdr, 0, 0, vec_start, vec_cnt);
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
	case AFU_ID:
		dev_err(binfo, "private feature %x belonging to AFU "
			"is not supported yet.\n", header.id);
		break;
	default:
		dev_err(binfo, "private feature %x belonging to TYPE %d "
			"(unknown_type) is not supported yet.\n",
			header.id, binfo->current_type);
		break;
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

static int build_info_prepare(struct build_feature_devs_info *binfo,
	struct dfl_fpga_enum_dfl *dfl)
{
	if (!binfo || !dfl)
		return -EINVAL;

	binfo->ioaddr = dfl->addr;
	binfo->ioend = (u8 *)dfl->addr + dfl->len;
	binfo->phys_addr = dfl->start;

	return 0;
}

static int parse_feature_list(struct build_feature_devs_info *binfo,
	struct dfl_fpga_enum_dfl *dfl)
{
	u8 *start, *end;
	struct feature_header *hdr, header;
	int ret = 0;

	ret = build_info_prepare(binfo, dfl);
	if (ret)
		return ret;

	start = (u8 *)binfo->ioaddr;
	end = (u8 *)binfo->ioend;

	/* walk through the device feature list via DFH's next DFH pointer. */
	for (; start < end; start += header.next_header_offset) {
		if ((unsigned int)(end - start) < (unsigned int)sizeof(*hdr)) {
			dev_err(binfo, "The region is too small to "
				"contain a feature.\n");
			ret = -EINVAL;
			break;
		}

		hdr = (struct feature_header *)start;
		header.csr = opae_readq(hdr);

		dev_debug(binfo, "%s: address=0x%p, val=0x%"PRIx64", "
			"header.id=0x%x, header.next_offset=0x%x, "
			"header.eol=0x%x, header.type=0x%x\n",
			__func__, hdr, header.csr, header.id,
			header.next_header_offset, header.end_of_list,
			header.type);

		ret = parse_feature(binfo, hdr);
		if (ret)
			return ret;

		/* stop parsing if EOL(End of List) is set or offset is 0 */
		if (header.end_of_list || !header.next_header_offset)
			break;
	}

	return build_info_commit_dev(binfo);
}

static void build_info_free(struct build_feature_devs_info *binfo)
{
	opae_free(binfo);
}

static void ifpga_print_device_feature_list(struct ifpga_hw *hw)
{
	struct ifpga_fme_hw *fme = &hw->fme;
	struct ifpga_port_hw *port;
	struct ifpga_feature *feature;
	int i;

	if (fme->state == IFPGA_FME_UNUSED) {
		dev_info(hw, "FME is not present\n");
		return;
	}

	dev_info(hw, "found fme_device, is in PF: %s\n",
		 is_ifpga_hw_pf(hw) ? "yes" : "no");

	ifpga_for_each_fme_feature(fme, feature) {
		if (feature->state != IFPGA_FEATURE_ATTACHED)
			continue;

		dev_info(hw, "%12s:	%p - %p  - paddr: 0x%lx\n",
			 feature->name, feature->addr,
			 feature->addr + feature->size - 1,
			 (unsigned long)feature->phys_addr);

	}

	for (i = 0; i < MAX_FPGA_PORT_NUM; i++) {
		port = &hw->port[i];

		if (port->state != IFPGA_PORT_ATTACHED)
			continue;

		dev_info(hw, "port device: %d\n", port->port_id);

		ifpga_for_each_port_feature(port, feature) {
			if (feature->state != IFPGA_FEATURE_ATTACHED)
				continue;

			dev_info(hw, "%12s:	%p - %p  - paddr:0x%lx\n",
				 feature->name,
				 feature->addr,
				 feature->addr +
				 feature->size - 1,
				 (unsigned long)feature->phys_addr);
		}

	}
}

static struct dfl_fpga_enum_info *dfl_fpga_enum_info_alloc(struct ifpga_hw *hw)
{
	struct dfl_fpga_enum_info *info;

	info = opae_zmalloc(sizeof(*info));
	if (!info)
		return NULL;

	info->hw = hw;
	TAILQ_INIT(&info->dfls);

	return info;
}

static void dfl_fpga_enum_info_free(struct dfl_fpga_enum_info *info)
{
	struct dfl_fpga_enum_dfl *tmp, *dfl;

	if (!info)
		return;

	/* remove all device feature lists in the list. */
	for (dfl = TAILQ_FIRST(&info->dfls);
		dfl && (tmp = TAILQ_NEXT(dfl, node), 1);
		dfl = tmp) {
		TAILQ_REMOVE(&info->dfls, dfl, node);
		opae_free(dfl);
	}

	opae_free(info);
}

static int dfl_fpga_enum_info_add_dfl(struct dfl_fpga_enum_info *info,
	u64 start, u64 len, void *addr)
{
	struct dfl_fpga_enum_dfl *dfl;

	dfl = opae_zmalloc(sizeof(*dfl));
	if (!dfl)
		return -ENOMEM;

	dfl->start = start;
	dfl->len = len;
	dfl->addr = addr;

	TAILQ_INSERT_TAIL(&info->dfls, dfl, node);

	return 0;
}

#define PCI_CFG_SPACE_SIZE	256
#define PCI_CFG_SPACE_EXP_SIZE	4096
#define PCI_EXT_CAP_ID(header)		(header & 0x0000ffff)
#define PCI_EXT_CAP_NEXT(header)	((header >> 20) & 0xffc)

static int
pci_find_next_ecap(int fd, int start, u32 cap)
{
	u32 header;
	int ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;
	int pos = PCI_CFG_SPACE_SIZE;
	int ret;

	if (start > 0)
		pos = start;

	ret = pread(fd, &header, sizeof(header), pos);
	if (ret < 0)
		return ret;

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if ((PCI_EXT_CAP_ID(header) == cap) && (pos != start))
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;
		ret = pread(fd, &header, sizeof(header), pos);
		if (ret < 0)
			return ret;
	}

	return 0;
}

#define PCI_EXT_CAP_ID_VNDR	0x0B
#define PCI_VNDR_HEADER		4
#define PCI_VNDR_HEADER_ID(x)	((x) & 0xffff)
#define PCI_VENDOR_ID_INTEL 0x8086
#define PCI_VSEC_ID_INTEL_DFLS 0x43
#define PCI_VNDR_DFLS_CNT 0x8
#define PCI_VNDR_DFLS_RES 0xc
#define PCI_VNDR_DFLS_RES_BAR_MASK GENMASK(2, 0)
#define PCI_VNDR_DFLS_RES_OFF_MASK GENMASK(31, 3)

static int find_dfls_by_vsec(struct dfl_fpga_enum_info *info)
{
	struct ifpga_hw *hw;
	struct opae_adapter_data_pci *pci_data;
	char path[64];
	u32 bir, offset, vndr_hdr, i, dfl_cnt, dfl_res;
	int fd, ret, dfl_res_off, voff = 0;
	u64 start, len;
	void *addr;

	if (!info || !info->hw)
		return -EINVAL;
	hw = info->hw;

	if (!hw->adapter || !hw->pci_data)
		return -EINVAL;
	pci_data = hw->pci_data;

	ret = snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/config",
			hw->adapter->name);
	if ((unsigned int)ret >= sizeof(path))
		return -EINVAL;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -EIO;

	while ((voff = pci_find_next_ecap(fd, voff,
		PCI_EXT_CAP_ID_VNDR))) {
		vndr_hdr = 0;
		ret = pread(fd, &vndr_hdr, sizeof(vndr_hdr),
			voff + PCI_VNDR_HEADER);
		if (ret < 0) {
			ret = -EIO;
			goto free_handle;
		}
		if (PCI_VNDR_HEADER_ID(vndr_hdr) == PCI_VSEC_ID_INTEL_DFLS &&
			pci_data->vendor_id == PCI_VENDOR_ID_INTEL)
			break;
	}

	if (!voff) {
		dev_debug(hw, "%s no DFL VSEC found\n", __func__);
		ret = -ENODEV;
		goto free_handle;
	}

	dfl_cnt = 0;
	ret = pread(fd, &dfl_cnt, sizeof(dfl_cnt), voff + PCI_VNDR_DFLS_CNT);
	if (ret < 0) {
		ret = -EIO;
		goto free_handle;
	}

	dfl_res_off = voff + PCI_VNDR_DFLS_RES;
	if (dfl_res_off + (dfl_cnt * sizeof(u32)) > PCI_CFG_SPACE_EXP_SIZE) {
		dev_err(hw, "%s DFL VSEC too big for PCIe config space\n",
			__func__);
		ret = -EINVAL;
		goto free_handle;
	}

	for (i = 0; i < dfl_cnt; i++, dfl_res_off += sizeof(u32)) {
		dfl_res = GENMASK(31, 0);
		ret = pread(fd, &dfl_res, sizeof(dfl_res), dfl_res_off);
		bir = dfl_res & PCI_VNDR_DFLS_RES_BAR_MASK;
		if (bir >= PCI_MAX_RESOURCE) {
			dev_err(hw, "%s bad bir number %d\n",
				__func__, bir);
			ret = -EINVAL;
			goto free_handle;
		}

		len = pci_data->region[bir].len;
		offset = dfl_res & PCI_VNDR_DFLS_RES_OFF_MASK;
		if (offset >= len) {
			dev_err(hw, "%s bad offset %u >= %"PRIu64"\n",
				__func__, offset, len);
			ret = -EINVAL;
			goto free_handle;
		}

		dev_debug(hw, "%s BAR %d offset 0x%x\n", __func__, bir, offset);
		len -= offset;
		start = pci_data->region[bir].phys_addr + offset;
		addr = pci_data->region[bir].addr + offset;
		dfl_fpga_enum_info_add_dfl(info, start, len, addr);
	}

free_handle:
	close(fd);
	return ret;
}

/* default method of finding dfls starting at offset 0 of bar 0 */
static int
find_dfls_by_default(struct dfl_fpga_enum_info *info)
{
	struct ifpga_hw *hw;
	struct opae_adapter_data_pci *pci_data;
	int port_num, bar, i, ret = 0;
	u64 start, len;
	void *addr;
	u32 offset;
	struct feature_header hdr;
	struct feature_fme_capability cap;
	struct feature_fme_port port;
	struct feature_fme_header *fme_hdr;

	if (!info || !info->hw)
		return -EINVAL;
	hw = info->hw;

	if (!hw->pci_data)
		return -EINVAL;
	pci_data = hw->pci_data;

	/* start to find Device Feature List from Bar 0 */
	addr = pci_data->region[0].addr;
	if (!addr)
		return -ENOMEM;

	/*
	 * PF device has FME and Ports/AFUs, and VF device only has one
	 * Port/AFU. Check them and add related "Device Feature List" info
	 * for the next step enumeration.
	 */
	hdr.csr = opae_readq(addr);
	if ((hdr.type == FEATURE_TYPE_FIU) && (hdr.id == FEATURE_FIU_ID_FME)) {
		start = pci_data->region[0].phys_addr;
		len = pci_data->region[0].len;
		addr = pci_data->region[0].addr;

		dfl_fpga_enum_info_add_dfl(info, start, len, addr);

		/*
		 * find more Device Feature Lists (e.g. Ports) per information
		 * indicated by FME module.
		 */
		fme_hdr = (struct feature_fme_header *)addr;
		cap.csr = opae_readq(&fme_hdr->capability);
		port_num = (int)cap.num_ports;

		dev_info(hw, "port_num = %d\n", port_num);
		if (port_num > MAX_FPGA_PORT_NUM)
			port_num = MAX_FPGA_PORT_NUM;

		for (i = 0; i < port_num; i++) {
			port.csr = opae_readq(&fme_hdr->port[i]);

			/* skip ports which are not implemented. */
			if (!port.port_implemented)
				continue;

			/* skip port which only could be accessed via VF */
			if (port.afu_access_control == FME_AFU_ACCESS_VF)
				continue;

			/*
			 * add Port's Device Feature List information for next
			 * step enumeration.
			 */
			bar = (int)port.port_bar;
			offset = port.port_offset;
			if (bar == FME_PORT_OFST_BAR_SKIP) {
				continue;
			} else if (bar >= PCI_MAX_RESOURCE) {
				dev_err(hw, "bad BAR %d for port %d\n", bar, i);
				ret = -EINVAL;
				break;
			}
			dev_info(hw, "BAR %d offset %u\n", bar, offset);

			len = pci_data->region[bar].len;
			if (offset >= len) {
				dev_warn(hw, "bad port offset %u >= %pa\n",
					 offset, &len);
				continue;
			}

			len -= offset;
			start = pci_data->region[bar].phys_addr + offset;
			addr = pci_data->region[bar].addr + offset;
			dfl_fpga_enum_info_add_dfl(info, start, len, addr);
		}
	} else if ((hdr.type == FEATURE_TYPE_FIU) &&
		(hdr.id == FEATURE_FIU_ID_PORT)) {
		start = pci_data->region[0].phys_addr;
		len = pci_data->region[0].len;
		addr = pci_data->region[0].addr;

		dfl_fpga_enum_info_add_dfl(info, start, len, addr);
	} else if (hdr.type == FEATURE_TYPE_AFU) {
		start = pci_data->region[0].phys_addr;
		len = pci_data->region[0].len;
		addr = pci_data->region[0].addr;

		dfl_fpga_enum_info_add_dfl(info, start, len, addr);
	} else {
		dev_info(hw, "Unknown feature type 0x%x id 0x%x\n",
			 hdr.type, hdr.id);
		ret = -ENODEV;
	}

	return ret;
}

static int dfl_fpga_feature_devs_enumerate(struct dfl_fpga_enum_info *info)
{
	struct build_feature_devs_info *binfo;
	struct dfl_fpga_enum_dfl *dfl;
	int ret = 0;

	if (!info || !info->hw)
		return -EINVAL;

	/* create and init build info for enumeration */
	binfo = opae_zmalloc(sizeof(*binfo));
	if (!binfo)
		return -ENOMEM;

	binfo->hw = info->hw;
	binfo->pci_data = info->hw->pci_data;

	/*
	 * start enumeration for all feature devices based on Device Feature
	 * Lists.
	 */
	TAILQ_FOREACH(dfl, &info->dfls, node) {
		ret = parse_feature_list(binfo, dfl);
		if (ret)
			break;
	}

	build_info_free(binfo);

	return ret;
}

int ifpga_bus_enumerate(struct ifpga_hw *hw)
{
	struct dfl_fpga_enum_info *info;
	int ret;

	/* allocate enumeration info */
	info = dfl_fpga_enum_info_alloc(hw);
	if (!info)
		return -ENOMEM;

	ret = find_dfls_by_vsec(info);
	if (ret < 0)
		ret = find_dfls_by_default(info);

	if (ret)
		goto exit;

	/* start enumeration with prepared enumeration information */
	ret = dfl_fpga_feature_devs_enumerate(info);
	if (ret < 0) {
		dev_err(hw, "Enumeration failure\n");
		goto exit;
	}

	ifpga_print_device_feature_list(hw);

exit:
	dfl_fpga_enum_info_free(info);

	return ret;
}

static void ifpga_print_acc_list(struct opae_adapter *adapter)
{
	struct opae_accelerator *acc;
	struct ifpga_afu_info *info;
	struct uuid guid;
	char buf[48];
	int i;

	opae_adapter_for_each_acc(adapter, acc) {
		info = acc->data;
		if (!info)
			continue;
		acc->ops->get_uuid(acc, &guid);
		i = sprintf(buf, "%02x%02x%02x%02x-",
			guid.b[15], guid.b[14], guid.b[13], guid.b[12]);
		i += sprintf(buf+i, "%02x%02x-", guid.b[11], guid.b[10]);
		i += sprintf(buf+i, "%02x%02x-", guid.b[9], guid.b[8]);
		i += sprintf(buf+i, "%02x%02x-", guid.b[7], guid.b[6]);
		sprintf(buf+i, "%02x%02x%02x%02x%02x%02x",
			guid.b[5], guid.b[4], guid.b[3],
			guid.b[2], guid.b[1], guid.b[0]);
		dev_info(hw, "AFU(%s-%d)@%p: len:0x%"PRIx64", guid:%s\n",
			acc->name, acc->index, info->region[0].addr,
			info->region[0].len, buf);
	}
}

int ifpga_bus_init(struct ifpga_hw *hw)
{
	int i, ret = 0;
	struct ifpga_port_hw *port;

	ret = fme_hw_init(&hw->fme);
	if (ret)
		return ret;

	for (i = 0; i < MAX_FPGA_PORT_NUM; i++) {
		port = &hw->port[i];
		port_hw_init(port);
	}
	ifpga_print_acc_list(hw->adapter);

	return 0;
}

int ifpga_bus_uinit(struct ifpga_hw *hw)
{
	int i;
	struct ifpga_port_hw *port;

	if (hw) {
		fme_hw_uinit(&hw->fme);
		for (i = 0; i < MAX_FPGA_PORT_NUM; i++) {
			port = &hw->port[i];
			port_hw_uinit(port);
		}
	}

	return 0;
}
