/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#include <sys/vmmeter.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>

struct contigmem_buffer {
	void           *addr;
	int             refcnt;
	struct mtx      mtx;
};

struct contigmem_vm_handle {
	int             buffer_index;
};

static int              contigmem_load(void);
static int              contigmem_unload(void);
static int              contigmem_physaddr(SYSCTL_HANDLER_ARGS);

static d_mmap_single_t  contigmem_mmap_single;
static d_open_t         contigmem_open;
static d_close_t        contigmem_close;

static int              contigmem_num_buffers = RTE_CONTIGMEM_DEFAULT_NUM_BUFS;
static int64_t          contigmem_buffer_size = RTE_CONTIGMEM_DEFAULT_BUF_SIZE;

static eventhandler_tag contigmem_eh_tag;
static struct contigmem_buffer contigmem_buffers[RTE_CONTIGMEM_MAX_NUM_BUFS];
static struct cdev     *contigmem_cdev = NULL;
static int              contigmem_refcnt;

TUNABLE_INT("hw.contigmem.num_buffers", &contigmem_num_buffers);
TUNABLE_QUAD("hw.contigmem.buffer_size", &contigmem_buffer_size);

static SYSCTL_NODE(_hw, OID_AUTO, contigmem, CTLFLAG_RD, 0, "contigmem");

SYSCTL_INT(_hw_contigmem, OID_AUTO, num_buffers, CTLFLAG_RD,
	&contigmem_num_buffers, 0, "Number of contigmem buffers allocated");
SYSCTL_QUAD(_hw_contigmem, OID_AUTO, buffer_size, CTLFLAG_RD,
	&contigmem_buffer_size, 0, "Size of each contiguous buffer");
SYSCTL_INT(_hw_contigmem, OID_AUTO, num_references, CTLFLAG_RD,
	&contigmem_refcnt, 0, "Number of references to contigmem");

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

	return error;
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
	.d_flags        = D_TRACKCLOSE,
	.d_mmap_single  = contigmem_mmap_single,
	.d_open         = contigmem_open,
	.d_close        = contigmem_close,
};

static int
contigmem_load()
{
	char index_string[8], description[32];
	int  i, error = 0;
	void *addr;

	if (contigmem_num_buffers > RTE_CONTIGMEM_MAX_NUM_BUFS) {
		printf("%d buffers requested is greater than %d allowed\n",
				contigmem_num_buffers, RTE_CONTIGMEM_MAX_NUM_BUFS);
		error = EINVAL;
		goto error;
	}

	if (contigmem_buffer_size < PAGE_SIZE ||
			(contigmem_buffer_size & (contigmem_buffer_size - 1)) != 0) {
		printf("buffer size 0x%lx is not greater than PAGE_SIZE and "
				"power of two\n", contigmem_buffer_size);
		error = EINVAL;
		goto error;
	}

	for (i = 0; i < contigmem_num_buffers; i++) {
		addr = contigmalloc(contigmem_buffer_size, M_CONTIGMEM, M_ZERO,
			0, BUS_SPACE_MAXADDR, contigmem_buffer_size, 0);
		if (addr == NULL) {
			printf("contigmalloc failed for buffer %d\n", i);
			error = ENOMEM;
			goto error;
		}

		printf("%2u: virt=%p phys=%p\n", i, addr,
			(void *)pmap_kextract((vm_offset_t)addr));

		mtx_init(&contigmem_buffers[i].mtx, "contigmem", NULL, MTX_DEF);
		contigmem_buffers[i].addr = addr;
		contigmem_buffers[i].refcnt = 0;

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

	return 0;

error:
	for (i = 0; i < contigmem_num_buffers; i++) {
		if (contigmem_buffers[i].addr != NULL)
			contigfree(contigmem_buffers[i].addr,
				contigmem_buffer_size, M_CONTIGMEM);
		if (mtx_initialized(&contigmem_buffers[i].mtx))
			mtx_destroy(&contigmem_buffers[i].mtx);
	}

	return error;
}

static int
contigmem_unload()
{
	int i;

	if (contigmem_refcnt > 0)
		return EBUSY;

	if (contigmem_cdev != NULL)
		destroy_dev(contigmem_cdev);

	if (contigmem_eh_tag != NULL)
		EVENTHANDLER_DEREGISTER(process_exit, contigmem_eh_tag);

	for (i = 0; i < RTE_CONTIGMEM_MAX_NUM_BUFS; i++) {
		if (contigmem_buffers[i].addr != NULL)
			contigfree(contigmem_buffers[i].addr,
				contigmem_buffer_size, M_CONTIGMEM);
		if (mtx_initialized(&contigmem_buffers[i].mtx))
			mtx_destroy(&contigmem_buffers[i].mtx);
	}

	return 0;
}

static int
contigmem_physaddr(SYSCTL_HANDLER_ARGS)
{
	uint64_t	physaddr;
	int		index = (int)(uintptr_t)arg1;

	physaddr = (uint64_t)vtophys(contigmem_buffers[index].addr);
	return sysctl_handle_64(oidp, &physaddr, 0, req);
}

static int
contigmem_open(struct cdev *cdev, int fflags, int devtype,
		struct thread *td)
{

	atomic_add_int(&contigmem_refcnt, 1);

	return 0;
}

static int
contigmem_close(struct cdev *cdev, int fflags, int devtype,
		struct thread *td)
{

	atomic_subtract_int(&contigmem_refcnt, 1);

	return 0;
}

static int
contigmem_cdev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
		vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct contigmem_vm_handle *vmh = handle;
	struct contigmem_buffer *buf;

	buf = &contigmem_buffers[vmh->buffer_index];

	atomic_add_int(&contigmem_refcnt, 1);

	mtx_lock(&buf->mtx);
	if (buf->refcnt == 0)
		memset(buf->addr, 0, contigmem_buffer_size);
	buf->refcnt++;
	mtx_unlock(&buf->mtx);

	return 0;
}

static void
contigmem_cdev_pager_dtor(void *handle)
{
	struct contigmem_vm_handle *vmh = handle;
	struct contigmem_buffer *buf;

	buf = &contigmem_buffers[vmh->buffer_index];

	mtx_lock(&buf->mtx);
	buf->refcnt--;
	mtx_unlock(&buf->mtx);

	free(vmh, M_CONTIGMEM);

	atomic_subtract_int(&contigmem_refcnt, 1);
}

static int
contigmem_cdev_pager_fault(vm_object_t object, vm_ooffset_t offset, int prot,
		vm_page_t *mres)
{
	vm_paddr_t paddr;
	vm_page_t m_paddr, page;
	vm_memattr_t memattr, memattr1;

	memattr = object->memattr;

	VM_OBJECT_WUNLOCK(object);

	paddr = offset;

	m_paddr = vm_phys_paddr_to_vm_page(paddr);
	if (m_paddr != NULL) {
		memattr1 = pmap_page_get_memattr(m_paddr);
		if (memattr1 != memattr)
			memattr = memattr1;
	}

	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		VM_OBJECT_WLOCK(object);
		vm_page_updatefake(page, paddr, memattr);
	} else {
		vm_page_t mret;
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the original page.
		 */
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_WLOCK(object);
		mret = vm_page_replace(page, object, (*mres)->pindex);
		KASSERT(mret == *mres,
		    ("invalid page replacement, old=%p, ret=%p", *mres, mret));
		vm_page_lock(mret);
		vm_page_free(mret);
		vm_page_unlock(mret);
		*mres = page;
	}

	page->valid = VM_PAGE_BITS_ALL;

	return VM_PAGER_OK;
}

static struct cdev_pager_ops contigmem_cdev_pager_ops = {
	.cdev_pg_ctor = contigmem_cdev_pager_ctor,
	.cdev_pg_dtor = contigmem_cdev_pager_dtor,
	.cdev_pg_fault = contigmem_cdev_pager_fault,
};

static int
contigmem_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
		struct vm_object **obj, int nprot)
{
	struct contigmem_vm_handle *vmh;
	uint64_t buffer_index;

	/*
	 * The buffer index is encoded in the offset.  Divide the offset by
	 *  PAGE_SIZE to get the index of the buffer requested by the user
	 *  app.
	 */
	buffer_index = *offset / PAGE_SIZE;
	if (buffer_index >= contigmem_num_buffers)
		return EINVAL;

	if (size > contigmem_buffer_size)
		return EINVAL;

	vmh = malloc(sizeof(*vmh), M_CONTIGMEM, M_NOWAIT | M_ZERO);
	if (vmh == NULL)
		return ENOMEM;
	vmh->buffer_index = buffer_index;

	*offset = (vm_ooffset_t)vtophys(contigmem_buffers[buffer_index].addr);
	*obj = cdev_pager_allocate(vmh, OBJT_DEVICE, &contigmem_cdev_pager_ops,
			size, nprot, *offset, curthread->td_ucred);

	return 0;
}
