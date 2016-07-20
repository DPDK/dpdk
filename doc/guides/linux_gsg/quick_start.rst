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

.. _linux_setup_script:

Quick Start Setup Script
========================

The dpdk-setup.sh script, found in the tools subdirectory, allows the user to perform the following tasks:

*   Build the DPDK libraries

*   Insert and remove the DPDK IGB_UIO kernel module

*   Insert and remove VFIO kernel modules

*   Insert and remove the DPDK KNI kernel module

*   Create and delete hugepages for NUMA and non-NUMA cases

*   View network port status and reserve ports for DPDK application use

*   Set up permissions for using VFIO as a non-privileged user

*   Run the test and testpmd applications

*   Look at hugepages in the meminfo

*   List hugepages in ``/mnt/huge``

*   Remove built DPDK libraries

Once these steps have been completed for one of the EAL targets,
the user may compile their own application that links in the EAL libraries to create the DPDK image.

Script Organization
-------------------

The dpdk-setup.sh script is logically organized into a series of steps that a user performs in sequence.
Each step provides a number of options that guide the user to completing the desired task.
The following is a brief synopsis of each step.

**Step 1: Build DPDK Libraries**

Initially, the user must select a DPDK target to choose the correct target type and compiler options to use when building the libraries.

The user must have all libraries, modules, updates and compilers installed in the system prior to this,
as described in the earlier chapters in this Getting Started Guide.

**Step 2: Setup Environment**

The user configures the Linux* environment to support the running of DPDK applications.
Hugepages can be set up for NUMA or non-NUMA systems. Any existing hugepages will be removed.
The DPDK kernel module that is needed can also be inserted in this step,
and network ports may be bound to this module for DPDK application use.

**Step 3: Run an Application**

The user may run the test application once the other steps have been performed.
The test application allows the user to run a series of functional tests for the DPDK.
The testpmd application, which supports the receiving and sending of packets, can also be run.

**Step 4: Examining the System**

This step provides some tools for examining the status of hugepage mappings.

**Step 5: System Cleanup**

The final step has options for restoring the system to its original state.

Use Cases
---------

The following are some example of how to use the dpdk-setup.sh script.
The script should be run using the source command.
Some options in the script prompt the user for further data before proceeding.

.. warning::

    The dpdk-setup.sh script should be run with root privileges.

.. code-block:: console

    source tools/dpdk-setup.sh

    ------------------------------------------------------------------------

    RTE_SDK exported as /home/user/rte

    ------------------------------------------------------------------------

    Step 1: Select the DPDK environment to build

    ------------------------------------------------------------------------

    [1] i686-native-linuxapp-gcc

    [2] i686-native-linuxapp-icc

    [3] ppc_64-power8-linuxapp-gcc

    [4] x86_64-ivshmem-linuxapp-gcc

    [5] x86_64-ivshmem-linuxapp-icc

    [6] x86_64-native-bsdapp-clang

    [7] x86_64-native-bsdapp-gcc

    [8] x86_64-native-linuxapp-clang

    [9] x86_64-native-linuxapp-gcc

    [10] x86_64-native-linuxapp-icc

    ------------------------------------------------------------------------

    Step 2: Setup linuxapp environment

    ------------------------------------------------------------------------

    [11] Insert IGB UIO module

    [12] Insert VFIO module

    [13] Insert KNI module

    [14] Setup hugepage mappings for non-NUMA systems

    [15] Setup hugepage mappings for NUMA systems

    [16] Display current Ethernet device settings

    [17] Bind Ethernet device to IGB UIO module

    [18] Bind Ethernet device to VFIO module

    [19] Setup VFIO permissions

    ------------------------------------------------------------------------

    Step 3: Run test application for linuxapp environment

    ------------------------------------------------------------------------

    [20] Run test application ($RTE_TARGET/app/test)

    [21] Run testpmd application in interactive mode ($RTE_TARGET/app/testpmd)

    ------------------------------------------------------------------------

    Step 4: Other tools

    ------------------------------------------------------------------------

    [22] List hugepage info from /proc/meminfo

    ------------------------------------------------------------------------

    Step 5: Uninstall and system cleanup

    ------------------------------------------------------------------------

    [23] Uninstall all targets

    [24] Unbind NICs from IGB UIO driver

    [25] Remove IGB UIO module

    [26] Remove VFIO module

    [27] Remove KNI module

    [28] Remove hugepage mappings

    [29] Exit Script

Option:

The following selection demonstrates the creation of the ``x86_64-native-linuxapp-gcc`` DPDK library.

.. code-block:: console

    Option: 9

    ================== Installing x86_64-native-linuxapp-gcc

    Configuration done
    == Build lib
    ...
    Build complete
    RTE_TARGET exported as x86_64-native-linuxapp-gcc

The following selection demonstrates the starting of the DPDK UIO driver.

.. code-block:: console

    Option: 25

    Unloading any existing DPDK UIO module
    Loading DPDK UIO module

The following selection demonstrates the creation of hugepages in a NUMA system.
1024 2 MByte pages are assigned to each node.
The result is that the application should use -m 4096 for starting the application to access both memory areas
(this is done automatically if the -m option is not provided).

.. note::

    If prompts are displayed to remove temporary files, type 'y'.

.. code-block:: console

    Option: 15

    Removing currently reserved hugepages
    mounting /mnt/huge and removing directory
    Input the number of 2MB pages for each node
    Example: to have 128MB of hugepages available per node,
    enter '64' to reserve 64 * 2MB pages on each node
    Number of pages for node0: 1024
    Number of pages for node1: 1024
    Reserving hugepages
    Creating /mnt/huge and mounting as hugetlbfs

The following selection demonstrates the launch of the test application to run on a single core.

.. code-block:: console

    Option: 20

    Enter hex bitmask of cores to execute test app on
    Example: to execute app on cores 0 to 7, enter 0xff
    bitmask: 0x01
    Launching app
    EAL: coremask set to 1
    EAL: Detected lcore 0 on socket 0
    ...
    EAL: Master core 0 is ready (tid=1b2ad720)
    RTE>>

Applications
------------

Once the user has run the dpdk-setup.sh script, built one of the EAL targets and set up hugepages (if using one of the Linux EAL targets),
the user can then move on to building and running their application or one of the examples provided.

The examples in the /examples directory provide a good starting point to gain an understanding of the operation of the DPDK.
The following command sequence shows how the helloworld sample application is built and run.
As recommended in Section 4.2.1 , "Logical Core Use by Applications",
the logical core layout of the platform should be determined when selecting a core mask to use for an application.

.. code-block:: console

    cd helloworld/
    make
      CC main.o
      LD helloworld
      INSTALL-APP helloworld
      INSTALL-MAP helloworld.map

    sudo ./build/app/helloworld -c 0xf -n 3
    [sudo] password for rte:

    EAL: coremask set to f
    EAL: Detected lcore 0 as core 0 on socket 0
    EAL: Detected lcore 1 as core 0 on socket 1
    EAL: Detected lcore 2 as core 1 on socket 0
    EAL: Detected lcore 3 as core 1 on socket 1
    EAL: Setting up hugepage memory...
    EAL: Ask a virtual area of 0x200000 bytes
    EAL: Virtual area found at 0x7f0add800000 (size = 0x200000)
    EAL: Ask a virtual area of 0x3d400000 bytes
    EAL: Virtual area found at 0x7f0aa0200000 (size = 0x3d400000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9fc00000 (size = 0x400000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9f600000 (size = 0x400000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9f000000 (size = 0x400000)
    EAL: Ask a virtual area of 0x800000 bytes
    EAL: Virtual area found at 0x7f0a9e600000 (size = 0x800000)
    EAL: Ask a virtual area of 0x800000 bytes
    EAL: Virtual area found at 0x7f0a9dc00000 (size = 0x800000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9d600000 (size = 0x400000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9d000000 (size = 0x400000)
    EAL: Ask a virtual area of 0x400000 bytes
    EAL: Virtual area found at 0x7f0a9ca00000 (size = 0x400000)
    EAL: Ask a virtual area of 0x200000 bytes
    EAL: Virtual area found at 0x7f0a9c600000 (size = 0x200000)
    EAL: Ask a virtual area of 0x200000 bytes
    EAL: Virtual area found at 0x7f0a9c200000 (size = 0x200000)
    EAL: Ask a virtual area of 0x3fc00000 bytes
    EAL: Virtual area found at 0x7f0a5c400000 (size = 0x3fc00000)
    EAL: Ask a virtual area of 0x200000 bytes
    EAL: Virtual area found at 0x7f0a5c000000 (size = 0x200000)
    EAL: Requesting 1024 pages of size 2MB from socket 0
    EAL: Requesting 1024 pages of size 2MB from socket 1
    EAL: Master core 0 is ready (tid=de25b700)
    EAL: Core 1 is ready (tid=5b7fe700)
    EAL: Core 3 is ready (tid=5a7fc700)
    EAL: Core 2 is ready (tid=5affd700)
    hello from core 1
    hello from core 2
    hello from core 3
    hello from core 0
