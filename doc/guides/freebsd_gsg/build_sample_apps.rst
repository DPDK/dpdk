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

.. _compiling_sample_apps:

Compiling and Running Sample Applications
=========================================

The chapter describes how to compile and run applications in a DPDK
environment. It also provides a pointer to where sample applications are stored.

Compiling a Sample Application
------------------------------

Once a DPDK target environment directory has been created (such as
x86_64-native-bsdapp-clang), it contains all libraries and header files required
to build an application.

When compiling an application in the FreeBSD* environment on the DPDK,
the following variables must be exported:

*   RTE_SDK - Points to the DPDK installation directory.

*   RTE_TARGET - Points to the DPDK target environment directory.
    For FreeBSD*, this is the x86_64-native-bsdapp-clang or
    x86_64-native-bsdapp-gcc directory.

The following is an example of creating the helloworld application, which runs
in the DPDK FreeBSD* environment. While the example demonstrates compiling
using gcc version 4.8, compiling with clang will be similar, except that the "CC="
parameter can probably be omitted. The "helloworld" example may be found in the
${RTE_SDK}/examples directory.

The directory contains the main.c file.  This file, when combined with the
libraries in the DPDK target environment, calls the various functions to
initialize the DPDK environment, then launches an entry point (dispatch
application) for each core to be utilized.  By default, the binary is generated
in the build directory.

.. code-block:: console

    user@host:~/DPDK$ cd examples/helloworld/
    user@host:~/DPDK/examples/helloworld$ setenv RTE_SDK $HOME/DPDK
    user@host:~/DPDK/examples/helloworld$ setenv RTE_TARGET x86_64-native-bsdapp-gcc
    user@host:~/DPDK/examples/helloworld$ gmake CC=gcc48
    CC main.o
    LD helloworld
    INSTALL-APP helloworld
    INSTALL-MAP helloworld.map
    user@host:~/DPDK/examples/helloworld$ ls build/app
    helloworld helloworld.map

.. note::

    In the above example, helloworld was in the directory structure of the
    DPDK.  However, it could have been located outside the directory
    structure to keep the DPDK structure intact.  In the following case,
    the helloworld application is copied to a new directory as a new starting
    point.

.. code-block:: console

    user@host:~$ setenv RTE_SDK /home/user/DPDK
    user@host:~$ cp -r $(RTE_SDK)/examples/helloworld my_rte_app
    user@host:~$ cd my_rte_app/
    user@host:~$ setenv RTE_TARGET x86_64-native-bsdapp-gcc
    user@host:~/my_rte_app$ gmake CC=gcc48
    CC main.o
    LD helloworld
    INSTALL-APP helloworld
    INSTALL-MAP helloworld.map

.. _running_sample_app:

Running a Sample Application
----------------------------

#.  The contigmem and nic_uio modules must be set up prior to running an application.

#.  Any ports to be used by the application must be already bound to the nic_uio module,
    as described in section :ref:`binding_network_ports`, prior to running the application.
    The application is linked with the DPDK target environment's Environment
    Abstraction Layer (EAL) library, which provides some options that are generic
    to every DPDK application.

The following is the list of options that can be given to the EAL:

.. code-block:: console

    ./rte-app -c COREMASK -n NUM [-b <domain:bus:devid.func>] [-r NUM] [-v] [--proc-type <primary|secondary|auto>]

.. note::

    EAL has a common interface between all operating systems and is based on the
    Linux* notation for PCI devices. For example, a FreeBSD* device selector of
    pci0:2:0:1 is referred to as 02:00.1 in EAL.

The EAL options for FreeBSD* are as follows:

*   -c COREMASK
    : A hexadecimal bit mask of the cores to run on.  Note that core numbering
    can change between platforms and should be determined beforehand.

*   -n NUM
    : Number of memory channels per processor socket.

*   -b <domain:bus:devid.func>
    : blacklisting of ports; prevent EAL from using specified PCI device
    (multiple -b options are allowed).

*   --use-device
    : use the specified Ethernet device(s) only.  Use comma-separate
    <[domain:]bus:devid.func> values. Cannot be used with -b option.

*   -r NUM
    : Number of memory ranks.

*   -v
    : Display version information on startup.

*   --proc-type
    : The type of process instance.

Other options, specific to Linux* and are not supported under FreeBSD* are as follows:

*   socket-mem
    : Memory to allocate from hugepages on specific sockets.

*   --huge-dir
    : The directory where hugetlbfs is mounted.

*   --file-prefix
    : The prefix text used for hugepage filenames.

*   -m MB
    : Memory to allocate from hugepages, regardless of processor socket.
    It is recommended that --socket-mem be used instead of this option.

The -c and the -n options are mandatory; the others are optional.

Copy the DPDK application binary to your target, then run the application
as follows (assuming the platform has four memory channels, and that cores 0-3
are present and are to be used for running the application):

.. code-block:: console

    root@target:~$ ./helloworld -c f -n 4

.. note::

    The --proc-type and --file-prefix EAL options are used for running multiple
    DPDK processes.  See the “Multi-process Sample Application” chapter
    in the *DPDK Sample Applications User Guide and the DPDK
    Programmers Guide* for more details.

.. _running_non_root:

Running DPDK Applications Without Root Privileges
-------------------------------------------------

Although applications using the DPDK use network ports and other hardware
resources directly, with a number of small permission adjustments, it is possible
to run these applications as a user other than “root”.  To do so, the ownership,
or permissions, on the following file system objects should be adjusted to ensure
that the user account being used to run the DPDK application has access
to them:

*   The userspace-io device files in /dev, for example, /dev/uio0, /dev/uio1, and so on

*   The userspace contiguous memory device:  /dev/contigmem

.. note::

    Please refer to the DPDK Release Notes for supported applications.
