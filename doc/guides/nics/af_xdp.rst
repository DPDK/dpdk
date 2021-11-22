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

Prerequisites
-------------

This is a Linux-specific PMD, thus the following prerequisites apply:

*  A Linux Kernel (version > v4.18) with XDP sockets configuration enabled;
*  libbpf (within kernel version > v5.1-rc4) with latest af_xdp support installed,
   User can install libbpf via `make install_lib` && `make install_headers` in
   <kernel src tree>/tools/lib/bpf;
*  A Kernel bound interface to attach to;
*  For need_wakeup feature, it requires kernel version later than v5.3-rc1;
*  For PMD zero copy, it requires kernel version later than v5.4-rc1;
*  For shared_umem, it requires kernel version v5.10 or later and libbpf version
   v0.2.0 or later.
*  For 32-bit OS, a kernel with version 5.4 or later is required.

Set up an af_xdp interface
-----------------------------

The following example will set up an af_xdp interface in DPDK:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1

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