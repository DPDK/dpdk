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

#ifndef _RTE_SERVICE_H_
#define _RTE_SERVICE_H_

/**
 * @file
 *
 * Service functions
 *
 * The service functionality provided by this header allows a DPDK component
 * to indicate that it requires a function call in order for it to perform
 * its processing.
 *
 * An example usage of this functionality would be a component that registers
 * a service to perform a particular packet processing duty: for example the
 * eventdev software PMD. At startup the application requests all services
 * that have been registered, and the cores in the service-coremask run the
 * required services. The EAL removes these number of cores from the available
 * runtime cores, and dedicates them to performing service-core workloads. The
 * application has access to the remaining lcores as normal.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include<stdio.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_lcore.h>

/* forward declaration only. Definition in rte_service_private.h */
struct rte_service_spec;

#define RTE_SERVICE_NAME_MAX 32

/* Capabilities of a service.
 *
 * Use the *rte_service_probe_capability* function to check if a service is
 * capable of a specific capability.
 */
/** When set, the service is capable of having multiple threads run it at the
 *  same time.
 */
#define RTE_SERVICE_CAP_MT_SAFE (1 << 0)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 *  Return the number of services registered.
 *
 * The number of services registered can be passed to *rte_service_get_by_id*,
 * enabling the application to retrieve the specification of each service.
 *
 * @return The number of services registered.
 */
uint32_t rte_service_get_count(void);


/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Return the specification of a service by integer id.
 *
 * This function provides the specification of a service. This can be used by
 * the application to understand what the service represents. The service
 * must not be modified by the application directly, only passed to the various
 * rte_service_* functions.
 *
 * @param id The integer id of the service to retrieve
 * @retval non-zero A valid pointer to the service_spec
 * @retval NULL Invalid *id* provided.
 */
struct rte_service_spec *rte_service_get_by_id(uint32_t id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Return the specification of a service by name.
 *
 * This function provides the specification of a service using the service name
 * as lookup key. This can be used by the application to understand what the
 * service represents. The service must not be modified by the application
 * directly, only passed to the various rte_service_* functions.
 *
 * @param name The name of the service to retrieve
 * @retval non-zero A valid pointer to the service_spec
 * @retval NULL Invalid *name* provided.
 */
struct rte_service_spec *rte_service_get_by_name(const char *name);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Return the name of the service.
 *
 * @return A pointer to the name of the service. The returned pointer remains
 *         in ownership of the service, and the application must not free it.
 */
const char *rte_service_get_name(const struct rte_service_spec *service);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Check if a service has a specific capability.
 *
 * This function returns if *service* has implements *capability*.
 * See RTE_SERVICE_CAP_* defines for a list of valid capabilities.
 * @retval 1 Capability supported by this service instance
 * @retval 0 Capability not supported by this service instance
 */
int32_t rte_service_probe_capability(const struct rte_service_spec *service,
				     uint32_t capability);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Enable a core to run a service.
 *
 * Each core can be added or removed from running specific services. This
 * functions adds *lcore* to the set of cores that will run *service*.
 *
 * If multiple cores are enabled on a service, an atomic is used to ensure that
 * only one cores runs the service at a time. The exception to this is when
 * a service indicates that it is multi-thread safe by setting the capability
 * called RTE_SERVICE_CAP_MT_SAFE. With the multi-thread safe capability set,
 * the service function can be run on multiple threads at the same time.
 *
 * @retval 0 lcore added successfully
 * @retval -EINVAL An invalid service or lcore was provided.
 */
int32_t rte_service_enable_on_lcore(struct rte_service_spec *service,
				   uint32_t lcore);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Disable a core to run a service.
 *
 * Each core can be added or removed from running specific services. This
 * functions removes *lcore* to the set of cores that will run *service*.
 *
 * @retval 0 Lcore removed successfully
 * @retval -EINVAL An invalid service or lcore was provided.
 */
int32_t rte_service_disable_on_lcore(struct rte_service_spec *service,
				   uint32_t lcore);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Return if an lcore is enabled for the service.
 *
 * This function allows the application to query if *lcore* is currently set to
 * run *service*.
 *
 * @retval 1 Lcore enabled on this lcore
 * @retval 0 Lcore disabled on this lcore
 * @retval -EINVAL An invalid service or lcore was provided.
 */
int32_t rte_service_get_enabled_on_lcore(struct rte_service_spec *service,
					uint32_t lcore);


/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Enable *service* to run.
 *
 * This function switches on a service during runtime.
 * @retval 0 The service was successfully started
 */
int32_t rte_service_start(struct rte_service_spec *service);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Disable *service*.
 *
 * Switch off a service, so it is not run until it is *rte_service_start* is
 * called on it.
 * @retval 0 Service successfully switched off
 */
int32_t rte_service_stop(struct rte_service_spec *service);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Returns if *service* is currently running.
 *
 * This function returns true if the service has been started using
 * *rte_service_start*, AND a service core is mapped to the service. This
 * function can be used to ensure that the service will be run.
 *
 * @retval 1 Service is currently running, and has a service lcore mapped
 * @retval 0 Service is currently stopped, or no service lcore is mapped
 * @retval -EINVAL Invalid service pointer provided
 */
int32_t rte_service_is_running(const struct rte_service_spec *service);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Start a service core.
 *
 * Starting a core makes the core begin polling. Any services assigned to it
 * will be run as fast as possible.
 *
 * @retval 0 Success
 * @retval -EINVAL Failed to start core. The *lcore_id* passed in is not
 *          currently assigned to be a service core.
 */
int32_t rte_service_lcore_start(uint32_t lcore_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Stop a service core.
 *
 * Stopping a core makes the core become idle, but remains  assigned as a
 * service core.
 *
 * @retval 0 Success
 * @retval -EINVAL Invalid *lcore_id* provided
 * @retval -EALREADY Already stopped core
 * @retval -EBUSY Failed to stop core, as it would cause a service to not
 *          be run, as this is the only core currently running the service.
 *          The application must stop the service first, and then stop the
 *          lcore.
 */
int32_t rte_service_lcore_stop(uint32_t lcore_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Adds lcore to the list of service cores.
 *
 * This functions can be used at runtime in order to modify the service core
 * mask.
 *
 * @retval 0 Success
 * @retval -EBUSY lcore is busy, and not available for service core duty
 * @retval -EALREADY lcore is already added to the service core list
 * @retval -EINVAL Invalid lcore provided
 */
int32_t rte_service_lcore_add(uint32_t lcore);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Removes lcore from the list of service cores.
 *
 * This can fail if the core is not stopped, see *rte_service_core_stop*.
 *
 * @retval 0 Success
 * @retval -EBUSY Lcore is not stopped, stop service core before removing.
 * @retval -EINVAL failed to add lcore to service core mask.
 */
int32_t rte_service_lcore_del(uint32_t lcore);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve the number of service cores currently available.
 *
 * This function returns the integer count of service cores available. The
 * service core count can be used in mapping logic when creating mappings
 * from service cores to services.
 *
 * See *rte_service_lcore_list* for details on retrieving the lcore_id of each
 * service core.
 *
 * @return The number of service cores currently configured.
 */
int32_t rte_service_lcore_count(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Resets all service core mappings. This does not remove the service cores
 * from duty, just unmaps all services / cores, and stops() the service cores.
 * The runstate of services is not modified.
 *
 * @retval 0 Success
 */
int32_t rte_service_lcore_reset_all(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Enable or disable statistics collection for *service*.
 *
 * This function enables per core, per-service cycle count collection.
 * @param service The service to enable statistics gathering on.
 * @param enable Zero to disable statistics, non-zero to enable.
 * @retval 0 Success
 * @retval -EINVAL Invalid service pointer passed
 */
int32_t rte_service_set_stats_enable(struct rte_service_spec *service,
				  int32_t enable);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve the list of currently enabled service cores.
 *
 * This function fills in an application supplied array, with each element
 * indicating the lcore_id of a service core.
 *
 * Adding and removing service cores can be performed using
 * *rte_service_lcore_add* and *rte_service_lcore_del*.
 * @param [out] array An array of at least *rte_service_lcore_count* items.
 *              If statically allocating the buffer, use RTE_MAX_LCORE.
 * @param [out] n The size of *array*.
 * @retval >=0 Number of service cores that have been populated in the array
 * @retval -ENOMEM The provided array is not large enough to fill in the
 *          service core list. No items have been populated, call this function
 *          with a size of at least *rte_service_core_count* items.
 */
int32_t rte_service_lcore_list(uint32_t array[], uint32_t n);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Dumps any information available about the service. If service is NULL,
 * dumps info for all services.
 */
int32_t rte_service_dump(FILE *f, struct rte_service_spec *service);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_SERVICE_H_ */
