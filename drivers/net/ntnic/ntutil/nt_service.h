/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#ifndef __NT_SERVICE_H__
#define __NT_SERVICE_H__

#include <stdint.h>
#include <rte_service_component.h>
#include <rte_stdatomic.h>
#include <rte_pmd_ntnic.h>

#define NT_SERVICE_GET_STATE(srv) \
	rte_atomic_load_explicit(&(srv)->initialized, rte_memory_order_seq_cst)

#define NT_SERVICE_SET_STATE(srv, state) \
	rte_atomic_store_explicit(&(srv)->initialized, state, rte_memory_order_seq_cst)


struct nt_service {
	const enum rte_ntnic_service_tag tag;
	uint32_t id;
	uint32_t lcore;
	RTE_ATOMIC(bool) initialized;
};

/**
 * Get service information by tag.
 *
 * This function retrieves the service information based on the provided tag.
 * It returns a pointer to the nt_service structure containing the service ID
 * and other relevant information.
 *
 * @param tag The tag of the service to retrieve.
 * @return Pointer to the nt_service structure or NULL if not found.
 */
struct nt_service *nthw_service_get_info(const enum rte_ntnic_service_tag tag);

/**
 * Register and start a service with the specified tag.
 *
 * @srv_spec: Pointer to the service specification structure.
 * @tag: Tag of the service to be registered.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int nthw_service_add(struct rte_service_spec *srv_spec, const enum rte_ntnic_service_tag tag);

/**
 * Unregisters a service by its tag.
 *
 * This function stops the service, waits for it to become inactive, and then
 * unregisters it from the service component.
 *
 * @param tag The tag of the service to be unregistered.
 * @return 0 on success, negative value on failure.
 */

int nthw_service_del(const enum rte_ntnic_service_tag tag);

#endif /* __NT_SERVICE_H__ */
