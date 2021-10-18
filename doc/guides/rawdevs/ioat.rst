..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

.. include:: <isonum.txt>

IOAT Rawdev Driver
===================

.. warning::
        As of DPDK 21.11 the rawdev implementation of the IOAT driver has been deprecated.
        Please use the dmadev library instead.

The ``ioat`` rawdev driver provides a poll-mode driver (PMD) for Intel\ |reg|
Data Streaming Accelerator `(Intel DSA)
<https://01.org/blogs/2019/introducing-intel-data-streaming-accelerator>`_ and for Intel\ |reg|
QuickData Technology, part of Intel\ |reg| I/O Acceleration Technology
`(Intel I/OAT)
<https://www.intel.com/content/www/us/en/wireless-network/accel-technology.html>`_.
This PMD, when used on supported hardware, allows data copies, for example,
cloning packet data, to be accelerated by that hardware rather than having to
be done by software, freeing up CPU cycles for other tasks.

Hardware Requirements
----------------------

The ``dpdk-devbind.py`` script, included with DPDK,
can be used to show the presence of supported hardware.
Running ``dpdk-devbind.py --status-dev misc`` will show all the miscellaneous,
or rawdev-based devices on the system.
For Intel\ |reg| QuickData Technology devices, the hardware will be often listed as "Crystal Beach DMA",
or "CBDMA".
For Intel\ |reg| DSA devices, they are currently (at time of writing) appearing as devices with type "0b25",
due to the absence of pci-id database entries for them at this point.

Compilation
------------

For builds using ``meson`` and ``ninja``, the driver will be built when the target platform is x86-based.
No additional compilation steps are necessary.

.. note::
        Since the addition of the dmadev library, the ``ioat`` and ``idxd`` parts of this driver
        will only be built if their ``dmadev`` counterparts are not built.
        The following can be used to disable the ``dmadev`` drivers,
        if the raw drivers are to be used instead::

                $ meson -Ddisable_drivers=dma/* <build_dir>

Device Setup
-------------

Depending on support provided by the PMD, HW devices can either use the kernel configured driver
or be bound to a user-space IO driver for use.
For example, Intel\ |reg| DSA devices can use the IDXD kernel driver or DPDK-supported drivers,
such as ``vfio-pci``.

Intel\ |reg| DSA devices using idxd kernel driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use a Intel\ |reg| DSA device bound to the IDXD kernel driver, the device must first be configured.
The `accel-config <https://github.com/intel/idxd-config>`_ utility library can be used for configuration.

.. note::
        The device configuration can also be done by directly interacting with the sysfs nodes.
        An example of how this may be done can be seen in the script ``dpdk_idxd_cfg.py``
        included in the driver source directory.

There are some mandatory configuration steps before being able to use a device with an application.
The internal engines, which do the copies or other operations,
and the work-queues, which are used by applications to assign work to the device,
need to be assigned to groups, and the various other configuration options,
such as priority or queue depth, need to be set for each queue.

To assign an engine to a group::

        $ accel-config config-engine dsa0/engine0.0 --group-id=0
        $ accel-config config-engine dsa0/engine0.1 --group-id=1

To assign work queues to groups for passing descriptors to the engines a similar accel-config command can be used.
However, the work queues also need to be configured depending on the use case.
Some configuration options include:

* mode (Dedicated/Shared): Indicates whether a WQ may accept jobs from multiple queues simultaneously.
* priority: WQ priority between 1 and 15. Larger value means higher priority.
* wq-size: the size of the WQ. Sum of all WQ sizes must be less that the total-size defined by the device.
* type: WQ type (kernel/mdev/user). Determines how the device is presented.
* name: identifier given to the WQ.

Example configuration for a work queue::

        $ accel-config config-wq dsa0/wq0.0 --group-id=0 \
           --mode=dedicated --priority=10 --wq-size=8 \
           --type=user --name=dpdk_app1

Once the devices have been configured, they need to be enabled::

        $ accel-config enable-device dsa0
        $ accel-config enable-wq dsa0/wq0.0

Check the device configuration::

        $ accel-config list

Devices using VFIO/UIO drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The HW devices to be used will need to be bound to a user-space IO driver for use.
The ``dpdk-devbind.py`` script can be used to view the state of the devices
and to bind them to a suitable DPDK-supported driver, such as ``vfio-pci``.
For example::

	$ dpdk-devbind.py -b vfio-pci 00:04.0 00:04.1

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For devices bound to a suitable DPDK-supported VFIO/UIO driver, the HW devices will
be found as part of the device scan done at application initialization time without
the need to pass parameters to the application.

For Intel\ |reg| DSA devices, DPDK will automatically configure the device with the
maximum number of workqueues available on it, partitioning all resources equally
among the queues.
If fewer workqueues are required, then the ``max_queues`` parameter may be passed to
the device driver on the EAL commandline, via the ``allowlist`` or ``-a`` flag e.g.::

	$ dpdk-test -a <b:d:f>,max_queues=4

For devices bound to the IDXD kernel driver,
the DPDK ioat driver will automatically perform a scan for available workqueues to use.
Any workqueues found listed in ``/dev/dsa`` on the system will be checked in ``/sys``,
and any which have ``dpdk_`` prefix in their name will be automatically probed by the
driver to make them available to the application.
Alternatively, to support use by multiple DPDK processes simultaneously,
the value used as the DPDK ``--file-prefix`` parameter may be used as a workqueue name prefix,
instead of ``dpdk_``,
allowing each DPDK application instance to only use a subset of configured queues.

Once probed successfully, irrespective of kernel driver, the device will appear as a ``rawdev``,
that is a "raw device type" inside DPDK, and can be accessed using APIs from the
``rte_rawdev`` library.

Using IOAT Rawdev Devices
--------------------------

To use the devices from an application, the rawdev API can be used, along
with definitions taken from the device-specific header file
``rte_ioat_rawdev.h``. This header is needed to get the definition of
structure parameters used by some of the rawdev APIs for IOAT rawdev
devices, as well as providing key functions for using the device for memory
copies.

Getting Device Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Basic information about each rawdev device can be queried using the
``rte_rawdev_info_get()`` API. For most applications, this API will be
needed to verify that the rawdev in question is of the expected type. For
example, the following code snippet can be used to identify an IOAT
rawdev device for use by an application:

.. code-block:: C

        for (i = 0; i < count && !found; i++) {
                struct rte_rawdev_info info = { .dev_private = NULL };
                found = (rte_rawdev_info_get(i, &info, 0) == 0 &&
                                strcmp(info.driver_name,
                                                IOAT_PMD_RAWDEV_NAME_STR) == 0);
        }

When calling the ``rte_rawdev_info_get()`` API for an IOAT rawdev device,
the ``dev_private`` field in the ``rte_rawdev_info`` struct should either
be NULL, or else be set to point to a structure of type
``rte_ioat_rawdev_config``, in which case the size of the configured device
input ring will be returned in that structure.

Device Configuration
~~~~~~~~~~~~~~~~~~~~~

Configuring an IOAT rawdev device is done using the
``rte_rawdev_configure()`` API, which takes the same structure parameters
as the, previously referenced, ``rte_rawdev_info_get()`` API. The main
difference is that, because the parameter is used as input rather than
output, the ``dev_private`` structure element cannot be NULL, and must
point to a valid ``rte_ioat_rawdev_config`` structure, containing the ring
size to be used by the device. The ring size must be a power of two,
between 64 and 4096.
If it is not needed, the tracking by the driver of user-provided completion
handles may be disabled by setting the ``hdls_disable`` flag in
the configuration structure also.

The following code shows how the device is configured in
``test_ioat_rawdev.c``:

.. code-block:: C

   #define IOAT_TEST_RINGSIZE 512
        struct rte_ioat_rawdev_config p = { .ring_size = -1 };
        struct rte_rawdev_info info = { .dev_private = &p };

        /* ... */

        p.ring_size = IOAT_TEST_RINGSIZE;
        if (rte_rawdev_configure(dev_id, &info, sizeof(p)) != 0) {
                printf("Error with rte_rawdev_configure()\n");
                return -1;
        }

Once configured, the device can then be made ready for use by calling the
``rte_rawdev_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~~

To perform data copies using IOAT rawdev devices, the functions
``rte_ioat_enqueue_copy()`` and ``rte_ioat_perform_ops()`` should be used.
Once copies have been completed, the completion will be reported back when
the application calls ``rte_ioat_completed_ops()``.

The ``rte_ioat_enqueue_copy()`` function enqueues a single copy to the
device ring for copying at a later point. The parameters to that function
include the IOVA addresses of both the source and destination buffers,
as well as two "handles" to be returned to the user when the copy is
completed. These handles can be arbitrary values, but two are provided so
that the library can track handles for both source and destination on
behalf of the user, e.g. virtual addresses for the buffers, or mbuf
pointers if packet data is being copied.

While the ``rte_ioat_enqueue_copy()`` function enqueues a copy operation on
the device ring, the copy will not actually be performed until after the
application calls the ``rte_ioat_perform_ops()`` function. This function
informs the device hardware of the elements enqueued on the ring, and the
device will begin to process them. It is expected that, for efficiency
reasons, a burst of operations will be enqueued to the device via multiple
enqueue calls between calls to the ``rte_ioat_perform_ops()`` function.

The following code from ``test_ioat_rawdev.c`` demonstrates how to enqueue
a burst of copies to the device and start the hardware processing of them:

.. code-block:: C

        struct rte_mbuf *srcs[32], *dsts[32];
        unsigned int j;

        for (i = 0; i < RTE_DIM(srcs); i++) {
                char *src_data;

                srcs[i] = rte_pktmbuf_alloc(pool);
                dsts[i] = rte_pktmbuf_alloc(pool);
                srcs[i]->data_len = srcs[i]->pkt_len = length;
                dsts[i]->data_len = dsts[i]->pkt_len = length;
                src_data = rte_pktmbuf_mtod(srcs[i], char *);

                for (j = 0; j < length; j++)
                        src_data[j] = rand() & 0xFF;

                if (rte_ioat_enqueue_copy(dev_id,
                                srcs[i]->buf_iova + srcs[i]->data_off,
                                dsts[i]->buf_iova + dsts[i]->data_off,
                                length,
                                (uintptr_t)srcs[i],
                                (uintptr_t)dsts[i]) != 1) {
                        printf("Error with rte_ioat_enqueue_copy for buffer %u\n",
                                        i);
                        return -1;
                }
        }
        rte_ioat_perform_ops(dev_id);

To retrieve information about completed copies, the API
``rte_ioat_completed_ops()`` should be used. This API will return to the
application a set of completion handles passed in when the relevant copies
were enqueued.

The following code from ``test_ioat_rawdev.c`` shows the test code
retrieving information about the completed copies and validating the data
is correct before freeing the data buffers using the returned handles:

.. code-block:: C

        if (rte_ioat_completed_ops(dev_id, 64, (void *)completed_src,
                        (void *)completed_dst) != RTE_DIM(srcs)) {
                printf("Error with rte_ioat_completed_ops\n");
                return -1;
        }
        for (i = 0; i < RTE_DIM(srcs); i++) {
                char *src_data, *dst_data;

                if (completed_src[i] != srcs[i]) {
                        printf("Error with source pointer %u\n", i);
                        return -1;
                }
                if (completed_dst[i] != dsts[i]) {
                        printf("Error with dest pointer %u\n", i);
                        return -1;
                }

                src_data = rte_pktmbuf_mtod(srcs[i], char *);
                dst_data = rte_pktmbuf_mtod(dsts[i], char *);
                for (j = 0; j < length; j++)
                        if (src_data[j] != dst_data[j]) {
                                printf("Error with copy of packet %u, byte %u\n",
                                                i, j);
                                return -1;
                        }
                rte_pktmbuf_free(srcs[i]);
                rte_pktmbuf_free(dsts[i]);
        }


Filling an Area of Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~

The IOAT driver also has support for the ``fill`` operation, where an area
of memory is overwritten, or filled, with a short pattern of data.
Fill operations can be performed in much the same was as copy operations
described above, just using the ``rte_ioat_enqueue_fill()`` function rather
than the ``rte_ioat_enqueue_copy()`` function.


Querying Device Statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The statistics from the IOAT rawdev device can be got via the xstats
functions in the ``rte_rawdev`` library, i.e.
``rte_rawdev_xstats_names_get()``, ``rte_rawdev_xstats_get()`` and
``rte_rawdev_xstats_by_name_get``. The statistics returned for each device
instance are:

* ``failed_enqueues``
* ``successful_enqueues``
* ``copies_started``
* ``copies_completed``
