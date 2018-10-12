/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <net/if.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_security_driver.h>
#include <rte_hexdump.h>

#include <caam_jr_log.h>

#define CRYPTODEV_NAME_CAAM_JR_PMD	crypto_caam_jr
static uint8_t cryptodev_driver_id;
int caam_jr_logtype;

/*
 * @brief Release the resources used by the SEC user space driver.
 *
 * Reset and release SEC's job rings indicated by the User Application at
 * init_job_ring() and free any memory allocated internally.
 * Call once during application tear down.
 *
 * @note In case there are any descriptors in-flight (descriptors received by
 * SEC driver for processing and for which no response was yet provided to UA),
 * the descriptors are discarded without any notifications to User Application.
 *
 * @retval ::0			is returned for a successful execution
 * @retval ::-1		is returned if SEC driver release is in progress
 */
static int
caam_jr_dev_uninit(struct rte_cryptodev *dev)
{
	if (dev == NULL)
		return -ENODEV;

	CAAM_JR_INFO("Closing crypto device %s", dev->data->name);

	return 0;
}

static int
caam_jr_dev_init(const char *name,
		 struct rte_vdev_device *vdev,
		 struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;

	PMD_INIT_FUNC_TRACE();

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		CAAM_JR_ERR("failed to create cryptodev vdev");
		goto cleanup;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = NULL;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		CAAM_JR_WARN("Device already init by primary process");
		return 0;
	}

	RTE_LOG(INFO, PMD, "%s cryptodev init\n", dev->data->name);

	return 0;

cleanup:
	CAAM_JR_ERR("driver %s: cryptodev_caam_jr_create failed",
			init_params->name);

	return -ENXIO;
}

/** Initialise CAAM JR crypto device */
static int
cryptodev_caam_jr_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		128,
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

	return caam_jr_dev_init(name, vdev, &init_params);
}

/** Uninitialise CAAM JR crypto device */
static int
cryptodev_caam_jr_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	caam_jr_dev_uninit(cryptodev);

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_caam_jr_drv = {
	.probe = cryptodev_caam_jr_probe,
	.remove = cryptodev_caam_jr_remove
};

static struct cryptodev_driver caam_jr_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CAAM_JR_PMD, cryptodev_caam_jr_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CAAM_JR_PMD,
	"max_nb_queue_pairs=<int>"
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(caam_jr_crypto_drv, cryptodev_caam_jr_drv.driver,
		cryptodev_driver_id);

RTE_INIT(caam_jr_init_log)
{
	caam_jr_logtype = rte_log_register("pmd.crypto.caam");
	if (caam_jr_logtype >= 0)
		rte_log_set_level(caam_jr_logtype, RTE_LOG_NOTICE);
}
