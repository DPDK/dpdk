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
