..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

.. include:: <isonum.txt>

IDXD DMA Device Driver
======================

The ``idxd`` dmadev driver provides a poll-mode driver (PMD) for Intel\ |reg|
Data Streaming Accelerator `(Intel DSA)
<https://software.intel.com/content/www/us/en/develop/articles/intel-data-streaming-accelerator-architecture-specification.html>`_.
This PMD can be used in conjunction with Intel\ |reg| DSA devices to offload
data operations, such as data copies, to hardware, freeing up CPU cycles for
other tasks.

Hardware Requirements
----------------------

The ``dpdk-devbind.py`` script, included with DPDK, can be used to show the
presence of supported hardware. Running ``dpdk-devbind.py --status-dev dma``
will show all the DMA devices on the system, including IDXD supported devices.
Intel\ |reg| DSA devices, are currently (at time of writing) appearing
as devices with type “0b25”, due to the absence of pci-id database entries for
them at this point.

Compilation
------------

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is x86-based. No additional compilation steps are necessary.

Device Setup
-------------

Devices using VFIO/UIO drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The HW devices to be used will need to be bound to a user-space IO driver for use.
The ``dpdk-devbind.py`` script can be used to view the state of the devices
and to bind them to a suitable DPDK-supported driver, such as ``vfio-pci``.
For example::

	$ dpdk-devbind.py -b vfio-pci 6a:01.0

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
