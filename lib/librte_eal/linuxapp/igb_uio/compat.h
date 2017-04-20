/*
 * Minimal wrappers to allow compiling igb_uio on older kernels.
 */

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
#define pci_cfg_access_lock   pci_block_user_cfg_access
#define pci_cfg_access_unlock pci_unblock_user_cfg_access
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
#define HAVE_PTE_MASK_PAGE_IOMAP
#endif

#ifndef PCI_MSIX_ENTRY_SIZE
#define PCI_MSIX_ENTRY_SIZE             16
#define  PCI_MSIX_ENTRY_LOWER_ADDR      0
#define  PCI_MSIX_ENTRY_UPPER_ADDR      4
#define  PCI_MSIX_ENTRY_DATA            8
#define  PCI_MSIX_ENTRY_VECTOR_CTRL     12
#define   PCI_MSIX_ENTRY_CTRL_MASKBIT   1
#endif

/*
 * for kernels < 2.6.38 and backported patch that moves MSI-X entry definition
 * to pci_regs.h Those kernels has PCI_MSIX_ENTRY_SIZE defined but not
 * PCI_MSIX_ENTRY_CTRL_MASKBIT
 */
#ifndef PCI_MSIX_ENTRY_CTRL_MASKBIT
#define PCI_MSIX_ENTRY_CTRL_MASKBIT    1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34) && \
	(!(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5, 9)))

static int pci_num_vf(struct pci_dev *dev)
{
	struct iov {
		int pos;
		int nres;
		u32 cap;
		u16 ctrl;
		u16 total;
		u16 initial;
		u16 nr_virtfn;
	} *iov = (struct iov *)dev->sriov;

	if (!dev->is_physfn)
		return 0;

	return iov->nr_virtfn;
}

#endif /* < 2.6.34 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39) && \
	(!(defined(RHEL_RELEASE_CODE) && \
	   RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 4)))

#define kstrtoul strict_strtoul

#endif /* < 2.6.39 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0) && \
	(!(defined(RHEL_RELEASE_CODE) && \
	   RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 3)))

/* Check if INTX works to control irq's.
 * Set's INTX_DISABLE flag and reads it back
 */
static bool pci_intx_mask_supported(struct pci_dev *pdev)
{
	bool mask_supported = false;
	uint16_t orig, new;

	pci_block_user_cfg_access(pdev);
	pci_read_config_word(pdev, PCI_COMMAND, &orig);
	pci_write_config_word(pdev, PCI_COMMAND,
			      orig ^ PCI_COMMAND_INTX_DISABLE);
	pci_read_config_word(pdev, PCI_COMMAND, &new);

	if ((new ^ orig) & ~PCI_COMMAND_INTX_DISABLE) {
		dev_err(&pdev->dev, "Command register changed from "
			"0x%x to 0x%x: driver or hardware bug?\n", orig, new);
	} else if ((new ^ orig) & PCI_COMMAND_INTX_DISABLE) {
		mask_supported = true;
		pci_write_config_word(pdev, PCI_COMMAND, orig);
	}
	pci_unblock_user_cfg_access(pdev);

	return mask_supported;
}

static bool pci_check_and_mask_intx(struct pci_dev *pdev)
{
	bool pending;
	uint32_t status;

	pci_block_user_cfg_access(pdev);
	pci_read_config_dword(pdev, PCI_COMMAND, &status);

	/* interrupt is not ours, goes to out */
	pending = (((status >> 16) & PCI_STATUS_INTERRUPT) != 0);
	if (pending) {
		uint16_t old, new;

		old = status;
		if (status != 0)
			new = old & (~PCI_COMMAND_INTX_DISABLE);
		else
			new = old | PCI_COMMAND_INTX_DISABLE;

		if (old != new)
			pci_write_config_word(pdev, PCI_COMMAND, new);
	}
	pci_unblock_user_cfg_access(pdev);

	return pending;
}

#endif /* < 3.3.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#define HAVE_PCI_ENABLE_MSIX
#endif
