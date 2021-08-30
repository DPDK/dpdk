/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_debug.h>
#include <rte_cycles.h>

/* allow application scheduling of the services */
#include <rte_service.h>

/* Allow application registration of its own services. An application does not
 * have to register services, but it can be useful if it wishes to run a
 * function on a core that is otherwise in use as a service core. In this
 * example, all services are dummy services registered by the sample app itself.
 */
#include <rte_service_component.h>

#define PROFILE_CORES_MAX 8
#define PROFILE_SERVICE_PER_CORE 5

/* dummy function to do "work" */
static int32_t service_func(void *args)
{
	RTE_SET_USED(args);
	rte_delay_us(2000);
	return 0;
}

static struct rte_service_spec services[] = {
	{"service_1", service_func, NULL, 0, 0},
	{"service_2", service_func, NULL, 0, 0},
	{"service_3", service_func, NULL, 0, 0},
	{"service_4", service_func, NULL, 0, 0},
	{"service_5", service_func, NULL, 0, 0},
};
#define NUM_SERVICES RTE_DIM(services)

/* this struct holds the mapping of a particular core to all services */
struct profile_for_core {
	uint32_t mapped_services[PROFILE_SERVICE_PER_CORE];
};

/* struct that can be applied as the service core mapping. Items in this
 * struct will be passed to the ordinary rte_service_* APIs to configure the
 * service cores at runtime, based on the requirements.
 *
 * These profiles can be considered a "configuration" for the service cores,
 * where switching profile just changes the number of cores and the mappings
 * for each of them. As a result, the core requirements and performance of the
 * application scales.
 */
struct profile {
	char name[64];
	uint32_t num_cores;
	struct profile_for_core cores[PROFILE_CORES_MAX];
};

static struct profile profiles[] = {
	/* profile 0: high performance */
	{
		.name = "High Performance",
		.num_cores = 5,
		.cores[0] = {.mapped_services = {1, 0, 0, 0, 0} },
		.cores[1] = {.mapped_services = {0, 1, 0, 0, 0} },
		.cores[2] = {.mapped_services = {0, 0, 1, 0, 0} },
		.cores[3] = {.mapped_services = {0, 0, 0, 1, 0} },
		.cores[4] = {.mapped_services = {0, 0, 0, 0, 1} },
	},
	/* profile 1: mid performance with single service priority */
	{
		.name = "Mid-High Performance",
		.num_cores = 3,
		.cores[0] = {.mapped_services = {1, 1, 0, 0, 0} },
		.cores[1] = {.mapped_services = {0, 0, 1, 1, 0} },
		.cores[2] = {.mapped_services = {0, 0, 0, 0, 1} },
		.cores[3] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[4] = {.mapped_services = {0, 0, 0, 0, 0} },
	},
	/* profile 2: mid performance with single service priority */
	{
		.name = "Mid-Low Performance",
		.num_cores = 2,
		.cores[0] = {.mapped_services = {1, 1, 1, 0, 0} },
		.cores[1] = {.mapped_services = {1, 1, 0, 1, 1} },
		.cores[2] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[3] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[4] = {.mapped_services = {0, 0, 0, 0, 0} },
	},
	/* profile 3: scale down performance on single core */
	{
		.name = "Scale down performance",
		.num_cores = 1,
		.cores[0] = {.mapped_services = {1, 1, 1, 1, 1} },
		.cores[1] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[2] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[3] = {.mapped_services = {0, 0, 0, 0, 0} },
		.cores[4] = {.mapped_services = {0, 0, 0, 0, 0} },
	},
};
#define NUM_PROFILES RTE_DIM(profiles)

static void
apply_profile(int profile_id)
{
	uint32_t i;
	uint32_t s;
	int ret;
	struct profile *p = &profiles[profile_id];
	const uint8_t core_off = 1;

	if (p->num_cores > rte_lcore_count() - 1) {
		printf("insufficent cores to run (%s)",
			p->name);
		return;
	}

	for (i = 0; i < p->num_cores; i++) {
		uint32_t core = i + core_off;
		ret = rte_service_lcore_add(core);
		if (ret && ret != -EALREADY)
			printf("core %d added ret %d\n", core, ret);

		ret = rte_service_lcore_start(core);
		if (ret && ret != -EALREADY)
			printf("core %d start ret %d\n", core, ret);

		for (s = 0; s < NUM_SERVICES; s++) {
			if (rte_service_map_lcore_set(s, core,
					p->cores[i].mapped_services[s]))
				printf("failed to map lcore %d\n", core);
		}
	}

	for ( ; i < PROFILE_CORES_MAX; i++) {
		uint32_t core = i + core_off;
		for (s = 0; s < NUM_SERVICES; s++) {
			ret = rte_service_map_lcore_set(s, core, 0);
			if (ret && ret != -EINVAL) {
				printf("%s %d: map lcore set = %d\n", __func__,
						__LINE__, ret);
			}
		}
		ret = rte_service_lcore_stop(core);
		if (ret && ret != -EALREADY) {
			printf("%s %d: lcore stop = %d\n", __func__,
					__LINE__, ret);
		}
		ret = rte_service_lcore_del(core);
		if (ret && ret != -EINVAL) {
			printf("%s %d: lcore del = %d\n", __func__,
					__LINE__, ret);
		}
	}
}

int
main(int argc, char **argv)
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	uint32_t i;
	for (i = 0; i < NUM_SERVICES; i++) {
		services[i].callback_userdata = 0;
		uint32_t id;
		/* Register a service as an application. 8< */
		ret = rte_service_component_register(&services[i], &id);
		if (ret)
			rte_exit(-1, "service register() failed");

		/* set the service itself to be ready to run. In the case of
		 * ethdev, eventdev etc PMDs, this will be set when the
		 * appropriate configure or setup function is called.
		 */
		rte_service_component_runstate_set(id, 1);

		/* Collect statistics for the service */
		rte_service_set_stats_enable(id, 1);

		/* the application sets the service to be active. Note that the
		 * previous component_runstate_set() is the PMD indicating
		 * ready, while this function is the application setting the
		 * service to run. Applications can choose to not run a service
		 * by setting runstate to 0 at any time.
		 */
		ret = rte_service_runstate_set(id, 1);
		if (ret)
			return -ENOEXEC;
		/* >8 End of registering a service as an application. */
	}

	i = 0;
	while (1) {
		const char clr[] = { 27, '[', '2', 'J', '\0' };
		const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
		printf("%s%s", clr, topLeft);

		apply_profile(i);
		printf("\n==> Profile: %s\n\n", profiles[i].name);

		rte_delay_us_sleep(1 * US_PER_S);
		rte_service_dump(stdout, UINT32_MAX);

		rte_delay_us_sleep(5 * US_PER_S);
		rte_service_dump(stdout, UINT32_MAX);

		i++;
		if (i >= NUM_PROFILES)
			i = 0;
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
