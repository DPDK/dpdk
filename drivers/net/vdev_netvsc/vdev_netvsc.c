/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd.
 */

#include <stddef.h>

#include <rte_bus_vdev.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_kvargs.h>
#include <rte_log.h>

#define VDEV_NETVSC_DRIVER net_vdev_netvsc
#define VDEV_NETVSC_ARG_IFACE "iface"
#define VDEV_NETVSC_ARG_MAC "mac"

#define DRV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		vdev_netvsc_logtype, \
		RTE_FMT(RTE_STR(VDEV_NETVSC_DRIVER) ": " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
		RTE_FMT_TAIL(__VA_ARGS__,)))

/** Driver-specific log messages type. */
static int vdev_netvsc_logtype;

/** Number of driver instances relying on context list. */
static unsigned int vdev_netvsc_ctx_inst;

/**
 * Probe NetVSC interfaces.
 *
 * @param dev
 *   Virtual device context for driver instance.
 *
 * @return
 *    Always 0, even in case of errors.
 */
static int
vdev_netvsc_vdev_probe(struct rte_vdev_device *dev)
{
	static const char *const vdev_netvsc_arg[] = {
		VDEV_NETVSC_ARG_IFACE,
		VDEV_NETVSC_ARG_MAC,
		NULL,
	};
	const char *name = rte_vdev_device_name(dev);
	const char *args = rte_vdev_device_args(dev);
	struct rte_kvargs *kvargs = rte_kvargs_parse(args ? args : "",
						     vdev_netvsc_arg);

	DRV_LOG(DEBUG, "invoked as \"%s\", using arguments \"%s\"", name, args);
	if (!kvargs) {
		DRV_LOG(ERR, "cannot parse arguments list");
		goto error;
	}
error:
	if (kvargs)
		rte_kvargs_free(kvargs);
	++vdev_netvsc_ctx_inst;
	return 0;
}

/**
 * Remove driver instance.
 *
 * @param dev
 *   Virtual device context for driver instance.
 *
 * @return
 *   Always 0.
 */
static int
vdev_netvsc_vdev_remove(__rte_unused struct rte_vdev_device *dev)
{
	--vdev_netvsc_ctx_inst;
	return 0;
}

/** Virtual device descriptor. */
static struct rte_vdev_driver vdev_netvsc_vdev = {
	.probe = vdev_netvsc_vdev_probe,
	.remove = vdev_netvsc_vdev_remove,
};

RTE_PMD_REGISTER_VDEV(VDEV_NETVSC_DRIVER, vdev_netvsc_vdev);
RTE_PMD_REGISTER_ALIAS(VDEV_NETVSC_DRIVER, eth_vdev_netvsc);
RTE_PMD_REGISTER_PARAM_STRING(net_vdev_netvsc,
			      VDEV_NETVSC_ARG_IFACE "=<string> "
			      VDEV_NETVSC_ARG_MAC "=<string>");

/** Initialize driver log type. */
RTE_INIT(vdev_netvsc_init_log)
{
	vdev_netvsc_logtype = rte_log_register("pmd.vdev_netvsc");
	if (vdev_netvsc_logtype >= 0)
		rte_log_set_level(vdev_netvsc_logtype, RTE_LOG_NOTICE);
}
