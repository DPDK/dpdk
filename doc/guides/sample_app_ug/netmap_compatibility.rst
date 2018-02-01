..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Netmap Compatibility Sample Application
=======================================

Introduction
------------

The Netmap compatibility library provides a minimal set of APIs to give programs written against the Netmap APIs
the ability to be run, with minimal changes to their source code, using the DPDK to perform the actual packet I/O.

Since Netmap applications use regular system calls, like ``open()``, ``ioctl()`` and
``mmap()`` to communicate with the Netmap kernel module performing the packet I/O,
the ``compat_netmap`` library provides a set of similar APIs to use in place of those system calls,
effectively turning a Netmap application into a DPDK application.

The provided library is currently minimal and doesn't support all the features that Netmap supports,
but is enough to run simple applications, such as the bridge example detailed below.

Knowledge of Netmap is required to understand the rest of this section.
Please refer to the Netmap distribution for details about Netmap.

Available APIs
--------------

The library provides the following drop-in replacements for system calls usually used in Netmap applications:

* ``rte_netmap_close()``

* ``rte_netmap_ioctl()``

* ``rte_netmap_open()``

* ``rte_netmap_mmap()``

* ``rte_netmap_poll()``

They use the same signature as their libc counterparts, and can be used as drop-in replacements in most cases.

Caveats
-------

Given the difference between the way Netmap and the DPDK approach packet I/O,
there are caveats and limitations to be aware of when trying to use the ``compat_netmap`` library, the most important of these are listed below.
These may change as the library is updated:

*   Any system call that can potentially affect file descriptors cannot be used with a descriptor returned by the ``rte_netmap_open()`` function.

Note that:

*   The ``rte_netmap_mmap()`` function merely returns the address of a DPDK memzone.
    The address, length, flags, offset, and other arguments are ignored.

*   The ``rte_netmap_poll()`` function only supports infinite (negative) or zero time outs.
    It effectively turns calls to the ``poll()`` system call made in a Netmap application into polling of the DPDK ports,
    changing the semantics of the usual POSIX defined poll.

*   Not all of Netmap's features are supported: host rings,
    slot flags and so on are not supported or are simply not relevant in the DPDK model.

*   The Netmap manual page states that "*a device obtained through /dev/netmap also supports the ioctl supported by network devices*".
    This is not the case with this compatibility layer.

*   The Netmap kernel module exposes a sysfs interface to change some internal parameters, such as the size of the shared memory region.
    This interface is not available when using this compatibility layer.

Porting Netmap Applications
---------------------------

Porting Netmap applications typically involves two major steps:

*   Changing the system calls to use their ``compat_netmap`` library counterparts.

*   Adding further DPDK initialization code.

Since the ``compat_netmap`` functions have the same signature as the usual libc calls, the change is trivial in most cases.

The usual DPDK initialization code involving ``rte_eal_init()`` and ``rte_pci_probe()``
has to be added to the Netmap application in the same way it is used in all other DPDK sample applications.
Please refer to the *DPDK Programmer's Guide* and example source code for details about initialization.

In addition of the regular DPDK initialization code,
the ported application needs to call initialization functions for the ``compat_netmap`` library,
namely ``rte_netmap_init()`` and ``rte_netmap_init_port()``.

These two initialization functions take ``compat_netmap`` specific data structures as parameters:
``struct rte_netmap_conf`` and ``struct rte_netmap_port_conf``.
The structures' fields are Netmap related and are self-explanatory for developers familiar with Netmap.
They are defined in ``$RTE_SDK/examples/netmap_compat/lib/compat_netmap.h``.

The bridge application is an example largely based on the bridge example shipped with the Netmap distribution.
It shows how a minimal Netmap application with minimal and straightforward source code changes can be run on top of the DPDK.
Please refer to ``$RTE_SDK/examples/netmap_compat/bridge/bridge.c`` for an example of a ported application.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``netmap_compat`` sub-directory.

Running the "bridge" Sample Application
---------------------------------------

The application requires a single command line option:

.. code-block:: console

    ./build/bridge [EAL options] -- -i INTERFACE_A [-i INTERFACE_B]

where,

*   ``-i INTERFACE``: Interface (DPDK port number) to use.

    If a single ``-i`` parameter is given, the interface will send back all the traffic it receives.
    If two ``-i`` parameters are given, the two interfaces form a bridge,
    where traffic received on one interface is replicated and sent to the other interface.

For example, to run the application in a linuxapp environment using port 0 and 2:

.. code-block:: console

    ./build/bridge [EAL options] -- -i 0 -i 2

Refer to the *DPDK Getting Started Guide for Linux* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Note that unlike a traditional bridge or the ``l2fwd`` sample application, no MAC address changes are done on the frames.
Do not forget to take this into account when configuring a traffic generators and testing this sample application.
