/*-
 *   BSD LICENSE
 *
 *   Copyright 2014 6WIND S.A.
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
 *     * Neither the name of 6WIND S.A nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
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
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_debug.h>
#include <rte_devargs.h>

#include "test.h"

/* clear devargs list that was modified by the test */
static void free_devargs_list(void)
{
	struct rte_devargs *devargs;

	while (!TAILQ_EMPTY(&devargs_list)) {
		devargs = TAILQ_FIRST(&devargs_list);
		TAILQ_REMOVE(&devargs_list, devargs, next);
		free(devargs->args);
		free(devargs);
	}
}

static int
test_devargs(void)
{
	struct rte_devargs_list save_devargs_list;
	struct rte_devargs *devargs;

	/* save the real devargs_list, it is restored at the end of the test */
	save_devargs_list = devargs_list;
	TAILQ_INIT(&devargs_list);

	/* test valid cases */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "08:00.1") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "0000:5:00.0") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI, "04:00.0,arg=val") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI, "0000:01:00.1") < 0)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_PCI) != 2)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_BLACKLISTED_PCI) != 2)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_VIRTUAL) != 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring0") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring1,key=val,k2=val2") < 0)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_VIRTUAL) != 2)
		goto fail;
	free_devargs_list();

	/* check virtual device with argument parsing */
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring1,k1=val,k2=val2") < 0)
		goto fail;
	devargs = TAILQ_FIRST(&devargs_list);
	if (strncmp(devargs->virt.drv_name, "net_ring1",
			sizeof(devargs->virt.drv_name)) != 0)
		goto fail;
	if (!devargs->args || strcmp(devargs->args, "k1=val,k2=val2") != 0)
		goto fail;
	free_devargs_list();

	/* check PCI device with empty argument parsing */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "04:00.1") < 0)
		goto fail;
	devargs = TAILQ_FIRST(&devargs_list);
	if (devargs->pci.addr.domain != 0 ||
		devargs->pci.addr.bus != 4 ||
		devargs->pci.addr.devid != 0 ||
		devargs->pci.addr.function != 1)
		goto fail;
	if (!devargs->args || strcmp(devargs->args, "") != 0)
		goto fail;
	free_devargs_list();

	/* test error case: bad PCI address */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "08:1") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "00.1") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "foo") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, ",") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "000f:0:0") == 0)
		goto fail;

	devargs_list = save_devargs_list;
	return 0;

 fail:
	free_devargs_list();
	devargs_list = save_devargs_list;
	return -1;
}

REGISTER_TEST_COMMAND(devargs_autotest, test_devargs);
