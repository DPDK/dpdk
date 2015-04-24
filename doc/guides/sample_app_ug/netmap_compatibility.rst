..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Netmap Compatibility Sample Application
=======================================

Introduction
------------

The Netmap compatibility library provides a minimal set of APIs to give the ability to programs written against the Netmap APIs
to be run with minimal changes to their source code,  using the DPDK to perform the actual packet I/O.

Since Netmap applications use regular system calls, like open(), ioctl() and
mmap() to communicate with the Netmap kernel module performing the packet I/O,
the compat_netmap library provides a set of similar APIs to use in place of those system calls,
effectively turning a Netmap application into a DPDK one.

The provided library is currently minimal and doesn't support all the features that Netmap supports,
but is enough to run simple applications, such as the bridge example detailed below.

Knowledge of Netmap is required to understand the rest of this section.
Please refer to the Netmap distribution for details about Netmap.

Available APIs
--------------

The library provides the following drop-in replacements for system calls usually used in Netmap applications:rte_netmap_close()

*   rte_netmap_ioctl()

*   rte_netmap_open()

*   rte_netmap_mmap()

*   rte_netmap_poll()

They use the same signature as their libc counterparts, and can be used as drop-in replacements in most cases.

Caveats
-------

Given the difference between the way Netmap and the DPDK approach packet I/O,
there are caveats and limitations to be aware of when trying to use the compat_netmap library, the most important of which are listed below.
Additional caveats are presented in the $RTE_SDK/examples/netmap_compat/README.md file.
These can change as the library is updated:

*   Any system call that can potentially affect file descriptors cannot be used with a descriptor returned by the rte_netmap_open() function.

Note that:

*   rte_netmap_mmap() merely returns the address of a DPDK memzone.
    The address, length, flags, offset, and so on arguments are therefore ignored completely.

*   rte_netmap_poll() only supports infinite (negative) or zero time outs.
    It effectively turns calls to the poll() system call made in a Netmap application into polling of the DPDK ports,
    changing the semantics of the usual POSIX defined poll.

*   Not all of Netmap's features are supported: "host rings",
    slot flags and so on are not supported or are simply not relevant in the DPDK model.

*   The Netmap manual page states that "a device obtained through /dev/netmap also supports the ioctl supported by network devices".
    It is not the case with this compatibility layer.

*   The Netmap kernel module exposes a sysfs interface to change some internal parameters, such as the size of the shared memory region.
    This interface is not available when using this compatibility layer.

Porting Netmap Applications
---------------------------

Porting Netmap applications typically involves two major steps:

*   Changing the system calls to use their compat_netmap library counterparts

*   Adding further DPDK initialization code

Since the compat_netmap functions have the same signature as the usual libc calls, the change is in most cases trivial.

The usual DPDK initialization code involving rte_eal_init() and rte_eal_pci_probe()
has to be added to the Netmap application in the same way it is used in all other DPDK sample applications.
Please refer to the *DPDK Programmer's Guide* - Rel 1.4 EAR and example source code for details about initialization.

In addition of the regular DPDK initialization code,
the ported application needs to call initialization functions for the compat_netmap library,
namely rte_netmap_init() and rte_netmap_init_port().

These two initialization functions take compat_netmap specific data structures as parameters:
struct rte_netmap_conf and struct rte_netmap_port_conf.
Those structures' fields are Netmap related and are self-explanatory for developers familiar with Netmap.
They are defined in $RTE_SDK/examples/netmap_compat/ lib/compat_netmap.h.

The bridge application is an example largely based on the bridge example shipped with the Netmap distribution.
It shows how a minimal Netmap application with minimal and straightforward source code changes can be run on top of the DPDK.
Please refer to $RTE_SDK/examples/netmap_compat/bridge/bridge.c for an example of ported application.

Compiling the "bridge" Sample Application
-----------------------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/netmap_compat

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the "bridge" Sample Application
---------------------------------------

The application requires a single command line option:

.. code-block:: console

    ./build/packet_ordering [EAL options] -- -p PORT_A [-p PORT_B]

where,

*   -p INTERFACE is the number of a valid DPDK port to use.

    If a single -p parameter is given, the interface will send back all the traffic it receives.
    If two -p parameters are given, the two interfaces form a bridge,
    where traffic received on one interface is replicated and sent by the other interface.

To run the application in a linuxapp environment using port 0 and 2, issue the following command:

.. code-block:: console

    ./build/packet_ordering [EAL options] -- -p 0 -p 2

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Note that unlike a traditional bridge or the l2fwd sample application, no MAC address changes are done on the frames.
Do not forget to take that into account when configuring your traffic generators if you decide to test this sample application.
