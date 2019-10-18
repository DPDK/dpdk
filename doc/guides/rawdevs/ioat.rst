..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

.. include:: <isonum.txt>

IOAT Rawdev Driver for Intel\ |reg| QuickData Technology
======================================================================

The ``ioat`` rawdev driver provides a poll-mode driver (PMD) for Intel\ |reg|
QuickData Technology, part of Intel\ |reg| I/O Acceleration Technology
`(Intel I/OAT)
<https://www.intel.com/content/www/us/en/wireless-network/accel-technology.html>`_.
This PMD, when used on supported hardware, allows data copies, for example,
cloning packet data, to be accelerated by that hardware rather than having to
be done by software, freeing up CPU cycles for other tasks.

Hardware Requirements
----------------------

On Linux, the presence of an Intel\ |reg| QuickData Technology hardware can
be detected by checking the output of the ``lspci`` command, where the
hardware will be often listed as "Crystal Beach DMA" or "CBDMA". For
example, on a system with Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @2.20GHz,
lspci shows:

.. code-block:: console

  # lspci | grep DMA
  00:04.0 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 0 (rev 01)
  00:04.1 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 1 (rev 01)
  00:04.2 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 2 (rev 01)
  00:04.3 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 3 (rev 01)
  00:04.4 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 4 (rev 01)
  00:04.5 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 5 (rev 01)
  00:04.6 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 6 (rev 01)
  00:04.7 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 7 (rev 01)

On a system with Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz, lspci
shows:

.. code-block:: console

  # lspci | grep DMA
  00:04.0 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.1 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.2 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.3 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.4 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.5 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.6 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.7 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)


Compilation
------------

For builds done with ``make``, the driver compilation is enabled by the
``CONFIG_RTE_LIBRTE_PMD_IOAT_RAWDEV`` build configuration option. This is
enabled by default in builds for x86 platforms, and disabled in other
configurations.

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is x86-based.

Device Setup
-------------

The Intel\ |reg| QuickData Technology HW devices will need to be bound to a
user-space IO driver for use. The script ``dpdk-devbind.py`` script
included with DPDK can be used to view the state of the devices and to bind
them to a suitable DPDK-supported kernel driver. When querying the status
of the devices, they will appear under the category of "Misc (rawdev)
devices", i.e. the command ``dpdk-devbind.py --status-dev misc`` can be
used to see the state of those devices alone.

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once bound to a suitable kernel device driver, the HW devices will be found
as part of the PCI scan done at application initialization time. No vdev
parameters need to be passed to create or initialize the device.

Once probed successfully, the device will appear as a ``rawdev``, that is a
"raw device type" inside DPDK, and can be accessed using APIs from the
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
                found = (rte_rawdev_info_get(i, &info) == 0 &&
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

The following code shows how the device is configured in
``test_ioat_rawdev.c``:

.. code-block:: C

   #define IOAT_TEST_RINGSIZE 512
        struct rte_ioat_rawdev_config p = { .ring_size = -1 };
        struct rte_rawdev_info info = { .dev_private = &p };

        /* ... */

        p.ring_size = IOAT_TEST_RINGSIZE;
        if (rte_rawdev_configure(dev_id, &info) != 0) {
                printf("Error with rte_rawdev_configure()\n");
                return -1;
        }

Once configured, the device can then be made ready for use by calling the
``rte_rawdev_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~~

To perform data copies using IOAT rawdev devices, the functions
``rte_ioat_enqueue_copy()`` and ``rte_ioat_do_copies()`` should be used.
Once copies have been completed, the completion will be reported back when
the application calls ``rte_ioat_completed_copies()``.

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
application calls the ``rte_ioat_do_copies()`` function. This function
informs the device hardware of the elements enqueued on the ring, and the
device will begin to process them. It is expected that, for efficiency
reasons, a burst of operations will be enqueued to the device via multiple
enqueue calls between calls to the ``rte_ioat_do_copies()`` function.

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
                                (uintptr_t)dsts[i],
                                0 /* nofence */) != 1) {
                        printf("Error with rte_ioat_enqueue_copy for buffer %u\n",
                                        i);
                        return -1;
                }
        }
        rte_ioat_do_copies(dev_id);

To retrieve information about completed copies, the API
``rte_ioat_completed_copies()`` should be used. This API will return to the
application a set of completion handles passed in when the relevant copies
were enqueued.

The following code from ``test_ioat_rawdev.c`` shows the test code
retrieving information about the completed copies and validating the data
is correct before freeing the data buffers using the returned handles:

.. code-block:: C

        if (rte_ioat_completed_copies(dev_id, 64, (void *)completed_src,
                        (void *)completed_dst) != RTE_DIM(srcs)) {
                printf("Error with rte_ioat_completed_copies\n");
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
