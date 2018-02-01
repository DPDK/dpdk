..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Compiling the Sample Applications
=================================

This section explains how to compile the DPDK sample applications.

To compile all the sample applications
--------------------------------------


Set the path to DPDK source code if its not set:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk

Go to DPDK source:

    .. code-block:: console

        cd $RTE_SDK

Build DPDK:

   .. code-block:: console

        make defconfig
        make

Build the sample applications:

   .. code-block:: console

       export RTE_TARGET=build
       make -C examples

For other possible ``RTE_TARGET`` values and additional information on
compiling see
:ref:`Compiling DPDK on Linux <linux_gsg_compiling_dpdk>` or
:ref:`Compiling DPDK on FreeBSD <building_from_source>`.
Applications are output to: ``$RTE_SDK/examples/app-dir/build`` or
``$RTE_SDK/examples/app-dir/$RTE_TARGET``.


In the example above the compiled application is written to the ``build`` subdirectory.
To have the applications written to a different location,
the ``O=/path/to/build/directory`` option may be specified in the make command.

    .. code-block:: console

       make O=/tmp

To build the applications for debugging use the ``DEBUG`` option.
This option adds some extra flags, disables compiler optimizations and
sets verbose output.

    .. code-block:: console

       make DEBUG=1


To compile a single application
-------------------------------

Set the path to DPDK source code:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk

Go to DPDK source:

    .. code-block:: console

       cd $RTE_SDK

Build DPDK:

    .. code-block:: console

        make defconfig
        make

Go to the sample application directory. Unless otherwise specified the sample
applications are located in ``$RTE_SDK/examples/``.


Build the application:

    .. code-block:: console

        export RTE_TARGET=build
        make
