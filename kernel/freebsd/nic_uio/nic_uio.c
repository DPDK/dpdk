/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h> /* defines used in kernel.h */
#include <sys/module.h>
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/conf.h> /* cdevsw struct */
#include <sys/bus.h> /* structs, prototypes for pci bus stuff and DEVMETHOD */
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <dev/pci/pcivar.h> /* For pci_get macros! */
#include <dev/pci/pcireg.h> /* The softc holds our per-instance data. */
#include <vm/vm.h>
#include <vm/uma.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include "nic_uio.h"

static MALLOC_DEFINE(M_NIC_UIO, "nic_uio", "DPDK interrupt owner");

struct nic_uio_file;

#define MAX_BARS (PCIR_MAX_BAR_0 + 1)

#define MAX_DETACHED_DEVICES	128
static device_t detached_devices[MAX_DETACHED_DEVICES] = {};
static int num_detached = 0;

struct nic_uio_softc {
	device_t        dev_t;
	struct cdev     *my_cdev;
	int              bar_id[MAX_BARS];
	struct resource *bar_res[MAX_BARS];
	u_long           bar_start[MAX_BARS];
	u_long           bar_size[MAX_BARS];
	struct sx        lock;
	struct mtx       event_lock;
	struct resource *irq;
	void            *irq_cookie;
	struct nic_uio_file *owner;
	unsigned int     opens;
	unsigned int     writers;
	bool             detaching;
};

struct nic_uio_file {
	struct nic_uio_softc *sc;
	struct knlist    knlist;
	uint32_t        events;
	uint32_t        consumed;
	bool            writable;
};

/* Function prototypes */
static d_open_t         nic_uio_open;
static d_read_t         nic_uio_read;
static d_ioctl_t        nic_uio_ioctl;
static d_kqfilter_t     nic_uio_kqfilter;
static d_mmap_t         nic_uio_mmap;
static d_mmap_single_t  nic_uio_mmap_single;
static int              nic_uio_probe(device_t dev);
static int              nic_uio_attach(device_t dev);
static int              nic_uio_detach(device_t dev);
static int              nic_uio_shutdown(void);
static int              nic_uio_modevent(module_t mod, int type, void *arg);

static struct cdevsw uio_cdevsw = {
		.d_name        = "nic_uio",
		.d_version     = D_VERSION,
		.d_open        = nic_uio_open,
		.d_read        = nic_uio_read,
		.d_ioctl       = nic_uio_ioctl,
		.d_kqfilter    = nic_uio_kqfilter,
		.d_mmap        = nic_uio_mmap,
		.d_mmap_single = nic_uio_mmap_single,
};

static device_method_t nic_uio_methods[] = {
	DEVMETHOD(device_probe,    nic_uio_probe),
	DEVMETHOD(device_attach,   nic_uio_attach),
	DEVMETHOD(device_detach,   nic_uio_detach),
	DEVMETHOD_END
};

struct device {
    int vend;
    int dev;
};

struct pci_bdf {
	uint32_t bus;
	uint32_t devid;
	uint32_t function;
};

DEFINE_CLASS_0(nic_uio, nic_uio_driver, nic_uio_methods, sizeof(struct nic_uio_softc));

#if __FreeBSD_version < 1400000
static devclass_t nic_uio_devclass;
DRIVER_MODULE(nic_uio, pci, nic_uio_driver, nic_uio_devclass, nic_uio_modevent, 0);
#else
DRIVER_MODULE(nic_uio, pci, nic_uio_driver, nic_uio_modevent, 0);
#endif

static int
nic_uio_mmap(struct cdev *cdev, vm_ooffset_t offset, vm_paddr_t *paddr,
		int prot, vm_memattr_t *memattr)
{
	*paddr = offset;
	return 0;
}

static int
nic_uio_alloc_bar(struct nic_uio_softc *sc, unsigned int bar)
{
	int type;

	sx_assert(&sc->lock, SA_XLOCKED);
	if (bar >= MAX_BARS)
		return EINVAL;
	if (sc->bar_res[bar] != NULL)
		return 0;
	sc->bar_id[bar] = PCIR_BAR(bar);
	type = PCI_BAR_IO(pci_read_config(sc->dev_t, sc->bar_id[bar], 4)) ?
		SYS_RES_IOPORT : SYS_RES_MEMORY;
	sc->bar_res[bar] = bus_alloc_resource_any(sc->dev_t, type,
		&sc->bar_id[bar], RF_ACTIVE);
	return sc->bar_res[bar] == NULL ? ENXIO : 0;
}

static int
nic_uio_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
		struct vm_object **obj, int nprot)
{
	/*
	 * The BAR index is encoded in the offset.  Divide the offset by
	 *  PAGE_SIZE to get the index of the bar requested by the user
	 *  app.
	 */
	unsigned bar = *offset/PAGE_SIZE;
	struct nic_uio_softc *sc = cdev->si_drv1;
	int error;

	sx_xlock(&sc->lock);
	error = nic_uio_alloc_bar(sc, bar);
	if (error != 0) {
		sx_xunlock(&sc->lock);
		return error;
	}

	sc->bar_start[bar] = rman_get_start(sc->bar_res[bar]);
	sc->bar_size[bar] = rman_get_size(sc->bar_res[bar]);

	device_printf(sc->dev_t, "Bar %u @ %lx, size %lx\n", bar,
			sc->bar_start[bar], sc->bar_size[bar]);

	*offset = sc->bar_start[bar];
	*obj = vm_pager_allocate(OBJT_DEVICE, cdev, size, nprot, *offset,
				curthread->td_ucred);
	sx_xunlock(&sc->lock);
	return 0;
}

static void
nic_uio_intr(void *arg)
{
	struct nic_uio_softc *sc = arg;
	struct nic_uio_file *priv;

	mtx_lock(&sc->event_lock);
	priv = sc->owner;
	priv->events++;
	KNOTE_LOCKED(&priv->knlist, 0);
	mtx_unlock(&sc->event_lock);
}

static void
nic_uio_irq_disable(struct nic_uio_softc *sc)
{
	sx_assert(&sc->lock, SA_XLOCKED);
	if (sc->irq_cookie != NULL) {
		bus_teardown_intr(sc->dev_t, sc->irq, sc->irq_cookie);
		sc->irq_cookie = NULL;
	}
}

static int
nic_uio_irq_init(struct nic_uio_file *priv)
{
	struct nic_uio_softc *sc = priv->sc;
	int count = 1, rid = 1, error;

	sx_assert(&sc->lock, SA_XLOCKED);
	if (sc->owner != NULL)
		return sc->owner == priv ? 0 : EBUSY;
	if (pci_msix_count(sc->dev_t) == 0)
		return EOPNOTSUPP;
	error = nic_uio_alloc_bar(sc, PCI_RID2BAR(pci_msix_table_bar(sc->dev_t)));
	if (error != 0)
		return error;
	error = nic_uio_alloc_bar(sc, PCI_RID2BAR(pci_msix_pba_bar(sc->dev_t)));
	if (error != 0)
		return error;
	error = pci_alloc_msix(sc->dev_t, &count);
	if (error != 0)
		return error;
	sc->irq = bus_alloc_resource_any(sc->dev_t, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq == NULL) {
		pci_release_msi(sc->dev_t);
		return ENXIO;
	}
	mtx_lock(&sc->event_lock);
	sc->owner = priv;
	mtx_unlock(&sc->event_lock);
	return 0;
}

static void
nic_uio_disable_busmaster(struct nic_uio_softc *sc)
{
	int error;

	sx_assert(&sc->lock, SA_XLOCKED);
	error = pci_disable_busmaster(sc->dev_t);
	if (error != 0)
		device_printf(sc->dev_t, "Cannot disable bus mastering: %d\n", error);
}

static void
nic_uio_file_destroy(void *arg)
{
	struct nic_uio_file *priv = arg;
	struct nic_uio_softc *sc = priv->sc;

	sx_xlock(&sc->lock);
	KASSERT(sc->opens != 0, ("nic_uio open count underflow"));
	KASSERT(!priv->writable || sc->writers != 0,
		("nic_uio writable open count underflow"));
	if (priv->writable && --sc->writers == 0)
		nic_uio_disable_busmaster(sc);
	if (sc->owner == priv) {
		nic_uio_irq_disable(sc);
		bus_release_resource(sc->dev_t, SYS_RES_IRQ, 1, sc->irq);
		pci_release_msi(sc->dev_t);
		sc->irq = NULL;
		mtx_lock(&sc->event_lock);
		sc->owner = NULL;
		mtx_unlock(&sc->event_lock);
	}
	knlist_clear(&priv->knlist, 0);
	knlist_destroy(&priv->knlist);
	free(priv, M_NIC_UIO);
	sc->opens--;
	sx_xunlock(&sc->lock);
}

int
nic_uio_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct nic_uio_softc *sc = dev->si_drv1;
	struct nic_uio_file *priv;
	int error;

	priv = malloc(sizeof(*priv), M_NIC_UIO, M_WAITOK | M_ZERO);
	priv->sc = sc;
	priv->writable = (oflags & FWRITE) != 0;
	knlist_init_mtx(&priv->knlist, &sc->event_lock);
	sx_xlock(&sc->lock);
	error = sc->detaching ? ENXIO : 0;
	if (error == 0 && priv->writable && sc->writers == 0)
		error = pci_enable_busmaster(sc->dev_t);
	if (error == 0)
		error = devfs_set_cdevpriv(priv, nic_uio_file_destroy);
	if (error == 0) {
		sc->opens++;
		if (priv->writable)
			sc->writers++;
	} else {
		if (!sc->detaching && priv->writable && sc->writers == 0)
			nic_uio_disable_busmaster(sc);
		knlist_destroy(&priv->knlist);
		free(priv, M_NIC_UIO);
	}
	sx_xunlock(&sc->lock);
	return error;
}

static int
nic_uio_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
		struct thread *td)
{
	struct nic_uio_file *priv;
	struct nic_uio_softc *sc;
	int error;

	if (cmd != NIC_UIO_IRQ_INIT && cmd != NIC_UIO_IRQ_ENABLE &&
			cmd != NIC_UIO_IRQ_DISABLE)
		return ENOTTY;
	if ((fflag & FWRITE) == 0)
		return EBADF;
	error = devfs_get_cdevpriv((void **)&priv);
	if (error != 0)
		return error;
	sc = priv->sc;
	sx_xlock(&sc->lock);
	if (cmd == NIC_UIO_IRQ_INIT) {
		error = nic_uio_irq_init(priv);
	} else if (sc->owner != priv) {
		error = EPERM;
	} else if (cmd == NIC_UIO_IRQ_DISABLE) {
		nic_uio_irq_disable(sc);
	} else if (sc->irq_cookie == NULL) {
		error = bus_setup_intr(sc->dev_t, sc->irq,
				INTR_TYPE_NET | INTR_MPSAFE, NULL, nic_uio_intr,
				sc, &sc->irq_cookie);
	}
	sx_xunlock(&sc->lock);
	return error;
}

static int
nic_uio_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct nic_uio_file *priv;
	struct nic_uio_softc *sc;
	uint32_t events;
	int error;

	if (uio->uio_resid < sizeof(events))
		return EINVAL;
	error = devfs_get_cdevpriv((void **)&priv);
	if (error != 0)
		return error;
	sc = priv->sc;
	/* Serialize readers while copying out without holding the event mutex. */
	sx_xlock(&sc->lock);
	mtx_lock(&sc->event_lock);
	events = priv->events;
	if (sc->owner != priv)
		error = EPERM;
	else if (events == priv->consumed)
		error = EAGAIN;
	mtx_unlock(&sc->event_lock);
	if (error == 0) {
		error = uiomove(&events, sizeof(events), uio);
		if (error == 0) {
			mtx_lock(&sc->event_lock);
			priv->consumed = events;
			mtx_unlock(&sc->event_lock);
		}
	}
	sx_xunlock(&sc->lock);
	return error;
}

static void
nic_uio_filter_detach(struct knote *kn)
{
	struct nic_uio_file *priv = kn->kn_hook;

	knlist_remove(&priv->knlist, kn, 0);
}

static int
nic_uio_filter_read(struct knote *kn, long hint)
{
	struct nic_uio_file *priv = kn->kn_hook;

	mtx_assert(&priv->sc->event_lock, MA_OWNED);
	kn->kn_data = priv->events != priv->consumed ? sizeof(uint32_t) : 0;
	return kn->kn_data != 0;
}

static const struct filterops nic_uio_read_filter = {
	.f_isfd = 1,
	.f_detach = nic_uio_filter_detach,
	.f_event = nic_uio_filter_read,
};

static int
nic_uio_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct nic_uio_file *priv;
	struct nic_uio_softc *sc;
	int error;

	if (kn->kn_filter != EVFILT_READ)
		return EINVAL;
	error = devfs_get_cdevpriv((void **)&priv);
	if (error != 0)
		return error;
	sc = priv->sc;
	mtx_lock(&sc->event_lock);
	if (sc->owner != priv) {
		error = ENODEV;
	} else {
		kn->kn_hook = priv;
		kn->kn_fop = &nic_uio_read_filter;
		knlist_add(&priv->knlist, kn, 1);
	}
	mtx_unlock(&sc->event_lock);
	return error;
}

static int
nic_uio_probe (device_t dev)
{
	int i;
	unsigned int bus = pci_get_bus(dev);
	unsigned int device = pci_get_slot(dev);
	unsigned int function = pci_get_function(dev);

	char bdf_str[256];
	char *token, *remaining;

	/* First check if we found this on load */
	for (i = 0; i < num_detached; i++)
		if (bus == pci_get_bus(detached_devices[i]) &&
		    device == pci_get_slot(detached_devices[i]) &&
		    function == pci_get_function(detached_devices[i])) {
			device_set_desc(dev, "DPDK PCI Device");
			return BUS_PROBE_SPECIFIC;
		}

	/* otherwise check if it's a new device and if it matches the BDF */
	memset(bdf_str, 0, sizeof(bdf_str));
	TUNABLE_STR_FETCH("hw.nic_uio.bdfs", bdf_str, sizeof(bdf_str));
	remaining = bdf_str;
	while (1) {
		if (remaining == NULL || remaining[0] == '\0')
			break;
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		bus = strtol(token, NULL, 10);
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		device = strtol(token, NULL, 10);
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		function = strtol(token, NULL, 10);

		if (bus == pci_get_bus(dev) &&
				device == pci_get_slot(dev) &&
				function == pci_get_function(dev)) {

			if (num_detached < MAX_DETACHED_DEVICES) {
				printf("%s: probed dev=%p\n",
					       __func__, dev);
				detached_devices[num_detached++] = dev;
				device_set_desc(dev, "DPDK PCI Device");
				return BUS_PROBE_SPECIFIC;
			} else {
				printf("%s: reached MAX_DETACHED_DEVICES=%d. dev=%p won't be reattached\n",
						__func__, MAX_DETACHED_DEVICES,
						dev);
				break;
			}
		}
	}

	return ENXIO;
}

static int
nic_uio_attach(device_t dev)
{
	struct make_dev_args args;
	int i, error;
	struct nic_uio_softc *sc;

	sc = device_get_softc(dev);
	sc->dev_t = dev;
	sx_init(&sc->lock, "nic_uio");
	mtx_init(&sc->event_lock, "nic_uio events", NULL, MTX_DEF);

	for (i = 0; i < MAX_BARS; i++)
		sc->bar_res[i] = NULL;
	/* Leave DMA disabled until a writable device file is opened. */
	error = pci_disable_busmaster(dev);
	if (error != 0)
		goto fail;
	make_dev_args_init(&args);
	args.mda_devsw = &uio_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0600;
	args.mda_si_drv1 = sc;
	args.mda_flags = MAKEDEV_WAITOK | MAKEDEV_CHECKNAME;
	error = make_dev_s(&args, &sc->my_cdev, "uio@pci:%u:%u:%u",
		pci_get_bus(dev), pci_get_slot(dev), pci_get_function(dev));
	if (error != 0)
		goto fail;
	return 0;

fail:
	mtx_destroy(&sc->event_lock);
	sx_destroy(&sc->lock);
	return error;
}

static int
nic_uio_detach(device_t dev)
{
	int i, error;
	struct nic_uio_softc *sc;
	sc = device_get_softc(dev);

	sx_xlock(&sc->lock);
	if (sc->opens != 0) {
		sx_xunlock(&sc->lock);
		return EBUSY;
	}
	error = pci_disable_busmaster(dev);
	if (error != 0) {
		sx_xunlock(&sc->lock);
		return error;
	}
	sc->detaching = true;
	sx_xunlock(&sc->lock);
	destroy_dev(sc->my_cdev);

	for (i = 0; i < MAX_BARS; i++)
		if (sc->bar_res[i] != NULL) {

			if (PCI_BAR_IO(pci_read_config(dev, sc->bar_id[i], 4)))
				bus_release_resource(dev, SYS_RES_IOPORT, sc->bar_id[i],
						sc->bar_res[i]);
			else
				bus_release_resource(dev, SYS_RES_MEMORY, sc->bar_id[i],
						sc->bar_res[i]);
		}

	mtx_destroy(&sc->event_lock);
	sx_destroy(&sc->lock);
	return 0;
}

static void
nic_uio_load(void)
{
	uint32_t bus, device, function;
	device_t dev;
	char bdf_str[256];
	char *token, *remaining;

	memset(bdf_str, 0, sizeof(bdf_str));
	TUNABLE_STR_FETCH("hw.nic_uio.bdfs", bdf_str, sizeof(bdf_str));
	remaining = bdf_str;
	printf("nic_uio: hw.nic_uio.bdfs = '%s'\n", bdf_str);
	/*
	 * Users should specify PCI BDFs in the format "b:d:f,b:d:f,b:d:f".
	 *  But the code below does not try differentiate between : and ,
	 *  and just blindly uses 3 tokens at a time to construct a
	 *  bus/device/function tuple.
	 *
	 * There is no checking on strtol() return values, but this should
	 *  be OK.  Worst case is it cannot convert and returns 0.  This
	 *  could give us a different BDF than intended, but as long as the
	 *  PCI device/vendor ID does not match it will not matter.
	 */
	while (1) {
		if (remaining == NULL || remaining[0] == '\0')
			break;
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		bus = strtol(token, NULL, 10);
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		device = strtol(token, NULL, 10);
		token = strsep(&remaining, ",:");
		if (token == NULL)
			break;
		function = strtol(token, NULL, 10);

		dev = pci_find_bsf(bus, device, function);
		if (dev == NULL)
			continue;

		if (num_detached < MAX_DETACHED_DEVICES) {
			printf("nic_uio_load: detaching and storing dev=%p\n",
			       dev);
			detached_devices[num_detached++] = dev;
		} else {
			printf("nic_uio_load: reached MAX_DETACHED_DEVICES=%d. dev=%p won't be reattached\n",
			       MAX_DETACHED_DEVICES, dev);
		}
		device_detach(dev);
	}
}

static void
nic_uio_unload(void)
{
	int i;
	printf("nic_uio_unload: entered...\n");

	for (i = 0; i < num_detached; i++) {
		printf("nic_uio_unload: calling to device_probe_and_attach for dev=%p...\n",
			detached_devices[i]);
		device_probe_and_attach(detached_devices[i]);
		printf("nic_uio_unload: done.\n");
	}

	printf("nic_uio_unload: leaving...\n");
}

static int
nic_uio_shutdown(void)
{
	return 0;
}

static int
nic_uio_modevent(module_t mod, int type, void *arg)
{

	switch (type) {
	case MOD_LOAD:
		nic_uio_load();
		break;
	case MOD_UNLOAD:
		nic_uio_unload();
		break;
	case MOD_SHUTDOWN:
		nic_uio_shutdown();
		break;
	default:
		break;
	}

	return 0;
}
