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

*   NO_AUTOLIBS: If set, the libraries provided by the framework will not be included in the LDLIBS variable automatically.
