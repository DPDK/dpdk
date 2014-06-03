/*-
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
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
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/version.h>

#include <xen/xen.h>
#include <xen/page.h>
#include <xen/xen-ops.h>
#include <xen/interface/memory.h>

#include <rte_config.h>
#include <exec-env/rte_dom0_common.h>

#include "dom0_mm_dev.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Kernel Module for supporting DPDK running on Xen Dom0");

static struct dom0_mm_dev dom0_dev;
static struct kobject *dom0_kobj = NULL;

static struct memblock_info *rsv_mm_info;

/* Default configuration for reserved memory size(2048 MB). */
static uint32_t rsv_memsize = 2048;

static int dom0_open(struct inode *inode, struct file *file);
static int dom0_release(struct inode *inode, struct file *file);
static int dom0_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param);
static int dom0_mmap(struct file *file, struct vm_area_struct *vma);
static int dom0_memory_free(uint32_t size);
static int dom0_memory_release(struct dom0_mm_data *mm_data);

static const struct file_operations data_fops = {
	.owner = THIS_MODULE,
	.open = dom0_open,
	.release = dom0_release,
	.mmap = dom0_mmap,
	.unlocked_ioctl = (void *)dom0_ioctl,
};

static ssize_t
show_memsize_rsvd(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%u\n", dom0_dev.used_memsize);
}

static ssize_t
show_memsize(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%u\n", dom0_dev.config_memsize);
}

static ssize_t
store_memsize(struct device *dev, struct device_attribute *attr,
            const char *buf, size_t count)
{
	int err = 0;
	unsigned long mem_size;

	if (0 != strict_strtoul(buf, 0, &mem_size))
		return  -EINVAL;

	mutex_lock(&dom0_dev.data_lock);
	if (0 == mem_size) {
		err = -EINVAL;
		goto fail;
	} else if (mem_size > (rsv_memsize - dom0_dev.used_memsize)) {
		XEN_ERR("configure memory size fail\n");
		err = -EINVAL;
		goto fail;
	} else
		dom0_dev.config_memsize = mem_size;

fail:
	mutex_unlock(&dom0_dev.data_lock);
	return err ? err : count;
}

static DEVICE_ATTR(memsize, S_IRUGO | S_IWUSR, show_memsize, store_memsize);
static DEVICE_ATTR(memsize_rsvd, S_IRUGO, show_memsize_rsvd, NULL);

static struct attribute *dev_attrs[] = {
	&dev_attr_memsize.attr,
	&dev_attr_memsize_rsvd.attr,
	NULL,
};

/* the memory size unit is MB */
static const struct attribute_group dev_attr_grp = {
	.name = "memsize-mB",
	.attrs = dev_attrs,
};


static void
sort_viraddr(struct memblock_info *mb, int cnt)
{
	int i,j;
	uint64_t tmp_pfn;
	uint64_t tmp_viraddr;

	/*sort virtual address and pfn */
	for(i = 0; i < cnt; i ++) {
		for(j = cnt - 1; j > i; j--) {
			if(mb[j].pfn < mb[j - 1].pfn) {
				tmp_pfn = mb[j - 1].pfn;
				mb[j - 1].pfn = mb[j].pfn;
				mb[j].pfn = tmp_pfn;

				tmp_viraddr = mb[j - 1].vir_addr;
				mb[j - 1].vir_addr = mb[j].vir_addr;
				mb[j].vir_addr = tmp_viraddr;
			}
		}
	}
}

static int
dom0_find_memdata(const char * mem_name)
{
	unsigned i;
	int idx = -1;
	for(i = 0; i< NUM_MEM_CTX; i++) {
		if(dom0_dev.mm_data[i] == NULL)
			continue;
		if (!strncmp(dom0_dev.mm_data[i]->name, mem_name,
			sizeof(char) * DOM0_NAME_MAX)) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int
dom0_find_mempos(void)
{
	unsigned i;
	int idx = -1;

	for(i = 0; i< NUM_MEM_CTX; i++) {
		if(dom0_dev.mm_data[i] == NULL){
			idx = i;
			break;
		}
	}

	return idx;
}

static int
dom0_memory_release(struct dom0_mm_data *mm_data)
{
	int idx;
	uint32_t  num_block, block_id;

	/* each memory block is 2M */
	num_block = mm_data->mem_size / SIZE_PER_BLOCK;
	if (num_block == 0)
		return -EINVAL;

	/* reset global memory data */
	idx = dom0_find_memdata(mm_data->name);
	if (idx >= 0) {
		dom0_dev.used_memsize -= mm_data->mem_size;
		dom0_dev.mm_data[idx] = NULL;
		dom0_dev.num_mem_ctx--;
	}

	/* reset these memory blocks status as free */
	for (idx = 0; idx < num_block; idx++) {
		block_id = mm_data->block_num[idx];
		rsv_mm_info[block_id].used = 0;
	}

	memset(mm_data, 0, sizeof(struct dom0_mm_data));
	vfree(mm_data);
	return 0;
}

static int
dom0_memory_free(uint32_t rsv_size)
{
	uint64_t vstart, vaddr;
	uint32_t i, num_block, size;

	if (!xen_pv_domain())
		return -1;

	/* each memory block is 2M */
	num_block = rsv_size / SIZE_PER_BLOCK;
	if (num_block == 0)
		return -EINVAL;

	/* free all memory blocks of size of 4M and destroy contiguous region */
	for (i = 0; i < dom0_dev.num_bigblock * 2; i += 2) {
		vstart = rsv_mm_info[i].vir_addr;
		if (vstart) {
		#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
			if (rsv_mm_info[i].exchange_flag)
				xen_destroy_contiguous_region(vstart,
						DOM0_CONTIG_NUM_ORDER);
			if (rsv_mm_info[i + 1].exchange_flag)
				xen_destroy_contiguous_region(vstart +
						DOM0_MEMBLOCK_SIZE,
						DOM0_CONTIG_NUM_ORDER);
		#else
			if (rsv_mm_info[i].exchange_flag)
				xen_destroy_contiguous_region(rsv_mm_info[i].pfn
					* PAGE_SIZE,
					DOM0_CONTIG_NUM_ORDER);
			if (rsv_mm_info[i + 1].exchange_flag)
				xen_destroy_contiguous_region(rsv_mm_info[i].pfn
					* PAGE_SIZE + DOM0_MEMBLOCK_SIZE,
					DOM0_CONTIG_NUM_ORDER);
		#endif

			size = DOM0_MEMBLOCK_SIZE * 2;
			vaddr = vstart;
			while (size > 0) {
				ClearPageReserved(virt_to_page(vaddr));
				vaddr += PAGE_SIZE;
				size -= PAGE_SIZE;
			}
			free_pages(vstart, MAX_NUM_ORDER);
		}
	}

	/* free all memory blocks size of 2M and destroy contiguous region */
	for (; i < num_block; i++) {
		vstart = rsv_mm_info[i].vir_addr;
		if (vstart) {
			if (rsv_mm_info[i].exchange_flag)
				xen_destroy_contiguous_region(vstart,
					DOM0_CONTIG_NUM_ORDER);

			size = DOM0_MEMBLOCK_SIZE;
			vaddr = vstart;
			while (size > 0) {
				ClearPageReserved(virt_to_page(vaddr));
				vaddr += PAGE_SIZE;
				size -= PAGE_SIZE;
			}
			free_pages(vstart, DOM0_CONTIG_NUM_ORDER);
		}
	}

	memset(rsv_mm_info, 0, sizeof(struct memblock_info) * num_block);
	vfree(rsv_mm_info);
	rsv_mm_info = NULL;

	return 0;
}

static void
find_free_memory(uint32_t count, struct dom0_mm_data *mm_data)
{
	uint32_t i = 0;
	uint32_t j = 0;

	while ((i < count) && (j < rsv_memsize / SIZE_PER_BLOCK)) {
		if (rsv_mm_info[j].used == 0) {
			mm_data->block_info[i].pfn = rsv_mm_info[j].pfn;
			mm_data->block_info[i].vir_addr =
				rsv_mm_info[j].vir_addr;
			mm_data->block_info[i].mfn = rsv_mm_info[j].mfn;
			mm_data->block_info[i].exchange_flag =
				rsv_mm_info[j].exchange_flag;
			mm_data->block_num[i] = j;
			rsv_mm_info[j].used = 1;
			i++;
		}
		j++;
	}
}

/**
 * Find all memory segments in which physical addresses are contiguous.
 */
static void
find_memseg(int count, struct dom0_mm_data * mm_data)
{
	int i = 0;
	int j, k, idx = 0;
	uint64_t zone_len, pfn, num_block;

	while(i < count) {
		if (mm_data->block_info[i].exchange_flag == 0) {
			i++;
			continue;
		}
		k = 0;
		pfn = mm_data->block_info[i].pfn;
		mm_data->seg_info[idx].pfn = pfn;
		mm_data->seg_info[idx].mfn[k] = mm_data->block_info[i].mfn;

		for (j = i + 1; j < count; j++) {

			/* ignore exchange fail memory block */
			if (mm_data->block_info[j].exchange_flag == 0)
				break;

			if (mm_data->block_info[j].pfn !=
				(mm_data->block_info[j - 1].pfn +
					 DOM0_MEMBLOCK_SIZE / PAGE_SIZE))
			    break;
			++k;
			mm_data->seg_info[idx].mfn[k] = mm_data->block_info[j].mfn;
		}

		num_block = j - i;
		zone_len = num_block * DOM0_MEMBLOCK_SIZE;
		mm_data->seg_info[idx].size = zone_len;

		XEN_PRINT("memseg id=%d, size=0x%llx\n", idx, zone_len);
		i = i+ num_block;
		idx++;
		if (idx == DOM0_NUM_MEMSEG)
			break;
	}
	mm_data->num_memseg = idx;
}

static int
dom0_memory_reserve(uint32_t rsv_size)
{
	uint64_t pfn, vstart, vaddr;
	uint32_t i, num_block, size, allocated_size = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	dma_addr_t dma_handle;
#endif

	/* 2M as memory block */
	num_block = rsv_size / SIZE_PER_BLOCK;

	rsv_mm_info = vmalloc(sizeof(struct memblock_info) * num_block);
	if (!rsv_mm_info) {
		XEN_ERR("Unable to allocate device memory information\n");
		return -ENOMEM;
	}
	memset(rsv_mm_info, 0, sizeof(struct memblock_info) * num_block);

	/* try alloc size of 4M once */
	for (i = 0; i < num_block; i += 2) {
		vstart = (unsigned long)
			__get_free_pages(GFP_ATOMIC, MAX_NUM_ORDER);
		if (vstart == 0)
			break;

		dom0_dev.num_bigblock = i / 2 + 1;
		allocated_size =  SIZE_PER_BLOCK * (i + 2);

		/* size of 4M */
		size = DOM0_MEMBLOCK_SIZE * 2;

		vaddr = vstart;
		while (size > 0) {
			SetPageReserved(virt_to_page(vaddr));
			vaddr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}

		pfn = virt_to_pfn(vstart);
		rsv_mm_info[i].pfn = pfn;
		rsv_mm_info[i].vir_addr = vstart;
		rsv_mm_info[i + 1].pfn =
				pfn + DOM0_MEMBLOCK_SIZE / PAGE_SIZE;
		rsv_mm_info[i + 1].vir_addr =
				vstart + DOM0_MEMBLOCK_SIZE;
	}

	/*if it failed to alloc 4M, and continue to alloc 2M once */
	for (; i < num_block; i++) {
		vstart = (unsigned long)
			__get_free_pages(GFP_ATOMIC, DOM0_CONTIG_NUM_ORDER);
		if (vstart == 0) {
			XEN_ERR("allocate memory fail.\n");
			dom0_memory_free(allocated_size);
			return -ENOMEM;
		}

		allocated_size += SIZE_PER_BLOCK;

		size = DOM0_MEMBLOCK_SIZE;
		vaddr = vstart;
		while (size > 0) {
			SetPageReserved(virt_to_page(vaddr));
			vaddr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
		pfn = virt_to_pfn(vstart);
		rsv_mm_info[i].pfn = pfn;
		rsv_mm_info[i].vir_addr = vstart;
	}

	sort_viraddr(rsv_mm_info, num_block);
	
	for (i = 0; i< num_block; i++) {

		/*
		 * This API is used to exchage MFN for getting a block of
		 * contiguous physical addresses, its maximum size is 2M.
		 */
	#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
		if (xen_create_contiguous_region(rsv_mm_info[i].vir_addr,
				DOM0_CONTIG_NUM_ORDER, 0) == 0) {
	#else
		if (xen_create_contiguous_region(rsv_mm_info[i].pfn * PAGE_SIZE,
				DOM0_CONTIG_NUM_ORDER, 0, &dma_handle) == 0) {
	#endif
			rsv_mm_info[i].exchange_flag = 1;
			rsv_mm_info[i].mfn =
				pfn_to_mfn(rsv_mm_info[i].pfn);
			rsv_mm_info[i].used = 0;
		} else {
			XEN_ERR("exchange memeory fail\n");
			rsv_mm_info[i].exchange_flag = 0;
			dom0_dev.fail_times++;
			if (dom0_dev.fail_times > MAX_EXCHANGE_FAIL_TIME) {
				dom0_memory_free(rsv_size);
				return  -EFAULT;
			}
		}
	}

	return 0;
}

static int
dom0_prepare_memsegs(struct memory_info *meminfo, struct dom0_mm_data *mm_data)
{
	uint32_t num_block;
	int idx;

	/* check if there is a free name buffer */
	memcpy(mm_data->name, meminfo->name, DOM0_NAME_MAX);
	mm_data->name[DOM0_NAME_MAX - 1] = '\0';
	idx = dom0_find_mempos();
	if (idx < 0)
		return -1;

	num_block = meminfo->size / SIZE_PER_BLOCK;
	/* find free memory and new memory segments*/
	find_free_memory(num_block, mm_data);
	find_memseg(num_block, mm_data);

	/* update private memory data */
	mm_data->refcnt++;
	mm_data->mem_size = meminfo->size;

	/* update global memory data */
	dom0_dev.mm_data[idx] = mm_data;
	dom0_dev.num_mem_ctx++;
	dom0_dev.used_memsize += mm_data->mem_size;

	return 0;
}

static int
dom0_check_memory (struct memory_info *meminfo)
{
	int idx;
	uint64_t mem_size;

	/* round memory size to the next even number. */
	if (meminfo->size % 2)
		++meminfo->size;

	mem_size = meminfo->size;
	if (dom0_dev.num_mem_ctx > NUM_MEM_CTX) {
		XEN_ERR("Memory data space is full in Dom0 driver\n");
		return -1;
	}
	idx = dom0_find_memdata(meminfo->name);
	if (idx >= 0) {
		XEN_ERR("Memory data name %s has already exsited in Dom0 driver.\n",
			meminfo->name);
		return -1;
	}
	if ((dom0_dev.used_memsize + mem_size) > rsv_memsize) {
		XEN_ERR("Total size can't be larger than reserved size.\n");
		return -1;
	}

	return 0;
}

static int __init
dom0_init(void)
{
	if (!xen_domain())
		return -ENODEV;

	if (rsv_memsize > DOM0_CONFIG_MEMSIZE) {
		XEN_ERR("The reserved memory size cannot be greater than %d\n",
			DOM0_CONFIG_MEMSIZE);
		return -EINVAL;
	}

	/* Setup the misc device */
	dom0_dev.miscdev.minor = MISC_DYNAMIC_MINOR;
	dom0_dev.miscdev.name = "dom0_mm";
	dom0_dev.miscdev.fops = &data_fops;

	/* register misc char device */
	if (misc_register(&dom0_dev.miscdev) != 0) {
		XEN_ERR("Misc device registration failed\n");
		return -EPERM;
	}

	mutex_init(&dom0_dev.data_lock);
	dom0_kobj = kobject_create_and_add("dom0-mm", mm_kobj);

	if (!dom0_kobj) {
		XEN_ERR("dom0-mm object creation failed\n");
		misc_deregister(&dom0_dev.miscdev);
		return -ENOMEM;
	}

	if (sysfs_create_group(dom0_kobj, &dev_attr_grp)) {
		kobject_put(dom0_kobj);
		misc_deregister(&dom0_dev.miscdev);
		return -EPERM;
	}

	if (dom0_memory_reserve(rsv_memsize) < 0) {
		sysfs_remove_group(dom0_kobj, &dev_attr_grp);
		kobject_put(dom0_kobj);
		misc_deregister(&dom0_dev.miscdev);
		return -ENOMEM;
	}

	XEN_PRINT("####### DPDK Xen Dom0 module loaded  #######\n");

	return 0;
}

static void __exit
dom0_exit(void)
{
	if (rsv_mm_info != NULL)
		dom0_memory_free(rsv_memsize);

	sysfs_remove_group(dom0_kobj, &dev_attr_grp);
	kobject_put(dom0_kobj);
	misc_deregister(&dom0_dev.miscdev);

	XEN_PRINT("####### DPDK Xen Dom0 module unloaded  #######\n");
}

static int
dom0_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	XEN_PRINT(KERN_INFO "/dev/dom0_mm opened\n");
	return 0;
}

static int
dom0_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct dom0_mm_data *mm_data = file->private_data;

	if (mm_data == NULL)
		return ret;

	mutex_lock(&dom0_dev.data_lock);
	if (--mm_data->refcnt == 0)
		ret = dom0_memory_release(mm_data);
	mutex_unlock(&dom0_dev.data_lock);

	file->private_data = NULL;
	XEN_PRINT(KERN_INFO "/dev/dom0_mm closed\n");
	return ret;
}

static int
dom0_mmap(struct file *file, struct vm_area_struct *vm)
{
	int status = 0;
	uint32_t idx = vm->vm_pgoff;
	uint64_t pfn, size = vm->vm_end - vm->vm_start;
	struct dom0_mm_data *mm_data = file->private_data;

	if(mm_data == NULL)
		return -EINVAL;

	mutex_lock(&dom0_dev.data_lock);
	if (idx >= mm_data->num_memseg) {
		mutex_unlock(&dom0_dev.data_lock);
		return -EINVAL;
	}

	if (size > mm_data->seg_info[idx].size){
		mutex_unlock(&dom0_dev.data_lock);
		return -EINVAL;
	}

	XEN_PRINT("mmap memseg idx =%d,size = 0x%llx\n", idx, size);

	pfn = mm_data->seg_info[idx].pfn;
	mutex_unlock(&dom0_dev.data_lock);

	status = remap_pfn_range(vm, vm->vm_start, pfn, size, PAGE_SHARED);

	return status;
}
static int
dom0_ioctl(struct file *file,
	unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int idx, ret;
	char name[DOM0_NAME_MAX] = {0};
	struct memory_info meminfo;
	struct dom0_mm_data *mm_data = file->private_data;

	XEN_PRINT("IOCTL num=0x%0x param=0x%0lx \n", ioctl_num, ioctl_param);

	/**
	 * Switch according to the ioctl called
	 */
	switch _IOC_NR(ioctl_num) {
	case _IOC_NR(RTE_DOM0_IOCTL_PREPARE_MEMSEG):
		ret = copy_from_user(&meminfo, (void *)ioctl_param,
			sizeof(struct memory_info));
		if (ret)
			return  -EFAULT;

		if (mm_data != NULL) {
			XEN_ERR("Cannot create memory segment for the same"
				" file descriptor\n");
			return -EINVAL;
		}

		/* Allocate private data */
		mm_data = vmalloc(sizeof(struct dom0_mm_data));
		if (!mm_data) {
			XEN_ERR("Unable to allocate device private data\n");
			return -ENOMEM;
		}
		memset(mm_data, 0, sizeof(struct dom0_mm_data));

		mutex_lock(&dom0_dev.data_lock);
		/* check if we can allocate memory*/
		if (dom0_check_memory(&meminfo) < 0) {
			mutex_unlock(&dom0_dev.data_lock);
			vfree(mm_data);
			return -EINVAL;
		}

		/* allocate memory and created memory segments*/
		if (dom0_prepare_memsegs(&meminfo, mm_data) < 0) {
			XEN_ERR("create memory segment fail.\n");
			mutex_unlock(&dom0_dev.data_lock);
			return -EIO;
		}

		file->private_data = mm_data;
		mutex_unlock(&dom0_dev.data_lock);
		break;

	/* support multiple process in term of memory mapping*/
	case _IOC_NR(RTE_DOM0_IOCTL_ATTACH_TO_MEMSEG):
		ret = copy_from_user(name, (void *)ioctl_param,
				sizeof(char) * DOM0_NAME_MAX);
		if (ret)
			return -EFAULT;

		mutex_lock(&dom0_dev.data_lock);
		idx = dom0_find_memdata(name);
		if (idx < 0) {
			mutex_unlock(&dom0_dev.data_lock);
			return -EINVAL;
		}

		mm_data = dom0_dev.mm_data[idx];
		mm_data->refcnt++;
		file->private_data = mm_data;
		mutex_unlock(&dom0_dev.data_lock);
		break;

	case _IOC_NR(RTE_DOM0_IOCTL_GET_NUM_MEMSEG):
		ret = copy_to_user((void *)ioctl_param, &mm_data->num_memseg,
				sizeof(int));
		if (ret)
			return -EFAULT;
		break;

	case _IOC_NR(RTE_DOM0_IOCTL_GET_MEMSEG_INFO):
		ret = copy_to_user((void *)ioctl_param,
				&mm_data->seg_info[0],
				sizeof(struct memseg_info) *
				mm_data->num_memseg);
		if (ret)
			return -EFAULT;
		break;
	default:
		XEN_PRINT("IOCTL default \n");
		break;
	}

	return 0;
}

module_init(dom0_init);
module_exit(dom0_exit);

module_param(rsv_memsize, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rsv_memsize, "Xen-dom0 reserved memory size(MB).\n");
