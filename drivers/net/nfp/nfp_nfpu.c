#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#include <rte_bus_pci.h>
#include <rte_malloc.h>

#include "nfp_nfpu.h"

/* PF BAR and expansion BAR for the NSP interface */
#define NFP_CFG_PCIE_BAR        0
#define NFP_CFG_EXP_BAR         7

#define NFP_CFG_EXP_BAR_CFG_BASE	0x30000

/* There could be other NFP userspace tools using the NSP interface.
 * Make sure there is no other process using it and locking the access for
 * avoiding problems.
 */
static int
nspv_aquire_process_lock(nfpu_desc_t *desc)
{
	int rc;
	struct flock lock;
	char lockname[30];

	memset(&lock, 0, sizeof(lock));

	snprintf(lockname, sizeof(lockname), "/var/lock/nfp%d", desc->nfp);

	/* Using S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH */
	desc->lock = open(lockname, O_RDWR | O_CREAT, 0666);

	if (desc->lock < 0)
		return desc->lock;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	rc = -1;
	while (rc != 0) {
		rc = fcntl(desc->lock, F_SETLK, &lock);
		if (rc < 0) {
			if ((errno != EAGAIN) && (errno != EACCES)) {
				close(desc->lock);
				return rc;
			}
		}
	}

	return 0;
}

int
nfpu_open(struct rte_pci_device *pci_dev, nfpu_desc_t *desc, int nfp)
{
	void *cfg_base, *mem_base;
	size_t barsz;
	int ret = 0;
	int i = 0;

	desc->nfp = nfp;

	ret = nspv_aquire_process_lock(desc);
	if (ret)
		return -1;

	barsz = pci_dev->mem_resource[0].len;

	/* barsz in log2 */
	while (barsz >>= 1)
		i++;

	barsz = i;

	/* Sanity check: we can assume any bar size less than 1MB an error */
	if (barsz < 20)
		return -1;

	/* Getting address for NFP expansion BAR registers */
	cfg_base = pci_dev->mem_resource[0].addr;
	cfg_base = (uint8_t *)cfg_base + NFP_CFG_EXP_BAR_CFG_BASE;

	/* Getting address for NFP NSP interface registers */
	mem_base = pci_dev->mem_resource[0].addr;
	mem_base = (uint8_t *)mem_base + (NFP_CFG_EXP_BAR << (barsz - 3));


	desc->nspu = rte_malloc("nfp nspu", sizeof(nspu_desc_t), 0);
	nfp_nspu_init(desc->nspu, desc->nfp, NFP_CFG_PCIE_BAR, barsz,
		      NFP_CFG_EXP_BAR, cfg_base, mem_base);

	return ret;
}

int
nfpu_close(nfpu_desc_t *desc)
{
	rte_free(desc->nspu);
	close(desc->lock);
	unlink("/var/lock/nfp0");
	return 0;
}
