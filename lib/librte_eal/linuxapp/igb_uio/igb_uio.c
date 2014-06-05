/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/msi.h>
#include <linux/version.h>

#ifdef CONFIG_XEN_DOM0
#include <xen/xen.h>
#endif
#include <rte_pci_dev_features.h>

/**
 * MSI-X related macros, copy from linux/pci_regs.h in kernel 2.6.39,
 * but none of them in kernel 2.6.35.
 */
#ifndef PCI_MSIX_ENTRY_SIZE
#define PCI_MSIX_ENTRY_SIZE             16
#define PCI_MSIX_ENTRY_LOWER_ADDR       0
#define PCI_MSIX_ENTRY_UPPER_ADDR       4
#define PCI_MSIX_ENTRY_DATA             8
#define PCI_MSIX_ENTRY_VECTOR_CTRL      12
#define PCI_MSIX_ENTRY_CTRL_MASKBIT     1
#endif

#ifdef RTE_PCI_CONFIG
#define PCI_SYS_FILE_BUF_SIZE      10
#define PCI_DEV_CAP_REG            0xA4
#define PCI_DEV_CTRL_REG           0xA8
#define PCI_DEV_CAP_EXT_TAG_MASK   0x20
#define PCI_DEV_CTRL_EXT_TAG_SHIFT 8
#define PCI_DEV_CTRL_EXT_TAG_MASK  (1 << PCI_DEV_CTRL_EXT_TAG_SHIFT)
#endif

#define IGBUIO_NUM_MSI_VECTORS 1

/**
 * A structure describing the private information for a uio device.
 */
struct rte_uio_pci_dev {
	struct uio_info info;
	struct pci_dev *pdev;
	spinlock_t lock; /* spinlock for accessing PCI config space or msix data in multi tasks/isr */
	enum rte_intr_mode mode;
	struct msix_entry \
		msix_entries[IGBUIO_NUM_MSI_VECTORS]; /* pointer to the msix vectors to be allocated later */
};

static char *intr_mode = NULL;
static enum rte_intr_mode igbuio_intr_mode_preferred = RTE_INTR_MODE_MSIX;

static inline struct rte_uio_pci_dev *
igbuio_get_uio_pci_dev(struct uio_info *info)
{
	return container_of(info, struct rte_uio_pci_dev, info);
}

/* sriov sysfs */
int local_pci_num_vf(struct pci_dev *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
	struct iov {
		int pos;
		int nres;
		u32 cap;
		u16 ctrl;
		u16 total;
		u16 initial;
		u16 nr_virtfn;
	} *iov = (struct iov*)dev->sriov;

	if (!dev->is_physfn)
		return 0;

	return iov->nr_virtfn;
#else
	return pci_num_vf(dev);
#endif
}

static ssize_t
show_max_vfs(struct device *dev, struct device_attribute *attr,
	     char *buf)
{
	return snprintf(buf, 10, "%u\n", local_pci_num_vf(
				container_of(dev, struct pci_dev, dev)));
}

static ssize_t
store_max_vfs(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	int err = 0;
	unsigned long max_vfs;
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);

	if (0 != strict_strtoul(buf, 0, &max_vfs))
		return -EINVAL;

	if (0 == max_vfs)
		pci_disable_sriov(pdev);
	else if (0 == local_pci_num_vf(pdev))
		err = pci_enable_sriov(pdev, max_vfs);
	else /* do nothing if change max_vfs number */
		err = -EINVAL;

	return err ? err : count;
}

#ifdef RTE_PCI_CONFIG
static ssize_t
show_extended_tag(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);
	uint32_t val = 0;

	pci_read_config_dword(pci_dev, PCI_DEV_CAP_REG, &val);
	if (!(val & PCI_DEV_CAP_EXT_TAG_MASK)) /* Not supported */
		return snprintf(buf, PCI_SYS_FILE_BUF_SIZE, "%s\n", "invalid");

	val = 0;
	pci_bus_read_config_dword(pci_dev->bus, pci_dev->devfn,
					PCI_DEV_CTRL_REG, &val);

	return snprintf(buf, PCI_SYS_FILE_BUF_SIZE, "%s\n",
		(val & PCI_DEV_CTRL_EXT_TAG_MASK) ? "on" : "off");
}

static ssize_t
store_extended_tag(struct device *dev,
		   struct device_attribute *attr,
		   const char *buf,
		   size_t count)
{
	struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);
	uint32_t val = 0, enable;

	if (strncmp(buf, "on", 2) == 0)
		enable = 1;
	else if (strncmp(buf, "off", 3) == 0)
		enable = 0;
	else
		return -EINVAL;

	pci_bus_read_config_dword(pci_dev->bus, pci_dev->devfn,
					PCI_DEV_CAP_REG, &val);
	if (!(val & PCI_DEV_CAP_EXT_TAG_MASK)) /* Not supported */
		return -EPERM;

	val = 0;
	pci_bus_read_config_dword(pci_dev->bus, pci_dev->devfn,
					PCI_DEV_CTRL_REG, &val);
	if (enable)
		val |= PCI_DEV_CTRL_EXT_TAG_MASK;
	else
		val &= ~PCI_DEV_CTRL_EXT_TAG_MASK;
	pci_bus_write_config_dword(pci_dev->bus, pci_dev->devfn,
					PCI_DEV_CTRL_REG, val);

	return count;
}

static ssize_t
show_max_read_request_size(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);
	int val = pcie_get_readrq(pci_dev);

	return snprintf(buf, PCI_SYS_FILE_BUF_SIZE, "%d\n", val);
}

static ssize_t
store_max_read_request_size(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);
	unsigned long size = 0;
	int ret;

	if (strict_strtoul(buf, 0, &size) != 0)
		return -EINVAL;

	ret = pcie_set_readrq(pci_dev, (int)size);
	if (ret < 0)
		return ret;

	return count;
}
#endif

static DEVICE_ATTR(max_vfs, S_IRUGO | S_IWUSR, show_max_vfs, store_max_vfs);
#ifdef RTE_PCI_CONFIG
static DEVICE_ATTR(extended_tag, S_IRUGO | S_IWUSR, show_extended_tag, \
	store_extended_tag);
static DEVICE_ATTR(max_read_request_size, S_IRUGO | S_IWUSR, \
	show_max_read_request_size, store_max_read_request_size);
#endif

static struct attribute *dev_attrs[] = {
	&dev_attr_max_vfs.attr,
#ifdef RTE_PCI_CONFIG
	&dev_attr_extended_tag.attr,
	&dev_attr_max_read_request_size.attr,
#endif
        NULL,
};

static const struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};

static inline int
pci_lock(struct pci_dev * pdev)
{
	/* Some function names changes between 3.2.0 and 3.3.0... */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
	pci_block_user_cfg_access(pdev);
	return 1;
#else
	return pci_cfg_access_trylock(pdev);
#endif
}

static inline void
pci_unlock(struct pci_dev * pdev)
{
	/* Some function names changes between 3.2.0 and 3.3.0... */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
	pci_unblock_user_cfg_access(pdev);
#else
	pci_cfg_access_unlock(pdev);
#endif
}

/**
 * It masks the msix on/off of generating MSI-X messages.
 */
static int
igbuio_msix_mask_irq(struct msi_desc *desc, int32_t state)
{
	uint32_t mask_bits = desc->masked;
	unsigned offset = desc->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;

	if (state != 0)
		mask_bits &= ~PCI_MSIX_ENTRY_CTRL_MASKBIT;
	else
		mask_bits |= PCI_MSIX_ENTRY_CTRL_MASKBIT;

	if (mask_bits != desc->masked) {
		writel(mask_bits, desc->mask_base + offset);
		readl(desc->mask_base);
		desc->masked = mask_bits;
	}

	return 0;
}

/**
 * This function sets/clears the masks for generating LSC interrupts.
 *
 * @param info
 *   The pointer to struct uio_info.
 * @param on
 *   The on/off flag of masking LSC.
 * @return
 *   -On success, zero value.
 *   -On failure, a negative value.
 */
static int
igbuio_set_interrupt_mask(struct rte_uio_pci_dev *udev, int32_t state)
{
	struct pci_dev *pdev = udev->pdev;

	if (udev->mode == RTE_INTR_MODE_MSIX) {
		struct msi_desc *desc;

		list_for_each_entry(desc, &pdev->msi_list, list) {
			igbuio_msix_mask_irq(desc, state);
		}
	} else if (udev->mode == RTE_INTR_MODE_LEGACY) {
		uint32_t status;
		uint16_t old, new;

		pci_read_config_dword(pdev, PCI_COMMAND, &status);
		old = status;
		if (state != 0)
			new = old & (~PCI_COMMAND_INTX_DISABLE);
		else
			new = old | PCI_COMMAND_INTX_DISABLE;

		if (old != new)
			pci_write_config_word(pdev, PCI_COMMAND, new);
	}

	return 0;
}

/**
 * This is the irqcontrol callback to be registered to uio_info.
 * It can be used to disable/enable interrupt from user space processes.
 *
 * @param info
 *  pointer to uio_info.
 * @param irq_state
 *  state value. 1 to enable interrupt, 0 to disable interrupt.
 *
 * @return
 *  - On success, 0.
 *  - On failure, a negative value.
 */
static int
igbuio_pci_irqcontrol(struct uio_info *info, s32 irq_state)
{
	unsigned long flags;
	struct rte_uio_pci_dev *udev = igbuio_get_uio_pci_dev(info);
	struct pci_dev *pdev = udev->pdev;

	spin_lock_irqsave(&udev->lock, flags);
	if (!pci_lock(pdev)) {
		spin_unlock_irqrestore(&udev->lock, flags);
		return -1;
	}

	igbuio_set_interrupt_mask(udev, irq_state);

	pci_unlock(pdev);
	spin_unlock_irqrestore(&udev->lock, flags);

	return 0;
}

/**
 * This is interrupt handler which will check if the interrupt is for the right device.
 * If yes, disable it here and will be enable later.
 */
static irqreturn_t
igbuio_pci_irqhandler(int irq, struct uio_info *info)
{
	irqreturn_t ret = IRQ_NONE;
	unsigned long flags;
	struct rte_uio_pci_dev *udev = igbuio_get_uio_pci_dev(info);
	struct pci_dev *pdev = udev->pdev;
	uint32_t cmd_status_dword;
	uint16_t status;

	spin_lock_irqsave(&udev->lock, flags);
	/* block userspace PCI config reads/writes */
	if (!pci_lock(pdev))
		goto spin_unlock;

	/* for legacy mode, interrupt maybe shared */
	if (udev->mode == RTE_INTR_MODE_LEGACY) {
		pci_read_config_dword(pdev, PCI_COMMAND, &cmd_status_dword);
		status = cmd_status_dword >> 16;
		/* interrupt is not ours, goes to out */
		if (!(status & PCI_STATUS_INTERRUPT))
			goto done;
	}

	igbuio_set_interrupt_mask(udev, 0);
	ret = IRQ_HANDLED;
done:
	/* unblock userspace PCI config reads/writes */
	pci_unlock(pdev);
spin_unlock:
	spin_unlock_irqrestore(&udev->lock, flags);
	printk(KERN_INFO "irq 0x%x %s\n", irq, (ret == IRQ_HANDLED) ? "handled" : "not handled");

	return ret;
}

#ifdef CONFIG_XEN_DOM0
static int
igbuio_dom0_mmap_phys(struct uio_info *info, struct vm_area_struct *vma)
{
	int idx;
	idx = (int)vma->vm_pgoff;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot.pgprot |= _PAGE_IOMAP;

	return remap_pfn_range(vma,
			vma->vm_start,
			info->mem[idx].addr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);
}

/**
 * This is uio device mmap method which will use igbuio mmap for Xen
 * Dom0 environment.
 */
static int
igbuio_dom0_pci_mmap(struct uio_info *info, struct vm_area_struct *vma)
{
	int idx;

	if (vma->vm_pgoff >= MAX_UIO_MAPS)
		return -EINVAL;
	if(info->mem[vma->vm_pgoff].size == 0)
		return  -EINVAL;

	idx = (int)vma->vm_pgoff;
	switch (info->mem[idx].memtype) {
	case UIO_MEM_PHYS:
		return igbuio_dom0_mmap_phys(info, vma);
	case UIO_MEM_LOGICAL:
	case UIO_MEM_VIRTUAL:
	default:
		return -EINVAL;
	}
}
#endif

/* Remap pci resources described by bar #pci_bar in uio resource n. */
static int
igbuio_pci_setup_iomem(struct pci_dev *dev, struct uio_info *info,
		       int n, int pci_bar, const char *name)
{
	unsigned long addr, len;
	void *internal_addr;

	if (sizeof(info->mem) / sizeof (info->mem[0]) <= n)
		return (EINVAL);

	addr = pci_resource_start(dev, pci_bar);
	len = pci_resource_len(dev, pci_bar);
	if (addr == 0 || len == 0)
		return -1;
	internal_addr = ioremap(addr, len);
	if (internal_addr == NULL)
		return -1;
	info->mem[n].name = name;
	info->mem[n].addr = addr;
	info->mem[n].internal_addr = internal_addr;
	info->mem[n].size = len;
	info->mem[n].memtype = UIO_MEM_PHYS;
	return 0;
}

/* Get pci port io resources described by bar #pci_bar in uio resource n. */
static int
igbuio_pci_setup_ioport(struct pci_dev *dev, struct uio_info *info,
		int n, int pci_bar, const char *name)
{
	unsigned long addr, len;

	if (sizeof(info->port) / sizeof (info->port[0]) <= n)
		return (EINVAL);

	addr = pci_resource_start(dev, pci_bar);
	len = pci_resource_len(dev, pci_bar);
	if (addr == 0 || len == 0)
		return (-1);

	info->port[n].name = name;
	info->port[n].start = addr;
	info->port[n].size = len;
	info->port[n].porttype = UIO_PORT_X86;

	return (0);
}

/* Unmap previously ioremap'd resources */
static void
igbuio_pci_release_iomem(struct uio_info *info)
{
	int i;
	for (i = 0; i < MAX_UIO_MAPS; i++) {
		if (info->mem[i].internal_addr)
			iounmap(info->mem[i].internal_addr);
	}
}

static int
igbuio_setup_bars(struct pci_dev *dev, struct uio_info *info)
{
	int i, iom, iop, ret;
	unsigned long flags;
	static const char *bar_names[PCI_STD_RESOURCE_END + 1]  = {
		"BAR0",
		"BAR1",
		"BAR2",
		"BAR3",
		"BAR4",
		"BAR5",
	};

	iom = 0;
	iop = 0;

	for (i = 0; i != sizeof(bar_names) / sizeof(bar_names[0]); i++) {
		if (pci_resource_len(dev, i) != 0 &&
				pci_resource_start(dev, i) != 0) {
			flags = pci_resource_flags(dev, i);
			if (flags & IORESOURCE_MEM) {
				if ((ret = igbuio_pci_setup_iomem(dev, info,
						iom, i, bar_names[i])) != 0)
					return (ret);
				iom++;
			} else if (flags & IORESOURCE_IO) {
				if ((ret = igbuio_pci_setup_ioport(dev, info,
						iop, i, bar_names[i])) != 0)
					return (ret);
				iop++;
			}
		}
	}

	return ((iom != 0) ? ret : ENOENT);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __devinit
#else
static int
#endif
igbuio_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct rte_uio_pci_dev *udev;

	udev = kzalloc(sizeof(struct rte_uio_pci_dev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	/*
	 * enable device: ask low-level code to enable I/O and
	 * memory
	 */
	if (pci_enable_device(dev)) {
		printk(KERN_ERR "Cannot enable PCI device\n");
		goto fail_free;
	}

	/*
	 * reserve device's PCI memory regions for use by this
	 * module
	 */
	if (pci_request_regions(dev, "igb_uio")) {
		printk(KERN_ERR "Cannot request regions\n");
		goto fail_disable;
	}

	/* enable bus mastering on the device */
	pci_set_master(dev);

	/* remap IO memory */
	if (igbuio_setup_bars(dev, &udev->info))
		goto fail_release_iomem;

	/* set 64-bit DMA mask */
	if (pci_set_dma_mask(dev,  DMA_BIT_MASK(64))) {
		printk(KERN_ERR "Cannot set DMA mask\n");
		goto fail_release_iomem;
	} else if (pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(64))) {
		printk(KERN_ERR "Cannot set consistent DMA mask\n");
		goto fail_release_iomem;
	}

	/* fill uio infos */
	udev->info.name = "Intel IGB UIO";
	udev->info.version = "0.1";
	udev->info.handler = igbuio_pci_irqhandler;
	udev->info.irqcontrol = igbuio_pci_irqcontrol;
#ifdef CONFIG_XEN_DOM0
	/* check if the driver run on Xen Dom0 */
	if (xen_initial_domain())
		udev->info.mmap = igbuio_dom0_pci_mmap;
#endif
	udev->info.priv = udev;
	udev->pdev = dev;
	udev->mode = RTE_INTR_MODE_LEGACY;
	spin_lock_init(&udev->lock);

	/* check if it need to try msix first */
	if (igbuio_intr_mode_preferred == RTE_INTR_MODE_MSIX) {
		int vector;

		for (vector = 0; vector < IGBUIO_NUM_MSI_VECTORS; vector ++)
			udev->msix_entries[vector].entry = vector;

		if (pci_enable_msix(udev->pdev, udev->msix_entries, IGBUIO_NUM_MSI_VECTORS) == 0) {
			udev->mode = RTE_INTR_MODE_MSIX;
		}
		else {
			pci_disable_msix(udev->pdev);
			printk(KERN_INFO "fail to enable pci msix, or not enough msix entries\n");
		}
	}
	switch (udev->mode) {
	case RTE_INTR_MODE_MSIX:
		udev->info.irq_flags = 0;
		udev->info.irq = udev->msix_entries[0].vector;
		break;
	case RTE_INTR_MODE_MSI:
		break;
	case RTE_INTR_MODE_LEGACY:
		udev->info.irq_flags = IRQF_SHARED;
		udev->info.irq = dev->irq;
		break;
	default:
		break;
	}

	pci_set_drvdata(dev, udev);
	igbuio_pci_irqcontrol(&udev->info, 0);

	if (sysfs_create_group(&dev->dev.kobj, &dev_attr_grp))
		goto fail_release_iomem;

	/* register uio driver */
	if (uio_register_device(&dev->dev, &udev->info))
		goto fail_release_iomem;

	printk(KERN_INFO "uio device registered with irq %lx\n", udev->info.irq);

	return 0;

fail_release_iomem:
	sysfs_remove_group(&dev->dev.kobj, &dev_attr_grp);
	igbuio_pci_release_iomem(&udev->info);
	if (udev->mode == RTE_INTR_MODE_MSIX)
		pci_disable_msix(udev->pdev);
	pci_release_regions(dev);
fail_disable:
	pci_disable_device(dev);
fail_free:
	kfree(udev);

	return -ENODEV;
}

static void
igbuio_pci_remove(struct pci_dev *dev)
{
	struct uio_info *info = pci_get_drvdata(dev);

	if (info->priv == NULL) {
		printk(KERN_DEBUG "Not igbuio device\n");
		return;
	}

	sysfs_remove_group(&dev->dev.kobj, &dev_attr_grp);
	uio_unregister_device(info);
	igbuio_pci_release_iomem(info);
	if (((struct rte_uio_pci_dev *)info->priv)->mode ==
			RTE_INTR_MODE_MSIX)
		pci_disable_msix(dev);
	pci_release_regions(dev);
	pci_disable_device(dev);
	pci_set_drvdata(dev, NULL);
	kfree(info);
}

static int
igbuio_config_intr_mode(char *intr_str)
{
	if (!intr_str) {
		printk(KERN_INFO "Use MSIX interrupt by default\n");
		return 0;
	}

	if (!strcmp(intr_str, RTE_INTR_MODE_MSIX_NAME)) {
		igbuio_intr_mode_preferred = RTE_INTR_MODE_MSIX;
		printk(KERN_INFO "Use MSIX interrupt\n");
	} else if (!strcmp(intr_str, RTE_INTR_MODE_LEGACY_NAME)) {
		igbuio_intr_mode_preferred = RTE_INTR_MODE_LEGACY;
		printk(KERN_INFO "Use legacy interrupt\n");
	} else {
		printk(KERN_INFO "Error: bad parameter - %s\n", intr_str);
		return -EINVAL;
	}

	return 0;
}

static struct pci_driver igbuio_pci_driver = {
	.name = "igb_uio",
	.id_table = NULL,
	.probe = igbuio_pci_probe,
	.remove = igbuio_pci_remove,
};

static int __init
igbuio_pci_init_module(void)
{
	int ret;

	ret = igbuio_config_intr_mode(intr_mode);
	if (ret < 0)
		return ret;

	return pci_register_driver(&igbuio_pci_driver);
}

static void __exit
igbuio_pci_exit_module(void)
{
	pci_unregister_driver(&igbuio_pci_driver);
}

module_init(igbuio_pci_init_module);
module_exit(igbuio_pci_exit_module);

module_param(intr_mode, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(intr_mode,
"igb_uio interrupt mode (default=msix):\n"
"    " RTE_INTR_MODE_MSIX_NAME "       Use MSIX interrupt\n"
"    " RTE_INTR_MODE_LEGACY_NAME "     Use Legacy interrupt\n"
"\n");

MODULE_DESCRIPTION("UIO driver for Intel IGB PCI cards");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel Corporation");
