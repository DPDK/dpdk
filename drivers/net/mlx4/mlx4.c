/*-
 *   BSD LICENSE
 *
 *   Copyright 2012 6WIND S.A.
 *   Copyright 2012 Mellanox
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

/* System headers. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <net/if.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <fcntl.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_dev.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <rte_atomic.h>
#include <rte_log.h>
#include <rte_alarm.h>
#include <rte_memory.h>
#include <rte_flow.h>
#include <rte_kvargs.h>
#include <rte_interrupts.h>
#include <rte_branch_prediction.h>

/* Generated configuration header. */
#include "mlx4_autoconf.h"

/* PMD headers. */
#include "mlx4.h"
#include "mlx4_flow.h"

/* Convenience macros for accessing mbuf fields. */
#define NEXT(m) ((m)->next)
#define DATA_LEN(m) ((m)->data_len)
#define PKT_LEN(m) ((m)->pkt_len)
#define DATA_OFF(m) ((m)->data_off)
#define SET_DATA_OFF(m, o) ((m)->data_off = (o))
#define NB_SEGS(m) ((m)->nb_segs)
#define PORT(m) ((m)->port)

/* Work Request ID data type (64 bit). */
typedef union {
	struct {
		uint32_t id;
		uint16_t offset;
	} data;
	uint64_t raw;
} wr_id_t;

#define WR_ID(o) (((wr_id_t *)&(o))->data)

/* Transpose flags. Useful to convert IBV to DPDK flags. */
#define TRANSPOSE(val, from, to) \
	(((from) >= (to)) ? \
	 (((val) & (from)) / ((from) / (to))) : \
	 (((val) & (from)) * ((to) / (from))))

/** Configuration structure for device arguments. */
struct mlx4_conf {
	struct {
		uint32_t present; /**< Bit-field for existing ports. */
		uint32_t enabled; /**< Bit-field for user-enabled ports. */
	} ports;
};

/* Available parameters list. */
const char *pmd_mlx4_init_params[] = {
	MLX4_PMD_PORT_KVARG,
	NULL,
};

static int
mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx);

static int
mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx);

static int
priv_rx_intr_vec_enable(struct priv *priv);

static void
priv_rx_intr_vec_disable(struct priv *priv);

/**
 * Lock private structure to protect it from concurrent access in the
 * control path.
 *
 * @param priv
 *   Pointer to private structure.
 */
void priv_lock(struct priv *priv)
{
	rte_spinlock_lock(&priv->lock);
}

/**
 * Unlock private structure.
 *
 * @param priv
 *   Pointer to private structure.
 */
void priv_unlock(struct priv *priv)
{
	rte_spinlock_unlock(&priv->lock);
}

/* Allocate a buffer on the stack and fill it with a printf format string. */
#define MKSTR(name, ...) \
	char name[snprintf(NULL, 0, __VA_ARGS__) + 1]; \
	\
	snprintf(name, sizeof(name), __VA_ARGS__)

/**
 * Get interface name from private structure.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_get_ifname(const struct priv *priv, char (*ifname)[IF_NAMESIZE])
{
	DIR *dir;
	struct dirent *dent;
	unsigned int dev_type = 0;
	unsigned int dev_port_prev = ~0u;
	char match[IF_NAMESIZE] = "";

	{
		MKSTR(path, "%s/device/net", priv->ctx->device->ibdev_path);

		dir = opendir(path);
		if (dir == NULL)
			return -1;
	}
	while ((dent = readdir(dir)) != NULL) {
		char *name = dent->d_name;
		FILE *file;
		unsigned int dev_port;
		int r;

		if ((name[0] == '.') &&
		    ((name[1] == '\0') ||
		     ((name[1] == '.') && (name[2] == '\0'))))
			continue;

		MKSTR(path, "%s/device/net/%s/%s",
		      priv->ctx->device->ibdev_path, name,
		      (dev_type ? "dev_id" : "dev_port"));

		file = fopen(path, "rb");
		if (file == NULL) {
			if (errno != ENOENT)
				continue;
			/*
			 * Switch to dev_id when dev_port does not exist as
			 * is the case with Linux kernel versions < 3.15.
			 */
try_dev_id:
			match[0] = '\0';
			if (dev_type)
				break;
			dev_type = 1;
			dev_port_prev = ~0u;
			rewinddir(dir);
			continue;
		}
		r = fscanf(file, (dev_type ? "%x" : "%u"), &dev_port);
		fclose(file);
		if (r != 1)
			continue;
		/*
		 * Switch to dev_id when dev_port returns the same value for
		 * all ports. May happen when using a MOFED release older than
		 * 3.0 with a Linux kernel >= 3.15.
		 */
		if (dev_port == dev_port_prev)
			goto try_dev_id;
		dev_port_prev = dev_port;
		if (dev_port == (priv->port - 1u))
			snprintf(match, sizeof(match), "%s", name);
	}
	closedir(dir);
	if (match[0] == '\0')
		return -1;
	strncpy(*ifname, match, sizeof(*ifname));
	return 0;
}

/**
 * Read from sysfs entry.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[in] entry
 *   Entry name relative to sysfs path.
 * @param[out] buf
 *   Data output buffer.
 * @param size
 *   Buffer size.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_sysfs_read(const struct priv *priv, const char *entry,
		char *buf, size_t size)
{
	char ifname[IF_NAMESIZE];
	FILE *file;
	int ret;
	int err;

	if (priv_get_ifname(priv, &ifname))
		return -1;

	MKSTR(path, "%s/device/net/%s/%s", priv->ctx->device->ibdev_path,
	      ifname, entry);

	file = fopen(path, "rb");
	if (file == NULL)
		return -1;
	ret = fread(buf, 1, size, file);
	err = errno;
	if (((size_t)ret < size) && (ferror(file)))
		ret = -1;
	else
		ret = size;
	fclose(file);
	errno = err;
	return ret;
}

/**
 * Write to sysfs entry.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[in] entry
 *   Entry name relative to sysfs path.
 * @param[in] buf
 *   Data buffer.
 * @param size
 *   Buffer size.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_sysfs_write(const struct priv *priv, const char *entry,
		 char *buf, size_t size)
{
	char ifname[IF_NAMESIZE];
	FILE *file;
	int ret;
	int err;

	if (priv_get_ifname(priv, &ifname))
		return -1;

	MKSTR(path, "%s/device/net/%s/%s", priv->ctx->device->ibdev_path,
	      ifname, entry);

	file = fopen(path, "wb");
	if (file == NULL)
		return -1;
	ret = fwrite(buf, 1, size, file);
	err = errno;
	if (((size_t)ret < size) || (ferror(file)))
		ret = -1;
	else
		ret = size;
	fclose(file);
	errno = err;
	return ret;
}

/**
 * Get unsigned long sysfs property.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] name
 *   Entry name relative to sysfs path.
 * @param[out] value
 *   Value output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_get_sysfs_ulong(struct priv *priv, const char *name, unsigned long *value)
{
	int ret;
	unsigned long value_ret;
	char value_str[32];

	ret = priv_sysfs_read(priv, name, value_str, (sizeof(value_str) - 1));
	if (ret == -1) {
		DEBUG("cannot read %s value from sysfs: %s",
		      name, strerror(errno));
		return -1;
	}
	value_str[ret] = '\0';
	errno = 0;
	value_ret = strtoul(value_str, NULL, 0);
	if (errno) {
		DEBUG("invalid %s value `%s': %s", name, value_str,
		      strerror(errno));
		return -1;
	}
	*value = value_ret;
	return 0;
}

/**
 * Set unsigned long sysfs property.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] name
 *   Entry name relative to sysfs path.
 * @param value
 *   Value to set.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_set_sysfs_ulong(struct priv *priv, const char *name, unsigned long value)
{
	int ret;
	MKSTR(value_str, "%lu", value);

	ret = priv_sysfs_write(priv, name, value_str, (sizeof(value_str) - 1));
	if (ret == -1) {
		DEBUG("cannot write %s `%s' (%lu) to sysfs: %s",
		      name, value_str, value, strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * Perform ifreq ioctl() on associated Ethernet device.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_ifreq(const struct priv *priv, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = -1;

	if (sock == -1)
		return ret;
	if (priv_get_ifname(priv, &ifr->ifr_name) == 0)
		ret = ioctl(sock, req, ifr);
	close(sock);
	return ret;
}

/**
 * Get device MTU.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] mtu
 *   MTU value output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_get_mtu(struct priv *priv, uint16_t *mtu)
{
	unsigned long ulong_mtu;

	if (priv_get_sysfs_ulong(priv, "mtu", &ulong_mtu) == -1)
		return -1;
	*mtu = ulong_mtu;
	return 0;
}

/**
 * Set device MTU.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mtu
 *   MTU value to set.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_set_mtu(struct priv *priv, uint16_t mtu)
{
	uint16_t new_mtu;

	if (priv_set_sysfs_ulong(priv, "mtu", mtu) ||
	    priv_get_mtu(priv, &new_mtu))
		return -1;
	if (new_mtu == mtu)
		return 0;
	errno = EINVAL;
	return -1;
}

/**
 * Set device flags.
 *
 * @param priv
 *   Pointer to private structure.
 * @param keep
 *   Bitmask for flags that must remain untouched.
 * @param flags
 *   Bitmask for flags to modify.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_set_flags(struct priv *priv, unsigned int keep, unsigned int flags)
{
	unsigned long tmp;

	if (priv_get_sysfs_ulong(priv, "flags", &tmp) == -1)
		return -1;
	tmp &= keep;
	tmp |= (flags & (~keep));
	return priv_set_sysfs_ulong(priv, "flags", tmp);
}

/* Device configuration. */

static int
txq_setup(struct rte_eth_dev *dev, struct txq *txq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_txconf *conf);

static void
txq_cleanup(struct txq *txq);

static int
rxq_setup(struct rte_eth_dev *dev, struct rxq *rxq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_rxconf *conf,
	  struct rte_mempool *mp);

static void
rxq_cleanup(struct rxq *rxq);

static void
priv_mac_addr_del(struct priv *priv);

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int txqs_n = dev->data->nb_tx_queues;

	priv->rxqs = (void *)dev->data->rx_queues;
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		INFO("%p: TX queues number update: %u -> %u",
		     (void *)dev, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (rxqs_n != priv->rxqs_n) {
		INFO("%p: Rx queues number update: %u -> %u",
		     (void *)dev, priv->rxqs_n, rxqs_n);
		priv->rxqs_n = rxqs_n;
	}
	return 0;
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	ret = dev_configure(dev);
	assert(ret >= 0);
	priv_unlock(priv);
	return -ret;
}

static uint16_t mlx4_tx_burst(void *, struct rte_mbuf **, uint16_t);
static uint16_t removed_rx_burst(void *, struct rte_mbuf **, uint16_t);

/* TX queues handling. */

/**
 * Allocate TX queue elements.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
txq_alloc_elts(struct txq *txq, unsigned int elts_n)
{
	unsigned int i;
	struct txq_elt (*elts)[elts_n] =
		rte_calloc_socket("TXQ", 1, sizeof(*elts), 0, txq->socket);
	linear_t (*elts_linear)[elts_n] =
		rte_calloc_socket("TXQ", 1, sizeof(*elts_linear), 0,
				  txq->socket);
	struct ibv_mr *mr_linear = NULL;
	int ret = 0;

	if ((elts == NULL) || (elts_linear == NULL)) {
		ERROR("%p: can't allocate packets array", (void *)txq);
		ret = ENOMEM;
		goto error;
	}
	mr_linear =
		ibv_reg_mr(txq->priv->pd, elts_linear, sizeof(*elts_linear),
			   IBV_ACCESS_LOCAL_WRITE);
	if (mr_linear == NULL) {
		ERROR("%p: unable to configure MR, ibv_reg_mr() failed",
		      (void *)txq);
		ret = EINVAL;
		goto error;
	}
	for (i = 0; (i != elts_n); ++i) {
		struct txq_elt *elt = &(*elts)[i];

		elt->buf = NULL;
	}
	DEBUG("%p: allocated and configured %u WRs", (void *)txq, elts_n);
	txq->elts_n = elts_n;
	txq->elts = elts;
	txq->elts_head = 0;
	txq->elts_tail = 0;
	txq->elts_comp = 0;
	/* Request send completion every MLX4_PMD_TX_PER_COMP_REQ packets or
	 * at least 4 times per ring. */
	txq->elts_comp_cd_init =
		((MLX4_PMD_TX_PER_COMP_REQ < (elts_n / 4)) ?
		 MLX4_PMD_TX_PER_COMP_REQ : (elts_n / 4));
	txq->elts_comp_cd = txq->elts_comp_cd_init;
	txq->elts_linear = elts_linear;
	txq->mr_linear = mr_linear;
	assert(ret == 0);
	return 0;
error:
	if (mr_linear != NULL)
		claim_zero(ibv_dereg_mr(mr_linear));

	rte_free(elts_linear);
	rte_free(elts);

	DEBUG("%p: failed, freed everything", (void *)txq);
	assert(ret > 0);
	return ret;
}

/**
 * Free TX queue elements.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static void
txq_free_elts(struct txq *txq)
{
	unsigned int elts_n = txq->elts_n;
	unsigned int elts_head = txq->elts_head;
	unsigned int elts_tail = txq->elts_tail;
	struct txq_elt (*elts)[elts_n] = txq->elts;
	linear_t (*elts_linear)[elts_n] = txq->elts_linear;
	struct ibv_mr *mr_linear = txq->mr_linear;

	DEBUG("%p: freeing WRs", (void *)txq);
	txq->elts_n = 0;
	txq->elts_head = 0;
	txq->elts_tail = 0;
	txq->elts_comp = 0;
	txq->elts_comp_cd = 0;
	txq->elts_comp_cd_init = 0;
	txq->elts = NULL;
	txq->elts_linear = NULL;
	txq->mr_linear = NULL;
	if (mr_linear != NULL)
		claim_zero(ibv_dereg_mr(mr_linear));

	rte_free(elts_linear);
	if (elts == NULL)
		return;
	while (elts_tail != elts_head) {
		struct txq_elt *elt = &(*elts)[elts_tail];

		assert(elt->buf != NULL);
		rte_pktmbuf_free(elt->buf);
#ifndef NDEBUG
		/* Poisoning. */
		memset(elt, 0x77, sizeof(*elt));
#endif
		if (++elts_tail == elts_n)
			elts_tail = 0;
	}
	rte_free(elts);
}


/**
 * Clean up a TX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static void
txq_cleanup(struct txq *txq)
{
	struct ibv_exp_release_intf_params params;
	size_t i;

	DEBUG("cleaning up %p", (void *)txq);
	txq_free_elts(txq);
	if (txq->if_qp != NULL) {
		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		assert(txq->qp != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq->priv->ctx,
						txq->if_qp,
						&params));
	}
	if (txq->if_cq != NULL) {
		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		assert(txq->cq != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq->priv->ctx,
						txq->if_cq,
						&params));
	}
	if (txq->qp != NULL)
		claim_zero(ibv_destroy_qp(txq->qp));
	if (txq->cq != NULL)
		claim_zero(ibv_destroy_cq(txq->cq));
	if (txq->rd != NULL) {
		struct ibv_exp_destroy_res_domain_attr attr = {
			.comp_mask = 0,
		};

		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		claim_zero(ibv_exp_destroy_res_domain(txq->priv->ctx,
						      txq->rd,
						      &attr));
	}
	for (i = 0; (i != elemof(txq->mp2mr)); ++i) {
		if (txq->mp2mr[i].mp == NULL)
			break;
		assert(txq->mp2mr[i].mr != NULL);
		claim_zero(ibv_dereg_mr(txq->mp2mr[i].mr));
	}
	memset(txq, 0, sizeof(*txq));
}

/**
 * Manage TX completions.
 *
 * When sending a burst, mlx4_tx_burst() posts several WRs.
 * To improve performance, a completion event is only required once every
 * MLX4_PMD_TX_PER_COMP_REQ sends. Doing so discards completion information
 * for other WRs, but this information would not be used anyway.
 *
 * @param txq
 *   Pointer to TX queue structure.
 *
 * @return
 *   0 on success, -1 on failure.
 */
static int
txq_complete(struct txq *txq)
{
	unsigned int elts_comp = txq->elts_comp;
	unsigned int elts_tail = txq->elts_tail;
	const unsigned int elts_n = txq->elts_n;
	int wcs_n;

	if (unlikely(elts_comp == 0))
		return 0;
	wcs_n = txq->if_cq->poll_cnt(txq->cq, elts_comp);
	if (unlikely(wcs_n == 0))
		return 0;
	if (unlikely(wcs_n < 0)) {
		DEBUG("%p: ibv_poll_cq() failed (wcs_n=%d)",
		      (void *)txq, wcs_n);
		return -1;
	}
	elts_comp -= wcs_n;
	assert(elts_comp <= txq->elts_comp);
	/*
	 * Assume WC status is successful as nothing can be done about it
	 * anyway.
	 */
	elts_tail += wcs_n * txq->elts_comp_cd_init;
	if (elts_tail >= elts_n)
		elts_tail -= elts_n;
	txq->elts_tail = elts_tail;
	txq->elts_comp = elts_comp;
	return 0;
}

struct mlx4_check_mempool_data {
	int ret;
	char *start;
	char *end;
};

/* Called by mlx4_check_mempool() when iterating the memory chunks. */
static void mlx4_check_mempool_cb(struct rte_mempool *mp,
	void *opaque, struct rte_mempool_memhdr *memhdr,
	unsigned mem_idx)
{
	struct mlx4_check_mempool_data *data = opaque;

	(void)mp;
	(void)mem_idx;

	/* It already failed, skip the next chunks. */
	if (data->ret != 0)
		return;
	/* It is the first chunk. */
	if (data->start == NULL && data->end == NULL) {
		data->start = memhdr->addr;
		data->end = data->start + memhdr->len;
		return;
	}
	if (data->end == memhdr->addr) {
		data->end += memhdr->len;
		return;
	}
	if (data->start == (char *)memhdr->addr + memhdr->len) {
		data->start -= memhdr->len;
		return;
	}
	/* Error, mempool is not virtually contigous. */
	data->ret = -1;
}

/**
 * Check if a mempool can be used: it must be virtually contiguous.
 *
 * @param[in] mp
 *   Pointer to memory pool.
 * @param[out] start
 *   Pointer to the start address of the mempool virtual memory area
 * @param[out] end
 *   Pointer to the end address of the mempool virtual memory area
 *
 * @return
 *   0 on success (mempool is virtually contiguous), -1 on error.
 */
static int mlx4_check_mempool(struct rte_mempool *mp, uintptr_t *start,
	uintptr_t *end)
{
	struct mlx4_check_mempool_data data;

	memset(&data, 0, sizeof(data));
	rte_mempool_mem_iter(mp, mlx4_check_mempool_cb, &data);
	*start = (uintptr_t)data.start;
	*end = (uintptr_t)data.end;

	return data.ret;
}

/* For best performance, this function should not be inlined. */
static struct ibv_mr *mlx4_mp2mr(struct ibv_pd *, struct rte_mempool *)
	__rte_noinline;

/**
 * Register mempool as a memory region.
 *
 * @param pd
 *   Pointer to protection domain.
 * @param mp
 *   Pointer to memory pool.
 *
 * @return
 *   Memory region pointer, NULL in case of error.
 */
static struct ibv_mr *
mlx4_mp2mr(struct ibv_pd *pd, struct rte_mempool *mp)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	uintptr_t start;
	uintptr_t end;
	unsigned int i;

	if (mlx4_check_mempool(mp, &start, &end) != 0) {
		ERROR("mempool %p: not virtually contiguous",
			(void *)mp);
		return NULL;
	}

	DEBUG("mempool %p area start=%p end=%p size=%zu",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	/* Round start and end to page boundary if found in memory segments. */
	for (i = 0; (i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL); ++i) {
		uintptr_t addr = (uintptr_t)ms[i].addr;
		size_t len = ms[i].len;
		unsigned int align = ms[i].hugepage_sz;

		if ((start > addr) && (start < addr + len))
			start = RTE_ALIGN_FLOOR(start, align);
		if ((end > addr) && (end < addr + len))
			end = RTE_ALIGN_CEIL(end, align);
	}
	DEBUG("mempool %p using start=%p end=%p size=%zu for MR",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	return ibv_reg_mr(pd,
			  (void *)start,
			  end - start,
			  IBV_ACCESS_LOCAL_WRITE);
}

/**
 * Get Memory Pool (MP) from mbuf. If mbuf is indirect, the pool from which
 * the cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static struct rte_mempool *
txq_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_INDIRECT(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
}

/**
 * Get Memory Region (MR) <-> Memory Pool (MP) association from txq->mp2mr[].
 * Add MP to txq->mp2mr[] if it's not registered yet. If mp2mr[] is full,
 * remove an entry first.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
static uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
{
	unsigned int i;
	struct ibv_mr *mr;

	for (i = 0; (i != elemof(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].mp == mp) {
			assert(txq->mp2mr[i].lkey != (uint32_t)-1);
			assert(txq->mp2mr[i].mr->lkey == txq->mp2mr[i].lkey);
			return txq->mp2mr[i].lkey;
		}
	}
	/* Add a new entry, register MR first. */
	DEBUG("%p: discovered new memory pool \"%s\" (%p)",
	      (void *)txq, mp->name, (void *)mp);
	mr = mlx4_mp2mr(txq->priv->pd, mp);
	if (unlikely(mr == NULL)) {
		DEBUG("%p: unable to configure MR, ibv_reg_mr() failed.",
		      (void *)txq);
		return (uint32_t)-1;
	}
	if (unlikely(i == elemof(txq->mp2mr))) {
		/* Table is full, remove oldest entry. */
		DEBUG("%p: MR <-> MP table full, dropping oldest entry.",
		      (void *)txq);
		--i;
		claim_zero(ibv_dereg_mr(txq->mp2mr[0].mr));
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq->mp2mr[i].mp = mp;
	txq->mp2mr[i].mr = mr;
	txq->mp2mr[i].lkey = mr->lkey;
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq, mp->name, (void *)mp, txq->mp2mr[i].lkey);
	return txq->mp2mr[i].lkey;
}

struct txq_mp2mr_mbuf_check_data {
	int ret;
};

/**
 * Callback function for rte_mempool_obj_iter() to check whether a given
 * mempool object looks like a mbuf.
 *
 * @param[in] mp
 *   The mempool pointer
 * @param[in] arg
 *   Context data (struct txq_mp2mr_mbuf_check_data). Contains the
 *   return value.
 * @param[in] obj
 *   Object address.
 * @param index
 *   Object index, unused.
 */
static void
txq_mp2mr_mbuf_check(struct rte_mempool *mp, void *arg, void *obj,
	uint32_t index __rte_unused)
{
	struct txq_mp2mr_mbuf_check_data *data = arg;
	struct rte_mbuf *buf = obj;

	/* Check whether mbuf structure fits element size and whether mempool
	 * pointer is valid. */
	if (sizeof(*buf) > mp->elt_size || buf->pool != mp)
		data->ret = -1;
}

/**
 * Iterator function for rte_mempool_walk() to register existing mempools and
 * fill the MP to MR cache of a TX queue.
 *
 * @param[in] mp
 *   Memory Pool to register.
 * @param *arg
 *   Pointer to TX queue structure.
 */
static void
txq_mp2mr_iter(struct rte_mempool *mp, void *arg)
{
	struct txq *txq = arg;
	struct txq_mp2mr_mbuf_check_data data = {
		.ret = 0,
	};

	/* Register mempool only if the first element looks like a mbuf. */
	if (rte_mempool_obj_iter(mp, txq_mp2mr_mbuf_check, &data) == 0 ||
			data.ret == -1)
		return;
	txq_mp2mr(txq, mp);
}

/**
 * Copy scattered mbuf contents to a single linear buffer.
 *
 * @param[out] linear
 *   Linear output buffer.
 * @param[in] buf
 *   Scattered input buffer.
 *
 * @return
 *   Number of bytes copied to the output buffer or 0 if not large enough.
 */
static unsigned int
linearize_mbuf(linear_t *linear, struct rte_mbuf *buf)
{
	unsigned int size = 0;
	unsigned int offset;

	do {
		unsigned int len = DATA_LEN(buf);

		offset = size;
		size += len;
		if (unlikely(size > sizeof(*linear)))
			return 0;
		memcpy(&(*linear)[offset],
		       rte_pktmbuf_mtod(buf, uint8_t *),
		       len);
		buf = NEXT(buf);
	} while (buf != NULL);
	return size;
}

/**
 * Handle scattered buffers for mlx4_tx_burst().
 *
 * @param txq
 *   TX queue structure.
 * @param segs
 *   Number of segments in buf.
 * @param elt
 *   TX queue element to fill.
 * @param[in] buf
 *   Buffer to process.
 * @param elts_head
 *   Index of the linear buffer to use if necessary (normally txq->elts_head).
 * @param[out] sges
 *   Array filled with SGEs on success.
 *
 * @return
 *   A structure containing the processed packet size in bytes and the
 *   number of SGEs. Both fields are set to (unsigned int)-1 in case of
 *   failure.
 */
static struct tx_burst_sg_ret {
	unsigned int length;
	unsigned int num;
}
tx_burst_sg(struct txq *txq, unsigned int segs, struct txq_elt *elt,
	    struct rte_mbuf *buf, unsigned int elts_head,
	    struct ibv_sge (*sges)[MLX4_PMD_SGE_WR_N])
{
	unsigned int sent_size = 0;
	unsigned int j;
	int linearize = 0;

	/* When there are too many segments, extra segments are
	 * linearized in the last SGE. */
	if (unlikely(segs > elemof(*sges))) {
		segs = (elemof(*sges) - 1);
		linearize = 1;
	}
	/* Update element. */
	elt->buf = buf;
	/* Register segments as SGEs. */
	for (j = 0; (j != segs); ++j) {
		struct ibv_sge *sge = &(*sges)[j];
		uint32_t lkey;

		/* Retrieve Memory Region key for this memory pool. */
		lkey = txq_mp2mr(txq, txq_mb2mp(buf));
		if (unlikely(lkey == (uint32_t)-1)) {
			/* MR does not exist. */
			DEBUG("%p: unable to get MP <-> MR association",
			      (void *)txq);
			/* Clean up TX element. */
			elt->buf = NULL;
			goto stop;
		}
		/* Update SGE. */
		sge->addr = rte_pktmbuf_mtod(buf, uintptr_t);
		if (txq->priv->vf)
			rte_prefetch0((volatile void *)
				      (uintptr_t)sge->addr);
		sge->length = DATA_LEN(buf);
		sge->lkey = lkey;
		sent_size += sge->length;
		buf = NEXT(buf);
	}
	/* If buf is not NULL here and is not going to be linearized,
	 * nb_segs is not valid. */
	assert(j == segs);
	assert((buf == NULL) || (linearize));
	/* Linearize extra segments. */
	if (linearize) {
		struct ibv_sge *sge = &(*sges)[segs];
		linear_t *linear = &(*txq->elts_linear)[elts_head];
		unsigned int size = linearize_mbuf(linear, buf);

		assert(segs == (elemof(*sges) - 1));
		if (size == 0) {
			/* Invalid packet. */
			DEBUG("%p: packet too large to be linearized.",
			      (void *)txq);
			/* Clean up TX element. */
			elt->buf = NULL;
			goto stop;
		}
		/* If MLX4_PMD_SGE_WR_N is 1, free mbuf immediately. */
		if (elemof(*sges) == 1) {
			do {
				struct rte_mbuf *next = NEXT(buf);

				rte_pktmbuf_free_seg(buf);
				buf = next;
			} while (buf != NULL);
			elt->buf = NULL;
		}
		/* Update SGE. */
		sge->addr = (uintptr_t)&(*linear)[0];
		sge->length = size;
		sge->lkey = txq->mr_linear->lkey;
		sent_size += size;
		/* Include last segment. */
		segs++;
	}
	return (struct tx_burst_sg_ret){
		.length = sent_size,
		.num = segs,
	};
stop:
	return (struct tx_burst_sg_ret){
		.length = -1,
		.num = -1,
	};
}

/**
 * DPDK callback for TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static uint16_t
mlx4_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	unsigned int elts_head = txq->elts_head;
	const unsigned int elts_n = txq->elts_n;
	unsigned int elts_comp_cd = txq->elts_comp_cd;
	unsigned int elts_comp = 0;
	unsigned int i;
	unsigned int max;
	int err;

	assert(elts_comp_cd != 0);
	txq_complete(txq);
	max = (elts_n - (elts_head - txq->elts_tail));
	if (max > elts_n)
		max -= elts_n;
	assert(max >= 1);
	assert(max <= elts_n);
	/* Always leave one free entry in the ring. */
	--max;
	if (max == 0)
		return 0;
	if (max > pkts_n)
		max = pkts_n;
	for (i = 0; (i != max); ++i) {
		struct rte_mbuf *buf = pkts[i];
		unsigned int elts_head_next =
			(((elts_head + 1) == elts_n) ? 0 : elts_head + 1);
		struct txq_elt *elt_next = &(*txq->elts)[elts_head_next];
		struct txq_elt *elt = &(*txq->elts)[elts_head];
		unsigned int segs = NB_SEGS(buf);
		unsigned int sent_size = 0;
		uint32_t send_flags = 0;

		/* Clean up old buffer. */
		if (likely(elt->buf != NULL)) {
			struct rte_mbuf *tmp = elt->buf;

#ifndef NDEBUG
			/* Poisoning. */
			memset(elt, 0x66, sizeof(*elt));
#endif
			/* Faster than rte_pktmbuf_free(). */
			do {
				struct rte_mbuf *next = NEXT(tmp);

				rte_pktmbuf_free_seg(tmp);
				tmp = next;
			} while (tmp != NULL);
		}
		/* Request TX completion. */
		if (unlikely(--elts_comp_cd == 0)) {
			elts_comp_cd = txq->elts_comp_cd_init;
			++elts_comp;
			send_flags |= IBV_EXP_QP_BURST_SIGNALED;
		}
		if (likely(segs == 1)) {
			uintptr_t addr;
			uint32_t length;
			uint32_t lkey;

			/* Retrieve buffer information. */
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			length = DATA_LEN(buf);
			/* Retrieve Memory Region key for this memory pool. */
			lkey = txq_mp2mr(txq, txq_mb2mp(buf));
			if (unlikely(lkey == (uint32_t)-1)) {
				/* MR does not exist. */
				DEBUG("%p: unable to get MP <-> MR"
				      " association", (void *)txq);
				/* Clean up TX element. */
				elt->buf = NULL;
				goto stop;
			}
			/* Update element. */
			elt->buf = buf;
			if (txq->priv->vf)
				rte_prefetch0((volatile void *)
					      (uintptr_t)addr);
			RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
			/* Put packet into send queue. */
			if (length <= txq->max_inline)
				err = txq->if_qp->send_pending_inline
					(txq->qp,
					 (void *)addr,
					 length,
					 send_flags);
			else
				err = txq->if_qp->send_pending
					(txq->qp,
					 addr,
					 length,
					 lkey,
					 send_flags);
			if (unlikely(err))
				goto stop;
			sent_size += length;
		} else {
			struct ibv_sge sges[MLX4_PMD_SGE_WR_N];
			struct tx_burst_sg_ret ret;

			ret = tx_burst_sg(txq, segs, elt, buf, elts_head,
					  &sges);
			if (ret.length == (unsigned int)-1)
				goto stop;
			RTE_MBUF_PREFETCH_TO_FREE(elt_next->buf);
			/* Put SG list into send queue. */
			err = txq->if_qp->send_pending_sg_list
				(txq->qp,
				 sges,
				 ret.num,
				 send_flags);
			if (unlikely(err))
				goto stop;
			sent_size += ret.length;
		}
		elts_head = elts_head_next;
		/* Increment sent bytes counter. */
		txq->stats.obytes += sent_size;
	}
stop:
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
	/* Ring QP doorbell. */
	err = txq->if_qp->send_flush(txq->qp);
	if (unlikely(err)) {
		/* A nonzero value is not supposed to be returned.
		 * Nothing can be done about it. */
		DEBUG("%p: send_flush() failed with error %d",
		      (void *)txq, err);
	}
	txq->elts_head = elts_head;
	txq->elts_comp += elts_comp;
	txq->elts_comp_cd = elts_comp_cd;
	return i;
}

/**
 * Configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq
 *   Pointer to TX queue structure.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
txq_setup(struct rte_eth_dev *dev, struct txq *txq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = dev->data->dev_private;
	struct txq tmpl = {
		.priv = priv,
		.socket = socket
	};
	union {
		struct ibv_exp_query_intf_params params;
		struct ibv_exp_qp_init_attr init;
		struct ibv_exp_res_domain_init_attr rd;
		struct ibv_exp_cq_init_attr cq;
		struct ibv_exp_qp_attr mod;
	} attr;
	enum ibv_exp_query_intf_status status;
	int ret = 0;

	(void)conf; /* Thresholds configuration (ignored). */
	if (priv == NULL)
		return EINVAL;
	if ((desc == 0) || (desc % MLX4_PMD_SGE_WR_N)) {
		ERROR("%p: invalid number of TX descriptors (must be a"
		      " multiple of %d)", (void *)dev, MLX4_PMD_SGE_WR_N);
		return EINVAL;
	}
	desc /= MLX4_PMD_SGE_WR_N;
	/* MRs will be registered in mp2mr[] later. */
	attr.rd = (struct ibv_exp_res_domain_init_attr){
		.comp_mask = (IBV_EXP_RES_DOMAIN_THREAD_MODEL |
			      IBV_EXP_RES_DOMAIN_MSG_MODEL),
		.thread_model = IBV_EXP_THREAD_SINGLE,
		.msg_model = IBV_EXP_MSG_HIGH_BW,
	};
	tmpl.rd = ibv_exp_create_res_domain(priv->ctx, &attr.rd);
	if (tmpl.rd == NULL) {
		ret = ENOMEM;
		ERROR("%p: RD creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.cq = (struct ibv_exp_cq_init_attr){
		.comp_mask = IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN,
		.res_domain = tmpl.rd,
	};
	tmpl.cq = ibv_exp_create_cq(priv->ctx, desc, NULL, NULL, 0, &attr.cq);
	if (tmpl.cq == NULL) {
		ret = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	attr.init = (struct ibv_exp_qp_init_attr){
		/* CQ to be associated with the send queue. */
		.send_cq = tmpl.cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = tmpl.cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_send_wr = ((priv->device_attr.max_qp_wr < desc) ?
					priv->device_attr.max_qp_wr :
					desc),
			/* Max number of scatter/gather elements in a WR. */
			.max_send_sge = ((priv->device_attr.max_sge <
					  MLX4_PMD_SGE_WR_N) ?
					 priv->device_attr.max_sge :
					 MLX4_PMD_SGE_WR_N),
			.max_inline_data = MLX4_PMD_MAX_INLINE,
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		/* Do *NOT* enable this, completions events are managed per
		 * TX burst. */
		.sq_sig_all = 0,
		.pd = priv->pd,
		.res_domain = tmpl.rd,
		.comp_mask = (IBV_EXP_QP_INIT_ATTR_PD |
			      IBV_EXP_QP_INIT_ATTR_RES_DOMAIN),
	};
	tmpl.qp = ibv_exp_create_qp(priv->ctx, &attr.init);
	if (tmpl.qp == NULL) {
		ret = (errno ? errno : EINVAL);
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/* ibv_create_qp() updates this value. */
	tmpl.max_inline = attr.init.cap.max_inline_data;
	attr.mod = (struct ibv_exp_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod,
				(IBV_EXP_QP_STATE | IBV_EXP_QP_PORT));
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	ret = txq_alloc_elts(&tmpl, desc);
	if (ret) {
		ERROR("%p: TXQ allocation failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.mod = (struct ibv_exp_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod, IBV_EXP_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod, IBV_EXP_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTS failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_CQ,
		.obj = tmpl.cq,
	};
	tmpl.if_cq = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_cq == NULL) {
		ERROR("%p: CQ interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_QP_BURST,
		.obj = tmpl.qp,
#ifdef HAVE_EXP_QP_BURST_CREATE_DISABLE_ETH_LOOPBACK
		/* MC loopback must be disabled when not using a VF. */
		.family_flags =
			(!priv->vf ?
			 IBV_EXP_QP_BURST_CREATE_DISABLE_ETH_LOOPBACK :
			 0),
#endif
	};
	tmpl.if_qp = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_qp == NULL) {
		ERROR("%p: QP interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	/* Clean up txq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old txq just in case", (void *)txq);
	txq_cleanup(txq);
	*txq = tmpl;
	DEBUG("%p: txq updated with %p", (void *)txq, (void *)&tmpl);
	/* Pre-register known mempools. */
	rte_mempool_walk(txq_mp2mr_iter, txq);
	assert(ret == 0);
	return 0;
error:
	txq_cleanup(&tmpl);
	assert(ret > 0);
	return ret;
}

/**
 * DPDK callback to configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = dev->data->dev_private;
	struct txq *txq = (*priv->txqs)[idx];
	int ret;

	priv_lock(priv);
	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= priv->txqs_n) {
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, priv->txqs_n);
		priv_unlock(priv);
		return -EOVERFLOW;
	}
	if (txq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)txq);
		if (priv->started) {
			priv_unlock(priv);
			return -EEXIST;
		}
		(*priv->txqs)[idx] = NULL;
		txq_cleanup(txq);
	} else {
		txq = rte_calloc_socket("TXQ", 1, sizeof(*txq), 0, socket);
		if (txq == NULL) {
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			priv_unlock(priv);
			return -ENOMEM;
		}
	}
	ret = txq_setup(dev, txq, desc, socket, conf);
	if (ret)
		rte_free(txq);
	else {
		txq->stats.idx = idx;
		DEBUG("%p: adding TX queue %p to list",
		      (void *)dev, (void *)txq);
		(*priv->txqs)[idx] = txq;
		/* Update send callback. */
		dev->tx_pkt_burst = mlx4_tx_burst;
	}
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to release a TX queue.
 *
 * @param dpdk_txq
 *   Generic TX queue pointer.
 */
static void
mlx4_tx_queue_release(void *dpdk_txq)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	struct priv *priv;
	unsigned int i;

	if (txq == NULL)
		return;
	priv = txq->priv;
	priv_lock(priv);
	for (i = 0; (i != priv->txqs_n); ++i)
		if ((*priv->txqs)[i] == txq) {
			DEBUG("%p: removing TX queue %p from list",
			      (void *)priv->dev, (void *)txq);
			(*priv->txqs)[i] = NULL;
			break;
		}
	txq_cleanup(txq);
	rte_free(txq);
	priv_unlock(priv);
}

/* RX queues handling. */

/**
 * Allocate RX queue elements with scattered packets support.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 * @param[in] pool
 *   If not NULL, fetch buffers from this array instead of allocating them
 *   with rte_pktmbuf_alloc().
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_alloc_elts_sp(struct rxq *rxq, unsigned int elts_n,
		  struct rte_mbuf **pool)
{
	unsigned int i;
	struct rxq_elt_sp (*elts)[elts_n] =
		rte_calloc_socket("RXQ elements", 1, sizeof(*elts), 0,
				  rxq->socket);
	int ret = 0;

	if (elts == NULL) {
		ERROR("%p: can't allocate packets array", (void *)rxq);
		ret = ENOMEM;
		goto error;
	}
	/* For each WR (packet). */
	for (i = 0; (i != elts_n); ++i) {
		unsigned int j;
		struct rxq_elt_sp *elt = &(*elts)[i];
		struct ibv_recv_wr *wr = &elt->wr;
		struct ibv_sge (*sges)[(elemof(elt->sges))] = &elt->sges;

		/* These two arrays must have the same size. */
		assert(elemof(elt->sges) == elemof(elt->bufs));
		/* Configure WR. */
		wr->wr_id = i;
		wr->next = &(*elts)[(i + 1)].wr;
		wr->sg_list = &(*sges)[0];
		wr->num_sge = elemof(*sges);
		/* For each SGE (segment). */
		for (j = 0; (j != elemof(elt->bufs)); ++j) {
			struct ibv_sge *sge = &(*sges)[j];
			struct rte_mbuf *buf;

			if (pool != NULL) {
				buf = *(pool++);
				assert(buf != NULL);
				rte_pktmbuf_reset(buf);
			} else
				buf = rte_pktmbuf_alloc(rxq->mp);
			if (buf == NULL) {
				assert(pool == NULL);
				ERROR("%p: empty mbuf pool", (void *)rxq);
				ret = ENOMEM;
				goto error;
			}
			elt->bufs[j] = buf;
			/* Headroom is reserved by rte_pktmbuf_alloc(). */
			assert(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
			/* Buffer is supposed to be empty. */
			assert(rte_pktmbuf_data_len(buf) == 0);
			assert(rte_pktmbuf_pkt_len(buf) == 0);
			/* sge->addr must be able to store a pointer. */
			assert(sizeof(sge->addr) >= sizeof(uintptr_t));
			if (j == 0) {
				/* The first SGE keeps its headroom. */
				sge->addr = rte_pktmbuf_mtod(buf, uintptr_t);
				sge->length = (buf->buf_len -
					       RTE_PKTMBUF_HEADROOM);
			} else {
				/* Subsequent SGEs lose theirs. */
				assert(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
				SET_DATA_OFF(buf, 0);
				sge->addr = (uintptr_t)buf->buf_addr;
				sge->length = buf->buf_len;
			}
			sge->lkey = rxq->mr->lkey;
			/* Redundant check for tailroom. */
			assert(sge->length == rte_pktmbuf_tailroom(buf));
		}
	}
	/* The last WR pointer must be NULL. */
	(*elts)[(i - 1)].wr.next = NULL;
	DEBUG("%p: allocated and configured %u WRs (%zu segments)",
	      (void *)rxq, elts_n, (elts_n * elemof((*elts)[0].sges)));
	rxq->elts_n = elts_n;
	rxq->elts_head = 0;
	rxq->elts.sp = elts;
	assert(ret == 0);
	return 0;
error:
	if (elts != NULL) {
		assert(pool == NULL);
		for (i = 0; (i != elemof(*elts)); ++i) {
			unsigned int j;
			struct rxq_elt_sp *elt = &(*elts)[i];

			for (j = 0; (j != elemof(elt->bufs)); ++j) {
				struct rte_mbuf *buf = elt->bufs[j];

				if (buf != NULL)
					rte_pktmbuf_free_seg(buf);
			}
		}
		rte_free(elts);
	}
	DEBUG("%p: failed, freed everything", (void *)rxq);
	assert(ret > 0);
	return ret;
}

/**
 * Free RX queue elements with scattered packets support.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts_sp(struct rxq *rxq)
{
	unsigned int i;
	unsigned int elts_n = rxq->elts_n;
	struct rxq_elt_sp (*elts)[elts_n] = rxq->elts.sp;

	DEBUG("%p: freeing WRs", (void *)rxq);
	rxq->elts_n = 0;
	rxq->elts.sp = NULL;
	if (elts == NULL)
		return;
	for (i = 0; (i != elemof(*elts)); ++i) {
		unsigned int j;
		struct rxq_elt_sp *elt = &(*elts)[i];

		for (j = 0; (j != elemof(elt->bufs)); ++j) {
			struct rte_mbuf *buf = elt->bufs[j];

			if (buf != NULL)
				rte_pktmbuf_free_seg(buf);
		}
	}
	rte_free(elts);
}

/**
 * Allocate RX queue elements.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 * @param[in] pool
 *   If not NULL, fetch buffers from this array instead of allocating them
 *   with rte_pktmbuf_alloc().
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_alloc_elts(struct rxq *rxq, unsigned int elts_n, struct rte_mbuf **pool)
{
	unsigned int i;
	struct rxq_elt (*elts)[elts_n] =
		rte_calloc_socket("RXQ elements", 1, sizeof(*elts), 0,
				  rxq->socket);
	int ret = 0;

	if (elts == NULL) {
		ERROR("%p: can't allocate packets array", (void *)rxq);
		ret = ENOMEM;
		goto error;
	}
	/* For each WR (packet). */
	for (i = 0; (i != elts_n); ++i) {
		struct rxq_elt *elt = &(*elts)[i];
		struct ibv_recv_wr *wr = &elt->wr;
		struct ibv_sge *sge = &(*elts)[i].sge;
		struct rte_mbuf *buf;

		if (pool != NULL) {
			buf = *(pool++);
			assert(buf != NULL);
			rte_pktmbuf_reset(buf);
		} else
			buf = rte_pktmbuf_alloc(rxq->mp);
		if (buf == NULL) {
			assert(pool == NULL);
			ERROR("%p: empty mbuf pool", (void *)rxq);
			ret = ENOMEM;
			goto error;
		}
		/* Configure WR. Work request ID contains its own index in
		 * the elts array and the offset between SGE buffer header and
		 * its data. */
		WR_ID(wr->wr_id).id = i;
		WR_ID(wr->wr_id).offset =
			(((uintptr_t)buf->buf_addr + RTE_PKTMBUF_HEADROOM) -
			 (uintptr_t)buf);
		wr->next = &(*elts)[(i + 1)].wr;
		wr->sg_list = sge;
		wr->num_sge = 1;
		/* Headroom is reserved by rte_pktmbuf_alloc(). */
		assert(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		assert(rte_pktmbuf_data_len(buf) == 0);
		assert(rte_pktmbuf_pkt_len(buf) == 0);
		/* sge->addr must be able to store a pointer. */
		assert(sizeof(sge->addr) >= sizeof(uintptr_t));
		/* SGE keeps its headroom. */
		sge->addr = (uintptr_t)
			((uint8_t *)buf->buf_addr + RTE_PKTMBUF_HEADROOM);
		sge->length = (buf->buf_len - RTE_PKTMBUF_HEADROOM);
		sge->lkey = rxq->mr->lkey;
		/* Redundant check for tailroom. */
		assert(sge->length == rte_pktmbuf_tailroom(buf));
		/* Make sure elts index and SGE mbuf pointer can be deduced
		 * from WR ID. */
		if ((WR_ID(wr->wr_id).id != i) ||
		    ((void *)((uintptr_t)sge->addr -
			WR_ID(wr->wr_id).offset) != buf)) {
			ERROR("%p: cannot store index and offset in WR ID",
			      (void *)rxq);
			sge->addr = 0;
			rte_pktmbuf_free(buf);
			ret = EOVERFLOW;
			goto error;
		}
	}
	/* The last WR pointer must be NULL. */
	(*elts)[(i - 1)].wr.next = NULL;
	DEBUG("%p: allocated and configured %u single-segment WRs",
	      (void *)rxq, elts_n);
	rxq->elts_n = elts_n;
	rxq->elts_head = 0;
	rxq->elts.no_sp = elts;
	assert(ret == 0);
	return 0;
error:
	if (elts != NULL) {
		assert(pool == NULL);
		for (i = 0; (i != elemof(*elts)); ++i) {
			struct rxq_elt *elt = &(*elts)[i];
			struct rte_mbuf *buf;

			if (elt->sge.addr == 0)
				continue;
			assert(WR_ID(elt->wr.wr_id).id == i);
			buf = (void *)((uintptr_t)elt->sge.addr -
				WR_ID(elt->wr.wr_id).offset);
			rte_pktmbuf_free_seg(buf);
		}
		rte_free(elts);
	}
	DEBUG("%p: failed, freed everything", (void *)rxq);
	assert(ret > 0);
	return ret;
}

/**
 * Free RX queue elements.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts(struct rxq *rxq)
{
	unsigned int i;
	unsigned int elts_n = rxq->elts_n;
	struct rxq_elt (*elts)[elts_n] = rxq->elts.no_sp;

	DEBUG("%p: freeing WRs", (void *)rxq);
	rxq->elts_n = 0;
	rxq->elts.no_sp = NULL;
	if (elts == NULL)
		return;
	for (i = 0; (i != elemof(*elts)); ++i) {
		struct rxq_elt *elt = &(*elts)[i];
		struct rte_mbuf *buf;

		if (elt->sge.addr == 0)
			continue;
		assert(WR_ID(elt->wr.wr_id).id == i);
		buf = (void *)((uintptr_t)elt->sge.addr -
			WR_ID(elt->wr.wr_id).offset);
		rte_pktmbuf_free_seg(buf);
	}
	rte_free(elts);
}

/**
 * Unregister a MAC address.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_mac_addr_del(struct priv *priv)
{
#ifndef NDEBUG
	uint8_t (*mac)[ETHER_ADDR_LEN] = &priv->mac.addr_bytes;
#endif

	if (!priv->mac_flow)
		return;
	DEBUG("%p: removing MAC address %02x:%02x:%02x:%02x:%02x:%02x",
	      (void *)priv,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5]);
	claim_zero(ibv_destroy_flow(priv->mac_flow));
	priv->mac_flow = NULL;
}

/**
 * Register a MAC address.
 *
 * The MAC address is registered in queue 0.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_mac_addr_add(struct priv *priv)
{
	uint8_t (*mac)[ETHER_ADDR_LEN] = &priv->mac.addr_bytes;
	struct rxq *rxq;
	struct ibv_flow *flow;

	/* If device isn't started, this is all we need to do. */
	if (!priv->started)
		return 0;
	if (priv->isolated)
		return 0;
	if (*priv->rxqs && (*priv->rxqs)[0])
		rxq = (*priv->rxqs)[0];
	else
		return 0;

	/* Allocate flow specification on the stack. */
	struct __attribute__((packed)) {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth spec;
	} data;
	struct ibv_flow_attr *attr = &data.attr;
	struct ibv_flow_spec_eth *spec = &data.spec;

	if (priv->mac_flow)
		priv_mac_addr_del(priv);
	/*
	 * No padding must be inserted by the compiler between attr and spec.
	 * This layout is expected by libibverbs.
	 */
	assert(((uint8_t *)attr + sizeof(*attr)) == (uint8_t *)spec);
	*attr = (struct ibv_flow_attr){
		.type = IBV_FLOW_ATTR_NORMAL,
		.priority = 3,
		.num_of_specs = 1,
		.port = priv->port,
		.flags = 0
	};
	*spec = (struct ibv_flow_spec_eth){
		.type = IBV_FLOW_SPEC_ETH,
		.size = sizeof(*spec),
		.val = {
			.dst_mac = {
				(*mac)[0], (*mac)[1], (*mac)[2],
				(*mac)[3], (*mac)[4], (*mac)[5]
			},
		},
		.mask = {
			.dst_mac = "\xff\xff\xff\xff\xff\xff",
		}
	};
	DEBUG("%p: adding MAC address %02x:%02x:%02x:%02x:%02x:%02x",
	      (void *)priv,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5]);
	/* Create related flow. */
	errno = 0;
	flow = ibv_create_flow(rxq->qp, attr);
	if (flow == NULL) {
		/* It's not clear whether errno is always set in this case. */
		ERROR("%p: flow configuration failed, errno=%d: %s",
		      (void *)rxq, errno,
		      (errno ? strerror(errno) : "Unknown error"));
		if (errno)
			return errno;
		return EINVAL;
	}
	assert(priv->mac_flow == NULL);
	priv->mac_flow = flow;
	return 0;
}

/**
 * Clean up a RX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
static void
rxq_cleanup(struct rxq *rxq)
{
	struct ibv_exp_release_intf_params params;

	DEBUG("cleaning up %p", (void *)rxq);
	if (rxq->sp)
		rxq_free_elts_sp(rxq);
	else
		rxq_free_elts(rxq);
	if (rxq->if_qp != NULL) {
		assert(rxq->priv != NULL);
		assert(rxq->priv->ctx != NULL);
		assert(rxq->qp != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(rxq->priv->ctx,
						rxq->if_qp,
						&params));
	}
	if (rxq->if_cq != NULL) {
		assert(rxq->priv != NULL);
		assert(rxq->priv->ctx != NULL);
		assert(rxq->cq != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(rxq->priv->ctx,
						rxq->if_cq,
						&params));
	}
	if (rxq->qp != NULL)
		claim_zero(ibv_destroy_qp(rxq->qp));
	if (rxq->cq != NULL)
		claim_zero(ibv_destroy_cq(rxq->cq));
	if (rxq->channel != NULL)
		claim_zero(ibv_destroy_comp_channel(rxq->channel));
	if (rxq->rd != NULL) {
		struct ibv_exp_destroy_res_domain_attr attr = {
			.comp_mask = 0,
		};

		assert(rxq->priv != NULL);
		assert(rxq->priv->ctx != NULL);
		claim_zero(ibv_exp_destroy_res_domain(rxq->priv->ctx,
						      rxq->rd,
						      &attr));
	}
	if (rxq->mr != NULL)
		claim_zero(ibv_dereg_mr(rxq->mr));
	memset(rxq, 0, sizeof(*rxq));
}

/**
 * Translate RX completion flags to packet type.
 *
 * @param flags
 *   RX completion flags returned by poll_length_flags().
 *
 * @note: fix mlx4_dev_supported_ptypes_get() if any change here.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(uint32_t flags)
{
	uint32_t pkt_type;

	if (flags & IBV_EXP_CQ_RX_TUNNEL_PACKET)
		pkt_type =
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_OUTER_IPV4_PACKET,
				  RTE_PTYPE_L3_IPV4_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_OUTER_IPV6_PACKET,
				  RTE_PTYPE_L3_IPV6_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV4_PACKET,
				  RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV6_PACKET,
				  RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN);
	else
		pkt_type =
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV4_PACKET,
				  RTE_PTYPE_L3_IPV4_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  IBV_EXP_CQ_RX_IPV6_PACKET,
				  RTE_PTYPE_L3_IPV6_EXT_UNKNOWN);
	return pkt_type;
}

static uint16_t
mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n);

/**
 * DPDK callback for RX with scattered packets support.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
static uint16_t
mlx4_rx_burst_sp(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct rxq_elt_sp (*elts)[rxq->elts_n] = rxq->elts.sp;
	const unsigned int elts_n = rxq->elts_n;
	unsigned int elts_head = rxq->elts_head;
	struct ibv_recv_wr head;
	struct ibv_recv_wr **next = &head.next;
	struct ibv_recv_wr *bad_wr;
	unsigned int i;
	unsigned int pkts_ret = 0;
	int ret;

	if (unlikely(!rxq->sp))
		return mlx4_rx_burst(dpdk_rxq, pkts, pkts_n);
	if (unlikely(elts == NULL)) /* See RTE_DEV_CMD_SET_MTU. */
		return 0;
	for (i = 0; (i != pkts_n); ++i) {
		struct rxq_elt_sp *elt = &(*elts)[elts_head];
		struct ibv_recv_wr *wr = &elt->wr;
		uint64_t wr_id = wr->wr_id;
		unsigned int len;
		unsigned int pkt_buf_len;
		struct rte_mbuf *pkt_buf = NULL; /* Buffer returned in pkts. */
		struct rte_mbuf **pkt_buf_next = &pkt_buf;
		unsigned int seg_headroom = RTE_PKTMBUF_HEADROOM;
		unsigned int j = 0;
		uint32_t flags;

		/* Sanity checks. */
#ifdef NDEBUG
		(void)wr_id;
#endif
		assert(wr_id < rxq->elts_n);
		assert(wr->sg_list == elt->sges);
		assert(wr->num_sge == elemof(elt->sges));
		assert(elts_head < rxq->elts_n);
		assert(rxq->elts_head < rxq->elts_n);
		ret = rxq->if_cq->poll_length_flags(rxq->cq, NULL, NULL,
						    &flags);
		if (unlikely(ret < 0)) {
			struct ibv_wc wc;
			int wcs_n;

			DEBUG("rxq=%p, poll_length() failed (ret=%d)",
			      (void *)rxq, ret);
			/* ibv_poll_cq() must be used in case of failure. */
			wcs_n = ibv_poll_cq(rxq->cq, 1, &wc);
			if (unlikely(wcs_n == 0))
				break;
			if (unlikely(wcs_n < 0)) {
				DEBUG("rxq=%p, ibv_poll_cq() failed (wcs_n=%d)",
				      (void *)rxq, wcs_n);
				break;
			}
			assert(wcs_n == 1);
			if (unlikely(wc.status != IBV_WC_SUCCESS)) {
				/* Whatever, just repost the offending WR. */
				DEBUG("rxq=%p, wr_id=%" PRIu64 ": bad work"
				      " completion status (%d): %s",
				      (void *)rxq, wc.wr_id, wc.status,
				      ibv_wc_status_str(wc.status));
				/* Increment dropped packets counter. */
				++rxq->stats.idropped;
				/* Link completed WRs together for repost. */
				*next = wr;
				next = &wr->next;
				goto repost;
			}
			ret = wc.byte_len;
		}
		if (ret == 0)
			break;
		len = ret;
		pkt_buf_len = len;
		/* Link completed WRs together for repost. */
		*next = wr;
		next = &wr->next;
		/*
		 * Replace spent segments with new ones, concatenate and
		 * return them as pkt_buf.
		 */
		while (1) {
			struct ibv_sge *sge = &elt->sges[j];
			struct rte_mbuf *seg = elt->bufs[j];
			struct rte_mbuf *rep;
			unsigned int seg_tailroom;

			/*
			 * Fetch initial bytes of packet descriptor into a
			 * cacheline while allocating rep.
			 */
			rte_prefetch0(seg);
			rep = rte_mbuf_raw_alloc(rxq->mp);
			if (unlikely(rep == NULL)) {
				/*
				 * Unable to allocate a replacement mbuf,
				 * repost WR.
				 */
				DEBUG("rxq=%p, wr_id=%" PRIu64 ":"
				      " can't allocate a new mbuf",
				      (void *)rxq, wr_id);
				if (pkt_buf != NULL) {
					*pkt_buf_next = NULL;
					rte_pktmbuf_free(pkt_buf);
				}
				/* Increase out of memory counters. */
				++rxq->stats.rx_nombuf;
				++rxq->priv->dev->data->rx_mbuf_alloc_failed;
				goto repost;
			}
#ifndef NDEBUG
			/* Poison user-modifiable fields in rep. */
			NEXT(rep) = (void *)((uintptr_t)-1);
			SET_DATA_OFF(rep, 0xdead);
			DATA_LEN(rep) = 0xd00d;
			PKT_LEN(rep) = 0xdeadd00d;
			NB_SEGS(rep) = 0x2a;
			PORT(rep) = 0x2a;
			rep->ol_flags = -1;
			/*
			 * Clear special flags in mbuf to avoid
			 * crashing while freeing.
			 */
			rep->ol_flags &=
				~(uint64_t)(IND_ATTACHED_MBUF |
					    CTRL_MBUF_FLAG);
#endif
			assert(rep->buf_len == seg->buf_len);
			/* Reconfigure sge to use rep instead of seg. */
			assert(sge->lkey == rxq->mr->lkey);
			sge->addr = ((uintptr_t)rep->buf_addr + seg_headroom);
			elt->bufs[j] = rep;
			++j;
			/* Update pkt_buf if it's the first segment, or link
			 * seg to the previous one and update pkt_buf_next. */
			*pkt_buf_next = seg;
			pkt_buf_next = &NEXT(seg);
			/* Update seg information. */
			seg_tailroom = (seg->buf_len - seg_headroom);
			assert(sge->length == seg_tailroom);
			SET_DATA_OFF(seg, seg_headroom);
			if (likely(len <= seg_tailroom)) {
				/* Last segment. */
				DATA_LEN(seg) = len;
				PKT_LEN(seg) = len;
				/* Sanity check. */
				assert(rte_pktmbuf_headroom(seg) ==
				       seg_headroom);
				assert(rte_pktmbuf_tailroom(seg) ==
				       (seg_tailroom - len));
				break;
			}
			DATA_LEN(seg) = seg_tailroom;
			PKT_LEN(seg) = seg_tailroom;
			/* Sanity check. */
			assert(rte_pktmbuf_headroom(seg) == seg_headroom);
			assert(rte_pktmbuf_tailroom(seg) == 0);
			/* Fix len and clear headroom for next segments. */
			len -= seg_tailroom;
			seg_headroom = 0;
		}
		/* Update head and tail segments. */
		*pkt_buf_next = NULL;
		assert(pkt_buf != NULL);
		assert(j != 0);
		NB_SEGS(pkt_buf) = j;
		PORT(pkt_buf) = rxq->port_id;
		PKT_LEN(pkt_buf) = pkt_buf_len;
		pkt_buf->packet_type = rxq_cq_to_pkt_type(flags);
		pkt_buf->ol_flags = 0;

		/* Return packet. */
		*(pkts++) = pkt_buf;
		++pkts_ret;
		/* Increase bytes counter. */
		rxq->stats.ibytes += pkt_buf_len;
repost:
		if (++elts_head >= elts_n)
			elts_head = 0;
		continue;
	}
	if (unlikely(i == 0))
		return 0;
	*next = NULL;
	/* Repost WRs. */
	ret = ibv_post_recv(rxq->qp, head.next, &bad_wr);
	if (unlikely(ret)) {
		/* Inability to repost WRs is fatal. */
		DEBUG("%p: ibv_post_recv(): failed for WR %p: %s",
		      (void *)rxq->priv,
		      (void *)bad_wr,
		      strerror(ret));
		abort();
	}
	rxq->elts_head = elts_head;
	/* Increase packets counter. */
	rxq->stats.ipackets += pkts_ret;
	return pkts_ret;
}

/**
 * DPDK callback for RX.
 *
 * The following function is the same as mlx4_rx_burst_sp(), except it doesn't
 * manage scattered packets. Improves performance when MRU is lower than the
 * size of the first segment.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
static uint16_t
mlx4_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct rxq_elt (*elts)[rxq->elts_n] = rxq->elts.no_sp;
	const unsigned int elts_n = rxq->elts_n;
	unsigned int elts_head = rxq->elts_head;
	struct ibv_sge sges[pkts_n];
	unsigned int i;
	unsigned int pkts_ret = 0;
	int ret;

	if (unlikely(rxq->sp))
		return mlx4_rx_burst_sp(dpdk_rxq, pkts, pkts_n);
	for (i = 0; (i != pkts_n); ++i) {
		struct rxq_elt *elt = &(*elts)[elts_head];
		struct ibv_recv_wr *wr = &elt->wr;
		uint64_t wr_id = wr->wr_id;
		unsigned int len;
		struct rte_mbuf *seg = (void *)((uintptr_t)elt->sge.addr -
			WR_ID(wr_id).offset);
		struct rte_mbuf *rep;
		uint32_t flags;

		/* Sanity checks. */
		assert(WR_ID(wr_id).id < rxq->elts_n);
		assert(wr->sg_list == &elt->sge);
		assert(wr->num_sge == 1);
		assert(elts_head < rxq->elts_n);
		assert(rxq->elts_head < rxq->elts_n);
		/*
		 * Fetch initial bytes of packet descriptor into a
		 * cacheline while allocating rep.
		 */
		rte_mbuf_prefetch_part1(seg);
		rte_mbuf_prefetch_part2(seg);
		ret = rxq->if_cq->poll_length_flags(rxq->cq, NULL, NULL,
						    &flags);
		if (unlikely(ret < 0)) {
			struct ibv_wc wc;
			int wcs_n;

			DEBUG("rxq=%p, poll_length() failed (ret=%d)",
			      (void *)rxq, ret);
			/* ibv_poll_cq() must be used in case of failure. */
			wcs_n = ibv_poll_cq(rxq->cq, 1, &wc);
			if (unlikely(wcs_n == 0))
				break;
			if (unlikely(wcs_n < 0)) {
				DEBUG("rxq=%p, ibv_poll_cq() failed (wcs_n=%d)",
				      (void *)rxq, wcs_n);
				break;
			}
			assert(wcs_n == 1);
			if (unlikely(wc.status != IBV_WC_SUCCESS)) {
				/* Whatever, just repost the offending WR. */
				DEBUG("rxq=%p, wr_id=%" PRIu64 ": bad work"
				      " completion status (%d): %s",
				      (void *)rxq, wc.wr_id, wc.status,
				      ibv_wc_status_str(wc.status));
				/* Increment dropped packets counter. */
				++rxq->stats.idropped;
				/* Add SGE to array for repost. */
				sges[i] = elt->sge;
				goto repost;
			}
			ret = wc.byte_len;
		}
		if (ret == 0)
			break;
		len = ret;
		rep = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			/*
			 * Unable to allocate a replacement mbuf,
			 * repost WR.
			 */
			DEBUG("rxq=%p, wr_id=%" PRIu32 ":"
			      " can't allocate a new mbuf",
			      (void *)rxq, WR_ID(wr_id).id);
			/* Increase out of memory counters. */
			++rxq->stats.rx_nombuf;
			++rxq->priv->dev->data->rx_mbuf_alloc_failed;
			/* Add SGE to array for repost. */
			sges[i] = elt->sge;
			goto repost;
		}

		/* Reconfigure sge to use rep instead of seg. */
		elt->sge.addr = (uintptr_t)rep->buf_addr + RTE_PKTMBUF_HEADROOM;
		assert(elt->sge.lkey == rxq->mr->lkey);
		WR_ID(wr->wr_id).offset =
			(((uintptr_t)rep->buf_addr + RTE_PKTMBUF_HEADROOM) -
			 (uintptr_t)rep);
		assert(WR_ID(wr->wr_id).id == WR_ID(wr_id).id);

		/* Add SGE to array for repost. */
		sges[i] = elt->sge;

		/* Update seg information. */
		SET_DATA_OFF(seg, RTE_PKTMBUF_HEADROOM);
		NB_SEGS(seg) = 1;
		PORT(seg) = rxq->port_id;
		NEXT(seg) = NULL;
		PKT_LEN(seg) = len;
		DATA_LEN(seg) = len;
		seg->packet_type = rxq_cq_to_pkt_type(flags);
		seg->ol_flags = 0;

		/* Return packet. */
		*(pkts++) = seg;
		++pkts_ret;
		/* Increase bytes counter. */
		rxq->stats.ibytes += len;
repost:
		if (++elts_head >= elts_n)
			elts_head = 0;
		continue;
	}
	if (unlikely(i == 0))
		return 0;
	/* Repost WRs. */
	ret = rxq->if_qp->recv_burst(rxq->qp, sges, i);
	if (unlikely(ret)) {
		/* Inability to repost WRs is fatal. */
		DEBUG("%p: recv_burst(): failed (ret=%d)",
		      (void *)rxq->priv,
		      ret);
		abort();
	}
	rxq->elts_head = elts_head;
	/* Increase packets counter. */
	rxq->stats.ipackets += pkts_ret;
	return pkts_ret;
}

/**
 * Allocate a Queue Pair.
 * Optionally setup inline receive if supported.
 *
 * @param priv
 *   Pointer to private structure.
 * @param cq
 *   Completion queue to associate with QP.
 * @param desc
 *   Number of descriptors in QP (hint only).
 *
 * @return
 *   QP pointer or NULL in case of error.
 */
static struct ibv_qp *
rxq_setup_qp(struct priv *priv, struct ibv_cq *cq, uint16_t desc,
	     struct ibv_exp_res_domain *rd)
{
	struct ibv_exp_qp_init_attr attr = {
		/* CQ to be associated with the send queue. */
		.send_cq = cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_recv_wr = ((priv->device_attr.max_qp_wr < desc) ?
					priv->device_attr.max_qp_wr :
					desc),
			/* Max number of scatter/gather elements in a WR. */
			.max_recv_sge = ((priv->device_attr.max_sge <
					  MLX4_PMD_SGE_WR_N) ?
					 priv->device_attr.max_sge :
					 MLX4_PMD_SGE_WR_N),
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		.comp_mask = (IBV_EXP_QP_INIT_ATTR_PD |
			      IBV_EXP_QP_INIT_ATTR_RES_DOMAIN),
		.pd = priv->pd,
		.res_domain = rd,
	};

	attr.max_inl_recv = priv->inl_recv_size,
	attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_INL_RECV;
	return ibv_exp_create_qp(priv->ctx, &attr);
}

/**
 * Reconfigure a RX queue with new parameters.
 *
 * rxq_rehash() does not allocate mbufs, which, if not done from the right
 * thread (such as a control thread), may corrupt the pool.
 * In case of failure, the queue is left untouched.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rxq
 *   RX queue pointer.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_rehash(struct rte_eth_dev *dev, struct rxq *rxq)
{
	struct priv *priv = rxq->priv;
	struct rxq tmpl = *rxq;
	unsigned int mbuf_n;
	unsigned int desc_n;
	struct rte_mbuf **pool;
	unsigned int i, k;
	struct ibv_exp_qp_attr mod;
	struct ibv_recv_wr *bad_wr;
	unsigned int mb_len;
	int err;

	mb_len = rte_pktmbuf_data_room_size(rxq->mp);
	DEBUG("%p: rehashing queue %p", (void *)dev, (void *)rxq);
	/* Number of descriptors and mbufs currently allocated. */
	desc_n = (tmpl.elts_n * (tmpl.sp ? MLX4_PMD_SGE_WR_N : 1));
	mbuf_n = desc_n;
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.enable_scatter &&
	    (dev->data->dev_conf.rxmode.max_rx_pkt_len >
	     (mb_len - RTE_PKTMBUF_HEADROOM))) {
		tmpl.sp = 1;
		desc_n /= MLX4_PMD_SGE_WR_N;
	} else
		tmpl.sp = 0;
	DEBUG("%p: %s scattered packets support (%u WRs)",
	      (void *)dev, (tmpl.sp ? "enabling" : "disabling"), desc_n);
	/* If scatter mode is the same as before, nothing to do. */
	if (tmpl.sp == rxq->sp) {
		DEBUG("%p: nothing to do", (void *)dev);
		return 0;
	}
	/* From now on, any failure will render the queue unusable.
	 * Reinitialize QP. */
	mod = (struct ibv_exp_qp_attr){ .qp_state = IBV_QPS_RESET };
	err = ibv_exp_modify_qp(tmpl.qp, &mod, IBV_EXP_QP_STATE);
	if (err) {
		ERROR("%p: cannot reset QP: %s", (void *)dev, strerror(err));
		assert(err > 0);
		return err;
	}
	err = ibv_resize_cq(tmpl.cq, desc_n);
	if (err) {
		ERROR("%p: cannot resize CQ: %s", (void *)dev, strerror(err));
		assert(err > 0);
		return err;
	}
	mod = (struct ibv_exp_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	err = ibv_exp_modify_qp(tmpl.qp, &mod,
				IBV_EXP_QP_STATE |
				IBV_EXP_QP_PORT);
	if (err) {
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(err));
		assert(err > 0);
		return err;
	};
	/* Allocate pool. */
	pool = rte_malloc(__func__, (mbuf_n * sizeof(*pool)), 0);
	if (pool == NULL) {
		ERROR("%p: cannot allocate memory", (void *)dev);
		return ENOBUFS;
	}
	/* Snatch mbufs from original queue. */
	k = 0;
	if (rxq->sp) {
		struct rxq_elt_sp (*elts)[rxq->elts_n] = rxq->elts.sp;

		for (i = 0; (i != elemof(*elts)); ++i) {
			struct rxq_elt_sp *elt = &(*elts)[i];
			unsigned int j;

			for (j = 0; (j != elemof(elt->bufs)); ++j) {
				assert(elt->bufs[j] != NULL);
				pool[k++] = elt->bufs[j];
			}
		}
	} else {
		struct rxq_elt (*elts)[rxq->elts_n] = rxq->elts.no_sp;

		for (i = 0; (i != elemof(*elts)); ++i) {
			struct rxq_elt *elt = &(*elts)[i];
			struct rte_mbuf *buf = (void *)
				((uintptr_t)elt->sge.addr -
				 WR_ID(elt->wr.wr_id).offset);

			assert(WR_ID(elt->wr.wr_id).id == i);
			pool[k++] = buf;
		}
	}
	assert(k == mbuf_n);
	tmpl.elts_n = 0;
	tmpl.elts.sp = NULL;
	assert((void *)&tmpl.elts.sp == (void *)&tmpl.elts.no_sp);
	err = ((tmpl.sp) ?
	       rxq_alloc_elts_sp(&tmpl, desc_n, pool) :
	       rxq_alloc_elts(&tmpl, desc_n, pool));
	if (err) {
		ERROR("%p: cannot reallocate WRs, aborting", (void *)dev);
		rte_free(pool);
		assert(err > 0);
		return err;
	}
	assert(tmpl.elts_n == desc_n);
	assert(tmpl.elts.sp != NULL);
	rte_free(pool);
	/* Clean up original data. */
	rxq->elts_n = 0;
	rte_free(rxq->elts.sp);
	rxq->elts.sp = NULL;
	/* Post WRs. */
	err = ibv_post_recv(tmpl.qp,
			    (tmpl.sp ?
			     &(*tmpl.elts.sp)[0].wr :
			     &(*tmpl.elts.no_sp)[0].wr),
			    &bad_wr);
	if (err) {
		ERROR("%p: ibv_post_recv() failed for WR %p: %s",
		      (void *)dev,
		      (void *)bad_wr,
		      strerror(err));
		goto skip_rtr;
	}
	mod = (struct ibv_exp_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	err = ibv_exp_modify_qp(tmpl.qp, &mod, IBV_EXP_QP_STATE);
	if (err)
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(err));
skip_rtr:
	*rxq = tmpl;
	assert(err >= 0);
	return err;
}

/**
 * Configure a RX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rxq
 *   Pointer to RX queue structure.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_setup(struct rte_eth_dev *dev, struct rxq *rxq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_rxconf *conf,
	  struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq tmpl = {
		.priv = priv,
		.mp = mp,
		.socket = socket
	};
	struct ibv_exp_qp_attr mod;
	union {
		struct ibv_exp_query_intf_params params;
		struct ibv_exp_cq_init_attr cq;
		struct ibv_exp_res_domain_init_attr rd;
	} attr;
	enum ibv_exp_query_intf_status status;
	struct ibv_recv_wr *bad_wr;
	unsigned int mb_len;
	int ret = 0;

	(void)conf; /* Thresholds configuration (ignored). */
	mb_len = rte_pktmbuf_data_room_size(mp);
	if ((desc == 0) || (desc % MLX4_PMD_SGE_WR_N)) {
		ERROR("%p: invalid number of RX descriptors (must be a"
		      " multiple of %d)", (void *)dev, MLX4_PMD_SGE_WR_N);
		return EINVAL;
	}
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
	    (mb_len - RTE_PKTMBUF_HEADROOM)) {
		tmpl.sp = 0;
	} else if (dev->data->dev_conf.rxmode.enable_scatter) {
		tmpl.sp = 1;
		desc /= MLX4_PMD_SGE_WR_N;
	} else {
		WARN("%p: the requested maximum Rx packet size (%u) is"
		     " larger than a single mbuf (%u) and scattered"
		     " mode has not been requested",
		     (void *)dev,
		     dev->data->dev_conf.rxmode.max_rx_pkt_len,
		     mb_len - RTE_PKTMBUF_HEADROOM);
	}
	DEBUG("%p: %s scattered packets support (%u WRs)",
	      (void *)dev, (tmpl.sp ? "enabling" : "disabling"), desc);
	/* Use the entire RX mempool as the memory region. */
	tmpl.mr = mlx4_mp2mr(priv->pd, mp);
	if (tmpl.mr == NULL) {
		ret = EINVAL;
		ERROR("%p: MR creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.rd = (struct ibv_exp_res_domain_init_attr){
		.comp_mask = (IBV_EXP_RES_DOMAIN_THREAD_MODEL |
			      IBV_EXP_RES_DOMAIN_MSG_MODEL),
		.thread_model = IBV_EXP_THREAD_SINGLE,
		.msg_model = IBV_EXP_MSG_HIGH_BW,
	};
	tmpl.rd = ibv_exp_create_res_domain(priv->ctx, &attr.rd);
	if (tmpl.rd == NULL) {
		ret = ENOMEM;
		ERROR("%p: RD creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	if (dev->data->dev_conf.intr_conf.rxq) {
		tmpl.channel = ibv_create_comp_channel(priv->ctx);
		if (tmpl.channel == NULL) {
			ret = ENOMEM;
			ERROR("%p: Rx interrupt completion channel creation"
			      " failure: %s",
			      (void *)dev, strerror(ret));
			goto error;
		}
	}
	attr.cq = (struct ibv_exp_cq_init_attr){
		.comp_mask = IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN,
		.res_domain = tmpl.rd,
	};
	tmpl.cq = ibv_exp_create_cq(priv->ctx, desc, NULL, tmpl.channel, 0,
				    &attr.cq);
	if (tmpl.cq == NULL) {
		ret = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	tmpl.qp = rxq_setup_qp(priv, tmpl.cq, desc, tmpl.rd);
	if (tmpl.qp == NULL) {
		ret = (errno ? errno : EINVAL);
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	mod = (struct ibv_exp_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &mod,
				IBV_EXP_QP_STATE |
				IBV_EXP_QP_PORT);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	if (tmpl.sp)
		ret = rxq_alloc_elts_sp(&tmpl, desc, NULL);
	else
		ret = rxq_alloc_elts(&tmpl, desc, NULL);
	if (ret) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	ret = ibv_post_recv(tmpl.qp,
			    (tmpl.sp ?
			     &(*tmpl.elts.sp)[0].wr :
			     &(*tmpl.elts.no_sp)[0].wr),
			    &bad_wr);
	if (ret) {
		ERROR("%p: ibv_post_recv() failed for WR %p: %s",
		      (void *)dev,
		      (void *)bad_wr,
		      strerror(ret));
		goto error;
	}
	mod = (struct ibv_exp_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &mod, IBV_EXP_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/* Save port ID. */
	tmpl.port_id = dev->data->port_id;
	DEBUG("%p: RTE port ID: %u", (void *)rxq, tmpl.port_id);
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_CQ,
		.obj = tmpl.cq,
	};
	tmpl.if_cq = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_cq == NULL) {
		ERROR("%p: CQ interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_QP_BURST,
		.obj = tmpl.qp,
	};
	tmpl.if_qp = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_qp == NULL) {
		ERROR("%p: QP interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	/* Clean up rxq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old rxq just in case", (void *)rxq);
	rxq_cleanup(rxq);
	*rxq = tmpl;
	DEBUG("%p: rxq updated with %p", (void *)rxq, (void *)&tmpl);
	assert(ret == 0);
	return 0;
error:
	rxq_cleanup(&tmpl);
	assert(ret > 0);
	return ret;
}

/**
 * DPDK callback to configure a RX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq *rxq = (*priv->rxqs)[idx];
	int ret;

	priv_lock(priv);
	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= priv->rxqs_n) {
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, priv->rxqs_n);
		priv_unlock(priv);
		return -EOVERFLOW;
	}
	if (rxq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)rxq);
		if (priv->started) {
			priv_unlock(priv);
			return -EEXIST;
		}
		(*priv->rxqs)[idx] = NULL;
		if (idx == 0)
			priv_mac_addr_del(priv);
		rxq_cleanup(rxq);
	} else {
		rxq = rte_calloc_socket("RXQ", 1, sizeof(*rxq), 0, socket);
		if (rxq == NULL) {
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			priv_unlock(priv);
			return -ENOMEM;
		}
	}
	ret = rxq_setup(dev, rxq, desc, socket, conf, mp);
	if (ret)
		rte_free(rxq);
	else {
		rxq->stats.idx = idx;
		DEBUG("%p: adding RX queue %p to list",
		      (void *)dev, (void *)rxq);
		(*priv->rxqs)[idx] = rxq;
		/* Update receive callback. */
		if (rxq->sp)
			dev->rx_pkt_burst = mlx4_rx_burst_sp;
		else
			dev->rx_pkt_burst = mlx4_rx_burst;
	}
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to release a RX queue.
 *
 * @param dpdk_rxq
 *   Generic RX queue pointer.
 */
static void
mlx4_rx_queue_release(void *dpdk_rxq)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct priv *priv;
	unsigned int i;

	if (rxq == NULL)
		return;
	priv = rxq->priv;
	priv_lock(priv);
	for (i = 0; (i != priv->rxqs_n); ++i)
		if ((*priv->rxqs)[i] == rxq) {
			DEBUG("%p: removing RX queue %p from list",
			      (void *)priv->dev, (void *)rxq);
			(*priv->rxqs)[i] = NULL;
			if (i == 0)
				priv_mac_addr_del(priv);
			break;
		}
	rxq_cleanup(rxq);
	rte_free(rxq);
	priv_unlock(priv);
}

static int
priv_dev_interrupt_handler_install(struct priv *, struct rte_eth_dev *);

static int
priv_dev_removal_interrupt_handler_install(struct priv *, struct rte_eth_dev *);

static int
priv_dev_link_interrupt_handler_install(struct priv *, struct rte_eth_dev *);

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by attaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_start(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	if (priv->started) {
		priv_unlock(priv);
		return 0;
	}
	DEBUG("%p: attaching configured flows to all RX queues", (void *)dev);
	priv->started = 1;
	ret = priv_mac_addr_add(priv);
	if (ret)
		goto err;
	ret = priv_dev_link_interrupt_handler_install(priv, dev);
	if (ret) {
		ERROR("%p: LSC handler install failed",
		     (void *)dev);
		goto err;
	}
	ret = priv_dev_removal_interrupt_handler_install(priv, dev);
	if (ret) {
		ERROR("%p: RMV handler install failed",
		     (void *)dev);
		goto err;
	}
	ret = priv_rx_intr_vec_enable(priv);
	if (ret) {
		ERROR("%p: Rx interrupt vector creation failed",
		      (void *)dev);
		goto err;
	}
	ret = mlx4_priv_flow_start(priv);
	if (ret) {
		ERROR("%p: flow start failed: %s",
		      (void *)dev, strerror(ret));
		goto err;
	}
	priv_unlock(priv);
	return 0;
err:
	/* Rollback. */
	priv_mac_addr_del(priv);
	priv->started = 0;
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_stop(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	if (!priv->started) {
		priv_unlock(priv);
		return;
	}
	DEBUG("%p: detaching flows from all RX queues", (void *)dev);
	priv->started = 0;
	mlx4_priv_flow_stop(priv);
	priv_mac_addr_del(priv);
	priv_unlock(priv);
}

/**
 * Dummy DPDK callback for TX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static uint16_t
removed_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

/**
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
static uint16_t
removed_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

static int
priv_dev_interrupt_handler_uninstall(struct priv *, struct rte_eth_dev *);

static int
priv_dev_removal_interrupt_handler_uninstall(struct priv *,
					     struct rte_eth_dev *);

static int
priv_dev_link_interrupt_handler_uninstall(struct priv *, struct rte_eth_dev *);

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_close(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	void *tmp;
	unsigned int i;

	if (priv == NULL)
		return;
	priv_lock(priv);
	DEBUG("%p: closing device \"%s\"",
	      (void *)dev,
	      ((priv->ctx != NULL) ? priv->ctx->device->name : ""));
	priv_mac_addr_del(priv);
	/* Prevent crashes when queues are still in use. This is unfortunately
	 * still required for DPDK 1.3 because some programs (such as testpmd)
	 * never release them before closing the device. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	if (priv->rxqs != NULL) {
		/* XXX race condition if mlx4_rx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i) {
			tmp = (*priv->rxqs)[i];
			if (tmp == NULL)
				continue;
			(*priv->rxqs)[i] = NULL;
			rxq_cleanup(tmp);
			rte_free(tmp);
		}
		priv->rxqs_n = 0;
		priv->rxqs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx4_tx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i) {
			tmp = (*priv->txqs)[i];
			if (tmp == NULL)
				continue;
			(*priv->txqs)[i] = NULL;
			txq_cleanup(tmp);
			rte_free(tmp);
		}
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	if (priv->pd != NULL) {
		assert(priv->ctx != NULL);
		claim_zero(ibv_dealloc_pd(priv->pd));
		claim_zero(ibv_close_device(priv->ctx));
	} else
		assert(priv->ctx == NULL);
	priv_dev_removal_interrupt_handler_uninstall(priv, dev);
	priv_dev_link_interrupt_handler_uninstall(priv, dev);
	priv_rx_intr_vec_disable(priv);
	priv_unlock(priv);
	memset(priv, 0, sizeof(*priv));
}

/**
 * Change the link state (UP / DOWN).
 *
 * @param priv
 *   Pointer to Ethernet device private data.
 * @param up
 *   Nonzero for link up, otherwise link down.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_set_link(struct priv *priv, int up)
{
	struct rte_eth_dev *dev = priv->dev;
	int err;
	unsigned int i;

	if (up) {
		err = priv_set_flags(priv, ~IFF_UP, IFF_UP);
		if (err)
			return err;
		for (i = 0; i < priv->rxqs_n; i++)
			if ((*priv->rxqs)[i]->sp)
				break;
		/* Check if an sp queue exists.
		 * Note: Some old frames might be received.
		 */
		if (i == priv->rxqs_n)
			dev->rx_pkt_burst = mlx4_rx_burst;
		else
			dev->rx_pkt_burst = mlx4_rx_burst_sp;
		dev->tx_pkt_burst = mlx4_tx_burst;
	} else {
		err = priv_set_flags(priv, ~IFF_UP, ~IFF_UP);
		if (err)
			return err;
		dev->rx_pkt_burst = removed_rx_burst;
		dev->tx_pkt_burst = removed_tx_burst;
	}
	return 0;
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
mlx4_set_link_down(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int err;

	priv_lock(priv);
	err = priv_set_link(priv, 0);
	priv_unlock(priv);
	return err;
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
mlx4_set_link_up(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int err;

	priv_lock(priv);
	err = priv_set_link(priv, 1);
	priv_unlock(priv);
	return err;
}
/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
static void
mlx4_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int max;
	char ifname[IF_NAMESIZE];

	info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (priv == NULL)
		return;
	priv_lock(priv);
	/* FIXME: we should ask the device for these values. */
	info->min_rx_bufsize = 32;
	info->max_rx_pktlen = 65536;
	/*
	 * Since we need one CQ per QP, the limit is the minimum number
	 * between the two values.
	 */
	max = ((priv->device_attr.max_cq > priv->device_attr.max_qp) ?
	       priv->device_attr.max_qp : priv->device_attr.max_cq);
	/* If max >= 65535 then max = 0, max_rx_queues is uint16_t. */
	if (max >= 65535)
		max = 65535;
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	/* Last array entry is reserved for broadcast. */
	info->max_mac_addrs = 1;
	info->rx_offload_capa = 0;
	info->tx_offload_capa = 0;
	if (priv_get_ifname(priv, &ifname) == 0)
		info->if_index = if_nametoindex(ifname);
	info->speed_capa =
			ETH_LINK_SPEED_1G |
			ETH_LINK_SPEED_10G |
			ETH_LINK_SPEED_20G |
			ETH_LINK_SPEED_40G |
			ETH_LINK_SPEED_56G;
	priv_unlock(priv);
}

static const uint32_t *
mlx4_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == mlx4_rx_burst ||
	    dev->rx_pkt_burst == mlx4_rx_burst_sp)
		return ptypes;
	return NULL;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 */
static void
mlx4_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_eth_stats tmp = {0};
	unsigned int i;
	unsigned int idx;

	if (priv == NULL)
		return;
	priv_lock(priv);
	/* Add software counters. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		idx = rxq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			tmp.q_ipackets[idx] += rxq->stats.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes;
			tmp.q_errors[idx] += (rxq->stats.idropped +
					      rxq->stats.rx_nombuf);
		}
		tmp.ipackets += rxq->stats.ipackets;
		tmp.ibytes += rxq->stats.ibytes;
		tmp.ierrors += rxq->stats.idropped;
		tmp.rx_nombuf += rxq->stats.rx_nombuf;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct txq *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
			tmp.q_errors[idx] += txq->stats.odropped;
		}
		tmp.opackets += txq->stats.opackets;
		tmp.obytes += txq->stats.obytes;
		tmp.oerrors += txq->stats.odropped;
	}
	*stats = tmp;
	priv_unlock(priv);
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_stats_reset(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int idx;

	if (priv == NULL)
		return;
	priv_lock(priv);
	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		idx = (*priv->rxqs)[i]->stats.idx;
		(*priv->rxqs)[i]->stats =
			(struct mlx4_rxq_stats){ .idx = idx };
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		if ((*priv->txqs)[i] == NULL)
			continue;
		idx = (*priv->txqs)[i]->stats.idx;
		(*priv->txqs)[i]->stats =
			(struct mlx4_txq_stats){ .idx = idx };
	}
	priv_unlock(priv);
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 */
static int
mlx4_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	const struct priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata = {
		.cmd = ETHTOOL_GSET
	};
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	int link_speed = 0;

	/* priv_lock() is not taken to allow concurrent calls. */

	if (priv == NULL)
		return -EINVAL;
	(void)wait_to_complete;
	if (priv_ifreq(priv, SIOCGIFFLAGS, &ifr)) {
		WARN("ioctl(SIOCGIFFLAGS) failed: %s", strerror(errno));
		return -1;
	}
	memset(&dev_link, 0, sizeof(dev_link));
	dev_link.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING));
	ifr.ifr_data = (void *)&edata;
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GSET) failed: %s",
		     strerror(errno));
		return -1;
	}
	link_speed = ethtool_cmd_speed(&edata);
	if (link_speed == -1)
		dev_link.link_speed = 0;
	else
		dev_link.link_speed = link_speed;
	dev_link.link_duplex = ((edata.duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			ETH_LINK_SPEED_FIXED);
	if (memcmp(&dev_link, &dev->data->dev_link, sizeof(dev_link))) {
		/* Link status changed. */
		dev->data->dev_link = dev_link;
		return 0;
	}
	/* Link status is still the same. */
	return -1;
}

/**
 * DPDK callback to change the MTU.
 *
 * Setting the MTU affects hardware MRU (packets larger than the MTU cannot be
 * received). Use this as a hint to enable/disable scattered packets support
 * and improve performance when not needed.
 * Since failure is not an option, reconfiguring queues on the fly is not
 * recommended.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param in_mtu
 *   New MTU.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct priv *priv = dev->data->dev_private;
	int ret = 0;
	unsigned int i;
	uint16_t (*rx_func)(void *, struct rte_mbuf **, uint16_t) =
		mlx4_rx_burst;

	priv_lock(priv);
	/* Set kernel interface MTU first. */
	if (priv_set_mtu(priv, mtu)) {
		ret = errno;
		WARN("cannot set port %u MTU to %u: %s", priv->port, mtu,
		     strerror(ret));
		goto out;
	} else
		DEBUG("adapter port %u MTU set to %u", priv->port, mtu);
	priv->mtu = mtu;
	/* Remove MAC flow. */
	priv_mac_addr_del(priv);
	/* Temporarily replace RX handler with a fake one, assuming it has not
	 * been copied elsewhere. */
	dev->rx_pkt_burst = removed_rx_burst;
	/* Make sure everyone has left mlx4_rx_burst() and uses
	 * removed_rx_burst() instead. */
	rte_wmb();
	usleep(1000);
	/* Reconfigure each RX queue. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		unsigned int max_frame_len;

		if (rxq == NULL)
			continue;
		/* Calculate new maximum frame length according to MTU. */
		max_frame_len = (priv->mtu + ETHER_HDR_LEN +
				 (ETHER_MAX_VLAN_FRAME_LEN - ETHER_MAX_LEN));
		/* Provide new values to rxq_setup(). */
		dev->data->dev_conf.rxmode.jumbo_frame =
			(max_frame_len > ETHER_MAX_LEN);
		dev->data->dev_conf.rxmode.max_rx_pkt_len = max_frame_len;
		ret = rxq_rehash(dev, rxq);
		if (ret) {
			/* Force SP RX if that queue requires it and abort. */
			if (rxq->sp)
				rx_func = mlx4_rx_burst_sp;
			break;
		}
		/* Scattered burst function takes priority. */
		if (rxq->sp)
			rx_func = mlx4_rx_burst_sp;
	}
	/* Burst functions can now be called again. */
	rte_wmb();
	dev->rx_pkt_burst = rx_func;
	/* Restore MAC flow. */
	ret = priv_mac_addr_add(priv);
out:
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
}

/**
 * DPDK callback to get flow control status.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] fc_conf
 *   Flow control output buffer.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_get_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct priv *priv = dev->data->dev_private;
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_GPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	priv_lock(priv);
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM)"
		     " failed: %s",
		     strerror(ret));
		goto out;
	}

	fc_conf->autoneg = ethpause.autoneg;
	if (ethpause.rx_pause && ethpause.tx_pause)
		fc_conf->mode = RTE_FC_FULL;
	else if (ethpause.rx_pause)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (ethpause.tx_pause)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;
	ret = 0;

out:
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
}

/**
 * DPDK callback to modify flow control parameters.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] fc_conf
 *   Flow control parameters.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_set_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct priv *priv = dev->data->dev_private;
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_SPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	ethpause.autoneg = fc_conf->autoneg;
	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_RX_PAUSE))
		ethpause.rx_pause = 1;
	else
		ethpause.rx_pause = 0;

	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_TX_PAUSE))
		ethpause.tx_pause = 1;
	else
		ethpause.tx_pause = 0;

	priv_lock(priv);
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_SPAUSEPARAM)"
		     " failed: %s",
		     strerror(ret));
		goto out;
	}
	ret = 0;

out:
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
}

const struct rte_flow_ops mlx4_flow_ops = {
	.validate = mlx4_flow_validate,
	.create = mlx4_flow_create,
	.destroy = mlx4_flow_destroy,
	.flush = mlx4_flow_flush,
	.query = NULL,
	.isolate = mlx4_flow_isolate,
};

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = EINVAL;

	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &mlx4_flow_ops;
		return 0;
	default:
		ERROR("%p: filter type (%d) not supported",
		      (void *)dev, filter_type);
		break;
	}
	return -ret;
}

static const struct eth_dev_ops mlx4_dev_ops = {
	.dev_configure = mlx4_dev_configure,
	.dev_start = mlx4_dev_start,
	.dev_stop = mlx4_dev_stop,
	.dev_set_link_down = mlx4_set_link_down,
	.dev_set_link_up = mlx4_set_link_up,
	.dev_close = mlx4_dev_close,
	.link_update = mlx4_link_update,
	.stats_get = mlx4_stats_get,
	.stats_reset = mlx4_stats_reset,
	.dev_infos_get = mlx4_dev_infos_get,
	.dev_supported_ptypes_get = mlx4_dev_supported_ptypes_get,
	.rx_queue_setup = mlx4_rx_queue_setup,
	.tx_queue_setup = mlx4_tx_queue_setup,
	.rx_queue_release = mlx4_rx_queue_release,
	.tx_queue_release = mlx4_tx_queue_release,
	.flow_ctrl_get = mlx4_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx4_dev_set_flow_ctrl,
	.mtu_set = mlx4_dev_set_mtu,
	.filter_ctrl = mlx4_dev_filter_ctrl,
	.rx_queue_intr_enable = mlx4_rx_intr_enable,
	.rx_queue_intr_disable = mlx4_rx_intr_disable,
};

/**
 * Get PCI information from struct ibv_device.
 *
 * @param device
 *   Pointer to Ethernet device structure.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
mlx4_ibv_device_to_pci_addr(const struct ibv_device *device,
			    struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", device->ibdev_path);

	file = fopen(path, "rb");
	if (file == NULL)
		return -1;
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   struct priv for the requested device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;

	if (priv_ifreq(priv, SIOCGIFHWADDR, &request))
		return -1;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
	return 0;
}

/**
 * Retrieve integer value from environment variable.
 *
 * @param[in] name
 *   Environment variable name.
 *
 * @return
 *   Integer value, 0 if the variable is not set.
 */
static int
mlx4_getenv_int(const char *name)
{
	const char *val = getenv(name);

	if (val == NULL)
		return 0;
	return atoi(val);
}

static void
mlx4_dev_link_status_handler(void *);
static void
mlx4_dev_interrupt_handler(void *);

/**
 * Link/device status handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @param events
 *   Pointer to event flags holder.
 *
 * @return
 *   Number of events
 */
static int
priv_dev_status_handler(struct priv *priv, struct rte_eth_dev *dev,
			uint32_t *events)
{
	struct ibv_async_event event;
	int port_change = 0;
	struct rte_eth_link *link = &dev->data->dev_link;
	int ret = 0;

	*events = 0;
	/* Read all message and acknowledge them. */
	for (;;) {
		if (ibv_get_async_event(priv->ctx, &event))
			break;
		if ((event.event_type == IBV_EVENT_PORT_ACTIVE ||
		     event.event_type == IBV_EVENT_PORT_ERR) &&
		    (priv->intr_conf.lsc == 1)) {
			port_change = 1;
			ret++;
		} else if (event.event_type == IBV_EVENT_DEVICE_FATAL &&
			   priv->intr_conf.rmv == 1) {
			*events |= (1 << RTE_ETH_EVENT_INTR_RMV);
			ret++;
		} else
			DEBUG("event type %d on port %d not handled",
			      event.event_type, event.element.port_num);
		ibv_ack_async_event(&event);
	}
	if (!port_change)
		return ret;
	mlx4_link_update(dev, 0);
	if (((link->link_speed == 0) && link->link_status) ||
	    ((link->link_speed != 0) && !link->link_status)) {
		if (!priv->pending_alarm) {
			/* Inconsistent status, check again later. */
			priv->pending_alarm = 1;
			rte_eal_alarm_set(MLX4_ALARM_TIMEOUT_US,
					  mlx4_dev_link_status_handler,
					  dev);
		}
	} else {
		*events |= (1 << RTE_ETH_EVENT_INTR_LSC);
	}
	return ret;
}

/**
 * Handle delayed link status event.
 *
 * @param arg
 *   Registered argument.
 */
static void
mlx4_dev_link_status_handler(void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct priv *priv = dev->data->dev_private;
	uint32_t events;
	int ret;

	priv_lock(priv);
	assert(priv->pending_alarm == 1);
	priv->pending_alarm = 0;
	ret = priv_dev_status_handler(priv, dev, &events);
	priv_unlock(priv);
	if (ret > 0 && events & (1 << RTE_ETH_EVENT_INTR_LSC))
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL,
					      NULL);
}

/**
 * Handle interrupts from the NIC.
 *
 * @param[in] intr_handle
 *   Interrupt handler.
 * @param cb_arg
 *   Callback argument.
 */
static void
mlx4_dev_interrupt_handler(void *cb_arg)
{
	struct rte_eth_dev *dev = cb_arg;
	struct priv *priv = dev->data->dev_private;
	int ret;
	uint32_t ev;
	int i;

	priv_lock(priv);
	ret = priv_dev_status_handler(priv, dev, &ev);
	priv_unlock(priv);
	if (ret > 0) {
		for (i = RTE_ETH_EVENT_UNKNOWN;
		     i < RTE_ETH_EVENT_MAX;
		     i++) {
			if (ev & (1 << i)) {
				ev &= ~(1 << i);
				_rte_eth_dev_callback_process(dev, i, NULL,
							      NULL);
				ret--;
			}
		}
		if (ret)
			WARN("%d event%s not processed", ret,
			     (ret > 1 ? "s were" : " was"));
	}
}

/**
 * Uninstall interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
priv_dev_interrupt_handler_uninstall(struct priv *priv, struct rte_eth_dev *dev)
{
	int ret;

	if (priv->intr_conf.lsc ||
	    priv->intr_conf.rmv)
		return 0;
	ret = rte_intr_callback_unregister(&priv->intr_handle,
					   mlx4_dev_interrupt_handler,
					   dev);
	if (ret < 0) {
		ERROR("rte_intr_callback_unregister failed with %d"
		      "%s%s%s", ret,
		      (errno ? " (errno: " : ""),
		      (errno ? strerror(errno) : ""),
		      (errno ? ")" : ""));
	}
	priv->intr_handle.fd = 0;
	priv->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
	return ret;
}

/**
 * Install interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
priv_dev_interrupt_handler_install(struct priv *priv,
				   struct rte_eth_dev *dev)
{
	int flags;
	int rc;

	/* Check whether the interrupt handler has already been installed
	 * for either type of interrupt
	 */
	if (priv->intr_conf.lsc &&
	    priv->intr_conf.rmv &&
	    priv->intr_handle.fd)
		return 0;
	assert(priv->ctx->async_fd > 0);
	flags = fcntl(priv->ctx->async_fd, F_GETFL);
	rc = fcntl(priv->ctx->async_fd, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		INFO("failed to change file descriptor async event queue");
		dev->data->dev_conf.intr_conf.lsc = 0;
		dev->data->dev_conf.intr_conf.rmv = 0;
		return -errno;
	} else {
		priv->intr_handle.fd = priv->ctx->async_fd;
		priv->intr_handle.type = RTE_INTR_HANDLE_EXT;
		rc = rte_intr_callback_register(&priv->intr_handle,
						 mlx4_dev_interrupt_handler,
						 dev);
		if (rc) {
			ERROR("rte_intr_callback_register failed "
			      " (errno: %s)", strerror(errno));
			return rc;
		}
	}
	return 0;
}

/**
 * Uninstall interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative value on error.
 */
static int
priv_dev_removal_interrupt_handler_uninstall(struct priv *priv,
					    struct rte_eth_dev *dev)
{
	if (dev->data->dev_conf.intr_conf.rmv) {
		priv->intr_conf.rmv = 0;
		return priv_dev_interrupt_handler_uninstall(priv, dev);
	}
	return 0;
}

/**
 * Uninstall interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative value on error,
 */
static int
priv_dev_link_interrupt_handler_uninstall(struct priv *priv,
					  struct rte_eth_dev *dev)
{
	int ret = 0;

	if (dev->data->dev_conf.intr_conf.lsc) {
		priv->intr_conf.lsc = 0;
		ret = priv_dev_interrupt_handler_uninstall(priv, dev);
		if (ret)
			return ret;
	}
	if (priv->pending_alarm)
		if (rte_eal_alarm_cancel(mlx4_dev_link_status_handler,
					 dev)) {
			ERROR("rte_eal_alarm_cancel failed "
			      " (errno: %s)", strerror(rte_errno));
			return -rte_errno;
		}
	priv->pending_alarm = 0;
	return 0;
}

/**
 * Install link interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative value on error.
 */
static int
priv_dev_link_interrupt_handler_install(struct priv *priv,
					struct rte_eth_dev *dev)
{
	int ret;

	if (dev->data->dev_conf.intr_conf.lsc) {
		ret = priv_dev_interrupt_handler_install(priv, dev);
		if (ret)
			return ret;
		priv->intr_conf.lsc = 1;
	}
	return 0;
}

/**
 * Install removal interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 * @return
 *   0 on success, negative value on error.
 */
static int
priv_dev_removal_interrupt_handler_install(struct priv *priv,
					   struct rte_eth_dev *dev)
{
	int ret;

	if (dev->data->dev_conf.intr_conf.rmv) {
		ret = priv_dev_interrupt_handler_install(priv, dev);
		if (ret)
			return ret;
		priv->intr_conf.rmv = 1;
	}
	return 0;
}

/**
 * Allocate queue vector and fill epoll fd list for Rx interrupts.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative on failure.
 */
static int
priv_rx_intr_vec_enable(struct priv *priv)
{
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	unsigned int count = 0;
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	if (!priv->dev->data->dev_conf.intr_conf.rxq)
		return 0;
	priv_rx_intr_vec_disable(priv);
	intr_handle->intr_vec = malloc(sizeof(intr_handle->intr_vec[rxqs_n]));
	if (intr_handle->intr_vec == NULL) {
		ERROR("failed to allocate memory for interrupt vector,"
		      " Rx interrupts will not be supported");
		return -ENOMEM;
	}
	intr_handle->type = RTE_INTR_HANDLE_EXT;
	for (i = 0; i != n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		int fd;
		int flags;
		int rc;

		/* Skip queues that cannot request interrupts. */
		if (!rxq || !rxq->channel) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		if (count >= RTE_MAX_RXTX_INTR_VEC_ID) {
			ERROR("too many Rx queues for interrupt vector size"
			      " (%d), Rx interrupts cannot be enabled",
			      RTE_MAX_RXTX_INTR_VEC_ID);
			priv_rx_intr_vec_disable(priv);
			return -1;
		}
		fd = rxq->channel->fd;
		flags = fcntl(fd, F_GETFL);
		rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		if (rc < 0) {
			ERROR("failed to make Rx interrupt file descriptor"
			      " %d non-blocking for queue index %d", fd, i);
			priv_rx_intr_vec_disable(priv);
			return rc;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = fd;
		count++;
	}
	if (!count)
		priv_rx_intr_vec_disable(priv);
	else
		intr_handle->nb_efd = count;
	return 0;
}

/**
 * Clean up Rx interrupts handler.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_rx_intr_vec_disable(struct priv *priv)
{
	struct rte_intr_handle *intr_handle = priv->dev->intr_handle;

	rte_intr_free_epoll_fd(intr_handle);
	free(intr_handle->intr_vec);
	intr_handle->nb_efd = 0;
	intr_handle->intr_vec = NULL;
}

/**
 * DPDK callback for Rx queue interrupt enable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Rx queue index.
 *
 * @return
 *   0 on success, negative on failure.
 */
static int
mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq *rxq = (*priv->rxqs)[idx];
	int ret;

	if (!rxq || !rxq->channel)
		ret = EINVAL;
	else
		ret = ibv_req_notify_cq(rxq->cq, 0);
	if (ret)
		WARN("unable to arm interrupt on rx queue %d", idx);
	return -ret;
}

/**
 * DPDK callback for Rx queue interrupt disable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Rx queue index.
 *
 * @return
 *   0 on success, negative on failure.
 */
static int
mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq *rxq = (*priv->rxqs)[idx];
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret;

	if (!rxq || !rxq->channel) {
		ret = EINVAL;
	} else {
		ret = ibv_get_cq_event(rxq->cq->channel, &ev_cq, &ev_ctx);
		if (ret || ev_cq != rxq->cq)
			ret = EINVAL;
	}
	if (ret)
		WARN("unable to disable interrupt on rx queue %d",
		     idx);
	else
		ibv_ack_cq_events(rxq->cq, 1);
	return -ret;
}

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param[in, out] conf
 *   Shared configuration data.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_arg_parse(const char *key, const char *val, struct mlx4_conf *conf)
{
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		WARN("%s: \"%s\" is not a valid integer", key, val);
		return -errno;
	}
	if (strcmp(MLX4_PMD_PORT_KVARG, key) == 0) {
		uint32_t ports = rte_log2_u32(conf->ports.present);

		if (tmp >= ports) {
			ERROR("port index %lu outside range [0,%" PRIu32 ")",
			      tmp, ports);
			return -EINVAL;
		}
		if (!(conf->ports.present & (1 << tmp))) {
			ERROR("invalid port index %lu", tmp);
			return -EINVAL;
		}
		conf->ports.enabled |= 1 << tmp;
	} else {
		WARN("%s: unknown parameter", key);
		return -EINVAL;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_args(struct rte_devargs *devargs, struct mlx4_conf *conf)
{
	struct rte_kvargs *kvlist;
	unsigned int arg_count;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, pmd_mlx4_init_params);
	if (kvlist == NULL) {
		ERROR("failed to parse kvargs");
		return -EINVAL;
	}
	/* Process parameters. */
	for (i = 0; pmd_mlx4_init_params[i]; ++i) {
		arg_count = rte_kvargs_count(kvlist, MLX4_PMD_PORT_KVARG);
		while (arg_count-- > 0) {
			ret = rte_kvargs_process(kvlist,
						 MLX4_PMD_PORT_KVARG,
						 (int (*)(const char *,
							  const char *,
							  void *))
						 mlx4_arg_parse,
						 conf);
			if (ret != 0)
				goto free_kvlist;
		}
	}
free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static struct rte_pci_driver mlx4_driver;

/**
 * DPDK callback to register a PCI device.
 *
 * This function creates an Ethernet device for each port of a given
 * PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx4_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
mlx4_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct ibv_device **list;
	struct ibv_device *ibv_dev;
	int err = 0;
	struct ibv_context *attr_ctx = NULL;
	struct ibv_device_attr device_attr;
	struct mlx4_conf conf = {
		.ports.present = 0,
	};
	unsigned int vf;
	int i;

	(void)pci_drv;
	assert(pci_drv == &mlx4_driver);

	list = ibv_get_device_list(&i);
	if (list == NULL) {
		assert(errno);
		if (errno == ENOSYS)
			ERROR("cannot list devices, is ib_uverbs loaded?");
		return -errno;
	}
	assert(i >= 0);
	/*
	 * For each listed device, check related sysfs entry against
	 * the provided PCI ID.
	 */
	while (i != 0) {
		struct rte_pci_addr pci_addr;

		--i;
		DEBUG("checking device \"%s\"", list[i]->name);
		if (mlx4_ibv_device_to_pci_addr(list[i], &pci_addr))
			continue;
		if ((pci_dev->addr.domain != pci_addr.domain) ||
		    (pci_dev->addr.bus != pci_addr.bus) ||
		    (pci_dev->addr.devid != pci_addr.devid) ||
		    (pci_dev->addr.function != pci_addr.function))
			continue;
		vf = (pci_dev->id.device_id ==
		      PCI_DEVICE_ID_MELLANOX_CONNECTX3VF);
		INFO("PCI information matches, using device \"%s\" (VF: %s)",
		     list[i]->name, (vf ? "true" : "false"));
		attr_ctx = ibv_open_device(list[i]);
		err = errno;
		break;
	}
	if (attr_ctx == NULL) {
		ibv_free_device_list(list);
		switch (err) {
		case 0:
			ERROR("cannot access device, is mlx4_ib loaded?");
			return -ENODEV;
		case EINVAL:
			ERROR("cannot use device, are drivers up to date?");
			return -EINVAL;
		}
		assert(err > 0);
		return -err;
	}
	ibv_dev = list[i];

	DEBUG("device opened");
	if (ibv_query_device(attr_ctx, &device_attr)) {
		err = ENODEV;
		goto error;
	}
	INFO("%u port(s) detected", device_attr.phys_port_cnt);

	conf.ports.present |= (UINT64_C(1) << device_attr.phys_port_cnt) - 1;
	if (mlx4_args(pci_dev->device.devargs, &conf)) {
		ERROR("failed to process device arguments");
		err = EINVAL;
		goto error;
	}
	/* Use all ports when none are defined */
	if (!conf.ports.enabled)
		conf.ports.enabled = conf.ports.present;
	for (i = 0; i < device_attr.phys_port_cnt; i++) {
		uint32_t port = i + 1; /* ports are indexed from one */
		struct ibv_context *ctx = NULL;
		struct ibv_port_attr port_attr;
		struct ibv_pd *pd = NULL;
		struct priv *priv = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		struct ibv_exp_device_attr exp_device_attr;
		struct ether_addr mac;

		/* If port is not enabled, skip. */
		if (!(conf.ports.enabled & (1 << i)))
			continue;
		exp_device_attr.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS;

		DEBUG("using port %u", port);

		ctx = ibv_open_device(ibv_dev);
		if (ctx == NULL) {
			err = ENODEV;
			goto port_error;
		}

		/* Check port status. */
		err = ibv_query_port(ctx, port, &port_attr);
		if (err) {
			ERROR("port query failed: %s", strerror(err));
			err = ENODEV;
			goto port_error;
		}

		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			ERROR("port %d is not configured in Ethernet mode",
			      port);
			err = EINVAL;
			goto port_error;
		}

		if (port_attr.state != IBV_PORT_ACTIVE)
			DEBUG("port %d is not active: \"%s\" (%d)",
			      port, ibv_port_state_str(port_attr.state),
			      port_attr.state);

		/* Allocate protection domain. */
		pd = ibv_alloc_pd(ctx);
		if (pd == NULL) {
			ERROR("PD allocation failure");
			err = ENOMEM;
			goto port_error;
		}

		/* from rte_ethdev.c */
		priv = rte_zmalloc("ethdev private structure",
				   sizeof(*priv),
				   RTE_CACHE_LINE_SIZE);
		if (priv == NULL) {
			ERROR("priv allocation failure");
			err = ENOMEM;
			goto port_error;
		}

		priv->ctx = ctx;
		priv->device_attr = device_attr;
		priv->port = port;
		priv->pd = pd;
		priv->mtu = ETHER_MTU;
		if (ibv_exp_query_device(ctx, &exp_device_attr)) {
			ERROR("ibv_exp_query_device() failed");
			err = ENODEV;
			goto port_error;
		}

		priv->inl_recv_size = mlx4_getenv_int("MLX4_INLINE_RECV_SIZE");

		if (priv->inl_recv_size) {
			exp_device_attr.comp_mask =
				IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ;
			if (ibv_exp_query_device(ctx, &exp_device_attr)) {
				INFO("Couldn't query device for inline-receive"
				     " capabilities.");
				priv->inl_recv_size = 0;
			} else {
				if ((unsigned)exp_device_attr.inline_recv_sz <
				    priv->inl_recv_size) {
					INFO("Max inline-receive (%d) <"
					     " requested inline-receive (%u)",
					     exp_device_attr.inline_recv_sz,
					     priv->inl_recv_size);
					priv->inl_recv_size =
						exp_device_attr.inline_recv_sz;
				}
			}
			INFO("Set inline receive size to %u",
			     priv->inl_recv_size);
		}

		priv->vf = vf;
		/* Configure the first MAC address by default. */
		if (priv_get_mac(priv, &mac.addr_bytes)) {
			ERROR("cannot get MAC address, is mlx4_en loaded?"
			      " (errno: %s)", strerror(errno));
			err = ENODEV;
			goto port_error;
		}
		INFO("port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		     priv->port,
		     mac.addr_bytes[0], mac.addr_bytes[1],
		     mac.addr_bytes[2], mac.addr_bytes[3],
		     mac.addr_bytes[4], mac.addr_bytes[5]);
		/* Register MAC address. */
		priv->mac = mac;
		if (priv_mac_addr_add(priv))
			goto port_error;
#ifndef NDEBUG
		{
			char ifname[IF_NAMESIZE];

			if (priv_get_ifname(priv, &ifname) == 0)
				DEBUG("port %u ifname is \"%s\"",
				      priv->port, ifname);
			else
				DEBUG("port %u ifname is unknown", priv->port);
		}
#endif
		/* Get actual MTU if possible. */
		priv_get_mtu(priv, &priv->mtu);
		DEBUG("port %u MTU is %u", priv->port, priv->mtu);

		/* from rte_ethdev.c */
		{
			char name[RTE_ETH_NAME_MAX_LEN];

			snprintf(name, sizeof(name), "%s port %u",
				 ibv_get_device_name(ibv_dev), port);
			eth_dev = rte_eth_dev_allocate(name);
		}
		if (eth_dev == NULL) {
			ERROR("can not allocate rte ethdev");
			err = ENOMEM;
			goto port_error;
		}

		eth_dev->data->dev_private = priv;
		eth_dev->data->mac_addrs = &priv->mac;
		eth_dev->device = &pci_dev->device;

		rte_eth_copy_pci_info(eth_dev, pci_dev);

		eth_dev->device->driver = &mlx4_driver.driver;

		/*
		 * Copy and override interrupt handle to prevent it from
		 * being shared between all ethdev instances of a given PCI
		 * device. This is required to properly handle Rx interrupts
		 * on all ports.
		 */
		priv->intr_handle_dev = *eth_dev->intr_handle;
		eth_dev->intr_handle = &priv->intr_handle_dev;

		priv->dev = eth_dev;
		eth_dev->dev_ops = &mlx4_dev_ops;
		eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

		/* Bring Ethernet device up. */
		DEBUG("forcing Ethernet interface up");
		priv_set_flags(priv, ~IFF_UP, IFF_UP);
		/* Update link status once if waiting for LSC. */
		if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			mlx4_link_update(eth_dev, 0);
		continue;

port_error:
		rte_free(priv);
		if (pd)
			claim_zero(ibv_dealloc_pd(pd));
		if (ctx)
			claim_zero(ibv_close_device(ctx));
		if (eth_dev)
			rte_eth_dev_release_port(eth_dev);
		break;
	}
	if (i == device_attr.phys_port_cnt)
		return 0;

	/*
	 * XXX if something went wrong in the loop above, there is a resource
	 * leak (ctx, pd, priv, dpdk ethdev) but we can do nothing about it as
	 * long as the dpdk does not provide a way to deallocate a ethdev and a
	 * way to enumerate the registered ethdevs to free the previous ones.
	 */

error:
	if (attr_ctx)
		claim_zero(ibv_close_device(attr_ctx));
	if (list)
		ibv_free_device_list(list);
	assert(err >= 0);
	return -err;
}

static const struct rte_pci_id mlx4_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3VF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx4_driver = {
	.driver = {
		.name = MLX4_DRIVER_NAME
	},
	.id_table = mlx4_pci_id_map,
	.probe = mlx4_pci_probe,
	.drv_flags = RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_INTR_RMV,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx4_pmd_init);
static void
rte_mlx4_pmd_init(void)
{
	RTE_BUILD_BUG_ON(sizeof(wr_id_t) != sizeof(uint64_t));
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	ibv_fork_init();
	rte_pci_register(&mlx4_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx4, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx4, mlx4_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx4,
	"* ib_uverbs & mlx4_en & mlx4_core & mlx4_ib");
