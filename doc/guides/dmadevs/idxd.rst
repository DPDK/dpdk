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

Intel\ |reg| DSA devices can use the IDXD kernel driver or DPDK-supported drivers,
such as ``vfio-pci``. Both are supported by the IDXD PMD.

Intel\ |reg| DSA devices using IDXD kernel driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use an Intel\ |reg| DSA device bound to the IDXD kernel driver, the device must first be configured.
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
           --max-batch-size=512 --type=user --name=dpdk_app1

Once the devices have been configured, they need to be enabled::

        $ accel-config enable-device dsa0
        $ accel-config enable-wq dsa0/wq0.0

Check the device configuration::

        $ accel-config list

Every Intel\ |reg| DSA instance supports multiple queues and each should be similarly configured.
As a further example, the following set of commands will configure and enable 4 queues on instance 0,
giving each an equal share of resources::

        # configure 4 groups, each with one engine
        accel-config config-engine dsa0/engine0.0 --group-id=0
        accel-config config-engine dsa0/engine0.1 --group-id=1
        accel-config config-engine dsa0/engine0.2 --group-id=2
        accel-config config-engine dsa0/engine0.3 --group-id=3

        # configure 4 queues, putting each in a different group, so each
        # is backed by a single engine
        accel-config config-wq dsa0/wq0.0 --group-id=0 --type=user --wq-size=32 \
            --priority=10 --max-batch-size=1024 --mode=dedicated --name=dpdk_app1
        accel-config config-wq dsa0/wq0.1 --group-id=1 --type=user --wq-size=32 \
            --priority=10 --max-batch-size=1024 --mode=dedicated --name=dpdk_app1
        accel-config config-wq dsa0/wq0.2 --group-id=2 --type=user --wq-size=32 \
            --priority=10 --max-batch-size=1024 --mode=dedicated --name=dpdk_app1
        accel-config config-wq dsa0/wq0.3 --group-id=3 --type=user --wq-size=32 \
            --priority=10 --max-batch-size=1024 --mode=dedicated --name=dpdk_app1

        # enable device and queues
        accel-config enable-device dsa0
        accel-config enable-wq dsa0/wq0.0 dsa0/wq0.1 dsa0/wq0.2 dsa0/wq0.3


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

For devices bound to the IDXD kernel driver,
the DPDK IDXD driver will automatically perform a scan for available workqueues
to use. Any workqueues found listed in ``/dev/dsa`` on the system will be checked
in ``/sys``, and any which have ``dpdk_`` prefix in their name will be automatically
probed by the driver to make them available to the application.
Alternatively, to support use by multiple DPDK processes simultaneously,
the value used as the DPDK ``--file-prefix`` parameter may be used as a workqueue
name prefix, instead of ``dpdk_``, allowing each DPDK application instance to only
use a subset of configured queues.

Once probed successfully, irrespective of kernel driver, the device will appear as a ``dmadev``,
that is a "DMA device type" inside DPDK, and can be accessed using APIs from the
``rte_dmadev`` library.

Using IDXD DMAdev Devices
--------------------------

To use the devices from an application, the dmadev API can be used.

Device Configuration
~~~~~~~~~~~~~~~~~~~~~

IDXD configuration requirements:

* ``ring_size`` must be a power of two, between 64 and 4096.
* Only one ``vchan`` is supported per device (work queue).
* IDXD devices do not support silent mode.
* The transfer direction must be set to ``RTE_DMA_DIR_MEM_TO_MEM`` to copy from memory to memory.

Once configured, the device can then be made ready for use by calling the
``rte_dma_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~~

Refer to the :ref:`Enqueue / Dequeue APIs <dmadev_enqueue_dequeue>` section of the dmadev library
documentation for details on operation enqueue, submission and completion API usage.

It is expected that, for efficiency reasons, a burst of operations will be enqueued to the
device via multiple enqueue calls between calls to the ``rte_dma_submit()`` function.

When gathering completions, ``rte_dma_completed()`` should be used, up until the point an error
occurs in an operation. If an error was encountered, ``rte_dma_completed_status()`` must be used
to kick the device off to continue processing operations and also to gather the status of each
individual operations which is filled in to the ``status`` array provided as parameter by the
application.

The following status codes are supported by IDXD:

* ``RTE_DMA_STATUS_SUCCESSFUL``: The operation was successful.
* ``RTE_DMA_STATUS_INVALID_OPCODE``: The operation failed due to an invalid operation code.
* ``RTE_DMA_STATUS_INVALID_LENGTH``: The operation failed due to an invalid data length.
* ``RTE_DMA_STATUS_NOT_ATTEMPTED``: The operation was not attempted.
* ``RTE_DMA_STATUS_ERROR_UNKNOWN``: The operation failed due to an unspecified error.

The following code shows how to retrieve the number of successfully completed
copies within a burst and then using ``rte_dma_completed_status()`` to check
which operation failed and kick off the device to continue processing operations:

.. code-block:: C

   enum rte_dma_status_code status[COMP_BURST_SZ];
   uint16_t count, idx, status_count;
   bool error = 0;

   count = rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error);

   if (error){
      status_count = rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ, &idx, status);
   }
