/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_cpp.h"

#include "nfp_logs.h"
#include "nfp_platform.h"
#include "nfp_target.h"
#include "nfp6000/nfp6000.h"
#include "nfp6000/nfp_xpb.h"
#include "nfp6000_pcie.h"

#define NFP_PL_DEVICE_PART_NFP6000              0x6200
#define NFP_PL_DEVICE_ID                        0x00000004
#define NFP_PL_DEVICE_ID_MASK                   0xff
#define NFP_PL_DEVICE_PART_MASK                 0xffff0000
#define NFP_PL_DEVICE_MODEL_MASK               (NFP_PL_DEVICE_PART_MASK | \
						NFP_PL_DEVICE_ID_MASK)

/* NFP CPP handle */
struct nfp_cpp {
	void *priv;  /**< Private data of the low-level implementation */

	uint32_t model;  /**< Chip model */
	uint16_t interface;  /**< Chip interface id */
	uint8_t serial[NFP_SERIAL_LEN];  /**< Chip serial number */

	/** Low-level implementation ops */
	const struct nfp_cpp_operations *op;

	/*
	 * NFP-6xxx originating island IMB CPP Address Translation. CPP Target
	 * ID is index into array. Values are obtained at runtime from local
	 * island XPB CSRs.
	 */
	uint32_t imb_cat_table[16];

	/**< MU access type bit offset */
	uint32_t mu_locality_lsb;

	bool driver_lock_needed;
};

/* NFP CPP device area handle */
struct nfp_cpp_area {
	struct nfp_cpp *cpp;
	char *name;
	uint64_t offset;
	uint32_t size;
	/* Here follows the 'priv' part of nfp_cpp_area. */
	/* Here follows the ASCII name, pointed by @name */
};

/**
 * Set the private data of the nfp_cpp instance
 *
 * @param cpp
 *   NFP CPP operations structure
 *
 * @return
 *   Opaque device pointer
 */
void
nfp_cpp_priv_set(struct nfp_cpp *cpp,
		void *priv)
{
	cpp->priv = priv;
}

/**
 * Return the private data of the nfp_cpp instance
 *
 * @param cpp
 *   NFP CPP operations structure
 *
 * @return
 *   Opaque device pointer
 */
void *
nfp_cpp_priv(struct nfp_cpp *cpp)
{
	return cpp->priv;
}

/**
 * Set the model id
 *
 * @param cpp
 *   NFP CPP operations structure
 * @param model
 *   Model ID
 */
void
nfp_cpp_model_set(struct nfp_cpp *cpp,
		uint32_t model)
{
	cpp->model = model;
}

/**
 * Retrieve the Model ID of the NFP
 *
 * @param cpp
 *   NFP CPP handle
 *
 * @return
 *   NFP CPP Model ID
 */
uint32_t
nfp_cpp_model(struct nfp_cpp *cpp)
{
	int err;
	uint32_t model;

	if (cpp == NULL)
		return NFP_CPP_MODEL_INVALID;

	err = nfp_cpp_model_autodetect(cpp, &model);

	if (err < 0)
		return err;

	return model;
}

/**
 * Set the private instance owned data of a nfp_cpp struct
 *
 * @param cpp
 *   NFP CPP operations structure
 * @param interface
 *   Interface ID
 */
void
nfp_cpp_interface_set(struct nfp_cpp *cpp,
		uint32_t interface)
{
	cpp->interface = interface;
}

/**
 * Retrieve the Serial ID of the NFP
 *
 * @param cpp
 *   NFP CPP handle
 * @param serial
 *   Pointer to NFP serial number
 *
 * @return
 *   Length of NFP serial number
 */
uint32_t
nfp_cpp_serial(struct nfp_cpp *cpp,
		const uint8_t **serial)
{
	*serial = &cpp->serial[0];

	return sizeof(cpp->serial);
}

/**
 * Set the private instance owned data of a nfp_cpp struct
 *
 * @param cpp
 *   NFP CPP operations structure
 * @param serial
 *   NFP serial byte array
 * @param serial_len
 *   Length of the serial byte array
 */
void
nfp_cpp_serial_set(struct nfp_cpp *cpp,
		const uint8_t *serial,
		size_t serial_len)
{
	memcpy(cpp->serial, serial, serial_len);
}

/**
 * Retrieve the Interface ID of the NFP
 *
 * @param cpp
 *   NFP CPP handle
 *
 * @return
 *   NFP CPP Interface ID
 */
uint16_t
nfp_cpp_interface(struct nfp_cpp *cpp)
{
	if (cpp == NULL)
		return NFP_CPP_INTERFACE(NFP_CPP_INTERFACE_TYPE_INVALID, 0, 0);

	return cpp->interface;
}

/**
 * Retrieve the driver need lock flag
 *
 * @param cpp
 *   NFP CPP handle
 *
 * @return
 *   The driver need lock flag
 */
bool
nfp_cpp_driver_need_lock(const struct nfp_cpp *cpp)
{
	return cpp->driver_lock_needed;
}

/**
 * Get the privately allocated portion of a NFP CPP area handle
 *
 * @param cpp_area
 *   NFP CPP area handle
 *
 * @return
 *   Pointer to the private area, or NULL on failure
 */
void *
nfp_cpp_area_priv(struct nfp_cpp_area *cpp_area)
{
	return &cpp_area[1];
}

/**
 * Get the NFP CPP handle that is the pci_dev of a NFP CPP area handle
 *
 * @param cpp_area
 *   NFP CPP area handle
 *
 * @return
 *   NFP CPP handle
 */
struct nfp_cpp *
nfp_cpp_area_cpp(struct nfp_cpp_area *cpp_area)
{
	return cpp_area->cpp;
}

/**
 * Get the name passed during allocation of the NFP CPP area handle
 *
 * @param cpp_area
 *   NFP CPP area handle
 *
 * @return
 *   Pointer to the area's name
 */
const char *
nfp_cpp_area_name(struct nfp_cpp_area *cpp_area)
{
	return cpp_area->name;
}

#define NFP_IMB_TGTADDRESSMODECFG_MODE_of(_x)       (((_x) >> 13) & 0x7)
#define NFP_IMB_TGTADDRESSMODECFG_ADDRMODE          RTE_BIT32(12)

static int
nfp_cpp_set_mu_locality_lsb(struct nfp_cpp *cpp)
{
	int ret;
	int mode;
	int addr40;
	uint32_t imbcppat;

	imbcppat = cpp->imb_cat_table[NFP_CPP_TARGET_MU];
	mode = NFP_IMB_TGTADDRESSMODECFG_MODE_of(imbcppat);
	addr40 = imbcppat & NFP_IMB_TGTADDRESSMODECFG_ADDRMODE;

	ret = nfp_cppat_mu_locality_lsb(mode, addr40);
	if (ret < 0)
		return ret;

	cpp->mu_locality_lsb = ret;

	return 0;
}

uint32_t
nfp_cpp_mu_locality_lsb(struct nfp_cpp *cpp)
{
	return cpp->mu_locality_lsb;
}

/**
 * Allocate and initialize a CPP area structure.
 * The area must later be locked down with an 'acquire' before
 * it can be safely accessed.
 *
 * @param cpp
 *   CPP device handle
 * @param dest
 *   CPP id
 * @param name
 *   Name of region
 * @param address
 *   Address of region
 * @param size
 *   Size of region
 *
 * @return
 *   NFP CPP area handle, or NULL
 *
 * NOTE: @address and @size must be 32-bit aligned values.
 */
struct nfp_cpp_area *
nfp_cpp_area_alloc_with_name(struct nfp_cpp *cpp,
		uint32_t dest,
		const char *name,
		uint64_t address,
		uint32_t size)
{
	int err;
	size_t name_len;
	uint32_t target_id;
	uint64_t target_addr;
	struct nfp_cpp_area *area;

	if (cpp == NULL)
		return NULL;

	/* Remap from cpp_island to cpp_target */
	err = nfp_target_cpp(dest, address, &target_id, &target_addr,
			cpp->imb_cat_table);
	if (err < 0)
		return NULL;

	if (name == NULL)
		name = "(reserved)";

	name_len = strlen(name) + 1;
	area = calloc(1, sizeof(*area) + cpp->op->area_priv_size + name_len);
	if (area == NULL)
		return NULL;

	area->cpp = cpp;
	area->name = ((char *)area) + sizeof(*area) + cpp->op->area_priv_size;
	memcpy(area->name, name, name_len);

	err = cpp->op->area_init(area, target_id, target_addr, size);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Area init op failed");
		free(area);
		return NULL;
	}

	area->offset = target_addr;
	area->size = size;

	return area;
}

/**
 * Allocate and initialize a CPP area structure.
 * The area must later be locked down with an 'acquire' before
 * it can be safely accessed.
 *
 * @param cpp
 *   CPP device handle
 * @param dest
 *   CPP id
 * @param address
 *   Address of region
 * @param size
 *   Size of region
 *
 * @return
 *   NFP CPP area handle, or NULL
 *
 * NOTE: @address and @size must be 32-bit aligned values.
 */
struct nfp_cpp_area *
nfp_cpp_area_alloc(struct nfp_cpp *cpp,
		uint32_t dest,
		uint64_t address,
		size_t size)
{
	return nfp_cpp_area_alloc_with_name(cpp, dest, NULL, address, size);
}

/**
 * Allocate and initialize a CPP area structure, and lock it down so
 * that it can be accessed directly.
 *
 * @param cpp
 *   CPP device handle
 * @param destination
 *   CPP id
 * @param address
 *   Address of region
 * @param size
 *   Size of region
 *
 * @return
 *   NFP CPP area handle, or NULL
 *
 * NOTE: @address and @size must be 32-bit aligned values.
 *
 * NOTE: The area must also be 'released' when the structure is freed.
 */
struct nfp_cpp_area *
nfp_cpp_area_alloc_acquire(struct nfp_cpp *cpp,
		uint32_t destination,
		uint64_t address,
		size_t size)
{
	struct nfp_cpp_area *area;

	area = nfp_cpp_area_alloc(cpp, destination, address, size);
	if (area == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate CPP area");
		return NULL;
	}

	if (nfp_cpp_area_acquire(area) != 0) {
		PMD_DRV_LOG(ERR, "Failed to acquire CPP area");
		nfp_cpp_area_free(area);
		return NULL;
	}

	return area;
}

/**
 * Frees up memory resources held by the CPP area.
 *
 * @param area
 *   CPP area handle
 */
void
nfp_cpp_area_free(struct nfp_cpp_area *area)
{
	if (area->cpp->op->area_cleanup != NULL)
		area->cpp->op->area_cleanup(area);
	free(area);
}

/**
 * Releases CPP area and frees up memory resources held by it.
 *
 * @param area
 *   CPP area handle
 */
void
nfp_cpp_area_release_free(struct nfp_cpp_area *area)
{
	nfp_cpp_area_release(area);
	nfp_cpp_area_free(area);
}

/**
 * Locks down the CPP area for a potential long term activity.
 * Area must always be locked down before being accessed.
 *
 * @param area
 *   CPP area handle
 *
 * @return
 *   0 on success, -1 on failure.
 */
int
nfp_cpp_area_acquire(struct nfp_cpp_area *area)
{
	if (area->cpp->op->area_acquire != NULL) {
		int err = area->cpp->op->area_acquire(area);
		if (err < 0) {
			PMD_DRV_LOG(ERR, "Area acquire op failed");
			return -1;
		}
	}

	return 0;
}

/**
 * Releases a previously locked down CPP area.
 *
 * @param area
 *   CPP area handle
 */
void
nfp_cpp_area_release(struct nfp_cpp_area *area)
{
	if (area->cpp->op->area_release != NULL)
		area->cpp->op->area_release(area);
}

/**
 * Returns an iomem pointer for use with readl()/writel() style operations.
 *
 * @param area
 *   CPP area handle
 *
 * @return
 *   Pointer to the area, or NULL
 *
 * NOTE: Area must have been locked down with an 'acquire'.
 */
void *
nfp_cpp_area_iomem(struct nfp_cpp_area *area)
{
	void *iomem = NULL;

	if (area->cpp->op->area_iomem != NULL)
		iomem = area->cpp->op->area_iomem(area);

	return iomem;
}

/**
 * Read data from indicated CPP region.
 *
 * @param area
 *   CPP area handle
 * @param offset
 *   Offset into CPP area
 * @param address
 *   Address to put data into
 * @param length
 *   Number of bytes to read
 *
 * @return
 *   Length of io, or -ERRNO
 *
 * NOTE: @offset and @length must be 32-bit aligned values.
 * NOTE: Area must have been locked down with an 'acquire'.
 */
int
nfp_cpp_area_read(struct nfp_cpp_area *area,
		uint32_t offset,
		void *address,
		size_t length)
{
	if ((offset + length) > area->size)
		return -EFAULT;

	return area->cpp->op->area_read(area, address, offset, length);
}

/**
 * Write data to indicated CPP region.
 *
 * @param area
 *   CPP area handle
 * @param offset
 *   Offset into CPP area
 * @param address
 *   Address to put data into
 * @param length
 *   Number of bytes to read
 *
 * @return
 *   Length of io, or -ERRNO
 *
 * NOTE: @offset and @length must be 32-bit aligned values.
 * NOTE: Area must have been locked down with an 'acquire'.
 */
int
nfp_cpp_area_write(struct nfp_cpp_area *area,
		uint32_t offset,
		const void *address,
		size_t length)
{
	if ((offset + length) > area->size)
		return -EFAULT;

	return area->cpp->op->area_write(area, address, offset, length);
}

/*
 * Return the correct CPP address, and fixup xpb_addr as needed,
 * based upon NFP model.
 */
static uint32_t
nfp_xpb_to_cpp(struct nfp_cpp *cpp,
		uint32_t *xpb_addr)
{
	int island;
	uint32_t xpb;

	xpb = NFP_CPP_ID(14, NFP_CPP_ACTION_RW, 0);

	/*
	 * Ensure that non-local XPB accesses go out through the
	 * global XPBM bus.
	 */
	island = (*xpb_addr >> 24) & 0x3f;

	if (island == 0)
		return xpb;

	if (island != 1) {
		*xpb_addr |= (1 << 30);
		return xpb;
	}

	/*
	 * Accesses to the ARM Island overlay uses Island 0
	 * Global Bit
	 */
	*xpb_addr &= ~0x7f000000;
	if (*xpb_addr < 0x60000) {
		*xpb_addr |= (1 << 30);
	} else {
		/* And only non-ARM interfaces use island id = 1 */
		if (NFP_CPP_INTERFACE_TYPE_of(nfp_cpp_interface(cpp)) !=
				NFP_CPP_INTERFACE_TYPE_ARM)
			*xpb_addr |= (1 << 24);
	}

	return xpb;
}

/**
 * Read a uint32_t value from an area
 *
 * @param area
 *   CPP Area handle
 * @param offset
 *   Offset into area
 * @param value
 *   Pointer to read buffer
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_area_readl(struct nfp_cpp_area *area,
		uint32_t offset,
		uint32_t *value)
{
	int sz;
	uint32_t tmp = 0;

	sz = nfp_cpp_area_read(area, offset, &tmp, sizeof(tmp));
	if (sz != sizeof(tmp))
		return sz < 0 ? sz : -EIO;

	*value = rte_le_to_cpu_32(tmp);

	return 0;
}

/**
 * Write a uint32_t vale to an area
 *
 * @param area
 *   CPP Area handle
 * @param offset
 *   Offset into area
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_area_writel(struct nfp_cpp_area *area,
		uint32_t offset,
		uint32_t value)
{
	int sz;

	value = rte_cpu_to_le_32(value);
	sz = nfp_cpp_area_write(area, offset, &value, sizeof(value));
	if (sz != sizeof(value))
		return sz < 0 ? sz : -EIO;

	return 0;
}

/**
 * Read a uint64_t value from an area
 *
 * @param area
 *   CPP Area handle
 * @param offset
 *   Offset into area
 * @param value
 *   Pointer to read buffer
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_area_readq(struct nfp_cpp_area *area,
		uint32_t offset,
		uint64_t *value)
{
	int sz;
	uint64_t tmp = 0;

	sz = nfp_cpp_area_read(area, offset, &tmp, sizeof(tmp));
	if (sz != sizeof(tmp))
		return sz < 0 ? sz : -EIO;

	*value = rte_le_to_cpu_64(tmp);

	return 0;
}

/**
 * Write a uint64_t vale to an area
 *
 * @param area
 *   CPP Area handle
 * @param offset
 *   Offset into area
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_area_writeq(struct nfp_cpp_area *area,
		uint32_t offset,
		uint64_t value)
{
	int sz;

	value = rte_cpu_to_le_64(value);
	sz = nfp_cpp_area_write(area, offset, &value, sizeof(value));
	if (sz != sizeof(value))
		return sz < 0 ? sz : -EIO;

	return 0;
}

/**
 * Read a uint32_t value from a CPP location
 *
 * @param cpp
 *   CPP device handle
 * @param cpp_id
 *   CPP ID for operation
 * @param address
 *   Address for operation
 * @param value
 *   Pointer to read buffer
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_readl(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t address,
		uint32_t *value)
{
	int sz;
	uint32_t tmp;

	sz = nfp_cpp_read(cpp, cpp_id, address, &tmp, sizeof(tmp));
	if (sz != sizeof(tmp))
		return sz < 0 ? sz : -EIO;

	*value = rte_le_to_cpu_32(tmp);

	return 0;
}

/**
 * Write a uint32_t value to a CPP location
 *
 * @param cpp
 *   CPP device handle
 * @param cpp_id
 *   CPP ID for operation
 * @param address
 *   Address for operation
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_writel(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t address,
		uint32_t value)
{
	int sz;

	value = rte_cpu_to_le_32(value);
	sz = nfp_cpp_write(cpp, cpp_id, address, &value, sizeof(value));
	if (sz != sizeof(value))
		return sz < 0 ? sz : -EIO;

	return 0;
}

/**
 * Read a uint64_t value from a CPP location
 *
 * @param cpp
 *   CPP device handle
 * @param cpp_id
 *   CPP ID for operation
 * @param address
 *   Address for operation
 * @param value
 *   Pointer to read buffer
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_readq(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t address,
		uint64_t *value)
{
	int sz;
	uint64_t tmp;

	sz = nfp_cpp_read(cpp, cpp_id, address, &tmp, sizeof(tmp));
	*value = rte_le_to_cpu_64(tmp);
	if (sz != sizeof(tmp))
		return sz < 0 ? sz : -EIO;

	return 0;
}

/**
 * Write a uint64_t value to a CPP location
 *
 * @param cpp
 *   CPP device handle
 * @param cpp_id
 *   CPP ID for operation
 * @param address
 *   Address for operation
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_cpp_writeq(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t address,
		uint64_t value)
{
	int sz;

	value = rte_cpu_to_le_64(value);
	sz = nfp_cpp_write(cpp, cpp_id, address, &value, sizeof(value));
	if (sz != sizeof(value))
		return sz < 0 ? sz : -EIO;

	return 0;
}

/**
 * Write a uint32_t word to a XPB location
 *
 * @param cpp
 *   CPP device handle
 * @param xpb_addr
 *   XPB target and address
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_xpb_writel(struct nfp_cpp *cpp,
		uint32_t xpb_addr,
		uint32_t value)
{
	uint32_t cpp_dest;

	cpp_dest = nfp_xpb_to_cpp(cpp, &xpb_addr);

	return nfp_cpp_writel(cpp, cpp_dest, xpb_addr, value);
}

/**
 * Read a uint32_t value from a XPB location
 *
 * @param cpp
 *   CPP device handle
 * @param xpb_addr
 *   XPB target and address
 * @param value
 *   Pointer to read buffer
 *
 * @return
 *   0 on success, or -ERRNO
 */
int
nfp_xpb_readl(struct nfp_cpp *cpp,
		uint32_t xpb_addr,
		uint32_t *value)
{
	uint32_t cpp_dest;

	cpp_dest = nfp_xpb_to_cpp(cpp, &xpb_addr);

	return nfp_cpp_readl(cpp, cpp_dest, xpb_addr, value);
}

static struct nfp_cpp *
nfp_cpp_alloc(struct rte_pci_device *pci_dev,
		void *priv,
		bool driver_lock_needed)
{
	int err;
	size_t target;
	uint32_t xpb_addr;
	struct nfp_cpp *cpp;
	const struct nfp_cpp_operations *ops;

	ops = nfp_cpp_transport_operations();
	if (ops == NULL || ops->init == NULL)
		return NULL;

	cpp = calloc(1, sizeof(*cpp));
	if (cpp == NULL)
		return NULL;

	cpp->op = ops;
	cpp->priv = priv;
	cpp->driver_lock_needed = driver_lock_needed;

	err = ops->get_interface(pci_dev, &cpp->interface);
	if (err != 0) {
		free(cpp);
		return NULL;
	}

	err = ops->get_serial(pci_dev, cpp->serial, NFP_SERIAL_LEN);
	if (err != 0) {
		free(cpp);
		return NULL;
	}

	/*
	 * NOTE: cpp_lock is NOT locked for op->init,
	 * since it may call NFP CPP API operations
	 */
	err = cpp->op->init(cpp);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "NFP interface initialization failed");
		free(cpp);
		return NULL;
	}

	err = nfp_cpp_model_autodetect(cpp, &cpp->model);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "NFP model detection failed");
		free(cpp);
		return NULL;
	}

	for (target = 0; target < RTE_DIM(cpp->imb_cat_table); target++) {
		/* Hardcoded XPB IMB Base, island 0 */
		xpb_addr = 0x000a0000 + (target * 4);
		err = nfp_xpb_readl(cpp, xpb_addr, &cpp->imb_cat_table[target]);
		if (err < 0) {
			PMD_DRV_LOG(ERR, "Can't read CPP mapping from device");
			free(cpp);
			return NULL;
		}
	}

	err = nfp_cpp_set_mu_locality_lsb(cpp);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Can't calculate MU locality bit offset");
		free(cpp);
		return NULL;
	}

	return cpp;
}

/**
 * Free the CPP handle
 *
 * @param cpp
 *   CPP handle
 */
void
nfp_cpp_free(struct nfp_cpp *cpp)
{
	if (cpp->op != NULL && cpp->op->free != NULL)
		cpp->op->free(cpp);

	free(cpp);
}

/**
 * Create a NFP CPP handle from device
 *
 * @param dev
 *   PCI device
 * @param priv
 *   Private data of low-level implementation
 * @param driver_lock_needed
 *   Driver lock flag
 *
 * @return
 *   NFP CPP handle on success, NULL on failure
 *
 * NOTE: On failure, cpp_ops->free will be called!
 */
struct nfp_cpp *
nfp_cpp_from_device_name(struct rte_pci_device *dev,
		void *priv,
		bool driver_lock_needed)
{
	return nfp_cpp_alloc(dev, priv, driver_lock_needed);
}

/**
 * Read from CPP target
 *
 * @param cpp
 *   CPP handle
 * @param destination
 *   CPP id
 * @param offset
 *   Offset into CPP target
 * @param address
 *   Buffer for result
 * @param length
 *   Number of bytes to read
 *
 * @return
 *   Length of io, or -ERRNO
 */
int
nfp_cpp_read(struct nfp_cpp *cpp,
		uint32_t destination,
		uint64_t offset,
		void *address,
		size_t length)
{
	int err;
	struct nfp_cpp_area *area;

	area = nfp_cpp_area_alloc_acquire(cpp, destination, offset, length);
	if (area == NULL) {
		PMD_DRV_LOG(ERR, "Area allocation/acquire failed for read");
		return -EACCES;
	}

	err = nfp_cpp_area_read(area, 0, address, length);

	nfp_cpp_area_release_free(area);
	return err;
}

/**
 * Write to CPP target
 *
 * @param cpp
 *   CPP handle
 * @param destination
 *   CPP id
 * @param offset
 *   Offset into CPP target
 * @param address
 *   Buffer to read from
 * @param length
 *   Number of bytes to write
 *
 * @return
 *   Length of io, or -ERRNO
 */
int
nfp_cpp_write(struct nfp_cpp *cpp,
		uint32_t destination,
		uint64_t offset,
		const void *address,
		size_t length)
{
	int err;
	struct nfp_cpp_area *area;

	area = nfp_cpp_area_alloc_acquire(cpp, destination, offset, length);
	if (area == NULL) {
		PMD_DRV_LOG(ERR, "Area allocation/acquire failed for write");
		return -EACCES;
	}

	err = nfp_cpp_area_write(area, 0, address, length);

	nfp_cpp_area_release_free(area);
	return err;
}

/*
 * NOTE: This code should not use nfp_xpb_* functions,
 * as those are model-specific
 */
uint32_t
nfp_cpp_model_autodetect(struct nfp_cpp *cpp,
		uint32_t *model)
{
	int err;
	uint32_t reg;

	err = nfp_xpb_readl(cpp, NFP_XPB_DEVICE(1, 1, 16) + NFP_PL_DEVICE_ID,
			&reg);
	if (err < 0)
		return err;

	*model = reg & NFP_PL_DEVICE_MODEL_MASK;
	/* Disambiguate the NFP4000/NFP5000/NFP6000 chips */
	if (FIELD_GET(NFP_PL_DEVICE_PART_MASK, reg) ==
			NFP_PL_DEVICE_PART_NFP6000) {
		if ((*model & NFP_PL_DEVICE_ID_MASK) != 0)
			*model -= 0x10;
	}

	return 0;
}

/**
 * Map an area of IOMEM access.
 * To undo the effect of this function call @nfp_cpp_area_release_free(*area).
 *
 * @param cpp
 *   NFP CPP handler
 * @param cpp_id
 *   CPP id
 * @param addr
 *   CPP address
 * @param size
 *   Size of the area
 * @param area
 *   Area handle (output)
 *
 * @return
 *   Pointer to memory mapped area or NULL
 */
uint8_t *
nfp_cpp_map_area(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t addr,
		uint32_t size,
		struct nfp_cpp_area **area)
{
	uint8_t *res;

	*area = nfp_cpp_area_alloc_acquire(cpp, cpp_id, addr, size);
	if (*area == NULL) {
		PMD_DRV_LOG(ERR, "Area allocation/acquire failed for map");
		goto err_eio;
	}

	res = nfp_cpp_area_iomem(*area);
	if (res == NULL)
		goto err_release_free;

	return res;

err_release_free:
	nfp_cpp_area_release_free(*area);
err_eio:
	return NULL;
}
