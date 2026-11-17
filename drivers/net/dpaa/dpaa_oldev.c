/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2026 NXP
 */

#include <stdio.h>
#include <sys/ioctl.h>

#include <rte_dpaa_logs.h>
#include <rte_kvargs.h>

#include <rte_string_fns.h>
#include <bus_dpaa_driver.h>
#include <dpaa_ethdev.h>
#include <dpaa_rxtx.h>
#include <dpaa_mempool.h>

#include <eal_export.h>
#include <fsl_fman.h>

#include "rte_pmd_dpaa_oldev.h"

/* The ol device is a tx are queue pair, which is provided to the kernel through
 * ioctl calls. It is exposed as a virtual port to the application.
 * This ASK Device provides IOCTL calls to driver for communicating fqid's,
 * buffer pool id's and channel id's with kernel.
 */
#define ASK_PATH		"/dev/cdx_ctrl"
#define CDX_IOC_MAGIC		0xbe

/* Private area reserved by driver.
 * Aligned with "struct annotations_t". Parse results will be written from
 * the 17th byte by the HW.
 */
#define DPAA_OL_PARSERSLT_START_OFFSET 16
#define DPAA_OL_DATA_ALIGNMENT 64
/* Change this value to increase the data offset.
 * final value will be aligned by DPAA_OL_DATA_ALIGNMENT
 */
#define DPAA_OL_MAX_EXTRA_SIZE 0
#define DPAA_OL_PARSE_RESULT_REQUIRED true
#define DPAA_OL_TIME_STAMP_REQUIRED false
#define DPAA_OL_HASH_RESULT_REQUIRED false
#define DPAA_OL_PCD_INFO_REQUIRED false

/* Work queue ID used for the offline port ingress sequence classifier. */
#define DPA_ISC_WQ_ID 2

static const struct eth_dev_ops dpaa_ol_devops;

struct ask_ctrl_offline_channel {
	uint32_t channel_id;
	uint32_t ctx_a_hi_val;	/* Setting required flags(B0,A2) while creating
				 * frame queue.
				 */
	uint32_t ctx_a_lo_val;	/* EBD and VSP enable bits */
	uint32_t ctx_b_val;	/* VSP ID */
};

struct ask_ctrl_dpdk_fq_info_s {
	uint32_t tx_fq_id;		/* DPDK transmits GTP packets using FQ
					 * ID
					 */
	uint32_t rx_fq_id;		/* DPDK receives packets from FMAN */
	uint16_t buff_size;		/* Size of each buffer in DPDPK buffer
					 * pool
					 */
	uint8_t bp_id;			/* DPDK buffer pool id */
	/* BH port interface name; set from the mandatory drv_bh_port
	 * devarg (see DRIVER_OL_BH_PORT).
	 */
	uint8_t bh_port_name[DPAA_OL_MAX_BH_PORT_NAME_LEN];
	uint8_t pad0;
	/* below fields are taken from structure t_FmBufferPrefixContent, These
	 * fields are required for VSP creation on DPDK buffer pool ID. These
	 * fields should be set based on expected parameters from FMAN
	 */
	uint16_t privDataSize;		/* Number of bytes to be left at the
					 * beginning of the external buffer;
					 * Note that the private-area will start
					 * from the base of the buffer address.
					 */
	uint8_t passPrsResult;		/* TRUE to pass the parse result to/from
					 * the FM; User may use
					 * FM_PORT_GetBufferPrsResult() in order
					 * to get the parser-result from a
					 * buffer.
					 */
	uint8_t passTimeStamp;		/* < TRUE to pass the timeStamp to/from
					 * the FM User may use
					 * FM_PORT_GetBufferTimeStamp() in order
					 * to get the parser-result from a
					 * buffer.
					 */
	uint8_t passHashResult;		/* TRUE to pass the KG hash result
					 * to/from the FM User may use
					 * FM_PORT_GetBufferHashResult() in
					 * order to get the parser-result from a
					 * buffer.
					 */
	uint8_t passAllOtherPCDInfo;	/* Add all other Internal-Context
					 * information: AD, hash-result, key,
					 * etc.
					 */
	uint16_t dataAlign;		/* 0 to use driver's default alignment
					 * [DEFAULT_FM_SP_bufferPrefixContent_dataAlign],
					 * other value for selecting a data
					 * alignment (must be a power of 2); if
					 * write optimization is used, must be
					 * >= 16.
					 */
	uint8_t manipExtraSpace;	/* Maximum extra size needed (insertion-
					 * size minus removal-size); Note that
					 * this field impacts the size of the
					 * buffer-prefix (i.e. it pushes the
					 * data offset); This field is
					 * irrelevant if DPAA_VERSION==10
					 */
	uint8_t pad1[3];
};

/* Layout is shared with the CDX kernel module. */
static_assert(sizeof(struct ask_ctrl_offline_channel) == 16,
	      "ask_ctrl_offline_channel ABI changed");
static_assert(sizeof(struct ask_ctrl_dpdk_fq_info_s) == 36,
	      "ask_ctrl_dpdk_fq_info_s ABI changed");
static_assert(sizeof(struct rte_pmd_dpaa_uplink_cls_info_s) == 44,
	      "rte_pmd_dpaa_uplink_cls_info_s ABI changed");
static_assert(sizeof(struct rte_pmd_dpaa_lgw_info_s) == 84,
	      "rte_pmd_dpaa_lgw_info_s ABI changed");

#define ASK_CTRL_GET_OFFLINE_CHANNEL_INFO \
	_IOWR(CDX_IOC_MAGIC, 8, struct ask_ctrl_offline_channel)
#define ASK_CTRL_SET_DPDK_INFO \
	_IOWR(CDX_IOC_MAGIC, 9, struct ask_ctrl_dpdk_fq_info_s)
#define ASK_CTRL_SET_CLASSIF_INFO \
	_IOWR(CDX_IOC_MAGIC, 10, struct rte_pmd_dpaa_uplink_cls_info_s)
#define ASK_CTRL_RESET_CLASSIF_INFO \
	_IOWR(CDX_IOC_MAGIC, 11, struct rte_pmd_dpaa_uplink_cls_info_s)
#define ASK_CTRL_SET_LGW_INFO \
	_IOWR(CDX_IOC_MAGIC, 12, struct rte_pmd_dpaa_lgw_info_s)
#define ASK_CTRL_RESET_LGW_INFO \
	_IOWR(CDX_IOC_MAGIC, 13, struct rte_pmd_dpaa_lgw_info_s)
/* Tells the kernel the Tx/Rx FQIDs set via ASK_CTRL_SET_DPDK_INFO are
 * being torn down.
 */
#define ASK_CTRL_RESET_DPDK_INFO \
	_IOWR(CDX_IOC_MAGIC, 14, struct ask_ctrl_dpdk_fq_info_s)

/* Validate port_id and return the rte_eth_dev if it belongs to this driver. */
static struct rte_eth_dev *
dpaa_ol_get_port(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		DPAA_PMD_ERR("Port %u is not a valid port", port_id);
		return NULL;
	}

	dev = &rte_eth_devices[port_id];
	if (dev->dev_ops != &dpaa_ol_devops) {
		DPAA_PMD_ERR("Port %u is not a DPAA offline (O/H) port", port_id);
		return NULL;
	}

	return dev;
}

static int check_fd(struct dpaa_if *dpaa_intf)
{
	return (dpaa_intf->ol_fd >= 0) ? dpaa_intf->ol_fd : -ENODEV;
}

static int ask_get_channel_info(struct dpaa_if *dpaa_intf,
				 struct ask_ctrl_offline_channel *ch_info)
{
	int ret, fd = check_fd(dpaa_intf);

	if (fd < 0)
		return fd;

	ret = ioctl(fd, ASK_CTRL_GET_OFFLINE_CHANNEL_INFO, ch_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Get channel info ioctl failed with errno: %s", strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Get channel info successful");
	DPAA_PMD_DEBUG("Channel id: %x", ch_info->channel_id);
	DPAA_PMD_DEBUG("ctx_a_hi_val: %x", ch_info->ctx_a_hi_val);
	DPAA_PMD_DEBUG("ctx_a_lo_val: %x", ch_info->ctx_a_lo_val);
	DPAA_PMD_DEBUG("ctx_b_val: %x", ch_info->ctx_b_val);

	return 0;
}

static int ask_set_fq_info(struct dpaa_if *dpaa_intf,
			    struct ask_ctrl_dpdk_fq_info_s *fq_info)
{
	int ret, fd = check_fd(dpaa_intf);

	if (fd < 0)
		return fd;

	ret = ioctl(fd, ASK_CTRL_SET_DPDK_INFO, fq_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Set FQ info ioctl failed with errno: %s", strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Set fq info successful");
	DPAA_PMD_DEBUG("Tx fqid: %x", fq_info->tx_fq_id);
	DPAA_PMD_DEBUG("Rx fqid: %x", fq_info->rx_fq_id);
	DPAA_PMD_DEBUG("buff_size: %d", fq_info->buff_size);
	DPAA_PMD_DEBUG("bp_id: %d", fq_info->bp_id);
	DPAA_PMD_DEBUG("Backhaul port name: %s", fq_info->bh_port_name);

	return 0;
}

static int ask_reset_fq_info(struct dpaa_if *dpaa_intf,
			      struct ask_ctrl_dpdk_fq_info_s *fq_info)
{
	int ret, fd = check_fd(dpaa_intf);

	if (fd < 0)
		return fd;

	ret = ioctl(fd, ASK_CTRL_RESET_DPDK_INFO, fq_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Reset FQ info ioctl failed with errno: %s", strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Reset fq info successful");

	return 0;
}

static int
dpaa_ol_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int dpaa_ol_dev_info(struct rte_eth_dev *dev,
			    struct rte_eth_dev_info *dev_info)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	dev_info->max_rx_queues = dpaa_intf->nb_rx_queues;
	dev_info->max_tx_queues = dpaa_intf->nb_tx_queues;
	dev_info->max_rx_pktlen = DPAA_MAX_RX_PKT_LEN;
	dev_info->max_mac_addrs = DPAA_MAX_MAC_FILTER;
	dev_info->max_hash_mac_addrs = 0;
	dev_info->max_vfs = 0;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10M_HD |
			       RTE_ETH_LINK_SPEED_10M |
			       RTE_ETH_LINK_SPEED_100M_HD |
			       RTE_ETH_LINK_SPEED_100M |
			       RTE_ETH_LINK_SPEED_1G |
			       RTE_ETH_LINK_SPEED_2_5G |
			       RTE_ETH_LINK_SPEED_10G;

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa_ol_set_classif_info, 26.11)
int rte_pmd_dpaa_ol_set_classif_info(uint16_t port_id,
		struct rte_pmd_dpaa_uplink_cls_info_s *classif_info)
{
	struct rte_eth_dev *dev;
	struct dpaa_if *dpaa_intf;
	int i, ret, fd;

	if (classif_info == NULL) {
		DPAA_PMD_ERR("No classification data available");
		return -EINVAL;
	}

	dev = dpaa_ol_get_port(port_id);
	if (dev == NULL)
		return -ENODEV;
	dpaa_intf = dev->data->dev_private;

	fd = check_fd(dpaa_intf);
	if (fd < 0)
		return fd;

	ret = ioctl(fd, ASK_CTRL_SET_CLASSIF_INFO, classif_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Set classification info ioctl failed with errno: %s",
			     strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Set classification info successful");
	for (i = 0; i < classif_info->num_ports; i++) {
		DPAA_PMD_DEBUG("UDP dest port: %d",
			       classif_info->gtp_udp_port[i]);
	}
	DPAA_PMD_DEBUG("Protocol ID: %d", classif_info->gtp_proto_id);

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa_ol_reset_classif_info, 26.11)
int rte_pmd_dpaa_ol_reset_classif_info(uint16_t port_id)
{
	struct rte_pmd_dpaa_uplink_cls_info_s classif_info;
	struct rte_eth_dev *dev;
	struct dpaa_if *dpaa_intf;
	int ret, fd;

	dev = dpaa_ol_get_port(port_id);
	if (dev == NULL)
		return -ENODEV;
	dpaa_intf = dev->data->dev_private;

	fd = check_fd(dpaa_intf);
	if (fd < 0)
		return fd;

	memset(&classif_info, 0, sizeof(classif_info));

	ret = ioctl(fd, ASK_CTRL_RESET_CLASSIF_INFO, &classif_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Reset classification info ioctl failed with errno: %s",
			     strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Reset classification info successful");

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa_ol_set_lgw_info, 26.11)
int rte_pmd_dpaa_ol_set_lgw_info(uint16_t port_id,
		struct rte_pmd_dpaa_lgw_info_s *lgw_info)
{
	struct rte_eth_dev *dev;
	struct dpaa_if *dpaa_intf;
	int ret, fd;

	if (lgw_info == NULL) {
		DPAA_PMD_ERR("No LGW data available");
		return -EINVAL;
	}

	dev = dpaa_ol_get_port(port_id);
	if (dev == NULL)
		return -ENODEV;
	dpaa_intf = dev->data->dev_private;

	fd = check_fd(dpaa_intf);
	if (fd < 0)
		return fd;

	ret = ioctl(fd, ASK_CTRL_SET_LGW_INFO, lgw_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Set LGW info ioctl failed with errno: %s", strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Set LGW info successful");

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa_ol_reset_lgw_info, 26.11)
int rte_pmd_dpaa_ol_reset_lgw_info(uint16_t port_id)
{
	struct rte_pmd_dpaa_lgw_info_s lgw_info;
	struct rte_eth_dev *dev;
	struct dpaa_if *dpaa_intf;
	int ret, fd;

	PMD_INIT_FUNC_TRACE();

	dev = dpaa_ol_get_port(port_id);
	if (dev == NULL)
		return -ENODEV;
	dpaa_intf = dev->data->dev_private;

	fd = check_fd(dpaa_intf);
	if (fd < 0)
		return fd;

	memset(&lgw_info, 0, sizeof(lgw_info));

	ret = ioctl(fd, ASK_CTRL_RESET_LGW_INFO, &lgw_info);
	if (ret) {
		int err = errno;

		/* Save errno first: the log call may overwrite it. */
		DPAA_PMD_ERR("Reset LGW info ioctl failed with errno: %s", strerror(err));
		return -err;
	}

	DPAA_PMD_DEBUG("Reset LGW info successful");

	return 0;
}

static int
dpaa_ol_get_bh_port_name(struct rte_devargs *devargs, char *val, size_t size);

static int dpaa_ol_set_fq_info(struct rte_eth_dev *dev);

static int dpaa_ol_dev_start(struct rte_eth_dev *dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Done in dev_start() since Rx/Tx queue setup order is not guaranteed. */
	ret = dpaa_ol_set_fq_info(dev);
	if (ret)
		return ret;

	dev->tx_pkt_burst = dpaa_eth_queue_tx;

	return 0;
}

static int dpaa_ol_reset_fq_info(struct dpaa_if *dpaa_intf)
{
	struct ask_ctrl_dpdk_fq_info_s fq_info;
	int ret;

	if (!dpaa_intf->ol_fq_info_set)
		return 0;

	memset(&fq_info, 0, sizeof(fq_info));
	if (dpaa_intf->tx_queues && dpaa_intf->nb_tx_queues)
		fq_info.tx_fq_id = dpaa_intf->tx_queues[0].fqid;
	if (dpaa_intf->rx_queues && dpaa_intf->nb_rx_queues)
		fq_info.rx_fq_id = dpaa_intf->rx_queues[0].fqid;

	ret = ask_reset_fq_info(dpaa_intf, &fq_info);
	if (ret == 0)
		dpaa_intf->ol_fq_info_set = false;

	return ret;
}

static int dpaa_ol_dev_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	dev->tx_pkt_burst = dpaa_eth_tx_drop_all;

	return dpaa_ol_reset_fq_info(dev->data->dev_private);
}

static void dpaa_oldev_queues_release(struct dpaa_if *dpaa_intf);

static int dpaa_ol_dev_close(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dpaa_intf = dev->data->dev_private;
	if (!dpaa_intf)
		return 0;

	/* Shut down FQs and free queue memory. */
	dpaa_oldev_queues_release(dpaa_intf);

	if (dpaa_intf->ol_fd >= 0) {
		close(dpaa_intf->ol_fd);
		dpaa_intf->ol_fd = -1;
	}

	return 0;
}

static inline void
dpaa_poll_queue_default_config(struct qm_mcc_initfq *opts)
{
	memset(opts, 0, sizeof(struct qm_mcc_initfq));
	opts->we_mask = QM_INITFQ_WE_CONTEXTA;
	opts->fqd.fq_ctrl = QM_FQCTRL_AVOIDBLOCK | QM_FQCTRL_CTXASTASHING |
			    QM_FQCTRL_PREFERINCACHE;
	opts->fqd.context_a.stashing.exclusive = 0;
	if (dpaa_soc_ver() != SVR_LS1046A_FAMILY)
		opts->fqd.context_a.stashing.annotation_cl =
				DPAA_IF_RX_ANNOTATION_STASH;
	opts->fqd.context_a.stashing.data_cl = DPAA_IF_RX_DATA_STASH;
	opts->fqd.context_a.stashing.context_cl = DPAA_IF_RX_CONTEXT_STASH;
}

static
int dpaa_ol_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			    __rte_unused uint16_t nb_desc,
			    unsigned int socket_id __rte_unused,
			    __rte_unused const struct rte_eth_rxconf *rx_conf,
			    struct rte_mempool *mp)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *rxq = &dpaa_intf->rx_queues[queue_idx];

	PMD_INIT_FUNC_TRACE();

	dpaa_intf->bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);
	dpaa_intf->valid = 1;

	if (!rte_dpaa_bpid_info) {
		DPAA_PMD_ERR("BPID info array not initialized");
		return -ENOMEM;
	}
	rxq->bp_array = rte_dpaa_bpid_info;
	dev->data->rx_queues[queue_idx] = rxq;

	return 0;
}

static int
dpaa_ol_get_bh_port_name(struct rte_devargs *devargs, char *val, size_t size)
{
	static const char * const valid_keys[] = { DRIVER_OL_BH_PORT, NULL };
	struct rte_kvargs *kvlist;
	const char *str;
	int ret;

	if (!devargs || !devargs->args)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, valid_keys);
	if (!kvlist) {
		DPAA_PMD_ERR("Invalid devargs for offline port \"%s\"",
			     devargs->args);
		return -EINVAL;
	}

	if (!rte_kvargs_count(kvlist, DRIVER_OL_BH_PORT)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	str = rte_kvargs_get(kvlist, DRIVER_OL_BH_PORT);
	if (str == NULL) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	ret = rte_strscpy(val, str, size);
	rte_kvargs_free(kvlist);
	if (ret < 0)
		return ret;

	return 1;
}

static
int dpaa_ol_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			   uint16_t nb_desc __rte_unused,
			   unsigned int socket_id __rte_unused,
			   const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct qman_fq *txq = &dpaa_intf->tx_queues[queue_idx];

	PMD_INIT_FUNC_TRACE();

	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

static int dpaa_ol_set_fq_info(struct rte_eth_dev *dev)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct ask_ctrl_dpdk_fq_info_s fq_info;
	struct qman_fq *txq, *rxq;
	struct rte_pktmbuf_pool_private *mbp_priv;
	int ret;

	if (dpaa_intf->rx_queues == NULL || dpaa_intf->tx_queues == NULL) {
		DPAA_PMD_ERR("Rx/Tx queues not allocated");
		return -EINVAL;
	}

	if (dpaa_intf->bp_info == NULL) {
		DPAA_PMD_ERR("Rx queue must be set up before starting the port");
		return -EINVAL;
	}

	txq = &dpaa_intf->tx_queues[0];
	rxq = &dpaa_intf->rx_queues[0];
	memset(&fq_info, 0, sizeof(fq_info));
	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(dpaa_intf->bp_info->mp);
	fq_info.tx_fq_id = txq->fqid;
	fq_info.rx_fq_id = rxq->fqid;
	fq_info.buff_size = mbp_priv->mbuf_data_room_size;
	fq_info.bp_id = dpaa_intf->bp_info->bpid;

	fq_info.privDataSize = DPAA_OL_PARSERSLT_START_OFFSET;
	fq_info.passPrsResult = DPAA_OL_PARSE_RESULT_REQUIRED;
	fq_info.passTimeStamp = DPAA_OL_TIME_STAMP_REQUIRED;
	fq_info.passHashResult = DPAA_OL_HASH_RESULT_REQUIRED;
	fq_info.passAllOtherPCDInfo = DPAA_OL_PCD_INFO_REQUIRED;
	fq_info.dataAlign = DPAA_OL_DATA_ALIGNMENT;
	fq_info.manipExtraSpace = DPAA_OL_MAX_EXTRA_SIZE;

	memcpy(fq_info.bh_port_name, dpaa_intf->ol_bh_port_name,
	       sizeof(fq_info.bh_port_name));

	ret = ask_set_fq_info(dpaa_intf, &fq_info);
	if (ret) {
		DPAA_PMD_ERR("Set FQ info failed with ret: %d", ret);
		return ret;
	}
	dpaa_intf->ol_fq_info_set = true;

	return 0;
}

static int dpaa_ol_link_update(struct rte_eth_dev *dev,
			       __rte_unused int wait_to_complete)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct rte_eth_link *link = &dev->data->dev_link;

	PMD_INIT_FUNC_TRACE();

	link->link_status = dpaa_intf->valid;
	link->link_speed = RTE_ETH_SPEED_NUM_NONE;
	link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link->link_autoneg = RTE_ETH_LINK_AUTONEG;

	return 0;
}

static int dpaa_ol_promiscuous_enable(__rte_unused struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int dpaa_ol_rx_queue_init(struct qman_fq *fq, uint32_t fqid)
{
	int ret;
	struct qm_mcc_initfq opts = {0};
	u32 flags = QMAN_FQ_FLAG_DYNAMIC_FQID | QMAN_FQ_FLAG_NO_ENQUEUE;

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		ret = rte_dpaa_portal_init(NULL);
		if (ret < 0) {
			DPAA_PMD_ERR("portal initialization failure");
			return ret;
		}
	}

	ret = qman_create_fq(fqid, flags, fq);
	if (ret) {
		DPAA_PMD_ERR("create rx fqid 0x%x failed with ret: %d", fqid,
			     ret);
		return ret;
	}
	fq->is_static = false;

	dpaa_poll_queue_default_config(&opts);

	ret = qman_init_fq(fq, 0, &opts);
	if (ret)
		DPAA_PMD_ERR("init rx fqid 0x%x failed with ret:%d", fqid, ret);
	return ret;
}

static int dpaa_ol_tx_queue_init(struct dpaa_if *dpaa_intf, struct qman_fq *fq,
				  uint32_t fqid)
{
	int ret;
	struct qm_mcc_initfq opts = {0};
	u32 flags = QMAN_FQ_FLAG_DYNAMIC_FQID | QMAN_FQ_FLAG_TO_DCPORTAL;
	struct ask_ctrl_offline_channel ch_info;

	memset(&ch_info, 0, sizeof(ch_info));

	ret = ask_get_channel_info(dpaa_intf, &ch_info);
	if (ret) {
		DPAA_PMD_ERR("Get channel info failed with ret: %d", ret);
		return ret;
	}
	ret = qman_create_fq(fqid, flags, fq);
	if (ret) {
		DPAA_PMD_ERR("Create tx fq failed with ret: %d", ret);
		return ret;
	}

	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		       QM_INITFQ_WE_CONTEXTB | QM_INITFQ_WE_CONTEXTA;

	opts.fqd.dest.channel = ch_info.channel_id;
	opts.fqd.dest.wq = DPA_ISC_WQ_ID;
	opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE;
	opts.fqd.context_b = ch_info.ctx_b_val;
	opts.fqd.context_a.hi = ch_info.ctx_a_hi_val;
	opts.fqd.context_a.lo = ch_info.ctx_a_lo_val;

	opts.fqid = fq->fqid;
	fq->ch_id = ch_info.channel_id;
	opts.count = 1;

	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	if (ret)
		DPAA_PMD_ERR("init tx fqid 0x%x failed %d", fq->fqid, ret);
	return ret;
}

static const struct eth_dev_ops dpaa_ol_devops = {
		.dev_configure            = dpaa_ol_dev_configure,
		.dev_start                = dpaa_ol_dev_start,
		.dev_stop                 = dpaa_ol_dev_stop,
		.dev_close                = dpaa_ol_dev_close,
		.dev_infos_get            = dpaa_ol_dev_info,

		.rx_queue_setup           = dpaa_ol_rx_queue_setup,
		.tx_queue_setup           = dpaa_ol_tx_queue_setup,

		.link_update              = dpaa_ol_link_update,
		.promiscuous_enable       = dpaa_ol_promiscuous_enable,
};

static void dpaa_oldev_queues_release(struct dpaa_if *dpaa_intf)
{
	uint16_t i;
	int ret, reset_ret;
	bool release_fqids;

	/* Release FQIDs only if the kernel confirms it dropped them. */
	release_fqids = true;
	reset_ret = dpaa_ol_reset_fq_info(dpaa_intf);
	if (reset_ret) {
		DPAA_PMD_WARN("Reset FQ info failed with ret: %d, not releasing fqids",
			      reset_ret);
		release_fqids = false;
	}

	if (dpaa_intf->tx_queues) {
		for (i = 0; i < dpaa_intf->nb_tx_queues; i++) {
			if (!dpaa_intf->tx_queues[i].fqid)
				continue;
			ret = qman_shutdown_fq(&dpaa_intf->tx_queues[i]);
			if (ret) {
				DPAA_PMD_WARN("Failed(%d) to shutdown txq%d(0x%x), fqid not released",
					      ret, i, dpaa_intf->tx_queues[i].fqid);
				continue;
			}
			if (release_fqids)
				qman_release_fqid(dpaa_intf->tx_queues[i].fqid);
		}
		rte_free(dpaa_intf->tx_queues);
		dpaa_intf->tx_queues = NULL;
		dpaa_intf->nb_tx_queues = 0;
	}

	if (dpaa_intf->rx_queues) {
		for (i = 0; i < dpaa_intf->nb_rx_queues; i++) {
			if (!dpaa_intf->rx_queues[i].fqid)
				continue;
			ret = qman_shutdown_fq(&dpaa_intf->rx_queues[i]);
			if (ret) {
				DPAA_PMD_WARN("Failed(%d) to shutdown rxq%d(0x%x), fqid not released",
					      ret, i, dpaa_intf->rx_queues[i].fqid);
				continue;
			}
			if (release_fqids)
				qman_release_fqid(dpaa_intf->rx_queues[i].fqid);
		}
		rte_free(dpaa_intf->rx_queues);
		dpaa_intf->rx_queues = NULL;
		dpaa_intf->nb_rx_queues = 0;
	}
}

static int dpaa_oldev_init(struct rte_eth_dev *eth_dev)
{
	int num_fqs, ret;
	struct dpaa_if *dpaa_intf;

	PMD_INIT_FUNC_TRACE();

	num_fqs = DPAA_DEFAULT_NUM_PCD_QUEUES;
	dpaa_intf = eth_dev->data->dev_private;
	dpaa_intf->ifid = eth_dev->data->port_id;
	dpaa_intf->ol_fd = open(ASK_PATH, O_RDWR | O_CLOEXEC);
	if (dpaa_intf->ol_fd < 0) {
		ret = -errno;
		DPAA_PMD_ERR("Failed to open %s: %s", ASK_PATH, strerror(-ret));
		return ret;
	}

	dpaa_intf->rx_queues = rte_zmalloc(NULL,
					   sizeof(struct qman_fq) * num_fqs,
					   MAX_CACHELINE);
	if (!dpaa_intf->rx_queues) {
		DPAA_PMD_ERR("Failed to alloc mem for RX queues");
		ret = -ENOMEM;
		goto free_rx;
	}
	dpaa_intf->nb_rx_queues = num_fqs;

	ret = dpaa_ol_rx_queue_init(&dpaa_intf->rx_queues[0], 0);
	if (ret)
		goto free_rx;
	dpaa_intf->rx_queues[0].dpaa_intf = dpaa_intf;

	dpaa_intf->tx_queues = rte_zmalloc(NULL,
					   sizeof(struct qman_fq) * num_fqs,
					   MAX_CACHELINE);
	if (!dpaa_intf->tx_queues) {
		DPAA_PMD_ERR("Failed to alloc mem for TX queues");
		ret = -ENOMEM;
		goto free_rx;
	}
	dpaa_intf->nb_tx_queues = num_fqs;

	ret = dpaa_ol_tx_queue_init(dpaa_intf, &dpaa_intf->tx_queues[0], 0);
	if (ret)
		goto free_rx;
	dpaa_intf->tx_queues[0].dpaa_intf = dpaa_intf;

	eth_dev->dev_ops = &dpaa_ol_devops;
	eth_dev->rx_pkt_burst = dpaa_eth_queue_rx;
	eth_dev->tx_pkt_burst = dpaa_eth_tx_drop_all;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
			RTE_ETHER_ADDR_LEN * DPAA_MAX_MAC_FILTER, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		DPAA_PMD_ERR("Failed to allocate %d bytes needed to "
			     "store MAC addresses",
			     RTE_ETHER_ADDR_LEN * DPAA_MAX_MAC_FILTER);
		ret = -ENOMEM;
		goto free_rx;
	}

	return 0;

free_rx:
	dpaa_oldev_queues_release(dpaa_intf);
	if (dpaa_intf->ol_fd >= 0) {
		close(dpaa_intf->ol_fd);
		dpaa_intf->ol_fd = -1;
	}
	return ret;
}

static int dpaa_ol_probe(__rte_unused struct rte_dpaa_driver *dpaa_drv,
			 struct rte_dpaa_device *dpaa_dev)
{
	int ret;
	struct rte_eth_dev *eth_dev;
	struct dpaa_if *dpaa_intf;
	char bh_port_name[DPAA_OL_MAX_BH_PORT_NAME_LEN];

	PMD_INIT_FUNC_TRACE();

	/* Offline port state is owned by the primary process only. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DPAA_PMD_WARN("%s: not supported in secondary process",
			      dpaa_dev->device.name);
		return -ENOTSUP;
	}

	ret = dpaa_ol_get_bh_port_name(dpaa_dev->device.devargs,
				       bh_port_name, sizeof(bh_port_name));
	if (ret <= 0) {
		DPAA_PMD_ERR("Offline port \"%s\" devarg not defined or invalid (ret=%d)",
			     DRIVER_OL_BH_PORT, ret);
		return ret < 0 ? ret : -EINVAL;
	}

	eth_dev = rte_eth_dev_allocate(dpaa_dev->name);
	if (!eth_dev)
		return -ENOMEM;
	eth_dev->data->dev_private = rte_zmalloc("ethdev private structure",
						 sizeof(struct dpaa_if),
						 RTE_CACHE_LINE_SIZE);
	if (!eth_dev->data->dev_private) {
		DPAA_PMD_ERR("Cannot allocate memzone for port data");
		rte_eth_dev_release_port(eth_dev);
		return -ENOMEM;
	}

	eth_dev->device = &dpaa_dev->device;

	dpaa_intf = eth_dev->data->dev_private;
	rte_strscpy(dpaa_intf->ol_bh_port_name, bh_port_name,
		    sizeof(dpaa_intf->ol_bh_port_name));

	ret = dpaa_oldev_init(eth_dev);
	if (ret == 0) {
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	rte_eth_dev_release_port(eth_dev);
	return ret;
}

static int dpaa_ol_remove(struct rte_dpaa_device *dpaa_dev)
{
	struct rte_eth_dev *eth_dev;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_dev = rte_eth_dev_allocated(dpaa_dev->device.name);
	if (eth_dev == NULL)
		return 0;

	dpaa_ol_dev_close(eth_dev);

	return rte_eth_dev_release_port(eth_dev);
}

static struct rte_dpaa_driver rte_dpaa_ol_pmd = {
		.drv_type = FSL_DPAA_OL,
		.probe = dpaa_ol_probe,
		.remove = dpaa_ol_remove,
};

RTE_PMD_REGISTER_DPAA(ol_dpaa, rte_dpaa_ol_pmd);
RTE_PMD_REGISTER_PARAM_STRING(ol_dpaa,
		DRIVER_OL_BH_PORT "=<string>");
