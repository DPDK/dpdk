..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019-2020 Intel Corporation.

AF_XDP Poll Mode Driver
==========================

AF_XDP is an address family that is optimized for high performance
packet processing. AF_XDP sockets enable the possibility for XDP program to
redirect packets to a memory buffer in userspace.

For the full details behind AF_XDP socket, you can refer to
`AF_XDP documentation in the Kernel
<https://www.kernel.org/doc/Documentation/networking/af_xdp.rst>`_.

This Linux-specific PMD creates the AF_XDP socket and binds it to a
specific netdev queue, it allows a DPDK application to send and receive raw
packets through the socket which would bypass the kernel network stack.
Current implementation only supports single queue, multi-queues feature will
be added later.

AF_XDP PMD enables need_wakeup flag by default if it is supported. This
need_wakeup feature is used to support executing application and driver on the
same core efficiently. This feature not only has a large positive performance
impact for the one core case, but also does not degrade 2 core performance and
actually improves it for Tx heavy workloads.

Options
-------

The following options can be provided to set up an af_xdp port in DPDK.

*   ``iface`` - name of the Kernel interface to attach to (required);
*   ``start_queue`` - starting netdev queue id (optional, default 0);
*   ``queue_count`` - total netdev queue number (optional, default 1);
*   ``shared_umem`` - PMD will attempt to share UMEM with others (optional,
    default 0);
*   ``xdp_prog`` - path to custom xdp program (optional, default none);
*   ``busy_budget`` - busy polling budget (optional, default 64);

Prerequisites
-------------

This is a Linux-specific PMD, thus the following prerequisites apply:

*  A Linux Kernel (version > v4.18) with XDP sockets configuration enabled;
*  Both libxdp >=v1.2.2 and libbpf libraries installed, or, libbpf <=v0.6.0
*  If using libxdp, it requires an environment variable called
   LIBXDP_OBJECT_PATH to be set to the location of where libxdp placed its bpf
   object files. This is usually in /usr/local/lib/bpf or /usr/local/lib64/bpf.
*  A Kernel bound interface to attach to;
*  For need_wakeup feature, it requires kernel version later than v5.3-rc1;
*  For PMD zero copy, it requires kernel version later than v5.4-rc1;
*  For shared_umem, it requires kernel version v5.10 or later and libbpf version
   v0.2.0 or later.
*  For 32-bit OS, a kernel with version 5.4 or later is required.
*  For busy polling, kernel version v5.11 or later is required.

Set up an af_xdp interface
-----------------------------

The following example will set up an af_xdp interface in DPDK:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1

If 'start_queue' is not specified in the vdev arguments,
the socket will by default be created on Rx queue 0.
To ensure traffic lands on this queue,
one can use flow steering if the network card supports it.
Or, a simpler way is to reduce the number of configured queues
for the device which will ensure that all traffic will land on queue 0
and thus reach the socket:

.. code-block:: console

    ethtool -L ens786f1 combined 1

Limitations
-----------

- **MTU**

  The MTU of the AF_XDP PMD is limited due to the XDP requirement of one packet
  per page. In the PMD we report the maximum MTU for zero copy to be equal
  to the page size less the frame overhead introduced by AF_XDP (XDP HR = 256)
  and DPDK (frame headroom = 320). With a 4K page size this works out at 3520.
  However in practice this value may be even smaller, due to differences between
  the supported RX buffer sizes of the underlying kernel netdev driver.

  For example, the largest RX buffer size supported by the underlying kernel driver
  which is less than the page size (4096B) may be 3072B. In this case, the maximum
  MTU value will be at most 3072, but likely even smaller than this, once relevant
  headers are accounted for eg. Ethernet and VLAN.

  To determine the actual maximum MTU value of the interface you are using with the
  AF_XDP PMD, consult the documentation for the kernel driver.

  Note: The AF_XDP PMD will fail to initialise if an MTU which violates the driver's
  conditions as above is set prior to launching the application.

- **Shared UMEM**

  The sharing of UMEM is only supported for AF_XDP sockets with unique contexts.
  The context refers to the netdev,qid tuple.

  The following combination will fail:

  .. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,shared_umem=1 \
    --vdev net_af_xdp1,iface=ens786f1,shared_umem=1 \

  Either of the following however is permitted since either the netdev or qid differs
  between the two vdevs:

  .. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,shared_umem=1 \
    --vdev net_af_xdp1,iface=ens786f1,start_queue=1,shared_umem=1 \

  .. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,shared_umem=1 \
    --vdev net_af_xdp1,iface=ens786f2,shared_umem=1 \

- **Preferred Busy Polling**

  The SO_PREFER_BUSY_POLL socket option was introduced in kernel v5.11. It can
  deliver a performance improvement for sockets with heavy traffic loads and
  can significantly improve single-core performance in this context.

  The feature is enabled by default in the AF_XDP PMD. To disable it, set the
  'busy_budget' vdevarg to zero:

  .. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,busy_budget=0

  The default 'busy_budget' is 64 and it represents the number of packets the
  kernel will attempt to process in the netdev's NAPI context. You can change
  the value for example to 256 like so:

  .. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,busy_budget=256

  It is also strongly recommended to set the following for optimal performance:

  .. code-block:: console

    echo 2 | sudo tee /sys/class/net/ens786f1/napi_defer_hard_irqs
    echo 200000 | sudo tee /sys/class/net/ens786f1/gro_flush_timeout

  The above defers interrupts for interface ens786f1 and instead schedules its
  NAPI context from a watchdog timer instead of from softirqs. More information
  on this feature can be found at [1].

- **Secondary Processes**

  Rx and Tx are not supported for secondary processes due to memory mapping of
  the AF_XDP rings being assigned by the kernel in the primary process only.
  However other operations including statistics retrieval are permitted.
  The maximum number of queues permitted for PMDs operating in this model is 8
  as this is the maximum number of fds that can be sent through the IPC APIs as
  defined by RTE_MP_MAX_FD_NUM.

- **libxdp**

  When using the default program (ie. when the vdev arg 'xdp_prog' is not used),
  the following logs will appear when an application is launched:

  .. code-block:: console

    libbpf: elf: skipping unrecognized data section(7) .xdp_run_config
    libbpf: elf: skipping unrecognized data section(8) xdp_metadata

  These logs are not errors and can be ignored.

  [1] https://lwn.net/Articles/837010/
