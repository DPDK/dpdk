..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Building_Your_Own_Application:

Building Your Own Application
=============================

Compiling a Sample Application in the Development Kit Directory
---------------------------------------------------------------

To compile a sample application with make (for example, hello world):

.. code-block:: console

    ~/DPDK$ cd examples/helloworld/
    ~/DPDK/examples/helloworld$ make

The binary is generated in the build directory by default:

.. code-block:: console

    ~/DPDK/examples/helloworld$ ls build/app
    helloworld helloworld.map

Please refer to :doc:`../linux_gsg/build_dpdk` for details on compiling with meson.

Build Your Own Application Outside the Development Kit
------------------------------------------------------

The sample application (Hello World) can be duplicated in a new directory as a starting point for your development:

.. code-block:: console

    ~$ cp -r DPDK/examples/helloworld my_rte_app
    ~$ cd my_rte_app/
    ~/my_rte_app$ make

Customizing Makefiles
---------------------

Application Makefile
~~~~~~~~~~~~~~~~~~~~

The default makefile provided with the Hello World sample application is a good starting point.

The user must define several variables:

*   APP: Contains the name of the application.

*   SRCS-y: List of source files (\*.c, \*.S).
