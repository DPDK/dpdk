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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

static int              contigmem_load(void);
static int              contigmem_unload(void);
static int              contigmem_physaddr(SYSCTL_HANDLER_ARGS);

static d_mmap_t         contigmem_mmap;
static d_mmap_single_t  contigmem_mmap_single;
static d_open_t         contigmem_open;

static int              contigmem_num_buffers = RTE_CONTIGMEM_DEFAULT_NUM_BUFS;
static int64_t          contigmem_buffer_size = RTE_CONTIGMEM_DEFAULT_BUF_SIZE;

static eventhandler_tag contigmem_eh_tag;
static void            *contigmem_buffers[RTE_CONTIGMEM_MAX_NUM_BUFS];
static struct cdev     *contigmem_cdev = NULL;

TUNABLE_INT("hw.contigmem.num_buffers", &contigmem_num_buffers);
TUNABLE_QUAD("hw.contigmem.buffer_size", &contigmem_buffer_size);

static SYSCTL_NODE(_hw, OID_AUTO, contigmem, CTLFLAG_RD, 0, "contigmem");

SYSCTL_INT(_hw_contigmem, OID_AUTO, num_buffers, CTLFLAG_RD,
	&contigmem_num_buffers, 0, "Number of contigmem buffers allocated");
SYSCTL_QUAD(_hw_contigmem, OID_AUTO, buffer_size, CTLFLAG_RD,
	&contigmem_buffer_size, 0, "Size of each contiguous buffer");

static SYSCTL_NODE(_hw_contigmem, OID_AUTO, physaddr, CTLFLAG_RD, 0,
	"physaddr");

MALLOC_DEFINE(M_CONTIGMEM, "contigmem", "contigmem(4) allocations");

static int contigmem_modevent(module_t mod, int type, void *arg)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = contigmem_load();
		break;
	case MOD_UNLOAD:
		error = contigmem_unload();
		break;
	default:
		break;
	}

	return (error);
}

moduledata_t contigmem_mod = {
	"contigmem",
	(modeventhand_t)contigmem_modevent,
	0
};

DECLARE_MODULE(contigmem, contigmem_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(contigmem, 1);

static struct cdevsw contigmem_ops = {
	.d_name         = "contigmem",
	.d_version      = D_VERSION,
	.d_mmap         = contigmem_mmap,
	.d_mmap_single  = contigmem_mmap_single,
	.d_open         = contigmem_open,
};

static int
contigmem_load()
{
	char index_string[8], description[32];
	int  i;

	if (contigmem_num_buffers > RTE_CONTIGMEM_MAX_NUM_BUFS) {
		printf("%d buffers requested is greater than %d allowed\n",
				contigmem_num_buffers, RTE_CONTIGMEM_MAX_NUM_BUFS);
		return (EINVAL);
	}

	if (contigmem_buffer_size < PAGE_SIZE ||
			(contigmem_buffer_size & (contigmem_buffer_size - 1)) != 0) {
		printf("buffer size 0x%lx is not greater than PAGE_SIZE and "
				"power of two\n", contigmem_buffer_size);
		return (EINVAL);
	}

	for (i = 0; i < contigmem_num_buffers; i++) {
		contigmem_buffers[i] =
				contigmalloc(contigmem_buffer_size, M_CONTIGMEM, M_ZERO, 0,
			BUS_SPACE_MAXADDR, contigmem_buffer_size, 0);

		if (contigmem_buffers[i] == NULL) {
			printf("contigmalloc failed for buffer %d\n", i);
			return (ENOMEM);
		}

		printf("%2u: virt=%p phys=%p\n", i, contigmem_buffers[i],
				(void *)pmap_kextract((vm_offset_t)contigmem_buffers[i]));

		snprintf(index_string, sizeof(index_string), "%d", i);
		snprintf(description, sizeof(description),
				"phys addr for buffer %d", i);
		SYSCTL_ADD_PROC(NULL,
				&SYSCTL_NODE_CHILDREN(_hw_contigmem, physaddr), OID_AUTO,
				index_string, CTLTYPE_U64 | CTLFLAG_RD,
				(void *)(uintptr_t)i, 0, contigmem_physaddr, "LU",
				description);
	}

	contigmem_cdev = make_dev_credf(0, &contigmem_ops, 0, NULL, UID_ROOT,
			GID_WHEEL, 0600, "contigmem");

	return (0);
}

static int
contigmem_unload()
{
	int i;

	if (contigmem_cdev != NULL)
		destroy_dev(contigmem_cdev);

	if (contigmem_eh_tag != NULL)
		EVENTHANDLER_DEREGISTER(process_exit, contigmem_eh_tag);

	for (i = 0; i < RTE_CONTIGMEM_MAX_NUM_BUFS; i++)
		if (contigmem_buffers[i] != NULL)
			contigfree(contigmem_buffers[i], contigmem_buffer_size,
					M_CONTIGMEM);

	return (0);
}

static int
contigmem_physaddr(SYSCTL_HANDLER_ARGS)
{
	uint64_t	physaddr;
	int		index = (int)(uintptr_t)arg1;

	physaddr = (uint64_t)vtophys(contigmem_buffers[index]);
	return (sysctl_handle_64(oidp, &physaddr, 0, req));
}

static int
contigmem_open(struct cdev *cdev, int fflags, int devtype,
		struct thread *td)
{
	return (0);
}

static int
contigmem_mmap(struct cdev *cdev, vm_ooffset_t offset, vm_paddr_t *paddr,
		int prot, vm_memattr_t *memattr)
{

	*paddr = offset;
	return (0);
}

static int
contigmem_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
		struct vm_object **obj, int nprot)
{
	/*
	 * The buffer index is encoded in the offset.  Divide the offset by
	 *  PAGE_SIZE to get the index of the buffer requested by the user
	 *  app.
	 */
	if ((*offset/PAGE_SIZE) >= contigmem_num_buffers)
		return (EINVAL);

	*offset = (vm_ooffset_t)vtophys(contigmem_buffers[*offset/PAGE_SIZE]);
	*obj = vm_pager_allocate(OBJT_DEVICE, cdev, size, nprot, *offset,
			curthread->td_ucred);

	return (0);
}

