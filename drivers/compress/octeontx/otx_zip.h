/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _RTE_OCTEONTX_ZIP_VF_H_
#define _RTE_OCTEONTX_ZIP_VF_H_

#include <unistd.h>

#include <rte_bus_pci.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>

#include <zip_regs.h>

int octtx_zip_logtype_driver;

/* ZIP VF Control/Status registers (CSRs): */
/* VF_BAR0: */
#define ZIP_VQ_ENA              (0x10)
#define ZIP_VQ_SBUF_ADDR        (0x20)
#define ZIP_VF_PF_MBOXX(x)      (0x400 | (x)<<3)
#define ZIP_VQ_DOORBELL         (0x1000)

/**< Vendor ID */
#define PCI_VENDOR_ID_CAVIUM	0x177D
/**< PCI device id of ZIP VF */
#define PCI_DEVICE_ID_OCTEONTX_ZIPVF	0xA037

/* maxmum number of zip vf devices */
#define ZIP_MAX_VFS 8

/* max size of one chunk */
#define ZIP_MAX_CHUNK_SIZE	8192

/* each instruction is fixed 128 bytes */
#define ZIP_CMD_SIZE		128

#define ZIP_CMD_SIZE_WORDS	(ZIP_CMD_SIZE >> 3) /* 16 64_bit words */

/* size of next chunk buffer pointer */
#define ZIP_MAX_NCBP_SIZE	8

/* size of instruction queue in units of instruction size */
#define ZIP_MAX_NUM_CMDS	((ZIP_MAX_CHUNK_SIZE - ZIP_MAX_NCBP_SIZE) / \
				ZIP_CMD_SIZE) /* 63 */

/* size of instruct queue in bytes */
#define ZIP_MAX_CMDQ_SIZE	((ZIP_MAX_NUM_CMDS * ZIP_CMD_SIZE) + \
				ZIP_MAX_NCBP_SIZE)/* ~8072ull */

#define ZIP_BUF_SIZE	256

#define ZIP_SGPTR_ALIGN	16
#define ZIP_CMDQ_ALIGN	128
#define MAX_SG_LEN	((ZIP_BUF_SIZE - ZIP_SGPTR_ALIGN) / sizeof(void *))

/**< ZIP PMD specified queue pairs */
#define ZIP_MAX_VF_QUEUE	1

#define ZIP_ALIGN_ROUNDUP(x, _align) \
	((_align) * (((x) + (_align) - 1) / (_align)))

/**< ZIP PMD device name */
#define COMPRESSDEV_NAME_ZIP_PMD	compress_octeonx

#define ZIP_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, \
	octtx_zip_logtype_driver, "%s(): "fmt "\n", \
	__func__, ##args)

#define ZIP_PMD_INFO(fmt, args...) \
	ZIP_PMD_LOG(INFO, fmt, ## args)
#define ZIP_PMD_ERR(fmt, args...) \
	ZIP_PMD_LOG(ERR, fmt, ## args)
#define ZIP_PMD_WARN(fmt, args...) \
	ZIP_PMD_LOG(WARNING, fmt, ## args)

/**
 * ZIP VF device structure.
 */
struct zip_vf {
	int vfid;
	/* vf index */
	struct rte_pci_device *pdev;
	/* pci device */
	void *vbar0;
	/* CSR base address for underlying BAR0 VF.*/
	uint64_t dom_sdom;
	/* Storing mbox domain and subdomain id for app rerun*/
	uint32_t  max_nb_queue_pairs;
	/* pointer to device qps */
	struct rte_mempool *zip_mp;
	/* pointer to pools */
} __rte_cache_aligned;

int
zipvf_create(struct rte_compressdev *compressdev);

int
zipvf_destroy(struct rte_compressdev *compressdev);

uint64_t
zip_reg_read64(uint8_t *hw_addr, uint64_t offset);

void
zip_reg_write64(uint8_t *hw_addr, uint64_t offset, uint64_t val);

#endif /* _RTE_ZIP_VF_H_ */
