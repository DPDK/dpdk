..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

.. _kni:

Kernel NIC Interface
====================

The DPDK Kernel NIC Interface (KNI) allows userspace applications access to the Linux* control plane.

The benefits of using the DPDK KNI are:

*   Faster than existing Linux TUN/TAP interfaces
    (by eliminating system calls and copy_to_user()/copy_from_user() operations.

*   Allows management of DPDK ports using standard Linux net tools such as ethtool, ifconfig and tcpdump.

*   Allows an interface with the kernel network stack.

The components of an application using the DPDK Kernel NIC Interface are shown in :numref:`figure_kernel_nic_intf`.

.. _figure_kernel_nic_intf:

.. figure:: img/kernel_nic_intf.*

   Components of a DPDK KNI Application


The DPDK KNI Kernel Module
--------------------------

The KNI kernel loadable module ``rte_kni`` provides the kernel interface
for DPDK applications.

When the ``rte_kni`` module is loaded, it will create a device ``/dev/kni``
that is used by the DPDK KNI API functions to control and communicate with
the kernel module.

The ``rte_kni`` kernel module contains several optional parameters which
can be specified when the module is loaded to control its behavior:

.. code-block:: console

    # modinfo rte_kni.ko
    <snip>
    parm:           lo_mode: KNI loopback mode (default=lo_mode_none):
                    lo_mode_none        Kernel loopback disabled
                    lo_mode_fifo        Enable kernel loopback with fifo
                    lo_mode_fifo_skb    Enable kernel loopback with fifo and skb buffer
                     (charp)
    parm:           kthread_mode: Kernel thread mode (default=single):
                    single    Single kernel thread mode enabled.
                    multiple  Multiple kernel thread mode enabled.
                     (charp)
    parm:           carrier: Default carrier state for KNI interface (default=off):
                    off   Interfaces will be created with carrier state set to off.
                    on    Interfaces will be created with carrier state set to on.
                     (charp)

Loading the ``rte_kni`` kernel module without any optional parameters is
the typical way a DPDK application gets packets into and out of the kernel
network stack.  Without any parameters, only one kernel thread is created
for all KNI devices for packet receiving in kernel side, loopback mode is
disabled, and the default carrier state of KNI interfaces is set to *off*.

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko

.. _kni_loopback_mode:

Loopback Mode
~~~~~~~~~~~~~

For testing, the ``rte_kni`` kernel module can be loaded in loopback mode
by specifying the ``lo_mode`` parameter:

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko lo_mode=lo_mode_fifo

The ``lo_mode_fifo`` loopback option will loop back ring enqueue/dequeue
operations in kernel space.

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko lo_mode=lo_mode_fifo_skb

The ``lo_mode_fifo_skb`` loopback option will loop back ring enqueue/dequeue
operations and sk buffer copies in kernel space.

If the ``lo_mode`` parameter is not specified, loopback mode is disabled.

.. _kni_kernel_thread_mode:

Kernel Thread Mode
~~~~~~~~~~~~~~~~~~

To provide flexibility of performance, the ``rte_kni`` KNI kernel module
can be loaded with the ``kthread_mode`` parameter.  The ``rte_kni`` kernel
module supports two options: "single kernel thread" mode and "multiple
kernel thread" mode.

Single kernel thread mode is enabled as follows:

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko kthread_mode=single

This mode will create only one kernel thread for all KNI interfaces to
receive data on the kernel side.  By default, this kernel thread is not
bound to any particular core, but the user can set the core affinity for
this kernel thread by setting the ``core_id`` and ``force_bind`` parameters
in ``struct rte_kni_conf`` when the first KNI interface is created:

For optimum performance, the kernel thread should be bound to a core in
on the same socket as the DPDK lcores used in the application.

The KNI kernel module can also be configured to start a separate kernel
thread for each KNI interface created by the DPDK application.  Multiple
kernel thread mode is enabled as follows:

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko kthread_mode=multiple

This mode will create a separate kernel thread for each KNI interface to
receive data on the kernel side.  The core affinity of each ``kni_thread``
kernel thread can be specified by setting the ``core_id`` and ``force_bind``
parameters in ``struct rte_kni_conf`` when each KNI interface is created.

Multiple kernel thread mode can provide scalable higher performance if
sufficient unused cores are available on the host system.

If the ``kthread_mode`` parameter is not specified, the "single kernel
thread" mode is used.

.. _kni_default_carrier_state:

Default Carrier State
~~~~~~~~~~~~~~~~~~~~~

The default carrier state of KNI interfaces created by the ``rte_kni``
kernel module is controlled via the ``carrier`` option when the module
is loaded.

If ``carrier=off`` is specified, the kernel module will leave the carrier
state of the interface *down* when the interface is management enabled.
The DPDK application can set the carrier state of the KNI interface using the
``rte_kni_update_link()`` function.  This is useful for DPDK applications
which require that the carrier state of the KNI interface reflect the
actual link state of the corresponding physical NIC port.

If ``carrier=on`` is specified, the kernel module will automatically set
the carrier state of the interface to *up* when the interface is management
enabled.  This is useful for DPDK applications which use the KNI interface as
a purely virtual interface that does not correspond to any physical hardware
and do not wish to explicitly set the carrier state of the interface with
``rte_kni_update_link()``.  It is also useful for testing in loopback mode
where the NIC port may not be physically connected to anything.

To set the default carrier state to *on*:

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko carrier=on

To set the default carrier state to *off*:

.. code-block:: console

    # insmod <build_dir>/kernel/linux/kni/rte_kni.ko carrier=off

If the ``carrier`` parameter is not specified, the default carrier state
of KNI interfaces will be set to *off*.

KNI Creation and Deletion
-------------------------

Before any KNI interfaces can be created, the ``rte_kni`` kernel module must
be loaded into the kernel and configured with the ``rte_kni_init()`` function.

The KNI interfaces are created by a DPDK application dynamically via the
``rte_kni_alloc()`` function.

The ``struct rte_kni_conf`` structure contains fields which allow the
user to specify the interface name, set the MTU size, set an explicit or
random MAC address and control the affinity of the kernel Rx thread(s)
(both single and multi-threaded modes).
By default the KNI sample example gets the MTU from the matching device,
and in case of KNI PMD it is derived from mbuf buffer length.

The ``struct rte_kni_ops`` structure contains pointers to functions to
handle requests from the ``rte_kni`` kernel module.  These functions
allow DPDK applications to perform actions when the KNI interfaces are
manipulated by control commands or functions external to the application.

For example, the DPDK application may wish to enabled/disable a physical
NIC port when a user enabled/disables a KNI interface with ``ip link set
[up|down] dev <ifaceX>``.  The DPDK application can register a callback for
``config_network_if`` which will be called when the interface management
state changes.

There are currently four callbacks for which the user can register
application functions:

``config_network_if``:

    Called when the management state of the KNI interface changes.
    For example, when the user runs ``ip link set [up|down] dev <ifaceX>``.

``change_mtu``:

    Called when the user changes the MTU size of the KNI
    interface.  For example, when the user runs ``ip link set mtu <size>
    dev <ifaceX>``.

``config_mac_address``:

    Called when the user changes the MAC address of the KNI interface.
    For example, when the user runs ``ip link set address <MAC>
    dev <ifaceX>``.  If the user sets this callback function to NULL,
    but sets the ``port_id`` field to a value other than -1, a default
    callback handler in the rte_kni library ``kni_config_mac_address()``
    will be called which calls ``rte_eth_dev_default_mac_addr_set()``
    on the specified ``port_id``.

``config_promiscusity``:

    Called when the user changes the promiscuity state of the KNI
    interface.  For example, when the user runs ``ip link set promisc
    [on|off] dev <ifaceX>``. If the user sets this callback function to
    NULL, but sets the ``port_id`` field to a value other than -1, a default
    callback handler in the rte_kni library ``kni_config_promiscusity()``
    will be called which calls ``rte_eth_promiscuous_<enable|disable>()``
    on the specified ``port_id``.

``config_allmulticast``:

    Called when the user changes the allmulticast state of the KNI interface.
    For example, when the user runs ``ifconfig <ifaceX> [-]allmulti``. If the
    user sets this callback function to NULL, but sets the ``port_id`` field to
    a value other than -1, a default callback handler in the rte_kni library
    ``kni_config_allmulticast()`` will be called which calls
    ``rte_eth_allmulticast_<enable|disable>()`` on the specified ``port_id``.

In order to run these callbacks, the application must periodically call
the ``rte_kni_handle_request()`` function.  Any user callback function
registered will be called directly from ``rte_kni_handle_request()`` so
care must be taken to prevent deadlock and to not block any DPDK fastpath
tasks.  Typically DPDK applications which use these callbacks will need
to create a separate thread or secondary process to periodically call
``rte_kni_handle_request()``.

The KNI interfaces can be deleted by a DPDK application with
``rte_kni_release()``.  All KNI interfaces not explicitly deleted will be
deleted when the ``/dev/kni`` device is closed, either explicitly with
``rte_kni_close()`` or when the DPDK application is closed.

DPDK mbuf Flow
--------------

To minimize the amount of DPDK code running in kernel space, the mbuf mempool is managed in userspace only.
The kernel module will be aware of mbufs,
but all mbuf allocation and free operations will be handled by the DPDK application only.

:numref:`figure_pkt_flow_kni` shows a typical scenario with packets sent in both directions.

.. _figure_pkt_flow_kni:

.. figure:: img/pkt_flow_kni.*

   Packet Flow via mbufs in the DPDK KNI


Use Case: Ingress
-----------------

On the DPDK RX side, the mbuf is allocated by the PMD in the RX thread context.
This thread will enqueue the mbuf in the rx_q FIFO,
and the next pointers in mbuf-chain will convert to physical address.
The KNI thread will poll all KNI active devices for the rx_q.
If an mbuf is dequeued, it will be converted to a sk_buff and sent to the net stack via netif_rx().
The dequeued mbuf must be freed, so the same pointer is sent back in the free_q FIFO,
and next pointers must convert back to virtual address if exists before put in the free_q FIFO.

The RX thread, in the same main loop, polls this FIFO and frees the mbuf after dequeuing it.
The address conversion of the next pointer is to prevent the chained mbuf
in different hugepage segments from causing kernel crash.

Use Case: Egress
----------------

For packet egress the DPDK application must first enqueue several mbufs to create an mbuf cache on the kernel side.

The packet is received from the Linux net stack, by calling the kni_net_tx() callback.
The mbuf is dequeued (without waiting due the cache) and filled with data from sk_buff.
The sk_buff is then freed and the mbuf sent in the tx_q FIFO.

The DPDK TX thread dequeues the mbuf and sends it to the PMD via ``rte_eth_tx_burst()``.
It then puts the mbuf back in the cache.

IOVA = VA: Support
------------------

KNI operates in IOVA_VA scheme when

- LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0) and
- EAL option `iova-mode=va` is passed or bus IOVA scheme in the DPDK is selected
  as RTE_IOVA_VA.

Due to IOVA to KVA address translations, based on the KNI use case there
can be a performance impact. For mitigation, forcing IOVA to PA via EAL
"--iova-mode=pa" option can be used, IOVA_DC bus iommu scheme can also
result in IOVA as PA.

Ethtool
-------

Ethtool is a Linux-specific tool with corresponding support in the kernel.
The current version of kni provides minimal ethtool functionality
including querying version and link state. It does not support link
control, statistics, or dumping device registers.
