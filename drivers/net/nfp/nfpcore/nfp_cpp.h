/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CPP_H__
#define __NFP_CPP_H__

#include <ethdev_pci.h>

struct nfp_cpp_mutex;

/*
 * NFP CPP handle
 */
struct nfp_cpp {
	uint32_t model;
	uint32_t interface;
	uint8_t *serial;
	int serial_len;
	void *priv;

	/* Mutex cache */
	struct nfp_cpp_mutex *mutex_cache;
	const struct nfp_cpp_operations *op;

	/*
	 * NFP-6xxx originating island IMB CPP Address Translation. CPP Target
	 * ID is index into array. Values are obtained at runtime from local
	 * island XPB CSRs.
	 */
	uint32_t imb_cat_table[16];

	/* MU access type bit offset */
	uint32_t mu_locality_lsb;

	int driver_lock_needed;
};

/*
 * NFP CPP device area handle
 */
struct nfp_cpp_area {
	struct nfp_cpp *cpp;
	char *name;
	unsigned long long offset;
	unsigned long size;
	/* Here follows the 'priv' part of nfp_cpp_area. */
};

/*
 * NFP CPP operations structure
 */
struct nfp_cpp_operations {
	/* Size of priv area in struct nfp_cpp_area */
	size_t area_priv_size;

	/* Instance an NFP CPP */
	int (*init)(struct nfp_cpp *cpp, struct rte_pci_device *dev);

	/*
	 * Free the bus.
	 * Called only once, during nfp_cpp_unregister()
	 */
	void (*free)(struct nfp_cpp *cpp);

	/*
	 * Initialize a new NFP CPP area
	 * NOTE: This is _not_ serialized
	 */
	int (*area_init)(struct nfp_cpp_area *area,
			 uint32_t dest,
			 unsigned long long address,
			 unsigned long size);
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

	void *(*area_mapped)(struct nfp_cpp_area *area);
	/*
	 * Perform a read from a NFP CPP area
	 * Serialized
	 */
	int (*area_read)(struct nfp_cpp_area *area,
			 void *kernel_vaddr,
			 unsigned long offset,
			 unsigned int length);
	/*
	 * Perform a write to a NFP CPP area
	 * Serialized
	 */
	int (*area_write)(struct nfp_cpp_area *area,
			  const void *kernel_vaddr,
			  unsigned long offset,
			  unsigned int length);
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

/*
 * Pack target, token, and action into a CPP ID.
 *
 * Create a 32-bit CPP identifier representing the access to be made.
 * These identifiers are used as parameters to other NFP CPP functions.
 * Some CPP devices may allow wildcard identifiers to be specified.
 *
 * @target      NFP CPP target id
 * @action      NFP CPP action id
 * @token       NFP CPP token id
 *
 * @return      NFP CPP ID
 */
#define NFP_CPP_ID(target, action, token)                               \
		((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
		(((action) & 0xff) << 8))

/*
 * Pack target, token, action, and island into a CPP ID.
 * @target      NFP CPP target id
 * @action      NFP CPP action id
 * @token       NFP CPP token id
 * @island      NFP CPP island id
 *
 * Create a 32-bit CPP identifier representing the access to be made.
 * These identifiers are used as parameters to other NFP CPP functions.
 * Some CPP devices may allow wildcard identifiers to be specified.
 *
 * @return      NFP CPP ID
 */
#define NFP_CPP_ISLAND_ID(target, action, token, island)                \
		((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
		(((action) & 0xff) << 8) | (((island) & 0xff) << 0))

/**
 * Return the NFP CPP target of a NFP CPP ID
 * @id          NFP CPP ID
 *
 * @return      NFP CPP target
 */
static inline uint8_t
NFP_CPP_ID_TARGET_of(uint32_t id)
{
	return (id >> 24) & NFP_CPP_TARGET_ID_MASK;
}

/*
 * Return the NFP CPP token of a NFP CPP ID
 * @id          NFP CPP ID
 *
 * @return      NFP CPP token
 */
static inline uint8_t
NFP_CPP_ID_TOKEN_of(uint32_t id)
{
	return (id >> 16) & 0xff;
}

/*
 * Return the NFP CPP action of a NFP CPP ID
 * @id          NFP CPP ID
 *
 * @return      NFP CPP action
 */
static inline uint8_t
NFP_CPP_ID_ACTION_of(uint32_t id)
{
	return (id >> 8) & 0xff;
}

/*
 * Return the NFP CPP island of a NFP CPP ID
 * @id          NFP CPP ID
 *
 * @return      NFP CPP island
 */
static inline uint8_t
NFP_CPP_ID_ISLAND_of(uint32_t id)
{
	return id & 0xff;
}

/*
 * This should be the only external function the transport
 * module supplies
 */
const struct nfp_cpp_operations *nfp_cpp_transport_operations(void);

/*
 * Set the model id
 *
 * @param   cpp     NFP CPP operations structure
 * @param   model   Model ID
 */
void nfp_cpp_model_set(struct nfp_cpp *cpp, uint32_t model);

/*
 * Set the private instance owned data of a nfp_cpp struct
 *
 * @param   cpp     NFP CPP operations structure
 * @param   interface Interface ID
 */
void nfp_cpp_interface_set(struct nfp_cpp *cpp, uint32_t interface);

/*
 * Set the private instance owned data of a nfp_cpp struct
 *
 * @param   cpp     NFP CPP operations structure
 * @param   serial  NFP serial byte array
 * @param   len     Length of the serial byte array
 */
int nfp_cpp_serial_set(struct nfp_cpp *cpp, const uint8_t *serial,
		       size_t serial_len);

/*
 * Set the private data of the nfp_cpp instance
 *
 * @param   cpp NFP CPP operations structure
 * @return      Opaque device pointer
 */
void nfp_cpp_priv_set(struct nfp_cpp *cpp, void *priv);

/*
 * Return the private data of the nfp_cpp instance
 *
 * @param   cpp NFP CPP operations structure
 * @return      Opaque device pointer
 */
void *nfp_cpp_priv(struct nfp_cpp *cpp);

/*
 * Get the privately allocated portion of a NFP CPP area handle
 *
 * @param   cpp_area    NFP CPP area handle
 * @return          Pointer to the private area, or NULL on failure
 */
void *nfp_cpp_area_priv(struct nfp_cpp_area *cpp_area);

uint32_t __nfp_cpp_model_autodetect(struct nfp_cpp *cpp, uint32_t *model);

/*
 * NFP CPP core interface for CPP clients.
 */

/*
 * Open a NFP CPP handle to a CPP device
 *
 * @param[in]	id	0-based ID for the CPP interface to use
 *
 * @return NFP CPP handle, or NULL on failure.
 */
struct nfp_cpp *nfp_cpp_from_device_name(struct rte_pci_device *dev,
					 int driver_lock_needed);

/*
 * Free a NFP CPP handle
 *
 * @param[in]	cpp	NFP CPP handle
 */
void nfp_cpp_free(struct nfp_cpp *cpp);

#define NFP_CPP_MODEL_INVALID   0xffffffff

/*
 * NFP_CPP_MODEL_CHIP_of - retrieve the chip ID from the model ID
 *
 * The chip ID is a 16-bit BCD+A-F encoding for the chip type.
 *
 * @param[in]   model   NFP CPP model id
 * @return      NFP CPP chip id
 */
#define NFP_CPP_MODEL_CHIP_of(model)        (((model) >> 16) & 0xffff)

/*
 * NFP_CPP_MODEL_IS_6000 - Check for the NFP6000 family of devices
 *
 * NOTE: The NFP4000 series is considered as a NFP6000 series variant.
 *
 * @param[in]	model	NFP CPP model id
 * @return		true if model is in the NFP6000 family, false otherwise.
 */
#define NFP_CPP_MODEL_IS_6000(model)		     \
		((NFP_CPP_MODEL_CHIP_of(model) >= 0x3800) && \
		(NFP_CPP_MODEL_CHIP_of(model) < 0x7000))

/*
 * nfp_cpp_model - Retrieve the Model ID of the NFP
 *
 * @param[in]	cpp	NFP CPP handle
 * @return		NFP CPP Model ID
 */
uint32_t nfp_cpp_model(struct nfp_cpp *cpp);

/*
 * NFP Interface types - logical interface for this CPP connection 4 bits are
 * reserved for interface type.
 */
#define NFP_CPP_INTERFACE_TYPE_INVALID		0x0
#define NFP_CPP_INTERFACE_TYPE_PCI		0x1
#define NFP_CPP_INTERFACE_TYPE_ARM		0x2
#define NFP_CPP_INTERFACE_TYPE_RPC		0x3
#define NFP_CPP_INTERFACE_TYPE_ILA		0x4

/*
 * Construct a 16-bit NFP Interface ID
 *
 * Interface IDs consists of 4 bits of interface type, 4 bits of unit
 * identifier, and 8 bits of channel identifier.
 *
 * The NFP Interface ID is used in the implementation of NFP CPP API mutexes,
 * which use the MU Atomic CompareAndWrite operation - hence the limit to 16
 * bits to be able to use the NFP Interface ID as a lock owner.
 *
 * @param[in]	type	NFP Interface Type
 * @param[in]	unit	Unit identifier for the interface type
 * @param[in]	channel	Channel identifier for the interface unit
 * @return		Interface ID
 */
#define NFP_CPP_INTERFACE(type, unit, channel)	\
	((((type) & 0xf) << 12) | \
	 (((unit) & 0xf) <<  8) | \
	 (((channel) & 0xff) << 0))

/*
 * Get the interface type of a NFP Interface ID
 * @param[in]	interface	NFP Interface ID
 * @return			NFP Interface ID's type
 */
#define NFP_CPP_INTERFACE_TYPE_of(interface)	(((interface) >> 12) & 0xf)

/*
 * Get the interface unit of a NFP Interface ID
 * @param[in]	interface	NFP Interface ID
 * @return			NFP Interface ID's unit
 */
#define NFP_CPP_INTERFACE_UNIT_of(interface)	(((interface) >>  8) & 0xf)

/*
 * Get the interface channel of a NFP Interface ID
 * @param[in]	interface	NFP Interface ID
 * @return			NFP Interface ID's channel
 */
#define NFP_CPP_INTERFACE_CHANNEL_of(interface)	(((interface) >>  0) & 0xff)

/*
 * Retrieve the Interface ID of the NFP
 * @param[in]	cpp	NFP CPP handle
 * @return		NFP CPP Interface ID
 */
uint16_t nfp_cpp_interface(struct nfp_cpp *cpp);

/*
 * Retrieve the NFP Serial Number (unique per NFP)
 * @param[in]	cpp	NFP CPP handle
 * @param[out]	serial	Pointer to reference the serial number array
 *
 * @return	size of the NFP6000 serial number, in bytes
 */
int nfp_cpp_serial(struct nfp_cpp *cpp, const uint8_t **serial);

/*
 * Allocate a NFP CPP area handle, as an offset into a CPP ID
 * @param[in]	cpp	NFP CPP handle
 * @param[in]	cpp_id	NFP CPP ID
 * @param[in]	address	Offset into the NFP CPP ID address space
 * @param[in]	size	Size of the area to reserve
 *
 * @return NFP CPP handle, or NULL on failure.
 */
struct nfp_cpp_area *nfp_cpp_area_alloc(struct nfp_cpp *cpp, uint32_t cpp_id,
					unsigned long long address,
					unsigned long size);

/*
 * Allocate a NFP CPP area handle, as an offset into a CPP ID, by a named owner
 * @param[in]	cpp	NFP CPP handle
 * @param[in]	cpp_id	NFP CPP ID
 * @param[in]	name	Name of owner of the area
 * @param[in]	address	Offset into the NFP CPP ID address space
 * @param[in]	size	Size of the area to reserve
 *
 * @return NFP CPP handle, or NULL on failure.
 */
struct nfp_cpp_area *nfp_cpp_area_alloc_with_name(struct nfp_cpp *cpp,
						  uint32_t cpp_id,
						  const char *name,
						  unsigned long long address,
						  unsigned long size);

/*
 * Free an allocated NFP CPP area handle
 * @param[in]	area	NFP CPP area handle
 */
void nfp_cpp_area_free(struct nfp_cpp_area *area);

/*
 * Acquire the resources needed to access the NFP CPP area handle
 *
 * @param[in]	area	NFP CPP area handle
 *
 * @return 0 on success, -1 on failure.
 */
int nfp_cpp_area_acquire(struct nfp_cpp_area *area);

/*
 * Release the resources needed to access the NFP CPP area handle
 *
 * @param[in]	area	NFP CPP area handle
 */
void nfp_cpp_area_release(struct nfp_cpp_area *area);

/*
 * Allocate, then acquire the resources needed to access the NFP CPP area handle
 * @param[in]	cpp	NFP CPP handle
 * @param[in]	cpp_id	NFP CPP ID
 * @param[in]	address	Offset into the NFP CPP ID address space
 * @param[in]	size	Size of the area to reserve
 *
 * @return NFP CPP handle, or NULL on failure.
 */
struct nfp_cpp_area *nfp_cpp_area_alloc_acquire(struct nfp_cpp *cpp,
						uint32_t cpp_id,
						unsigned long long address,
						unsigned long size);

/*
 * Release the resources, then free the NFP CPP area handle
 * @param[in]	area	NFP CPP area handle
 */
void nfp_cpp_area_release_free(struct nfp_cpp_area *area);

uint8_t *nfp_cpp_map_area(struct nfp_cpp *cpp, uint32_t cpp_id,
			   uint64_t addr, unsigned long size,
			   struct nfp_cpp_area **area);
/*
 * Return an IO pointer to the beginning of the NFP CPP area handle. The area
 * must be acquired with 'nfp_cpp_area_acquire()' before calling this operation.
 *
 * @param[in]	area	NFP CPP area handle
 *
 * @return Pointer to IO memory, or NULL on failure.
 */
void *nfp_cpp_area_mapped(struct nfp_cpp_area *area);

/*
 * Read from a NFP CPP area handle into a buffer. The area must be acquired with
 * 'nfp_cpp_area_acquire()' before calling this operation.
 *
 * @param[in]	area	NFP CPP area handle
 * @param[in]	offset	Offset into the area
 * @param[in]	buffer	Location of buffer to receive the data
 * @param[in]	length	Length of the data to read
 *
 * @return bytes read on success, negative value on failure.
 *
 */
int nfp_cpp_area_read(struct nfp_cpp_area *area, unsigned long offset,
		      void *buffer, size_t length);

/*
 * Write to a NFP CPP area handle from a buffer. The area must be acquired with
 * 'nfp_cpp_area_acquire()' before calling this operation.
 *
 * @param[in]	area	NFP CPP area handle
 * @param[in]	offset	Offset into the area
 * @param[in]	buffer	Location of buffer that holds the data
 * @param[in]	length	Length of the data to read
 *
 * @return bytes written on success, negative value on failure.
 */
int nfp_cpp_area_write(struct nfp_cpp_area *area, unsigned long offset,
		       const void *buffer, size_t length);

/*
 * nfp_cpp_area_iomem() - get IOMEM region for CPP area
 * @area:       CPP area handle
 *
 * Returns an iomem pointer for use with readl()/writel() style operations.
 *
 * NOTE: Area must have been locked down with an 'acquire'.
 *
 * Return: pointer to the area, or NULL
 */
void *nfp_cpp_area_iomem(struct nfp_cpp_area *area);

/*
 * Verify that IO can be performed on an offset in an area
 *
 * @param[in]	area	NFP CPP area handle
 * @param[in]	offset	Offset into the area
 * @param[in]	size	Size of region to validate
 *
 * @return 0 on success, negative value on failure.
 */
int nfp_cpp_area_check_range(struct nfp_cpp_area *area,
			     unsigned long long offset, unsigned long size);

/*
 * Get the NFP CPP handle that is the parent of a NFP CPP area handle
 *
 * @param	cpp_area	NFP CPP area handle
 * @return			NFP CPP handle
 */
struct nfp_cpp *nfp_cpp_area_cpp(struct nfp_cpp_area *cpp_area);

/*
 * Get the name passed during allocation of the NFP CPP area handle
 *
 * @param	cpp_area	NFP CPP area handle
 * @return			Pointer to the area's name
 */
const char *nfp_cpp_area_name(struct nfp_cpp_area *cpp_area);

/*
 * Read a block of data from a NFP CPP ID
 *
 * @param[in]	cpp	NFP CPP handle
 * @param[in]	cpp_id	NFP CPP ID
 * @param[in]	address	Offset into the NFP CPP ID address space
 * @param[in]	kernel_vaddr	Buffer to copy read data to
 * @param[in]	length	Size of the area to reserve
 *
 * @return bytes read on success, -1 on failure.
 */
int nfp_cpp_read(struct nfp_cpp *cpp, uint32_t cpp_id,
		 unsigned long long address, void *kernel_vaddr, size_t length);

/*
 * Write a block of data to a NFP CPP ID
 *
 * @param[in]	cpp	NFP CPP handle
 * @param[in]	cpp_id	NFP CPP ID
 * @param[in]	address	Offset into the NFP CPP ID address space
 * @param[in]	kernel_vaddr	Buffer to copy write data from
 * @param[in]	length	Size of the area to reserve
 *
 * @return bytes written on success, -1 on failure.
 */
int nfp_cpp_write(struct nfp_cpp *cpp, uint32_t cpp_id,
		  unsigned long long address, const void *kernel_vaddr,
		  size_t length);



/*
 * Fill a NFP CPP area handle and offset with a value
 *
 * @param[in]	area	NFP CPP area handle
 * @param[in]	offset	Offset into the NFP CPP ID address space
 * @param[in]	value	32-bit value to fill area with
 * @param[in]	length	Size of the area to reserve
 *
 * @return bytes written on success, negative value on failure.
 */
int nfp_cpp_area_fill(struct nfp_cpp_area *area, unsigned long offset,
		      uint32_t value, size_t length);

/*
 * Read a single 32-bit value from a NFP CPP area handle
 *
 * @param area		NFP CPP area handle
 * @param offset	offset into NFP CPP area handle
 * @param value		output value
 *
 * The area must be acquired with 'nfp_cpp_area_acquire()' before calling this
 * operation.
 *
 * NOTE: offset must be 32-bit aligned.
 *
 * @return 0 on success, or -1 on error.
 */
int nfp_cpp_area_readl(struct nfp_cpp_area *area, unsigned long offset,
		       uint32_t *value);

/*
 * Write a single 32-bit value to a NFP CPP area handle
 *
 * @param area		NFP CPP area handle
 * @param offset	offset into NFP CPP area handle
 * @param value		value to write
 *
 * The area must be acquired with 'nfp_cpp_area_acquire()' before calling this
 * operation.
 *
 * NOTE: offset must be 32-bit aligned.
 *
 * @return 0 on success, or -1 on error.
 */
int nfp_cpp_area_writel(struct nfp_cpp_area *area, unsigned long offset,
			uint32_t value);

/*
 * Read a single 64-bit value from a NFP CPP area handle
 *
 * @param area		NFP CPP area handle
 * @param offset	offset into NFP CPP area handle
 * @param value		output value
 *
 * The area must be acquired with 'nfp_cpp_area_acquire()' before calling this
 * operation.
 *
 * NOTE: offset must be 64-bit aligned.
 *
 * @return 0 on success, or -1 on error.
 */
int nfp_cpp_area_readq(struct nfp_cpp_area *area, unsigned long offset,
		       uint64_t *value);

/*
 * Write a single 64-bit value to a NFP CPP area handle
 *
 * @param area		NFP CPP area handle
 * @param offset	offset into NFP CPP area handle
 * @param value		value to write
 *
 * The area must be acquired with 'nfp_cpp_area_acquire()' before calling this
 * operation.
 *
 * NOTE: offset must be 64-bit aligned.
 *
 * @return 0 on success, or -1 on error.
 */
int nfp_cpp_area_writeq(struct nfp_cpp_area *area, unsigned long offset,
			uint64_t value);

/*
 * Write a single 32-bit value on the XPB bus
 *
 * @param cpp           NFP CPP device handle
 * @param xpb_tgt	XPB target and address
 * @param value         value to write
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_xpb_writel(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t value);

/*
 * Read a single 32-bit value from the XPB bus
 *
 * @param cpp           NFP CPP device handle
 * @param xpb_tgt	XPB target and address
 * @param value         output value
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_xpb_readl(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t *value);

/*
 * Modify bits of a 32-bit value from the XPB bus
 *
 * @param cpp           NFP CPP device handle
 * @param xpb_tgt       XPB target and address
 * @param mask          mask of bits to alter
 * @param value         value to modify
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_xpb_writelm(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t mask,
		    uint32_t value);

/*
 * Modify bits of a 32-bit value from the XPB bus
 *
 * @param cpp           NFP CPP device handle
 * @param xpb_tgt       XPB target and address
 * @param mask          mask of bits to alter
 * @param value         value to monitor for
 * @param timeout_us    maximum number of us to wait (-1 for forever)
 *
 * @return >= 0 on success, negative value on failure.
 */
int nfp_xpb_waitlm(struct nfp_cpp *cpp, uint32_t xpb_tgt, uint32_t mask,
		   uint32_t value, int timeout_us);

/*
 * Read a 32-bit word from a NFP CPP ID
 *
 * @param cpp           NFP CPP handle
 * @param cpp_id        NFP CPP ID
 * @param address       offset into the NFP CPP ID address space
 * @param value         output value
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_cpp_readl(struct nfp_cpp *cpp, uint32_t cpp_id,
		  unsigned long long address, uint32_t *value);

/*
 * Write a 32-bit value to a NFP CPP ID
 *
 * @param cpp           NFP CPP handle
 * @param cpp_id        NFP CPP ID
 * @param address       offset into the NFP CPP ID address space
 * @param value         value to write
 *
 * @return 0 on success, or -1 on failure.
 *
 */
int nfp_cpp_writel(struct nfp_cpp *cpp, uint32_t cpp_id,
		   unsigned long long address, uint32_t value);

/*
 * Read a 64-bit work from a NFP CPP ID
 *
 * @param cpp           NFP CPP handle
 * @param cpp_id        NFP CPP ID
 * @param address       offset into the NFP CPP ID address space
 * @param value         output value
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_cpp_readq(struct nfp_cpp *cpp, uint32_t cpp_id,
		  unsigned long long address, uint64_t *value);

/*
 * Write a 64-bit value to a NFP CPP ID
 *
 * @param cpp           NFP CPP handle
 * @param cpp_id        NFP CPP ID
 * @param address       offset into the NFP CPP ID address space
 * @param value         value to write
 *
 * @return 0 on success, or -1 on failure.
 */
int nfp_cpp_writeq(struct nfp_cpp *cpp, uint32_t cpp_id,
		   unsigned long long address, uint64_t value);

/*
 * Initialize a mutex location

 * The CPP target:address must point to a 64-bit aligned location, and will
 * initialize 64 bits of data at the location.
 *
 * This creates the initial mutex state, as locked by this nfp_cpp_interface().
 *
 * This function should only be called when setting up the initial lock state
 * upon boot-up of the system.
 *
 * @param cpp		NFP CPP handle
 * @param target	NFP CPP target ID
 * @param address	Offset into the address space of the NFP CPP target ID
 * @param key_id	Unique 32-bit value for this mutex
 *
 * @return 0 on success, negative value on failure.
 */
int nfp_cpp_mutex_init(struct nfp_cpp *cpp, int target,
		       unsigned long long address, uint32_t key_id);

/*
 * Create a mutex handle from an address controlled by a MU Atomic engine
 *
 * The CPP target:address must point to a 64-bit aligned location, and reserve
 * 64 bits of data at the location for use by the handle.
 *
 * Only target/address pairs that point to entities that support the MU Atomic
 * Engine's CmpAndSwap32 command are supported.
 *
 * @param cpp		NFP CPP handle
 * @param target	NFP CPP target ID
 * @param address	Offset into the address space of the NFP CPP target ID
 * @param key_id	32-bit unique key (must match the key at this location)
 *
 * @return		A non-NULL struct nfp_cpp_mutex * on success, NULL on
 *                      failure.
 */
struct nfp_cpp_mutex *nfp_cpp_mutex_alloc(struct nfp_cpp *cpp, int target,
					  unsigned long long address,
					  uint32_t key_id);

/*
 * Get the NFP CPP handle the mutex was created with
 *
 * @param   mutex   NFP mutex handle
 * @return          NFP CPP handle
 */
struct nfp_cpp *nfp_cpp_mutex_cpp(struct nfp_cpp_mutex *mutex);

/*
 * Get the mutex key
 *
 * @param   mutex   NFP mutex handle
 * @return          Mutex key
 */
uint32_t nfp_cpp_mutex_key(struct nfp_cpp_mutex *mutex);

/*
 * Get the mutex owner
 *
 * @param   mutex   NFP mutex handle
 * @return          Interface ID of the mutex owner
 *
 * NOTE: This is for debug purposes ONLY - the owner may change at any time,
 * unless it has been locked by this NFP CPP handle.
 */
uint16_t nfp_cpp_mutex_owner(struct nfp_cpp_mutex *mutex);

/*
 * Get the mutex target
 *
 * @param   mutex   NFP mutex handle
 * @return          Mutex CPP target (ie NFP_CPP_TARGET_MU)
 */
int nfp_cpp_mutex_target(struct nfp_cpp_mutex *mutex);

/*
 * Get the mutex address
 *
 * @param   mutex   NFP mutex handle
 * @return          Mutex CPP address
 */
uint64_t nfp_cpp_mutex_address(struct nfp_cpp_mutex *mutex);

/*
 * Free a mutex handle - does not alter the lock state
 *
 * @param mutex		NFP CPP Mutex handle
 */
void nfp_cpp_mutex_free(struct nfp_cpp_mutex *mutex);

/*
 * Lock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex		NFP CPP Mutex handle
 *
 * @return 0 on success, negative value on failure.
 */
int nfp_cpp_mutex_lock(struct nfp_cpp_mutex *mutex);

/*
 * Unlock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex		NFP CPP Mutex handle
 *
 * @return 0 on success, negative value on failure.
 */
int nfp_cpp_mutex_unlock(struct nfp_cpp_mutex *mutex);

/*
 * Attempt to lock a mutex handle, using the NFP MU Atomic Engine
 *
 * @param mutex		NFP CPP Mutex handle
 * @return		0 if the lock succeeded, negative value on failure.
 */
int nfp_cpp_mutex_trylock(struct nfp_cpp_mutex *mutex);

uint32_t nfp_cpp_mu_locality_lsb(struct nfp_cpp *cpp);

#endif /* !__NFP_CPP_H__ */
