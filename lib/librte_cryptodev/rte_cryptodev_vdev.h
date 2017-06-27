/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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
 *     * Neither the name of the copyright holder nor the names of its
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

#ifndef _RTE_CRYPTODEV_VDEV_H_
#define _RTE_CRYPTODEV_VDEV_H_

#include <rte_vdev.h>
#include <inttypes.h>

#include "rte_cryptodev.h"

#define RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS	8
#define RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS	2048

#define RTE_CRYPTODEV_VDEV_NAME				("name")
#define RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG		("max_nb_queue_pairs")
#define RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG		("max_nb_sessions")
#define RTE_CRYPTODEV_VDEV_SOCKET_ID			("socket_id")

static const char * const cryptodev_vdev_valid_params[] = {
	RTE_CRYPTODEV_VDEV_NAME,
	RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
	RTE_CRYPTODEV_VDEV_SOCKET_ID
};

/**
 * @internal
 * Initialisation parameters for virtual crypto devices
 */
struct rte_crypto_vdev_init_params {
	unsigned int max_nb_queue_pairs;
	unsigned int max_nb_sessions;
	uint8_t socket_id;
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
};

/**
 * @internal
 * Creates a new virtual crypto device and returns the pointer
 * to that device.
 *
 * @param	name			PMD type name
 * @param	dev_private_size	Size of crypto PMDs private data
 * @param	socket_id		Socket to allocate resources on.
 * @param	vdev			Pointer to virtual device structure.
 *
 * @return
 *   - Cryptodev pointer if device is successfully created.
 *   - NULL if device cannot be created.
 */
struct rte_cryptodev *
rte_cryptodev_vdev_pmd_init(const char *name, size_t dev_private_size,
		int socket_id, struct rte_vdev_device *vdev);

/**
 * @internal
 * Parse virtual device initialisation parameters input arguments
 *
 * @params	params		Initialisation parameters with defaults set.
 * @params	input_args	Command line arguments
 *
 * @return
 * 0 on successful parse
 * <0 on failure to parse
 */
int
rte_cryptodev_vdev_parse_init_params(struct rte_crypto_vdev_init_params *params,
		const char *input_args);

#endif /* _RTE_CRYPTODEV_VDEV_H_ */
