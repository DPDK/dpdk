/*
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

#ifndef _RTE_SERVICE_PRIVATE_H_
#define _RTE_SERVICE_PRIVATE_H_

/* This file specifies the internal service specification.
 * Include this file if you are writing a component that requires CPU cycles to
 * operate, and you wish to run the component using service cores
 */

#include <rte_service.h>

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Signature of callback function to run a service.
 */
typedef int32_t (*rte_service_func)(void *args);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * The specification of a service.
 *
 * This struct contains metadata about the service itself, the callback
 * function to run one iteration of the service, a userdata pointer, flags etc.
 */
struct rte_service_spec {
	/** The name of the service. This should be used by the application to
	 * understand what purpose this service provides.
	 */
	char name[RTE_SERVICE_NAME_MAX];
	/** The callback to invoke to run one iteration of the service. */
	rte_service_func callback;
	/** The userdata pointer provided to the service callback. */
	void *callback_userdata;
	/** Flags to indicate the capabilities of this service. See defines in
	 * the public header file for values of RTE_SERVICE_CAP_*
	 */
	uint32_t capabilities;
	/** NUMA socket ID that this service is affinitized to */
	int socket_id;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Register a new service.
 *
 * A service represents a component that the requires CPU time periodically to
 * achieve its purpose.
 *
 * For example the eventdev SW PMD requires CPU cycles to perform its
 * scheduling. This can be achieved by registering it as a service, and the
 * application can then assign CPU resources to it using
 * *rte_service_set_coremask*.
 *
 * @param spec The specification of the service to register
 * @retval 0 Successfully registered the service.
 *         -EINVAL Attempted to register an invalid service (eg, no callback
 *         set)
 */
int32_t rte_service_register(const struct rte_service_spec *spec);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Unregister a service.
 *
 * The service being removed must be stopped before calling this function.
 *
 * @retval 0 The service was successfully unregistered.
 * @retval -EBUSY The service is currently running, stop the service before
 *          calling unregister. No action has been taken.
 */
int32_t rte_service_unregister(struct rte_service_spec *service);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Private function to allow EAL to initialized default mappings.
 *
 * This function iterates all the services, and maps then to the available
 * cores. Based on the capabilities of the services, they are set to run on the
 * available cores in a round-robin manner.
 *
 * @retval 0 Success
 * @retval -ENOTSUP No service lcores in use
 * @retval -EINVAL Error while iterating over services
 * @retval -ENODEV Error in enabling service lcore on a service
 * @retval -ENOEXEC Error when starting services
 */
int32_t rte_service_start_with_defaults(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Initialize the service library.
 *
 * In order to use the service library, it must be initialized. EAL initializes
 * the library at startup.
 *
 * @retval 0 Success
 * @retval -EALREADY Service library is already initialized
 */
int32_t rte_service_init(void);

#endif /* _RTE_SERVICE_PRIVATE_H_ */
