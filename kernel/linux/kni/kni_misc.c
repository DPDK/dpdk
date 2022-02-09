// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2010-2014 Intel Corporation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

#include <rte_kni_common.h>

#include "compat.h"
#include "kni_dev.h"

MODULE_VERSION(KNI_VERSION);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Kernel Module for managing kni devices");

#define KNI_RX_LOOP_NUM 1000

#define KNI_MAX_DEVICES 32

/* loopback mode */
static char *lo_mode;

/* Kernel thread mode */
static char *kthread_mode;
static uint32_t multiple_kthread_on;

/* Default carrier state for created KNI network interfaces */
static char *carrier;
uint32_t kni_dflt_carrier;

/* Request processing support for bifurcated drivers. */
static char *enable_bifurcated;
uint32_t bifurcated_support;

/* KNI thread scheduling interval */
static long min_scheduling_interval = 100; /* us */
static long max_scheduling_interval = 200; /* us */

#define KNI_DEV_IN_USE_BIT_NUM 0 /* Bit number for device in use */

static int kni_net_id;

struct kni_net {
	unsigned long device_in_use; /* device in use flag */
	struct mutex kni_kthread_lock;
	struct task_struct *kni_kthread;
	struct rw_semaphore kni_list_lock;
	struct list_head kni_list_head;
};

static int __net_init
kni_init_net(struct net *net)
{
#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	struct kni_net *knet = net_generic(net, kni_net_id);

	memset(knet, 0, sizeof(*knet));
#else
	struct kni_net *knet;
	int ret;

	knet = kzalloc(sizeof(struct kni_net), GFP_KERNEL);
	if (!knet) {
		ret = -ENOMEM;
		return ret;
	}
#endif

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &knet->device_in_use);

	mutex_init(&knet->kni_kthread_lock);

	init_rwsem(&knet->kni_list_lock);
	INIT_LIST_HEAD(&knet->kni_list_head);

#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	return 0;
#else
	ret = net_assign_generic(net, kni_net_id, knet);
	if (ret < 0)
		kfree(knet);

	return ret;
#endif
}

static void __net_exit
kni_exit_net(struct net *net)
{
	struct kni_net *knet __maybe_unused;

	knet = net_generic(net, kni_net_id);
	mutex_destroy(&knet->kni_kthread_lock);

#ifndef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	kfree(knet);
#endif
}

static struct pernet_operations kni_net_ops = {
	.init = kni_init_net,
	.exit = kni_exit_net,
#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	.id   = &kni_net_id,
	.size = sizeof(struct kni_net),
#endif
};

static int
kni_thread_single(void *data)
{
	struct kni_net *knet = data;
	int j;
	struct kni_dev *dev;

	while (!kthread_should_stop()) {
		down_read(&knet->kni_list_lock);
		for (j = 0; j < KNI_RX_LOOP_NUM; j++) {
			list_for_each_entry(dev, &knet->kni_list_head, list) {
				kni_net_rx(dev);
				kni_net_poll_resp(dev);
			}
		}
		up_read(&knet->kni_list_lock);
		/* reschedule out for a while */
		usleep_range(min_scheduling_interval, max_scheduling_interval);
	}

	return 0;
}

static int
kni_thread_multiple(void *param)
{
	int j;
	struct kni_dev *dev = param;

	while (!kthread_should_stop()) {
		for (j = 0; j < KNI_RX_LOOP_NUM; j++) {
			kni_net_rx(dev);
			kni_net_poll_resp(dev);
		}
		usleep_range(min_scheduling_interval, max_scheduling_interval);
	}

	return 0;
}

static int
kni_open(struct inode *inode, struct file *file)
{
	struct net *net = current->nsproxy->net_ns;
	struct kni_net *knet = net_generic(net, kni_net_id);

	/* kni device can be opened by one user only per netns */
	if (test_and_set_bit(KNI_DEV_IN_USE_BIT_NUM, &knet->device_in_use))
		return -EBUSY;

	file->private_data = get_net(net);
	pr_debug("/dev/kni opened\n");

	return 0;
}

static int
kni_dev_remove(struct kni_dev *dev)
{
	if (!dev)
		return -ENODEV;

	/*
	 * The memory of kni device is allocated and released together
	 * with net device. Release mbuf before freeing net device.
	 */
	kni_net_release_fifo_phy(dev);

	if (dev->net_dev) {
		unregister_netdev(dev->net_dev);
		free_netdev(dev->net_dev);
	}

	return 0;
}

static int
kni_release(struct inode *inode, struct file *file)
{
	struct net *net = file->private_data;
	struct kni_net *knet = net_generic(net, kni_net_id);
	struct kni_dev *dev, *n;

	/* Stop kernel thread for single mode */
	if (multiple_kthread_on == 0) {
		mutex_lock(&knet->kni_kthread_lock);
		/* Stop kernel thread */
		if (knet->kni_kthread != NULL) {
			kthread_stop(knet->kni_kthread);
			knet->kni_kthread = NULL;
		}
		mutex_unlock(&knet->kni_kthread_lock);
	}

	down_write(&knet->kni_list_lock);
	list_for_each_entry_safe(dev, n, &knet->kni_list_head, list) {
		/* Stop kernel thread for multiple mode */
		if (multiple_kthread_on && dev->pthread != NULL) {
			kthread_stop(dev->pthread);
			dev->pthread = NULL;
		}

		list_del(&dev->list);
		kni_dev_remove(dev);
	}
	up_write(&knet->kni_list_lock);

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &knet->device_in_use);

	put_net(net);
	pr_debug("/dev/kni closed\n");

	return 0;
}

static int
kni_check_param(struct kni_dev *kni, struct rte_kni_device_info *dev)
{
	if (!kni || !dev)
		return -1;

	/* Check if network name has been used */
	if (!strncmp(kni->name, dev->name, RTE_KNI_NAMESIZE)) {
		pr_err("KNI name %s duplicated\n", dev->name);
		return -1;
	}

	return 0;
}

static int
kni_run_thread(struct kni_net *knet, struct kni_dev *kni, uint8_t force_bind)
{
	/**
	 * Create a new kernel thread for multiple mode, set its core affinity,
	 * and finally wake it up.
	 */
	if (multiple_kthread_on) {
		kni->pthread = kthread_create(kni_thread_multiple,
			(void *)kni, "kni_%s", kni->name);
		if (IS_ERR(kni->pthread)) {
			kni_dev_remove(kni);
			return -ECANCELED;
		}

		if (force_bind)
			kthread_bind(kni->pthread, kni->core_id);
		wake_up_process(kni->pthread);
	} else {
		mutex_lock(&knet->kni_kthread_lock);

		if (knet->kni_kthread == NULL) {
			knet->kni_kthread = kthread_create(kni_thread_single,
				(void *)knet, "kni_single");
			if (IS_ERR(knet->kni_kthread)) {
				mutex_unlock(&knet->kni_kthread_lock);
				kni_dev_remove(kni);
				return -ECANCELED;
			}

			if (force_bind)
				kthread_bind(knet->kni_kthread, kni->core_id);
			wake_up_process(knet->kni_kthread);
		}

		mutex_unlock(&knet->kni_kthread_lock);
	}

	return 0;
}

static int
kni_ioctl_create(struct net *net, uint32_t ioctl_num,
		unsigned long ioctl_param)
{
	struct kni_net *knet = net_generic(net, kni_net_id);
	int ret;
	struct rte_kni_device_info dev_info;
	struct net_device *net_dev = NULL;
	struct kni_dev *kni, *dev, *n;

	pr_info("Creating kni...\n");
	/* Check the buffer size, to avoid warning */
	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
		return -EINVAL;

	/* Copy kni info from user space */
	if (copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info)))
		return -EFAULT;

	/* Check if name is zero-ended */
	if (strnlen(dev_info.name, sizeof(dev_info.name)) == sizeof(dev_info.name)) {
		pr_err("kni.name not zero-terminated");
		return -EINVAL;
	}

	/**
	 * Check if the cpu core id is valid for binding.
	 */
	if (dev_info.force_bind && !cpu_online(dev_info.core_id)) {
		pr_err("cpu %u is not online\n", dev_info.core_id);
		return -EINVAL;
	}

	/* Check if it has been created */
	down_read(&knet->kni_list_lock);
	list_for_each_entry_safe(dev, n, &knet->kni_list_head, list) {
		if (kni_check_param(dev, &dev_info) < 0) {
			up_read(&knet->kni_list_lock);
			return -EINVAL;
		}
	}
	up_read(&knet->kni_list_lock);

	net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
#ifdef NET_NAME_USER
							NET_NAME_USER,
#endif
							kni_net_init);
	if (net_dev == NULL) {
		pr_err("error allocating device \"%s\"\n", dev_info.name);
		return -EBUSY;
	}

	dev_net_set(net_dev, net);

	kni = netdev_priv(net_dev);

	kni->net_dev = net_dev;
	kni->core_id = dev_info.core_id;
	strncpy(kni->name, dev_info.name, RTE_KNI_NAMESIZE);

	/* Translate user space info into kernel space info */
	if (dev_info.iova_mode) {
#ifdef HAVE_IOVA_TO_KVA_MAPPING_SUPPORT
		kni->tx_q = iova_to_kva(current, dev_info.tx_phys);
		kni->rx_q = iova_to_kva(current, dev_info.rx_phys);
		kni->alloc_q = iova_to_kva(current, dev_info.alloc_phys);
		kni->free_q = iova_to_kva(current, dev_info.free_phys);

		kni->req_q = iova_to_kva(current, dev_info.req_phys);
		kni->resp_q = iova_to_kva(current, dev_info.resp_phys);
		kni->sync_va = dev_info.sync_va;
		kni->sync_kva = iova_to_kva(current, dev_info.sync_phys);
		kni->usr_tsk = current;
		kni->iova_mode = 1;
#else
		pr_err("KNI module does not support IOVA to VA translation\n");
		return -EINVAL;
#endif
	} else {

		kni->tx_q = phys_to_virt(dev_info.tx_phys);
		kni->rx_q = phys_to_virt(dev_info.rx_phys);
		kni->alloc_q = phys_to_virt(dev_info.alloc_phys);
		kni->free_q = phys_to_virt(dev_info.free_phys);

		kni->req_q = phys_to_virt(dev_info.req_phys);
		kni->resp_q = phys_to_virt(dev_info.resp_phys);
		kni->sync_va = dev_info.sync_va;
		kni->sync_kva = phys_to_virt(dev_info.sync_phys);
		kni->iova_mode = 0;
	}

	kni->mbuf_size = dev_info.mbuf_size;

	pr_debug("tx_phys:      0x%016llx, tx_q addr:      0x%p\n",
		(unsigned long long) dev_info.tx_phys, kni->tx_q);
	pr_debug("rx_phys:      0x%016llx, rx_q addr:      0x%p\n",
		(unsigned long long) dev_info.rx_phys, kni->rx_q);
	pr_debug("alloc_phys:   0x%016llx, alloc_q addr:   0x%p\n",
		(unsigned long long) dev_info.alloc_phys, kni->alloc_q);
	pr_debug("free_phys:    0x%016llx, free_q addr:    0x%p\n",
		(unsigned long long) dev_info.free_phys, kni->free_q);
	pr_debug("req_phys:     0x%016llx, req_q addr:     0x%p\n",
		(unsigned long long) dev_info.req_phys, kni->req_q);
	pr_debug("resp_phys:    0x%016llx, resp_q addr:    0x%p\n",
		(unsigned long long) dev_info.resp_phys, kni->resp_q);
	pr_debug("mbuf_size:    %u\n", kni->mbuf_size);

	/* if user has provided a valid mac address */
	if (is_valid_ether_addr(dev_info.mac_addr))
		memcpy(net_dev->dev_addr, dev_info.mac_addr, ETH_ALEN);
	else
		/* Generate random MAC address. */
		eth_random_addr(net_dev->dev_addr);

	if (dev_info.mtu)
		net_dev->mtu = dev_info.mtu;
#ifdef HAVE_MAX_MTU_PARAM
	net_dev->max_mtu = net_dev->mtu;

	if (dev_info.min_mtu)
		net_dev->min_mtu = dev_info.min_mtu;

	if (dev_info.max_mtu)
		net_dev->max_mtu = dev_info.max_mtu;
#endif

	ret = register_netdev(net_dev);
	if (ret) {
		pr_err("error %i registering device \"%s\"\n",
					ret, dev_info.name);
		kni->net_dev = NULL;
		kni_dev_remove(kni);
		free_netdev(net_dev);
		return -ENODEV;
	}

	netif_carrier_off(net_dev);

	ret = kni_run_thread(knet, kni, dev_info.force_bind);
	if (ret != 0)
		return ret;

	down_write(&knet->kni_list_lock);
	list_add(&kni->list, &knet->kni_list_head);
	up_write(&knet->kni_list_lock);

	return 0;
}

static int
kni_ioctl_release(struct net *net, uint32_t ioctl_num,
		unsigned long ioctl_param)
{
	struct kni_net *knet = net_generic(net, kni_net_id);
	int ret = -EINVAL;
	struct kni_dev *dev, *n;
	struct rte_kni_device_info dev_info;

	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
		return -EINVAL;

	if (copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info)))
		return -EFAULT;

	/* Release the network device according to its name */
	if (strlen(dev_info.name) == 0)
		return -EINVAL;

	down_write(&knet->kni_list_lock);
	list_for_each_entry_safe(dev, n, &knet->kni_list_head, list) {
		if (strncmp(dev->name, dev_info.name, RTE_KNI_NAMESIZE) != 0)
			continue;

		if (multiple_kthread_on && dev->pthread != NULL) {
			kthread_stop(dev->pthread);
			dev->pthread = NULL;
		}

		list_del(&dev->list);
		kni_dev_remove(dev);
		ret = 0;
		break;
	}
	up_write(&knet->kni_list_lock);
	pr_info("%s release kni named %s\n",
		(ret == 0 ? "Successfully" : "Unsuccessfully"), dev_info.name);

	return ret;
}

static long
kni_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	long ret = -EINVAL;
	struct net *net = current->nsproxy->net_ns;

	pr_debug("IOCTL num=0x%0x param=0x%0lx\n", ioctl_num, ioctl_param);

	/*
	 * Switch according to the ioctl called
	 */
	switch (_IOC_NR(ioctl_num)) {
	case _IOC_NR(RTE_KNI_IOCTL_TEST):
		/* For test only, not used */
		break;
	case _IOC_NR(RTE_KNI_IOCTL_CREATE):
		ret = kni_ioctl_create(net, ioctl_num, ioctl_param);
		break;
	case _IOC_NR(RTE_KNI_IOCTL_RELEASE):
		ret = kni_ioctl_release(net, ioctl_num, ioctl_param);
		break;
	default:
		pr_debug("IOCTL default\n");
		break;
	}

	return ret;
}

static long
kni_compat_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param)
{
	/* 32 bits app on 64 bits OS to be supported later */
	pr_debug("Not implemented.\n");

	return -EINVAL;
}

static const struct file_operations kni_fops = {
	.owner = THIS_MODULE,
	.open = kni_open,
	.release = kni_release,
	.unlocked_ioctl = kni_ioctl,
	.compat_ioctl = kni_compat_ioctl,
};

static struct miscdevice kni_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KNI_DEVICE,
	.fops = &kni_fops,
};

static int __init
kni_parse_kthread_mode(void)
{
	if (!kthread_mode)
		return 0;

	if (strcmp(kthread_mode, "single") == 0)
		return 0;
	else if (strcmp(kthread_mode, "multiple") == 0)
		multiple_kthread_on = 1;
	else
		return -1;

	return 0;
}

static int __init
kni_parse_carrier_state(void)
{
	if (!carrier) {
		kni_dflt_carrier = 0;
		return 0;
	}

	if (strcmp(carrier, "off") == 0)
		kni_dflt_carrier = 0;
	else if (strcmp(carrier, "on") == 0)
		kni_dflt_carrier = 1;
	else
		return -1;

	return 0;
}

static int __init
kni_parse_bifurcated_support(void)
{
	if (!enable_bifurcated) {
		bifurcated_support = 0;
		return 0;
	}

	if (strcmp(enable_bifurcated, "on") == 0)
		bifurcated_support = 1;
	else
		return -1;

	return 0;
}

static int __init
kni_init(void)
{
	int rc;

	if (kni_parse_kthread_mode() < 0) {
		pr_err("Invalid parameter for kthread_mode\n");
		return -EINVAL;
	}

	if (multiple_kthread_on == 0)
		pr_debug("Single kernel thread for all KNI devices\n");
	else
		pr_debug("Multiple kernel thread mode enabled\n");

	if (kni_parse_carrier_state() < 0) {
		pr_err("Invalid parameter for carrier\n");
		return -EINVAL;
	}

	if (kni_dflt_carrier == 0)
		pr_debug("Default carrier state set to off.\n");
	else
		pr_debug("Default carrier state set to on.\n");

	if (kni_parse_bifurcated_support() < 0) {
		pr_err("Invalid parameter for bifurcated support\n");
		return -EINVAL;
	}
	if (bifurcated_support == 1)
		pr_debug("bifurcated support is enabled.\n");

	if (min_scheduling_interval < 0 || max_scheduling_interval < 0 ||
		min_scheduling_interval > KNI_KTHREAD_MAX_RESCHEDULE_INTERVAL ||
		max_scheduling_interval > KNI_KTHREAD_MAX_RESCHEDULE_INTERVAL ||
		min_scheduling_interval >= max_scheduling_interval) {
		pr_err("Invalid parameters for scheduling interval\n");
		return -EINVAL;
	}

#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	rc = register_pernet_subsys(&kni_net_ops);
#else
	rc = register_pernet_gen_subsys(&kni_net_id, &kni_net_ops);
#endif
	if (rc)
		return -EPERM;

	rc = misc_register(&kni_misc);
	if (rc != 0) {
		pr_err("Misc registration failed\n");
		goto out;
	}

	/* Configure the lo mode according to the input parameter */
	kni_net_config_lo_mode(lo_mode);

	return 0;

out:
#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	unregister_pernet_subsys(&kni_net_ops);
#else
	unregister_pernet_gen_subsys(kni_net_id, &kni_net_ops);
#endif
	return rc;
}

static void __exit
kni_exit(void)
{
	misc_deregister(&kni_misc);
#ifdef HAVE_SIMPLIFIED_PERNET_OPERATIONS
	unregister_pernet_subsys(&kni_net_ops);
#else
	unregister_pernet_gen_subsys(kni_net_id, &kni_net_ops);
#endif
}

module_init(kni_init);
module_exit(kni_exit);

module_param(lo_mode, charp, 0644);
MODULE_PARM_DESC(lo_mode,
"KNI loopback mode (default=lo_mode_none):\n"
"\t\tlo_mode_none        Kernel loopback disabled\n"
"\t\tlo_mode_fifo        Enable kernel loopback with fifo\n"
"\t\tlo_mode_fifo_skb    Enable kernel loopback with fifo and skb buffer\n"
"\t\t"
);

module_param(kthread_mode, charp, 0644);
MODULE_PARM_DESC(kthread_mode,
"Kernel thread mode (default=single):\n"
"\t\tsingle    Single kernel thread mode enabled.\n"
"\t\tmultiple  Multiple kernel thread mode enabled.\n"
"\t\t"
);

module_param(carrier, charp, 0644);
MODULE_PARM_DESC(carrier,
"Default carrier state for KNI interface (default=off):\n"
"\t\toff   Interfaces will be created with carrier state set to off.\n"
"\t\ton    Interfaces will be created with carrier state set to on.\n"
"\t\t"
);

module_param(enable_bifurcated, charp, 0644);
MODULE_PARM_DESC(enable_bifurcated,
"Enable request processing support for bifurcated drivers, "
"which means releasing rtnl_lock before calling userspace callback and "
"supporting async requests (default=off):\n"
"\t\ton    Enable request processing support for bifurcated drivers.\n"
"\t\t"
);

module_param(min_scheduling_interval, long, 0644);
MODULE_PARM_DESC(min_scheduling_interval,
"KNI thread min scheduling interval (default=100 microseconds)"
);

module_param(max_scheduling_interval, long, 0644);
MODULE_PARM_DESC(max_scheduling_interval,
"KNI thread max scheduling interval (default=200 microseconds)"
);
