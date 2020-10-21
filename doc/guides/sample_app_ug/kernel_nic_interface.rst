..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Kernel NIC Interface Sample Application
=======================================

The Kernel NIC Interface (KNI) is a DPDK control plane solution that
allows userspace applications to exchange packets with the kernel networking stack.
To accomplish this, DPDK userspace applications use an IOCTL call
to request the creation of a KNI virtual device in the Linux* kernel.
The IOCTL call provides interface information and the DPDK's physical address space,
which is re-mapped into the kernel address space by the KNI kernel loadable module
that saves the information to a virtual device context.
The DPDK creates FIFO queues for packet ingress and egress
to the kernel module for each device allocated.

The KNI kernel loadable module is a standard net driver,
which upon receiving the IOCTL call access the DPDK's FIFO queue to
receive/transmit packets from/to the DPDK userspace application.
The FIFO queues contain pointers to data packets in the DPDK. This:

*   Provides a faster mechanism to interface with the kernel net stack and eliminates system calls

*   Facilitates the DPDK using standard Linux* userspace net tools (tshark, rsync, and so on)

*   Eliminate the copy_to_user and copy_from_user operations on packets.

The Kernel NIC Interface sample application is a simple example that demonstrates the use
of the DPDK to create a path for packets to go through the Linux* kernel.
This is done by creating one or more kernel net devices for each of the DPDK ports.
The application allows the use of standard Linux tools (ethtool, iproute, tshark) with the DPDK ports and
also the exchange of packets between the DPDK application and the Linux* kernel.

The Kernel NIC Interface sample application requires that the
KNI kernel module ``rte_kni`` be loaded into the kernel.  See
:doc:`../prog_guide/kernel_nic_interface` for more information on loading
the ``rte_kni`` kernel module.

Overview
--------

The Kernel NIC Interface sample application ``kni`` allocates one or more
KNI interfaces for each physical NIC port.  For each physical NIC port,
``kni`` uses two DPDK threads in user space; one thread reads from the port and
writes to the corresponding KNI interfaces and the other thread reads from
the KNI interfaces and writes the data unmodified to the physical NIC port.

It is recommended to configure one KNI interface for each physical NIC port.
The application can be configured with more than one KNI interface for
each physical NIC port for performance testing or it can work together with
VMDq support in future.

The packet flow through the Kernel NIC Interface application is as shown
in the following figure.

.. _figure_kernel_nic:

.. figure:: img/kernel_nic.*

   Kernel NIC Application Packet Flow

If link monitoring is enabled with the ``-m`` command line flag, one
additional pthread is launched which will check the link status of each
physical NIC port and will update the carrier status of the corresponding
KNI interface(s) to match the physical NIC port's state.  This means that
the KNI interface(s) will be disabled automatically when the Ethernet link
goes down and enabled when the Ethernet link goes up.

If link monitoring is enabled, the ``rte_kni`` kernel module should be loaded
such that the :ref:`default carrier state <kni_default_carrier_state>` is
set to *off*.  This ensures that the KNI interface is only enabled *after*
the Ethernet link of the corresponding NIC port has reached the linkup state.

If link monitoring is not enabled, the ``rte_kni`` kernel module should be
loaded with the :ref:`default carrier state <kni_default_carrier_state>`
set to *on*.  This sets the carrier state of the KNI interfaces to *on*
when the KNI interfaces are enabled without regard to the actual link state
of the corresponding NIC port.  This is useful for testing in loopback
mode where the NIC port may not be physically connected to anything.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``examples/kni`` sub-directory.

.. note::

        This application is intended as a linux only.

Running the kni Example Application
-----------------------------------

The ``kni`` example application requires a number of command line options:

.. code-block:: console

    dpdk-kni [EAL options] -- -p PORTMASK --config="(port,lcore_rx,lcore_tx[,lcore_kthread,...])[,(port,lcore_rx,lcore_tx[,lcore_kthread,...])]" [-P] [-m]

Where:

*   ``-p PORTMASK``:

    Hexadecimal bitmask of ports to configure.

*   ``--config="(port,lcore_rx,lcore_tx[,lcore_kthread,...])[,(port,lcore_rx,lcore_tx[,lcore_kthread,...])]"``:

    Determines which lcores the Rx and Tx DPDK tasks, and (optionally)
    the KNI kernel thread(s) are bound to for each physical port.

*   ``-P``:

    Optional flag to set all ports to promiscuous mode so that packets are
    accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination
    address set to the Ethernet address of the port are accepted.

*   ``-m``:

    Optional flag to enable monitoring and updating of the Ethernet
    carrier state.  With this option set, a thread will be started which
    will periodically check the Ethernet link status of the physical
    Ethernet ports and set the carrier state of the corresponding KNI
    network interface to match it.  This means that the KNI interface will
    be disabled automatically when the Ethernet link goes down and enabled
    when the Ethernet link goes up.

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.

The ``-c coremask`` or ``-l corelist`` parameter of the EAL options must
include the lcores specified by ``lcore_rx`` and ``lcore_tx`` for each port,
but does not need to include lcores specified by ``lcore_kthread`` as those
cores are used to pin the kernel threads in the ``rte_kni`` kernel module.

The ``--config`` parameter must include a set of
``(port,lcore_rx,lcore_tx,[lcore_kthread,...])`` values for each physical
port specified in the ``-p PORTMASK`` parameter.

The optional ``lcore_kthread`` lcore ID parameter in ``--config`` can be
specified zero, one or more times for each physical port.

If no lcore ID is specified for ``lcore_kthread``, one KNI interface will
be created for the physical port ``port`` and the KNI kernel thread(s)
will have no specific core affinity.

If one or more lcore IDs are specified for ``lcore_kthread``, a KNI interface
will be created for each lcore ID specified, bound to the physical port
``port``.  If the ``rte_kni`` kernel module is loaded in :ref:`multiple
kernel thread <kni_kernel_thread_mode>` mode, a kernel thread will be created
for each KNI interface and bound to the specified core.  If the ``rte_kni``
kernel module is loaded in :ref:`single kernel thread <kni_kernel_thread_mode>`
mode, only one kernel thread is started for all KNI interfaces.  The kernel
thread will be bound to the first ``lcore_kthread`` lcore ID specified.

Example Configurations
~~~~~~~~~~~~~~~~~~~~~~~

The following commands will first load the ``rte_kni`` kernel module in
:ref:`multiple kernel thread <kni_kernel_thread_mode>` mode.  The ``kni``
application is then started using two ports;  Port 0 uses lcore 4 for the
Rx task, lcore 6 for the Tx task, and will create a single KNI interface
``vEth0_0`` with the kernel thread bound to lcore 8.  Port 1 uses lcore
5 for the Rx task, lcore 7 for the Tx task, and will create a single KNI
interface ``vEth1_0`` with the kernel thread bound to lcore 9.

.. code-block:: console

    # rmmod rte_kni
    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko kthread_mode=multiple
    # ./<build-dir>/examples/dpdk-kni -l 4-7 -n 4 -- -P -p 0x3 -m --config="(0,4,6,8),(1,5,7,9)"

The following example is identical, except an additional ``lcore_kthread``
core is specified per physical port.  In this case, ``kni`` will create
four KNI interfaces: ``vEth0_0``/``vEth0_1`` bound to physical port 0 and
``vEth1_0``/``vEth1_1`` bound to physical port 1.

The kernel thread for each interface will be bound as follows:

    * ``vEth0_0`` - bound to lcore 8.
    * ``vEth0_1`` - bound to lcore 10.
    * ``vEth1_0`` - bound to lcore 9.
    * ``vEth1_1`` - bound to lcore 11

.. code-block:: console

    # rmmod rte_kni
    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko kthread_mode=multiple
    # ./<build-dir>/examples/dpdk-kni -l 4-7 -n 4 -- -P -p 0x3 -m --config="(0,4,6,8,10),(1,5,7,9,11)"

The following example can be used to test the interface between the ``kni``
test application and the ``rte_kni`` kernel module.  In this example,
the ``rte_kni`` kernel module is loaded in :ref:`single kernel thread
mode <kni_kernel_thread_mode>`, :ref:`loopback mode <kni_loopback_mode>`
enabled, and the :ref:`default carrier state <kni_default_carrier_state>`
is set to *on* so that the corresponding physical NIC port does not have
to be connected in order to use the KNI interface.  One KNI interface
``vEth0_0`` is created for port 0 and one KNI interface ``vEth1_0`` is
created for port 1.  Since ``rte_kni`` is loaded in "single kernel thread"
mode, the one kernel thread is bound to lcore 8.

Since the physical NIC ports are not being used, link monitoring can be
disabled by **not** specifying the ``-m`` flag to ``kni``:

.. code-block:: console

    # rmmod rte_kni
    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko lo_mode=lo_mode_fifo carrier=on
    # ./<build-dir>/examples/dpdk-kni -l 4-7 -n 4 -- -P -p 0x3 --config="(0,4,6,8),(1,5,7,9)"

KNI Operations
--------------

Once the ``kni`` application is started, the user can use the normal
Linux commands to manage the KNI interfaces as if they were any other
Linux network interface.

Enable KNI interface and assign an IP address:

.. code-block:: console

    # ip addr add dev vEth0_0 192.168.0.1

Show KNI interface configuration and statistics:

.. code-block:: console

    # ip -s -d addr show vEth0_0

The user can also check and reset the packet statistics inside the ``kni``
application by sending the app the USR1 and USR2 signals:

.. code-block:: console

    # Print statistics
    # pkill -USR1 kni

    # Zero statistics
    # pkill -USR2 kni

Dump network traffic:

.. code-block:: console

    # tshark -n -i vEth0_0

The normal Linux commands can also be used to change the MAC address and
MTU size used by the physical NIC which corresponds to the KNI interface.
However, if more than one KNI interface is configured for a physical port,
these commands will only work on the first KNI interface for that port.

Change the MAC address:

.. code-block:: console

    # ip link set dev vEth0_0 lladdr 0C:01:02:03:04:08

Change the MTU size:

.. code-block:: console

    # ip link set dev vEth0_0 mtu 1450

Limited ethtool support:

.. code-block:: console

    # ethtool -i vEth0_0

When the ``kni`` application is closed, all the KNI interfaces are deleted
from the Linux kernel.

Explanation
-----------

The following sections provide some explanation of code.

Initialization
~~~~~~~~~~~~~~

Setup of mbuf pool, driver and queues is similar to the setup done in the :doc:`l2_forward_real_virtual`..
In addition, one or more kernel NIC interfaces are allocated for each
of the configured ports according to the command line parameters.

The code for allocating the kernel NIC interfaces for a specific port is
in the function ``kni_alloc``.

The other step in the initialization process that is unique to this sample application
is the association of each port with lcores for RX, TX and kernel threads.

*   One lcore to read from the port and write to the associated one or more KNI devices

*   Another lcore to read from one or more KNI devices and write to the port

*   Other lcores for pinning the kernel threads on one by one

This is done by using the ``kni_port_params_array[]`` array, which is indexed by the port ID.
The code is in the function ``parse_config``.

Packet Forwarding
~~~~~~~~~~~~~~~~~

After the initialization steps are completed, the main_loop() function is run on each lcore.
This function first checks the lcore_id against the user provided lcore_rx and lcore_tx
to see if this lcore is reading from or writing to kernel NIC interfaces.

For the case that reads from a NIC port and writes to the kernel NIC interfaces (``kni_ingress``),
the packet reception is the same as in L2 Forwarding sample application
(see :ref:`l2_fwd_app_rx_tx_packets`).
The packet transmission is done by sending mbufs into the kernel NIC interfaces by ``rte_kni_tx_burst()``.
The KNI library automatically frees the mbufs after the kernel successfully copied the mbufs.

For the other case that reads from kernel NIC interfaces
and writes to a physical NIC port (``kni_egress``),
packets are retrieved by reading mbufs from kernel NIC interfaces by ``rte_kni_rx_burst()``.
The packet transmission is the same as in the L2 Forwarding sample application
(see :ref:`l2_fwd_app_rx_tx_packets`).
