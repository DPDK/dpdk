..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Marvell.

Marvell CNXK BPHY Driver
========================

CN10K/CN9K Fusion product families offer an internal BPHY unit which provides
set of hardware accelerators for performing baseband related operations.
Connectivity to the outside world happens through a block called RFOE which is
backed by ethernet I/O block called CGX or RPM (depending on the chip version).
RFOE stands for Radio Frequency Over Ethernet and provides support for
IEEE 1904.3 (RoE) standard.

Features
--------

The BPHY CGX/RPM implements following features in the rawdev API:

- Access to BPHY CGX/RPM via a set of predefined messages
- Access to BPHY memory
- Custom interrupt handlers

Device Setup
------------

The BPHY CGX/RPM devices will need to be bound to a user-space IO driver for
use. The script ``dpdk-devbind.py`` script included with DPDK can be used to
view the state of the devices and to bind them to a suitable DPDK-supported
kernel driver. When querying the status of the devices, they will appear under
the category of "Misc (rawdev) devices", i.e. the command
``dpdk-devbind.py --status-dev misc`` can be used to see the state of those
devices alone.

Before performing actual data transfer one needs to first retrieve number of
available queues with ``rte_rawdev_queue_count()`` and capacity of each
using ``rte_rawdev_queue_conf_get()``.

To perform data transfer use standard ``rte_rawdev_enqueue_buffers()`` and
``rte_rawdev_dequeue_buffers()`` APIs. Not all messages produce sensible
responses hence dequeuing is not always necessary.

BPHY CGX/RPM PMD
----------------

BPHY CGX/RPM PMD accepts ``struct cnxk_bphy_cgx_msg`` messages which differ by type and payload.
Message types along with description are listed below. As for the usage examples please refer to
``cnxk_bphy_cgx_dev_selftest()``.

Get link information
~~~~~~~~~~~~~~~~~~~~

Message is used to get information about link state.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO``. In response one will
get message containing payload i.e ``struct cnxk_bphy_cgx_msg_link_info`` filled with information
about current link state.

Change internal loopback state
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Message is used to enable or disable internal loopback.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE`` or
``CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE``. Former will activate internal loopback while the latter
will do the opposite.

Change PTP RX state
~~~~~~~~~~~~~~~~~~~

Message is used to enable or disable PTP mode.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE`` or
``CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE``. Former will enable PTP while the latter will do the
opposite.

Set link mode
~~~~~~~~~~~~~

Message is used to change link mode.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_MODE``. Prior to sending actual
message payload i.e ``struct cnxk_bphy_cgx_msg_link_mode`` needs to be filled with relevant
information.

Change link state
~~~~~~~~~~~~~~~~~

Message is used to set link up or down.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE``. Prior to sending actual
message payload i.e ``struct cnxk_bphy_cgx_msg_set_link_state`` needs to be filled with relevant
information.

Start or stop RX/TX
~~~~~~~~~~~~~~~~~~~

Message is used to start or stop accepting traffic.

Message must have type set to ``CNXK_BPHY_CGX_MSG_TYPE_START_RXTX`` or
``CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX``. Former will enable traffic while the latter will
do the opposite.

BPHY PMD
--------

BPHY PMD accepts ``struct cnxk_bphy_irq_msg`` messages which differ by type and payload.
Message types along with description are listed below. For some usage examples please refer to
``bphy_rawdev_selftest()``.

Initialize or finalize interrupt handling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Message is used to setup low level interrupt handling.

Message must have type set to ``CNXK_BPHY_IRQ_MSG_TYPE_INIT`` or ``CNXK_BPHY_IRQ_MSG_TYPE_FINI``.
The former will setup low level interrupt handling while the latter will tear everything down. There
are also two convenience functions namely ``rte_pmd_bphy_intr_init()`` and
``rte_pmd_bphy_intr_fini()`` that take care of all details.


Register or remove interrupt handler
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Message is used setup custom interrupt handler.

Message must have type set to ``CNXK_BPHY_IRQ_MSG_TYPE_REGISTER`` or
``CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER``. The former will register an interrupt handler while the
latter will remove it. Prior sending actual message payload i.e ``struct cnxk_bphy_irq_info`` needs
to be filled with relevant information. There are also two convenience functions namely
``rte_pmd_bphy_intr_register()`` and ``rte_pmd_bphy_intr_unregister()`` that take care of all
details.

Get device memory
~~~~~~~~~~~~~~~~~

Message is used to read device MMIO address.

Message must have type set to ``CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET``. There's a convenience function
``rte_pmd_bphy_intr_mem_get()`` available that takes care of retrieving that address.

Self test
---------

On EAL initialization BPHY and BPHY CGX/RPM devices will be probed and populated into
the raw devices. The rawdev ID of the device can be obtained using invocation
of ``rte_rawdev_get_dev_id("NAME:x")`` from the test application, where:

- NAME is the desired subsystem: use "BPHY" for regular, and "BPHY_CGX" for
  RFOE module.
- x is the device's bus id specified in "bus:device.func" (BDF) format. BDF follows convention
  used by lspci i.e bus, device and func are specified using respectively two, two and one hex
  digit(s).

Use this identifier for further rawdev function calls.

Selftest rawdev API can be used to verify the BPHY and BPHY CGX/RPM functionality.
