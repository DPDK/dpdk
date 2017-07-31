/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>

#include <rte_service.h>
#include "include/rte_service_component.h"

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_malloc.h>

#define RTE_SERVICE_NUM_MAX 64

#define SERVICE_F_REGISTERED    (1 << 0)
#define SERVICE_F_STATS_ENABLED (1 << 1)

/* runstates for services and lcores, denoting if they are active or not */
#define RUNSTATE_STOPPED 0
#define RUNSTATE_RUNNING 1

/* internal representation of a service */
struct rte_service_spec_impl {
	/* public part of the struct */
	struct rte_service_spec spec;

	/* atomic lock that when set indicates a service core is currently
	 * running this service callback. When not set, a core may take the
	 * lock and then run the service callback.
	 */
	rte_atomic32_t execute_lock;

	/* API set/get-able variables */
	int32_t runstate;
	uint8_t internal_flags;

	/* per service statistics */
	uint32_t num_mapped_cores;
	uint64_t calls;
	uint64_t cycles_spent;
} __rte_cache_aligned;

/* the internal values of a service core */
struct core_state {
	/* map of services IDs are run on this core */
	uint64_t service_mask;
	uint8_t runstate; /* running or stopped */
	uint8_t is_service_core; /* set if core is currently a service core */

	/* extreme statistics */
	uint64_t calls_per_service[RTE_SERVICE_NUM_MAX];
} __rte_cache_aligned;

static uint32_t rte_service_count;
static struct rte_service_spec_impl *rte_services;
static struct core_state *lcore_states;
static uint32_t rte_service_library_initialized;

int32_t rte_service_init(void)
{
	if (rte_service_library_initialized) {
		printf("service library init() called, init flag %d\n",
			rte_service_library_initialized);
		return -EALREADY;
	}

	rte_services = rte_calloc("rte_services", RTE_SERVICE_NUM_MAX,
			sizeof(struct rte_service_spec_impl),
			RTE_CACHE_LINE_SIZE);
	if (!rte_services) {
		printf("error allocating rte services array\n");
		return -ENOMEM;
	}

	lcore_states = rte_calloc("rte_service_core_states", RTE_MAX_LCORE,
			sizeof(struct core_state), RTE_CACHE_LINE_SIZE);
	if (!lcore_states) {
		printf("error allocating core states array\n");
		return -ENOMEM;
	}

	int i;
	int count = 0;
	struct rte_config *cfg = rte_eal_get_configuration();
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_config[i].core_role == ROLE_SERVICE) {
			if ((unsigned int)i == cfg->master_lcore)
				continue;
			rte_service_lcore_add(i);
			count++;
		}
	}

	rte_service_library_initialized = 1;
	return 0;
}

/* returns 1 if service is registered and has not been unregistered
 * Returns 0 if service never registered, or has been unregistered
 */
static inline int
service_valid(uint32_t id)
{
	return !!(rte_services[id].internal_flags & SERVICE_F_REGISTERED);
}

/* returns 1 if statistics should be colleced for service
 * Returns 0 if statistics should not be collected for service
 */
static inline int
service_stats_enabled(struct rte_service_spec_impl *impl)
{
	return !!(impl->internal_flags & SERVICE_F_STATS_ENABLED);
}

static inline int
service_mt_safe(struct rte_service_spec_impl *s)
{
	return s->spec.capabilities & RTE_SERVICE_CAP_MT_SAFE;
}

int32_t rte_service_set_stats_enable(struct rte_service_spec *service,
				  int32_t enabled)
{
	struct rte_service_spec_impl *impl =
		(struct rte_service_spec_impl *)service;
	if (!impl)
		return -EINVAL;

	if (enabled)
		impl->internal_flags |= SERVICE_F_STATS_ENABLED;
	else
		impl->internal_flags &= ~(SERVICE_F_STATS_ENABLED);

	return 0;
}

uint32_t
rte_service_get_count(void)
{
	return rte_service_count;
}

struct rte_service_spec *
rte_service_get_by_id(uint32_t id)
{
	struct rte_service_spec *service = NULL;
	if (id < rte_service_count)
		service = (struct rte_service_spec *)&rte_services[id];

	return service;
}

struct rte_service_spec *rte_service_get_by_name(const char *name)
{
	struct rte_service_spec *service = NULL;
	int i;
	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		if (service_valid(i) &&
				strcmp(name, rte_services[i].spec.name) == 0) {
			service = (struct rte_service_spec *)&rte_services[i];
			break;
		}
	}

	return service;
}

const char *
rte_service_get_name(const struct rte_service_spec *service)
{
	return service->name;
}

int32_t
rte_service_probe_capability(const struct rte_service_spec *service,
			     uint32_t capability)
{
	return service->capabilities & capability;
}

int32_t
rte_service_is_running(const struct rte_service_spec *spec)
{
	const struct rte_service_spec_impl *impl =
		(const struct rte_service_spec_impl *)spec;
	if (!impl)
		return -EINVAL;

	return (impl->runstate == RUNSTATE_RUNNING) &&
		(impl->num_mapped_cores > 0);
}

int32_t
rte_service_register(const struct rte_service_spec *spec)
{
	uint32_t i;
	int32_t free_slot = -1;

	if (spec->callback == NULL || strlen(spec->name) == 0)
		return -EINVAL;

	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		if (!service_valid(i)) {
			free_slot = i;
			break;
		}
	}

	if ((free_slot < 0) || (i == RTE_SERVICE_NUM_MAX))
		return -ENOSPC;

	struct rte_service_spec_impl *s = &rte_services[free_slot];
	s->spec = *spec;
	s->internal_flags |= SERVICE_F_REGISTERED;

	rte_smp_wmb();
	rte_service_count++;

	return 0;
}

int32_t
rte_service_unregister(struct rte_service_spec *spec)
{
	struct rte_service_spec_impl *s = NULL;
	struct rte_service_spec_impl *spec_impl =
		(struct rte_service_spec_impl *)spec;

	uint32_t i;
	uint32_t service_id;
	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		if (&rte_services[i] == spec_impl) {
			s = spec_impl;
			service_id = i;
			break;
		}
	}

	if (!s)
		return -EINVAL;

	rte_service_count--;
	rte_smp_wmb();

	s->internal_flags &= ~(SERVICE_F_REGISTERED);

	for (i = 0; i < RTE_MAX_LCORE; i++)
		lcore_states[i].service_mask &= ~(UINT64_C(1) << service_id);

	memset(&rte_services[service_id], 0,
			sizeof(struct rte_service_spec_impl));

	return 0;
}

int32_t
rte_service_start(struct rte_service_spec *service)
{
	struct rte_service_spec_impl *s =
		(struct rte_service_spec_impl *)service;
	s->runstate = RUNSTATE_RUNNING;
	rte_smp_wmb();
	return 0;
}

int32_t
rte_service_stop(struct rte_service_spec *service)
{
	struct rte_service_spec_impl *s =
		(struct rte_service_spec_impl *)service;
	s->runstate = RUNSTATE_STOPPED;
	rte_smp_wmb();
	return 0;
}

static int32_t
rte_service_runner_func(void *arg)
{
	RTE_SET_USED(arg);
	uint32_t i;
	const int lcore = rte_lcore_id();
	struct core_state *cs = &lcore_states[lcore];

	while (lcore_states[lcore].runstate == RUNSTATE_RUNNING) {
		const uint64_t service_mask = cs->service_mask;
		for (i = 0; i < rte_service_count; i++) {
			struct rte_service_spec_impl *s = &rte_services[i];
			if (s->runstate != RUNSTATE_RUNNING ||
					!(service_mask & (UINT64_C(1) << i)))
				continue;

			/* check do we need cmpset, if MT safe or <= 1 core
			 * mapped, atomic ops are not required.
			 */
			const int need_cmpset = !((service_mt_safe(s) == 0) &&
						(s->num_mapped_cores > 1));
			uint32_t *lock = (uint32_t *)&s->execute_lock;

			if (need_cmpset || rte_atomic32_cmpset(lock, 0, 1)) {
				void *userdata = s->spec.callback_userdata;

				if (service_stats_enabled(s)) {
					uint64_t start = rte_rdtsc();
					s->spec.callback(userdata);
					uint64_t end = rte_rdtsc();
					s->cycles_spent += end - start;
					cs->calls_per_service[i]++;
					s->calls++;
				} else
					s->spec.callback(userdata);

				if (need_cmpset)
					rte_atomic32_clear(&s->execute_lock);
			}
		}

		rte_smp_rmb();
	}

	lcore_config[lcore].state = WAIT;

	return 0;
}

int32_t
rte_service_lcore_count(void)
{
	int32_t count = 0;
	uint32_t i;
	for (i = 0; i < RTE_MAX_LCORE; i++)
		count += lcore_states[i].is_service_core;
	return count;
}

int32_t
rte_service_lcore_list(uint32_t array[], uint32_t n)
{
	uint32_t count = rte_service_lcore_count();
	if (count > n)
		return -ENOMEM;

	if (!array)
		return -EINVAL;

	uint32_t i;
	uint32_t idx = 0;
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct core_state *cs = &lcore_states[i];
		if (cs->is_service_core) {
			array[idx] = i;
			idx++;
		}
	}

	return count;
}

int32_t
rte_service_start_with_defaults(void)
{
	/* create a default mapping from cores to services, then start the
	 * services to make them transparent to unaware applications.
	 */
	uint32_t i;
	int ret;
	uint32_t count = rte_service_get_count();

	int32_t lcore_iter = 0;
	uint32_t ids[RTE_MAX_LCORE];
	int32_t lcore_count = rte_service_lcore_list(ids, RTE_MAX_LCORE);

	if (lcore_count == 0)
		return -ENOTSUP;

	for (i = 0; (int)i < lcore_count; i++)
		rte_service_lcore_start(ids[i]);

	for (i = 0; i < count; i++) {
		struct rte_service_spec *s = rte_service_get_by_id(i);
		if (!s)
			return -EINVAL;

		/* do 1:1 core mapping here, with each service getting
		 * assigned a single core by default. Adding multiple services
		 * should multiplex to a single core, or 1:1 if there are the
		 * same amount of services as service-cores
		 */
		ret = rte_service_enable_on_lcore(s, ids[lcore_iter]);
		if (ret)
			return -ENODEV;

		lcore_iter++;
		if (lcore_iter >= lcore_count)
			lcore_iter = 0;

		ret = rte_service_start(s);
		if (ret)
			return -ENOEXEC;
	}

	return 0;
}

static int32_t
service_update(struct rte_service_spec *service, uint32_t lcore,
		uint32_t *set, uint32_t *enabled)
{
	uint32_t i;
	int32_t sid = -1;

	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		if ((struct rte_service_spec *)&rte_services[i] == service &&
				service_valid(i)) {
			sid = i;
			break;
		}
	}

	if (sid == -1 || lcore >= RTE_MAX_LCORE)
		return -EINVAL;

	if (!lcore_states[lcore].is_service_core)
		return -EINVAL;

	uint64_t sid_mask = UINT64_C(1) << sid;
	if (set) {
		if (*set) {
			lcore_states[lcore].service_mask |= sid_mask;
			rte_services[sid].num_mapped_cores++;
		} else {
			lcore_states[lcore].service_mask &= ~(sid_mask);
			rte_services[sid].num_mapped_cores--;
		}
	}

	if (enabled)
		*enabled = (lcore_states[lcore].service_mask & (sid_mask));

	rte_smp_wmb();

	return 0;
}

int32_t rte_service_get_enabled_on_lcore(struct rte_service_spec *service,
					uint32_t lcore)
{
	uint32_t enabled;
	int ret = service_update(service, lcore, 0, &enabled);
	if (ret == 0)
		return enabled;
	return -EINVAL;
}

int32_t
rte_service_enable_on_lcore(struct rte_service_spec *service, uint32_t lcore)
{
	uint32_t on = 1;
	return service_update(service, lcore, &on, 0);
}

int32_t
rte_service_disable_on_lcore(struct rte_service_spec *service, uint32_t lcore)
{
	uint32_t off = 0;
	return service_update(service, lcore, &off, 0);
}

int32_t rte_service_lcore_reset_all(void)
{
	/* loop over cores, reset all to mask 0 */
	uint32_t i;
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		lcore_states[i].service_mask = 0;
		lcore_states[i].is_service_core = 0;
		lcore_states[i].runstate = RUNSTATE_STOPPED;
	}
	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++)
		rte_services[i].num_mapped_cores = 0;

	rte_smp_wmb();

	return 0;
}

static void
set_lcore_state(uint32_t lcore, int32_t state)
{
	/* mark core state in hugepage backed config */
	struct rte_config *cfg = rte_eal_get_configuration();
	cfg->lcore_role[lcore] = state;

	/* mark state in process local lcore_config */
	lcore_config[lcore].core_role = state;

	/* update per-lcore optimized state tracking */
	lcore_states[lcore].is_service_core = (state == ROLE_SERVICE);
}

int32_t
rte_service_lcore_add(uint32_t lcore)
{
	if (lcore >= RTE_MAX_LCORE)
		return -EINVAL;
	if (lcore_states[lcore].is_service_core)
		return -EALREADY;

	set_lcore_state(lcore, ROLE_SERVICE);

	/* ensure that after adding a core the mask and state are defaults */
	lcore_states[lcore].service_mask = 0;
	lcore_states[lcore].runstate = RUNSTATE_STOPPED;

	rte_smp_wmb();
	return 0;
}

int32_t
rte_service_lcore_del(uint32_t lcore)
{
	if (lcore >= RTE_MAX_LCORE)
		return -EINVAL;

	struct core_state *cs = &lcore_states[lcore];
	if (!cs->is_service_core)
		return -EINVAL;

	if (cs->runstate != RUNSTATE_STOPPED)
		return -EBUSY;

	set_lcore_state(lcore, ROLE_RTE);

	rte_smp_wmb();
	return 0;
}

int32_t
rte_service_lcore_start(uint32_t lcore)
{
	if (lcore >= RTE_MAX_LCORE)
		return -EINVAL;

	struct core_state *cs = &lcore_states[lcore];
	if (!cs->is_service_core)
		return -EINVAL;

	if (cs->runstate == RUNSTATE_RUNNING)
		return -EALREADY;

	/* set core to run state first, and then launch otherwise it will
	 * return immediately as runstate keeps it in the service poll loop
	 */
	lcore_states[lcore].runstate = RUNSTATE_RUNNING;

	int ret = rte_eal_remote_launch(rte_service_runner_func, 0, lcore);
	/* returns -EBUSY if the core is already launched, 0 on success */
	return ret;
}

int32_t
rte_service_lcore_stop(uint32_t lcore)
{
	if (lcore >= RTE_MAX_LCORE)
		return -EINVAL;

	if (lcore_states[lcore].runstate == RUNSTATE_STOPPED)
		return -EALREADY;

	uint32_t i;
	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		int32_t enabled =
			lcore_states[i].service_mask & (UINT64_C(1) << i);
		int32_t service_running = rte_services[i].runstate !=
						RUNSTATE_STOPPED;
		int32_t only_core = rte_services[i].num_mapped_cores == 1;

		/* if the core is mapped, and the service is running, and this
		 * is the only core that is mapped, the service would cease to
		 * run if this core stopped, so fail instead.
		 */
		if (enabled && service_running && only_core)
			return -EBUSY;
	}

	lcore_states[lcore].runstate = RUNSTATE_STOPPED;

	return 0;
}

static void
rte_service_dump_one(FILE *f, struct rte_service_spec_impl *s,
		     uint64_t all_cycles, uint32_t reset)
{
	/* avoid divide by zero */
	if (all_cycles == 0)
		all_cycles = 1;

	int calls = 1;
	if (s->calls != 0)
		calls = s->calls;

	fprintf(f, "  %s: stats %d\tcalls %"PRIu64"\tcycles %"
			PRIu64"\tavg: %"PRIu64"\n",
			s->spec.name, service_stats_enabled(s), s->calls,
			s->cycles_spent, s->cycles_spent / calls);

	if (reset) {
		s->cycles_spent = 0;
		s->calls = 0;
	}
}

static void
service_dump_calls_per_lcore(FILE *f, uint32_t lcore, uint32_t reset)
{
	uint32_t i;
	struct core_state *cs = &lcore_states[lcore];

	fprintf(f, "%02d\t", lcore);
	for (i = 0; i < RTE_SERVICE_NUM_MAX; i++) {
		if (!service_valid(i))
			continue;
		fprintf(f, "%"PRIu64"\t", cs->calls_per_service[i]);
		if (reset)
			cs->calls_per_service[i] = 0;
	}
	fprintf(f, "\n");
}

int32_t rte_service_dump(FILE *f, struct rte_service_spec *service)
{
	uint32_t i;

	uint64_t total_cycles = 0;
	for (i = 0; i < rte_service_count; i++) {
		if (!service_valid(i))
			continue;
		total_cycles += rte_services[i].cycles_spent;
	}

	if (service) {
		struct rte_service_spec_impl *s =
			(struct rte_service_spec_impl *)service;
		fprintf(f, "Service %s Summary\n", s->spec.name);
		uint32_t reset = 0;
		rte_service_dump_one(f, s, total_cycles, reset);
		return 0;
	}

	fprintf(f, "Services Summary\n");
	for (i = 0; i < rte_service_count; i++) {
		uint32_t reset = 1;
		rte_service_dump_one(f, &rte_services[i], total_cycles, reset);
	}

	fprintf(f, "Service Cores Summary\n");
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_config[i].core_role != ROLE_SERVICE)
			continue;

		uint32_t reset = 0;
		service_dump_calls_per_lcore(f, i, reset);
	}

	return 0;
}
