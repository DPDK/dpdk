..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _compiling_sample_apps:

Compiling and Running Sample Applications
=========================================

The chapter describes how to compile and run applications in a DPDK
environment. It also provides a pointer to where sample applications are stored.

Compiling a Sample Application
------------------------------

Once a DPDK target environment directory has been created (such as
``x86_64-native-freebsd-clang``), it contains all libraries and header files required
to build an application.

When compiling an application in the FreeBSD environment on the DPDK,
the following variables must be exported:

*   ``RTE_SDK`` - Points to the DPDK installation directory.

*   ``RTE_TARGET`` - Points to the DPDK target environment directory.
    For FreeBSD, this is the ``x86_64-native-freebsd-clang`` or
    ``x86_64-native-freebsd-gcc`` directory.

The following is an example of creating the ``helloworld`` application, which runs
in the DPDK FreeBSD environment. While the example demonstrates compiling
using gcc version 4.9, compiling with clang will be similar, except that the ``CC=``
parameter can probably be omitted. The ``helloworld`` example may be found in the
``${RTE_SDK}/examples`` directory.

The directory contains the ``main.c`` file. This file, when combined with the
libraries in the DPDK target environment, calls the various functions to
initialize the DPDK environment, then launches an entry point (dispatch
application) for each core to be utilized. By default, the binary is generated
in the build directory.

.. code-block:: console

    setenv RTE_SDK /home/user/DPDK
    cd $(RTE_SDK)
    cd examples/helloworld/
    setenv RTE_SDK $HOME/DPDK
    setenv RTE_TARGET x86_64-native-freebsd-gcc

    gmake CC=gcc49
      CC main.o
      LD helloworld
      INSTALL-APP helloworld
      INSTALL-MAP helloworld.map

    ls build/app
      helloworld helloworld.map

.. note::

    In the above example, ``helloworld`` was in the directory structure of the
    DPDK. However, it could have been located outside the directory
    structure to keep the DPDK structure intact.  In the following case,
    the ``helloworld`` application is copied to a new directory as a new starting
    point.

.. code-block:: console

    setenv RTE_SDK /home/user/DPDK
    cp -r $(RTE_SDK)/examples/helloworld my_rte_app
    cd my_rte_app/
    setenv RTE_TARGET x86_64-native-freebsd-gcc

    gmake CC=gcc49
      CC main.o
      LD helloworld
      INSTALL-APP helloworld
      INSTALL-MAP helloworld.map

.. _running_sample_app:

Running a Sample Application
----------------------------

#.  The ``contigmem`` and ``nic_uio`` modules must be set up prior to running an application.

#.  Any ports to be used by the application must be already bound to the ``nic_uio`` module,
    as described in section :ref:`binding_network_ports`, prior to running the application.
    The application is linked with the DPDK target environment's Environment
    Abstraction Layer (EAL) library, which provides some options that are generic
    to every DPDK application.

The following is the list of options that can be given to the EAL:

.. code-block:: console

    ./rte-app -l CORELIST [-n NUM] [-b <domain:bus:devid.func>] \
              [-r NUM] [-v] [--proc-type <primary|secondary|auto>]

.. note::

    EAL has a common interface between all operating systems and is based on the
    Linux notation for PCI devices. For example, a FreeBSD device selector of
    ``pci0:2:0:1`` is referred to as ``02:00.1`` in EAL.

The EAL options for FreeBSD are as follows:

*   ``-c COREMASK`` or ``-l CORELIST``:
    A hexadecimal bit mask of the cores to run on.  Note that core numbering
    can change between platforms and should be determined beforehand. The corelist
    is a list of cores to use instead of a core mask.

*   ``-n NUM``:
    Number of memory channels per processor socket.

*   ``-b <domain:bus:devid.func>``:
    Blacklisting of ports; prevent EAL from using specified PCI device
    (multiple ``-b`` options are allowed).

*   ``--use-device``:
    Use the specified Ethernet device(s) only.  Use comma-separate
    ``[domain:]bus:devid.func`` values. Cannot be used with ``-b`` option.

*   ``-r NUM``:
    Number of memory ranks.

*   ``-v``:
    Display version information on startup.

*   ``--proc-type``:
    The type of process instance.

*   ``-m MB``:
    Memory to allocate from hugepages, regardless of processor socket.

Other options, specific to Linux and are not supported under FreeBSD are as follows:

*   ``socket-mem``:
    Memory to allocate from hugepages on specific sockets.

*   ``--huge-dir``:
    The directory where hugetlbfs is mounted.

*   ``mbuf-pool-ops-name``:
    Pool ops name for mbuf to use.

*   ``--file-prefix``:
    The prefix text used for hugepage filenames.

The ``-c`` or ``-l`` option is mandatory; the others are optional.

Copy the DPDK application binary to your target, then run the application
as follows (assuming the platform has four memory channels, and that cores 0-3
are present and are to be used for running the application)::

    ./helloworld -l 0-3 -n 4

.. note::

    The ``--proc-type`` and ``--file-prefix`` EAL options are used for running multiple
    DPDK processes.  See the "Multi-process Sample Application" chapter
    in the *DPDK Sample Applications User Guide and the DPDK
    Programmers Guide* for more details.

.. _running_non_root:

Running DPDK Applications Without Root Privileges
-------------------------------------------------

Although applications using the DPDK use network ports and other hardware
resources directly, with a number of small permission adjustments, it is possible
to run these applications as a user other than "root".  To do so, the ownership,
or permissions, on the following file system objects should be adjusted to ensure
that the user account being used to run the DPDK application has access
to them:

*   The userspace-io device files in ``/dev``, for example, ``/dev/uio0``, ``/dev/uio1``, and so on

*   The userspace contiguous memory device: ``/dev/contigmem``

.. note::

    Please refer to the DPDK Release Notes for supported applications.
