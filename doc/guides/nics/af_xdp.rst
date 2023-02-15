..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019-2020 Intel Corporation.

AF_XDP Poll Mode Driver
==========================

AF_XDP is an address family that is optimized for high performance
packet processing. AF_XDP sockets enable the possibility for an XDP program to
redirect packets to a memory buffer in userspace.

Further information about AF_XDP can be found in the
`AF_XDP kernel documentation
<https://www.kernel.org/doc/Documentation/networking/af_xdp.rst>`_.

This Linux-specific PMD creates the AF_XDP socket and binds it to a
specific netdev queue. The DPDK application can then send and receive raw
packets through the socket which bypass the kernel network stack.

Prerequisites
-------------

*  A Linux Kernel (version >= v4.18) with the XDP sockets configuration option
   enabled (CONFIG_XDP_SOCKETS=y).
*  Both libxdp (>= v1.2.2) and libbpf (any version) libraries installed, or
   alternatively just the libbpf library <= v0.6.0.
*  The pkg-config package should be installed on the system as it is used to
   discover the libbpf and libxdp libraries and determine their versions are
   sufficient.
*  If using libxdp, it requires an environment variable called
   LIBXDP_OBJECT_PATH to be set to the location of where libxdp placed its bpf
   object files. This is usually in /usr/local/lib/bpf or /usr/local/lib64/bpf.
*  A Kernel bound interface to attach to.
*  The need_wakeup feature requires kernel version >= v5.4.
*  The PMD zero copy feature requires kernel version >= v5.4.
*  The shared UMEM feature requires kernel version >= v5.10 and libbpf version
   v0.2.0 or later. The LINUX_VERSION_CODE defined in the version.h kernel
   header is used to determine the kernel version at compile time.
*  A kernel with version 5.4 or later is required for 32-bit OS.
*  The busy polling feature requires kernel version >= v5.11.


Options
-------

iface
~~~~~

The ``iface`` option is the only required option. It is the name of the Kernel
interface to attach to.

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1

The socket will by default be created on Rx queue 0. To ensure traffic lands on
this queue, one can use flow steering if the network card supports it. Or, a
simpler way is to reduce the number of configured queues for the device to just
a single queue which will ensure that all traffic will land on that queue (queue
1) and thus reach the socket. This can be configured using ethtool:

.. code-block:: console

    ethtool -L ens786f1 combined 1

start_queue
~~~~~~~~~~~

To create a socket on another queue, first configure the netdev with multiple
queues, for example 2, like so:

.. code-block:: console

    ethtool -L ens786f1 combined 2

Then, create the socket on one of those queues, for example queue 1:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,start_queue=1

queue_count
~~~~~~~~~~~

To create a PMD with sockets on multiple queues, use the queue_count arg. The
following example creates sockets on queues 0 and 1:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,queue_count=2

shared_umem
~~~~~~~~~~~

The shared UMEM feature allows for two sockets to share UMEM and can be
configured like so:

.. code-block:: console

    --vdev net_af_xdp0,iface=ens786f1,shared_umem=1 \
    --vdev net_af_xdp1,iface=ens786f2,shared_umem=1

xdp_prog
~~~~~~~~

The xdp_prog argument allows for the user to provide a path to a custom XDP
program which should be used in place of the default libbpf/libxdp program which
simply redirects packets to the sockets. For example:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,xdp_prog=/path/to/prog.o

busy_budget
~~~~~~~~~~~

The busy polling feature aims to improve single-core performance for AF_XDP
sockets under heavy load. It is enabled by default if the detected kernel
version is sufficient ie. >= v5.11. The busy_budget arg sets the busy-polling
NAPI budget which is the number of packets the kernel will attempt to process in
the netdev's NAPI context. It can be configured like so:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,busy_budget=32

To disable busy polling, simply set the busy_budget to 0:

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,busy_budget=0

It is also strongly recommended to set the following for optimal performance
when using the busy polling feature:

.. code-block:: console

    echo 2 | sudo tee /sys/class/net/ens786f1/napi_defer_hard_irqs
    echo 200000 | sudo tee /sys/class/net/ens786f1/gro_flush_timeout

The above defers interrupts for interface ens786f1 and instead schedules its
NAPI context from a watchdog timer instead of from softirqs. More information
on this feature can be found at [1].

force_copy
~~~~~~~~~~

The force_copy argument allows the user to force the socket to use copy mode
instead of zero copy mode (if available).

.. code-block:: console

    --vdev net_af_xdp,iface=ens786f1,force_copy=1

use_cni
~~~~~~~

The EAL vdev argument ``use_cni`` is used to indicate that the user wishes to
enable the `AF_XDP Plugin for Kubernetes`_ within a DPDK application.

.. _AF_XDP Plugin for Kubernetes: https://github.com/intel/afxdp-plugins-for-kubernetes

.. code-block:: console

   --vdev=net_af_xdp0,use_cni=1

.. note::

   When using `use_cni`_, both parameters `xdp_prog`_ and `busy_budget`_ are disabled
   as both of these will be handled by the AF_XDP plugin.
   Since the DPDK application is running in limited privileges
   so enabling and disabling of the promiscuous mode through the DPDK application
   is also not supported.

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
