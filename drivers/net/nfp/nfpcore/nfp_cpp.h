/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CPP_H__
#define __NFP_CPP_H__

#include <ethdev_pci.h>

/* NFP CPP handle */
struct nfp_cpp;

/* NFP CPP device area handle */
struct nfp_cpp_area;

#define NFP_SERIAL_LEN        6

#define NFP_CPP_NUM_TARGETS             16

#define PCI_64BIT_BAR_COUNT             3

/*
 * NFP CPP operations structure
 */
struct nfp_cpp_operations {
	/* Size of priv area in struct nfp_cpp_area */
	size_t area_priv_size;

	/* Instance an NFP CPP */
	int (*init)(struct nfp_cpp *cpp);

	/*
	 * Free the bus.
	 * Called only once, during nfp_cpp_unregister()
	 */
	void (*free)(struct nfp_cpp *cpp);

	int (*get_interface)(struct rte_pci_device *dev,
			uint16_t *interface);

	int (*get_serial)(struct rte_pci_device *dev,
			uint8_t *serial,
			size_t length);

	/*
	 * Initialize a new NFP CPP area
	 * NOTE: This is _not_ serialized
	 */
	int (*area_init)(struct nfp_cpp_area *area,
			uint32_t dest,
			uint64_t address,
			size_t size);
	/*
	 * Clean up a NFP CPP area before it is freed
	 * NOTE: This is _not_ serialized
	 */
	void (*area_cleanup)(struct nfp_cpp_area *area);

	/*
	 * Acquire resources for a NFP CPP area
	 * Serialized
	 */
	int (*area_acquire)(struct nfp_cpp_area *area);

	/*
	 * Release resources for a NFP CPP area
	 * Serialized
	 */
	void (*area_release)(struct nfp_cpp_area *area);

	/*
	 * Return a void IO pointer to a NFP CPP area
	 * NOTE: This is _not_ serialized
	 */
	void *(*area_iomem)(struct nfp_cpp_area *area);

	/*
	 * Perform a read from a NFP CPP area
	 * Serialized
	 */
	int (*area_read)(struct nfp_cpp_area *area,
			void *kernel_vaddr,
			uint32_t offset,
			size_t length);
	/*
	 * Perform a write to a NFP CPP area
	 * Serialized
	 */
	int (*area_write)(struct nfp_cpp_area *area,
			const void *kernel_vaddr,
			uint32_t offset,
			size_t length);
};

/*
 * Wildcard indicating a CPP read or write action
 *
 * The action used will be either read or write depending on whether a read or
 * write instruction/call is performed on the NFP_CPP_ID.  It is recommended that
 * the RW action is used even if all actions to be performed on a NFP_CPP_ID are
 * known to be only reads or writes. Doing so will in many cases save NFP CPP
 * internal software resources.
 */
#define NFP_CPP_ACTION_RW 32

#define NFP_CPP_TARGET_ID_MASK 0x1f

/**
 * Pack target, token, and action into a CPP ID.
 *
 * Create a 32-bit CPP identifier representing the access to be made.
 * These identifiers are used as parameters to other NFP CPP functions.
 * Some CPP devices may allow wildcard identifiers to be specified.
 *
 * @param target
 *   NFP CPP target id
 * @param action
 *   NFP CPP action id
 * @param token
 *   NFP CPP token id
 *
 * @return
 *   NFP CPP ID
 */
#define NFP_CPP_ID(target, action, token)                               \
		((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
		(((action) & 0xff) << 8))

/**
 * Pack target, token, action, and island into a CPP ID.
 *
 * Create a 32-bit CPP identifier representing the access to be made.
 * These identifiers are used as parameters to other NFP CPP functions.
 * Some CPP devices may allow wildcard identifiers to be specified.
 *
 * @param target
 *   NFP CPP target id
 * @param action
 *   NFP CPP action id
 * @param token
 *   NFP CPP token id
 * @param island
 *   NFP CPP island id
 *
 * @return
 *   NFP CPP ID
 */
#define NFP_CPP_ISLAND_ID(target, action, token, island)                \
		((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
		(((action) & 0xff) << 8) | (((island) & 0xff) << 0))

/**
 * Return the NFP CPP target of a NFP CPP ID
 *
 * @param id
 *   NFP CPP ID
 *
 * @return
 *   NFP CPP target
 */
static inline uint8_t
NFP_CPP_ID_TARGET_of(uint32_t id)
{
	return (id >> 24) & NFP_CPP_TARGET_ID_MASK;
}

/**
 * Return the NFP CPP token of a NFP CPP ID
 *
 * @param id
 *   NFP CPP ID
 *
 * @return
 *   NFP CPP token
 */
static inline uint8_t
NFP_CPP_ID_TOKEN_of(uint32_t id)
{
	return (id >> 16) & 0xff;
}

/**
 * Return the NFP CPP action of a NFP CPP ID
 *
 * @param id
 *   NFP CPP ID
 *
 * @return
 *   NFP CPP action
 */
static inline uint8_t
NFP_CPP_ID_ACTION_of(uint32_t id)
{
	return (id >> 8) & 0xff;
}

/**
 * Return the NFP CPP island of a NFP CPP ID
 *
 * @param id
 *   NFP CPP ID
 *
 * @return
 *   NFP CPP island
 */
static inline uint8_t
NFP_CPP_ID_ISLAND_of(uint32_t id)
{
	return id & 0xff;
}

void nfp_cpp_model_set(struct nfp_cpp *cpp, uint32_t model);

void nfp_cpp_interface_set(struct nfp_cpp *cpp, uint32_t interface);

void nfp_cpp_serial_set(struct nfp_cpp *cpp, const uint8_t *serial,
		size_t serial_len);

void nfp_cpp_priv_set(struct nfp_cpp *cpp, void *priv);

void *nfp_cpp_priv(struct nfp_cpp *cpp);

void *nfp_cpp_area_priv(struct nfp_cpp_area *cpp_area);

uint32_t nfp_cpp_model_autodetect(struct nfp_cpp *cpp, uint32_t *model);

/* NFP CPP core interface for CPP clients */
struct nfp_cpp *nfp_cpp_from_device_name(struct rte_pci_device *dev,
		void *priv, bool driver_lock_needed);

void nfp_cpp_free(struct nfp_cpp *cpp);

#define NFP_CPP_MODEL_INVALID   0xffffffff

uint32_t nfp_cpp_model(struct nfp_cpp *cpp);

/*
 * NFP Interface types - logical interface for this CPP connection 4 bits are
 * reserved for interface type.
 */
#define NFP_CPP_INTERFACE_TYPE_INVALID          0x0
#define NFP_CPP_INTERFACE_TYPE_PCI              0x1
#define NFP_CPP_INTERFACE_TYPE_ARM              0x2
#define NFP_CPP_INTERFACE_TYPE_RPC              0x3
#define NFP_CPP_INTERFACE_TYPE_ILA              0x4

/**
 * Construct a 16-bit NFP Interface ID
 *
 * Interface IDs consists of 4 bits of interface type, 4 bits of unit
 * identifier, and 8 bits of channel identifier.
 *
 * The NFP Interface ID is used in the implementation of NFP CPP API mutexes,
 * which use the MU Atomic CompareAndWrite operation - hence the limit to 16
 * bits to be able to use the NFP Interface ID as a lock owner.
 *
 * @param type
 *   NFP Interface Type
 * @param unit
 *   Unit identifier for the interface type
 * @param channel
 *   Channel identifier for the interface unit
 *
 * @return
 *   Interface ID
 */
#define NFP_CPP_INTERFACE(type, unit, channel) \
	((((type) & 0xf) << 12) | \
	 (((unit) & 0xf) <<  8) | \
	 (((channel) & 0xff) << 0))

/**
 * Get the interface type of a NFP Interface ID
 *
 * @param interface
 *   NFP Interface ID
 *
 * @return
 *   NFP Interface ID's type
 */
#define NFP_CPP_INTERFACE_TYPE_of(interface)	(((interface) >> 12) & 0xf)

/**
 * Get the interface unit of a NFP Interface ID
 *
 * @param interface
 *   NFP Interface ID
 *
 * @return
 *   NFP Interface ID's unit
 */
#define NFP_CPP_INTERFACE_UNIT_of(interface)	(((interface) >>  8) & 0xf)

/**
 * Get the interface channel of a NFP Interface ID
 *
 * @param interface
 *   NFP Interface ID
 *
 * @return
 *   NFP Interface ID's channel
 */
#define NFP_CPP_INTERFACE_CHANNEL_of(interface)	(((interface) >>  0) & 0xff)

/*
 * Use this channel ID for multiple virtual channel interfaces
 * (ie ARM and PCIe) when setting up the interface field.
 */
#define NFP_CPP_INTERFACE_CHANNEL_PEROPENER    255

uint16_t nfp_cpp_interface(struct nfp_cpp *cpp);

uint32_t nfp_cpp_serial(struct nfp_cpp *cpp, const uint8_t **serial);

bool nfp_cpp_driver_need_lock(const struct nfp_cpp *cpp);

struct nfp_cpp_area *nfp_cpp_area_alloc(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, size_t size);

struct nfp_cpp_area *nfp_cpp_area_alloc_with_name(struct nfp_cpp *cpp,
		uint32_t cpp_id, const char *name, uint64_t address,
		uint32_t size);

void nfp_cpp_area_free(struct nfp_cpp_area *area);

int nfp_cpp_area_acquire(struct nfp_cpp_area *area);

void nfp_cpp_area_release(struct nfp_cpp_area *area);

struct nfp_cpp_area *nfp_cpp_area_alloc_acquire(struct nfp_cpp *cpp,
		uint32_t cpp_id, uint64_t address, size_t size);

void nfp_cpp_area_release_free(struct nfp_cpp_area *area);

uint8_t *nfp_cpp_map_area(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t addr, uint32_t size, struct nfp_cpp_area **area);

int nfp_cpp_area_read(struct nfp_cpp_area *area, uint32_t offset,
		void *address, size_t length);

int nfp_cpp_area_write(struct nfp_cpp_area *area, uint32_t offset,
		const void *address, size_t length);

void *nfp_cpp_area_iomem(struct nfp_cpp_area *area);

struct nfp_cpp *nfp_cpp_area_cpp(struct nfp_cpp_area *cpp_area);

const char *nfp_cpp_area_name(struct nfp_cpp_area *cpp_area);

int nfp_cpp_read(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, void *buf, size_t length);

int nfp_cpp_write(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, const void *buf, size_t length);

int nfp_cpp_area_readl(struct nfp_cpp_area *area, uint32_t offset,
		uint32_t *value);

int nfp_cpp_area_writel(struct nfp_cpp_area *area, uint32_t offset,
		uint32_t value);

int nfp_cpp_area_readq(struct nfp_cpp_area *area, uint32_t offset,
		uint64_t *value);

int nfp_cpp_area_writeq(struct nfp_cpp_area *area, uint32_t offset,
		uint64_t value);

int nfp_xpb_writel(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t value);

int nfp_xpb_readl(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t *value);

int nfp_cpp_readl(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, uint32_t *value);

int nfp_cpp_writel(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, uint32_t value);

int nfp_cpp_readq(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, uint64_t *value);

int nfp_cpp_writeq(struct nfp_cpp *cpp, uint32_t cpp_id,
		uint64_t address, uint64_t value);

uint32_t nfp_cpp_mu_locality_lsb(struct nfp_cpp *cpp);

#endif /* __NFP_CPP_H__ */
