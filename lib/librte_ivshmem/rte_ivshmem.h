/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef RTE_IVSHMEM_H_
#define RTE_IVSHMEM_H_

#include <rte_memzone.h>
#include <rte_mempool.h>

/**
 * @file
 *
 * The RTE IVSHMEM interface provides functions to create metadata files
 * describing memory segments to be shared via QEMU IVSHMEM.
 */


#ifdef __cplusplus
extern "C" {
#endif

#define IVSHMEM_MAGIC 0x0BADC0DE
#define IVSHMEM_NAME_LEN 32

/**
 * Structure that holds IVSHMEM shared metadata entry.
 */
struct rte_ivshmem_metadata_entry {
	struct rte_memzone mz;	/**< shared memzone */
	uint64_t offset;	/**< offset of memzone within IVSHMEM device */
};

/**
 * Structure that holds IVSHMEM metadata.
 */
struct rte_ivshmem_metadata {
	int magic_number;				/**< magic number */
	char name[IVSHMEM_NAME_LEN];	/**< name of the metadata file */
	struct rte_ivshmem_metadata_entry entry[RTE_LIBRTE_IVSHMEM_MAX_ENTRIES];
			/**< metadata entries */
};

/**
 * Creates metadata file with a given name
 *
 * @param name
 *  Name of metadata file to be created
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value
 */
int rte_ivshmem_metadata_create(const char * name);

/**
 * Adds memzone to a specific metadata file
 *
 * @param mz
 *  Memzone to be added
 * @param md_name
 *  Name of metadata file for the memzone to be added to
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value
 */
int rte_ivshmem_metadata_add_memzone(const struct rte_memzone * mz,
		const char * md_name);

/**
 * Adds a ring descriptor to a specific metadata file
 *
 * @param r
 *  Ring descriptor to be added
 * @param md_name
 *  Name of metadata file for the ring to be added to
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value
 */
int rte_ivshmem_metadata_add_ring(const struct rte_ring * r,
		const char * md_name);

/**
 * Adds a mempool to a specific metadata file
 *
 * @param mp
 *  Mempool to be added
 * @param md_name
 *  Name of metadata file for the mempool to be added to
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value
 */
int rte_ivshmem_metadata_add_mempool(const struct rte_mempool * mp,
		const char * md_name);


/**
 * Generates the QEMU command-line for IVSHMEM device for a given metadata file.
 * This function is to be called after all the objects were added.
 *
 * @param buffer
 *  Buffer to be filled with the command line arguments.
 * @param size
 *  Size of the buffer.
 * @param name
 *  Name of metadata file to generate QEMU command-line parameters for
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value
 */
int rte_ivshmem_metadata_cmdline_generate(char *buffer, unsigned size,
		const char *name);


/**
 * Dump all metadata entries from a given metadata file to the console.
 *
 * @param f
 *   A pointer to a file for output
 * @name
 *  Name of the metadata file to be dumped to console.
 */
void rte_ivshmem_metadata_dump(FILE *f, const char *name);


#ifdef __cplusplus
}
#endif

#endif /* RTE_IVSHMEM_H_ */
