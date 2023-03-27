/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>

#include <rte_hexdump.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "ccp_dev.h"
#include "ccp_pmd_private.h"

static TAILQ_HEAD(, ccp_device) ccp_list = TAILQ_HEAD_INITIALIZER(ccp_list);
static int ccp_dev_id;

int
ccp_dev_start(struct rte_cryptodev *dev)
{
	struct ccp_private *priv = dev->data->dev_private;

	priv->last_dev = TAILQ_FIRST(&ccp_list);
	return 0;
}

struct ccp_queue *
ccp_allot_queue(struct rte_cryptodev *cdev, int slot_req)
{
	int i, ret = 0;
	struct ccp_device *dev;
	struct ccp_private *priv = cdev->data->dev_private;

	dev = TAILQ_NEXT(priv->last_dev, next);
	if (unlikely(dev == NULL))
		dev = TAILQ_FIRST(&ccp_list);
	priv->last_dev = dev;
	if (dev->qidx >= dev->cmd_q_count)
		dev->qidx = 0;
	ret = rte_atomic64_read(&dev->cmd_q[dev->qidx].free_slots);
	if (ret >= slot_req)
		return &dev->cmd_q[dev->qidx];
	for (i = 0; i < dev->cmd_q_count; i++) {
		dev->qidx++;
		if (dev->qidx >= dev->cmd_q_count)
			dev->qidx = 0;
		ret = rte_atomic64_read(&dev->cmd_q[dev->qidx].free_slots);
		if (ret >= slot_req)
			return &dev->cmd_q[dev->qidx];
	}
	return NULL;
}

int
ccp_read_hwrng(uint32_t *value)
{
	struct ccp_device *dev;

	TAILQ_FOREACH(dev, &ccp_list, next) {
		void *vaddr = (void *)(dev->pci->mem_resource[2].addr);

		while (dev->hwrng_retries++ < CCP_MAX_TRNG_RETRIES) {
			*value = CCP_READ_REG(vaddr, TRNG_OUT_REG);
			if (*value) {
				dev->hwrng_retries = 0;
				return 0;
			}
		}
		dev->hwrng_retries = 0;
	}
	return -1;
}

static const struct rte_memzone *
ccp_queue_dma_zone_reserve(const char *queue_name,
			   uint32_t queue_size,
			   int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(queue_name);
	if (mz != 0) {
		if (((size_t)queue_size <= mz->len) &&
		    ((socket_id == SOCKET_ID_ANY) ||
		     (socket_id == mz->socket_id))) {
			CCP_LOG_INFO("re-use memzone already "
				     "allocated for %s", queue_name);
			return mz;
		}
		CCP_LOG_ERR("Incompatible memzone already "
			    "allocated %s, size %u, socket %d. "
			    "Requested size %u, socket %u",
			    queue_name, (uint32_t)mz->len,
			    mz->socket_id, queue_size, socket_id);
		return NULL;
	}

	CCP_LOG_INFO("Allocate memzone for %s, size %u on socket %u",
		     queue_name, queue_size, socket_id);

	return rte_memzone_reserve_aligned(queue_name, queue_size,
			socket_id, RTE_MEMZONE_IOVA_CONTIG, queue_size);
}

/* bitmap support apis */
static inline void
ccp_set_bit(unsigned long *bitmap, int n)
{
	__atomic_fetch_or(&bitmap[WORD_OFFSET(n)], (1UL << BIT_OFFSET(n)),
		__ATOMIC_SEQ_CST);
}

static inline void
ccp_clear_bit(unsigned long *bitmap, int n)
{
	__atomic_fetch_and(&bitmap[WORD_OFFSET(n)], ~(1UL << BIT_OFFSET(n)),
		__ATOMIC_SEQ_CST);
}

static inline uint32_t
ccp_get_bit(unsigned long *bitmap, int n)
{
	return ((bitmap[WORD_OFFSET(n)] & (1 << BIT_OFFSET(n))) != 0);
}


static inline uint32_t
ccp_ffz(unsigned long word)
{
	unsigned long first_zero;

	first_zero = __builtin_ffsl(~word);
	return first_zero ? (first_zero - 1) :
		BITS_PER_WORD;
}

static inline uint32_t
ccp_find_first_zero_bit(unsigned long *addr, uint32_t limit)
{
	uint32_t i;
	uint32_t nwords = 0;

	nwords = (limit - 1) / BITS_PER_WORD + 1;
	for (i = 0; i < nwords; i++) {
		if (addr[i] == 0UL)
			return i * BITS_PER_WORD;
		if (addr[i] < ~(0UL))
			break;
	}
	return (i == nwords) ? limit : i * BITS_PER_WORD + ccp_ffz(addr[i]);
}

static void
ccp_bitmap_set(unsigned long *map, unsigned int start, int len)
{
	unsigned long *p = map + WORD_OFFSET(start);
	const unsigned int size = start + len;
	int bits_to_set = BITS_PER_WORD - (start % BITS_PER_WORD);
	unsigned long mask_to_set = CCP_BITMAP_FIRST_WORD_MASK(start);

	while (len - bits_to_set >= 0) {
		*p |= mask_to_set;
		len -= bits_to_set;
		bits_to_set = BITS_PER_WORD;
		mask_to_set = ~0UL;
		p++;
	}
	if (len) {
		mask_to_set &= CCP_BITMAP_LAST_WORD_MASK(size);
		*p |= mask_to_set;
	}
}

static void
ccp_bitmap_clear(unsigned long *map, unsigned int start, int len)
{
	unsigned long *p = map + WORD_OFFSET(start);
	const unsigned int size = start + len;
	int bits_to_clear = BITS_PER_WORD - (start % BITS_PER_WORD);
	unsigned long mask_to_clear = CCP_BITMAP_FIRST_WORD_MASK(start);

	while (len - bits_to_clear >= 0) {
		*p &= ~mask_to_clear;
		len -= bits_to_clear;
		bits_to_clear = BITS_PER_WORD;
		mask_to_clear = ~0UL;
		p++;
	}
	if (len) {
		mask_to_clear &= CCP_BITMAP_LAST_WORD_MASK(size);
		*p &= ~mask_to_clear;
	}
}


static unsigned long
_ccp_find_next_bit(const unsigned long *addr,
		   unsigned long nbits,
		   unsigned long start,
		   unsigned long invert)
{
	unsigned long tmp;

	if (!nbits || start >= nbits)
		return nbits;

	tmp = addr[start / BITS_PER_WORD] ^ invert;

	/* Handle 1st word. */
	tmp &= CCP_BITMAP_FIRST_WORD_MASK(start);
	start = ccp_round_down(start, BITS_PER_WORD);

	while (!tmp) {
		start += BITS_PER_WORD;
		if (start >= nbits)
			return nbits;

		tmp = addr[start / BITS_PER_WORD] ^ invert;
	}

	return RTE_MIN(start + (ffs(tmp) - 1), nbits);
}

static unsigned long
ccp_find_next_bit(const unsigned long *addr,
		  unsigned long size,
		  unsigned long offset)
{
	return _ccp_find_next_bit(addr, size, offset, 0UL);
}

static unsigned long
ccp_find_next_zero_bit(const unsigned long *addr,
		       unsigned long size,
		       unsigned long offset)
{
	return _ccp_find_next_bit(addr, size, offset, ~0UL);
}

/**
 * bitmap_find_next_zero_area - find a contiguous aligned zero area
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 */
static unsigned long
ccp_bitmap_find_next_zero_area(unsigned long *map,
			       unsigned long size,
			       unsigned long start,
			       unsigned int nr)
{
	unsigned long index, end, i;

again:
	index = ccp_find_next_zero_bit(map, size, start);

	end = index + nr;
	if (end > size)
		return end;
	i = ccp_find_next_bit(map, end, index);
	if (i < end) {
		start = i + 1;
		goto again;
	}
	return index;
}

static uint32_t
ccp_lsb_alloc(struct ccp_queue *cmd_q, unsigned int count)
{
	struct ccp_device *ccp;
	int start;

	/* First look at the map for the queue */
	if (cmd_q->lsb >= 0) {
		start = (uint32_t)ccp_bitmap_find_next_zero_area(cmd_q->lsbmap,
								 LSB_SIZE, 0,
								 count);
		if (start < LSB_SIZE) {
			ccp_bitmap_set(cmd_q->lsbmap, start, count);
			return start + cmd_q->lsb * LSB_SIZE;
		}
	}

	/* try to get an entry from the shared blocks */
	ccp = cmd_q->dev;

	rte_spinlock_lock(&ccp->lsb_lock);

	start = (uint32_t)ccp_bitmap_find_next_zero_area(ccp->lsbmap,
						    MAX_LSB_CNT * LSB_SIZE,
						    0, count);
	if (start <= MAX_LSB_CNT * LSB_SIZE) {
		ccp_bitmap_set(ccp->lsbmap, start, count);
		rte_spinlock_unlock(&ccp->lsb_lock);
		return start * LSB_ITEM_SIZE;
	}
	CCP_LOG_ERR("NO LSBs available");

	rte_spinlock_unlock(&ccp->lsb_lock);

	return 0;
}

static void __rte_unused
ccp_lsb_free(struct ccp_queue *cmd_q,
	     unsigned int start,
	     unsigned int count)
{
	int lsbno = start / LSB_SIZE;

	if (!start)
		return;

	if (cmd_q->lsb == lsbno) {
		/* An entry from the private LSB */
		ccp_bitmap_clear(cmd_q->lsbmap, start % LSB_SIZE, count);
	} else {
		/* From the shared LSBs */
		struct ccp_device *ccp = cmd_q->dev;

		rte_spinlock_lock(&ccp->lsb_lock);
		ccp_bitmap_clear(ccp->lsbmap, start, count);
		rte_spinlock_unlock(&ccp->lsb_lock);
	}
}

static int
ccp_find_lsb_regions(struct ccp_queue *cmd_q, uint64_t status)
{
	int q_mask = 1 << cmd_q->id;
	int weight = 0;
	int j;

	/* Build a bit mask to know which LSBs
	 * this queue has access to.
	 * Don't bother with segment 0
	 * as it has special
	 * privileges.
	 */
	cmd_q->lsbmask = 0;
	status >>= LSB_REGION_WIDTH;
	for (j = 1; j < MAX_LSB_CNT; j++) {
		if (status & q_mask)
			ccp_set_bit(&cmd_q->lsbmask, j);

		status >>= LSB_REGION_WIDTH;
	}

	for (j = 0; j < MAX_LSB_CNT; j++)
		if (ccp_get_bit(&cmd_q->lsbmask, j))
			weight++;

	CCP_LOG_DBG("Queue %d can access %d LSB regions  of mask  %lu\n",
	       (int)cmd_q->id, weight, cmd_q->lsbmask);

	return weight ? 0 : -EINVAL;
}

static int
ccp_find_and_assign_lsb_to_q(struct ccp_device *ccp,
			     int lsb_cnt, int n_lsbs,
			     unsigned long *lsb_pub)
{
	unsigned long qlsb = 0;
	int bitno = 0;
	int qlsb_wgt = 0;
	int i, j;

	/* For each queue:
	 * If the count of potential LSBs available to a queue matches the
	 * ordinal given to us in lsb_cnt:
	 * Copy the mask of possible LSBs for this queue into "qlsb";
	 * For each bit in qlsb, see if the corresponding bit in the
	 * aggregation mask is set; if so, we have a match.
	 *     If we have a match, clear the bit in the aggregation to
	 *     mark it as no longer available.
	 *     If there is no match, clear the bit in qlsb and keep looking.
	 */
	for (i = 0; i < ccp->cmd_q_count; i++) {
		struct ccp_queue *cmd_q = &ccp->cmd_q[i];

		qlsb_wgt = 0;
		for (j = 0; j < MAX_LSB_CNT; j++)
			if (ccp_get_bit(&cmd_q->lsbmask, j))
				qlsb_wgt++;

		if (qlsb_wgt == lsb_cnt) {
			qlsb = cmd_q->lsbmask;

			bitno = ffs(qlsb) - 1;
			while (bitno < MAX_LSB_CNT) {
				if (ccp_get_bit(lsb_pub, bitno)) {
					/* We found an available LSB
					 * that this queue can access
					 */
					cmd_q->lsb = bitno;
					ccp_clear_bit(lsb_pub, bitno);
					break;
				}
				ccp_clear_bit(&qlsb, bitno);
				bitno = ffs(qlsb) - 1;
			}
			if (bitno >= MAX_LSB_CNT)
				return -EINVAL;
			n_lsbs--;
		}
	}
	return n_lsbs;
}

/* For each queue, from the most- to least-constrained:
 * find an LSB that can be assigned to the queue. If there are N queues that
 * can only use M LSBs, where N > M, fail; otherwise, every queue will get a
 * dedicated LSB. Remaining LSB regions become a shared resource.
 * If we have fewer LSBs than queues, all LSB regions become shared
 * resources.
 */
static int
ccp_assign_lsbs(struct ccp_device *ccp)
{
	unsigned long lsb_pub = 0, qlsb = 0;
	int n_lsbs = 0;
	int bitno;
	int i, lsb_cnt;
	int rc = 0;

	rte_spinlock_init(&ccp->lsb_lock);

	/* Create an aggregate bitmap to get a total count of available LSBs */
	for (i = 0; i < ccp->cmd_q_count; i++)
		lsb_pub |= ccp->cmd_q[i].lsbmask;

	for (i = 0; i < MAX_LSB_CNT; i++)
		if (ccp_get_bit(&lsb_pub, i))
			n_lsbs++;

	if (n_lsbs >= ccp->cmd_q_count) {
		/* We have enough LSBS to give every queue a private LSB.
		 * Brute force search to start with the queues that are more
		 * constrained in LSB choice. When an LSB is privately
		 * assigned, it is removed from the public mask.
		 * This is an ugly N squared algorithm with some optimization.
		 */
		for (lsb_cnt = 1; n_lsbs && (lsb_cnt <= MAX_LSB_CNT);
		     lsb_cnt++) {
			rc = ccp_find_and_assign_lsb_to_q(ccp, lsb_cnt, n_lsbs,
							  &lsb_pub);
			if (rc < 0)
				return -EINVAL;
			n_lsbs = rc;
		}
	}

	rc = 0;
	/* What's left of the LSBs, according to the public mask, now become
	 * shared. Any zero bits in the lsb_pub mask represent an LSB region
	 * that can't be used as a shared resource, so mark the LSB slots for
	 * them as "in use".
	 */
	qlsb = lsb_pub;
	bitno = ccp_find_first_zero_bit(&qlsb, MAX_LSB_CNT);
	while (bitno < MAX_LSB_CNT) {
		ccp_bitmap_set(ccp->lsbmap, bitno * LSB_SIZE, LSB_SIZE);
		ccp_set_bit(&qlsb, bitno);
		bitno = ccp_find_first_zero_bit(&qlsb, MAX_LSB_CNT);
	}

	return rc;
}

static int
ccp_add_device(struct ccp_device *dev)
{
	int i;
	uint32_t qmr, status_lo, status_hi, dma_addr_lo, dma_addr_hi;
	uint64_t status;
	struct ccp_queue *cmd_q;
	const struct rte_memzone *q_mz;
	void *vaddr;

	if (dev == NULL)
		return -1;

	dev->id = ccp_dev_id++;
	dev->qidx = 0;
	vaddr = (void *)(dev->pci->mem_resource[2].addr);

	if (dev->pci->id.device_id == AMD_PCI_CCP_5B) {
		CCP_WRITE_REG(vaddr, CMD_TRNG_CTL_OFFSET, 0x00012D57);
		CCP_WRITE_REG(vaddr, CMD_CONFIG_0_OFFSET, 0x00000003);
		for (i = 0; i < 12; i++) {
			CCP_WRITE_REG(vaddr, CMD_AES_MASK_OFFSET,
				      CCP_READ_REG(vaddr, TRNG_OUT_REG));
		}
		CCP_WRITE_REG(vaddr, CMD_QUEUE_MASK_OFFSET, 0x0000001F);
		CCP_WRITE_REG(vaddr, CMD_QUEUE_PRIO_OFFSET, 0x00005B6D);
		CCP_WRITE_REG(vaddr, CMD_CMD_TIMEOUT_OFFSET, 0x00000000);

		CCP_WRITE_REG(vaddr, LSB_PRIVATE_MASK_LO_OFFSET, 0x3FFFFFFF);
		CCP_WRITE_REG(vaddr, LSB_PRIVATE_MASK_HI_OFFSET, 0x000003FF);

		CCP_WRITE_REG(vaddr, CMD_CLK_GATE_CTL_OFFSET, 0x00108823);
	}
	CCP_WRITE_REG(vaddr, CMD_REQID_CONFIG_OFFSET, 0x0);

	/* Copy the private LSB mask to the public registers */
	status_lo = CCP_READ_REG(vaddr, LSB_PRIVATE_MASK_LO_OFFSET);
	status_hi = CCP_READ_REG(vaddr, LSB_PRIVATE_MASK_HI_OFFSET);
	CCP_WRITE_REG(vaddr, LSB_PUBLIC_MASK_LO_OFFSET, status_lo);
	CCP_WRITE_REG(vaddr, LSB_PUBLIC_MASK_HI_OFFSET, status_hi);
	status = ((uint64_t)status_hi<<30) | ((uint64_t)status_lo);

	dev->cmd_q_count = 0;
	/* Find available queues */
	qmr = CCP_READ_REG(vaddr, Q_MASK_REG);
	for (i = 0; i < MAX_HW_QUEUES; i++) {
		if (!(qmr & (1 << i)))
			continue;
		cmd_q = &dev->cmd_q[dev->cmd_q_count++];
		cmd_q->dev = dev;
		cmd_q->id = i;
		cmd_q->qidx = 0;
		cmd_q->qsize = Q_SIZE(Q_DESC_SIZE);

		cmd_q->reg_base = (uint8_t *)vaddr +
			CMD_Q_STATUS_INCR * (i + 1);

		/* CCP queue memory */
		snprintf(cmd_q->memz_name, sizeof(cmd_q->memz_name),
			 "%s_%d_%s_%d_%s",
			 "ccp_dev",
			 (int)dev->id, "queue",
			 (int)cmd_q->id, "mem");
		q_mz = ccp_queue_dma_zone_reserve(cmd_q->memz_name,
						  cmd_q->qsize, SOCKET_ID_ANY);
		cmd_q->qbase_addr = (void *)q_mz->addr;
		cmd_q->qbase_desc = (void *)q_mz->addr;
		cmd_q->qbase_phys_addr =  q_mz->iova;

		cmd_q->qcontrol = 0;
		/* init control reg to zero */
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
			      cmd_q->qcontrol);

		/* Disable the interrupts */
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_INT_ENABLE_BASE, 0x00);
		CCP_READ_REG(cmd_q->reg_base, CMD_Q_INT_STATUS_BASE);
		CCP_READ_REG(cmd_q->reg_base, CMD_Q_STATUS_BASE);

		/* Clear the interrupts */
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_INTERRUPT_STATUS_BASE,
			      ALL_INTERRUPTS);

		/* Configure size of each virtual queue accessible to host */
		cmd_q->qcontrol &= ~(CMD_Q_SIZE << CMD_Q_SHIFT);
		cmd_q->qcontrol |= QUEUE_SIZE_VAL << CMD_Q_SHIFT;

		dma_addr_lo = low32_value(cmd_q->qbase_phys_addr);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE,
			      (uint32_t)dma_addr_lo);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_HEAD_LO_BASE,
			      (uint32_t)dma_addr_lo);

		dma_addr_hi = high32_value(cmd_q->qbase_phys_addr);
		cmd_q->qcontrol |= (dma_addr_hi << 16);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
			      cmd_q->qcontrol);

		/* create LSB Mask map */
		if (ccp_find_lsb_regions(cmd_q, status))
			CCP_LOG_ERR("queue doesn't have lsb regions");
		cmd_q->lsb = -1;

		rte_atomic64_init(&cmd_q->free_slots);
		rte_atomic64_set(&cmd_q->free_slots, (COMMANDS_PER_QUEUE - 1));
		/* unused slot barrier b/w H&T */
	}

	if (ccp_assign_lsbs(dev))
		CCP_LOG_ERR("Unable to assign lsb region");

	/* pre-allocate LSB slots */
	for (i = 0; i < dev->cmd_q_count; i++) {
		dev->cmd_q[i].sb_key =
			ccp_lsb_alloc(&dev->cmd_q[i], 1);
		dev->cmd_q[i].sb_iv =
			ccp_lsb_alloc(&dev->cmd_q[i], 1);
		dev->cmd_q[i].sb_sha =
			ccp_lsb_alloc(&dev->cmd_q[i], 2);
		dev->cmd_q[i].sb_hmac =
			ccp_lsb_alloc(&dev->cmd_q[i], 2);
	}

	TAILQ_INSERT_TAIL(&ccp_list, dev, next);
	return 0;
}

static void
ccp_remove_device(struct ccp_device *dev)
{
	if (dev == NULL)
		return;

	TAILQ_REMOVE(&ccp_list, dev, next);
}

int
ccp_probe_device(struct rte_pci_device *pci_dev)
{
	struct ccp_device *ccp_dev;

	ccp_dev = rte_zmalloc("ccp_device", sizeof(*ccp_dev),
			      RTE_CACHE_LINE_SIZE);
	if (ccp_dev == NULL)
		goto fail;

	ccp_dev->pci = pci_dev;

	/* device is valid, add in list */
	if (ccp_add_device(ccp_dev)) {
		ccp_remove_device(ccp_dev);
		goto fail;
	}

	return 0;
fail:
	CCP_LOG_ERR("CCP Device probe failed");
	rte_free(ccp_dev);
	return -1;
}
