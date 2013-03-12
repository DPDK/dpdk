/*-
 * GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/kthread.h>

#include <exec-env/rte_kni_common.h>
#include "kni_dev.h"
#include <rte_config.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Kernel Module for managing kni devices");

#define KNI_RX_LOOP_NUM 1000

#define KNI_MAX_DEVICES 32

extern void kni_net_rx(struct kni_dev *kni);
extern void kni_net_init(struct net_device *dev);
extern void kni_net_config_lo_mode(char *lo_str);
extern void kni_net_poll_resp(struct kni_dev *kni);
extern void kni_set_ethtool_ops(struct net_device *netdev);

extern int ixgbe_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
extern void ixgbe_kni_remove(struct pci_dev *pdev);
extern int igb_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
extern void igb_kni_remove(struct pci_dev *pdev);

static int kni_open(struct inode *inode, struct file *file);
static int kni_release(struct inode *inode, struct file *file);
static int kni_ioctl(struct inode *inode, unsigned int ioctl_num,
					unsigned long ioctl_param);
static int kni_compat_ioctl(struct inode *inode, unsigned int ioctl_num,
						unsigned long ioctl_param);

/* kni kernel thread for rx */
static int kni_thread(void *unused);

static struct file_operations kni_fops = {
	.owner = THIS_MODULE,
	.open = kni_open,
	.release = kni_release,
	.unlocked_ioctl = (void *)kni_ioctl,
	.compat_ioctl = (void *)kni_compat_ioctl,
};

static struct miscdevice kni_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KNI_DEVICE,
	.fops = &kni_fops,
};

/* loopback mode */
static char *lo_mode = NULL;

static struct kni_dev *kni_devs[KNI_MAX_DEVICES];
static volatile int num_devs;  /* number of kni devices */

#define KNI_DEV_IN_USE_BIT_NUM 0 /* Bit number for device in use */

static volatile unsigned long device_in_use; /* device in use flag */
static struct task_struct *kni_kthread;

static int __init
kni_init(void)
{
	KNI_PRINT("######## DPDK kni module loading ########\n");

	if (misc_register(&kni_misc) != 0) {
		KNI_ERR("Misc registration failed\n");
		return 1;
	}

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use);

	/* Configure the lo mode according to the input parameter */
	kni_net_config_lo_mode(lo_mode);

	KNI_PRINT("######## DPDK kni module loaded  ########\n");

	return 0;
}

static void __exit
kni_exit(void)
{
	misc_deregister(&kni_misc);
	KNI_PRINT("####### DPDK kni module unloaded  #######\n");
}

static int
kni_open(struct inode *inode, struct file *file)
{
	/* kni device can be opened by one user only, test and set bit */
	if (test_and_set_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use))
		return -EBUSY;

	memset(kni_devs, 0, sizeof(kni_devs));
	num_devs = 0;

	/* Create kernel thread for RX */
	kni_kthread = kthread_run(kni_thread, NULL, "kni_thread");
	if (IS_ERR(kni_kthread)) {
		KNI_ERR("Unable to create kernel threaed\n");
		return PTR_ERR(kni_kthread);
	}

	KNI_PRINT("/dev/kni opened\n");

	return 0;
}

static int
kni_release(struct inode *inode, struct file *file)
{
	int i;

	KNI_PRINT("Stopping KNI thread...");

	/* Stop kernel thread */
	kthread_stop(kni_kthread);
	kni_kthread = NULL;

	for (i = 0; i < KNI_MAX_DEVICES; i++) {
		if (kni_devs[i] != NULL) {
			/* Call the remove part to restore pci dev */
			switch (kni_devs[i]->device_id) {
			#define RTE_PCI_DEV_ID_DECL_IGB(vend, dev) case (dev):
			#include <rte_pci_dev_ids.h>
				igb_kni_remove(kni_devs[i]->pci_dev);
				break;
			#define RTE_PCI_DEV_ID_DECL_IXGBE(vend, dev) case (dev):
			#include <rte_pci_dev_ids.h>
				ixgbe_kni_remove(kni_devs[i]->pci_dev);
				break;
			default:
				break;
			}

			unregister_netdev(kni_devs[i]->net_dev);
			free_netdev(kni_devs[i]->net_dev);
			kni_devs[i] = NULL;
		}
	}
	num_devs = 0;

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use);

	KNI_PRINT("/dev/kni closed\n");

	return 0;
}

static int
kni_thread(void *unused)
{
	int i, j;

	KNI_PRINT("Kernel thread for KNI started\n");
	while (!kthread_should_stop()) {
		int n_devs = num_devs;
		for (j = 0; j < KNI_RX_LOOP_NUM; j++) {
			for (i = 0; i < n_devs; i++) {
				/* This shouldn't be needed */
				if (kni_devs[i]) {
					kni_net_rx(kni_devs[i]);
					kni_net_poll_resp(kni_devs[i]);
				}
				else
					KNI_ERR("kni_thread -no kni found!!!");
			}
		}
		/* reschedule out for a while */
		schedule_timeout_interruptible(usecs_to_jiffies( \
				KNI_KTHREAD_RESCHEDULE_INTERVAL));
	}
	KNI_PRINT("Kernel thread for KNI stopped\n");

	return 0;
}

static int
kni_ioctl_create(unsigned int ioctl_num, unsigned long ioctl_param)
{
	int ret;
	struct rte_kni_device_info dev_info;
	struct pci_dev *pci = NULL;
	struct pci_dev *found_pci = NULL;
	struct net_device *net_dev = NULL;
	struct net_device *lad_dev = NULL;
	struct kni_dev *kni;

	if (num_devs == KNI_MAX_DEVICES)
		return -EBUSY;

	/* Check the buffer size, to avoid warning */
	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
		return -EINVAL;

	/* Copy kni info from user space */
	ret = copy_from_user(&dev_info, (void *)ioctl_param,
					_IOC_SIZE(ioctl_num));
	if (ret) {
		KNI_ERR("copy_from_user");
		return -EIO;
	}

	net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
							kni_net_init);
	if (net_dev == NULL) {
		KNI_ERR("error allocating device \"%s\"\n", dev_info.name);
		return -EBUSY;
	}

	kni = netdev_priv(net_dev);

	kni->net_dev = net_dev;
	kni->idx = num_devs;

	/* Translate user space info into kernel space info */
	kni->tx_q = phys_to_virt(dev_info.tx_phys);
	kni->rx_q = phys_to_virt(dev_info.rx_phys);
	kni->alloc_q = phys_to_virt(dev_info.alloc_phys);
	kni->free_q = phys_to_virt(dev_info.free_phys);

	kni->req_q = phys_to_virt(dev_info.req_phys);
	kni->resp_q = phys_to_virt(dev_info.resp_phys);

	kni->sync_va = dev_info.sync_va;
	kni->sync_kva = phys_to_virt(dev_info.sync_phys);

	kni->mbuf_kva = phys_to_virt(dev_info.mbuf_phys);
	kni->mbuf_va = dev_info.mbuf_va;

	kni->mbuf_size = dev_info.mbuf_size;

	KNI_PRINT("tx_phys:          0x%016llx, tx_q addr:          0x%p\n",
						dev_info.tx_phys, kni->tx_q);
	KNI_PRINT("rx_phys:          0x%016llx, rx_q addr:          0x%p\n",
						dev_info.rx_phys, kni->rx_q);
	KNI_PRINT("alloc_phys:       0x%016llx, alloc_q addr:       0x%p\n",
					dev_info.alloc_phys, kni->alloc_q);
	KNI_PRINT("free_phys:        0x%016llx, free_q addr:        0x%p\n",
					dev_info.free_phys, kni->free_q);
	KNI_PRINT("req_phys:         0x%016llx, req_q addr:         0x%p\n",
					dev_info.req_phys, kni->req_q);
	KNI_PRINT("resp_phys:        0x%016llx, resp_q addr:        0x%p\n",
					dev_info.resp_phys, kni->resp_q);
	KNI_PRINT("mbuf_phys:        0x%016llx, mbuf_kva:           0x%p\n",
					dev_info.mbuf_phys, kni->mbuf_kva);
	KNI_PRINT("mbuf_va:          0x%p\n", dev_info.mbuf_va);
	KNI_PRINT("mbuf_size:        %u\n", kni->mbuf_size);

	KNI_DBG("PCI: %02x:%02x.%02x %04x:%04x\n", dev_info.bus, dev_info.devid,
			dev_info.function, dev_info.vendor_id, dev_info.device_id);

	pci = pci_get_device(dev_info.vendor_id, dev_info.device_id, NULL);

	/* Support Ethtool */
	while (pci) {
		KNI_PRINT("pci_bus: %02x:%02x:%02x \n", pci->bus->number,
				PCI_SLOT(pci->devfn), PCI_FUNC(pci->devfn));

		if ((pci->bus->number == dev_info.bus) &&
			(PCI_SLOT(pci->devfn) == dev_info.devid) &&
			(PCI_FUNC(pci->devfn) == dev_info.function)) {
			found_pci = pci;

			switch (dev_info.device_id) {
			#define RTE_PCI_DEV_ID_DECL_IGB(vend, dev) case (dev):
			#include <rte_pci_dev_ids.h>
				ret = igb_kni_probe(found_pci, &lad_dev);
				break;
			#define RTE_PCI_DEV_ID_DECL_IXGBE(vend, dev) case (dev):
			#include <rte_pci_dev_ids.h>
				ret = ixgbe_kni_probe(found_pci, &lad_dev);
				break;
			default:
				ret = -1;
				break;
			}

			KNI_DBG("PCI found: pci=0x%p, lad_dev=0x%p\n", pci, lad_dev);
			if (ret == 0) {
				kni->lad_dev = lad_dev;
				kni_set_ethtool_ops(kni->net_dev);
			}
			else {
				KNI_ERR("Device not supported by ethtool");
				kni->lad_dev = NULL;
			}

			kni->pci_dev = found_pci;
			kni->device_id = dev_info.device_id;
			break;
		}
		pci = pci_get_device(dev_info.vendor_id, dev_info.device_id,
									pci);
	}

	if (pci)
		pci_dev_put(pci);

	ret = register_netdev(net_dev);
	if (ret) {
		KNI_ERR("error %i registering device \"%s\"\n", ret,
							dev_info.name);
		free_netdev(net_dev);
		return -ENODEV;
	}

	kni_devs[num_devs++] = kni;

	return 0;
}

static int
kni_ioctl(struct inode *inode,
	unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int ret = -EINVAL;

	KNI_DBG("IOCTL num=0x%0x param=0x%0lx \n", ioctl_num, ioctl_param);

	/*
	 * Switch according to the ioctl called
	 */
	switch (_IOC_NR(ioctl_num)) {
	case _IOC_NR(RTE_KNI_IOCTL_TEST):
		/* For test only, not used */
		break;
	case _IOC_NR(RTE_KNI_IOCTL_CREATE):
		ret = kni_ioctl_create(ioctl_num, ioctl_param);
		break;
	default:
		KNI_DBG("IOCTL default \n");
		break;
	}

	return ret;
}

static int
kni_compat_ioctl(struct inode *inode,
		unsigned int ioctl_num,
		unsigned long ioctl_param)
{
	/* 32 bits app on 64 bits OS to be supported later */
	KNI_PRINT("Not implemented.\n");

	return -EINVAL;
}

module_init(kni_init);
module_exit(kni_exit);

module_param(lo_mode, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lo_mode,
"KNI loopback mode (default=lo_mode_none):\n"
"    lo_mode_none        Kernel loopback disabled\n"
"    lo_mode_fifo        Enable kernel loopback with fifo\n"
"    lo_mode_fifo_skb    Enable kernel loopback with fifo and skb buffer\n"
"\n"
);

