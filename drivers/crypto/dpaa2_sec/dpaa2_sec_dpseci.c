/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
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
 *     * Neither the name of  Freescale Semiconductor, Inc nor the names of its
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
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_common.h>
#include <rte_fslmc.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <fsl_dpseci.h>
#include <fsl_mc_sys.h>

#include "dpaa2_sec_priv.h"
#include "dpaa2_sec_logs.h"

#define FSL_VENDOR_ID           0x1957
#define FSL_DEVICE_ID           0x410
#define FSL_SUBSYSTEM_SEC       1
#define FSL_MC_DPSECI_DEVID     3

static int
dpaa2_sec_dev_configure(struct rte_cryptodev *dev __rte_unused,
			struct rte_cryptodev_config *config __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOTSUP;
}

static int
dpaa2_sec_dev_start(struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	struct dpseci_attr attr;
	struct dpaa2_queue *dpaa2_q;
	struct dpaa2_sec_qp **qp = (struct dpaa2_sec_qp **)
					dev->data->queue_pairs;
	struct dpseci_rx_queue_attr rx_attr;
	struct dpseci_tx_queue_attr tx_attr;
	int ret, i;

	PMD_INIT_FUNC_TRACE();

	memset(&attr, 0, sizeof(struct dpseci_attr));

	ret = dpseci_enable(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "DPSECI with HW_ID = %d ENABLE FAILED\n",
			     priv->hw_id);
		goto get_attr_failure;
	}
	ret = dpseci_get_attributes(dpseci, CMD_PRI_LOW, priv->token, &attr);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "DPSEC ATTRIBUTE READ FAILED, disabling DPSEC\n");
		goto get_attr_failure;
	}
	for (i = 0; i < attr.num_rx_queues && qp[i]; i++) {
		dpaa2_q = &qp[i]->rx_vq;
		dpseci_get_rx_queue(dpseci, CMD_PRI_LOW, priv->token, i,
				    &rx_attr);
		dpaa2_q->fqid = rx_attr.fqid;
		PMD_INIT_LOG(DEBUG, "rx_fqid: %d", dpaa2_q->fqid);
	}
	for (i = 0; i < attr.num_tx_queues && qp[i]; i++) {
		dpaa2_q = &qp[i]->tx_vq;
		dpseci_get_tx_queue(dpseci, CMD_PRI_LOW, priv->token, i,
				    &tx_attr);
		dpaa2_q->fqid = tx_attr.fqid;
		PMD_INIT_LOG(DEBUG, "tx_fqid: %d", dpaa2_q->fqid);
	}

	return 0;
get_attr_failure:
	dpseci_disable(dpseci, CMD_PRI_LOW, priv->token);
	return -1;
}

static void
dpaa2_sec_dev_stop(struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = dpseci_disable(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure in disabling dpseci %d device",
			     priv->hw_id);
		return;
	}

	ret = dpseci_reset(dpseci, CMD_PRI_LOW, priv->token);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "SEC Device cannot be reset:Error = %0x\n",
			     ret);
		return;
	}
}

static int
dpaa2_sec_dev_close(struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Function is reverse of dpaa2_sec_dev_init.
	 * It does the following:
	 * 1. Detach a DPSECI from attached resources i.e. buffer pools, dpbp_id
	 * 2. Close the DPSECI device
	 * 3. Free the allocated resources.
	 */

	/*Close the device at underlying layer*/
	ret = dpseci_close(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure closing dpseci device with"
			     " error code %d\n", ret);
		return -1;
	}

	/*Free the allocated memory for ethernet private data and dpseci*/
	priv->hw = NULL;
	free(dpseci);

	return 0;
}

static void
dpaa2_sec_dev_infos_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_info *info)
{
	struct dpaa2_sec_dev_private *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		info->feature_flags = dev->feature_flags;
		info->capabilities = dpaa2_sec_capabilities;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->dev_type = RTE_CRYPTODEV_DPAA2_SEC_PMD;
	}
}

static struct rte_cryptodev_ops crypto_ops = {
	.dev_configure	      = dpaa2_sec_dev_configure,
	.dev_start	      = dpaa2_sec_dev_start,
	.dev_stop	      = dpaa2_sec_dev_stop,
	.dev_close	      = dpaa2_sec_dev_close,
	.dev_infos_get        = dpaa2_sec_dev_infos_get,
};

static int
dpaa2_sec_uninit(const struct rte_cryptodev_driver *crypto_drv __rte_unused,
		 struct rte_cryptodev *dev)
{
	PMD_INIT_LOG(INFO, "Closing DPAA2_SEC device %s on numa socket %u\n",
		     dev->data->name, rte_socket_id());

	return 0;
}

static int
dpaa2_sec_dev_init(struct rte_cryptodev *cryptodev)
{
	struct dpaa2_sec_dev_private *internals;
	struct rte_device *dev = cryptodev->device;
	struct rte_dpaa2_device *dpaa2_dev;
	struct fsl_mc_io *dpseci;
	uint16_t token;
	struct dpseci_attr attr;
	int retcode, hw_id;

	PMD_INIT_FUNC_TRACE();
	dpaa2_dev = container_of(dev, struct rte_dpaa2_device, device);
	if (dpaa2_dev == NULL) {
		PMD_INIT_LOG(ERR, "dpaa2_device not found\n");
		return -1;
	}
	hw_id = dpaa2_dev->object_id;

	cryptodev->dev_type = RTE_CRYPTODEV_DPAA2_SEC_PMD;
	cryptodev->dev_ops = &crypto_ops;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;

	internals = cryptodev->data->dev_private;
	internals->max_nb_sessions = RTE_DPAA2_SEC_PMD_MAX_NB_SESSIONS;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_INIT_LOG(DEBUG, "Device already init by primary process");
		return 0;
	}
	/*Open the rte device via MC and save the handle for further use*/
	dpseci = (struct fsl_mc_io *)rte_calloc(NULL, 1,
				sizeof(struct fsl_mc_io), 0);
	if (!dpseci) {
		PMD_INIT_LOG(ERR,
			     "Error in allocating the memory for dpsec object");
		return -1;
	}
	dpseci->regs = rte_mcp_ptr_list[0];

	retcode = dpseci_open(dpseci, CMD_PRI_LOW, hw_id, &token);
	if (retcode != 0) {
		PMD_INIT_LOG(ERR, "Cannot open the dpsec device: Error = %x",
			     retcode);
		goto init_error;
	}
	retcode = dpseci_get_attributes(dpseci, CMD_PRI_LOW, token, &attr);
	if (retcode != 0) {
		PMD_INIT_LOG(ERR,
			     "Cannot get dpsec device attributed: Error = %x",
			     retcode);
		goto init_error;
	}
	sprintf(cryptodev->data->name, "dpsec-%u", hw_id);

	internals->max_nb_queue_pairs = attr.num_tx_queues;
	cryptodev->data->nb_queue_pairs = internals->max_nb_queue_pairs;
	internals->hw = dpseci;
	internals->token = token;

	PMD_INIT_LOG(DEBUG, "driver %s: created\n", cryptodev->data->name);
	return 0;

init_error:
	PMD_INIT_LOG(ERR, "driver %s: create failed\n", cryptodev->data->name);

	/* dpaa2_sec_uninit(crypto_dev_name); */
	return -EFAULT;
}

static int
cryptodev_dpaa2_sec_probe(struct rte_dpaa2_driver *dpaa2_drv,
			  struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	int retval;

	sprintf(cryptodev_name, "dpsec-%d", dpaa2_dev->object_id);

	cryptodev = rte_cryptodev_pmd_allocate(cryptodev_name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private = rte_zmalloc_socket(
					"cryptodev private structure",
					sizeof(struct dpaa2_sec_dev_private),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
					"device data");
	}

	dpaa2_dev->cryptodev = cryptodev;
	cryptodev->device = &dpaa2_dev->device;
	cryptodev->driver = (struct rte_cryptodev_driver *)dpaa2_drv;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	/* Invoke PMD device initialization function */
	retval = dpaa2_sec_dev_init(cryptodev);
	if (retval == 0)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;

	return -ENXIO;
}

static int
cryptodev_dpaa2_sec_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_cryptodev *cryptodev;
	int ret;

	cryptodev = dpaa2_dev->cryptodev;
	if (cryptodev == NULL)
		return -ENODEV;

	ret = dpaa2_sec_uninit(NULL, cryptodev);
	if (ret)
		return ret;

	/* free crypto device */
	rte_cryptodev_pmd_release_device(cryptodev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->device = NULL;
	cryptodev->driver = NULL;
	cryptodev->data = NULL;

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_sec_driver = {
	.drv_type = DPAA2_MC_DPSECI_DEVID,
	.driver = {
		.name = "DPAA2 SEC PMD"
	},
	.probe = cryptodev_dpaa2_sec_probe,
	.remove = cryptodev_dpaa2_sec_remove,
};

RTE_PMD_REGISTER_DPAA2(dpaa2_sec_pmd, rte_dpaa2_sec_driver);
