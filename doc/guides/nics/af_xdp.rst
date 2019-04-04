..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

AF_XDP Poll Mode Driver
==========================

AF_XDP is an address family that is optimized for high performance
packet processing. AF_XDP sockets enable the possibility for XDP program to
redirect packets to a memory buffer in userspace.

For the full details behind AF_XDP socket, you can refer to
`AF_XDP documentation in the Kernel
<https://www.kernel.org/doc/Documentation/networking/af_xdp.rst>`_.

This Linux-specific PMD driver creates the AF_XDP socket and binds it to a
specific netdev queue, it allows a DPDK application to send and receive raw
packets through the socket which would bypass the kernel network stack.
Current implementation only supports single queue, multi-queues feature will
be added later.

Note that MTU of AF_XDP PMD is limited due to XDP lacks support for
fragmentation.

Options
-------

The following options can be provided to set up an af_xdp port in DPDK.

*   ``iface`` - name of the Kernel interface to attach to (required);
*   ``queue`` - netdev queue id (optional, default 0);

Prerequisites
-------------

This is a Linux-specific PMD, thus the following prerequisites apply:

*  A Linux Kernel (version > v4.18) with XDP sockets configuration enabled;
*  libbpf (within kernel version > v5.1-rc4) with latest af_xdp support installed,
   User can install libbpf via `make install_lib` && `make install_headers` in
   <kernel src tree>/tools/lib/bpf;
*  A Kernel bound interface to attach to;

Set up an af_xdp interface
-----------------------------

The following example will set up an af_xdp interface in DPDK:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,queue=0
