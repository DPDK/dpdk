/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <bus_pci_driver.h>
#include <rte_common.h>
#include <dev_driver.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_bphy_irq.h"
#include "rte_pmd_bphy.h"

static const struct rte_pci_id pci_bphy_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_BPHY)},
	{
		.vendor_id = 0,
	},
};

struct bphy_test {
	int irq_num;
	cnxk_bphy_intr_handler_t handler;
	void *data;
	int cpu;
	bool handled_intr;
	int handled_data;
	int test_data;
};

static struct bphy_test *test;

static void
bphy_test_handler_fn(int irq_num, void *isr_data)
{
	test[irq_num].handled_intr = true;
	test[irq_num].handled_data = *((int *)isr_data);
}

int
rte_pmd_bphy_npa_pf_func_get_rmt(uint16_t *pf_func)
{
	*pf_func = roc_bphy_npa_pf_func_get();

	return 0;
}

int
rte_pmd_bphy_sso_pf_func_get_rmt(uint16_t *pf_func)
{
	*pf_func = roc_bphy_sso_pf_func_get();

	return 0;
}

static int
bphy_rawdev_selftest(uint16_t dev_id)
{
	unsigned int i, queues, descs;
	uint16_t pf_func;
	uint64_t max_irq;
	int ret;

	queues = rte_rawdev_queue_count(dev_id);
	if (queues == 0)
		return -ENODEV;
	if (queues != BPHY_QUEUE_CNT)
		return -EINVAL;

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	ret = rte_rawdev_queue_conf_get(dev_id, CNXK_BPHY_DEF_QUEUE, &descs,
					sizeof(descs));
	if (ret)
		goto err_desc;
	if (descs != 1) {
		ret = -ENODEV;
		plt_err("Wrong number of descs reported\n");
		goto err_desc;
	}

	ret = rte_pmd_bphy_npa_pf_func_get(dev_id, &pf_func);
	if (ret || pf_func == 0)
		plt_warn("NPA pf_func is invalid");

	ret = rte_pmd_bphy_sso_pf_func_get(dev_id, &pf_func);
	if (ret || pf_func == 0)
		plt_warn("SSO pf_func is invalid");

	ret = rte_pmd_bphy_intr_init(dev_id);
	if (ret) {
		plt_err("intr init failed");
		return ret;
	}

	max_irq = cnxk_bphy_irq_max_get(dev_id);

	test = rte_zmalloc("BPHY", max_irq * sizeof(*test), 0);
	if (test == NULL) {
		plt_err("intr alloc failed");
		goto err_alloc;
	}

	for (i = 0; i < max_irq; i++) {
		test[i].test_data = i;
		test[i].irq_num = i;
		test[i].handler = bphy_test_handler_fn;
		test[i].data = &test[i].test_data;
	}

	for (i = 0; i < max_irq; i++) {
		ret = rte_pmd_bphy_intr_register(dev_id, test[i].irq_num,
						 test[i].handler, test[i].data,
						 0);
		if (ret == -ENOTSUP) {
			/* In the test we iterate over all irq numbers
			 * so if some of them are not supported by given
			 * platform we treat respective results as valid
			 * ones. This way they have no impact on overall
			 * test results.
			 */
			test[i].handled_intr = true;
			test[i].handled_data = test[i].test_data;
			ret = 0;
			continue;
		}

		if (ret) {
			plt_err("intr register failed at irq %d", i);
			goto err_register;
		}
	}

	for (i = 0; i < max_irq; i++)
		roc_bphy_intr_handler(i);

	for (i = 0; i < max_irq; i++) {
		if (!test[i].handled_intr) {
			plt_err("intr %u not handled", i);
			ret = -1;
			break;
		}
		if (test[i].handled_data != test[i].test_data) {
			plt_err("intr %u has wrong handler", i);
			ret = -1;
			break;
		}
	}

err_register:
	/*
	 * In case of registration failure the loop goes over all
	 * interrupts which is safe due to internal guards in
	 * rte_pmd_bphy_intr_unregister().
	 */
	for (i = 0; i < max_irq; i++)
		rte_pmd_bphy_intr_unregister(dev_id, i);

	rte_free(test);
err_alloc:
	rte_pmd_bphy_intr_fini(dev_id);
err_desc:
	rte_rawdev_stop(dev_id);

	return ret;
}

static void
bphy_rawdev_get_name(char *name, struct rte_pci_device *pci_dev)
{
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "BPHY:%02x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);
}

static int
cnxk_bphy_irq_enqueue_bufs(struct rte_rawdev *dev,
			   struct rte_rawdev_buf **buffers, unsigned int count,
			   rte_rawdev_obj_t context)
{
	struct bphy_device *bphy_dev = (struct bphy_device *)dev->dev_private;
	struct cnxk_bphy_irq_msg *msg = buffers[0]->buf_addr;
	struct bphy_irq_queue *qp = &bphy_dev->queues[0];
	unsigned int queue = (size_t)context;
	struct cnxk_bphy_irq_info *info;
	struct cnxk_bphy_mem *mem;
	uint16_t *pf_func;
	void *rsp = NULL;
	int ret;

	if (queue >= RTE_DIM(bphy_dev->queues))
		return -EINVAL;

	if (count == 0)
		return 0;

	switch (msg->type) {
	case CNXK_BPHY_IRQ_MSG_TYPE_INIT:
		ret = cnxk_bphy_intr_init(dev->dev_id);
		if (ret)
			return ret;
		break;
	case CNXK_BPHY_IRQ_MSG_TYPE_FINI:
		cnxk_bphy_intr_fini(dev->dev_id);
		break;
	case CNXK_BPHY_IRQ_MSG_TYPE_REGISTER:
		info = (struct cnxk_bphy_irq_info *)msg->data;
		ret = cnxk_bphy_intr_register(dev->dev_id, info->irq_num,
					      info->handler, info->data,
					      info->cpu);
		if (ret)
			return ret;
		break;
	case CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER:
		info = (struct cnxk_bphy_irq_info *)msg->data;
		cnxk_bphy_intr_unregister(dev->dev_id, info->irq_num);
		break;
	case CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET:
		mem = rte_zmalloc(NULL, sizeof(*mem), 0);
		if (!mem)
			return -ENOMEM;

		*mem = bphy_dev->mem;
		rsp = mem;
		break;
	case CNXK_BPHY_MSG_TYPE_NPA_PF_FUNC:
		pf_func = rte_malloc(NULL, sizeof(*pf_func), 0);
		if (!pf_func)
			return -ENOMEM;

		*pf_func = roc_bphy_npa_pf_func_get();
		rsp = pf_func;
		break;
	case CNXK_BPHY_MSG_TYPE_SSO_PF_FUNC:
		pf_func = rte_malloc(NULL, sizeof(*pf_func), 0);
		if (!pf_func)
			return -ENOMEM;

		*pf_func = roc_bphy_sso_pf_func_get();
		rsp = pf_func;
		break;
	default:
		return -EINVAL;
	}

	/* get rid of last response if any */
	if (qp->rsp) {
		RTE_LOG(WARNING, PMD, "Previous response got overwritten\n");
		rte_free(qp->rsp);
	}
	qp->rsp = rsp;

	return 1;
}

static int
cnxk_bphy_irq_dequeue_bufs(struct rte_rawdev *dev,
			   struct rte_rawdev_buf **buffers, unsigned int count,
			   rte_rawdev_obj_t context)
{
	struct bphy_device *bphy_dev = (struct bphy_device *)dev->dev_private;
	unsigned int queue = (size_t)context;
	struct bphy_irq_queue *qp;

	if (queue >= RTE_DIM(bphy_dev->queues))
		return -EINVAL;

	if (count == 0)
		return 0;

	qp = &bphy_dev->queues[queue];
	if (qp->rsp) {
		buffers[0]->buf_addr = qp->rsp;
		qp->rsp = NULL;

		return 1;
	}

	return 0;
}

static uint16_t
cnxk_bphy_irq_queue_count(struct rte_rawdev *dev)
{
	struct bphy_device *bphy_dev = (struct bphy_device *)dev->dev_private;

	return RTE_DIM(bphy_dev->queues);
}

static int
cnxk_bphy_irq_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			     rte_rawdev_obj_t queue_conf,
			     size_t queue_conf_size)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	if (queue_conf_size != sizeof(unsigned int))
		return -EINVAL;

	*(unsigned int *)queue_conf = 1;

	return 0;
}

static const struct rte_rawdev_ops bphy_rawdev_ops = {
	.queue_def_conf = cnxk_bphy_irq_queue_def_conf,
	.enqueue_bufs = cnxk_bphy_irq_enqueue_bufs,
	.dequeue_bufs = cnxk_bphy_irq_dequeue_bufs,
	.queue_count = cnxk_bphy_irq_queue_count,
	.dev_selftest = bphy_rawdev_selftest,
};

static int
bphy_rawdev_probe(struct rte_pci_driver *pci_drv,
		  struct rte_pci_device *pci_dev)
{
	struct bphy_device *bphy_dev = NULL;
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *bphy_rawdev;
	int ret;

	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->mem_resource[0].addr) {
		plt_err("BARs have invalid values: BAR0 %p\n BAR2 %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	ret = roc_plt_init();
	if (ret)
		return ret;

	bphy_rawdev_get_name(name, pci_dev);
	bphy_rawdev = rte_rawdev_pmd_allocate(name, sizeof(*bphy_dev),
					      rte_socket_id());
	if (bphy_rawdev == NULL) {
		plt_err("Failed to allocate rawdev");
		return -ENOMEM;
	}

	bphy_rawdev->dev_ops = &bphy_rawdev_ops;
	bphy_rawdev->device = &pci_dev->device;
	bphy_rawdev->driver_name = pci_dev->driver->driver.name;

	bphy_dev = (struct bphy_device *)bphy_rawdev->dev_private;
	bphy_dev->mem.res0 = pci_dev->mem_resource[0];
	bphy_dev->mem.res2 = pci_dev->mem_resource[2];
	bphy_dev->bphy.pci_dev = pci_dev;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = roc_bphy_dev_init(&bphy_dev->bphy);
		if (ret) {
			rte_rawdev_pmd_release(bphy_rawdev);
			return ret;
		}
	}

	return 0;
}

static int
bphy_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct bphy_device *bphy_dev;
	struct rte_rawdev *rawdev;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev == NULL) {
		plt_err("invalid pci_dev");
		return -EINVAL;
	}

	bphy_rawdev_get_name(name, pci_dev);
	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		plt_err("invalid device name (%s)", name);
		return -EINVAL;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		bphy_dev = (struct bphy_device *)rawdev->dev_private;
		roc_bphy_dev_fini(&bphy_dev->bphy);
	}

	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver cnxk_bphy_rawdev_pmd = {
	.id_table = pci_bphy_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = bphy_rawdev_probe,
	.remove = bphy_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(bphy_rawdev_pci_driver, cnxk_bphy_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(bphy_rawdev_pci_driver, pci_bphy_map);
RTE_PMD_REGISTER_KMOD_DEP(bphy_rawdev_pci_driver, "vfio-pci");
