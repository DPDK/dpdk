..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

**Part 2: Development Environment**

Source Organization
===================

This section describes the organization of sources in the DPDK framework.

Makefiles and Config
--------------------

.. note::

    In the following descriptions,
    ``RTE_SDK`` is the environment variable that points to the base directory into which the tarball was extracted.
    See
    :ref:`Useful_Variables_Provided_by_the_Build_System`
    for descriptions of other variables.

Makefiles that are provided by the DPDK libraries and applications are located in ``$(RTE_SDK)/mk``.

Config templates are located in ``$(RTE_SDK)/config``. The templates describe the options that are enabled for each target.
The config file also contains items that can be enabled and disabled for many of the DPDK libraries,
including debug options.
The user should look at the config file and become familiar with these options.
The config file is also used to create a header file, which will be located in the new build directory.

Libraries
---------

Libraries are located in subdirectories of ``$(RTE_SDK)/lib``.
By convention a library refers to any code that provides an API to an application.
Typically, it generates an archive file (``.a``), but a kernel module would also go in the same directory.

The lib directory contains::

    lib
    +-- librte_cmdline      # Command line interface helper
    +-- librte_distributor  # Packet distributor
    +-- librte_eal          # Environment abstraction layer
    +-- librte_ethdev       # Generic interface to poll mode driver
    +-- librte_hash         # Hash library
    +-- librte_ip_frag      # IP fragmentation library
    +-- librte_kni          # Kernel NIC interface
    +-- librte_kvargs       # Argument parsing library
    +-- librte_lpm          # Longest prefix match library
    +-- librte_mbuf         # Packet buffer manipulation
    +-- librte_mempool      # Memory pool manager (fixed sized objects)
    +-- librte_meter        # QoS metering library
    +-- librte_net          # Various IP-related headers
    +-- librte_power        # Power management library
    +-- librte_ring         # Software rings (act as lockless FIFOs)
    +-- librte_sched        # QoS scheduler and dropper library
    +-- librte_timer        # Timer library

Drivers
-------

Drivers are special libraries which provide poll-mode driver implementations for
devices: either hardware devices or pseudo/virtual devices. They are contained
in the *drivers* subdirectory, classified by type, and each compiles to a
library with the format ``librte_pmd_X.a`` where ``X`` is the driver name.

The drivers directory has a *net* subdirectory which contains::

    drivers/net
    +-- af_packet          # Poll mode driver based on Linux af_packet
    +-- bonding            # Bonding poll mode driver
    +-- cxgbe              # Chelsio Terminator 10GbE/40GbE poll mode driver
    +-- e1000              # 1GbE poll mode drivers (igb and em)
    +-- enic               # Cisco VIC Ethernet NIC Poll-mode Driver
    +-- fm10k              # Host interface PMD driver for FM10000 Series
    +-- i40e               # 40GbE poll mode driver
    +-- ixgbe              # 10GbE poll mode driver
    +-- mlx4               # Mellanox ConnectX-3 poll mode driver
    +-- null               # NULL poll mode driver for testing
    +-- pcap               # PCAP poll mode driver
    +-- ring               # Ring poll mode driver
    +-- szedata2           # SZEDATA2 poll mode driver
    +-- virtio             # Virtio poll mode driver
    +-- vmxnet3            # VMXNET3 poll mode driver

.. note::

   Several of the ``driver/net`` directories contain a ``base``
   sub-directory. The ``base`` directory generally contains code the shouldn't
   be modified directly by the user. Any enhancements should be done via the
   ``X_osdep.c`` and/or ``X_osdep.h`` files in that directory. Refer to the
   local README in the base directories for driver specific instructions.


Applications
------------

Applications are source files that contain a ``main()`` function.
They are located in the ``$(RTE_SDK)/app`` and ``$(RTE_SDK)/examples`` directories.

The app directory contains sample applications that are used to test DPDK (such as autotests)
or the Poll Mode Drivers (test-pmd)::

    app
    +-- chkincs            # Test program to check include dependencies
    +-- cmdline_test       # Test the commandline library
    +-- test               # Autotests to validate DPDK features
    +-- test-acl           # Test the ACL library
    +-- test-pipeline      # Test the IP Pipeline framework
    +-- test-pmd           # Test and benchmark poll mode drivers

The examples directory contains sample applications that show how libraries can be used::

    examples
    +-- cmdline            # Example of using the cmdline library
    +-- exception_path     # Sending packets to and from Linux TAP device
    +-- helloworld         # Basic Hello World example
    +-- ip_reassembly      # Example showing IP reassembly
    +-- ip_fragmentation   # Example showing IPv4 fragmentation
    +-- ipv4_multicast     # Example showing IPv4 multicast
    +-- kni                # Kernel NIC Interface (KNI) example
    +-- l2fwd              # L2 forwarding with and without SR-IOV
    +-- l3fwd              # L3 forwarding example
    +-- l3fwd-power        # L3 forwarding example with power management
    +-- l3fwd-vf           # L3 forwarding example with SR-IOV
    +-- link_status_interrupt # Link status change interrupt example
    +-- load_balancer      # Load balancing across multiple cores/sockets
    +-- multi_process      # Example apps using multiple DPDK processes
    +-- qos_meter          # QoS metering example
    +-- qos_sched          # QoS scheduler and dropper example
    +-- timer              # Example of using librte_timer library
    +-- vmdq_dcb           # Example of VMDQ and DCB receiving
    +-- vmdq               # Example of VMDQ receiving
    +-- vhost              # Example of userspace vhost and switch

.. note::

    The actual examples directory may contain additional sample applications to those shown above.
    Check the latest DPDK source files for details.
