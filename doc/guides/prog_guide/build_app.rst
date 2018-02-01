..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Building_Your_Own_Application:

Building Your Own Application
=============================

Compiling a Sample Application in the Development Kit Directory
---------------------------------------------------------------

When compiling a sample application (for example, hello world), the following variables must be exported:
RTE_SDK and RTE_TARGET.

.. code-block:: console

    ~/DPDK$ cd examples/helloworld/
    ~/DPDK/examples/helloworld$ export RTE_SDK=/home/user/DPDK
    ~/DPDK/examples/helloworld$ export RTE_TARGET=x86_64-native-linuxapp-gcc
    ~/DPDK/examples/helloworld$ make
        CC main.o
        LD helloworld
        INSTALL-APP helloworld
        INSTALL-MAP helloworld.map

The binary is generated in the build directory by default:

.. code-block:: console

    ~/DPDK/examples/helloworld$ ls build/app
    helloworld helloworld.map

Build Your Own Application Outside the Development Kit
------------------------------------------------------

The sample application (Hello World) can be duplicated in a new directory as a starting point for your development:

.. code-block:: console

    ~$ cp -r DPDK/examples/helloworld my_rte_app
    ~$ cd my_rte_app/
    ~/my_rte_app$ export RTE_SDK=/home/user/DPDK
    ~/my_rte_app$ export RTE_TARGET=x86_64-native-linuxapp-gcc
    ~/my_rte_app$ make
        CC main.o
        LD helloworld
        INSTALL-APP helloworld
        INSTALL-MAP helloworld.map

Customizing Makefiles
---------------------

Application Makefile
~~~~~~~~~~~~~~~~~~~~

The default makefile provided with the Hello World sample application is a good starting point. It includes:

*   $(RTE_SDK)/mk/rte.vars.mk at the beginning

*   $(RTE_SDK)/mk/rte.extapp.mk at the end

The user must define several variables:

*   APP: Contains the name of the application.

*   SRCS-y: List of source files (\*.c, \*.S).

Library Makefile
~~~~~~~~~~~~~~~~

It is also possible to build a library in the same way:

*   Include $(RTE_SDK)/mk/rte.vars.mk at the beginning.

*   Include $(RTE_SDK)/mk/rte.extlib.mk  at the end.

The only difference is that APP should be replaced by LIB, which contains the name of the library. For example, libfoo.a.

Customize Makefile Actions
~~~~~~~~~~~~~~~~~~~~~~~~~~

Some variables can be defined to customize Makefile actions. The most common are listed below. Refer to
:ref:`Makefile Description <Makefile_Description>` section in
:ref:`Development Kit Build System <Development_Kit_Build_System>`

chapter for details.

*   VPATH: The path list where the build system will search for sources. By default,
    RTE_SRCDIR will be included in VPATH.

*   CFLAGS_my_file.o: The specific flags to add for C compilation of my_file.c.

*   CFLAGS: The flags to use for C compilation.

*   LDFLAGS: The flags to use for linking.

*   CPPFLAGS: The flags to use to provide flags to the C preprocessor (only useful when assembling .S files)

*   LDLIBS: A list of libraries to link with (for example, -L /path/to/libfoo - lfoo)
