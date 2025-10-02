..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Compiling the Sample Applications
=================================

This section explains how to compile the DPDK sample applications.

To compile all the sample applications
--------------------------------------

Go to DPDK build directory:

    .. code-block:: console

       cd dpdk/<build_dir>

Enable examples compilation:

   .. code-block:: console

      meson configure -Dexamples=all

Build:

   .. code-block:: console

      ninja

For additional information on compiling see
:doc:`Compiling DPDK on Linux </linux_gsg/build_dpdk>` or
:doc:`Compiling DPDK on FreeBSD </freebsd_gsg/build_dpdk>`.
Applications are output to: ``dpdk/<build_dir>/examples``.


To compile a single application
-------------------------------


Using meson
~~~~~~~~~~~

Go to DPDK build directory:

    .. code-block:: console

       cd dpdk/<build_dir>

Enable example app compilation:

   .. code-block:: console

      meson configure -Dexamples=helloworld

Build:

   .. code-block:: console

      ninja


Using Make
~~~~~~~~~~

Pkg-config is used when building an example app standalone using make, please
see :ref:`linux_gsg_building_app_using_installed_dpdk` for more information.

Go to the sample application directory. Unless otherwise specified the sample
applications are located in ``dpdk/examples/``.

Build the application:

    .. code-block:: console

        make

To build the application for debugging use the ``DEBUG`` option.
This option adds some extra flags, disables compiler optimizations and
sets verbose output.

    .. code-block:: console

       make DEBUG=1
