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

.. _External_Application/Library_Makefile_help:

External Application/Library Makefile help
==========================================

External applications or libraries should include specific Makefiles from RTE_SDK, located in mk directory.
These Makefiles are:

*   ${RTE_SDK}/mk/rte.extapp.mk: Build an application

*   ${RTE_SDK}/mk/rte.extlib.mk: Build a static library

*   ${RTE_SDK}/mk/rte.extobj.mk: Build objects (.o)

Prerequisites
-------------

The following variables must be defined:

*   ${RTE_SDK}: Points to the root directory of the DPDK.

*   ${RTE_TARGET}: Reference the target to be used for compilation (for example, x86_64-native-linuxapp-gcc).

Build Targets
-------------

Build targets support the specification of the name of the output directory, using O=mybuilddir.
This is optional; the default output directory is build.

*   all, "nothing" (meaning just make)

    Build the application or the library in the specified output directory.

    Example:

    .. code-block:: console

        make O=mybuild

*   clean

    Clean all objects created using make build.

    Example:

    .. code-block:: console

        make clean O=mybuild

Help Targets
------------

*   help

    Show this help.

Other Useful Command-line Variables
-----------------------------------

The following variables can be specified at the command line:

*   S=

    Specify the directory in which the sources are located. By default, it is the current directory.

*   M=

    Specify the Makefile to call once the output directory is created. By default, it uses $(S)/Makefile.

*   V=

    Enable verbose build (show full compilation command line and some intermediate commands).

*   D=

    Enable dependency debugging. This provides some useful information about why a target must be rebuilt or not.

*   EXTRA_CFLAGS=, EXTRA_LDFLAGS=, EXTRA_ASFLAGS=, EXTRA_CPPFLAGS=

    Append specific compilation, link or asm flags.

*   CROSS=

    Specify a cross-toolchain header that will prefix all gcc/binutils applications. This only works when using gcc.

Make from Another Directory
---------------------------

It is possible to run the Makefile from another directory, by specifying the output and the source dir. For example:

.. code-block:: console

    export RTE_SDK=/path/to/DPDK
    export RTE_TARGET=x86_64-native-linuxapp-icc
    make -f /path/to/my_app/Makefile S=/path/to/my_app O=/path/to/build_dir
